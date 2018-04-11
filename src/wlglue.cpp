#include <glib.h>

#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "xdg-shell-unstable-v6-client-protocol.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <unordered_map>

#include <linux/input.h>
#include <wpe/input.h>

#include <unistd.h>
#include <sys/mman.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>

struct EventSource {
    static GSourceFuncs sourceFuncs;

    GSource source;
    GPollFD pfd;
    struct wl_display* display;
};

GSourceFuncs EventSource::sourceFuncs = {
    // prepare
    [](GSource* base, gint* timeout) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);
        struct wl_display* display = source->display;

        *timeout = -1;

        wl_display_dispatch_pending(display);
        wl_display_flush(display);

        return FALSE;
    },
    // check
    [](GSource* base) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);
        return !!source->pfd.revents;
    },
    // dispatch
    [](GSource* base, GSourceFunc, gpointer) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);
        struct wl_display* display = source->display;

        if (source->pfd.revents & G_IO_IN)
            wl_display_dispatch(display);

        if (source->pfd.revents & (G_IO_ERR | G_IO_HUP))
            return FALSE;

        source->pfd.revents = 0;
        return TRUE;
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

struct WlGlueHost {
    struct wl_display* display;
    struct wl_compositor* compositor;
    struct zxdg_shell_v6* xdg_v6;
    struct wl_seat* seat;
    GSource* eventSource;

    EGLDisplay eglDisplay;

    struct SeatData {
        std::unordered_map<struct wl_surface*, struct wlglue_window_client*> clients;

        struct {
            struct wl_pointer* object { nullptr };
            std::pair<struct wl_surface*, struct wlglue_window_client*> target;
            std::pair<int, int> coords { 0, 0 };
            uint32_t button { 0 };
            uint32_t state { 0 };
        } pointer;

        struct {
            struct wl_keyboard* object { nullptr };
            std::pair<struct wl_surface*, struct wlglue_window_client*> target;
        } keyboard;

        struct {
            struct xkb_context* context { nullptr };
            struct xkb_keymap* keymap { nullptr };
            struct xkb_state* state { nullptr };
            struct {
                xkb_mod_index_t control { 0 };
                xkb_mod_index_t alt { 0 };
                xkb_mod_index_t shift { 0 };
            } indexes;
            uint8_t modifiers { 0 };
            struct xkb_compose_table* composeTable { nullptr };
            struct xkb_compose_state* composeState { nullptr };
        } xkb;

        struct {
            int32_t rate;
            int32_t delay;
        } repeatInfo { 0, 0 };

        struct {
            uint32_t key;
            uint32_t time;
            uint32_t state;
            uint32_t eventSource;
        } repeatData { 0, 0, 0, 0 };

    } seatData;
};

static const struct wl_registry_listener s_registryListener = {
    // global
    [](void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t)
    {
        auto* host = static_cast<WlGlueHost*>(data);

        if (!std::strcmp(interface, "wl_compositor"))
            host->compositor = static_cast<struct wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 1));

        if (!std::strcmp(interface, "zxdg_shell_v6"))
            host->xdg_v6 = static_cast<struct zxdg_shell_v6*>(wl_registry_bind(registry, name, &zxdg_shell_v6_interface, 1));

        if (!std::strcmp(interface, "wl_seat"))
            host->seat = static_cast<struct wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 4));
    },
    // global_remove
    [](void*, struct wl_registry*, uint32_t) { },
};

static const struct zxdg_shell_v6_listener s_xdg6ShellListener = {
    // ping
    [](void*, struct zxdg_shell_v6* shell, uint32_t serial)
    {
        zxdg_shell_v6_pong(shell, serial);
    },
};

static const struct zxdg_surface_v6_listener s_xdg6SurfaceListener = {
    // configure
    [](void*, struct zxdg_surface_v6* surface, uint32_t serial)
    {
        zxdg_surface_v6_ack_configure(surface, serial);
    },
};

static const struct zxdg_toplevel_v6_listener s_xdg6ToplevelListener = {
    // configure
    [](void*, struct zxdg_toplevel_v6*, int32_t width, int32_t height, struct wl_array*)
    {
        // FIXME: dispatch the size against wpe_view_backend.
    },
    // close
    [](void*, struct zxdg_toplevel_v6*) { },
};

struct wlglue_window_client {
    void (*frame_displayed)();
    void (*release_buffer_resource)(struct wl_resource*);

    void (*dispatch_input_pointer_event)(struct wpe_input_pointer_event*);
    void (*dispatch_input_axis_event)(struct wpe_input_axis_event*);
    void (*dispatch_input_keyboard_event)(struct wpe_input_keyboard_event*);
};

static const struct wl_pointer_listener g_pointerListener = {
    // enter
    [](void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface, wl_fixed_t, wl_fixed_t)
    {
        auto& seatData = static_cast<WlGlueHost*>(data)->seatData;

        auto it = seatData.clients.find(surface);
        if (it != seatData.clients.end())
            seatData.pointer.target = *it;
    },
    // leave
    [](void* data, struct wl_pointer*, uint32_t serial, struct wl_surface* surface)
    {
        auto& seatData = static_cast<WlGlueHost*>(data)->seatData;

        auto it = seatData.clients.find(surface);
        if (it != seatData.clients.end() && seatData.pointer.target.first == it->first)
            seatData.pointer.target = { };
    },
    // motion
    [](void* data, struct wl_pointer*, uint32_t time, wl_fixed_t fixedX, wl_fixed_t fixedY)
    {
        auto& seatData = static_cast<WlGlueHost*>(data)->seatData;

        int x = wl_fixed_to_int(fixedX);
        int y = wl_fixed_to_int(fixedY);
        seatData.pointer.coords = { x, y };

        if (seatData.pointer.target.first) {
            struct wpe_input_pointer_event event = { wpe_input_pointer_event_type_motion,
                time, x, y, seatData.pointer.button, seatData.pointer.state };
            seatData.pointer.target.second->dispatch_input_pointer_event(&event);
        }
    },
    // button
    [](void* data, struct wl_pointer*, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
    {
        auto& seatData = static_cast<WlGlueHost*>(data)->seatData;

        if (button >= BTN_MOUSE)
            button = button - BTN_MOUSE + 1;
        else
            button = 0;

        seatData.pointer.button = !!state ? button : 0;
        seatData.pointer.state = state;

        if (seatData.pointer.target.first) {
            struct wpe_input_pointer_event event = { wpe_input_pointer_event_type_button,
                time, seatData.pointer.coords.first, seatData.pointer.coords.second, button, state };
            seatData.pointer.target.second->dispatch_input_pointer_event(&event);
        }
    },
    // axis
    [](void* data, struct wl_pointer*, uint32_t time, uint32_t axis, wl_fixed_t value)
    {
        auto& seatData = static_cast<WlGlueHost*>(data)->seatData;

        if (seatData.pointer.target.first) {
            struct wpe_input_axis_event event = { wpe_input_axis_event_type_motion,
                time, seatData.pointer.coords.first, seatData.pointer.coords.second, axis, -wl_fixed_to_int(value) };
            seatData.pointer.target.second->dispatch_input_axis_event(&event);
        }
    },
};

static void
handleKeyEvent(WlGlueHost::SeatData& seatData, uint32_t key, uint32_t state, uint32_t time)
{
    auto& xkb = seatData.xkb;
    uint32_t keysym = xkb_state_key_get_one_sym(xkb.state, key);
    uint32_t unicode = xkb_state_key_get_utf32(xkb.state, key);

    if (xkb.composeState
        && state == WL_KEYBOARD_KEY_STATE_PRESSED
        && xkb_compose_state_feed(xkb.composeState, keysym) == XKB_COMPOSE_FEED_ACCEPTED
        && xkb_compose_state_get_status(xkb.composeState) == XKB_COMPOSE_COMPOSED)
    {
        keysym = xkb_compose_state_get_one_sym(xkb.composeState);
        unicode = xkb_keysym_to_utf32(keysym);
    }

    if (seatData.keyboard.target.first) {
        struct wpe_input_keyboard_event event = { time, keysym, unicode, !!state, xkb.modifiers };
        seatData.keyboard.target.second->dispatch_input_keyboard_event(&event);
    }
}

static gboolean
repeatRateTimeout(void* data)
{
    auto& seatData = static_cast<WlGlueHost*>(data)->seatData;
    handleKeyEvent(seatData, seatData.repeatData.key, seatData.repeatData.state, seatData.repeatData.time);
    return G_SOURCE_CONTINUE;
}

static gboolean
repeatDelayTimeout(void* data)
{
    auto& seatData = static_cast<WlGlueHost*>(data)->seatData;
    handleKeyEvent(seatData, seatData.repeatData.key, seatData.repeatData.state, seatData.repeatData.time);
    seatData.repeatData.eventSource = g_timeout_add(seatData.repeatInfo.rate, static_cast<GSourceFunc>(repeatRateTimeout), data);
    return G_SOURCE_REMOVE;
}

static const struct wl_keyboard_listener g_keyboardListener = {
    // keymap
    [](void* data, struct wl_keyboard*, uint32_t format, int fd, uint32_t size)
    {
        auto& seatData = static_cast<WlGlueHost*>(data)->seatData;

        if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
            close(fd);
            return;
        }

        void* mapping = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
        if (mapping == MAP_FAILED) {
            close(fd);
            return;
        }

        auto& xkb = seatData.xkb;
        xkb.keymap = xkb_keymap_new_from_string(xkb.context, static_cast<char*>(mapping),
            XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(mapping, size);
        close(fd);

        if (!xkb.keymap)
            return;

        xkb.state = xkb_state_new(xkb.keymap);
        if (!xkb.state)
            return;

        xkb.indexes.control = xkb_keymap_mod_get_index(xkb.keymap, XKB_MOD_NAME_CTRL);
        xkb.indexes.alt = xkb_keymap_mod_get_index(xkb.keymap, XKB_MOD_NAME_ALT);
        xkb.indexes.shift = xkb_keymap_mod_get_index(xkb.keymap, XKB_MOD_NAME_SHIFT);
    },
    // enter
    [](void* data, struct wl_keyboard*, uint32_t serial, struct wl_surface* surface, struct wl_array*)
    {
        auto& seatData = static_cast<WlGlueHost*>(data)->seatData;

        auto it = seatData.clients.find(surface);
        if (it != seatData.clients.end())
            seatData.keyboard.target = *it;
    },
    // leave
    [](void* data, struct wl_keyboard*, uint32_t serial, struct wl_surface* surface)
    {
        auto& seatData = static_cast<WlGlueHost*>(data)->seatData;

        auto it = seatData.clients.find(surface);
        if (it != seatData.clients.end() && seatData.keyboard.target.first == it->first)
            seatData.keyboard.target = { nullptr, nullptr };
    },
    // key
    [](void* data, struct wl_keyboard*, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
    {
        auto& seatData = static_cast<WlGlueHost*>(data)->seatData;

        // IDK.
        key += 8;

        handleKeyEvent(seatData, key, state, time);

        if (!seatData.repeatInfo.rate)
            return;

        if (state == WL_KEYBOARD_KEY_STATE_RELEASED
            && seatData.repeatData.key == key) {
            if (seatData.repeatData.eventSource)
                g_source_remove(seatData.repeatData.eventSource);
            seatData.repeatData = { 0, 0, 0, 0 };
        } else if (state == WL_KEYBOARD_KEY_STATE_PRESSED
            && xkb_keymap_key_repeats(seatData.xkb.keymap, key)) {

            if (seatData.repeatData.eventSource)
                g_source_remove(seatData.repeatData.eventSource);

            seatData.repeatData = { key, time, state, g_timeout_add(seatData.repeatInfo.delay, static_cast<GSourceFunc>(repeatDelayTimeout), data) };
        }
    },
    // modifiers
    [](void* data, struct wl_keyboard*, uint32_t serial, uint32_t depressedMods, uint32_t latchedMods, uint32_t lockedMods, uint32_t group)
    {
        auto& xkb = static_cast<WlGlueHost*>(data)->seatData.xkb;

        xkb_state_update_mask(xkb.state, depressedMods, latchedMods, lockedMods, 0, 0, group);

        auto& modifiers = xkb.modifiers;
        modifiers = 0;
        auto component = static_cast<xkb_state_component>(XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED);
        if (xkb_state_mod_index_is_active(xkb.state, xkb.indexes.control, component))
            modifiers |= wpe_input_keyboard_modifier_control;
        if (xkb_state_mod_index_is_active(xkb.state, xkb.indexes.alt, component))
            modifiers |= wpe_input_keyboard_modifier_alt;
        if (xkb_state_mod_index_is_active(xkb.state, xkb.indexes.shift, component))
            modifiers |= wpe_input_keyboard_modifier_shift;
    },
    // repeat_info
    [](void* data, struct wl_keyboard*, int32_t rate, int32_t delay)
    {
        auto& seatData = static_cast<WlGlueHost*>(data)->seatData;

        auto& repeatInfo = seatData.repeatInfo;
        repeatInfo = { rate, delay };

        // A rate of zero disables any repeating.
        if (!rate) {
            auto& repeatData = seatData.repeatData;
            if (repeatData.eventSource) {
                g_source_remove(repeatData.eventSource);
                repeatData = { 0, 0, 0, 0 };
            }
        }
    },
};

static const struct wl_seat_listener s_seatListener = {
    // capabilities
    [](void* data, struct wl_seat* seat, uint32_t capabilities)
    {
        auto* host = static_cast<WlGlueHost*>(data);
        auto& seatData = host->seatData;

        // WL_SEAT_CAPABILITY_POINTER
        const bool hasPointerCap = capabilities & WL_SEAT_CAPABILITY_POINTER;
        if (hasPointerCap && !seatData.pointer.object) {
            seatData.pointer.object = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(seatData.pointer.object, &g_pointerListener, host);
        }
        if (!hasPointerCap && seatData.pointer.object) {
            wl_pointer_destroy(seatData.pointer.object);
            seatData.pointer.object = nullptr;
        }

        // WL_SEAT_CAPABILITY_KEYBOARD
        const bool hasKeyboardCap = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
        if (hasKeyboardCap && !seatData.keyboard.object) {
            seatData.keyboard.object = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(seatData.keyboard.object, &g_keyboardListener, host);
        }
        if (!hasKeyboardCap && seatData.keyboard.object) {
            wl_keyboard_destroy(seatData.keyboard.object);
            seatData.keyboard.object = nullptr;
        }
    },
    // name
    [](void*, struct wl_seat*, const char*) { }
};

struct wlglue_window_client;

struct WlGlueWindow {
    struct WlGlueHost* host;
    struct wlglue_window_client* client;

    struct wl_surface* surface;
    struct zxdg_surface_v6* xdg6Surface;
    struct zxdg_toplevel_v6* xdg6Toplevel;

    struct wl_egl_window* eglWindow;
    EGLContext eglContext;
    EGLSurface eglSurface;

    GLuint program;
    GLuint textureUniform;
    GLuint viewTexture;

    struct {
        struct wl_resource* buffer_resource;
        EGLImageKHR image;
    } committed;
};

static EGLint s_configAttributes[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 1,
    EGL_GREEN_SIZE, 1,
    EGL_BLUE_SIZE, 1,
    EGL_ALPHA_SIZE, 0,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
};

static EGLint s_contextAttributes[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
};

static PFNEGLCREATEIMAGEKHRPROC createImage;
static PFNEGLDESTROYIMAGEKHRPROC destroyImage;
static PFNEGLQUERYWAYLANDBUFFERWL queryBuffer;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC imageTargetTexture2D;

extern "C" {

struct WlGlueHost*
wlglue_host_create()
{
    WlGlueHost* host = new WlGlueHost;
    if (!host)
        return nullptr;

    host->display = wl_display_connect(nullptr);
    if (!host->display)
        return nullptr;

    {
        auto* registry = wl_display_get_registry(host->display);
        wl_registry_add_listener(registry, &s_registryListener, host);
        wl_display_roundtrip(host->display);

        if (host->xdg_v6)
            zxdg_shell_v6_add_listener(host->xdg_v6, &s_xdg6ShellListener, nullptr);

        if (host->seat)
            wl_seat_add_listener(host->seat, &s_seatListener, host);

        host->seatData.xkb.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        host->seatData.xkb.composeTable = xkb_compose_table_new_from_locale(host->seatData.xkb.context, setlocale(LC_CTYPE, nullptr), XKB_COMPOSE_COMPILE_NO_FLAGS);
        if (host->seatData.xkb.composeTable)
            host->seatData.xkb.composeState = xkb_compose_state_new(host->seatData.xkb.composeTable, XKB_COMPOSE_STATE_NO_FLAGS);
    }

    host->eventSource = g_source_new(&EventSource::sourceFuncs, sizeof(EventSource));
    {
        auto& source = *reinterpret_cast<EventSource*>(host->eventSource);
        source.display = host->display;

        source.pfd.fd = wl_display_get_fd(host->display);
        source.pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
        source.pfd.revents = 0;
        g_source_add_poll(&source.source, &source.pfd);

        g_source_set_priority(&source.source, G_PRIORITY_HIGH + 30);
        g_source_set_can_recurse(&source.source, TRUE);
        g_source_attach(&source.source, g_main_context_get_thread_default());
    }

    host->eglDisplay = eglGetDisplay(host->display);
    if (host->eglDisplay == EGL_NO_DISPLAY)
        return nullptr;
    if (!eglInitialize(host->eglDisplay, nullptr, nullptr))
        return nullptr;

    return host;
}

EGLDisplay
wlglue_host_get_egl_display(struct WlGlueHost* host)
{
    return host->eglDisplay;
}

struct WlGlueWindow*
wlglue_window_create(struct WlGlueHost* host, struct wlglue_window_client* client)
{
    WlGlueWindow* window = new WlGlueWindow;
    if (!window)
        return nullptr;

    window->host = host;
    window->client = client;

    EGLConfig eglConfig;
    {
        EGLint count = 0;
        if (!eglGetConfigs(host->eglDisplay, nullptr, 0, &count) || count < 1)
            return nullptr;

        EGLConfig* configs = g_new0(EGLConfig, count);
        EGLint matched = 0;
        if (eglChooseConfig(host->eglDisplay, s_configAttributes, configs, count, &matched) && !!matched)
            eglConfig = configs[0];
        g_free(configs);
    }

    if (!eglBindAPI(EGL_OPENGL_ES_API))
        return nullptr;

    window->eglContext = eglCreateContext(host->eglDisplay, eglConfig, EGL_NO_CONTEXT, s_contextAttributes);
    if (!window->eglContext)
        return nullptr;

    window->surface = wl_compositor_create_surface(host->compositor);
    if (host->xdg_v6) {
        window->xdg6Surface = zxdg_shell_v6_get_xdg_surface(host->xdg_v6, window->surface);
        zxdg_surface_v6_add_listener(window->xdg6Surface, &s_xdg6SurfaceListener, nullptr);
        window->xdg6Toplevel = zxdg_surface_v6_get_toplevel(window->xdg6Surface);
        if (window->xdg6Toplevel) {
            zxdg_toplevel_v6_add_listener(window->xdg6Toplevel, &s_xdg6ToplevelListener, nullptr);
            zxdg_toplevel_v6_set_title(window->xdg6Toplevel, "WPE");
            wl_surface_commit(window->surface);
        }
    }

    window->eglWindow = wl_egl_window_create(window->surface, 1280, 720);

    auto createPlatformWindowSurface =
        reinterpret_cast<PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC>(eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT"));
    window->eglSurface = createPlatformWindowSurface(host->eglDisplay, eglConfig, window->eglWindow, nullptr);
    if (!window->eglSurface)
        return nullptr;

    eglMakeCurrent(host->eglDisplay, window->eglSurface, window->eglSurface, window->eglContext);

    {
        static const char* vertexShaderSource =
            "attribute vec2 pos;\n"
            "attribute vec2 texture;\n"
            "varying vec2 v_texture;\n"
            "void main() {\n"
            "  v_texture = texture;\n"
            "  gl_Position = vec4(pos, 0, 1);\n"
            "}\n";
        static const char* fragmentShaderSource =
            "precision mediump float;\n"
            "uniform sampler2D u_texture;\n"
            "varying vec2 v_texture;\n"
            "void main() {\n"
            "  gl_FragColor = texture2D(u_texture, v_texture);\n"
            "}\n";

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
        glCompileShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
        glCompileShader(fragmentShader);

        window->program = glCreateProgram();
        glAttachShader(window->program, vertexShader);
        glAttachShader(window->program, fragmentShader);
        glLinkProgram(window->program);

        glBindAttribLocation(window->program, 0, "pos");
        glBindAttribLocation(window->program, 1, "texture");
        window->textureUniform = glGetUniformLocation(window->program, "u_texture");
    }
    {
        glGenTextures(1, &window->viewTexture);
        glBindTexture(GL_TEXTURE_2D, window->viewTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1280, 720, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    host->seatData.clients.insert({ window->surface, client });

    return window;
}

static const struct wl_callback_listener s_frameListener = {
    // frame
    [](void* data, struct wl_callback* callback, uint32_t)
    {
        if (callback)
            wl_callback_destroy(callback);

        auto* window = static_cast<struct WlGlueWindow*>(data);
        window->client->frame_displayed();

        if (window->committed.image)
            destroyImage(window->host->eglDisplay, window->committed.image);
        if (window->committed.buffer_resource)
            window->client->release_buffer_resource(window->committed.buffer_resource);
        window->committed = { nullptr, nullptr };
    },
};

void
wlglue_window_display_buffer(struct WlGlueWindow* window, struct wl_resource* buffer_resource)
{
    eglMakeCurrent(window->host->eglDisplay, window->eglSurface, window->eglSurface,
        window->eglContext);

    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!createImage) {
        createImage = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
        destroyImage = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
        queryBuffer = reinterpret_cast<PFNEGLQUERYWAYLANDBUFFERWL>(eglGetProcAddress("eglQueryWaylandBufferWL"));
        imageTargetTexture2D = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    }

    glUseProgram(window->program);

    {
        static EGLint s_imageAttributes[] = {
            EGL_WAYLAND_PLANE_WL, 0,
            EGL_NONE
        };
        EGLImageKHR image = createImage(window->host->eglDisplay, EGL_NO_CONTEXT,
            EGL_WAYLAND_BUFFER_WL, buffer_resource, s_imageAttributes);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, window->viewTexture);
        imageTargetTexture2D(GL_TEXTURE_2D, image);
        glUniform1i(window->textureUniform, 0);

        window->committed = { buffer_resource, image };
    }

    static const GLfloat s_vertices[4][2] = {
        { -1.0, 1.0 },
        { 1.0, 1.0 },
        { -1.0, -1.0 },
        { 1.0, -1.0 },
    };

    static const GLfloat s_texturePos[4][2] = {
        { 0, 0 },
        { 1, 0 },
        { 0, 1 },
        { 1, 1 },
    };

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, s_vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, s_texturePos);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    struct wl_callback* callback = wl_surface_frame(window->surface);
    wl_callback_add_listener(callback, &s_frameListener, window);

    eglSwapBuffers(window->host->eglDisplay, window->eglSurface);
}

}

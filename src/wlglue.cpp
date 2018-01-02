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
    GSource* eventSource;

    EGLDisplay eglDisplay;
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
    WlGlueHost* host = g_new0(WlGlueHost, 1);
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

struct wlglue_window_client {
    void (*frame_displayed)();
    void (*release_buffer_resource)(struct wl_resource*);
};

struct WlGlueWindow*
wlglue_window_create(struct WlGlueHost* host, struct wlglue_window_client* client)
{
    WlGlueWindow* window = g_new0(WlGlueWindow, 1);
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

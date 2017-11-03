#include <glib.h>

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <cstdio>

struct X11Host {
    Display* display;
    xcb_connection_t* connection;
    xcb_screen_t* screen;

    EGLDisplay eglDisplay;
};

struct X11Window {
    X11Host* host;

    xcb_window_t window;
    EGLContext eglContext;
    EGLSurface eglSurface;
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

extern "C" {

X11Host*
x11_host_create()
{
    fprintf(stderr, "x11_host_create()\n");
    X11Host* host = g_new0(X11Host, 1);
    if (!host)
        return nullptr;

    host->display = XOpenDisplay(nullptr);
    if (!host->display) {
        g_free(host);
        return nullptr;
    }

    host->connection = XGetXCBConnection(host->display);
    if (!host->connection) {
        XCloseDisplay(host->display);
        g_free(host);
        return nullptr;
    }

    XSetEventQueueOwner(host->display, XCBOwnsEventQueue);
    if (xcb_connection_has_error(host->connection)) {
        XCloseDisplay(host->display);
        g_free(host);
        return nullptr;
    }

    host->screen =
        [&host] {
            int screen_number = XDefaultScreen(host->display);
            xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(host->connection));
            for (int i = 0; !!iter.rem; xcb_screen_next(&iter), ++i) {
                if (i == screen_number)
                    return iter.data;
            }
            return xcb_setup_roots_iterator(xcb_get_setup(host->connection)).data;
        }();

    auto getPlatformDisplay =
        reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!getPlatformDisplay) {
        XCloseDisplay(host->display);
        g_free(host);
        return nullptr;
    }

    host->eglDisplay = getPlatformDisplay(EGL_PLATFORM_X11_KHR, host->display, nullptr);
    if (host->eglDisplay == EGL_NO_DISPLAY) {
        XCloseDisplay(host->display);
        g_free(host);
        return nullptr;
    }

    if (!eglInitialize(host->eglDisplay, nullptr, nullptr)) {
        XCloseDisplay(host->display);
        g_free(host);
        return nullptr;
    }

    fprintf(stderr, "\treturning %p, eglDisplay %p\n", host, host->eglDisplay);

    return host;
}

EGLDisplay
x11_host_get_egl_display(X11Host* host)
{
    return host->eglDisplay;
}

X11Window*
x11_window_create(X11Host* host)
{
    if (!host)
        return nullptr;

    X11Window* window = g_new0(X11Window, 1);
    if (!window)
        return nullptr;

    window->host = host;

    uint32_t mask = XCB_CW_EVENT_MASK | XCB_CW_CURSOR;
    uint32_t values[2] = { XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY, 0 };

    window->window = xcb_generate_id(host->connection);
    xcb_create_window(host->connection, XCB_COPY_FROM_PARENT,
        window->window, host->screen->root, 0, 0, 1280, 720,
        0, XCB_WINDOW_CLASS_INPUT_OUTPUT, host->screen->root_visual,
        mask, values);

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

    auto createPlatformWindowSurface =
        reinterpret_cast<PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC>(eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT"));
    window->eglSurface = createPlatformWindowSurface(host->eglDisplay, eglConfig, &window->window, nullptr);
    if (!window->eglSurface)
        return nullptr;

    xcb_map_window(host->connection, window->window);
    xcb_flush(host->connection);
}

}

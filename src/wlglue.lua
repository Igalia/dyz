local ffi = require("ffi")

local lib = ffi.load("wlglue.so")

ffi.cdef[[
    typedef void* EGLDisplay;

    struct WlGlueHost* wlglue_host_create();
    EGLDisplay wlglue_host_get_egl_display(struct WlGlueHost* host);

    struct wlglue_window_client {
        void (*frame_displayed)();
        void (*release_buffer_resource)(struct wl_resource* buffer_resource);
    };

    struct WlGlueWindow* wlglue_window_create(struct WlGlueHost* host, struct wlglue_window_client* client);
    void wlglue_window_display_buffer(struct WlGlueWindow* host, struct wl_resource* buffer_resource);
]]

return lib

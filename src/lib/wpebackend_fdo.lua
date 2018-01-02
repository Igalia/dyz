local ffi = require("ffi")

local lib = ffi.load("libWPEBackend-fdo.so")

ffi.cdef[[
    typedef void* EGLDisplay;
    struct wpe_view_backend_exportable_fdo_client {
        void (*export_buffer_resource)(void* data, struct wl_resource* buffer_resource);
    };

    void
    wpe_fdo_initialize_for_egl_display(EGLDisplay);

    struct wpe_view_backend_exportable_fdo*
    wpe_view_backend_exportable_fdo_create(struct wpe_view_backend_exportable_fdo_client*, void*, uint32_t width, uint32_t height);
    void
    wpe_view_backend_exportable_fdo_destroy(struct wpe_view_backend_exportable_fdo*);
    struct wpe_view_backend*
    wpe_view_backend_exportable_fdo_get_view_backend(struct wpe_view_backend_exportable_fdo*);
    void
    wpe_view_backend_exportable_fdo_dispatch_frame_complete(struct wpe_view_backend_exportable_fdo*);
    void
    wpe_view_backend_exportable_fdo_dispatch_release_buffer(struct wpe_view_backend_exportable_fdo*, struct wl_resource*);
]]

return lib

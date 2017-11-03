local ffi = require("ffi")

local lib = ffi.load("x11host.so")

ffi.cdef[[
    typedef void* EGLDisplay;

    struct X11Host*
    x11_host_create();
    EGLDisplay
    x11_host_get_egl_display(struct X11Host* host);

    struct X11Window*
    x11_window_create(struct X11Host* host);
]]

return lib

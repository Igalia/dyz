local ffi = require("ffi")

local lib = ffi.load("libWPEBackend.so")

ffi.cdef[[
    struct wpe_view_backend;
    struct wpe_input_axis_event;
    struct wpe_input_keyboard_event;
    struct wpe_input_pointer_event;
    struct wpe_input_touch_event;

    void
    wpe_view_backend_dispatch_keyboard_event(struct wpe_view_backend*, struct wpe_input_keyboard_event*);

    void
    wpe_view_backend_dispatch_pointer_event(struct wpe_view_backend*, struct wpe_input_pointer_event*);

    void
    wpe_view_backend_dispatch_axis_event(struct wpe_view_backend*, struct wpe_input_axis_event*);

    void
    wpe_view_backend_dispatch_touch_event(struct wpe_view_backend*, struct wpe_input_touch_event*);
]]

return lib

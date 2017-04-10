local ffi = require("ffi")

local lib = ffi.load("libglib-2.0.so")

ffi.cdef[[
    typedef struct _GMainContext GMainContext;
    typedef struct _GMainLoop GMainLoop;

    typedef int gboolean;

    GMainContext* g_main_context_default (void);

    GMainLoop* g_main_loop_new (GMainContext *context, gboolean is_running);
    void g_main_loop_run (GMainLoop *loop);
    void g_main_loop_unref (GMainLoop *loop);
]]

return lib

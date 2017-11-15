local ffi = require("ffi")

local lib = ffi.load("libgobject-2.0.so")

ffi.cdef[[
    typedef struct _GMainContext GMainContext;
    typedef struct _GMainLoop GMainLoop;
    typedef struct _GClosure GClosure;

    typedef void (*GCallback)(void);
    typedef void (*GClosureNotify)(void *data, GClosure *closure);
    typedef int (*GSourceFunc)(void *data);

    typedef enum { G_CONNECT_AFTER = 1, G_CONNECT_SWAPPED = 2 } GConnectFlags;

    typedef int gboolean;
    typedef void* gpointer;
    typedef size_t GType;

    GMainContext* g_main_context_default (void);

    GMainLoop* g_main_loop_new (GMainContext *context, gboolean is_running);
    void g_main_loop_quit (GMainLoop *loop);
    void g_main_loop_run (GMainLoop *loop);
    void g_main_loop_unref (GMainLoop *loop);

    gpointer g_object_new (GType object_type, const char *first_property_name, ...);
    void g_object_unref (gpointer object);

    unsigned long g_signal_connect_data (gpointer instance, const char *detailed_signal, GCallback c_handler, gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags);
]]

return lib

local ffi = require("ffi")

local lib = ffi.load("libglib-2.0.so")

ffi.cdef[[
    typedef struct _GMainContext GMainContext;
    typedef struct _GMainLoop GMainLoop;
    typedef struct _GPollFD GPollFD;
    typedef struct _GSource GSource;
    typedef struct _GSourceFuncs GSourceFuncs;

    typedef int gboolean;
    typedef void* gpointer;
    typedef int gint;
    typedef unsigned int guint;

    GMainContext* g_main_context_new (void);
    GMainContext* g_main_context_default (void);
    GMainContext* g_main_context_get_thread_default (void);
    void g_main_context_push_thread_default (GMainContext* context);
    void g_main_context_pop_thread_default (GMainContext* context);

    GMainLoop* g_main_loop_new (GMainContext *context, gboolean is_running);
    void g_main_loop_run (GMainLoop *loop);
    void g_main_loop_unref (GMainLoop *loop);

    void g_object_unref (gpointer object);

    GSource* g_source_new (GSourceFuncs *source_funcs, guint struct_size);
    void g_source_add_poll (GSource *source , GPollFD *fd);
    void g_source_set_name (GSource *source, const char *name);
    void g_source_set_priority (GSource *source, gint priority);
    void g_source_set_can_recurse (GSource *source, gboolean can_recurse);
    guint g_source_attach (GSource *source, GMainContext *context);
]]

return lib

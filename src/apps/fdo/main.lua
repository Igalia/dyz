
local module = {}

local bit = require("bit")
local ffi = require("ffi")
local drm = require("lib.drm")
local egl = require("lib.egl")
local gbm = require("lib.gbm")
local glib = require("lib.glib")
local libc = require("lib.libc")
local wk = require("lib.wpewebkit_capi")
local wpebackend_fdo = require("lib.wpebackend-fdo")

ffi.cdef[[

typedef int             gint;
typedef unsigned int    guint;
typedef unsigned short  gushort;

typedef struct _GSourceCallbackFuncs    GSourceCallbackFuncs;
typedef struct _GSourceFuncs            GSourceFuncs;
typedef struct _GSList GSList;

typedef struct _GSource GSource;
typedef struct _GSourcePrivate GSourcePrivate;

typedef gboolean (*GSourceFunc)       (gpointer user_data);
typedef void (*GSourceDummyMarshal) (void);

struct _GSource
{
  /*< private >*/
  gpointer callback_data;
  GSourceCallbackFuncs *callback_funcs;

  const GSourceFuncs *source_funcs;
  guint ref_count;

  GMainContext *context;

  gint priority;
  guint flags;
  guint source_id;

  GSList *poll_fds;
  
  GSource *prev;
  GSource *next;

  char    *name;

  GSourcePrivate *priv;
};

struct _GSourceFuncs
{
  gboolean (*prepare)  (GSource    *source,
                        gint       *timeout_);
  gboolean (*check)    (GSource    *source);
  gboolean (*dispatch) (GSource    *source,
                        GSourceFunc callback,
                        gpointer    user_data);
  void     (*finalize) (GSource    *source); /* Can be NULL */

  /*< private >*/
  /* For use by g_source_set_closure */
  GSourceFunc     closure_callback;        
  GSourceDummyMarshal closure_marshal; /* Really is of type GClosureMarshal */
};

typedef struct _GPollFD GPollFD;

struct _GPollFD
{
  gint		fd;
  gushort 	events;
  gushort 	revents;
};



struct EventSource {
    GSource source;
    GPollFD pfd;

    int drmFd;
    drmEventContext eventContext;
};

]]

function module.run(args)
    -- glib

    local context = glib.g_main_context_new()
    local loop = glib.g_main_loop_new(context, 0)
    print("glib context:", context)
    local exportable_ref = nil

    glib.g_main_context_push_thread_default(context)

    --

    -- 524290 -- O_RDWR | O_CLOEXEC
    local drm_data = {
        fd = -1,
        resources = nil,
        connector = nil,
        mode = nil,
        width = 0,
        height = 0,
        encoder = nil,
        crtcId = 0,
        connectorId = 0,
    }
    drm_data.fd = libc.open("/dev/dri/card0", 524290);
    print("fd: ", drm_data.fd)

    local magic = ffi.new("drm_magic_t[1]");
    local ret = drm.drmGetMagic(drm_data.fd, magic);
    print("drmGetMagic:", ret)
    ret = drm.drmAuthMagic(drm_data.fd, magic[0])
    print("drmAuthMagic:", ret)

    drm_data.resources = drm.drmModeGetResources(drm_data.fd)
    print("drmModeGetResources:", drm_data.resources)
    print("drm_data.resources.count_connectors:", drm_data.resources.count_connectors)

    for i = 1, drm_data.resources.count_connectors do
        local connector = drm.drmModeGetConnector(drm_data.fd, drm_data.resources.connectors[i - 1])
        if connector.connection == drm.DRM_MODE_CONNECTED then
            drm_data.connector = connector
            break
        end
    end

    print("drm_data.connector:", drm_data.connector, drm_data.connector.count_modes)

    local current_area = 0
    for i = 1, drm_data.connector.count_modes do
        local mode = drm_data.connector.modes[i - 1]
        print("mode:", mode)

        local area = mode.hdisplay * mode.vdisplay
        if area > current_area then
            drm_data.mode = mode
            drm_data.width = mode.hdisplay
            drm_data.height = mode.vdisplay
            current_area = area
        end
    end

    print("size:", drm_data.width, drm_data.height)

    for i = 1, drm_data.resources.count_encoders do
        local encoder = drm.drmModeGetEncoder(drm_data.fd, drm_data.resources.encoders[i - 1])
        if encoder.encoder_id == drm_data.connector.encoder_id then
            drm_data.encoder = encoder
            break
        end
    end

    drm_data.crtcId = drm_data.encoder.crtc_id
    drm_data.connectorId = drm_data.connector.connector_id
    print("drm_data.encoder:", drm_data.encoder, drm_data.crtcId, drm_data.connectorId)

    --

    local gbm_data = {
        device = -1,
    }
    gbm_data.device = gbm.gbm_create_device(drm_data.fd)
    print("gbm_data.device:", gbm_data.device)

    --

    local egl_data = {
        display = nil,
    }

    egl_data.display = egl.eglGetDisplay(gbm_data.device)
    print("egl_data.display:", egl_data.display)
    ret = egl.eglInitialize(egl_data.display, nil, nil)
    print("ret:", ret)

    wpebackend_fdo.wpe_renderer_host_exportable_fdo_initialize(egl_data.display)

    --

    local buffer_data = {
        locked = {
            valid = false,
            resource = nil,
        },
        pending = {
            valid = false,
            resource = nil,
        },
    }

    local glib_data = {
        callback_funcs = nil,
        source = nil,
    }

    glib_data.callback_funcs = ffi.new("GSourceFuncs")
    glib_data.callback_funcs.prepare = nil
    glib_data.callback_funcs.check =
        function(source)
            local event_source = ffi.cast("struct EventSource*", source)
            if event_source.pfd.revents > 0 then
                return true
            end
            return false
        end
    glib_data.callback_funcs.dispatch =
        function(source, source_func, data)
            local event_source = ffi.cast("struct EventSource*", source)

            -- 1 == G_IO_IN
            if bit.band(event_source.pfd.revents, 1) then
                drm.drmHandleEvent(event_source.drmFd, event_source.eventContext)
            end

            -- 24 == G_IO_ERR | G_IO_HUP
            if bit.band(event_source.pfd.revents, 24) ~= 0 then
                return false
            end
            return true
        end
    glib_data.callback_funcs.finalize = nil

    glib_data.source = glib.g_source_new(glib_data.callback_funcs, ffi.sizeof("struct EventSource"))
    print("glib_data.source:", glib_data.source)

    local event_source = ffi.cast("struct EventSource*", glib_data.source)
    print("event_source:", event_source)
    event_source.pfd.fd = drm_data.fd
    event_source.pfd.events = 25 -- G_IO_IN | G_IO_ERR | G_IO_HUP
    event_source.pfd.revents = 0
    print("event_source.pfd:", event_source.pfd)

    event_source.drmFd = drm_data.fd
    event_source.eventContext = ffi.new("drmEventContext")
    event_source.eventContext.version = drm.DRM_EVENT_CONTEXT_VERSION
    event_source.eventContext.vblank_handler = nil
    event_source.eventContext.page_flip_handler =
        function(fd, sequence, tv_sec, tv_usec, user_data)
            print("drmEventContext::page_flip_handler()", fd, sequence, tv_sec, tv_usec, user_data)

            wpebackend_fdo.wpe_view_backend_exportable_fdo_dispatch_frame_complete(exportable_ref)

            print("buffer_data.locked:", buffer_data.locked.valid, buffer_data.locked.resource)
            print("buffer_data.pending:", buffer_data.pending.valid, buffer_data.pending.resource)

            local previous_locked = {
                valid = buffer_data.locked.valid,
                resource = buffer_data.locked.resource,
            }
            buffer_data.locked = {
                valid = buffer_data.pending.valid,
                resource = buffer_data.pending.resource,
            }
            buffer_data.pending = {
                valid = false,
                resource = nil,
            }

            print("buffer_data.locked:", buffer_data.locked.valid, buffer_data.locked.resource)
            print("buffer_data.pending:", buffer_data.pending.valid, buffer_data.pending.resource)

            print("previous_locked:", previous_locked.valid, previous_locked.resource)

            if previous_locked.valid then
                wpebackend_fdo.wpe_view_backend_exportable_fdo_dispatch_release_buffer(exportable_ref, previous_locked.resource)
            end
        end

    glib.g_source_add_poll(glib_data.source, event_source.pfd)
    glib.g_source_set_name(glib_data.source, "dyz::fdo")
    glib.g_source_set_priority(glib_data.source, -70)
    glib.g_source_set_can_recurse(glib_data.source, true)
    glib.g_source_attach(glib_data.source, context)

    --

    local page_configuration = wk.PageConfiguration()

    page_configuration:set_context(wk.Context())
    page_configuration:set_page_group(wk.PageGroup("WPEPageGroup"))

    local exportable_client = ffi.new("struct wpe_view_backend_exportable_fdo_client")
    print(exportable_client)
    print(exportable_client.export_buffer_resource)
    exportable_client.export_buffer_resource = function(data, buffer_resource)
        print("hoi", data, buffer_resource)
        local bo = gbm.gbm_bo_import(gbm_data.device, gbm.GBM_BO_IMPORT_WL_BUFFER, buffer_resource, gbm.GBM_BO_USE_SCANOUT)

        local bo_data = {
            width = gbm.gbm_bo_get_width(bo),
            height = gbm.gbm_bo_get_height(bo),
            stride = gbm.gbm_bo_get_stride(bo),
            prime = gbm.gbm_bo_get_handle(bo).u32,
        }
        print("bo:", bo, bo_data.width, bo_data.height)

        buffer_data.pending.valid = true
        buffer_data.pending.resource = buffer_resource

        local fbId = ffi.new("uint32_t[1]")

        ret = drm.drmModeAddFB(drm_data.fd, bo_data.width, bo_data.height,
            24, 32, bo_data.stride, bo_data.prime, fbId);
        print("ret:", ret, fbId[0])

        ret = drm.drmModePageFlip(drm_data.fd, drm_data.crtcId, fbId[0], drm.DRM_MODE_PAGE_FLIP_EVENT, nil)
        print("ret:", ret)
    end;
    print(exportable_client.export_buffer_resource)

    local exportable = wpebackend_fdo.wpe_view_backend_exportable_fdo_create(exportable_client, nil, drm_data.width, drm_data.height)
    exportable_ref = exportable
    local view_backend = wpebackend_fdo.wpe_view_backend_exportable_fdo_get_view_backend(exportable)

    print("running view")
    local view = wk.View(view_backend, page_configuration)

    local url = "https://www.duckduckgo.com"
    if #args > 0 then
        url = args[1]
    end

    view:get_page():load_url(url)

    glib.g_main_loop_run(loop);

    glib.g_main_loop_unref(loop);
end

return module


local module = {}

local ffi = require("ffi")
local glib = require("lib.glib")
local wpe = require("lib.wpewebkit_capi")
local wpebackend_fdo = require("lib.wpebackend_fdo")
local wlglue = require("wlglue")

function module.run(args)
    local context = glib.g_main_context_default()
    local loop = glib.g_main_loop_new(context, 0)

    local host = wlglue.wlglue_host_create()

    wpebackend_fdo.wpe_fdo_initialize_for_egl_display(wlglue.wlglue_host_get_egl_display(host))

    local window_client = ffi.new("struct wlglue_window_client")
    local window = wlglue.wlglue_window_create(host, window_client)

    local exportable_client = ffi.new("struct wpe_view_backend_exportable_fdo_client")
    local exportable = wpebackend_fdo.wpe_view_backend_exportable_fdo_create(exportable_client, nil, 1280, 720)
    local view_backend = wpebackend_fdo.wpe_view_backend_exportable_fdo_get_view_backend(exportable)

    window_client.frame_displayed =
        function()
            wpebackend_fdo.wpe_view_backend_exportable_fdo_dispatch_frame_complete(exportable)
        end
    window_client.release_buffer_resource =
        function(buffer_resource)
            wpebackend_fdo.wpe_view_backend_exportable_fdo_dispatch_release_buffer(exportable, buffer_resource)
        end

    exportable_client.export_buffer_resource =
        function(data, buffer_resource)
            wlglue.wlglue_window_display_buffer(window, buffer_resource)
        end

    local page_configuration = wpe.PageConfiguration()
    page_configuration:set_context(wpe.Context())
    page_configuration:set_page_group(wpe.PageGroup("WPEPageGroup"))
    local view = wpe.View(view_backend, page_configuration)

    local url = "https://www.duckduckgo.com"
    if #args > 0 then
        url = args[1]
    end

    view:get_page():load_url(url)

    glib.g_main_loop_run(loop)

    glib.gobject_unref(view)
    glib.g_main_loop_unref(loop)
end

return module

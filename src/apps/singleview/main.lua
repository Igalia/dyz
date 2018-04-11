
local module = {}

local ffi = require("ffi")
local glib = require("lib.glib")
local wpe = require("lib.wpewebkit_glibapi")
local wpebackend = require("lib.wpebackend")
local wpebackend_fdo = require("lib.wpebackend_fdo")
local wlglue = require("wlglue")

ffi.cdef[[
    typedef void (*WebContextAutomationStartedCallback) (WebKitWebContext*, WebKitAutomationSession*, WebKitWebView*);
    typedef WebKitWebView* (*AutomationSessionCreateWebViewCallback) (WebKitAutomationSession*, WebKitWebView*);
    typedef void (*WebViewCloseCallback)(WebKitWebView*, GMainLoop*);
]]

local function table_contains(table, item)
    for key, value in pairs(table) do
        if value == item then
            return true
        end
    end
    return false
end

local function g_signal_connect(obj, sig, ftype, f, data, flags)
    return glib.g_signal_connect_data(obj, sig,
        ffi.cast("GCallback", ffi.cast(ftype, f)), data, nil, flags or 0)
end

local function create_web_view_for_automation_cb(session, view)
    return view
end

local function automation_started_cb(web_context, session, view)
    g_signal_connect(session, "create-web-view", "AutomationSessionCreateWebViewCallback", create_web_view_for_automation_cb, view)
end

local function web_view_close_cb(view, loop)
    glib.g_main_loop_quit(loop);
end

function module.run(args)
    local context = glib.g_main_context_default()
    local loop = glib.g_main_loop_new(context, 0)

    local host = wlglue.wlglue_host_create()

    wpebackend_fdo.wpe_fdo_initialize_for_egl_display(wlglue.wlglue_host_get_egl_display(host))

    local window_client = ffi.new("struct wlglue_window_client")
    local window = wlglue.wlglue_window_create(host, window_client)

    local exportable_client = ffi.new("struct wpe_view_backend_exportable_fdo_client")
    local exportable = wpebackend_fdo.wpe_view_backend_exportable_fdo_create(exportable_client, nil, 1280, 720)
    local exportable_view_backend = wpebackend_fdo.wpe_view_backend_exportable_fdo_get_view_backend(exportable)

    window_client.frame_displayed =
        function()
            wpebackend_fdo.wpe_view_backend_exportable_fdo_dispatch_frame_complete(exportable)
        end
    window_client.release_buffer_resource =
        function(buffer_resource)
            wpebackend_fdo.wpe_view_backend_exportable_fdo_dispatch_release_buffer(exportable, buffer_resource)
        end
    window_client.dispatch_input_pointer_event =
        function(event)
            wpebackend.wpe_view_backend_dispatch_pointer_event(exportable_view_backend, event)
        end
    window_client.dispatch_input_axis_event =
        function(event)
            wpebackend.wpe_view_backend_dispatch_axis_event(exportable_view_backend, event)
        end
    window_client.dispatch_input_keyboard_event =
        function(event)
            wpebackend.wpe_view_backend_dispatch_keyboard_event(exportable_view_backend, event);
        end

    exportable_client.export_buffer_resource =
        function(data, buffer_resource)
            wlglue.wlglue_window_display_buffer(window, buffer_resource)
        end

    local automation_mode = table_contains(args, "--automation")
    local web_context
    if automation_mode then
        web_context = wpe.webkit_web_context_new_ephemeral()
    else
        web_context = wpe.webkit_web_context_get_default()
    end

    local view_backend = wpe.webkit_web_view_backend_new(
                        wpebackend_fdo.wpe_view_backend_exportable_fdo_get_view_backend(exportable),
                        wpebackend_fdo.wpe_view_backend_exportable_fdo_destroy,
                        exportable)
    local settings = wpe.webkit_settings_new_with_settings(
                        "allow-file-access-from-file-urls", true,
                        "allow-universal-access-from-file-urls", true,
                        "enable-write-console-messages-to-stdout", true,
                        nil)
    local view = glib.g_object_new(wpe.webkit_web_view_get_type(),
                        "backend", view_backend,
                        "web-context", web_context,
                        "settings", settings,
                        "is-controlled-by-automation", automation_mode,
                        nil)
    glib.g_object_unref(settings)
    g_signal_connect(view, "close", "WebViewCloseCallback", web_view_close_cb, loop)
    if automation_mode then
        wpe.webkit_web_context_set_automation_allowed(web_context, true)
        g_signal_connect(web_context, "automation-started", "WebContextAutomationStartedCallback", automation_started_cb, view)
    end

    local url
    if automation_mode then
        url = "about:blank"
    elseif #args > 0 then
        url = args[1]
    else
        url = "https://www.duckduckgo.com"
    end

    wpe.webkit_web_view_load_uri(view, url)

    glib.g_main_loop_run(loop)

    glib.g_object_unref(view)
    if automation_mode then
        glib.g_object_unref(web_context)
    end
    glib.g_main_loop_unref(loop)
end

return module

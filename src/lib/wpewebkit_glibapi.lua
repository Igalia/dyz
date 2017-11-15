local ffi = require("ffi")

local wk = ffi.load("libWPEWebKit.so")

ffi.cdef[[
    typedef struct _WebKitAutomationSession WebKitAutomationSession;
    typedef struct _WebKitWebContext WebKitWebContext;
    typedef struct _WebKitWebView WebKitWebView;
    typedef struct _WebKitSettings WebKitSettings;

    WebKitSettings* webkit_settings_new_with_settings (const char* first_setting_name, ...);

    WebKitWebContext* webkit_web_context_new_ephemeral ();
    WebKitWebContext* webkit_web_context_get_default ();
    void webkit_web_context_set_automation_allowed (WebKitWebContext* context, gboolean allowed);

    GType webkit_web_view_get_type ();
    WebKitWebView* webkit_web_view_new_with_context  (WebKitWebContext* context);
    WebKitWebView* webkit_web_view_new ();
    void webkit_web_view_set_settings (WebKitWebView* web_view, WebKitSettings* settings);
    void webkit_web_view_load_uri (WebKitWebView* web_view, const char* uri);
]]

return wk

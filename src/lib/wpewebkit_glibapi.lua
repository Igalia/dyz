local ffi = require("ffi")

local wk = ffi.load("libWPEWebKit.so")

ffi.cdef[[
    typedef struct _WebKitWebView WebKitWebView;
    typedef struct _WebKitSettings WebKitSettings;

    WebKitSettings* webkit_settings_new_with_settings (const char* first_setting_name, ...);

    WebKitWebView* webkit_web_view_new_with_settings  (WebKitSettings* settings);
    WebKitWebView* webkit_web_view_new ();
    void webkit_web_view_load_uri (WebKitWebView* web_view, const char* uri);
]]

return wk

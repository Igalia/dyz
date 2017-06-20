local ffi = require("ffi")

local wk = ffi.load("libWPEWebKit.so")

ffi.cdef[[
    typedef struct _WebKitWebView WebKitWebView;

    WebKitWebView* webkit_web_view_new ();
    void webkit_web_view_load_uri (WebKitWebView* web_view, const char* uri);
]]

return wk

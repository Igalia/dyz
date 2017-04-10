
local module = {}

local glib = require("lib.glib")
local wpewebkit = require("lib.wpewebkit")

function module.run(args)
    local context = glib.g_main_context_default()
    local loop = glib.g_main_loop_new(context, 0)

    local context = wpewebkit.WKContextCreate()
    local pageGroupIdentifier = wpewebkit.WKStringCreateWithUTF8CString("WPEPageGroup")
    local pageGroup = wpewebkit.WKPageGroupCreateWithIdentifier(pageGroupIdentifier)
    local pageConfiguration = wpewebkit.WKPageConfigurationCreate()
    wpewebkit.WKPageConfigurationSetContext(pageConfiguration, context)
    wpewebkit.WKPageConfigurationSetPageGroup(pageConfiguration, pageGroup)

    local view = wpewebkit.WKViewCreate(pageConfiguration)
    local page = wpewebkit.WKViewGetPage(view)

    local url = wpewebkit.WKURLCreateWithUTF8CString("https://inexorabletash.github.io/polyfill/demos/raf.html")
    wpewebkit.WKPageLoadURL(page, url)

    glib.g_main_loop_run(loop);

    glib.g_main_loop_unref(loop);
end

return module

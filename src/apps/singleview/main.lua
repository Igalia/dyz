
local module = {}

local glib = require("lib.glib")
local wpe = require("lib.wpewebkit_glibapi")

function module.run(args)
    local context = glib.g_main_context_default()
    local loop = glib.g_main_loop_new(context, 0)

    local settings = wpe.webkit_settings_new_with_settings(
                        "allow-file-access-from-file-urls", true,
                        "allow-universal-access-from-file-urls", true,
                        "enable-write-console-messages-to-stdout", true,
                        nil)

    local view = wpe.webkit_web_view_new_with_settings(settings)

    local url = "https://www.duckduckgo.com"
    if #args > 0 then
        url = args[1]
    end

    wpe.webkit_web_view_load_uri(view, url)

    glib.g_main_loop_run(loop)

    glib.gobject_unref(view)
    glib.g_main_loop_unref(loop)
end

return module

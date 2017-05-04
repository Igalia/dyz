
local module = {}

local glib = require("lib.glib")
local wpe = require("lib.wpe")

function module.run(args)
    local context = glib.g_main_context_default()
    local loop = glib.g_main_loop_new(context, 0)

    local page_configuration = wpe.PageConfiguration()

    page_configuration:set_context(wpe.Context())
    page_configuration:set_page_group(wpe.PageGroup("WPEPageGroup"))

    local view = wpe.View(page_configuration)

    local url = "https://www.duckduckgo.com"
    if #args > 0 then
        url = args[1]
    end

    view:get_page():load_url(url)

    glib.g_main_loop_run(loop);

    glib.g_main_loop_unref(loop);
end

return module

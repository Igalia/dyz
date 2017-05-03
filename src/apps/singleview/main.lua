
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
    view:get_page():load_url("https://inexorabletash.github.io/polyfill/demos/raf.html")

    glib.g_main_loop_run(loop);

    glib.g_main_loop_unref(loop);
end

return module

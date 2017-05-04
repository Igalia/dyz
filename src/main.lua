local ffi = require("ffi")
local singleview = require("apps.singleview.main")

ffi.cdef[[
    extern int argc;
    extern char** argv;
]]

function main ()
    local args = {}
    for i = 1, ffi.C.argc - 1 do
        table.insert(args, ffi.string(ffi.C.argv[i]))
    end

    singleview.run(args)
end

main()

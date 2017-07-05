local ffi = require("ffi")

local libc = ffi.load("libdrm.so")

ffi.cdef[[
    enum {
        O_RDWR = 02,
        O_CLOEXEC = 02000000,
    };

    int open(const char *pathname, int flags);
]]

return libc

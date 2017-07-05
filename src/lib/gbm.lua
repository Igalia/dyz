local ffi = require("ffi")

local drm = ffi.load("libgbm.so")

ffi.cdef[[

union gbm_bo_handle {
   void *ptr;
   int32_t s32;
   uint32_t u32;
   int64_t s64;
   uint64_t u64;
};

enum gbm_bo_flags {
    GBM_BO_USE_SCANOUT = (1 << 0),
    GBM_BO_USE_CURSOR = (1 << 1),
    GBM_BO_USE_CURSOR_64X64 = GBM_BO_USE_CURSOR,
    GBM_BO_USE_RENDERING = (1 << 2),
    GBM_BO_USE_WRITE = (1 << 3),
    GBM_BO_USE_LINEAR = (1 << 4),
};

enum {
    GBM_BO_IMPORT_WL_BUFFER = 0x5501,
};

struct gbm_device *
gbm_create_device(int fd);

struct gbm_bo *
gbm_bo_import(struct gbm_device *gbm, uint32_t type, void *buffer, uint32_t usage);

uint32_t
gbm_bo_get_width(struct gbm_bo *bo);

uint32_t
gbm_bo_get_height(struct gbm_bo *bo);

uint32_t
gbm_bo_get_stride(struct gbm_bo *bo);

union gbm_bo_handle
gbm_bo_get_handle(struct gbm_bo *bo);

]]

return drm

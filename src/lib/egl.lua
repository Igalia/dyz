local ffi = require("ffi")

local egl = ffi.load("libEGL.so")

ffi.cdef[[

typedef int32_t khronos_int32_t;
typedef khronos_int32_t EGLint;
typedef unsigned int EGLBoolean;

typedef void* EGLNativeDisplayType;
typedef void *EGLDisplay;

EGLDisplay eglGetDisplay (EGLNativeDisplayType display_id);
EGLBoolean eglInitialize (EGLDisplay dpy, EGLint *major, EGLint *minor);

]]

return egl

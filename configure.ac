AC_PREREQ([2.69])
AC_INIT([dyz], [0.1], [https://github.com/Igalia/dyz],
        [dyz], [https://github.com/Igalia/dyz])
AC_CONFIG_HEADER([src/config.h:src/config.h.in])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/dyz.c])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES([luajit], [luajit])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h unistd.h])

# Checks for library functions.
AC_CHECK_FUNCS([setenv])

# Define the dyz paths
LUA_VERSION=$(pkg-config --variable=abiver luajit)
AC_SUBST([LUA_VERSION])
dyz_luadir="$libdir/lua/$LUA_VERSION/dyz"
AC_SUBST([dyz_luadir])
dyz_luapath="$dyz_luadir/?.lua;?.lua"
AC_SUBST([dyz_luapath])

AH_TEMPLATE([DYZBINDIR], The base path of the dyz binaries)
AC_DEFINE_DIR(DYZBINDIR, bindir)

AH_TEMPLATE([DYZLUADIR], The base path of the dyz lua modules)
AC_DEFINE_DIR(DYZLUADIR, dyz_luadir)

AH_TEMPLATE([LUASEARCHPATH], The default search path of lua modules)
AC_DEFINE_DIR(LUASEARCHPATH, dyz_luapath)

AC_OUTPUT(src/Makefile)

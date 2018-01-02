-include ../config.mk

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
LUAJIT ?= luajit
CC ?= gcc

TARGET_MACHINE := $(shell $(CC) -dumpmachine)

ifneq (,$(findstring aarch64-,$(TARGET_MACHINE)))
	ARCH := arm64
else ifneq (,$(findstring arm-,$(TARGET_MACHINE)))
	ARCH := arm
else ifneq (,$(findstring mips-,$(TARGET_MACHINE)))
	ARCH := mips
else ifneq (,$(findstring x86_64-,$(TARGET_MACHINE)))
	ARCH := x64
else ifneq (,$(findstring i686-,$(TARGET_MACHINE)))
	ARCH := x86
else ifneq (,$(findstring i586-,$(TARGET_MACHINE)))
	ARCH := x86
else ifneq (,$(findstring i486-,$(TARGET_MACHINE)))
	ARCH := x86
else ifneq (,$(findstring i386-,$(TARGET_MACHINE)))
	ARCH := x86
endif

ifeq ($(strip $(ARCH)),)
$(error Unsupported target CPU: $(TARGET_MACHINE))
endif

LUA_SRCS := \
	apps/singleview/main.lua \
	entrypoint.lua \
	lib/glib.lua \
	lib/wpebackend_fdo.lua \
	lib/wpewebkit_glibapi.lua \
	main.lua \
	wlglue.lua

LUA_OBJS := \
	$(patsubst %.lua,%_lua.o,$(LUA_SRCS))

# Use := to avoid calling pkg-config each time this variables are used.
LUAJIT_CFLAGS := $(shell pkg-config luajit --cflags)
LUAJIT_LDLIBS := $(shell pkg-config luajit --libs)

WPEBACKEND_CFLAGS := $(shell pkg-config wpe --cflags)
WPEBACKEND_LDLIBS := $(shell pkg-config wpe --libs)

CFLAGS  += $(LUAJIT_CFLAGS)
LDFLAGS += $(LUAJIT_LDLIBS)

X11HOST_CFLAGS := $(shell pkg-config egl glib-2.0 x11-xcb --cflags)
X11HOST_LDLIBS := $(shell pkg-config egl glib-2.0 x11-xcb --libs)
libx11host.so: x11host.cpp
	$(CXX) -shared -std=c++11 -fPIC -o $@ $^ $(X11HOST_CFLAGS) $(X11HOST_LDLIBS)

WLGLUE_CFLAGS := $(shell pkg-config egl glib-2.0 wayland-client wayland-egl glesv2 --cflags)
WLGLUE_LDLIBS := $(shell pkg-config egl glib-2.0 wayland-client wayland-egl glesv2 --libs)
xdg-shell-unstable-v6-protocol.o: xdg-shell-unstable-v6-protocol.c
	$(CC) -c -o $@ $^ $(WLGLUE_CFLAGS)
libwlglue.so: wlglue.cpp xdg-shell-unstable-v6-protocol.o
	mkdir -p $(DESTDIR)$(LIBDIR)
	$(CXX) -shared -std=c++11 -fPIC -o $@ $^ $(WLGLUE_CFLAGS) $(WLGLUE_LDLIBS) -Wl,-rpath -Wl,$(LIBDIR)

dyz: main.o libwlglue.so $(LUA_OBJS)
	mkdir -p $(DESTDIR)$(BINDIR)
	$(CC) -o $@ $^ $(LDFLAGS) -Wl,--no-as-needed -Wl,-E -Wl,-rpath -Wl,$(LIBDIR)

%_lua.o: %.lua
	$(LUAJIT) -bg -a $(ARCH) -n $(subst /,.,$*) $< $@

libWPEBackend-dyz.so: loader.c
	$(CC) -shared -fPIC -o $@ $^ $(WPEBACKEND_CFLAGS) $(WPEBACKEND_LDLIBS)

all: dyz libwlglue.so libWPEBackend-dyz.so

install: dyz libwlglue.so libWPEBackend-dyz.so
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(LIBDIR)
	install -m755 -t $(DESTDIR)$(BINDIR) dyz
	install -m755 -t $(DESTDIR)$(LIBDIR) libwlglue.so
	install -m755 -t $(DESTDIR)$(LIBDIR) libWPEBackend-dyz.so
	ln -frs $(DESTDIR)$(LIBDIR)/libWPEBackend-dyz.so $(DESTDIR)$(LIBDIR)/libWPEBackend-default.so

clean:
	$(RM) $(LUA_OBJS) main.o dyz libwlglue.so

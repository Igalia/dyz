-include ../config.mk

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
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
	entrypoint.lua \
	main.lua \
	lib/glib.lua \
	lib/wpewebkit_glibapi.lua \
	apps/singleview/main.lua

LUA_OBJS := \
	$(patsubst %.lua,%_lua.o,$(LUA_SRCS))

# Use := to avoid calling pkg-config each time this variables are used.
LUAJIT_CFLAGS := $(shell pkg-config luajit --cflags)
LUAJIT_LDLIBS := $(shell pkg-config luajit --libs)

CFLAGS  += $(LUAJIT_CFLAGS)
LDFLAGS += $(LUAJIT_LDLIBS)

ifeq ($(strip $(LUAJIT)),)
  LUAJIT := $(shell pkg-config luajit --variable=prefix)/bin/luajit
endif

all: dyz

X11HOST_CFLAGS := $(shell pkg-config egl glib-2.0 x11-xcb --cflags)
X11HOST_LDLIBS := $(shell pkg-config egl glib-2.0 x11-xcb --libs)
libx11host.so: x11host.cpp
	$(CXX) -shared -std=c++11 -fPIC -o $@ $^ $(X11HOST_CFLAGS) $(X11HOST_LDLIBS)

WLGLUE_CFLAGS := $(shell pkg-config egl glib-2.0 wayland-client wayland-egl glesv2 --cflags)
WLGLUE_LDLIBS := $(shell pkg-config egl glib-2.0 wayland-client wayland-egl glesv2 --libs)
xdg-shell-unstable-v6-protocol.o: xdg-shell-unstable-v6-protocol.c
	$(CC) -c -o $@ $^ $(WLGLUE_CFLAGS)
libwlglue.so: wlglue.cpp xdg-shell-unstable-v6-protocol.o
	$(CXX) -shared -std=c++11 -fPIC -o $@ $^ $(WLGLUE_CFLAGS) $(WLGLUE_LDLIBS)

dyz: main.o libwlglue.so $(LUA_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -Wl,--no-as-needed -Wl,-E

%_lua.o: %.lua
	$(LUAJIT) -bg -a $(ARCH) -n $(subst /,.,$*) $< $@

install: dyz
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m755 -t $(DESTDIR)$(BINDIR) dyz

clean:
	$(RM) $(LUA_OBJS) main.o dyz

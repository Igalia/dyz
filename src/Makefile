-include ../config.mk

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
CC ?= gcc

LUA_SRCS := \
	entrypoint.lua \
	main.lua \
	lib/glib.lua \
	lib/wpe.lua \
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

dyz: main.o $(LUA_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -Wl,--no-as-needed -Wl,-E

%_lua.o: %.lua
	$(LUAJIT) -bg -n $(subst /,.,$*) $< $@

install: dyz
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m755 -t $(DESTDIR)$(BINDIR) dyz

clean:
	$(RM) $(LUA_OBJS) main.o dyz

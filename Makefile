all:
	$(MAKE) -C src $@

install:
	$(MAKE) -C src $@

clean:
	$(MAKE) -C src $@

distclean:
	@rm -f aclocal.m4 config.log config.status src/config.h src/config.h.in configure
	@rm -f src/Makefile src/dyz
	@rm -fr autom4te.cache

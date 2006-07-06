## Makefile.am

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure ac-tools/config.guess\
                       ac-tools/config.h.in ac-tools/config.h.in~\
                       ac-tools/config.sub ac-tools/depcomp\
                       ac-tools/install-sh ac-tools/missing\
                       ac-tools/mkinstalldirs ac-tools/compile\
                       $(PACKAGE)-$(VERSION).tar.gz\
                       $(PACKAGE)-$(VERSION).tar.bz2\
                       afd.spec\
                       errors
SUBDIRS              = src scripts etc
EXTRA_DIST           = ac-tools doc scripts afd.spec.in COPYING CREDITS\
                       Changelog FAQ KNOWN-BUGS README.configure THANKS TODO\
                       changes-1.2.x-1.3.x

rpm:	dist
	$(RPM) -ta $(PACKAGE)-$(VERSION).tar.bz2 </dev/null;

dist-hook:
	cp afd.spec $(distdir)

.PHONY: rpm

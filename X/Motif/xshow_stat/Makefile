#
# Makefile to build xshow_stat for the AFD
#

include ../../../Include.mk

INSTDIR   = ../../../..
COMDIR    = ../../../common
XCOMDIR   = ../common
LIB       = $(COMDIR)/libafd.a $(XCOMDIR)/libxafd.a
CFLAGS    = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../../../init_afd -I../../../statistics -I../afd_ctrl $(XINCLUDES)

XSTATPROG = xshow_stat
XSTATOBJS = callbacks.o\
            draw_stat.o\
            expose_handler_stat.o\
            get_data_points.o\
            setup_window.o\
            xshow_stat.o\
            window_size.o

PROGS     = $(XSTATPROG)
OBJS      = $(XSTATOBJS)
HEADERS   = ../../../init_afd/afddefs.h ../../../config.h ../afd_ctrl/afd_ctrl.h ../../../statistics/statdefs.h $(XCOMDIR)/x_common_defs.h xshow_stat.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(XSTATPROG) : $(XSTATOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(XSTATOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	chmod $(BINMODE) $(INSTDIR)/bin/$(XSTATPROG)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other dependencies
xshow_stat.o : ../../../version.h

#
# Makefile to build the MON_CTRL
#

include ../../../Include.mk

INSTDIR  = ../../../../bin
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(XCOMDIR)/libxafd.a $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../../init_afd -I../../.. -I../show_log -I../afd_ctrl -I../../../afd_mon $(XINCLUDES)

MONCPROG = mon_ctrl
MONCOBJS = mon_ctrl.o\
           check_afd_status.o\
           draw_mon_line.o\
           mon_expose_handler.o\
           mouse_handler.o\
           resize_mon_window.o\
           setup_mon_window.o\
           mon_window_size.o

PROGS    = $(MONCPROG)
OBJS     = $(MONCOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../config.h ../../../afd_mon/mondefs.h ../show_log/show_log.h ../afd_ctrl/afd_ctrl.h $(XCOMDIR)/x_common_defs.h mon_ctrl.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(MONCPROG) : $(MONCOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(MONCOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(MATHSLIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR) && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)
	chmod $(BINMODE) $(INSTDIR)/$(MONCPROG)

# Other dependencies
mon_ctrl.o : ../../../version.h ../../../init_afd/permission.h
setup_mon_window.o mon_expose_handler.o mouse_handler.o : ../../../init_afd/permission.h

#
# Makefile to build the DIR_CTRL
#

include ../../../Include.mk

INSTDIR  = ../../../../bin
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(XCOMDIR)/libxafd.a $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../../init_afd -I../../.. -I../show_log -I../afd_ctrl $(XINCLUDES)

DIRCPROG = dir_ctrl
DIRCOBJS = check_dir_status.o\
           dir_ctrl.o\
           dir_expose_handler.o\
           dir_window_size.o\
           draw_dir_line.o\
           mouse_handler.o\
           resize_dir_window.o\
           setup_dir_window.o

PROGS    = $(DIRCPROG)
OBJS     = $(DIRCOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../config.h ../afd_ctrl/afd_ctrl.h $(XCOMDIR)/x_common_defs.h dir_ctrl.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(DIRCPROG) : $(DIRCOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(DIRCOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(MATHSLIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR) && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)
	chmod $(BINMODE) $(INSTDIR)/$(DIRCPROG)

# Other dependencies
dir_ctrl.o : ../../../version.h ../../../init_afd/permission.h
setup_dir_window.o dir_expose_handler.o mouse_handler.o : ../../../init_afd/permission.h
mouse_handler.o : ../show_log/show_log.h

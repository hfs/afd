#
# Makefile to build the AFD_CTRL (Automatic File Distributor Control)
#

include ../../../Include.mk

INSTDIR  = ../../../..
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(XCOMDIR)/libxafd.a $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../../init_afd -I../../.. -I../show_log $(XINCLUDES)

AFDCPROG = afd_ctrl
AFDCOBJS = afd_ctrl.o\
           calc_but_coord.o\
           check_host_status.o\
           check_status.o\
           check_tv_status.o\
           create_tv_window.o\
           draw_line.o\
           draw_tv_line.o\
           expose_handler.o\
           find_host_dialog.o\
           init_jd_structure.o\
           locate_xy_column.o\
           mouse_handler.o\
           resize_tv_window.o\
           resize_window.o\
           setup_tv_window.o\
           setup_window.o\
           tv_window_size.o\
           window_size.o

PROGS    = $(AFDCPROG)$(EXE_EXT)
OBJS     = $(AFDCOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../config.h $(XCOMDIR)/x_common_defs.h afd_ctrl.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(AFDCPROG)$(EXE_EXT) : $(AFDCOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(AFDCOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(MATHSLIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	chmod $(SUIDMODE) $(INSTDIR)/bin/$(AFDCPROG)$(EXE_EXT)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other dependencies
afd_ctrl.o : ../../../version.h ../../../init_afd/permission.h
setup_window.o expose_handler.o mouse_handler.o : ../../../init_afd/permission.h
mouse_handler.o : ../show_log/show_log.h

#
# Makefile to build edit_hc for the AFD
#

include ../../../Include.mk

INSTDIR  = ../../../../bin
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(COMDIR)/libafd.a $(XCOMDIR)/libxafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../../../init_afd -I../../../amg -I../afd_ctrl

EHCPROG  = edit_hc
EHCOBJS  = accept_drop.o\
           edit_hc.o\
           edit_hc_callbacks.o\
           remove_host.o\
           start_drag.o

PROGS    = $(EHCPROG)
OBJS     = $(EHCOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../config.h ../afd_ctrl/afd_ctrl.h $(XCOMDIR)/x_common_defs.h edit_hc.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(EHCPROG) : $(EHCOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(EHCOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR) && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)
	chmod $(BINMODE) $(INSTDIR)/$(EHCPROG)

# Other dependencies
edit_hc.o : ../../../version.h ../../../init_afd/permission.h source.h source_mask.h no_source.h no_source_mask.h

#
# Makefile to build the view_dc (view DIR_CONFIG) dialog
#

include ../../../Include.mk

INSTDIR = ../../../../bin
COMDIR  = ../../../common
XCOMDIR = ../common
LIB     = $(XCOMDIR)/libxafd.a $(COMDIR)/libafd.a
CFLAGS  = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../afd_ctrl -I../../../afd_mon -I../../../init_afd

VDCPROG = view_dc
VDCOBJS = callbacks.o\
          get_dc_data.o\
          view_dc.o

PROGS   = $(VDCPROG)
OBJS    = $(VDCOBJS)
HEADERS = ../../../init_afd/afddefs.h ../../../config.h $(XCOMDIR)/x_common_defs.h view_dc.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(VDCPROG) : $(VDCOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(VDCOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	make clean
	rm -f $(PROGS)

install :
	cp -p $(PROGS) $(INSTDIR)
	chmod $(BINMODE) $(INSTDIR)/$(VDCPROG)

# Other dependencies
view_dc.o : ../../../version.h

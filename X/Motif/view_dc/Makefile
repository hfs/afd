#
# Makefile to build the view_dc (view DIR_CONFIG) dialog
#

include ../../../Include.mk

INSTDIR = ../../../..
COMDIR  = ../../../common
XCOMDIR = ../common
LIB     = $(XCOMDIR)/libxafd.a $(COMDIR)/libafd.a
CFLAGS  = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../afd_ctrl -I../../../afd_mon -I../../../init_afd $(XINCLUDES)

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
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(VDCOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	chmod $(BINMODE) $(INSTDIR)/bin/$(VDCPROG)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other dependencies
view_dc.o : ../../../version.h

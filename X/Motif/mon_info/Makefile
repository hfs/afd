#
# Makefile to build the MON_INFO (mon_ctrl Information)
#

include ../../../Include.mk

INSTDIR  = ../../../../bin
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(XCOMDIR)/libxafd.a $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../afd_ctrl -I../../../afd_mon -I../../../init_afd

MONIPROG = mon_info
MONIOBJS = mon_info.o\
           info_callbacks.o\
           update_info.o

PROGS    = $(MONIPROG)
OBJS     = $(MONIOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../afd_mon/mondefs.h ../../../config.h $(XCOMDIR)/x_common_defs.h mon_info.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(MONIPROG) : $(MONIOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(MONIOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	make clean
	rm -f $(PROGS)

install :
	cp -p $(PROGS) $(INSTDIR)
	chmod $(BINMODE) $(INSTDIR)/$(MONIPROG)

# Other dependencies
update_info.o : ../afd_ctrl/afd_ctrl.h
afd_info.o : ../../../version.h

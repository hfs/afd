#
# Makefile to build the AFD_INFO (Aoutomatic File Distributor Information)
#

include ../../../Include.mk

INSTDIR  = ../../../../bin
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(XCOMDIR)/libxafd.a $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../afd_ctrl -I../../../init_afd $(XINCLUDES)

AFDIPROG = afd_info
AFDIOBJS = afd_info.o\
           info_callbacks.o\
           update_info.o

PROGS    = $(AFDIPROG)
OBJS     = $(AFDIOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../config.h $(XCOMDIR)/x_common_defs.h afd_info.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

afd_info.o : active_passive.h

$(AFDIPROG) : $(AFDIOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(AFDIOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR) && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)
	chmod $(BINMODE) $(INSTDIR)/$(AFDIPROG)

# Other dependencies
update_info.o : ../afd_ctrl/afd_ctrl.h
afd_info.o : ../../../version.h

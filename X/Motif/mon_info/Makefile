#
# Makefile to build the MON_INFO (mon_ctrl Information)
#

include ../../../Include.mk

INSTDIR  = ../../../..
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(XCOMDIR)/libxafd.a $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../afd_ctrl -I../../../afd_mon -I../../../init_afd $(XINCLUDES)

MONIPROG = mon_info
MONIOBJS = mon_info.o\
           info_callbacks.o\
           update_info.o

PROGS    = $(MONIPROG)$(EXE_EXT)
OBJS     = $(MONIOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../afd_mon/mondefs.h ../../../config.h $(XCOMDIR)/x_common_defs.h mon_info.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(MONIPROG)$(EXE_EXT) : $(MONIOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(MONIOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	chmod $(BINMODE) $(INSTDIR)/bin/$(MONIPROG)$(EXE_EXT)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other dependencies
update_info.o : ../afd_ctrl/afd_ctrl.h
mon_info.o : ../../../version.h

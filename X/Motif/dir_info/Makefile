#
# Makefile to build the DIR_INFO (dir_ctrl Information)
#

include ../../../Include.mk

INSTDIR  = ../../../..
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(XCOMDIR)/libxafd.a $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../afd_ctrl -I../../../init_afd $(XINCLUDES)

DIRIPROG = dir_info
DIRIOBJS = dir_info.o\
           info_callbacks.o\
           update_info.o

PROGS    = $(DIRIPROG)$(EXE_EXT)
OBJS     = $(DIRIOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../config.h $(XCOMDIR)/x_common_defs.h dir_info.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(DIRIPROG)$(EXE_EXT) : $(DIRIOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(DIRIOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	chmod $(BINMODE) $(INSTDIR)/bin/$(DIRIPROG)$(EXE_EXT)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other dependencies
update_info.o : ../afd_ctrl/afd_ctrl.h
dir_info.o : ../../../version.h ../../../init_afd/permission.h

#
# Makefile to build afd_load dialog for the AFD
#

include ../../../Include.mk

INSTDIR  = ../../../..
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../../../init_afd -I../../../statistics -I../afd_ctrl $(XINCLUDES)

ALOGPROG = afd_load
ALOGOBJS = afd_load.o\
           afd_load_callbacks.o

PROGS    = $(ALOGPROG)
OBJS     = $(ALOGOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../statistics/statdefs.h ../../../config.h ../afd_ctrl/afd_ctrl.h $(XCOMDIR)/x_common_defs.h ./afd_load.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(ALOGPROG) : $(ALOGOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(ALOGOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XAWLIBS) $(XLIBS) $(SPECIAL_LIBS)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	chmod $(BINMODE) $(INSTDIR)/bin/$(ALOGPROG)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other dependencies
afd_load.o : ../../../version.h

#
# Makefile to build show_dlog for the AFD
#

include ../../../Include.mk

INSTDIR  = ../../../..
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(XCOMDIR)/libxafd.a $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../../../init_afd -I../../../log -I../../../fd -I../afd_ctrl $(XINCLUDES)

SLOGPROG = show_dlog
SLOGOBJS = show_dlog.o\
           calculate_summary.o\
           callbacks.o\
           format_info.o\
           get_data.o\
           get_info.o\
           print_data.o

PROGS    = $(SLOGPROG)$(EXE_EXT)
OBJS     = $(SLOGOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../config.h $(XCOMDIR)/x_common_defs.h show_dlog.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(SLOGPROG)$(EXE_EXT) : $(SLOGOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SLOGOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	chmod $(BINMODE) $(INSTDIR)/bin/$(SLOGPROG)$(EXE_EXT)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other dependencies
show_dlog.o : ../../../version.h ../../../init_afd/permission.h ../../../log/logdefs.h

#
# Makefile to build show_ilog for the AFD
#

include ../../../Include.mk

INSTDIR  = ../../../../bin
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(XCOMDIR)/libxafd.a $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../../../init_afd -I../../../log -I../../../fd -I../afd_ctrl $(XINCLUDES)

SLOGPROG = show_ilog
SLOGOBJS = calculate_summary.o\
           callbacks.o\
           format_info.o\
           get_data.o\
           get_info.o\
           print_data.o\
           show_ilog.o

PROGS    = $(SLOGPROG)
OBJS     = $(SLOGOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../config.h $(XCOMDIR)/x_common_defs.h show_ilog.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(SLOGPROG) : $(SLOGOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SLOGOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR) && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)
	chmod $(BINMODE) $(INSTDIR)/$(SLOGPROG)

# Other dependencies
show_ilog.o : ../../../version.h ../../../init_afd/permission.h

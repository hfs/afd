#
# Makefile to build show_log for the AFD
#

include ../../../Include.mk

INSTDIR  = ../../../../bin
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(COMDIR)/libafd.a $(XCOMDIR)/libxafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../../../init_afd -I../../../log -I../afd_ctrl -I../../../afd_mon $(XINCLUDES)

SLOGPROG = show_log
SLOGOBJS = show_log.o\
           check_log.o\
           init_text.o\
           log_callbacks.o\
           log_filter.o

PROGS    = $(SLOGPROG)
OBJS     = $(SLOGOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../config.h ../afd_ctrl/afd_ctrl.h ../../../log/logdefs.h $(XCOMDIR)/x_common_defs.h show_log.h

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
show_log.o : ../../../version.h ../../../afd_mon/mondefs.h

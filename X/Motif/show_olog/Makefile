#
# Makefile to build show_olog for the AFD
#

include ../../../Include.mk

INSTDIR  = ../../../../bin
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(XCOMDIR)/libxafd.a $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../../../init_afd -I../../../log -I../../../fd -I../afd_ctrl

SLOGPROG = show_olog
SLOGOBJS = calculate_summary.o\
           callbacks.o\
           format_info.o\
           get_data.o\
           get_info.o\
           get_send_data.o\
           print_data.o\
           resend_files.o\
           send_files.o\
           show_olog.o

PROGS    = $(SLOGPROG)
OBJS     = $(SLOGOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../config.h $(XCOMDIR)/x_common_defs.h show_olog.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(SLOGPROG) : $(SLOGOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(SLOGOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	make clean
	rm -f $(PROGS)

install :
	cp -p $(PROGS) $(INSTDIR)
	chmod $(BINMODE) $(INSTDIR)/$(SLOGPROG)

# Other dependencies
show_olog.o : ../../../version.h ../../../init_afd/permission.h

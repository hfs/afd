#
# Makefile to build show_olog for the AFD
#

include ../../../Include.mk

INSTDIR  = ../../../..
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(XCOMDIR)/libxafd.a $(COMDIR)/libafd.a
PROTOLIB = ../../../protocols/libprotocols.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../../../init_afd -I../../../log -I../../../fd -I../../../protocols -I../afd_ctrl $(XINCLUDES)

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
           send_files_ftp.o\
           send_files_smtp.o\
           show_olog.o\
           trans_log.o

PROGS    = $(SLOGPROG)
OBJS     = $(SLOGOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../config.h $(XCOMDIR)/x_common_defs.h show_olog.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(SLOGPROG) : $(SLOGOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SLOGOBJS) $(LIB) $(PROTOLIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	chmod $(SUIDMODE) $(INSTDIR)/bin/$(SLOGPROG)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other dependencies
show_olog.o : ../../../version.h ../../../init_afd/permission.h ../../../log/logdefs.h
get_send_data.o : ../../../protocols/ftpdefs.h ../../../protocols/smtpdefs.h
send_files_ftp.o : ../../../protocols/ftpdefs.h ../../../fd/fddefs.h

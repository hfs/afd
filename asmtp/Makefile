#
# Makefile to build ASMTP (Automatic SMTP)
#

include ../Include.mk

INSTDIR    = ../../bin
LIB        = ../common/libafd.a
PROTOLIB   = ../protocols/libprotocols.a
CFLAGS     = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd -I../protocols -I../fd

ASMTPPROG  = asmtp
ASMTPOBJS  = asmtp.o\
             eval_filename_file.o\
             init_asmtp.o\
             trans_log.o

PROGS      = $(ASMTPPROG)
OBJS       = $(ASMTPOBJS)
HEADERS    = ../init_afd/afddefs.h ../config.h asmtpdefs.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(ASMTPPROG) : $(ASMTPOBJS) $(LIB) $(PROTOLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(ASMTPOBJS) $(PROTOLIB) $(LIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR) && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)
	cd $(INSTDIR) && chmod $(BINMODE) $(PROGS)

# Other dependencies
asmtp.o : ../version.h
asmtp.o : ../protocols/smtpdefs.h

#
# Makefile to build AFTP (Automatic FTP)
#

include ../Include.mk

INSTDIR    = ../../bin
LIB        = ../common/libafd.a
PROTOLIB   = ../protocols/libprotocols.a
CFLAGS     = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd -I../protocols -I../fd

AFTPPROG   = aftp
AFTPOBJS   = aftp.o\
             eval_files.o\
             init_aftp.o\
             trans_log.o

PROGS      = $(AFTPPROG)
OBJS       = $(AFTPOBJS)
HEADERS    = ../init_afd/afddefs.h ../config.h aftpdefs.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(AFTPPROG) : $(AFTPOBJS) $(LIB) $(PROTOLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(AFTPOBJS) $(PROTOLIB) $(LIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

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
aftp.o : ../version.h
aftp.o : ../protocols/ftpdefs.h

#
# Makefile to build AFTP (Automatic FTP)
#

include ../Include.mk

INSTDIR    = ../..
LIB        = ../common/libafd.a
PROTOLIB   = ../protocols/libprotocols.a
CFLAGS     = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd -I../protocols -I../fd

AFTPPROG   = aftp
AFTPOBJS   = aftp.o\
             eval_files.o\
             get_remote_file_names.o\
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
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	cd $(INSTDIR)/bin && chmod $(BINMODE) $(PROGS)
	@if [ ! -r $(INSTDIR)/bin/raftp ] ; \
	then \
	   cd $(INSTDIR)/bin ; \
	   ln -s $(AFTPPROG) raftp ; \
	fi

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other dependencies
aftp.o : ../version.h
aftp.o : ../protocols/ftpdefs.h

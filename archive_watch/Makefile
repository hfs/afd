#
# Makefile to build the archive_watch
#

include ../Include.mk

INSTDIR = ../..
COMDIR  = ../common
LIB     = $(COMDIR)/libafd.a
CFLAGS  = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd

AWPROG = archive_watch
AWOBJS = archive_watch.o\
         inspect_archive.o

PROGS   = $(AWPROG)$(EXE_EXT)
HEADERS = awdefs.h ../init_afd/afddefs.h ../config.h

all : $(PROGS)

$(AWOBJS) : $(HEADERS)

$(AWPROG)$(EXE_EXT) : $(AWOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(AWOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(AWOBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	chmod $(BINMODE) $(INSTDIR)/bin/$(AWPROG)$(EXE_EXT)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other dependencies
archive_watch.o : ../version.h

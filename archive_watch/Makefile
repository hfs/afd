#
# Makefile to build the archive_watch
#

include ../Include.mk

INSTDIR = ../../bin
COMDIR  = ../common
LIB     = $(COMDIR)/libafd.a
CFLAGS  = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd

AWPROG = archive_watch
AWOBJS = archive_watch.o\
         inspect_archive.o

PROGS   = $(AWPROG)
HEADERS = awdefs.h ../init_afd/afddefs.h ../config.h

all : $(PROGS)

$(AWOBJS) : $(HEADERS)

$(AWPROG) : $(AWOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(AWOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(AWOBJS) core

clobber :
	make clean
	rm -f $(PROGS)

install :
	cp -p $(PROGS) $(INSTDIR)
	chmod $(BINMODE) $(INSTDIR)/$(AWPROG)

# Other dependencies
archive_watch.o : ../version.h

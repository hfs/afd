#
# Makefile to build init_afd
#

include ../Include.mk

INSTALLDIR = ../../bin
COMDIR     = ../common
LIB        = $(COMDIR)/libafd.a
CFLAGS     = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../amg

IAFDPROG   = init_afd
IAFDOBJS   = init_afd.o\
             check_afd.o\
             init_fifos_afd.o

AFDPROG    = afd
AFDOBJS    = afd.o\
             check_afd.o\
             shutdown_afd.o

PROGS      = $(IAFDPROG) $(AFDPROG)
OBJS       = $(IAFDOBJS) $(AFDOBJS)
HEADERS    = afddefs.h ../config.h

all : $(PROGS)

$(OBJS): $(HEADERS)

$(IAFDPROG) : $(IAFDOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(IAFDOBJS) $(LIB) $(EFENCE) $(CCMALLOC)

$(AFDPROG) : $(AFDOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(AFDOBJS) $(LIB) $(EFENCE) $(CCMALLOC)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cp -p $(PROGS) $(INSTALLDIR)
	chmod $(BINMODE) $(INSTALLDIR)/$(IAFDPROG)
	chmod $(BINMODE) $(INSTALLDIR)/$(AFDPROG)

# Other dependencies
init_afd.o afd.o : ../version.h
afd.o : permission.h

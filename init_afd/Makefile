#
# Makefile to build init_afd
#

include ../Include.mk

INSTDIR    = ../..
COMDIR     = ../common
LIB        = $(COMDIR)/libafd.a
CFLAGS     = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../amg

IAFDPROG   = init_afd
IAFDOBJS   = init_afd.o\
             check_afd.o\
             count_files.o\
             init_fifos_afd.o

AFDPROG    = afd
AFDOBJS    = afd.o\
             check_afd.o\
             check_afd_heartbeat.o\
             shutdown_afd.o

PROGS      = $(IAFDPROG)$(EXE_EXT) $(AFDPROG)$(EXE_EXT)
OBJS       = $(IAFDOBJS) $(AFDOBJS)
HEADERS    = afddefs.h ../config.h

all : $(PROGS)

$(OBJS): $(HEADERS)

$(IAFDPROG)$(EXE_EXT) : $(IAFDOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(IAFDOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

$(AFDPROG)$(EXE_EXT) : $(AFDOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(AFDOBJS) $(LIB) $(EFENCE) $(CCMALLOC)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	chmod $(SUIDMODE) $(INSTDIR)/bin/$(IAFDPROG)$(EXE_EXT)
	chmod $(SUIDMODE) $(INSTDIR)/bin/$(AFDPROG)$(EXE_EXT)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other dependencies
init_afd.o afd.o : ../version.h
afd.o : permission.h

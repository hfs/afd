include ../Include.mk

INSTDIR = ../../bin
COMDIR  = ../common
LIB     = $(COMDIR)/libafd.a
CFLAGS  = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd

EPROG   = encode64
EOBJS   = encode64.o\
          encode_base64.o

ARGPROG = arg
ARGOBJS = arg.o

PROGS   = $(EPROG) $(ARGPROG)
OBJS    = $(EOBJS) $(ARGOBJS)

all : $(PROGS)

$(EPROG) : $(EOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(EOBJS) $(LIB)

$(ARGPROG) : $(ARGOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(ARGOBJS) $(LIB)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS) amg.debug

include ../Include.mk

INSTDIR  = ../../bin
COMDIR   = ../common
LIB      = $(COMDIR)/libafd.a
PROTOLIB = ../protocols/libprotocols.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd -I../protocols -I../fd

EPROG    = encode64
EOBJS    = encode64.o\
           encode_base64.o

ARGPROG  = arg
ARGOBJS  = arg.o

SCPPROG  = ascp1
SCPOBJS  = ascp1.o\
           trans_log.o

PROGS    = $(EPROG) $(ARGPROG)
OBJS     = $(EOBJS) $(ARGOBJS)

all : $(PROGS)

$(EPROG) : $(EOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(EOBJS) $(LIB)

$(ARGPROG) : $(ARGOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(ARGOBJS) $(LIB)

$(SCPPROG) : $(SCPOBJS) $(LIB) $(PROTOLIB)
	$(CC) $(CFLAGS) -o $@ $(SCPOBJS) $(PROTOLIB) $(LIB)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS) amg.debug

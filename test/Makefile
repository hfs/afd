include ../Include.mk

INSTDIR  = ../..
COMDIR   = ../common
LIB      = $(COMDIR)/libafd.a
PROTOLIB = ../protocols/libprotocols.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd -I../protocols -I../fd

EPROG    = encode64
EOBJS    = encode64.o\
           encode_base64.o

CTPROG   = copy_file
CTOBJS   = copy_file.o

CNPROG   = change_name
CNOBJS   = change_name.o

ARGPROG  = arg
ARGOBJS  = arg.o

SCPPROG  = ascp
SCPOBJS  = ascp.o\
           trans_log.o

TLOKPROG = testlock
TLOKOBJS = testlock.o

PROGS    = $(EPROG)$(EXE_EXT) $(ARGPROG)$(EXE_EXT) $(TLOKPROG)$(EXE_EXT) $(SCPPROG)$(EXT_EXT) $(CNPROG)$(EXT_EXT)
OBJS     = $(EOBJS) $(ARGOBJS) $(CTOBJS) $(TLOKOBJS) $(SCPOBJS) $(CNOBJS)

all : $(PROGS)

$(EPROG)$(EXE_EXT) : $(EOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(EOBJS) $(LIB)

$(ARGPROG)$(EXE_EXT) : $(ARGOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(ARGOBJS) $(LIB)

$(CTPROG)$(EXE_EXT) : $(CTOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(CTOBJS) $(LIB)

$(CNPROG)$(EXE_EXT) : $(CNOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(CNOBJS) $(LIB)

$(SCPPROG)$(EXE_EXT) : $(SCPOBJS) $(LIB) $(PROTOLIB)
	$(CC) $(CFLAGS) -o $@ $(SCPOBJS) $(PROTOLIB) $(LIB)

$(TLOKPROG)$(EXE_EXT) : $(TLOKOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(TLOKOBJS) $(LIB)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

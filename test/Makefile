include ../Include.mk

INSTDIR = ../../bin
COMDIR  = ../common
LIB     = $(COMDIR)/libafd.a
CFLAGS  = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd

EPROG   = encode64
EOBJS   = encode64.o\
          encode_base64.o

FPROG   = tfilter
FOBJS   = tfilter.o

PROGS   = $(EPROG) $(FPROG)
OBJS    = $(EOBJS) $(FOBJS)

all : $(PROGS)

$(EPROG) : $(EOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(EOBJS) $(LIB)

$(FPROG) : $(FOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(FOBJS) $(LIB)

clean :
	rm -f $(OBJS) core

clobber :
	make clean
	rm -f $(PROGS) amg.debug

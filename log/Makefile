#
# Makefile to build the log system for the AFD
#

include ../Include.mk

INSTDIR  = ../../bin
COMDIR   = ../common
LIB      = $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd

SLPROG   = system_log
SLOBJS   = system_log.o

TLPROG   = transfer_log
TLOBJS   = transfer_log.o\
           open_log_file.o

TDBLPROG= trans_db_log
TDBLOBJS= trans_db_log.o

MSLPROG  = mon_sys_log
MSLOBJS  = mon_sys_log.o

MLOGPROG = monitor_log
MLOGOBJS = monitor_log.o\
           open_log_file.o

LIPROG   = input_log
LIOBJS   = input_log.o\
           open_log_file.o
LOPROG   = output_log
LOOBJS   = output_log.o\
           open_log_file.o
LDPROG   = delete_log
LDOBJS   = delete_log.o\
           open_log_file.o

PROGS    = $(SLPROG) $(TLPROG) $(TDBLPROG) $(MSLPROG) $(MLOGPROG) $(LIPROG) $(LOPROG) $(LDPROG)
OBJS     = $(SLOBJS) $(TLOBJS) $(TDBLOBJS) $(MSLOBJS) $(MLOGOBJS) $(LIOBJS) $(LOOBJS) $(LDOBJS)
HEADERS  = ../init_afd/afddefs.h ../config.h ../version.h logdefs.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(SLPROG) : $(SLOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(SLOBJS) $(LIB) $(EFENCE)

$(TLPROG) : $(TLOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(TLOBJS) $(LIB) $(EFENCE)

$(TDBLPROG) : $(TDBLOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(TDBLOBJS) $(LIB) $(EFENCE)

$(MSLPROG) : $(MSLOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(MSLOBJS) $(LIB) $(EFENCE)

$(MLOGPROG) : $(MLOGOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(MLOGOBJS) $(LIB) $(EFENCE)

$(LIPROG) : $(LIOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(LIOBJS) $(LIB) $(EFENCE)

$(LOPROG) : $(LOOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(LOOBJS) $(LIB) $(EFENCE)

$(LDPROG) : $(LDOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(LDOBJS) $(LIB) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cp -p $(PROGS) $(INSTDIR)
	cd $(INSTDIR) ; chmod $(BINMODE) $(PROGS)

# Other dependencies

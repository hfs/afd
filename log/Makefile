#
# Makefile to build the log system for the AFD
#

include ../Include.mk

INSTDIR  = ../..
COMDIR   = ../common
LIB      = $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd

SLPROG   = system_log
SLOBJS   = fprint_dup_msg.o\
           logger.o\
           system_log.o

RLPROG   = receive_log
RLOBJS   = fprint_dup_msg.o\
           open_log_file.o\
           receive_log.o

TLPROG   = transfer_log
TLOBJS   = fprint_dup_msg.o\
           open_log_file.o\
           transfer_log.o

TDBLPROG = trans_db_log
TDBLOBJS = fprint_dup_msg.o\
           logger.o\
           trans_db_log.o

MSLPROG  = mon_sys_log
MSLOBJS  = fprint_dup_msg.o\
           logger.o\
           mon_sys_log.o

MLOGPROG = monitor_log
MLOGOBJS = fprint_dup_msg.o\
           monitor_log.o\
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

PROGS    = $(SLPROG)$(EXE_EXT) $(RLPROG)$(EXE_EXT) $(TLPROG)$(EXE_EXT) $(TDBLPROG)$(EXE_EXT) $(MSLPROG)$(EXE_EXT) $(MLOGPROG)$(EXE_EXT) $(LIPROG)$(EXE_EXT) $(LOPROG)$(EXE_EXT) $(LDPROG)$(EXE_EXT)
OBJS     = $(SLOBJS) $(RLOBJS) $(TLOBJS) $(TDBLOBJS) $(MSLOBJS) $(MLOGOBJS) $(LIOBJS) $(LOOBJS) $(LDOBJS)
HEADERS  = ../init_afd/afddefs.h ../config.h ../version.h logdefs.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(SLPROG)$(EXE_EXT) : $(SLOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SLOBJS) $(LIB) $(EFENCE)

$(RLPROG)$(EXE_EXT) : $(RLOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(RLOBJS) $(LIB) $(EFENCE)

$(TLPROG)$(EXE_EXT) : $(TLOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(TLOBJS) $(LIB) $(EFENCE)

$(TDBLPROG)$(EXE_EXT) : $(TDBLOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(TDBLOBJS) $(LIB) $(EFENCE)

$(MSLPROG)$(EXE_EXT) : $(MSLOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(MSLOBJS) $(LIB) $(EFENCE)

$(MLOGPROG)$(EXE_EXT) : $(MLOGOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(MLOGOBJS) $(LIB) $(EFENCE)

$(LIPROG)$(EXE_EXT) : $(LIOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(LIOBJS) $(LIB) $(EFENCE)

$(LOPROG)$(EXE_EXT) : $(LOOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(LOOBJS) $(LIB) $(EFENCE)

$(LDPROG)$(EXE_EXT) : $(LDOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(LDOBJS) $(LIB) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	cd $(INSTDIR)/bin && chmod $(BINMODE) $(PROGS)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other dependencies

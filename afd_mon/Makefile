#
# Makefile to build the AFD monitor
#

include ../Include.mk

INSTDIR  = ../../bin
COMDIR   = ../common
AFDLIB   = $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd -I../afdd

MAFDPROG = mafd
MAFDOBJS = check_mon.o\
           mafd.o\
           shutdown_mon.o

AMONPROG = afd_mon
AMONOBJS = afd_mon.o\
           check_mon.o\
           create_msa.o\
           eval_afd_mon_db.o\
           init_fifos_mon.o\
           start_all.o\
           start_process.o\
           stop_all.o

MSAVPROG = msa_view
MSAVOBJS = msa_view.o

VHSTPROG = view_hosts
VHSTOBJS = view_hosts.o

MONPROG  = mon
MONOBJS  = mon.o\
           tcpcmd.o

PROGS    = $(MAFDPROG) $(AMONPROG) $(MONPROG) $(MSAVPROG) $(VHSTPROG)
OBJS     = $(MAFDOBJS) $(AMONOBJS) $(MONOBJS) $(MSAVOBJS) $(VHSTOBJS)
HEADERS  = ../init_afd/afddefs.h ../config.h mondefs.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(MAFDPROG) : $(MAFDOBJS) $(AFDLIB)
	$(CC) $(CFLAGS) -o $@ $(MAFDOBJS) $(AFDLIB) $(EFENCE) 

$(AMONPROG) : $(AMONOBJS) $(AFDLIB)
	$(CC) $(CFLAGS) -o $@ $(AMONOBJS) $(AFDLIB) $(EFENCE) 

$(MONPROG) : $(MONOBJS) $(AFDLIB)
	$(CC) $(CFLAGS) -o $@ $(MONOBJS) $(AFDLIB) $(SPECIAL_LIBS) $(EFENCE) 

$(MSAVPROG) : $(MSAVOBJS) $(AFDLIB)
	$(CC) $(CFLAGS) -o $@ $(MSAVOBJS) $(AFDLIB) $(EFENCE)

$(VHSTPROG) : $(VHSTOBJS) $(AFDLIB)
	$(CC) $(CFLAGS) -o $@ $(VHSTOBJS) $(AFDLIB) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	make clean
	rm -f $(PROGS)

install :
	cp -p $(MAFDPROG) $(AMONPROG) $(MONPROG) $(INSTDIR)
	cp -p $(MSAVPROG) ../../sbin
	cp -p $(VHSTPROG) ../../sbin
	cd $(INSTDIR) ; chmod $(BINMODE) $(MAFDPROG) $(AMONPROG) $(MONPROG)
	cd ../../sbin ; chmod $(SBINMODE) $(MSAVPROG) $(VHSTPROG)

# Other depnedencies
afd_mon.o mafd.o : ../version.h ../init_afd/permission.h

#
# Makefile to build the AFD monitor
#

include ../Include.mk

INSTDIR  = ../..
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
           stop_process.o

MSAVPROG = msa_view
MSAVOBJS = msa_view.o

TOPVPROG = topview
TOPVOBJS = topview.o

VHSTPROG = view_hosts
VHSTOBJS = view_hosts.o

MONPROG  = mon
MONOBJS  = mon.o\
           tcpcmd.o

PROGS    = $(MAFDPROG) $(AMONPROG) $(MONPROG) $(MSAVPROG) $(TOPVPROG) $(VHSTPROG)
OBJS     = $(MAFDOBJS) $(AMONOBJS) $(MONOBJS) $(MSAVOBJS) $(TOPVOBJS) $(VHSTOBJS)
HEADERS  = ../init_afd/afddefs.h ../config.h mondefs.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(MAFDPROG) : $(MAFDOBJS) $(AFDLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(MAFDOBJS) $(AFDLIB) $(EFENCE) 

$(AMONPROG) : $(AMONOBJS) $(AFDLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(AMONOBJS) $(AFDLIB) $(EFENCE) 

$(MONPROG) : $(MONOBJS) $(AFDLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(MONOBJS) $(AFDLIB) $(SPECIAL_LIBS) $(EFENCE) 

$(MSAVPROG) : $(MSAVOBJS) $(AFDLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(MSAVOBJS) $(AFDLIB) $(EFENCE)

$(TOPVPROG) : $(TOPVOBJS) $(AFDLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(TOPVOBJS) $(AFDLIB) $(EFENCE)

$(VHSTPROG) : $(VHSTOBJS) $(AFDLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(VHSTOBJS) $(AFDLIB) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR)/bin && rm -f $(MAFDPROG) $(AMONPROG) $(MONPROG)
	cp -p $(MAFDPROG) $(AMONPROG) $(MONPROG) $(INSTDIR)/bin
	cd $(INSTDIR)/bin && chmod $(BINMODE) $(AMONPROG) $(MONPROG)
	cd $(INSTDIR)/bin && chmod $(SUIDMODE) $(MAFDPROG)
	cd $(INSTDIR)/sbin && rm -f $(MSAVPROG) $(TOPVPROG) $(VHSTPROG)
	cp -p $(MSAVPROG) $(TOPVPROG) $(VHSTPROG) $(INSTDIR)/sbin
	cd $(INSTDIR)/sbin && chmod $(SBINMODE) $(MSAVPROG) $(TOPVPROG) $(VHSTPROG)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other depnedencies
afd_mon.o mafd.o : ../version.h ../init_afd/permission.h

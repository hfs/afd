#
# Makefile to build the maintenance tools for the AFD
#

include ../Include.mk

INSTDIR   = ../../sbin
COMDIR    = ../common
LIB       = $(COMDIR)/libafd.a
MISCLIB   = ../misc/libmisc.a
CFLAGS    = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd -I../amg -I../fd -I../statistics

ACMDPROG  = afdcmd
ACMDOBJS  = afdcmd.o

AFDSPROG  = afd_status
AFDSOBJS  = afd_status.o

FSAVPROG  = fsa_view
FSAVOBJS  = fsa_view.o

FSAEPROG  = fsa_edit
FSAEOBJS  = fsa_edit.o

RFSAPROG  = reset_fsa
RFSAOBJS  = reset_fsa.o

WDIRPROG  = watch_dir
WDIROBJS  = watch_dir.o

SYSCPROG  = sysconf
SYSCOBJS  = sysconf.o

BFCPROG   = bfc
BFCOBJS   = bull_file_check.o

BIFCPROG  = binfc
BIFCOBJS  = binfc.o

GCPROG    = get_counter
GCOBJS    = get_counter.o

SCPROG    = set_counter
SCOBJS    = set_counter.o

TSPROG    = tiff-sniffer
TSOBJS    = tiff-sniffer.o

DATEPROG  = get_full_date
DATEOBJS  = get_full_date.o

G2TPROG   = gts2tiff
G2TOBJS   = gts2tiff.o

GFSPROG   = get_file_size
GFSOBJS   = get_file_size.o

CFSAPROG  = convert_fsa
CFSAOBJS  = convert_fsa.o

CSTATPROG = convert_stat
CSTATOBJS = convert_stat.o

QSPYPROG  = queue_spy
QSPYOBJS  = queue_spy.o

CSPYPROG  = cache_spy
CSPYOBJS  = cache_spy.o

DSPYPROG  = dir_spy
DSPYOBJS  = dir_spy.o

JSPYPROG  = job_list_spy
JSPYOBJS  = job_list_spy.o

CJSPYPROG = current_job_list_spy
CJSPYOBJS = current_job_list_spy.o

FCHEKPROG = file_dir_check
FCHEKOBJS = file_dir_check.o

RMJOBPROG = rm_job
RMJOBOBJS = rm_job.o

UDBPROG   = udc
UDBOBJS   = update_db.o

GDDPROG   = get_dc_data
GDDOBJS   = get_dc_data.o

PROGS   = $(FSAVPROG) $(FSAEPROG) $(RFSAPROG) $(WDIRPROG) $(DSPYPROG)\
          $(AFDSPROG) $(BFCPROG) $(GCPROG) $(SCPROG) $(TSPROG) $(G2TPROG)\
          $(GFSPROG) $(CFSAPROG) $(QSPYPROG) $(CSPYPROG) $(JSPYPROG)\
          $(CJSPYPROG) $(RMJOBPROG) $(DATEPROG) $(FCHEKPROG)\
          $(CSTATPROG) $(UDBPROG) $(ACMDPROG) $(GDDPROG) # $(SYSCPROG)
OBJS    = $(FSAVOBJS) $(FSAEOBJS) $(RFSAOBJS) $(WDIROBJS) $(DSPYOBJS)\
          $(AFDSOBJS) $(BFCOBJS) $(SCOBJS) $(GCOBJS) $(TSOBJS) $(G2TOBJS)\
          $(GFSOBJS) $(CFSAOBJS) $(QSPYOBJS) $(CSPYOBJS) $(JSPYOBJS)\
          $(CJSPYOBJS) $(RMJOBOBJS) $(DATEOBJS) $(FCHEKOBJS)\
          $(CSTATOBJS) $(UDBOBJS) $(AOBJS) $(GDDOBJS) # $(SYSCOBJS)
HEADERS = ../init_afd/afddefs.h ../config.h ../fd/fddefs.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(ACMDPROG) : $(ACMDOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(ACMDOBJS) $(LIB) $(EFENCE)

$(AFDSPROG) : $(AFDSOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(AFDSOBJS) $(LIB) $(EFENCE)

$(FSAVPROG) : $(FSAVOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(FSAVOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

$(FSAEPROG) : $(FSAEOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(FSAEOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

$(RFSAPROG) : $(RFSAOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(RFSAOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

$(WDIRPROG) : $(WDIROBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(WDIROBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

$(BFCPROG) : $(BFCOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(BFCOBJS) $(LIB) $(EFENCE)

$(BIFCPROG) : $(BIFCOBJS) $(MISCLIB) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(BIFCOBJS) $(MISCLIB) $(LIB) $(EFENCE)

$(GCPROG) : $(GCOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(GCOBJS) $(LIB) $(EFENCE)

$(SCPROG) : $(SCOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(SCOBJS) $(LIB) $(EFENCE)

$(TSPROG) : $(TSOBJS)
	$(CC) $(CFLAGS) -o $@ $(TSOBJS) $(EFENCE)

$(G2TPROG) : $(G2TOBJS) $(MLIB) $(MISCLIB) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(G2TOBJS) $(MISCLIB) $(LIB) $(EFENCE)

$(GFSPROG) : $(GFSOBJS)
	$(CC) $(CFLAGS) -o $@ $(GFSOBJS) $(EFENCE)

$(CFSAPROG) : $(CFSAOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(CFSAOBJS) $(LIB) $(EFENCE)

$(CSTATPROG) : $(CSTATOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(CSTATOBJS) $(LIB) $(EFENCE)

$(QSPYPROG) : $(QSPYOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(QSPYOBJS) $(LIB) $(EFENCE)

$(CSPYPROG) : $(CSPYOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(CSPYOBJS) $(LIB) $(EFENCE)

$(DSPYPROG) : $(DSPYOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(DSPYOBJS) $(LIB) $(EFENCE)

$(JSPYPROG) : $(JSPYOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(JSPYOBJS) $(LIB) $(EFENCE)

$(CJSPYPROG) : $(CJSPYOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(CJSPYOBJS) $(LIB) $(EFENCE)

$(RMJOBPROG) : $(RMJOBOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(RMJOBOBJS) $(LIB) $(EFENCE)

$(DATEPROG) : $(DATEOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(DATEOBJS) $(LIB) $(EFENCE)

$(FCHEKPROG) : $(FCHEKOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(FCHEKOBJS) $(LIB) $(EFENCE)

$(UDBPROG) : $(UDBOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(UDBOBJS) $(LIB) $(EFENCE)

$(GDDPROG) : $(GDDOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(GDDOBJS) $(LIB) $(EFENCE)

#$(SYSCPROG) : $(SYSCOBJS) $(LIB)
#	$(CC) $(CFLAGS) -o $@ $(SYSCOBJS) $(LIB) $(EFENCE)

clean :
	rm -f $(OBJS) *.o core

clobber :
	make clean
	rm -f $(PROGS)

real_clobber :
	make clobber
	cd $(INSTDIR) ; rm -f $(PROGS) uhc

install :
	cp -p $(PROGS) $(INSTDIR)
	cd $(INSTDIR) ; chmod $(SBINMODE) $(PROGS) ; chmod $(BINMODE) $(UDBPROG) $(ACMDPROG)
	@if [ ! -r $(INSTDIR)/uhc ] ; \
	then \
           cd $(INSTDIR) ; \
           ln -s $(UDBPROG) uhc ; \
        fi

# Other Dependencies
show_stat.o : ../statistics/statdefs.h
afdcmd.o afd_status.o fsa_edit.o fsa_view.o reset_fsa.o set_counter.o tiff_sniffer.o : ../version.h

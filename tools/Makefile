#
# Makefile to build the maintenance tools for the AFD
#

include ../Include.mk

INSTDIR   = ../..
COMDIR    = ../common
LIB       = $(COMDIR)/libafd.a
MISCLIB   = ../misc/libmisc.a
CFLAGS    = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd -I../amg -I../fd -I../statistics

ACMDPROG  = afdcmd
ACMDOBJS  = afdcmd.o

ACFGPROG  = afdcfg
ACFGOBJS  = afdcfg.o

AFDSPROG  = afd_status
AFDSOBJS  = afd_status.o

FSAVPROG  = fsa_view
FSAVOBJS  = fsa_view.o

FRAVPROG  = fra_view
FRAVOBJS  = fra_view.o

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
G2TOBJS   = gts2tiff.o\
            receive_log.o

GFSPROG   = get_file_size
GFSOBJS   = get_file_size.o

CFSAPROG  = convert_fsa
CFSAOBJS  = convert_fsa.o

CJIDPROG  = convert_jid
CJIDOBJS  = convert_jid.o

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

ACHEKPROG = force_archive_check
ACHEKOBJS = force_archive_check.o

RMJOBPROG = rm_job
RMJOBOBJS = rm_job.o

UDBPROG   = udc
UDBOBJS   = update_db.o

GDDPROG   = get_dc_data
GDDOBJS   = get_dc_data.o

DELCPROG  = del_cache
DELCOBJS  = del_cache.o

PROGS   = $(FSAVPROG) $(FRAVPROG) $(FSAEPROG) $(RFSAPROG) $(WDIRPROG)\
          $(DSPYPROG) $(AFDSPROG) $(BFCPROG) $(GCPROG) $(SCPROG) $(TSPROG)\
          $(G2TPROG) $(GFSPROG) $(CFSAPROG) $(CJIDPROG) $(QSPYPROG) $(CSPYPROG)\
          $(JSPYPROG) $(CJSPYPROG) $(RMJOBPROG) $(DATEPROG) $(FCHEKPROG)\
          $(ACHEKPROG) $(CSTATPROG) $(UDBPROG) $(ACMDPROG) $(ACFGPROG)\
          $(GDDPROG) # $(SYSCPROG)
OBJS    = $(FSAVOBJS) $(FRAVOBJS) $(FSAEOBJS) $(RFSAOBJS) $(WDIROBJS)\
          $(DSPYOBJS) $(AFDSOBJS) $(BFCOBJS) $(SCOBJS) $(GCOBJS) $(TSOBJS)\
          $(G2TOBJS) $(GFSOBJS) $(CFSAOBJS) $(CJIDOBJS) $(QSPYOBJS) $(CSPYOBJS)\
          $(JSPYOBJS) $(CJSPYOBJS) $(RMJOBOBJS) $(DATEOBJS) $(FCHEKOBJS)\
          $(ACHEKOBJS) $(CSTATOBJS) $(UDBOBJS) $(ACMDOBJS) $(ACFGOBJS)\
          $(GDDOBJS) $(DELCOBJS) # $(SYSCOBJS)
HEADERS = ../init_afd/afddefs.h ../config.h ../fd/fddefs.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(ACMDPROG) : $(ACMDOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(ACMDOBJS) $(LIB) $(EFENCE)

$(ACFGPROG) : $(ACFGOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(ACFGOBJS) $(LIB) $(EFENCE)

$(AFDSPROG) : $(AFDSOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(AFDSOBJS) $(LIB) $(EFENCE)

$(FSAVPROG) : $(FSAVOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(FSAVOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

$(FRAVPROG) : $(FRAVOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(FRAVOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

$(FSAEPROG) : $(FSAEOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(FSAEOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

$(RFSAPROG) : $(RFSAOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(RFSAOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

$(WDIRPROG) : $(WDIROBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(WDIROBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

$(BFCPROG) : $(BFCOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(BFCOBJS) $(LIB) $(EFENCE)

$(BIFCPROG) : $(BIFCOBJS) $(MISCLIB) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(BIFCOBJS) $(MISCLIB) $(LIB) $(EFENCE)

$(GCPROG) : $(GCOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(GCOBJS) $(LIB) $(EFENCE)

$(SCPROG) : $(SCOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SCOBJS) $(LIB) $(EFENCE)

$(TSPROG) : $(TSOBJS)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(TSOBJS) $(EFENCE)

$(G2TPROG) : $(G2TOBJS) $(MLIB) $(MISCLIB) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(G2TOBJS) $(MISCLIB) $(LIB) $(EFENCE)

$(GFSPROG) : $(GFSOBJS)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(GFSOBJS) $(EFENCE)

$(CFSAPROG) : $(CFSAOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(CFSAOBJS) $(LIB) $(EFENCE)

$(CJIDPROG) : $(CJIDOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(CJIDOBJS) $(LIB) $(EFENCE)

$(CSTATPROG) : $(CSTATOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(CSTATOBJS) $(LIB) $(EFENCE)

$(QSPYPROG) : $(QSPYOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(QSPYOBJS) $(LIB) $(EFENCE)

$(CSPYPROG) : $(CSPYOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(CSPYOBJS) $(LIB) $(EFENCE)

$(DSPYPROG) : $(DSPYOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(DSPYOBJS) $(LIB) $(EFENCE)

$(JSPYPROG) : $(JSPYOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(JSPYOBJS) $(LIB) $(EFENCE)

$(CJSPYPROG) : $(CJSPYOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(CJSPYOBJS) $(LIB) $(EFENCE)

$(RMJOBPROG) : $(RMJOBOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(RMJOBOBJS) $(LIB) $(EFENCE)

$(DATEPROG) : $(DATEOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(DATEOBJS) $(LIB) $(EFENCE)

$(FCHEKPROG) : $(FCHEKOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(FCHEKOBJS) $(LIB) $(EFENCE)

$(ACHEKPROG) : $(ACHEKOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(ACHEKOBJS) $(LIB) $(EFENCE)

$(UDBPROG) : $(UDBOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(UDBOBJS) $(LIB) $(EFENCE)

$(GDDPROG) : $(GDDOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(GDDOBJS) $(LIB) $(EFENCE)

$(DELCPROG) : $(DELCOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(DELCOBJS) $(LIB) $(EFENCE)

#$(SYSCPROG) : $(SYSCOBJS) $(LIB)
#	rm -f $@
#	$(CC) $(CFLAGS) -o $@ $(SYSCOBJS) $(LIB) $(EFENCE)

clean :
	rm -f $(OBJS) *.o core

clobber :
	$(MAKE) clean
	rm -f $(PROGS) $(DELCPROG)

real_clobber :
	$(MAKE) clobber
	cd $(INSTDIR)/sbin && rm -f $(PROGS) uhc $(DELCPROG)

install :
	cd $(INSTDIR)/sbin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/sbin
	cd $(INSTDIR)/sbin && chmod $(SBINMODE) $(PROGS) && chmod $(SUIDMODE) $(UDBPROG) $(ACMDPROG)
	@if [ ! -r $(INSTDIR)/sbin/uhc ] ; \
	then \
           cd $(INSTDIR)/sbin ; \
           ln -s $(UDBPROG) uhc ; \
        fi

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other Dependencies
show_stat.o : ../statistics/statdefs.h
afdcmd.o afd_status.o fsa_edit.o fsa_view.o fra_view.o reset_fsa.o set_counter.o tiff_sniffer.o : ../version.h

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

CFRAPPROG = check_fra_dir_pos
CFRAPOBJS = check_fra_dir_pos.o

GENFPROG  = gen_file
GENFOBJS  = gen_file.o

PROGS   = $(FSAVPROG)$(EXE_EXT) $(FRAVPROG)$(EXE_EXT) $(FSAEPROG)$(EXE_EXT) $(RFSAPROG)$(EXE_EXT) $(WDIRPROG)$(EXE_EXT)\
          $(DSPYPROG)$(EXE_EXT) $(AFDSPROG)$(EXE_EXT) $(BFCPROG)$(EXE_EXT) $(GCPROG)$(EXE_EXT) $(SCPROG)$(EXE_EXT) $(TSPROG)$(EXE_EXT)\
          $(G2TPROG)$(EXE_EXT) $(GFSPROG)$(EXE_EXT) $(CFSAPROG)$(EXE_EXT) $(CJIDPROG)$(EXE_EXT) $(QSPYPROG)$(EXE_EXT) $(CSPYPROG)$(EXE_EXT)\
          $(JSPYPROG)$(EXE_EXT) $(CJSPYPROG)$(EXE_EXT) $(RMJOBPROG)$(EXE_EXT) $(DATEPROG)$(EXE_EXT) $(FCHEKPROG)$(EXE_EXT)\
          $(ACHEKPROG)$(EXE_EXT) $(CSTATPROG)$(EXE_EXT) $(UDBPROG)$(EXE_EXT) $(ACMDPROG)$(EXE_EXT) $(ACFGPROG)$(EXE_EXT)\
          $(GDDPROG)$(EXE_EXT) $(CFRAPPROG)$(EXE_EXT)\
          $(GENFPROG)$(EXT_EXT) # $(SYSCPROG)$(EXE_EXT)
OBJS    = $(FSAVOBJS) $(FRAVOBJS) $(FSAEOBJS) $(RFSAOBJS) $(WDIROBJS)\
          $(DSPYOBJS) $(AFDSOBJS) $(BFCOBJS) $(SCOBJS) $(GCOBJS) $(TSOBJS)\
          $(G2TOBJS) $(GFSOBJS) $(CFSAOBJS) $(CJIDOBJS) $(QSPYOBJS) $(CSPYOBJS)\
          $(JSPYOBJS) $(CJSPYOBJS) $(RMJOBOBJS) $(DATEOBJS) $(FCHEKOBJS)\
          $(ACHEKOBJS) $(CSTATOBJS) $(UDBOBJS) $(ACMDOBJS) $(ACFGOBJS)\
          $(GDDOBJS) $(DELCOBJS) $(CFRAPOBJS) $(GENFOBJS) # $(SYSCOBJS)
HEADERS = ../init_afd/afddefs.h ../config.h ../fd/fddefs.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(ACMDPROG)$(EXE_EXT) : $(ACMDOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(ACMDOBJS) $(LIB) $(EFENCE)

$(ACFGPROG)$(EXE_EXT) : $(ACFGOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(ACFGOBJS) $(LIB) $(EFENCE)

$(AFDSPROG)$(EXE_EXT) : $(AFDSOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(AFDSOBJS) $(LIB) $(EFENCE)

$(FSAVPROG)$(EXE_EXT) : $(FSAVOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(FSAVOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

$(FRAVPROG)$(EXE_EXT) : $(FRAVOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(FRAVOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

$(FSAEPROG)$(EXE_EXT) : $(FSAEOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(FSAEOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

$(RFSAPROG)$(EXE_EXT) : $(RFSAOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(RFSAOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

$(WDIRPROG)$(EXE_EXT) : $(WDIROBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(WDIROBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE)

$(BFCPROG)$(EXE_EXT) : $(BFCOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(BFCOBJS) $(LIB) $(EFENCE)

$(BIFCPROG)$(EXE_EXT) : $(BIFCOBJS) $(MISCLIB) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(BIFCOBJS) $(MISCLIB) $(LIB) $(EFENCE)

$(GCPROG)$(EXE_EXT) : $(GCOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(GCOBJS) $(LIB) $(EFENCE)

$(SCPROG)$(EXE_EXT) : $(SCOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SCOBJS) $(LIB) $(EFENCE)

$(TSPROG)$(EXE_EXT) : $(TSOBJS)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(TSOBJS) $(EFENCE)

$(G2TPROG)$(EXE_EXT) : $(G2TOBJS) $(MLIB) $(MISCLIB) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(G2TOBJS) $(MISCLIB) $(LIB) $(EFENCE)

$(GFSPROG)$(EXE_EXT) : $(GFSOBJS)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(GFSOBJS) $(EFENCE)

$(CFSAPROG)$(EXE_EXT) : $(CFSAOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(CFSAOBJS) $(LIB) $(EFENCE)

$(CJIDPROG)$(EXE_EXT) : $(CJIDOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(CJIDOBJS) $(LIB) $(EFENCE)

$(CSTATPROG)$(EXE_EXT) : $(CSTATOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(CSTATOBJS) $(LIB) $(EFENCE)

$(QSPYPROG)$(EXE_EXT) : $(QSPYOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(QSPYOBJS) $(LIB) $(EFENCE)

$(CSPYPROG)$(EXE_EXT) : $(CSPYOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(CSPYOBJS) $(LIB) $(EFENCE)

$(DSPYPROG)$(EXE_EXT) : $(DSPYOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(DSPYOBJS) $(LIB) $(EFENCE)

$(JSPYPROG)$(EXE_EXT) : $(JSPYOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(JSPYOBJS) $(LIB) $(EFENCE)

$(CJSPYPROG)$(EXE_EXT) : $(CJSPYOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(CJSPYOBJS) $(LIB) $(EFENCE)

$(RMJOBPROG)$(EXE_EXT) : $(RMJOBOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(RMJOBOBJS) $(LIB) $(EFENCE)

$(DATEPROG)$(EXE_EXT) : $(DATEOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(DATEOBJS) $(LIB) $(EFENCE)

$(FCHEKPROG)$(EXE_EXT) : $(FCHEKOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(FCHEKOBJS) $(LIB) $(EFENCE)

$(ACHEKPROG)$(EXE_EXT) : $(ACHEKOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(ACHEKOBJS) $(LIB) $(EFENCE)

$(UDBPROG)$(EXE_EXT) : $(UDBOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(UDBOBJS) $(LIB) $(EFENCE)

$(GDDPROG)$(EXE_EXT) : $(GDDOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(GDDOBJS) $(LIB) $(EFENCE)

$(DELCPROG)$(EXE_EXT) : $(DELCOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(DELCOBJS) $(LIB) $(EFENCE)

$(CFRAPPROG)$(EXE_EXT) : $(CFRAPOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(CFRAPOBJS) $(LIB) $(EFENCE)

$(GENFPROG)$(EXE_EXT) : $(GENFOBJS)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(GENFOBJS) $(EFENCE)

#$(SYSCPROG)$(EXE_EXT) : $(SYSCOBJS) $(LIB)
#	rm -f $@
#	$(CC) $(CFLAGS) -o $@ $(SYSCOBJS) $(LIB) $(EFENCE)

clean :
	rm -f $(OBJS) *.o core

clobber :
	$(MAKE) clean
	rm -f $(PROGS) $(DELCPROG)$(EXE_EXT) $(SYSCPROG)$(EXE_EXT)

real_clobber :
	$(MAKE) clobber
	cd $(INSTDIR)/sbin && rm -f $(PROGS) uhc$(EXE_EXT) $(DELCPROG)$(EXE_EXT) $(SYSCPROG)$(EXE_EXT)

install :
	cd $(INSTDIR)/sbin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/sbin
	cd $(INSTDIR)/sbin && chmod $(SBINMODE) $(PROGS)
	@if [ "`find $(INSTDIR)/sbin -user 0 -name $(UDBPROG)$(EXE_EXT) -print`" = "" ] ; \
	then \
		cd $(INSTDIR)/sbin && chmod $(SUIDMODE) $(UDBPROG)$(EXE_EXT) ; \
	fi
	@if [ "`find $(INSTDIR)/sbin -user 0 -name $(ACMDPROG)$(EXE_EXT) -print`" = "" ] ; \
	then \
		cd $(INSTDIR)/sbin && chmod $(SUIDMODE) $(ACMDPROG)$(EXE_EXT) ; \
	fi
	@if [ ! -r $(INSTDIR)/sbin/uhc$(EXE_EXT) ] ; \
	then \
           cd $(INSTDIR)/sbin ; \
           ln -s $(UDBPROG) uhc$(EXE_EXT) ; \
        fi

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other Dependencies
show_stat.o : ../statistics/statdefs.h
afdcmd.o afd_status.o fsa_edit.o fsa_view.o fra_view.o reset_fsa.o set_counter.o tiff_sniffer.o : ../version.h

#
# Makefile to build the FD (File Distributor)
#

include ../Include.mk

INSTDIR    = ../..
LIB        = ../common/libafd.a
PROTOLIB   = ../protocols/libprotocols.a
MISCLIB    = ../misc/libmisc.a
CFLAGS     = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd -I../amg -I../misc -I../protocols -I../log

FDPROG     = fd
FDOBJS     = check_file_dir.o\
             check_fra_fd.o\
             check_fsa_entries.o\
             check_msg_time.o\
             check_queue_space.o\
             fd.o\
             get_job_data.o\
             get_new_positions.o\
             init_fifos_fd.o\
             init_fra_data.o\
             init_msg_buffer.o\
             init_msg_ptrs.o\
             lookup_job_id.o\
             recreate_msg.o\
             remove_connection.o\
             remove_files.o\
             remove_msg.o

SFFTPPROG  = sf_ftp
SFFTPOBJS  = append.o\
             archive_file.o\
             check_burst_2.o\
             eval_input_sf.o\
             eval_recipient.o\
             eval_message.o\
             get_file_names.o\
             get_group_list.o\
             handle_proxy.o\
             init_sf.o\
             init_sf_burst2.o\
             output_log_ptrs.o\
             reset_fsa.o\
             sf_ftp.o\
             trans_db_log.o\
             trans_exec.o\
             trans_log.o

SFSMTPPROG = sf_smtp
SFSMTPOBJS = append.o\
             archive_file.o\
             eval_input_sf.o\
             eval_message.o\
             eval_recipient.o\
             get_file_names.o\
             get_group_list.o\
             init_sf.o\
             output_log_ptrs.o\
             reset_fsa.o\
             sf_smtp.o\
             trans_db_log.o\
             trans_exec.o\
             trans_log.o

SFSCP1PROG = sf_scp1
SFSCP1OBJS = append.o\
             archive_file.o\
             eval_input_sf.o\
             eval_recipient.o\
             eval_message.o\
             get_file_names.o\
             get_group_list.o\
             init_sf.o\
             output_log_ptrs.o\
             reset_fsa.o\
             sf_scp1.o\
             trans_db_log.o\
             trans_exec.o\
             trans_log.o

SFWMOPROG  = sf_wmo
SFWMOOBJS  = append.o\
             archive_file.o\
             eval_input_sf.o\
             eval_message.o\
             eval_recipient.o\
             get_file_names.o\
             get_group_list.o\
             init_sf.o\
             next_wmo_counter.o\
             output_log_ptrs.o\
             reset_fsa.o\
             sf_wmo.o\
             trans_db_log.o\
             trans_exec.o\
             trans_log.o

SFMAPPROG  = sf_map
SFMAPOBJS  = append.o\
             archive_file.o\
             eval_input_sf.o\
             eval_message.o\
             eval_recipient.o\
             get_file_names.o\
             get_group_list.o\
             init_sf.o\
             output_log_ptrs.o\
             reset_fsa.o\
             sf_map.o\
             trans_db_log.o\
             trans_log.o

SFLOCPROG  = sf_loc
SFLOCOBJS  = append.o\
             archive_file.o\
             check_burst_2.o\
             eval_input_sf.o\
             eval_message.o\
             eval_recipient.o\
             get_file_names.o\
             get_group_list.o\
             init_sf.o\
             init_sf_burst2.o\
             output_log_ptrs.o\
             reset_fsa.o\
             sf_loc.o\
             trans_db_log.o\
             trans_exec.o\
             trans_log.o

GFFTPPROG  = gf_ftp
GFFTPOBJS  = eval_input_gf.o\
             eval_recipient.o\
             get_group_list.o\
             get_remote_file_names.o\
             gf_ftp.o\
             handle_proxy.o\
             init_gf.o\
             read_file_mask.o\
             reset_fsa.o\
             trans_db_log.o\
             trans_log.o

PROGS      = $(FDPROG) $(SFFTPPROG) $(SFSMTPPROG) $(SFSCP1PROG) $(SFWMOPROG) $(SFMAPPROG) $(SFLOCPROG) $(GFFTPPROG)
OBJS       = $(FDOBJS) $(SFFTPOBJS) $(SFSMTPOBJS) $(SFSCP1OBJS) $(SFWMOOBJS) $(SFMAPOBJS) $(SFLOCOBJS) $(GFFTPOBJS)
HEADERS    = ../init_afd/afddefs.h ../config.h fddefs.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(FDPROG) : $(FDOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(FDOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

$(SFFTPPROG) : $(SFFTPOBJS) $(LIB) $(PROTOLIB) $(MISCLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SFFTPOBJS) $(PROTOLIB) $(LIB) $(MISCLIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

$(SFSMTPPROG) : $(SFSMTPOBJS) $(LIB) $(PROTOLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SFSMTPOBJS) $(PROTOLIB) $(LIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

$(SFSCP1PROG) : $(SFSCP1OBJS) $(LIB) $(PROTOLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SFSCP1OBJS) $(PROTOLIB) $(LIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

$(SFWMOPROG) : $(SFWMOOBJS) $(LIB) $(PROTOLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SFWMOOBJS) $(PROTOLIB) $(LIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

$(SFMAPPROG) : $(SFMAPOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SFMAPOBJS) $(LIB) $(SPECIAL_LIBS) $(MAP_LIB) $(EFENCE) $(CCMALLOC)

$(SFLOCPROG) : $(SFLOCOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SFLOCOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

$(GFFTPPROG) : $(GFFTPOBJS) $(LIB) $(PROTOLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(GFFTPOBJS) $(PROTOLIB) $(LIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

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
fd.o sf_ftp.o sf_loc.o sf_smtp.o sf_map.o sf_wmo.o gf_ftp.o : ../version.h
fd.o : ../log/logdefs.h
sf_ftp.o init_sf.o init_gf.o eval_message.o : ../protocols/ftpdefs.h
sf_ftp.o : ../misc/eumetsat_header_defs.h
sf_scp1.o init_sf.o eval_message.o : ../protocols/scp1defs.h
sf_smtp.o init_sf.o init_gf.o : ../protocols/smtpdefs.h
sf_wmo.o init_sf.o init_gf.o : ../protocols/wmodefs.h

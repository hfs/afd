#
# Makefile to build the FD (File Distributor)
#

include ../Include.mk

INSTDIR    = ../../bin
COMDIR     = ../common
LIB        = $(COMDIR)/libafd.a
MISCLIB    = ../misc/libmisc.a
CFLAGS     = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd -I../amg -I../misc

FDPROG     = fd
FDOBJS     = check_file_dir.o\
             check_fsa_entries.o\
             check_job_name.o\
             check_msg_time.o\
             fd.o\
             get_job_data.o\
             init_fifos_fd.o\
             init_msg_buffer.o\
             init_msg_ptrs.o\
             lookup_job_id.o\
             recreate_msg.o\
             remove_files.o\
             remove_msg.o

SFFTPPROG  = sf_ftp
SFFTPOBJS  = append.o\
             archive_file.o\
             eval_input_sf.o\
             eval_recipient.o\
             eval_message.o\
             ftpcmd.o\
             get_file_names.o\
             init_sf.o\
             output_log_ptrs.o\
             reset_fsa.o\
             sf_ftp.o\
             trans_log.o

AFTPPROG   = aftp
AFTPOBJS   = aftp.o\
             ftpcmd.o\
             init_aftp.o\
             trans_log_aftp.o

SFSMTPPROG = sf_smtp
SFSMTPOBJS = append.o\
             archive_file.o\
             encode_base64.o\
             eval_input_sf.o\
             eval_message.o\
             eval_recipient.o\
             get_file_names.o\
             init_sf.o\
             output_log_ptrs.o\
             reset_fsa.o\
             sf_smtp.o\
             smtpcmd.o

SFWMOPROG  = sf_wmo
SFWMOOBJS  = append.o\
             sf_wmo.o\
             init_sf.o\
             output_log_ptrs.o\
             eval_input_sf.o\
             eval_recipient.o\
             eval_message.o\
             archive_file.o\
             next_wmo_counter.o\
             reset_fsa.o\
             get_file_names.o\
             wmocmd.o

SFMAPPROG  = sf_map
SFMAPOBJS  = append.o\
             sf_map.o\
             init_sf.o\
             output_log_ptrs.o\
             eval_input_sf.o\
             eval_recipient.o\
             eval_message.o\
             archive_file.o\
             reset_fsa.o\
             get_file_names.o

SFLOCPROG  = sf_loc
SFLOCOBJS  = append.o\
             sf_loc.o\
             init_sf.o\
             output_log_ptrs.o\
             eval_input_sf.o\
             eval_recipient.o\
             eval_message.o\
             archive_file.o\
             reset_fsa.o\
             get_file_names.o

PROGS      = $(FDPROG) $(SFFTPPROG) $(SFSMTPPROG) $(SFWMOPROG) $(SFMAPPROG) $(SFLOCPROG) $(AFTPPROG)
OBJS       = $(FDOBJS) $(SFFTPOBJS) $(SFSMTPOBJS) $(SFWMOOBJS) $(SFMAPOBJS) $(SFLOCOBJS) $(AFTPOBJS)
HEADERS    = ../init_afd/afddefs.h ../config.h fddefs.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(FDPROG) : $(FDOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(FDOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

$(SFFTPPROG) : $(SFFTPOBJS) $(MISCLIB) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(SFFTPOBJS) $(MISCLIB) $(LIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

$(SFSMTPPROG) : $(SFSMTPOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(SFSMTPOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

$(SFWMOPROG) : $(SFWMOOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(SFWMOOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

$(SFMAPPROG) : $(SFMAPOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(SFMAPOBJS) $(LIB) $(SPECIAL_LIBS) $(MAP_LIB) $(EFENCE) $(CCMALLOC)

$(SFLOCPROG) : $(SFLOCOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(SFLOCOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

$(AFTPPROG) : $(AFTPOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(AFTPOBJS) $(LIB) $(SPECIAL_LIBS) $(EFENCE) $(CCMALLOC)

clean :
	rm -f $(OBJS) core

clobber :
	make clean
	rm -f $(PROGS)

install :
	cp -p $(PROGS) $(INSTDIR)
	cd $(INSTDIR); chmod $(BINMODE) $(PROGS)

# Other dependencies
aftp.o fd.o sf_ftp.o sf_loc.o sf_smtp.o sf_map.o sf_wmo.o : ../version.h
sf_ftp.o ftpcmd.o init_sf.o : ftpdefs.h
sf_ftp.o : ../misc/eumetsat_header_defs.h
sf_smtp.o smtpcmd.o init_sf.o : smtpdefs.h
sf_wmo.o wmocmd.o init_sf.o : wmodefs.h

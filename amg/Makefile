#
# Makefile to build the AMG (Automatic Message Generator)
#

include ../Include.mk

INSTDIR = ../../bin
COMDIR  = ../common
AFDLIB  = $(COMDIR)/libafd.a
MISCLIB = ../misc/libmisc.a
CFLAGS  = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd

AMGPROG = amg
AMGOBJS = amg.o\
          amg_zombie_check.o\
          clear_pool_dir.o\
          com.o\
          create_fsa.o\
          create_fra.o\
          create_sa.o\
          eval_dir_config.o\
          eval_dir_options.o\
          eval_input_amg.o\
          eval_time_str.o\
          get_hostname.o\
          lookup_dir_no.o\
          make_process_amg.o\
          remove_pool_directory.o\
          reread_dir_config.o\
          reread_host_config.o\
          show_shm.o\
          store_file_mask.o

DCHKPROG= dir_check
DCHKOBJS= check_files.o\
          check_paused_dir.o\
          check_old_time_jobs.o\
          clear_msg_buffer.o\
          count_pool_files.o\
          create_db.o\
          dir_check.o\
          enter_time_job.o\
          eval_time_str.o\
          get_hostname.o\
          handle_options.o\
          handle_time_jobs.o\
          init_dir_check.o\
          init_job_data.o\
          init_msg_buffer.o\
          in_time.o\
          link_files.o\
          lookup_fra_pos.o\
          lookup_job_id.o\
          map_instant_db.o\
          next.o\
          receive_log.o\
          remove_time_dir.o\
          rename_files.o\
          save_files.o\
          search_old_files.o\
          send_message.o\
          sort_time_job.o

TTPROG  = test_time
TTOBJS  = eval_time_str.o\
          test_time.o

TITPROG = test_in_time
TITOBJS = eval_time_str.o\
          in_time.o\
          test_in_time.o

PROGS   = $(AMGPROG) $(DCHKPROG)
OBJS    = $(AMGOBJS) $(DCHKOBJS)
HEADERS = ../init_afd/afddefs.h ../config.h amgdefs.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(AMGPROG) : $(AMGOBJS) $(AFDLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(AMGOBJS) $(AFDLIB) $(EFENCE) $(CCMALLOC)

$(DCHKPROG) : $(DCHKOBJS) $(MISCLIB) $(AFDLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(DCHKOBJS) $(MISCLIB) $(AFDLIB) $(SPECIAL_LIBS) $(THREAD_LIB) $(EFENCE) $(CCMALLOC)

$(TTPROG) : $(TTOBJS) $(AFDLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(TTOBJS) $(AFDLIB) $(SPECIAL_LIBS)

$(TITPROG) : $(TITOBJS) $(AFDLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(TITOBJS) $(AFDLIB) $(SPECIAL_LIBS)

clean :
	rm -f $(OBJS) $(TTOBJS) $(TITOBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS) $(TTPROG) $(TITOBJS) amg.debug

install :
	cd $(INSTDIR) && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)
	chmod $(BINMODE) $(INSTDIR)/$(AMGPROG) $(INSTDIR)/$(DCHKPROG)

# Other depnedencies
create_message.o : ../fd/fddefs.h
amg.o dir_check.o : ../version.h
eval_time_str.o : ../init_afd/bit_array.h

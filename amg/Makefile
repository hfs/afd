#
# Makefile to build the AMG (Automatic Message Generator)
#

include ../Include.mk

INSTDIR = ../../bin
COMDIR  = ../common
AFDLIB  = $(COMDIR)/libafd.a
MISCLIB = ../misc/libmisc.a
CFLAGS  = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd -I../fd

AMGPROG = amg
AMGOBJS = amg.o\
          amg_zombie_check.o\
          clear_pool_dir.o\
          com.o\
          create_fsa.o\
          eval_dir_config.o\
          eval_host_config.o\
          eval_input_amg.o\
          get_hostname.o\
          make_process_amg.o\
          remove_pool_directory.o\
          reread_dir_config.o\
          reread_host_config.o\
          show_shm.o\
          write_host_config.o

DCHKPROG= dir_check
DCHKOBJS= calc_next_time.o\
          check_files.o\
          check_paused_dir.o\
          check_process_list.o\
          check_old_time_jobs.o\
          clear_msg_buffer.o\
          create_db.o\
          dir_check.o\
          enter_time_job.o\
          eval_time_str.o\
          exec_cmd.o\
          get_hostname.o\
          handle_options.o\
          handle_time_jobs.o\
          init_dir_check.o\
          init_job_data.o\
          init_msg_buffer.o\
          link_files.o\
          lookup_dir_no.o\
          lookup_job_id.o\
          map_instant_db.o\
          next.o\
          remove_time_dir.o\
          rename_files.o\
          save_files.o\
          search_old_files.o\
          send_message.o\
          sort_time_job.o

TTPROG  = test_time
TTOBJS  = calc_next_time.o\
          eval_time_str.o\
          test_time.o

PROGS   = $(AMGPROG) $(DCHKPROG)
OBJS    = $(AMGOBJS) $(DCHKOBJS)
HEADERS = ../init_afd/afddefs.h ../config.h amgdefs.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(AMGPROG) : $(AMGOBJS) $(AFDLIB)
	$(CC) $(CFLAGS) -o $@ $(AMGOBJS) $(AFDLIB) $(EFENCE) $(CCMALLOC)

$(DCHKPROG) : $(DCHKOBJS) $(MISCLIB) $(AFDLIB)
	$(CC) $(CFLAGS) -o $@ $(DCHKOBJS) $(MISCLIB) $(AFDLIB) $(SPECIAL_LIBS) $(THREAD_LIB) $(EFENCE) $(CCMALLOC)

$(TTPROG) : $(TTOBJS) $(AFDLIB)
	$(CC) $(CFLAGS) -o $@ $(TTOBJS) $(AFDLIB) $(SPECIAL_LIBS)

clean :
	rm -f $(OBJS) $(TTOBJS) core

clobber :
	make clean
	rm -f $(PROGS) $(TTPROG) amg.debug

install :
	cp -p $(PROGS) $(INSTDIR)
	chmod $(BINMODE) $(INSTDIR)/$(AMGPROG) $(INSTDIR)/$(DCHKPROG)

# Other depnedencies
create_message.o : ../fd/fddefs.h
amg.o dir_check.o : ../version.h

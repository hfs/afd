#
# Makefile to build the AFD library
#

include ../Include.mk

CFLAGS  = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd -I../amg -I../fd -I../misc -I../log -I../afd_mon
LIB     = libafd.a
HEADERS = ../init_afd/afddefs.h ../config.h
OBJS    = attach_afd_status.o\
          attach_buf.o\
          bitstuff.o\
          calc_next_time.o\
          change_alias_order.o\
          change_name.o\
          check_lock.o\
          coe_open.o\
          create_message.o\
          create_name.o\
          create_remote_dir.o\
          check_burst.o\
          check_dir.o\
          check_fra.o\
          check_fsa.o\
          check_job_name.o\
          check_msa.o\
          clr_fl.o\
          copy_file.o\
          daemon_init.o\
          delete_log_ptrs.o\
          detach_afd_status.o\
          eaccess.o\
          eval_host_config.o\
          exec_cmd.o\
          fra_attach.o\
          fra_detach.o\
          fsa_attach.o\
          fsa_detach.o\
          get_afd_name.o\
          get_afd_path.o\
          get_arg.o\
          get_definition.o\
          get_hostname.o\
          get_log_number.o\
          get_max_log_number.o\
          get_mon_path.o\
          get_permissions.o\
          get_position.o\
          get_rename_rules.o\
          get_rule.o\
          get_user.o\
          inform_fd_about_fsa_change.o\
          lock_file.o\
          lock_proc.o\
          lock_region.o\
          lock_region_w.o\
          make_fifo.o\
          map_file.o\
          mmap_emu.o\
          mmap_resize.o\
          move_file.o\
          msa_attach.o\
          msa_detach.o\
          msync_emu.o\
          munmap_emu.o\
          my_strncpy.o\
          my_usleep.o\
          next_counter.o\
          open_counter_file.o\
          posi.o\
          pmatch.o\
          read_file.o\
          readn.o\
          rec.o\
          rec_rmdir.o\
          remove_dir.o\
          reshuffel_log_files.o\
          rlock_region.o\
          send_cmd.o\
          set_afd_euid.o\
          set_fl.o\
          show_fifo_data.o\
          system_log.o\
          t_hostname.o\
          unlock_region.o\
          write_host_config.o


all : $(LIB)

$(OBJS) : $(HEADERS)

$(LIB) : $(OBJS)
	$(AR) $(ARFLAGS) $@ $?

clean :
	rm -f $(OBJS)

clobber :
	$(MAKE) clean
	rm -f $(LIB)

# Other Dependencies
create_name.o next_counter.o open_counter_file.o : ../amg/amgdefs.h
lock_region.o unlock_region.o : ../fd/fddefs.h
mmap_emu.o msync_emu.o munmap_emu.o : ../misc/mmap_emu.h
get_mon_path.o : ../afd_mon/mondefs.h
calc_next_time.o : ../init_afd/bit_array.h

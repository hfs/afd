## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2001 - 2003 Holger Kiehl
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
##

if WITH_AFD_MON
   AFD_MON_PROGS = msa_attach.c msa_detach.c attach_afd_mon_status.c detach_afd_mon_status.c
   EXTRA_AFD_MON =
else
   AFD_MON_PROGS =
   EXTRA_AFD_MON = msa_attach.c msa_detach.c attach_afd_mon_status.c detach_afd_mon_status.c
endif
if WITHOUT_MMAP
   AFD_MAPPER = mmap_emu.c msync_emu.c munmap_emu.c
   EXTRA_MAPPER =
else
   AFD_MAPPER =
   EXTRA_MAPPER = mmap_emu.c msync_emu.c munmap_emu.c
endif
if HAVE_EACCESS
   AFD_EACCESS =
   EXTRA_EACCESS = eaccess.c
else
   AFD_EACCESS = eaccess.c
   EXTRA_EACCESS =
endif
if WITHOUT_DUPCHECK
   DUPCHECK =
   EXTRA_DUPCHECK = isdup.c eval_dupcheck_options.c
else
   DUPCHECK = isdup.c eval_dupcheck_options.c
   EXTRA_DUPCHECK =
endif
if WITH_SETUID
   SET_AFD_EUID = set_afd_euid.c
   EXTRA_SET_AFD_EUID =
else
   SET_AFD_EUID =
   EXTRA_SET_AFD_EUID = set_afd_euid.c
endif
if WITH_FIFO_RW_SUPPORT
   OPEN_FIFO_RW =
   EXTRA_OPEN_FIFO_RW = open_fifo_rw.c
else
   OPEN_FIFO_RW = open_fifo_rw.c
   EXTRA_OPEN_FIFO_RW =
endif
if ERROR_QUEUE_SUPPORT
   HANDLE_ERROR_QUEUE = handle_error_queue.c
   EXTRA_HANDLE_ERROR_QUEUE =
else
   HANDLE_ERROR_QUEUE =
   EXTRA_HANDLE_ERROR_QUEUE = handle_error_queue.c
endif
EXTRA_DIST = $(EXTRA_MAPPER) $(EXTRA_AFD_MON) $(EXTRA_EACCESS)\
             $(EXTRA_DUPCHECK) $(EXTRA_SET_AFD_EUID) $(EXTRA_OPEN_FIFO_RW)\
             $(EXTRA_HANDLE_ERROR_QUEUE)

MAINTAINERCLEANFILES = Makefile.in
INCLUDES             = -I.. -I../init_afd -I../amg -I../fd -I../misc\
                       -I../afd_mon

lib_LIBRARIES        = libafd.a
libafd_a_SOURCES     = attach_afd_status.c\
                       attach_buf.c\
                       bitstuff.c\
                       calc_next_time.c\
                       change_alias_order.c\
                       change_name.c\
                       check_lock.c\
                       coe_open.c\
                       create_message.c\
                       create_remote_dir.c\
                       check_create_path.c\
                       check_dir.c\
                       check_fake_user.c\
                       check_fra.c\
                       check_fsa.c\
                       check_msa.c\
                       clr_fl.c\
                       copy_file.c\
                       create_name.c\
                       daemon_init.c\
                       datestr2unixtime.c\
                       delete_log_ptrs.c\
                       detach_afd_status.c\
                       $(AFD_EACCESS)\
                       error_action.c\
                       eval_host_config.c\
                       eval_timeout.c\
                       exec_cmd.c\
                       fra_attach.c\
                       fra_detach.c\
                       fsa_attach.c\
                       fsa_detach.c\
                       get_afd_name.c\
                       get_afd_path.c\
                       get_arg.c\
                       get_checksum.c\
                       get_definition.c\
                       get_dir_number.c\
                       get_dir_options.c\
                       get_error_str.c\
                       get_hostname.c\
                       get_log_number.c\
                       get_max_log_number.c\
                       get_mon_path.c\
                       get_permissions.c\
                       get_position.c\
                       get_pw.c\
                       get_rename_rules.c\
                       get_rule.c\
                       get_user.c\
                       $(HANDLE_ERROR_QUEUE)\
                       inform_fd_about_fsa_change.c\
                       $(DUPCHECK)\
                       is_msgname.c\
                       lock_file.c\
                       lock_proc.c\
                       lock_region.c\
                       lock_region_w.c\
                       make_fifo.c\
                       map_file.c\
                       $(AFD_MAPPER)\
                       mmap_resize.c\
                       $(AFD_MON_PROGS)\
                       move_file.c\
                       my_strncpy.c\
                       my_usleep.c\
                       next_counter.c\
                       open_counter_file.c\
                       $(OPEN_FIFO_RW)\
                       posi.c\
                       pmatch.c\
                       production_log.c\
                       read_file.c\
                       readn.c\
                       rec.c\
                       rec_rmdir.c\
                       remove_dir.c\
                       remove_files.c\
                       reshuffel_log_files.c\
                       rlock_region.c\
                       send_cmd.c\
                       $(SET_AFD_EUID)\
                       set_fl.c\
                       show_fifo_data.c\
                       system_log.c\
                       t_hostname.c\
                       unlock_region.c\
                       unmap_data.c\
                       write_host_config.c\
                       ../init_afd/afddefs.h\
                       ../afdsetup.h\
                       ../../config.h

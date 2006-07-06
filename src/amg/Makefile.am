## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2001 - 2005 Holger Kiehl
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

MAINTAINERCLEANFILES  = Makefile.in
noinst_HEADERS        = amgdefs.h
INCLUDES              = -I.. -I../init_afd -I../fd
bin_PROGRAMS          = amg dir_check
EXTRA_PROGRAMS        = test_in_time test_time show_amg_data
amg_SOURCES           = amg.c\
                        amg_zombie_check.c\
                        clear_pool_dir.c\
                        com.c\
                        convert_fsa.c\
                        convert_fra.c\
                        create_fsa.c\
                        create_fra.c\
                        create_sa.c\
                        eval_dir_config.c\
                        eval_dir_options.c\
                        eval_time_str.c\
                        lookup_dc_id.c\
                        lookup_dir_id.c\
                        make_process_amg.c\
                        remove_pool_directory.c\
                        reread_dir_config.c\
                        reread_host_config.c\
                        store_file_mask.c\
                        store_passwd.c
amg_LDADD             = ../common/libafd.a
dir_check_SOURCES     = check_files.c\
                        check_paused_dir.c\
                        check_old_time_jobs.c\
                        clear_msg_buffer.c\
                        count_pool_files.c\
                        create_db.c\
                        dir_check.c\
                        enter_time_job.c\
                        eval_time_str.c\
                        handle_options.c\
                        handle_time_jobs.c\
                        init_dir_check.c\
                        init_job_data.c\
                        init_msg_buffer.c\
                        in_time.c\
                        link_files.c\
                        lookup_fra_pos.c\
                        lookup_file_mask_id.c\
                        lookup_job_id.c\
                        next.c\
                        receive_log.c\
                        remove_time_dir.c\
                        rename_files.c\
                        save_files.c\
                        search_old_files.c\
                        send_message.c\
                        sort_time_job.c\
                        store_passwd.c
dir_check_LDADD       = ../common/libafd.a ../misc/libmisc.a @EXTRA_SOCKET_LIBS@
test_time_SOURCES     = eval_time_str.c\
                        test_time.c
test_time_LDADD       = ../common/libafd.a
test_in_time_SOURCES  = eval_time_str.c\
                        in_time.c\
                        test_in_time.c
test_in_time_LDADD    = ../common/libafd.a
show_amg_data_SOURCES = show_amg_data.c
show_amg_data_LDADD   = ../common/libafd.a

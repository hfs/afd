## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2001 - 2010 Holger Kiehl
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

if WITH_MISC_LIB
   MISC_LIB = ../misc/libmisc.a
else
   MISC_LIB =
endif
if WITH_MAP_SUPPORT
   AFD_MAP_PROG       = sf_map
   EXTRA_MAP_PROG     =
else
   AFD_MAP_PROG       =
   EXTRA_MAP_PROG     = sf_map
endif
if WITH_OUTPUT_LOG
   OUTPUT_LOG_PTRS    = output_log_fd.c output_log_ptrs.c
   EXTRA_OUTPUT_PTRS  =
else
   OUTPUT_LOG_PTRS    =
   EXTRA_OUTPUT_PTRS  = output_log_fd.c output_log_ptrs.c
endif

noinst_HEADERS        = fddefs.h
INCLUDES              = -I.. -I../init_afd -I../amg -I../log -I../misc\
                        -I../protocols
bin_PROGRAMS          = fd sf_ftp sf_ftp_trace sf_smtp sf_smtp_trace\
                        sf_scp sf_scp_trace sf_sftp sf_sftp_trace sf_wmo\
                        sf_wmo_trace $(AFD_MAP_PROG) sf_loc gf_ftp\
                        gf_ftp_trace gf_http gf_http_trace gf_sftp\
                        gf_sftp_trace sf_http sf_http_trace
EXTRA_PROGRAMS        = $(EXTRA_MAP_PROG)
EXTRA_DIST            = $(EXTRA_OUTPUT_PTRS)
MAINTAINERCLEANFILES  = Makefile.in $(EXTRA_PROGRAMS)
CLEANFILES            = gmon.out core core.*
fd_SOURCES            = check_fra_fd.c\
                        check_fsa_entries.c\
                        check_msg_time.c\
                        check_queue_space.c\
                        fd.c\
                        fd_check_fsa.c\
                        get_job_data.c\
                        get_new_positions.c\
                        handle_delete_fifo.c\
                        init_fifos_fd.c\
                        init_fra_data.c\
                        init_msg_buffer.c\
                        init_msg_ptrs.c\
                        lookup_job_id.c\
                        manage_trl_process.c\
                        read_current_msg_list.c\
                        recount_jobs_queued.c\
                        recreate_msg.c\
                        remove_connection.c\
                        remove_ls_data.c\
                        remove_msg.c
fd_LDADD              = ../common/libafd.a
fd_DEPENDENCIES       = $(fd_LDADD) fddefs.h
sf_ftp_SOURCES        = append.c\
                        archive_file.c\
                        check_burst_2.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        handle_proxy.c\
                        init_sf.c\
                        init_sf_burst2.c\
                        limit_transfer_rate.c\
                        noop_wrapper_ftp.c\
                        $(OUTPUT_LOG_PTRS)\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_ftp.c\
                        trans_db_log.c\
                        trans_exec.c\
                        trans_log.c\
                        update_tfc.c
sf_ftp_LDADD          = $(MISC_LIB) @EXTRA_SOCKET_LIBS@\
                        ../protocols/libprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
sf_ftp_trace_SOURCES  = $(sf_ftp_SOURCES) trace_log.c
sf_ftp_trace_LDADD    = $(MISC_LIB) @EXTRA_SOCKET_LIBS@\
                        ../protocols/libtraceprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
sf_http_SOURCES       = append.c\
                        archive_file.c\
                        check_burst_2.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        init_sf_burst2.c\
                        limit_transfer_rate.c\
                        next_wmo_counter.c\
                        noop_wrapper_dummy.c\
                        $(OUTPUT_LOG_PTRS)\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_http.c\
                        trans_db_log.c\
                        trans_exec.c\
                        trans_log.c\
                        update_tfc.c
sf_http_LDADD         = $(MISC_LIB) @EXTRA_SOCKET_LIBS@\
                        ../protocols/libprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
sf_http_trace_SOURCES = $(sf_http_SOURCES) trace_log.c
sf_http_trace_LDADD   = $(MISC_LIB) @EXTRA_SOCKET_LIBS@\
                        ../protocols/libtraceprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
sf_sftp_SOURCES       = append.c\
                        archive_file.c\
                        check_burst_2.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        init_sf_burst2.c\
                        limit_transfer_rate.c\
                        noop_wrapper_sftp.c\
                        $(OUTPUT_LOG_PTRS)\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_sftp.c\
                        trans_db_log.c\
                        trans_exec.c\
                        trans_log.c\
                        update_tfc.c
sf_sftp_LDADD         = $(MISC_LIB) @EXTRA_SOCKET_LIBS@\
                        ../protocols/libprotocols.a\
                        ../common/libafd.a @AFD_UTIL_LIB@
sf_sftp_trace_SOURCES = $(sf_sftp_SOURCES) trace_log.c
sf_sftp_trace_LDADD   = $(MISC_LIB) @EXTRA_SOCKET_LIBS@\
                        ../protocols/libtraceprotocols.a\
                        ../common/libafd.a @AFD_UTIL_LIB@
sf_smtp_SOURCES       = append.c\
                        archive_file.c\
                        check_burst_2.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        init_sf_burst2.c\
                        limit_transfer_rate.c\
                        noop_wrapper_smtp.c\
                        $(OUTPUT_LOG_PTRS)\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_smtp.c\
                        trans_db_log.c\
                        trans_exec.c\
                        trans_log.c\
                        update_tfc.c
sf_smtp_LDADD         = @EXTRA_SOCKET_LIBS@ ../protocols/libprotocols.a\
                        ../common/libafd.a
sf_smtp_trace_SOURCES = $(sf_smtp_SOURCES) trace_log.c
sf_smtp_trace_LDADD   = @EXTRA_SOCKET_LIBS@ ../protocols/libtraceprotocols.a\
                        ../common/libafd.a
sf_scp_SOURCES        = append.c\
                        archive_file.c\
                        check_burst_2.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        init_sf_burst2.c\
                        limit_transfer_rate.c\
                        noop_wrapper_dummy.c\
                        $(OUTPUT_LOG_PTRS)\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_scp.c\
                        trans_db_log.c\
                        trans_exec.c\
                        trans_log.c\
                        update_tfc.c
sf_scp_LDADD          = @EXTRA_SOCKET_LIBS@ ../protocols/libprotocols.a\
                        ../common/libafd.a @AFD_UTIL_LIB@
sf_scp_trace_SOURCES  = $(sf_scp_SOURCES) trace_log.c
sf_scp_trace_LDADD    = @EXTRA_SOCKET_LIBS@ ../protocols/libtraceprotocols.a\
                        ../common/libafd.a @AFD_UTIL_LIB@
sf_wmo_SOURCES        = append.c\
                        archive_file.c\
                        check_burst_2.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        init_sf_burst2.c\
                        limit_transfer_rate.c\
                        next_wmo_counter.c\
                        noop_wrapper_dummy.c\
                        $(OUTPUT_LOG_PTRS)\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_wmo.c\
                        trans_db_log.c\
                        trans_exec.c\
                        trans_log.c\
                        update_tfc.c
sf_wmo_LDADD          = @EXTRA_SOCKET_LIBS@ ../protocols/libprotocols.a\
                        ../common/libafd.a
sf_wmo_trace_SOURCES  = $(sf_wmo_SOURCES) trace_log.c
sf_wmo_trace_LDADD    = @EXTRA_SOCKET_LIBS@ ../protocols/libtraceprotocols.a\
                        ../common/libafd.a
sf_map_SOURCES        = append.c\
                        archive_file.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        noop_wrapper_dummy.c\
                        $(OUTPUT_LOG_PTRS)\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_map.c\
                        trans_db_log.c\
                        trans_log.c\
                        update_tfc.c
sf_map_LDADD          = @EXTRA_SOCKET_LIBS@ ../common/libafd.a @AFD_MAP_LIBS@
sf_loc_SOURCES        = append.c\
                        archive_file.c\
                        attach_ls_data.c\
                        check_burst_2.c\
                        compare_dir_local.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        init_sf_burst2.c\
                        noop_wrapper_dummy.c\
                        $(OUTPUT_LOG_PTRS)\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_loc.c\
                        trans_db_log.c\
                        trans_exec.c\
                        trans_log.c\
                        update_tfc.c
sf_loc_LDADD          = @EXTRA_SOCKET_LIBS@ ../common/libafd.a
gf_ftp_SOURCES        = attach_ls_data.c\
                        eval_input_gf.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_group_list.c\
                        get_remote_file_names_ftp.c\
                        gf_ftp.c\
                        gsf_check_fra.c\
                        gsf_check_fsa.c\
                        handle_proxy.c\
                        init_gf.c\
                        limit_transfer_rate.c\
                        read_file_mask.c\
                        remove_ls_data.c\
                        reset_fsa.c\
                        reset_values.c\
                        send_proc_fin.c\
                        trans_db_log.c\
                        trans_log.c
gf_ftp_LDADD          = @EXTRA_SOCKET_LIBS@ ../protocols/libprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
gf_ftp_trace_SOURCES  = $(gf_ftp_SOURCES) trace_log.c
gf_ftp_trace_LDADD    = @EXTRA_SOCKET_LIBS@ ../protocols/libtraceprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
gf_sftp_SOURCES       = attach_ls_data.c\
                        eval_input_gf.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_group_list.c\
                        get_remote_file_names_sftp.c\
                        gf_sftp.c\
                        gsf_check_fra.c\
                        gsf_check_fsa.c\
                        init_gf.c\
                        limit_transfer_rate.c\
                        read_file_mask.c\
                        reset_fsa.c\
                        reset_values.c\
                        send_proc_fin.c\
                        trans_db_log.c\
                        trans_log.c
gf_sftp_LDADD         = @EXTRA_SOCKET_LIBS@ ../protocols/libprotocols.a\
                        ../common/libafd.a @AFD_UTIL_LIB@
gf_sftp_trace_SOURCES = $(gf_sftp_SOURCES) trace_log.c
gf_sftp_trace_LDADD   = @EXTRA_SOCKET_LIBS@ ../protocols/libtraceprotocols.a\
                        ../common/libafd.a @AFD_UTIL_LIB@
gf_http_SOURCES       = attach_ls_data.c\
                        eval_input_gf.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_group_list.c\
                        get_remote_file_names_http.c\
                        gf_http.c\
                        gsf_check_fra.c\
                        gsf_check_fsa.c\
                        init_gf.c\
                        limit_transfer_rate.c\
                        read_file_mask.c\
                        remove_ls_data.c\
                        reset_fsa.c\
                        reset_values.c\
                        send_proc_fin.c\
                        trans_db_log.c\
                        trans_log.c
gf_http_LDADD         = @EXTRA_SOCKET_LIBS@ ../protocols/libprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
gf_http_trace_SOURCES = $(gf_http_SOURCES) trace_log.c
gf_http_trace_LDADD   = @EXTRA_SOCKET_LIBS@ ../protocols/libtraceprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@

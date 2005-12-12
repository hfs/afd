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

MAINTAINERCLEANFILES = Makefile.in
noinst_HEADERS       = x_common_defs.h
INCLUDES             = @AFD_MOTIF_INCLUDES@ -I../../.. -I../../../init_afd\
                       -I../afd_ctrl -I../show_olog
lib_LIBRARIES        = libxafd.a
libxafd_a_SOURCES    = check_host_permissions.c\
                       check_info_file.c\
                       check_nummeric.c\
                       config_log.c\
                       generic_print.c\
                       get_current_jid_list.c\
                       get_file_mask_list.c\
                       get_ip_no.c\
                       get_printer_cmd.c\
                       handle_setup_file.c\
                       init_color.c\
                       insert_passwd.c\
                       locate_xy.c\
                       lookup_color.c\
                       make_xprocess.c\
                       print_file_size.c\
                       sfilter.c\
                       show_info.c\
                       show_message.c\
                       store_host_names.c\
                       update_time.c\
                       wait_visible.c\
                       window_id.c\
                       xrec.c

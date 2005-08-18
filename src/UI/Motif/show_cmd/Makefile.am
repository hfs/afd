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

MAINTAINERCLEANFILES = Makefile.in
noinst_HEADERS       = show_cmd.h
INCLUDES             = @AFD_MOTIF_INCLUDES@ -I../../.. -I../common\
                       -I../../../init_afd -I../../../log -I../afd_ctrl\
                       -I../../../afd_mon
bin_PROGRAMS         = show_cmd
show_cmd_SOURCES     = cmd_callbacks.c\
                       xexec_cmd.c\
                       print_data.c\
                       show_cmd.c
show_cmd_LDADD       = ../common/libxafd.a\
                       ../../../common/libafd.a @AFD_MOTIF_LIBS@

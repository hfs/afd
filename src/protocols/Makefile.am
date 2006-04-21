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

MAINTAINERCLEANFILES        = Makefile.in
noinst_HEADERS              = commondefs.h ftpdefs.h httpdefs.h scpdefs.h\
                              sftpdefs.h smtpdefs.h ssh_commondefs.h wmodefs.h
INCLUDES                    = -I.. -I../init_afd -I../fd @AFD_SSL_INCLUDES@
lib_LIBRARIES               = libprotocols.a libtraceprotocols.a
libprotocols_a_SOURCES      = common.c\
                              encode_base64.c\
                              ftpcmd.c\
                              httpcmd.c\
                              scpcmd.c\
                              sftpcmd.c\
                              smtpcmd.c\
                              ssh_common.c\
                              wmocmd.c
libtraceprotocols_a_SOURCES = $(libprotocols_a_SOURCES)
libtraceprotocols_a_CFLAGS  = -DWITH_TRACE

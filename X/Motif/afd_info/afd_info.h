/*
 *  afd_info.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __afd_info_h
#define __afd_info_h

#include "x_common_defs.h"

#define MAXARGS                  20
#define MAX_INFO_STRING_LENGTH   40
#define NO_OF_FSA_ROWS           5
#define FSA_INFO_TEXT_WIDTH_L    15
#define FSA_INFO_TEXT_WIDTH_R    18
#define AFD_INFO_LENGTH          20

#define UPDATE_INTERVAL          1000
#define FILE_UPDATE_INTERVAL     4

#define INFO_IDENTIFIER          "INFO-"
#define NO_SECODARY_HOST         "No secondary host    :"
#define SYSTEM_HOST_FILE         "/etc/hosts"

struct prev_values
       {
          char          real_hostname[2][MAX_REAL_HOSTNAME_LENGTH];
          int           retry_interval;
          unsigned int  files_send;
          unsigned int  bytes_send;
          unsigned int  total_errors;
          unsigned int  no_of_connections;
          time_t        last_connection;
          time_t        host_file_time;
#ifdef _WITH_XPM
          char          host_toggle;
#endif
          char          toggle_pos;
          unsigned int  protocol;
       };

/* Function prototypes */
extern void close_button(Widget, XtPointer, XtPointer),
            update_info(Widget);

#endif /* __afd_info_h */

/*
 *  dir_info.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __dir_info_h
#define __dir_info_h

#include "x_common_defs.h"

#define MAXARGS                  20
#define MAX_INFO_STRING_LENGTH   60
#define NO_OF_DIR_ROWS           6
#define DIR_INFO_TEXT_WIDTH_L    15
#define DIR_INFO_TEXT_WIDTH_R    18
#define DIR_INFO_LENGTH_L        20
#define DIR_INFO_LENGTH_R        20

#define UPDATE_INTERVAL          1000
#define FILE_UPDATE_INTERVAL     4

struct prev_values
       {
          char          real_dir_name[MAX_PATH_LENGTH];
          char          host_alias[MAX_HOSTNAME_LENGTH + 1];
          int           dir_pos;
          char          dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          char          display_url[MAX_RECIPIENT_LENGTH];
          char          url[MAX_RECIPIENT_LENGTH];
          char          priority;
          unsigned char stupid_mode;
          unsigned char remove;
          char          force_reread;
          unsigned char report_unknown_files;
          int           unknown_file_time;
          int           queued_file_time;
          unsigned char delete_files_flag;
          unsigned long bytes_received;
          unsigned int  files_received;
          time_t        last_retrieval;
          time_t        next_check_time;
          unsigned char time_option;
       };

/* Function prototypes */
extern void close_button(Widget, XtPointer, XtPointer),
            update_info(Widget);

#endif /* __dir_info_h */

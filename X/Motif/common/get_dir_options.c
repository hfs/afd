/*
 *  get_dir_options.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#include "afddefs.h"

DESCR__S_M3
/*
 ** NAME
 **   get_dir_options - gets directory options and store them in a structure
 **
 ** SYNOPSIS
 **   void get_dir_options(int dir_no, struct dir_options *d_o)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.07.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "x_common_defs.h"

/* External global variables. */
extern Widget                     toplevel_w;
extern int                        fra_fd,
                                  no_of_dirs;
extern struct fileretrieve_status *fra;


/*+++++++++++++++++++++++++++ get_dir_options() +++++++++++++++++++++++++*/
void                                                                
get_dir_options(int dir_pos, struct dir_options *d_o)
{
   register int i;

   if (fra_fd == -1)
   {
      /* Attach to FRA to get directory options. */
      if (fra_attach() < 0)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to attach to FRA. (%s %d)", __FILE__, __LINE__);
         return;
      }
   }

   d_o->url[0] = '\0';
   for (i = 0; i < no_of_dirs; i++)
   {
      if (dir_pos == fra[i].dir_pos)
      {
         d_o->no_of_dir_options = 0;
         (void)strcpy(d_o->dir_alias, fra[i].dir_alias);
         if (fra[i].delete_files_flag != 0)
         {
            if (fra[i].delete_files_flag & UNKNOWN_FILES)
            {
               (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                            DEL_UNKNOWN_FILES_ID);
               d_o->no_of_dir_options++;
            }
            if (fra[i].delete_files_flag & QUEUED_FILES)
            {
               (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                            DEL_QUEUED_FILES_ID);
               d_o->no_of_dir_options++;
            }

            if (fra[i].old_file_time > 0)
            {
               (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %d",
                             OLD_FILE_TIME_ID, fra[i].old_file_time / 3600);
               d_o->no_of_dir_options++;
            }
         }
         if (fra[i].report_unknown_files == NO)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         DONT_REP_UNKNOWN_FILES_ID);
            d_o->no_of_dir_options++;
         }
         else if ((fra[i].old_file_time != 86400) && (fra[i].old_file_time > 0))
              {
                 (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                              REP_UNKNOWN_FILES_ID);
                 d_o->no_of_dir_options++;

                 if (fra[i].delete_files_flag == 0)
                 {
                    (void)sprintf(d_o->aoptions[d_o->no_of_dir_options],
                                  "%s %d", OLD_FILE_TIME_ID,
                                  fra[i].old_file_time / 3600);
                    d_o->no_of_dir_options++;
                 }
              }
         if (fra[i].stupid_mode == NO)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         STORE_REMOTE_LIST);
            d_o->no_of_dir_options++;
         }
         if (fra[i].force_reread == YES)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options], "force reread");
            d_o->no_of_dir_options++;
         }
         if (fra[i].end_character != -1)
         {
            (void)sprintf(d_o->aoptions[d_o->no_of_dir_options],
                          "%s %d", END_CHARACTER_ID, fra[i].end_character);
            d_o->no_of_dir_options++;
         }
         if (fra[i].protocol == FTP)
         {
            (void)strcpy(d_o->url, fra[i].url);
         }
         break;
      }
   }

   return;
}

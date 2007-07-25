/*
 *  get_dir_options.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   void get_dir_options(unsigned int dir_id, struct dir_options *d_o)
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
 **   22.05.2002 H.Kiehl Separate old file times for unknown and queued files.
 **   03.09.2004 H.Kiehl Added "ignore size" and "ignore file time" options.
 **   17.08.2005 H.Kiehl Move this function to common section
 **                      (from Motif/common).
 **   20.12.2006 H.Kiehl Added dupcheck option filename without last
 **                      suffix.
 **   16.02.2007 H.Kiehl Added 'warn time' option.
 **   24.02.2007 H.Kiehl Added 'inotify' option.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "amgdefs.h"
#include "bit_array.h"

/* External global variables. */
extern int                        fra_fd,
                                  no_of_dirs;
extern struct fileretrieve_status *fra;


/*+++++++++++++++++++++++++++ get_dir_options() +++++++++++++++++++++++++*/
void                                                                
get_dir_options(unsigned int dir_id, struct dir_options *d_o)
{
   register int i;
   int          attached = NO;

   if (fra_fd == -1)
   {
      /* Attach to FRA to get directory options. */
      if (fra_attach_passive() < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to attach to FRA.");
         return;
      }
      attached = YES;
   }

   d_o->url[0] = '\0';
   d_o->no_of_dir_options = 0;
   for (i = 0; i < no_of_dirs; i++)
   {
      if (dir_id == fra[i].dir_id)
      {
         (void)strcpy(d_o->dir_alias, fra[i].dir_alias);
         if (fra[i].delete_files_flag != 0)
         {
            if ((fra[i].delete_files_flag & UNKNOWN_FILES) &&
                (fra[i].in_dc_flag & UNKNOWN_FILES_IDC))
            {
               (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %d",
                             DEL_UNKNOWN_FILES_ID,
                             fra[i].unknown_file_time / 3600);
               d_o->no_of_dir_options++;
            }
            if ((fra[i].delete_files_flag & QUEUED_FILES) &&
                (fra[i].in_dc_flag & QUEUED_FILES_IDC))
            {
               (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %d",
                             DEL_QUEUED_FILES_ID,
                             fra[i].queued_file_time / 3600);
               d_o->no_of_dir_options++;
            }
            if ((fra[i].delete_files_flag & OLD_LOCKED_FILES) &&
                (fra[i].in_dc_flag & OLD_LOCKED_FILES_IDC))
            {
               (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %d",
                             DEL_OLD_LOCKED_FILES_ID,
                             fra[i].locked_file_time / 3600);
               d_o->no_of_dir_options++;
            }
         }
         if ((fra[i].report_unknown_files == NO) &&
             (fra[i].in_dc_flag & DONT_REPUKW_FILES_IDC))
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         DONT_REP_UNKNOWN_FILES_ID);
            d_o->no_of_dir_options++;
         }
         if ((fra[i].report_unknown_files == YES) &&
             (fra[i].in_dc_flag & REPUKW_FILES_IDC))
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         REP_UNKNOWN_FILES_ID);
            d_o->no_of_dir_options++;
         }
#ifndef _WITH_PTHREAD
         if (fra[i].important_dir == YES)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         IMPORTANT_DIR_ID);
            d_o->no_of_dir_options++;
         }
#endif
         if ((fra[i].in_dc_flag & WARN_TIME_IDC) &&
             (fra[i].warn_time != DEFAULT_DIR_WARN_TIME))
         {
#if SIZEOF_TIME_T == 4
            (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %ld",
#else
            (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %lld",
#endif
                          DIR_WARN_TIME_ID, (pri_time_t)fra[i].warn_time);
            d_o->no_of_dir_options++;
         }
         if ((fra[i].in_dc_flag & KEEP_CONNECTED_IDC) &&
             (fra[i].keep_connected != DEFAULT_KEEP_CONNECTED_TIME))
         {
            (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %u",
                          KEEP_CONNECTED_ID, fra[i].keep_connected);
            d_o->no_of_dir_options++;
         }
#ifdef WITH_INOTIFY
         if (fra[i].in_dc_flag & INOTIFY_FLAG_IDC)
         {
            (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %u",
                          INOTIFY_FLAG_ID, fra[i].inotify_flag);
            d_o->no_of_dir_options++;
         }
#endif
         if (fra[i].max_process != MAX_PROCESS_PER_DIR)
         {
            (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %d",
                          MAX_PROCESS_ID, fra[i].max_process);
            d_o->no_of_dir_options++;
         }
         if ((fra[i].max_copied_files != MAX_COPIED_FILES) &&
             (fra[i].in_dc_flag & MAX_CP_FILES_IDC))
         {
            (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %d",
                          MAX_FILES_ID, fra[i].max_copied_files);
            d_o->no_of_dir_options++;
         }
         if ((fra[i].max_copied_file_size != (MAX_COPIED_FILE_SIZE * 1024)) &&
             (fra[i].in_dc_flag & MAX_CP_FILE_SIZE_IDC))
         {
#if SIZEOF_OFF_T == 4
            (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %ld",
#else
            (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %lld",
#endif
                         MAX_SIZE_ID,
                         (pri_off_t)(fra[i].max_copied_file_size / 1024));
            d_o->no_of_dir_options++;
         }
         if (fra[i].ignore_size != 0)
         {
            char gt_lt_sign;

            if (fra[i].gt_lt_sign & ISIZE_GREATER_THEN)
            {
               gt_lt_sign = '>';
            }
            else if (fra[i].gt_lt_sign & ISIZE_LESS_THEN)
                 {
                    gt_lt_sign = '<';
                 }
                 else
                 {
                    gt_lt_sign = '=';
                 }
            if (gt_lt_sign == '=')
            {
#if SIZEOF_OFF_T == 4
               (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %ld",
#else
               (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %lld",
#endif
                             IGNORE_SIZE_ID, (pri_off_t)fra[i].ignore_size);
            }
            else
            {
#if SIZEOF_OFF_T == 4
               (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %c%ld",
#else
               (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %c%lld",
#endif
                             IGNORE_SIZE_ID, gt_lt_sign,
                             (pri_off_t)fra[i].ignore_size);
            }
            d_o->no_of_dir_options++;
         }
         if (fra[i].priority != DEFAULT_PRIORITY)
         {
            (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %c",
                          PRIORITY_ID, fra[i].priority);
            d_o->no_of_dir_options++;
         }
         if (fra[i].wait_for_filename[0] != '\0')
         {
            (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %s",
                          WAIT_FOR_FILENAME_ID, fra[i].wait_for_filename);
            d_o->no_of_dir_options++;
         }
         if (fra[i].accumulate != 0)
         {
            (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %d",
                          ACCUMULATE_ID, fra[i].accumulate);
            d_o->no_of_dir_options++;
         }
         if (fra[i].accumulate_size != 0)
         {
#if SIZEOF_OFF_T == 4
            (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %ld",
#else
            (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %lld",
#endif
                          ACCUMULATE_SIZE_ID,
                          (pri_off_t)fra[i].accumulate_size);
            d_o->no_of_dir_options++;
         }
         if (fra[i].stupid_mode == NO)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         STORE_RETRIEVE_LIST_ID);
            d_o->no_of_dir_options++;
         }
         else if (fra[i].stupid_mode == GET_ONCE_ONLY)
              {
                 (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                              STORE_RETRIEVE_LIST_ID);
                 (void)strcat(d_o->aoptions[d_o->no_of_dir_options], " once");
                 d_o->no_of_dir_options++;
              }
         if (fra[i].remove == NO)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         DO_NOT_REMOVE_ID);
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
         if (fra[i].ignore_file_time != 0)
         {
            char gt_lt_sign;

            if (fra[i].gt_lt_sign & IFTIME_GREATER_THEN)
            {
               gt_lt_sign = '>';
            }
            else if (fra[i].gt_lt_sign & IFTIME_LESS_THEN)
                 {
                    gt_lt_sign = '<';
                 }
                 else
                 {
                    gt_lt_sign = '=';
                 }
            if (gt_lt_sign == '=')
            {
               (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %u",
                             IGNORE_FILE_TIME_ID, fra[i].ignore_file_time);
            }
            else
            {
               (void)sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s %c%u",
                             IGNORE_FILE_TIME_ID, gt_lt_sign,
                             fra[i].ignore_file_time);
            }
            d_o->no_of_dir_options++;
         }
         if (fra[i].time_option == YES)
         {
            int           length,
                          ii;
            unsigned char full_minute[8] = {0,0,0,0,0,0,0,0};

            length = sprintf(d_o->aoptions[d_o->no_of_dir_options], "%s ",
                             TIME_ID);

            /* Minute */
#ifdef _WORKING_LONG_LONG
            if (fra[i].te.minute == ALL_MINUTES)
#else
            for (ii = 0; ii < 60; ii++)
            {
               bitset(full_minute, ii);
            }
            if (memcmp(fra[i].te.minute, full_minute, 8) == 0)
#endif
            {
               d_o->aoptions[d_o->no_of_dir_options][length] = '*';
               d_o->aoptions[d_o->no_of_dir_options][length + 1] = ' ';
               length += 2;
            }
            else
            {
               for (ii = 0; ii < 60; ii++)
               {
#ifdef _WORKING_LONG_LONG
                  if (fra[i].te.minute & bit_array[ii])
#else
                  if (bittest(fra[i].te.minute, ii) == YES)
#endif
                  {
                     if (d_o->aoptions[d_o->no_of_dir_options][length - 1] == ' ')
                     {
                        length += sprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                          "%d", ii);
                     }
                     else
                     {
                        length += sprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                          ",%d", ii);
                     }
                  }
               }
               d_o->aoptions[d_o->no_of_dir_options][length] = ' ';
               length++;
            }

            /* Hour */
            if (fra[i].te.hour == ALL_HOURS)
            {
               d_o->aoptions[d_o->no_of_dir_options][length] = '*';
               d_o->aoptions[d_o->no_of_dir_options][length + 1] = ' ';
               length += 2;
            }
            else
            {
               for (ii = 0; ii < 24; ii++)
               {
                  if (fra[i].te.hour & bit_array[ii])
                  {
                     if (d_o->aoptions[d_o->no_of_dir_options][length - 1] == ' ')
                     {
                        length += sprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                          "%d", ii);
                     }
                     else
                     {
                        length += sprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                          ",%d", ii);
                     }
                  }
               }
               d_o->aoptions[d_o->no_of_dir_options][length] = ' ';
               length++;
            }

            /* Day of Month */
            if (fra[i].te.day_of_month == ALL_DAY_OF_MONTH)
            {
               d_o->aoptions[d_o->no_of_dir_options][length] = '*';
               d_o->aoptions[d_o->no_of_dir_options][length + 1] = ' ';
               length += 2;
            }
            else
            {
               for (ii = 1; ii < 32; ii++)
               {
                  if (fra[i].te.day_of_month & bit_array[ii - 1])
                  {
                     if (d_o->aoptions[d_o->no_of_dir_options][length - 1] == ' ')
                     {
                        length += sprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                          "%d", ii);
                     }
                     else
                     {
                        length += sprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                          ",%d", ii);
                     }
                  }
               }
               d_o->aoptions[d_o->no_of_dir_options][length] = ' ';
               length++;
            }

            /* Month */
            if (fra[i].te.month == ALL_MONTH)
            {
               d_o->aoptions[d_o->no_of_dir_options][length] = '*';
               d_o->aoptions[d_o->no_of_dir_options][length + 1] = ' ';
               length += 2;
            }
            else
            {
               for (ii = 1; ii < 13; ii++)
               {
                  if (fra[i].te.month & bit_array[ii - 1])
                  {
                     if (d_o->aoptions[d_o->no_of_dir_options][length - 1] == ' ')
                     {
                        length += sprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                          "%d", ii);
                     }
                     else
                     {
                        length += sprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                          ",%d", ii);
                     }
                  }
               }
               d_o->aoptions[d_o->no_of_dir_options][length] = ' ';
               length++;
            }

            /* Day of Week */
            if (fra[i].te.day_of_week == ALL_DAY_OF_WEEK)
            {
               d_o->aoptions[d_o->no_of_dir_options][length] = '*';
               d_o->aoptions[d_o->no_of_dir_options][length + 1] = ' ';
               length += 2;
            }
            else
            {
               for (ii = 1; ii < 8; ii++)
               {
                  if (fra[i].te.day_of_week & bit_array[ii - 1])
                  {
                     if (d_o->aoptions[d_o->no_of_dir_options][length - 1] == ' ')
                     {
                        length += sprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                          "%d", ii);
                     }
                     else
                     {
                        length += sprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                          ",%d", ii);
                     }
                  }
               }
               d_o->aoptions[d_o->no_of_dir_options][length] = ' ';
               length++;
            }
            d_o->aoptions[d_o->no_of_dir_options][length] = '\0';
            d_o->no_of_dir_options++;
         }
#ifdef WITH_DUP_CHECK
         if (fra[i].dup_check_flag != 0)
         {
            int length;

            if (fra[i].dup_check_flag & DC_FILENAME_ONLY)
            {
               length = sprintf(d_o->aoptions[d_o->no_of_dir_options],
# if SIZEOF_TIME_T == 4
                                "%s %ld %d",
# else
                                "%s %lld %d",
# endif
                                DUPCHECK_ID,
                                (pri_time_t)fra[i].dup_check_timeout,
                                DC_FILENAME_ONLY_BIT);
            }
            else if (fra[i].dup_check_flag & DC_NAME_NO_SUFFIX)
                 {
                    length = sprintf(d_o->aoptions[d_o->no_of_dir_options],
# if SIZEOF_TIME_T == 4
                                     "%s %ld %d",
# else
                                     "%s %lld %d",
# endif
                                     DUPCHECK_ID,
                                     (pri_time_t)fra[i].dup_check_timeout,
                                     DC_NAME_NO_SUFFIX_BIT);
                 }
            else if (fra[i].dup_check_flag & DC_FILE_CONTENT)
                 {
                    length = sprintf(d_o->aoptions[d_o->no_of_dir_options],
# if SIZEOF_TIME_T == 4
                                     "%s %ld %d",
# else
                                     "%s %lld %d",
# endif
                                     DUPCHECK_ID,
                                     (pri_time_t)fra[i].dup_check_timeout,
                                     DC_FILE_CONTENT_BIT);
                 }
                 else
                 {
                    length = sprintf(d_o->aoptions[d_o->no_of_dir_options],
# if SIZEOF_TIME_T == 4
                                     "%s %ld %d",
# else
                                     "%s %lld %d",
# endif
                                     DUPCHECK_ID,
                                     (pri_time_t)fra[i].dup_check_timeout,
                                     DC_FILE_CONT_NAME_BIT);
                 }
            if (fra[i].dup_check_flag & DC_DELETE)
            {
               if (fra[i].dup_check_flag & DC_WARN)
               {
                  length += sprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                    " %d %d", DC_DELETE_WARN_BIT, DC_CRC32_BIT);
               }
               else
               {
                  length += sprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                    " %d %d", DC_DELETE_BIT, DC_CRC32_BIT);
               }
            }
            else if (fra[i].dup_check_flag & DC_STORE)
                 {
                    if (fra[i].dup_check_flag & DC_WARN)
                    {
                       length += sprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                         " %d %d",
                                         DC_STORE_WARN_BIT, DC_CRC32_BIT);
                    }
                    else
                    {
                       length += sprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                         " %d %d",
                                         DC_STORE_BIT, DC_CRC32_BIT);
                    }
                 }
                 else
                 {
                    length += sprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                      " %d %d",
                                      DC_WARN_BIT, DC_CRC32_BIT);
                 }
            d_o->aoptions[d_o->no_of_dir_options][length] = '\0';
            d_o->no_of_dir_options++;
         }
#endif
         if ((fra[i].protocol == FTP) || (fra[i].protocol == HTTP) ||
             (fra[i].protocol == SFTP))
         {
            (void)strcpy(d_o->url, fra[i].url);
         }
         break;
      }
   }

   if (attached == YES)
   {
      fra_detach();
   }

   return;
}

/*
 *  format_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997, 1998 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   format_info - puts data from a structure into a human readable
 **                 form
 **
 ** SYNOPSIS
 **   void format_info(char *text)
 **
 ** DESCRIPTION
 **   This function formats data from the global structure info_data
 **   to the following form:
 **         Local name : xxxxxxx.xx
 **         Remote name: XXXyyyy.ZZ
 **         Directory  : /aaa/bbb/ccc
 **         Filter     : filter_1
 **                      filter_2
 **                      filter_n
 **         Recipient  : ftp://donald:secret@hollywood//home/user
 **         AMG-options: option_1
 **                      option_2
 **                      option_n
 **         FD-options : option_1
 **                      option_2
 **                      option_n
 **         Priority   : 5
 **         Job-ID     : 4323121
 **         Archive dir: hollywood/donald/0/p5_862868443_0491
 **
 ** RETURN VALUES
 **   Returns the formated text.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.05.1997 H.Kiehl Created
 **   15.01.1998 H.Kiehl Support for remote file name.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include "show_olog.h"

/* External global variables */
extern int              max_x,
                        max_y;
extern struct info_data id;
extern struct sol_perm  perm;


/*############################ format_info() ############################*/
void
format_info(char *text)
{
   int i,
       count,
       length;

   max_x = sprintf(text, "Local name : %s\n", id.local_file_name);
   length = max_x;
   max_y = 1;
   if (id.remote_file_name[0] != '\0')
   {
      count = sprintf(text + length, "Remote name: %s\n", id.remote_file_name);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }

   if (id.no_of_files > 0)
   {
      count = sprintf(text + length, "Directory  : %s\n", id.dir);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      count = sprintf(text + length, "Filter     : %s\n", id.files[0]);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y += 2;
      for (i = 1; i < id.no_of_files; i++)
      {
         count = sprintf(text + length, "             %s\n", id.files[i]);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
      }

      /* Print recipient */
      if (perm.view_passwd != YES)
      {
         char *ptr = id.recipient;

         /*
          * The user may NOT see the password. Lets cut it out and
          * replace it with DEFAULT_VIEW_PASSWD.
          */
         while (*ptr != ':')
         {
            ptr++;
         }
         ptr++;
         while ((*ptr != ':') && (*ptr != '@'))
         {
            if (*ptr == '\\')
            {
               ptr++;
            }
            ptr++;
         }
         if (*ptr == ':')
         {
            char *p_end = ptr + 1,
                 tmp_buffer[MAX_RECIPIENT_LENGTH];

            ptr++;
            while (*ptr != '@')
            {
               if (*ptr == '\\')
               {
                  ptr++;
               }
               ptr++;
            }
            (void)strcpy(tmp_buffer, ptr);
            *p_end = '\0';
            (void)strcat(id.recipient, "XXXXX");
            (void)strcat(id.recipient, tmp_buffer);
         }
      }
      count = sprintf(text + length, "Recipient  : %s\n", id.recipient);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
      if (id.no_of_loptions > 0)
      {
         count = sprintf(text + length, "AMG-options: %s\n", id.loptions[0]);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         for (i = 1; i < id.no_of_loptions; i++)
         {
            count = sprintf(text + length, "             %s\n", id.loptions[i]);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
         }
      }
      if (id.no_of_soptions == 1)
      {
         count = sprintf(text + length, "FD-options : %s\n", id.soptions);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
      }
      else if (id.no_of_soptions > 1)
           {
              int  first = YES;
              char *p_start,
                   *p_end;

              p_start = p_end = id.soptions;
              do
              {
                 while ((*p_end != '\n') && (*p_end != '\0'))
                 {
                    p_end++;
                 }
                 if (*p_end == '\n')
                 {
                    *p_end = '\0';
                    if (first == YES)
                    {
                       first = NO;
                       count = sprintf(text + length, "FD-options : %s\n", p_start);
                    }
                    else
                    {
                       count = sprintf(text + length, "             %s\n", p_start);
                    }
                    length += count;
                    if (count > max_x)
                    {
                       max_x = count;
                    }
                    max_y++;
                    p_end++;
                    p_start = p_end;
                 }
              } while (*p_end != '\0');
              count = sprintf(text + length, "             %s\n", p_start);
              length += count;
              if (count > max_x)
              {
                 max_x = count;
              }
              max_y++;
           }
      count = sprintf(text + length, "Priority   : %c\n", id.priority);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   } /* if (id.no_of_files > 0) */

   count = sprintf(text + length, "Job-ID     : %u", id.job_no);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;

   /* Show archive directory if it is available. */
   if (id.archive_dir[0] != '\0')
   {
      count = sprintf(text + length, "\nArchive dir: %s", id.archive_dir);
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }

   return;
}

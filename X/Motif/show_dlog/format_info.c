/*
 *  format_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   void format_output_info(char *text)
 **   void format_input_info(char *text)
 **
 ** DESCRIPTION
 **   The function format_output_info() formats data from the global
 **   structure info_data to the following form:
 **         File name  : xxxxxxx.xx
 **         Proc/User  : sf_ftp
 **         Add. reason: > 50 min
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
 **
 **    format_input_info() does it slightly differently:
 **         File name  : xxxxxxx.xx
 **         Proc/User  : sf_ftp
 **         Add. reason: > 50 min
 **         Dir-ID     : 4323121
 **         Directory  : /aaa/bbb/ccc
 **         =====================================================
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
 **         -----------------------------------------------------
 **                                  .
 **                                  .
 **                                 etc.
 **
 **
 ** RETURN VALUES
 **   Returns the formated text.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.02.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include <stdlib.h>                /* atoi()                             */
#include <errno.h>
#include "show_dlog.h"

/* External global variables */
extern int              max_x,
                        max_y;
extern struct info_data id;
extern struct sol_perm  perm;


/*######################## format_output_info() #########################*/
void
format_output_info(char *text)
{
   int i,
       count,
       length;

   max_x = sprintf(text, "File name  : %s\n", id.file_name);
   length = max_x;

   /* Show process/user that deleted file. */
   count = sprintf(text + length, "Proc/User  : %s\n", id.proc_user);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y = 2;

   /* Show additional reason if it is available. */
   if (id.extra_reason[0] != '\0')
   {
      if (id.extra_reason[0] == '>')
      {
         int days,
             diff_time = atoi(&id.extra_reason[1]),
             hours,
             min,
             sec;

         days = diff_time / 86400;
         hours = (diff_time % 86400) / 3600;
         min = ((diff_time % 86400) % 3600) / 60;
         sec = ((diff_time % 86400) % 3600) % 60;
         if (days > 0)
         {
            count = sprintf(text + length,
                            "Add. reason: > %d days %d hours %d min %d sec\n",
                            days, hours, min, sec);
         }
         else if (hours > 0)
              {
                 count = sprintf(text + length,
                                 "Add. reason: > %d hours %d min %d sec\n",
                                 hours, min, sec);
              }
         else if (min > 0)
              {
                 count = sprintf(text + length,
                                 "Add. reason: > %d min %d sec\n",
                                 min, sec);
              }
              else
              {
                 count = sprintf(text + length,
                                 "Add. reason: > %d sec\n",
                                 sec);
              }
      }
      else
      {
         count = sprintf(text + length, "Add. reason: %s\n", id.extra_reason);
      }
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }

   if ((id.dbe != NULL) && (id.dbe[0].no_of_files > 0))
   {
      count = sprintf(text + length, "Directory  : %s\n", id.dir);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      count = sprintf(text + length, "Filter     : %s\n", id.dbe[0].files[0]);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y += 2;
      for (i = 1; i < id.dbe[0].no_of_files; i++)
      {
         count = sprintf(text + length, "             %s\n", id.dbe[0].files[i]);
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
         remove_passwd(id.dbe[0].recipient);
      }
      count = sprintf(text + length, "Recipient  : %s\n", id.dbe[0].recipient);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
      if (id.dbe[0].no_of_loptions > 0)
      {
         count = sprintf(text + length, "AMG-options: %s\n", id.dbe[0].loptions[0]);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         for (i = 1; i < id.dbe[0].no_of_loptions; i++)
         {
            count = sprintf(text + length, "             %s\n", id.dbe[0].loptions[i]);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
         }
      }
      if (id.dbe[0].no_of_soptions == 1)
      {
         count = sprintf(text + length, "FD-options : %s\n", id.dbe[0].soptions);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
      }
      else if (id.dbe[0].no_of_soptions > 1)
           {
              int  first = YES;
              char *p_start,
                   *p_end;

              p_start = p_end = id.dbe[0].soptions;
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
      count = sprintf(text + length, "Priority   : %c\n", id.dbe[0].priority);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   } /* if (id.dbe[0].no_of_files > 0) */

   count = sprintf(text + length, "Job-ID     : %u", id.job_no);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;

   return;
}


/*######################### format_input_info() #########################*/
void
format_input_info(char *text)
{
   int  count,
        length;
   char *p_begin_underline = NULL,
        **p_array = NULL;

   if (id.count > 0)
   {
      if ((p_array = (char **)calloc(id.count, sizeof(char *))) == NULL)
      {
         (void)fprintf(stderr, "calloc() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   max_x = sprintf(text, "File name  : %s\n", id.file_name);
   length = max_x;

   /* Show process/user that deleted file. */
   count = sprintf(text + length, "Proc/User  : %s\n", id.proc_user);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y = 2;

   /* Show additional reason if it is available. */
   if (id.extra_reason[0] != '\0')
   {
      if (id.extra_reason[0] == '>')
      {
         int days,
             diff_time = atoi(&id.extra_reason[1]),
             hours,
             min,
             sec;

         days = diff_time / 86400;
         hours = (diff_time % 86400) / 3600;
         min = ((diff_time % 86400) % 3600) / 60;
         sec = ((diff_time % 86400) % 3600) % 60;
         if (days > 0)
         {
            count = sprintf(text + length,
                            "Add. reason: > %d days %d hours %d min %d sec\n",
                            days, hours, min, sec);
         }
         else if (hours > 0)
              {
                 count = sprintf(text + length,
                                 "Add. reason: > %d hours %d min %d sec\n",
                                 hours, min, sec);
              }
         else if (min > 0)
              {
                 count = sprintf(text + length,
                                 "Add. reason: > %d min %d sec\n",
                                 min, sec);
              }
              else
              {
                 count = sprintf(text + length,
                                 "Add. reason: > %d sec\n",
                                 sec);
              }
      }
      else
      {
         count = sprintf(text + length, "Add. reason: %s\n", id.extra_reason);
      }
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }
   count = sprintf(text + length, "Dir_ID     : %u\n", id.job_no);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;

   if (id.dir[0] != '\0')
   {
      int i,
          j;

      count = sprintf(text + length, "Directory  : %s\n", id.dir);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
      p_begin_underline = text + length;

      for (j = 0; j < id.count; j++)
      {
         count = sprintf(text + length, "Filter     : %s\n", id.dbe[j].files[0]);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         for (i = 1; i < id.dbe[j].no_of_files; i++)
         {
            count = sprintf(text + length, "             %s\n", id.dbe[j].files[i]);
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
            remove_passwd(id.dbe[j].recipient);
         }
         count = sprintf(text + length, "Recipient  : %s\n", id.dbe[j].recipient);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         if (id.dbe[j].no_of_loptions > 0)
         {
            count = sprintf(text + length, "AMG-options: %s\n", id.dbe[j].loptions[0]);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
            for (i = 1; i < id.dbe[j].no_of_loptions; i++)
            {
               count = sprintf(text + length, "             %s\n", id.dbe[j].loptions[i]);
               length += count;
               if (count > max_x)
               {
                  max_x = count;
               }
               max_y++;
            }
         }
         if (id.dbe[j].no_of_soptions == 1)
         {
            count = sprintf(text + length, "FD-options : %s\n", id.dbe[j].soptions);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
         }
         else if (id.dbe[j].no_of_soptions > 1)
              {
                 int  first = YES;
                 char *p_start,
                      *p_end;

                 p_start = p_end = id.dbe[j].soptions;
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
         count = sprintf(text + length, "Priority   : %c\n", id.dbe[j].priority);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;

         p_array[j] = text + length;
      } /* for (j = 0; j < id.count; j++) */
      *(text + length - 1) = '\0';
   } /* if (id.dir[0] != '\0') */

   if (id.count > 0)
   {
      int i,
          j;

      (void)memmove(p_begin_underline + max_x + 1, p_begin_underline, (length - (p_begin_underline - text) + 1));
      (void)memset(p_begin_underline, '=', max_x);
      *(p_begin_underline + max_x) = '\n';
      max_y++;

      for (i = 0; i < (id.count - 1); i++)
      {
         for (j = i; j < (id.count - 1); j++)
         {
            p_array[j] += (max_x + 1);
         }
         length += (max_x + 1);
         (void)memmove(p_array[i] + max_x + 1, p_array[i],
                       (length - (p_array[i] - text) + 1));
         (void)memset(p_array[i], '-', max_x);
         *(p_array[i] + max_x) = '\n';
         max_y++;
      }
   }

   if (id.count > 0)
   {
      free((void *)p_array);
   }

   return;
}

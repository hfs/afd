/*
 *  format_info.c - Part of AFD, an automatic file distribution program.
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
 **   format_info - puts data from a structure into a human readable
 **                 form
 **
 ** SYNOPSIS
 **   void format_output_info(char *text, int pos)
 **   void format_input_info(char *text, int pos)
 **
 ** DESCRIPTION
 **   The function format_output_info() formats data from the various
 **   structures to the following form:
 **         File name  : xxxxxxx.xx
 **         Msg dir    : 3_991243800_118
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
 **         Hostname   : esoc
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
 **   29.07.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include <stdlib.h>                /* atoi()                             */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include "show_queue.h"

/* External global variables */
extern Widget                  toplevel_w;
extern int                     max_x,
                               max_y;
extern char                    *p_work_dir;
extern struct sol_perm         perm;
extern struct queued_file_list *qfl;


/*######################## format_output_info() #########################*/
void
format_output_info(char *text, int pos)
{
   int i,
       count,
       length;

   max_x = sprintf(text, "File name  : %s\n", qfl[pos].file_name);
   length = max_x;

   /* Show message name. */
   if (qfl[pos].in_error_dir == YES)
   {
      count = sprintf(text + length, "Msg dir    : %s%s%s/%s\n",
                      p_work_dir, AFD_FILE_DIR, ERROR_DIR, qfl[pos].msg_name);
   }
   else
   {
      count = sprintf(text + length, "Msg name   : %s%s/%s\n",
                      p_work_dir, AFD_FILE_DIR, qfl[pos].msg_name);
   }
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y = 2;

   if (qfl[pos].job_id != -1)
   {
      int                fd,
                         jd_pos = -1,
                         no_of_jobs;
      off_t              jd_size;
      char               fullname[MAX_PATH_LENGTH];
      struct stat        stat_buf;
      struct job_id_data *jd;

      /* Map to job ID data file. */
      (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR,
                    JOB_ID_DATA_FILE);
      if ((fd = open(fullname, O_RDONLY)) == -1)             
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to open() %s : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         return;
      }
      if (fstat(fd, &stat_buf) == -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to fstat() %s : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
      if (stat_buf.st_size > 0)
      {
         char *ptr;

         if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                         MAP_SHARED, fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(toplevel_w, ERROR_DIALOG,
                       "Failed to mmap() to %s : %s (%s %d)",
                       fullname, strerror(errno), __FILE__, __LINE__);
            (void)close(fd);
            return;
         }
         jd_size = stat_buf.st_size;
         no_of_jobs = *(int *)ptr;
         ptr += AFD_WORD_OFFSET;
         jd = (struct job_id_data *)ptr;
         (void)close(fd);
      }
      else
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Job ID database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
      for (i = 0; i < no_of_jobs; i++)
      {
         if (qfl[pos].job_id == jd[i].job_id)
         {
            jd_pos = i;
            break;
         }
      }
      if (jd_pos != -1)
      {
         off_t               dnb_size;
         char                *p_file_list,
                             recipient[MAX_RECIPIENT_LENGTH];
         struct dir_name_buf *dnb;

         /* Map to directory name buffer. */
         (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR,
                       DIR_NAME_FILE);
         if ((fd = open(fullname, O_RDONLY)) == -1)
         {
            (void)xrec(toplevel_w, ERROR_DIALOG,
                       "Failed to open() <%s> : %s (%s %d)",
                       fullname, strerror(errno), __FILE__, __LINE__);
            return;
         }
         if (fstat(fd, &stat_buf) == -1)
         {
            (void)xrec(toplevel_w, ERROR_DIALOG,
                       "Failed to fstat() <%s> : %s (%s %d)",
                       fullname, strerror(errno), __FILE__, __LINE__);
            (void)close(fd);
            return;
         }
         if (stat_buf.st_size > 0)
         {
            char *ptr;

            if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                            MAP_SHARED, fd, 0)) == (caddr_t) -1)
            {
               (void)xrec(toplevel_w, ERROR_DIALOG,
                          "Failed to mmap() to <%s> : %s (%s %d)",
                          fullname, strerror(errno), __FILE__, __LINE__);
               (void)close(fd);
               return;
            }
            dnb_size = stat_buf.st_size;
            ptr += AFD_WORD_OFFSET;
            dnb = (struct dir_name_buf *)ptr;
            (void)close(fd);
         }
         else
         {
            (void)xrec(toplevel_w, ERROR_DIALOG,
                       "Dirname database file is empty. (%s %d)",
                       __FILE__, __LINE__);
            (void)close(fd);
            return;
         }

         count = sprintf(text + length, "Directory  : %s\n",
                         dnb[qfl[pos].dir_id_pos].dir_name);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         p_file_list = jd[jd_pos].file_list;
         count = sprintf(text + length, "Filter     : %s\n", p_file_list);
         length += count;
         NEXT(p_file_list);
         if (count > max_x)
         {
            max_x = count;
         }
         max_y += 2;
         for (i = 1; i < jd[jd_pos].no_of_files; i++)
         {
            count = sprintf(text + length, "             %s\n", p_file_list);
            length += count;
            NEXT(p_file_list);
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
         }

         /* Print recipient */
         (void)strcpy(recipient, jd[jd_pos].recipient);
         if (perm.view_passwd != YES)
         {
            remove_passwd(recipient);
         }
         count = sprintf(text + length, "Recipient  : %s\n", recipient);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         if (jd[jd_pos].no_of_loptions > 0)
         {
            char *p_tmp;

            p_tmp = jd[jd_pos].loptions;
            count = sprintf(text + length, "AMG-options: %s\n", p_tmp);
            NEXT(p_tmp);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
            for (i = 1; i < jd[jd_pos].no_of_loptions; i++)
            {
               count = sprintf(text + length, "             %s\n", p_tmp);
               NEXT(p_tmp);
               length += count;
               if (count > max_x)
               {
                  max_x = count;
               }
               max_y++;
            }
         }
         if (jd[jd_pos].no_of_soptions == 1)
         {
            count = sprintf(text + length, "FD-options : %s\n", jd[jd_pos].soptions);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
         }
         else if (jd[jd_pos].no_of_soptions > 1)
              {
                 int  first = YES;
                 char *p_start,
                      *p_end,
                      *p_soptions;

                 if ((p_soptions = malloc(MAX_OPTION_LENGTH)) == NULL)
                 {
                    (void)xrec(toplevel_w, ERROR_DIALOG,
                               "malloc() erro : %s (%s %d)",
                               strerror(errno), __FILE__, __LINE__);
                    (void)close(fd);
                    return;
                 }
                 (void)memcpy(p_soptions, jd[jd_pos].soptions, MAX_OPTION_LENGTH);
                 p_start = p_end = p_soptions;
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
                 free(p_soptions);
              }
         count = sprintf(text + length, "Priority   : %c\n", jd[jd_pos].priority);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;

         if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) == -1)
         {
            (void)xrec(toplevel_w, INFO_DIALOG,
                       "munmap() error : %s (%s %d)",
                       strerror(errno), __FILE__, __LINE__);
         }
      }
      if (munmap(((char *)jd - AFD_WORD_OFFSET), jd_size) == -1)
      {
         (void)xrec(toplevel_w, INFO_DIALOG,
                    "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
   }

   count = sprintf(text + length, "Job-ID     : %u", qfl[pos].job_id);
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
format_input_info(char *text, int pos)
{
   int                 count,
                       fd,
                       jobs_found = 0,
                       length,
                       no_of_dirs;
   off_t               dnb_size;
   char                fullname[MAX_PATH_LENGTH],
                       *p_begin_underline = NULL,
                       **p_array = NULL;
   struct stat         stat_buf;
   struct dir_name_buf *dnb;

   max_x = sprintf(text, "File name  : %s\n", qfl[pos].file_name);
   length = max_x;

   /* Show hostname. */
   count = sprintf(text + length, "Hostname   : %s\n", qfl[pos].hostname);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y = 2;

   /* Map to directory name buffer. */
   (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR,
                 DIR_NAME_FILE);
   if ((fd = open(fullname, O_RDONLY)) == -1)
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Failed to open() <%s> : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      return;
   }
   if (fstat(fd, &stat_buf) == -1)
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Failed to fstat() <%s> : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return;
   }
   if (stat_buf.st_size > 0)
   {
      char *ptr;

      if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                      MAP_SHARED, fd, 0)) == (caddr_t) -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to mmap() to <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
      dnb_size = stat_buf.st_size;
      no_of_dirs = *(int *)ptr;
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
      (void)close(fd);
   }
   else
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Dirname database file is empty. (%s %d)",
                 __FILE__, __LINE__);
      (void)close(fd);
      return;
   }

   count = sprintf(text + length, "Dir_ID     : %u\n",
                   dnb[qfl[pos].dir_id_pos].dir_id);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;

   if (dnb[qfl[pos].dir_id_pos].dir_name[0] != '\0')
   {
      int                i,
                         no_of_jobs;
      off_t              jd_size;
      struct job_id_data *jd;

      count = sprintf(text + length, "Directory  : %s\n",
                      dnb[qfl[pos].dir_id_pos].dir_name);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
      p_begin_underline = text + length;

      /* Map to job ID data file. */
      (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR,
                    JOB_ID_DATA_FILE);
      if ((fd = open(fullname, O_RDONLY)) == -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to open() %s : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         return;
      }
      if (fstat(fd, &stat_buf) == -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to fstat() %s : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
      if (stat_buf.st_size > 0)
      {
         char *ptr;

         if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                         MAP_SHARED, fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(toplevel_w, ERROR_DIALOG,
                       "Failed to mmap() to %s : %s (%s %d)",
                       fullname, strerror(errno), __FILE__, __LINE__);
            (void)close(fd);
            return;
         }
         jd_size = stat_buf.st_size;
         no_of_jobs = *(int *)ptr;
         ptr += AFD_WORD_OFFSET;
         jd = (struct job_id_data *)ptr;
         (void)close(fd);
      }
      else
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Job ID database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(fd);
         return;
      }

      for (i = 0; i < no_of_jobs; i++)
      {
         if ((qfl[pos].dir_id_pos == jd[i].dir_id_pos) &&
             ((qfl[pos].hostname[0] == '\0') ||
              ((qfl[pos].hostname[0] != '\0') &&
               (strcmp(qfl[pos].hostname, jd[i].host_alias) == 0))))
         {
            int  gotcha = NO,
                 j;
            char *p_file;

            p_file = jd[i].file_list;
            for (j = 0; j < jd[i].no_of_files; j++)
            {
               if (sfilter(p_file, qfl[pos].file_name) == 0)
               {
                  gotcha = YES;
                  break;
               }
               NEXT(p_file);
            }

            if (gotcha == YES)
            {
               char *p_file_list,
                    recipient[MAX_RECIPIENT_LENGTH];

               if (jobs_found == 0)
               {
                  if ((p_array = (char **)malloc(1 * sizeof(char *))) == NULL)
                  {
                     (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
               }
               else
               {
                  if ((p_array = (char **)realloc(p_array,
                                                  (jobs_found * sizeof(char *)))) == NULL)
                  {
                     (void)fprintf(stderr, "realloc() error : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
               }
               p_file_list = jd[i].file_list;
               count = sprintf(text + length, "Filter     : %s\n", p_file_list);
               NEXT(p_file_list);
               length += count;
               if (count > max_x)
               {
                  max_x = count;
               }
               max_y++;
               for (j = 1; j < jd[i].no_of_files; j++)
               {
                  count = sprintf(text + length, "             %s\n", p_file_list);
                  NEXT(p_file_list);
                  length += count;
                  if (count > max_x)
                  {
                     max_x = count;
                  }
                  max_y++;
               }

               /* Print recipient */
               (void)strcpy(recipient, jd[i].recipient);
               if (perm.view_passwd != YES)
               {
                  remove_passwd(recipient);
               }
               count = sprintf(text + length, "Recipient  : %s\n", recipient);
               length += count;
               if (count > max_x)
               {
                  max_x = count;
               }
               max_y++;
               if (jd[i].no_of_loptions > 0)
               {
                  char *p_tmp;

                  p_tmp = jd[i].loptions;
                  count = sprintf(text + length, "AMG-options: %s\n", p_tmp);
                  NEXT(p_tmp);
                  length += count;
                  if (count > max_x)
                  {
                     max_x = count;
                  }
                  max_y++;
                  for (j = 1; j < jd[i].no_of_loptions; j++)
                  {
                     count = sprintf(text + length, "             %s\n", p_tmp);
                     NEXT(p_tmp);
                     length += count;
                     if (count > max_x)
                     {
                        max_x = count;
                     }
                     max_y++;
                  }
               }
               if (jd[i].no_of_soptions == 1)
               {
                  count = sprintf(text + length, "FD-options : %s\n", jd[i].soptions);
                  length += count;
                  if (count > max_x)
                  {
                     max_x = count;
                  }
                  max_y++;
               }
               else if (jd[i].no_of_soptions > 1)
                    {
                       int  first = YES;
                       char *p_start,
                            *p_end,
                            *p_soptions;

                       if ((p_soptions = malloc(MAX_OPTION_LENGTH)) == NULL)
                       {
                          (void)xrec(toplevel_w, ERROR_DIALOG,
                                     "malloc() erro : %s (%s %d)",
                                     strerror(errno), __FILE__, __LINE__);
                          (void)close(fd);
                          return;
                       }
                       (void)memcpy(p_soptions, jd[i].soptions, MAX_OPTION_LENGTH);
                       p_start = p_end = p_soptions;
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
                       free(p_soptions);
                    }
               count = sprintf(text + length, "Priority   : %c\n", jd[i].priority);
               length += count;
               if (count > max_x)
               {
                  max_x = count;
               }
               max_y++;

               p_array[jobs_found] = text + length;
               jobs_found++;
            }
         }
      }
      if (munmap(((char *)jd - AFD_WORD_OFFSET), jd_size) == -1)
      {
         (void)xrec(toplevel_w, INFO_DIALOG,
                    "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
      *(text + length - 1) = '\0';
   }
   if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) == -1)
   {
      (void)xrec(toplevel_w, INFO_DIALOG,
                 "munmap() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
   }

   if (jobs_found > 0)
   {
      int i,
          j;

      (void)memmove(p_begin_underline + max_x + 1, p_begin_underline, (length - (p_begin_underline - text) + 1));
      (void)memset(p_begin_underline, '=', max_x);
      *(p_begin_underline + max_x) = '\n';
      max_y++;

      for (i = 0; i < (jobs_found - 1); i++)
      {
         for (j = i; j < (jobs_found - 1); j++)
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

   if (jobs_found > 0)
   {
      free((void *)p_array);
   }

   return;
}

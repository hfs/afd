/*
 *  get_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001, 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_data - searches the AFD queues for files
 **
 ** SYNOPSIS
 **   void get_data(void)
 **
 ** DESCRIPTION
 **   This function searches for the selected data in the
 **   queues of the AFD. The following things can be selected:
 **   start & end time, file name, file length, directory and
 **   recipient. Only selected data will be shown in the list
 **   widget.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.07.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>        /* sprintf()                                   */
#include <string.h>       /* strerror()                                  */
#include <stdlib.h>       /* malloc(), realloc(), free(), strtod(), abs()*/
#include <time.h>         /* time()                                      */
#include <unistd.h>       /* close()                                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>       /* opendir(), closedir(), readdir()            */
#ifndef _NO_MMAP
#include <sys/mman.h>     /* mmap(), munmap()                            */
#ifndef MAP_FILE          /* Required for BSD          */
#define MAP_FILE 0        /* All others do not need it */
#endif
#endif
#include <errno.h>

#include <Xm/Xm.h>
#include <Xm/List.h>
#include <Xm/Text.h>
#include "x_common_defs.h"
#include "show_queue.h"
#include "fddefs.h"

#define _WITH_HEAPSORT

/* External global variables */
extern Display                 *display;
extern Widget                  listbox_w,
                               scrollbar_w,
                               statusbox_w,
                               special_button_w,
                               summarybox_w,
                               toplevel_w;
extern int                     toggles_set,
                               radio_set,
                               gt_lt_sign,
                               no_of_search_hosts,
                               special_button_flag,
                               file_name_length;
extern unsigned int            total_no_files;
extern size_t                  search_file_size;
extern time_t                  start_time_val,
                               end_time_val;
extern double                  total_file_size;
extern char                    *p_work_dir,
                               search_file_name[],
                               search_directory_name[],
                               **search_recipient,
                               **search_user,
                               summary_str[],
                               total_summary_str[];
extern struct sol_perm         perm;
extern struct queued_file_list *qfl;

/* Local global variables. */
static int                     no_of_dirs,
                               no_of_jobs;
static char                    limit_reached;
static struct dir_name_buf     *dnb;
static struct job_id_data      *jd;

/* Local function prototypes */
static void                    get_all_input_files(void),
                               get_input_files(void),
                               get_output_files(void),
                               insert_file(char *, char *, char *, char *,
                                           char, char, int, int),
                               searching(char *),
#ifdef _WITH_HEAPSORT
                               sort_data(int);
#else
                               sort_data(int, int);
#endif
static int                     get_job_id(char *),
                               get_pos(int);


/*############################### get_data() ############################*/
void
get_data(void)
{
   int         fd;
   off_t       dnb_size,
               jd_size;
   char        fullname[MAX_PATH_LENGTH],
               status_message[MAX_MESSAGE_LENGTH];
   struct stat stat_buf;
   XmString    xstr;

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
   if ((toggles_set & SHOW_OUTPUT) || (toggles_set & SHOW_UNSENT_INPUT) ||
       (toggles_set & SHOW_UNSENT_OUTPUT))
   {
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
   }

   special_button_flag = STOP_BUTTON;
   xstr = XmStringCreateLtoR("Stop", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   (void)memset(summary_str, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);
   summary_str[MAX_OUTPUT_LINE_LENGTH + file_name_length] = '\0';
   XmTextSetString(summarybox_w, summary_str);
   CHECK_INTERRUPT();

   limit_reached = NO;
   total_file_size = 0.0;
   total_no_files = 0;
   if ((toggles_set & SHOW_OUTPUT) || (toggles_set & SHOW_UNSENT_OUTPUT))
   {
      get_output_files();
   }
   if (toggles_set & SHOW_INPUT)
   {
      get_input_files();
   }
   if (toggles_set & SHOW_UNSENT_INPUT)
   {
      get_all_input_files();
   }

   if (total_no_files != 0)
   {
      (void)strcpy(status_message, "Sorting...");
      SHOW_MESSAGE();
#ifdef _WITH_HEAPSORT
      sort_data(total_no_files);
#else
      sort_data(0, total_no_files - 1);
#endif
      (void)strcpy(status_message, "Displaying...");
      SHOW_MESSAGE();
      display_data();
      (void)strcpy(status_message, " ");
      SHOW_MESSAGE();
   }
   else
   {
      (void)strcpy(status_message, "No data found.");
      SHOW_MESSAGE();
   }

   if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) == -1)
   {
      (void)xrec(toplevel_w, INFO_DIALOG,
                 "munmap() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
   }
   if ((toggles_set & SHOW_OUTPUT) || (toggles_set & SHOW_UNSENT_INPUT) ||
       (toggles_set & SHOW_UNSENT_OUTPUT))
   {
      if (munmap(((char *)jd - AFD_WORD_OFFSET), jd_size) == -1)
      {
         (void)xrec(toplevel_w, INFO_DIALOG,
                    "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
   }
   show_summary(total_no_files, total_file_size);

   special_button_flag = SEARCH_BUTTON;
   xstr = XmStringCreateLtoR("Search", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   return;
}


/*+++++++++++++++++++++++++++ get_output_files() ++++++++++++++++++++++++*/
static void
get_output_files(void)
{
   int fd;
   char fullname[MAX_PATH_LENGTH];

   (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR, MSG_QUEUE_FILE);
   if ((fd = open(fullname, O_RDWR)) == -1)
   {
      (void)xrec(toplevel_w, FATAL_DIALOG,
                 "Failed to open() <%s> : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      struct stat stat_buf;

      if (fstat(fd, &stat_buf) == -1)
      {
         (void)xrec(toplevel_w, FATAL_DIALOG,
                    "Failed to fstat() <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         if (stat_buf.st_size > 0)
         {
            char *buffer;

            if ((buffer = malloc(stat_buf.st_size)) == NULL)
            {
               (void)xrec(toplevel_w, FATAL_DIALOG,
                          "malloc() error : %s (%s %d)",
                          strerror(errno), __FILE__, __LINE__);
            }
            else
            {
               if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
               {
                  (void)xrec(toplevel_w, FATAL_DIALOG,
                             "Failed to read() <%s> : %s (%s %d)",
                             fullname, strerror(errno), __FILE__, __LINE__);
               }
               else
               {
                  register int     i;
                  int              gotcha,
                                   job_id,
                                   no_msg_queued,
                                   pos;
                  char             error_queue_dir[MAX_PATH_LENGTH],
                                   *p_error_queue_msg,
                                   *p_queue_msg,
                                   queue_dir[MAX_PATH_LENGTH];
                  struct queue_buf *qb;

                  p_queue_msg = queue_dir + sprintf(queue_dir, "%s%s/",
                                                    p_work_dir, AFD_FILE_DIR);
                  p_error_queue_msg = error_queue_dir +
                                      sprintf(error_queue_dir, "%s%s%s/",
                                              p_work_dir, AFD_FILE_DIR,
                                              ERROR_DIR);

                  no_msg_queued = *(int *)buffer;
                  buffer += AFD_WORD_OFFSET;
                  qb = (struct queue_buf *)buffer;

                  for (i = 0; ((i < no_msg_queued) && (limit_reached == NO)); i++)
                  {
                     if ((qb[i].msg_name[0] != '\0') &&
                         (((toggles_set & SHOW_OUTPUT) &&
                           ((toggles_set & SHOW_UNSENT_OUTPUT) ||
                            (qb[i].pid == PENDING))) ||
                          (((toggles_set & SHOW_OUTPUT) == 0) &&
                            (toggles_set & SHOW_UNSENT_OUTPUT) &&
                            (qb[i].pid != PENDING))) &&
                         ((job_id = get_job_id(qb[i].msg_name)) != -1) &&
                         ((pos = get_pos(job_id)) != -1))
                     {
                        if (no_of_search_hosts == 0)
                        {
                           gotcha = YES;
                        }
                        else
                        {
                           register int j;

                           gotcha = NO;
                           for (j = 0; j < no_of_search_hosts; j++)
                           {
                              if (pmatch(search_recipient[j],
                                         jd[pos].host_alias) == 0)
                              {
                                 gotcha = YES;
                                 break;
                              }
                           }
                        }

                        if (gotcha == YES)
                        {
                           char *p_queue_dir,
                                *ptr_file;

                           /* Check if an input directory was specified. */
                           if ((search_directory_name[0] == '\0') ||
                               ((search_directory_name[0] != '\0') &&
                                (pmatch(search_directory_name, dnb[jd[pos].dir_id_pos].dir_name) == 0)))
                           {
                              char queue_typ;

                              if (qb[i].in_error_dir == YES)
                              {
                                 p_queue_dir = error_queue_dir;
                                 ptr_file = p_error_queue_msg +
                                            sprintf(p_error_queue_msg,
                                                    "/%s/%s/",
                                                    jd[pos].host_alias,
                                                    qb[i].msg_name);
                              }
                              else
                              {
                                 p_queue_dir = queue_dir;
                                 ptr_file = p_queue_msg +
                                            sprintf(p_queue_msg, "/%s/",
                                                    qb[i].msg_name);
                              }
                              if (qb[i].pid == PENDING)
                              {
                                 queue_typ = SHOW_OUTPUT;
                              }
                              else
                              {
                                 queue_typ = SHOW_UNSENT_OUTPUT;
                              }
                              insert_file(p_queue_dir, ptr_file,
                                          qb[i].msg_name, jd[pos].host_alias,
                                          queue_typ, qb[i].in_error_dir,
                                          job_id, jd[pos].dir_id_pos);
                           }
                        }
                     }

                     if ((i % 100) == 0)
                     {
                        searching("Output");
                     }
                  }
               }
               free(buffer - AFD_WORD_OFFSET);
            }
         }
      }

      if (close(fd) == -1)
      {
         (void)xrec(toplevel_w, INFO_DIALOG,
                    "Failed to close() <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
      }
   }
   searching("Output");

   return;
}


/*++++++++++++++++++++++++++++ get_input_files() ++++++++++++++++++++++++*/
static void
get_input_files(void)
{
   register int  i;
   DIR           *dp;
   struct dirent *p_dir;
   struct stat   stat_buf;

   for (i = 0; ((i < no_of_dirs) && (limit_reached == NO)); i++)
   {
      if ((search_directory_name[0] == '\0') ||
          ((search_directory_name[0] != '\0') &&
           (pmatch(search_directory_name, dnb[i].dir_name) == 0)))
      {
         if ((dp = opendir(dnb[i].dir_name)) != NULL)
         {
            while (((p_dir = readdir(dp)) != NULL) && (limit_reached == NO))
            {
               if ((p_dir->d_name[0] == '.') && (p_dir->d_name[1] != '\0') &&
                   (p_dir->d_name[1] != '.') &&
                   (strlen(&p_dir->d_name[1]) <= MAX_HOSTNAME_LENGTH))
               {
                  char *p_file,
                       queue_dir[MAX_PATH_LENGTH];

                  p_file = queue_dir + sprintf(queue_dir, "%s/%s/",
                                               dnb[i].dir_name, p_dir->d_name);
                  if ((stat(queue_dir, &stat_buf) != -1) &&
                      (S_ISDIR(stat_buf.st_mode)))
                  {
                     int gotcha = NO;

                     if (no_of_search_hosts == 0)
                     {
                        gotcha = YES;
                     }
                     else
                     {
                        register int j;

                        gotcha = NO;
                        for (j = 0; j < no_of_search_hosts; j++)
                        {
                           if (pmatch(search_recipient[j],
                                      &p_dir->d_name[1]) == 0)
                           {
                              gotcha = YES;
                              break;
                           }
                        }
                     }

                     if (gotcha == YES)
                     {
                        insert_file(queue_dir, p_file, "\0",
                                    &p_dir->d_name[1], SHOW_INPUT, NO, -1, i);
                     }
                  }
               }
            }
            if (closedir(dp) == -1)
            {
               (void)xrec(toplevel_w, INFO_DIALOG,
                          "Failed to closedir() <%s> : %s (%s %d)",
                          dnb[i].dir_name, strerror(errno), __FILE__, __LINE__);
            }
         }
      }
      searching("Input");
   } /* for (i = 0; i < no_of_dirs; i++) */
   searching("Input");

   return;
}


/*++++++++++++++++++++++++++ get_all_input_files() ++++++++++++++++++++++*/
static void
get_all_input_files(void)
{
   register int i;

   for (i = 0; ((i < no_of_dirs) && (limit_reached == NO)); i++)
   {
      if ((search_directory_name[0] == '\0') ||
          ((search_directory_name[0] != '\0') &&
           (pmatch(search_directory_name, dnb[i].dir_name) == 0)))
      {
         int gotcha = NO;

         if (no_of_search_hosts == 0)
         {
            gotcha = YES;
         }
         else
         {
            register int j, k;

            gotcha = NO;
            for (j = 0; ((j < no_of_jobs) && (gotcha == NO)); j++)
            {
               if (jd[j].dir_id_pos == i)
               {
                  for (k = 0; k < no_of_search_hosts; k++)
                  {
                     if (pmatch(search_recipient[k],
                                jd[j].host_alias) == 0)
                     {
                        gotcha = YES;
                        break;
                     }
                  }
               }
            }
         }
         if (gotcha == YES)
         {
            char input_dir[MAX_PATH_LENGTH],
                 *ptr_file;

            ptr_file = input_dir + sprintf(input_dir, "%s/",
                       dnb[i].dir_name);
            insert_file(input_dir, ptr_file, "\0", "\0",
                        SHOW_UNSENT_INPUT, NO, -1, i);
         }
      }
      searching("Unsent");
   }
   searching("Unsent");

   return;
}

#ifdef _WITH_HEAPSORT
/*++++++++++++++++++++++++++++++ sort_data() ++++++++++++++++++++++++++++*/
/*                               -----------                             */
/* Description: Heapsort found in linux kernel mailing list from         */
/*              Jamie Lokier.                                            */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
sort_data(int count)
{
   int                     i, j, k;
   struct queued_file_list tmp_qfl;

   for (i = 1; i < count; i++)
   {
      j = i;
      (void)memcpy(&tmp_qfl, &qfl[j], sizeof(struct queued_file_list));
      while ((j > 0) && (tmp_qfl.mtime > qfl[(j - 1) / 2].mtime))
      {
         (void)memcpy(&qfl[j], &qfl[(j - 1) / 2], sizeof(struct queued_file_list));
         j = (j - 1) / 2;
      }
      (void)memcpy(&qfl[j], &tmp_qfl, sizeof(struct queued_file_list));
   }
   for (i = (count - 1); i > 0; i--)
   {
      j = 0;
      k = 1;
      (void)memcpy(&tmp_qfl, &qfl[i], sizeof(struct queued_file_list));
      (void)memcpy(&qfl[i], &qfl[0], sizeof(struct queued_file_list));
      while ((k < i) &&
             ((tmp_qfl.mtime < qfl[k].mtime) ||
              (((k + 1) < i) && (tmp_qfl.mtime < qfl[k + 1].mtime))))
      {
         k += (((k + 1) < i) && (qfl[k + 1].mtime > qfl[k].mtime));
         (void)memcpy(&qfl[j], &qfl[k], sizeof(struct queued_file_list));
         j = k;
         k = 2 * j + 1;
      }
      (void)memcpy(&qfl[j], &tmp_qfl, sizeof(struct queued_file_list));
   }

   return;
}
#else

/*+++++++++++++++++++++++++++++ sort_data() +++++++++++++++++++++++++++++*/
static void
sort_data(int start, int end)
{
   int                     i, j;
   time_t                  center;
   struct queued_file_list tmp_qfl;

   i = start; j = end;
   center = qfl[(start + end)/2].mtime;
   do
   {
      while ((i < end) && (qfl[i].mtime< center))
      {
         i++;
      }
      while ((j > start) && (qfl[j].mtime > center))
      {
         j--;
      }
      if (i <= j)
      {
         (void)memcpy(&tmp_qfl, &qfl[i], sizeof(struct queued_file_list));
         (void)memcpy(&qfl[i], &qfl[j], sizeof(struct queued_file_list));
         (void)memcpy(&qfl[j], &tmp_qfl, sizeof(struct queued_file_list));
         i++; j--;
      }
   } while (i <= j);
   if (start < j)
   {
      sort_data(start, j);
   }
   if (i < end)
   {
      sort_data(i, end);
   }

   return;
}
#endif


#ifdef _SORT_NOT_WORKING
/*+++++++++++++++++++++++++++++ sort_data() +++++++++++++++++++++++++++++*/
static void
sort_data(void)
{
   int                     i, j,
                           start = 0,
                           end = total_no_files - 1;
   time_t                  center;
   struct queued_file_list tmp_qfl;

   i = start; j = end;
   for (;;)
   {
      center = qfl[(start + end)/2].mtime;
      do
      {
         while ((i < end) && (qfl[i].mtime < center))
         {
            i++;
         }
         while ((j > start) && (qfl[j].mtime > center))
         {
            j--;
         }
         if (i <= j)
         {
            (void)memcpy(&tmp_qfl, &qfl[i], sizeof(struct queued_file_list));
            (void)memcpy(&qfl[i], &qfl[j], sizeof(struct queued_file_list));
            (void)memcpy(&qfl[j], &tmp_qfl, sizeof(struct queued_file_list));
            i++; j--;
         }
      } while (i <= j);

      if (start < j)
      {
         i = start;
         end = j;
      }
      else if (i < end)
           {
              start = i;
              j = end;
           }
           else
           {
              break;
           }
   }

   return;
}
#endif /* _SORT_NOT_WORKING */


/*---------------------------- insert_file() ----------------------------*/
static void
insert_file(char *queue_dir,
            char *ptr_file,
            char *msg_name,
            char *hostname,
            char queue_type,
            char in_error_dir,
            int  job_id,
            int  dir_id_pos)
{
   DIR         *dpfile;
   struct stat stat_buf;

   if ((dpfile = opendir(queue_dir)) != NULL)
   {
      struct dirent *dirp;

      while (((dirp = readdir(dpfile)) != NULL) && (limit_reached == NO))
      {
         if (dirp->d_name[0] != '.')
         {
            /* Check if we need to search for a specific file. */
            if ((search_file_name[0] == '\0') ||
                ((search_file_name[0] != '\0') &&
                 (pmatch(search_file_name, dirp->d_name) == 0)))
            {
               (void)strcpy(ptr_file, dirp->d_name);
               if ((stat(queue_dir, &stat_buf) != -1) &&
                   (!S_ISDIR(stat_buf.st_mode)))
               {
                  /* If necessary check if its in the time span. */
                  if (((start_time_val == -1) ||
                       (stat_buf.st_mtime >= start_time_val)) &&
                      ((end_time_val == -1) ||
                       (stat_buf.st_mtime <= end_time_val)))
                  {
                     /* If necessary check the file size. */
                     if ((search_file_size == -1) ||
                         ((search_file_size != -1) &&
                          (((gt_lt_sign == GREATER_THEN_SIGN) &&
                            (stat_buf.st_size > search_file_size)) ||
                           ((gt_lt_sign == LESS_THEN_SIGN) &&
                            (stat_buf.st_size < search_file_size)) ||
                           ((gt_lt_sign == EQUAL_SIGN) &&
                            (stat_buf.st_size == search_file_size)))))
                     {
                        /* Finally we got a file. */
                        if ((total_no_files % 50) == 0)
                        {
                           size_t new_size = (((total_no_files / 50) + 1) * 50) *
                                             sizeof(struct queued_file_list);

                           if (total_no_files == 0)
                           {
                              if ((qfl = (struct queued_file_list *)malloc(new_size)) == NULL)
                              {
                                 (void)xrec(toplevel_w, FATAL_DIALOG,
                                            "malloc() error : %s (%s %d)",
                                            strerror(errno), __FILE__, __LINE__);
                                 return;
                              }
                           }
                           else
                           {
                              if ((qfl = (struct queued_file_list *)realloc((char *)qfl, new_size)) == NULL)
                              {
                                 (void)xrec(toplevel_w, FATAL_DIALOG,
                                            "malloc() error : %s (%s %d)",
                                            strerror(errno), __FILE__, __LINE__);
                                 return;
                              }
                           }
                        }
                        if ((qfl[total_no_files].file_name = malloc(strlen(dirp->d_name) + 1)) == NULL)
                        {
                           (void)xrec(toplevel_w, FATAL_DIALOG,
                                      "malloc() error : %s (%s %d)",
                                      strerror(errno), __FILE__, __LINE__);
                           return;
                        }
                        qfl[total_no_files].job_id = job_id;
                        qfl[total_no_files].size = stat_buf.st_size;
                        qfl[total_no_files].mtime = stat_buf.st_mtime;
                        qfl[total_no_files].dir_id_pos = dir_id_pos;
                        qfl[total_no_files].in_error_dir = in_error_dir;
                        qfl[total_no_files].queue_type = queue_type;
                        (void)strcpy(qfl[total_no_files].hostname, hostname);
                        (void)strcpy(qfl[total_no_files].file_name, dirp->d_name);
                        (void)strcpy(qfl[total_no_files].msg_name, msg_name);
                        total_no_files++;
                        total_file_size += stat_buf.st_size;
                        if ((perm.list_limit > 0) &&
                            (total_no_files > perm.list_limit))
                        {
                           char msg_buffer[40];

                           (void)sprintf(msg_buffer,
                                         "List limit (%d) reached!",
                                         perm.list_limit);
                           show_message(statusbox_w, msg_buffer);
                           limit_reached = YES;
                        }
                     }
                  }
               }
            }
         }
      }
      if (closedir(dpfile) == -1)
      {
         *ptr_file = '\0';
         (void)xrec(toplevel_w, INFO_DIALOG,
                    "Failed to closedir() <%s> : %s (%s %d)",
                    queue_dir, strerror(errno), __FILE__, __LINE__);
      }
   }
   return;
}


/*------------------------------- get_job_id() --------------------------*/
static int
get_job_id(char *msg_name)
{
   char *ptr = msg_name;

   while (*ptr != '\0')
   {
      ptr++;
   }
   if (ptr != msg_name)
   {
      do
      {
         ptr--;
      } while ((*ptr != '_') && (ptr != msg_name));
      if (*ptr == '_')
      {
         ptr++;
         return(atoi(ptr));
      }
   }
   return(-1);
}


/*--------------------------------- get_pos() ---------------------------*/
static int
get_pos(int job_id)
{
   register int i;

   for (i = 0; i < no_of_jobs; i++)
   {
      if (job_id == jd[i].job_id)
      {
         return(i);
      }
   }
   return(-1);
}


/*------------------------------- searching() ---------------------------*/
static void
searching(char *where)
{
   static int rotate = 0;
   char       status_message[MAX_MESSAGE_LENGTH];

   if (rotate == 0)
   {
      (void)sprintf(status_message, "Searching %s -", where);
   }
   else if (rotate == 1)
        {
           (void)sprintf(status_message, "Searching %s \\", where);
        }
   else if (rotate == 2)
        {
           (void)sprintf(status_message, "Searching %s |", where);
        }
        else
        {
           (void)sprintf(status_message, "Searching %s /", where);
           rotate = -1;
        }
   rotate++;
   SHOW_MESSAGE();

   return;
}

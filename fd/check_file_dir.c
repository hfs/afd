/*
 *  check_file_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2001 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_file_dir - checks AFD file directory for jobs without
 **                    messages
 **
 ** SYNOPSIS
 **   int check_file_dir(int force_check)
 **
 ** DESCRIPTION
 **   The function check_file_dir() checks the AFD file directory for
 **   jobs without messages. When 'force_check' is set to YES it
 **   will search the file directory no matter how many jobs are
 **   currently waiting. NO will only check a directories that have
 **   less then MAX_FD_DIR_CHECK jobs.
 **
 ** RETURN VALUES
 **   Returns YES when all directories have been checked successfully
 **   otherwise NO is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.04.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>          /* strcmp(), strerror(), strcpy(), strlen() */
#include <stdlib.h>          /* atoi(), atol()                           */
#include <sys/types.h>
#include <sys/stat.h>        /* S_ISDIR(), stat()                        */
#include <dirent.h>          /* opendir(), readdir()                     */
#include <errno.h>
#include "fddefs.h"

/* External global variables */
extern int                        *no_msg_queued,
                                  qb_fd,
                                  sys_log_fd;
extern char                       file_dir[],
                                  *p_work_dir;
extern struct queue_buf           *qb;
extern struct msg_cache_buf       *mdb;
extern struct filetransfer_status *fsa;

/* Local function prototypes */
static void                       add_message_to_queue(char *, char);
static int                        check_jobs(char, int, nlink_t);


/*########################## check_file_dir() ###########################*/
int
check_file_dir(int force_check)
{
   static int    all_checked;
   char          *p_start;
   DIR           *dp = NULL;
   struct dirent *dirp;
   struct stat   stat_buf;

   if (stat(file_dir, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Failed to stat() %s : %s (%s %d)\n",
                file_dir, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   /* First check only the file directory. */
   if (check_jobs(NO, force_check, stat_buf.st_nlink) == SUCCESS)
   {
      all_checked = YES;
   }
   else
   {
      all_checked = NO;
   }

   /* Now lets check the error file directory. */
   p_start = file_dir + strlen(file_dir);
   (void)strcpy(p_start, ERROR_DIR);
   if ((dp = opendir(file_dir)) == NULL)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Failed to opendir() %s : %s (%s %d)\n",
                file_dir, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      char *ptr;

      ptr = file_dir + strlen(file_dir);
      *(ptr++) = '/';

      /* Check each host if it has error jobs. */
      errno = 0;
      while ((dirp = readdir(dp)) != NULL)
      {
         if (dirp->d_name[0] != '.')
         {
            (void)strcpy(ptr, dirp->d_name);
            if (stat(file_dir, &stat_buf) == -1)
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Failed to stat() %s : %s (%s %d)\n",
                         file_dir, strerror(errno), __FILE__, __LINE__);
            }
            else
            {
               /* Test if it is a directory. */
               if (S_ISDIR(stat_buf.st_mode))
               {
                  if (check_jobs(YES, force_check, stat_buf.st_nlink) != SUCCESS)
                  {
                     all_checked = NO;
                  }
               }
            }
         }
         errno = 0;
      }
      if (errno)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to readdir() %s : %s (%s %d)\n",
                   file_dir, strerror(errno), __FILE__, __LINE__);
      }
      if (closedir(dp) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to closedir() %s : %s (%s %d)\n",
                   file_dir, strerror(errno), __FILE__, __LINE__);
      }
      *p_start = '\0';
   }

   return(all_checked);
}


/*++++++++++++++++++++++++++++++ check_jobs() +++++++++++++++++++++++++++*/
static int
check_jobs(char in_error_dir, int force_check, nlink_t st_nlink)
{
   if ((st_nlink < MAX_FD_DIR_CHECK) || (force_check == YES))
   {
      char          *ptr;
      DIR           *dp = NULL;
      struct dirent *dirp;
      struct stat   stat_buf;

      /*
       * Check file directory for files that do not have a message.
       */
      if ((dp = opendir(file_dir)) == NULL)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Failed to opendir() %s : %s (%s %d)\n",
                   file_dir, strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
      ptr = file_dir + strlen(file_dir);
      if (*(ptr - 1) != '/')
      {
         *(ptr++) = '/';
      }

      /* Check each job if it has a corresponding message. */
      errno = 0;
      while ((dirp = readdir(dp)) != NULL)
      {
         if (dirp->d_name[0] != '.')
         {
            (void)strcpy(ptr, dirp->d_name);

            if (stat(file_dir, &stat_buf) == -1)
            {
               /*
                * Be silent when there is no such file or directory, since
                * it could be that a sf_xxx just has deleted its own job.
                */
               if (errno != ENOENT)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Failed to stat() %s : %s (%s %d)\n",
                            file_dir, strerror(errno), __FILE__, __LINE__);
               }
            }
            else
            {
               /* Test if it is a directory. */
               if (S_ISDIR(stat_buf.st_mode))
               {
                  /* Make sure this is a job. */
                  if (check_job_name(dirp->d_name) == SUCCESS)
                  {
                     int gotcha = NO,
                         i;

                     for (i = 0; i < *no_msg_queued; i++)
                     {
                        if (CHECK_STRCMP(qb[i].msg_name, dirp->d_name) == 0)
                        {
                           gotcha = YES;
                           break;
                        }
                     }
                     if (gotcha == NO)
                     {
                        /*
                         * Message is NOT in queue. Add message to queue.
                         */
                        (void)rec(sys_log_fd, WARN_SIGN,
                                  "Message %s not in queue, adding message. (%s %d)\n",
                                  dirp->d_name, __FILE__, __LINE__);
                        add_message_to_queue(dirp->d_name, in_error_dir);
                     }
                  }
               }
            }
         }
         errno = 0;
      } /* while ((dirp = readdir(dp)) != NULL) */

      *ptr = '\0';
      if ((errno) && (errno != ENOENT))
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to readdir() %s : %s (%s %d)\n",
                   file_dir, strerror(errno), __FILE__, __LINE__);
      }
      if (closedir(dp) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to closedir() %s : %s (%s %d)\n",
                   file_dir, strerror(errno), __FILE__, __LINE__);
      }
   } /* if ((st_nlink < MAX_FD_DIR_CHECK) || (force_check == YES)) */
   else
   {
      return(INCORRECT);
   }

   return(SUCCESS);
}


/*------------------------ add_message_to_queue() -----------------------*/
static void
add_message_to_queue(char *dir_name, char in_error_dir)
{
   int    job_id,
          pos,
          unique_number;
   time_t creation_time;
   char   msg_name[MAX_MSG_NAME_LENGTH],
          msg_priority,
          *p_start,
          *ptr;

   check_queue_space();

   /*
    * Retrieve priority, creation time, unique number and job ID
    * from the message name.
    */
   (void)strcpy(msg_name, dir_name);
   ptr = msg_name;
   msg_priority = *ptr;
   ptr += 2;
   p_start = ptr;
   while (*ptr != '_')
   {
      ptr++;
   }
   *ptr = '\0';
   creation_time = atol(p_start);
   ptr++;
   p_start = ptr;
   while (*ptr != '_')
   {
      ptr++;
   }
   *ptr = '\0';
   unique_number = atoi(p_start);
   ptr++;
   p_start = ptr;
   while (*ptr != '\0')
   {
      ptr++;
   }
   job_id = atoi(p_start);

   if ((pos = lookup_job_id(job_id)) == INCORRECT)
   {
      char del_dir[MAX_PATH_LENGTH];

      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not locate job %d (%s %d)\n",
                job_id, __FILE__, __LINE__);
      (void)sprintf(del_dir, "%s%s/%s", p_work_dir,
                    AFD_FILE_DIR, dir_name);
#ifdef _DELETE_LOG
      remove_files(del_dir, -1, job_id, OTHER_DEL);
#else
      remove_files(del_dir, -1);
#endif
   }
   else
   {
      int    i,
             qb_pos = *no_msg_queued;
      double msg_number = ((double)msg_priority - 47.0) *
                          (((double)creation_time * 10000.0) +
                          (double)unique_number);

      for (i = 0; i < *no_msg_queued; i++)
      {
         if (msg_number < qb[i].msg_number)
         {
            size_t move_size;

            move_size = (*no_msg_queued - i) *
                        sizeof(struct queue_buf);
            (void)memmove(&qb[i + 1], &qb[i], move_size);
            qb_pos = i;
            break;
         }
      }
      (void)strcpy(qb[qb_pos].msg_name, dir_name);
      qb[qb_pos].msg_number = msg_number;
      qb[qb_pos].pid = PENDING;
      qb[qb_pos].creation_time = creation_time;
      qb[qb_pos].pos = pos;
      qb[qb_pos].connect_pos = -1;
      qb[qb_pos].in_error_dir = in_error_dir;
      (*no_msg_queued)++;
      fsa[mdb[pos].fsa_pos].jobs_queued++;
   }

   return;
}

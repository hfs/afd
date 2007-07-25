/*
 *  check_file_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2007 Deutscher Wetterdienst (DWD),
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
 **   16.04.2002 H.Kiehl Improve speed when inserting new jobs in queue.
 **
 */
DESCR__E_M3

#include <string.h>          /* strcmp(), strerror(), strcpy(), strlen() */
#include <stdlib.h>          /* strtoul(), strtol()                      */
#include <sys/types.h>
#include <sys/stat.h>        /* S_ISDIR(), stat()                        */
#include <dirent.h>          /* opendir(), readdir()                     */
#include <unistd.h>          /* rmdir()                                  */
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"

/* #define WITH_VERBOSE_LOG */

/* External global variables. */
extern int                        *no_msg_queued,
                                  qb_fd;
extern char                       file_dir[],
                                  *p_file_dir,
                                  *p_work_dir;
extern struct queue_buf           *qb;
extern struct msg_cache_buf       *mdb;
extern struct filetransfer_status *fsa;

/* Local global variables. */
static int                        no_of_job_ids;
static struct job_id_data         *jd = NULL;

/* Local function prototypes */
static void                       add_message_to_queue(char *, int, off_t,
                                                       unsigned int, int);
static int                        check_jobs(int),
                                  read_job_ids(void);


/*########################## check_file_dir() ###########################*/
int
check_file_dir(int force_check)
{
   int         all_checked;
   struct stat stat_buf;

   if (stat(file_dir, &stat_buf) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to stat() `%s' : %s", file_dir, strerror(errno));
      return(INCORRECT);
   }

#ifdef WITH_VERBOSE_LOG
   system_log(DEBUG_SIGN, NULL, 0, "FD starting file dir check . . .");
#endif

   if (read_job_ids() == INCORRECT)
   {
      no_of_job_ids = 0;
   }

   /* First check only the file directory. */
   if (check_jobs(force_check) == SUCCESS)
   {
      all_checked = YES;
   }
   else
   {
      all_checked = NO;
   }
#ifdef WITH_VERBOSE_LOG
   system_log(DEBUG_SIGN, NULL, 0, "FD file dir check done.");
#endif


   if (jd != NULL)
   {
      free(jd);
      jd = NULL;
   }

   return(all_checked);
}


/*++++++++++++++++++++++++++++++ check_jobs() +++++++++++++++++++++++++++*/
static int
check_jobs(int force_check)
{
   int           i,
                 jd_pos,
                 ret = SUCCESS;
   unsigned int  job_id;
   char          *p_dir_no,
                 *p_job_id,
                 *ptr;
   DIR           *dir_no_dp,
                 *dp,
                 *id_dp;
   struct dirent *dir_no_dirp,
                 *dirp,
                 *id_dirp;
   struct stat   stat_buf;

   /*
    * Check file directory for files that do not have a message.
    */
   if ((dp = opendir(file_dir)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to opendir() `%s' : %s", file_dir, strerror(errno));
      return(INCORRECT);
   }
   ptr = p_file_dir;
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
         job_id = (unsigned int)strtoul(ptr, (char **)NULL, 16);
         jd_pos = -1;
         for (i = 0; i < no_of_job_ids; i++)
         {
            if (jd[i].job_id == job_id)
            {
               jd_pos = i;
               break;
            }
         }
         if (jd_pos == -1)
         {
            /*
             * This is a old directory no longer in
             * our job list. So lets remove the
             * entire directory.
             */
            if (rec_rmdir(file_dir) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to rec_rmdir() %s", file_dir);
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Removed directory %s since it is no longer in database.",
                          file_dir);
            }
         }
         else
         {
            if (stat(file_dir, &stat_buf) == -1)
            {
               /*
                * Be silent when there is no such file or directory, since
                * it could be that it has been removed by some other process.
                */
               if (errno != ENOENT)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to stat() `%s' : %s",
                             file_dir, strerror(errno));
               }
            }
            else
            {
               /* Test if it is a directory. */
               if (S_ISDIR(stat_buf.st_mode))
               {
                  if ((id_dp = opendir(file_dir)) == NULL)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Failed to opendir() `%s' : %s",
                                file_dir, strerror(errno));
                  }
                  else
                  {
                     p_job_id = ptr + strlen(dirp->d_name);
                     *p_job_id = '/';
                     p_job_id++;
                     errno = 0;
                     while ((id_dirp = readdir(id_dp)) != NULL)
                     {
                        if (id_dirp->d_name[0] != '.')
                        {
                           (void)strcpy(p_job_id, id_dirp->d_name);
                           if (stat(file_dir, &stat_buf) == -1)
                           {
                              if (errno != ENOENT)
                              {
                                 system_log(WARN_SIGN, __FILE__, __LINE__,
                                            "Failed to stat() `%s' : %s",
                                            file_dir, strerror(errno));
                              }
                           }
                           else
                           {
                              if (S_ISDIR(stat_buf.st_mode))
                              {
                                 if ((stat_buf.st_nlink < MAX_FD_DIR_CHECK) ||
                                     (force_check == YES))
                                 {
                                    if ((dir_no_dp = opendir(file_dir)) == NULL)
                                    {
                                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                                  "Failed to opendir() `%s' : %s",
                                                  file_dir, strerror(errno));
                                    }
                                    else
                                    {
                                       p_dir_no = p_job_id + strlen(id_dirp->d_name);
                                       *p_dir_no = '/';
                                       p_dir_no++;
                                       errno = 0;
                                       while ((dir_no_dirp = readdir(dir_no_dp)) != NULL)
                                       {
                                          if (dir_no_dirp->d_name[0] != '.')
                                          {
                                             (void)strcpy(p_dir_no, dir_no_dirp->d_name);
                                             if (stat(file_dir, &stat_buf) == -1)
                                             {
                                                if (errno != ENOENT)
                                                {
                                                   system_log(WARN_SIGN, __FILE__, __LINE__,
                                                              "Failed to stat() `%s' : %s",
                                                              file_dir, strerror(errno));
                                                }
                                             }
                                             else
                                             {
                                                if (S_ISDIR(stat_buf.st_mode))
                                                {
                                                   int gotcha = NO;

                                                   for (i = 0; i < *no_msg_queued; i++)
                                                   {
                                                      if (CHECK_STRCMP(qb[i].msg_name, ptr) == 0)
                                                      {
                                                         gotcha = YES;
                                                         break;
                                                      }
                                                   }
                                                   if (gotcha == NO)
                                                   {
                                                      int           file_counter = 0;
                                                      off_t         size_counter = 0;
                                                      char          *p_file;
                                                      DIR           *file_dp;
                                                      struct dirent *file_dirp;

                                                      if ((file_dp = opendir(file_dir)) == NULL)
                                                      {
                                                         system_log(WARN_SIGN, __FILE__, __LINE__,
                                                                    "Failed to opendir() `%s' : %s",
                                                                    file_dir, strerror(errno));
                                                      }
                                                      else
                                                      {
                                                         p_file = file_dir + strlen(file_dir);
                                                         *p_file = '/';
                                                         p_file++;
                                                         errno = 0;
                                                         while ((file_dirp = readdir(file_dp)) != NULL)
                                                         {
                                                            if (((file_dirp->d_name[0] == '.') &&
                                                                (file_dirp->d_name[1] == '\0')) ||
                                                                ((file_dirp->d_name[0] == '.') &&
                                                                (file_dirp->d_name[1] == '.') &&
                                                                (file_dirp->d_name[2] == '\0')))
                                                            {
                                                               continue;
                                                            }
                                                            (void)strcpy(p_file, file_dirp->d_name);
                                                            if (stat(file_dir, &stat_buf) != -1)
                                                            {
                                                               file_counter++;
                                                               size_counter += stat_buf.st_size;
                                                            }
                                                            errno = 0;
                                                         }
                                                         *(p_file - 1) = '\0';
                                                         if ((errno) && (errno != ENOENT))
                                                         {
                                                            system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                                       "Failed to readdir() `%s' : %s",
                                                                       file_dir, strerror(errno));
                                                         }
                                                         if (closedir(file_dp) == -1)
                                                         {
                                                            system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                                       "Failed to closedir() `%s' : %s",
                                                                       file_dir, strerror(errno));
                                                         }
                                                      }

                                                      if (file_counter > 0)
                                                      {
                                                         /*
                                                          * Message is NOT in queue. Add message to queue.
                                                          */
                                                         system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                                                                    "Message `%s' not in queue, adding message (%d files %ld bytes).",
#else
                                                                    "Message `%s' not in queue, adding message (%d files %lld bytes).",
#endif
                                                                    ptr, file_counter,
                                                                    (pri_off_t)size_counter);
                                                         add_message_to_queue(ptr, file_counter, size_counter, job_id, jd_pos);
                                                      }
                                                      else
                                                      {
                                                         /*
                                                          * This is just an empty directory, delete it.
                                                          */
                                                         if (rmdir(file_dir) == -1)
                                                         {
                                                            if ((errno == ENOTEMPTY) ||
                                                                (errno == EEXIST))
                                                            {
                                                               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                                          "Failed to rmdir() `%s' because there is still data in it, deleting everything in this directory.",
                                                                          file_dir);
                                                               (void)rec_rmdir(file_dir);
                                                            }
                                                            else
                                                            {
                                                               system_log(WARN_SIGN, __FILE__, __LINE__,
                                                                          "Failed to rmdir() `%s' : %s",
                                                                          file_dir, strerror(errno));
                                                            }
                                                         }
                                                         else
                                                         {
                                                            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                                       "Deleted empty directory `%s'.",
                                                                       file_dir);
                                                         }
                                                      }
                                                   }
                                                }
                                             }
                                          }
                                          errno = 0;
                                       }

                                       if ((errno) && (errno != ENOENT))
                                       {
                                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                     "Failed to readdir() `%s' : %s",
                                                     file_dir, strerror(errno));
                                       }
                                       if (closedir(dir_no_dp) == -1)
                                       {
                                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                     "Failed to closedir() `%s' : %s",
                                                     file_dir, strerror(errno));
                                       }
                                    }
                                 }
                                 else
                                 {
                                    if (stat_buf.st_nlink >= MAX_FD_DIR_CHECK)
                                    {
                                       ret = INCORRECT;
                                    }
                                 }
                              }
                           }
                        }
                        errno = 0;
                     }

                     if ((errno) && (errno != ENOENT))
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to readdir() `%s' : %s",
                                   file_dir, strerror(errno));
                     }
                     if (closedir(id_dp) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to closedir() `%s' : %s",
                                   file_dir, strerror(errno));
                     }
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
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to readdir() `%s' : %s",
                 file_dir, strerror(errno));
   }
   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to closedir() `%s' : %s",
                 file_dir, strerror(errno));
   }

   return(ret);
}


/*------------------------ add_message_to_queue() -----------------------*/
static void
add_message_to_queue(char         *dir_name,
                     int          file_counter,
                     off_t        size_counter,
                     unsigned int job_id,
                     int          jd_pos)
{
   unsigned int pos,
                split_job_counter,
                unique_number;
   time_t       creation_time;
   char         msg_name[MAX_MSG_NAME_LENGTH],
                msg_priority = 0,
                *p_start,
                *ptr;

   check_queue_space();

   /*
    * Retrieve priority, creation time, unique number and job ID
    * from the message name.
    */
   (void)strcpy(msg_name, dir_name);
   ptr = msg_name;
   while ((*ptr != '/') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr != '/')
   {
      return;
   }
   ptr++;
   while ((*ptr != '/') && (*ptr != '\0'))
   {
      ptr++; /* Ignore directory number */
   }
   if (*ptr != '/')
   {
      return;
   }
   ptr++;
   p_start = ptr;
   while ((*ptr != '_') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr != '_')
   {
      return;
   }
   *ptr = '\0';
   creation_time = (time_t)str2timet(p_start, (char **)NULL, 16);
   ptr++;
   p_start = ptr;
   while ((*ptr != '_') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr != '_')
   {
      return;
   }
   *ptr = '\0';
   unique_number = (unsigned int)strtoul(p_start, (char **)NULL, 16);
   ptr++;
   p_start = ptr;
   while ((*ptr != '_') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr != '\0')
   {
      return;
   }
   *ptr = '\0';
   split_job_counter = (unsigned int)strtoul(p_start, (char **)NULL, 16);

   if ((pos = lookup_job_id(job_id)) == INCORRECT)
   {
      char del_dir[MAX_PATH_LENGTH];

      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not locate job %x", job_id);
      (void)sprintf(del_dir, "%s%s%s/%s", p_work_dir,
                    AFD_FILE_DIR, OUTGOING_DIR, dir_name);
#ifdef _DELETE_LOG
      remove_job_files(del_dir, -1, job_id, OTHER_OUTPUT_DEL);
#else
      remove_job_files(del_dir, -1);
#endif
   }
   else
   {
      if (jd_pos != -1)
      {
         int    qb_pos;
         double msg_number;

         msg_priority = jd[jd_pos].priority;
         msg_number = ((double)msg_priority - 47.0) *
                      (((double)creation_time * 10000.0) +
                      (double)unique_number + (double)split_job_counter);
         if (*no_msg_queued > 0)
         {
            if (*no_msg_queued == 1)
            {
               if (qb[0].msg_number < msg_number)
               {
                  qb_pos = 1;
               }
               else
               {
                  size_t move_size;
   
                  move_size = *no_msg_queued * sizeof(struct queue_buf);
                  (void)memmove(&qb[1], &qb[0], move_size);
                  qb_pos = 0;
               }
            }
            else
            {
               if (msg_number < qb[0].msg_number)
               {
                  size_t move_size;

                  move_size = *no_msg_queued * sizeof(struct queue_buf);
                  (void)memmove(&qb[1], &qb[0], move_size);
                  qb_pos = 0;
               }
               else if (msg_number > qb[*no_msg_queued - 1].msg_number)
                    {
                       qb_pos = *no_msg_queued;
                    }
                    else
                    {
                       int center,
                           end = *no_msg_queued - 1,
                           start = 0;

                       for (;;)
                       {
                          center = (end - start) / 2;
                          if (center == 0)
                          {
                             size_t move_size;

                             move_size = (*no_msg_queued - (start + 1)) *
                                         sizeof(struct queue_buf);
                             (void)memmove(&qb[start + 2], &qb[start + 1],
                                           move_size);
                             qb_pos = start + 1;
                             break;
                          }
                          if (msg_number < qb[start + center].msg_number)
                          {
                             end = start + center;
                          }
                          else
                          {
                             start = start + center;
                          }
                       }
                    }
            }
         }
         else
         {
            qb_pos = 0;
         }

         (void)strcpy(qb[qb_pos].msg_name, dir_name);
         qb[qb_pos].msg_number = msg_number;
         qb[qb_pos].pid = PENDING;
         qb[qb_pos].creation_time = creation_time;
         qb[qb_pos].pos = pos;
         qb[qb_pos].connect_pos = -1;
         qb[qb_pos].retries = 0;
         qb[qb_pos].files_to_send = file_counter;
         qb[qb_pos].file_size_to_send = size_counter;
         qb[qb_pos].special_flag = 0;
         (*no_msg_queued)++;
         fsa[mdb[pos].fsa_pos].jobs_queued++;
      }
   }

   return;
}


/*---------------------------- read_job_ids() ---------------------------*/
static int
read_job_ids(void)
{
   int  fd,
        ret = SUCCESS;
   char job_id_data_file[MAX_PATH_LENGTH];

   (void)strcpy(job_id_data_file, p_work_dir);
   (void)strcat(job_id_data_file, FIFO_DIR);
   (void)strcat(job_id_data_file, JOB_ID_DATA_FILE);
   if ((fd = open(job_id_data_file, O_RDONLY)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s",
                 job_id_data_file, strerror(errno));
      ret = INCORRECT;
   }
   else
   {
      if (read(fd, &no_of_job_ids, sizeof(int)) != sizeof(int))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to read() `%s' : %s",
                    job_id_data_file, strerror(errno));
         ret = INCORRECT;
      }
      else
      {
         if (lseek(fd, AFD_WORD_OFFSET, SEEK_SET) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to lseek() `%s' : %s",
                       job_id_data_file, strerror(errno));
            ret = INCORRECT;
         }
         else
         {
            size_t size;

            size = no_of_job_ids * sizeof(struct job_id_data);
            if ((jd = malloc(size)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to malloc() %d bytes : %s",
                          size, strerror(errno));
               ret = INCORRECT;
            }
            else
            {
               if (read(fd, jd, size) != size)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to read() from `%s' : %s",
                             job_id_data_file, strerror(errno));
                  free(jd);
                  jd = NULL;
                  ret = INCORRECT;
               }
            }
         }
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to close() `%s' : %s",
                    job_id_data_file, strerror(errno));
      }
   }
   return(ret);
}

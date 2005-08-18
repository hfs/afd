/*
 *  handle_time_jobs.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_time_jobs - process all time jobs of AMG
 **
 ** SYNOPSIS
 **   void handle_time_jobs(int *no_of_process, time_t now);
 **
 ** DESCRIPTION
 **   The function handle_time_jobs() searches the time directory for
 **   jobs that have to be distributed. After handling them it also
 **   calculates the next time for each job.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.04.1999 H.Kiehl Created
 **   24.09.2004 H.Kiehl Added split job counter.
 **
 */
DESCR__E_M3

#include <string.h>           /* strerror(), strcpy()                    */
#include <stdlib.h>           /* strtoul()                               */
#include <time.h>             /* time()                                  */
#include <ctype.h>            /* isdigit()                               */
#include <sys/types.h>
#include <sys/stat.h>         /* S_ISDIR()                               */
#include <dirent.h>           /* opendir(), readdir(), closedir()        */
#include <unistd.h>           /* sleep()                                 */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int                        amg_counter_fd,
                                  fin_fd,
                                  max_process,
                                  no_of_jobs,
                                  no_of_time_jobs,
                                  *time_job_list;
extern char                       outgoing_file_dir[],
#ifndef _WITH_PTHREAD
                                  *file_name_buffer,
#endif
                                  time_dir[];
extern struct dc_proc_list        *dcpl;      /* Dir Check Process List */
extern struct instant_db          *db;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;

/* Local variables */
static unsigned int               files_handled;
static size_t                     time_dir_length;

/* Local function prototypes */
static void                       handle_time_dir(int, int *);

#define MAX_FILES_FOR_TIME_JOBS   800


/*######################### handle_time_jobs() ##########################*/
void
handle_time_jobs(int *no_of_process, time_t now)
{
   register int i;

   files_handled = 0;
   /*
    * First rescan structure to update all next_start_time values.
    */
   for (i = 0; i < no_of_time_jobs; i++)
   {
      if (db[time_job_list[i]].next_start_time <= now)
      {
         handle_time_dir(i, no_of_process);
         if (files_handled > MAX_FILES_FOR_TIME_JOBS)
         {
            break;
         }
         db[time_job_list[i]].next_start_time = calc_next_time(&db[time_job_list[i]].te, now);
      }
   }

   return;
}


/*++++++++++++++++++++++++++ handle_time_dir() ++++++++++++++++++++++++++*/
static void
handle_time_dir(int time_job_no, int *no_of_process)
{
   char *time_dir_ptr;
   DIR  *dp;

   /*
    * Now search time job directory and see if there are any files to
    * be processed.
    */
   if (time_dir_length == 0)
   {
      time_dir_length = strlen(time_dir);
   }
   time_dir_ptr = time_dir + time_dir_length;
   (void)strcpy(time_dir_ptr, db[time_job_list[time_job_no]].str_job_id);

   if ((dp = opendir(time_dir)) == NULL)
   {
      if (errno != ENOENT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Can't access directory %s : %s",
                    time_dir, strerror(errno));
      }
   }
   else
   {
      int            files_moved;
      off_t          file_size_moved;
      time_t         creation_time;
      int            unique_number;
      unsigned int   split_job_counter;
      char           dest_file_path[MAX_PATH_LENGTH],
                     *p_dest,
                     *p_dest_end = NULL,
                     *p_src,
                     unique_name[MAX_PATH_LENGTH];
      struct dirent  *p_dir;
      struct stat    stat_buf;

      unique_name[0] = '/';
      p_src = time_dir + strlen(time_dir);
      *p_src++ = '/';
      do
      {
         files_moved = 0;
         file_size_moved = 0;
         p_dest = NULL;

         while ((files_moved < fra[db[time_job_list[time_job_no]].fra_pos].max_copied_files) &&
                ((p_dir = readdir(dp)) != NULL))
         {
            if (p_dir->d_name[0] == '.')
            {
               continue;
            }

            (void)strcpy(p_src, p_dir->d_name);
            if (stat(time_dir, &stat_buf) == -1)
            {
               if (errno != ENOENT)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to stat() %s : %s", strerror(errno));
               }
            }
            else
            {
               if (p_dest == NULL)
               {
                  if (db[time_job_list[time_job_no]].loptions != NULL)
                  {
                     /* Create a new message name and directory. */
                     creation_time = time(NULL);
                     (void)strcpy(dest_file_path, outgoing_file_dir);
                     split_job_counter = 0;
                     unique_number = -1;
                     if (create_name(dest_file_path,
                                     db[time_job_list[time_job_no]].priority,
                                     creation_time,
                                     db[time_job_list[time_job_no]].job_id,
                                     &split_job_counter, &unique_number,
                                     &unique_name[1], amg_counter_fd) < 0)
                     {
                        if (errno == ENOSPC)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "DISK FULL!!! Will retry in %d second interval.",
                                      DISK_FULL_RESCAN_TIME);

                           while (errno == ENOSPC)
                           {
                              (void)sleep(DISK_FULL_RESCAN_TIME);
                              creation_time = time(NULL);
                              errno = 0;
                              split_job_counter = 0;
                              unique_number = -1;
                              if (create_name(dest_file_path,
                                              db[time_job_list[time_job_no]].priority,
                                              creation_time,
                                              db[time_job_list[time_job_no]].job_id,
                                              &split_job_counter,
                                              &unique_number,
                                              &unique_name[1],
                                              amg_counter_fd) < 0)
                              {
                                 if (errno != ENOSPC)
                                 {
                                    system_log(FATAL_SIGN, __FILE__, __LINE__,
                                               "Failed to create a unique name : %s",
                                               strerror(errno));
                                    exit(INCORRECT);
                                 }
                              }
                           }

                           system_log(INFO_SIGN, __FILE__, __LINE__,
                                      "Continuing after disk was full.");
                        }
                        else
                        {
                           system_log(FATAL_SIGN, __FILE__, __LINE__,
                                      "Failed to create a unique name : %s",
                                      strerror(errno));
                           exit(INCORRECT);
                        }
                     }
                     p_dest_end = dest_file_path + strlen(dest_file_path);
                     (void)strcpy(p_dest_end, unique_name);
                     p_dest = p_dest_end + strlen(unique_name);
                     *(p_dest++) = '/';
                     *p_dest = '\0';
                  }
                  else
                  {
                     int dir_no;

                     if ((dir_no = get_dir_number(dest_file_path,
                                                  db[time_job_list[time_job_no]].job_id,
                                                  NULL)) == INCORRECT)
                     {
                        if (p_dest_end != NULL)
                        {
                           *p_dest_end = '\0';
                        }
                        return;
                     }
                     creation_time = time(NULL);
                     p_dest_end = dest_file_path + strlen(dest_file_path);
                     if (*(p_dest_end - 1) == '/')
                     {
                        p_dest_end--;
                     }
                     (void)sprintf(unique_name, "%x/%x/%x_%x_%x",
                                   db[time_job_list[time_job_no]].job_id,
                                   dir_no, creation_time, unique_number,
                                   split_job_counter);
                     p_dest = p_dest_end +
                              sprintf(p_dest_end, "/%s/", unique_name);
                     if (mkdir(dest_file_path, DIR_MODE) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to create directory %s : %s",
                                   dest_file_path, strerror(errno));
                        if (p_dest_end != NULL)
                        {
                           *p_dest_end = '\0';
                        }
                        return;
                     }
                  }
               }

               (void)strcpy(p_dest, p_dir->d_name);
               if (move_file(time_dir, dest_file_path) < 0)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to move file %s to %s",
                             time_dir, dest_file_path);
               }
               else
               {
#ifndef _WITH_PTHREAD
                  if ((files_moved % 10) == 0)
                  {
                     size_t new_size;

                     /* Calculate new size of file name buffer */
                     new_size = ((files_moved / 10) + 1) * 10 * MAX_FILENAME_LENGTH;

                     /* Increase the space for the file name buffer */
                     if ((file_name_buffer = realloc(file_name_buffer, new_size)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Could not realloc() memory [%d bytes] : %s",
                                   new_size, strerror(errno));
                        exit(INCORRECT);
                     }
                  }
                  (void)strcpy((file_name_buffer + (files_moved * MAX_FILENAME_LENGTH)), p_dir->d_name);
#endif /* !_WITH_PTHREAD */
                  files_handled++;
                  files_moved++;
                  file_size_moved += stat_buf.st_size;
               }
            }
         } /* while ((p_dir = readdir(dp)) != NULL) */

         if (files_moved > 0)
         {
            if ((db[time_job_list[time_job_no]].lfs & GO_PARALLEL) &&
                (*no_of_process < max_process) &&
                (fra[db[time_job_list[time_job_no]].fra_pos].no_of_process < fra[db[time_job_list[time_job_no]].fra_pos].max_process) &&
                (fsa[db[time_job_list[time_job_no]].position].host_status < 2) &&
                ((fsa[db[time_job_list[time_job_no]].position].special_flag & HOST_DISABLED) == 0))
            {
               if ((dcpl[*no_of_process].pid = fork()) == -1)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to fork() : %s", strerror(errno));
                  send_message(outgoing_file_dir, unique_name, split_job_counter,
                               unique_number, creation_time,
                               time_job_list[time_job_no],
                               files_moved, file_size_moved);
               }
               else if (dcpl[*no_of_process].pid == 0) /* Child process */
                    {
                       pid_t pid;
#ifdef _FIFO_DEBUG
                       char  cmd[2];
#endif

                       send_message(outgoing_file_dir, unique_name,
                                    split_job_counter, unique_number,
                                    creation_time, time_job_list[time_job_no],
                                    files_moved, file_size_moved);

                       /* Tell parent we completed our task. */
#ifdef _FIFO_DEBUG
                       cmd[0] = ACKN; cmd[1] = '\0';
                       show_fifo_data('W', "ip_fin", cmd, 1, __FILE__, __LINE__);
#endif
                       pid = getpid();
                       if (write(fin_fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Could not write() to fifo %s : %s",
                                     IP_FIN_FIFO, strerror(errno));
                       }
                       (void)close(fin_fd);
                       exit(SUCCESS);
                    }
                    else /* Parent process */
                    {
                       fra[db[time_job_list[time_job_no]].fra_pos].no_of_process++;
                       dcpl[*no_of_process].fra_pos = db[time_job_list[time_job_no]].fra_pos;
                       (*no_of_process)++;
                    }
            }
            else
            {
               send_message(outgoing_file_dir, unique_name, split_job_counter,
                            unique_number, creation_time,
                            time_job_list[time_job_no],
                            files_moved, file_size_moved);
            }
         } /* if (files_moved > 0) */
      } while ((p_dir != NULL) && (files_handled < MAX_FILES_FOR_TIME_JOBS));

      if (closedir(dp) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to closedir() %s : %s", time_dir, strerror(errno));
      }
      if (p_dest_end != NULL)
      {
         *p_dest_end = '\0';
      }
   }
   *time_dir_ptr = '\0';

   return;
}

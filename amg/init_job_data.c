/*
 *  init_job_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_job_data - 
 **
 ** SYNOPSIS
 **   void init_job_data(int *jid_number)
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
 **   21.01.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>       /* strcpy(), strcat(), strlen(), strerror()    */
#include <stdlib.h>       /* strtol()                                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>       /* read()                                      */
#include <dirent.h>       /* opendir(), closedir(), readdir(), DIR       */
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                 dnb_fd,
                           jd_fd,
                           jid_fd,
                           *no_of_dir_names,
                           *no_of_job_ids,
                           sys_log_fd;
extern char                msg_dir[],
                           *p_msg_dir,
                           *p_work_dir;
extern struct job_id_data  *jd;
extern struct dir_name_buf *dnb;


/*########################### init_job_data() ###########################*/
void
init_job_data(int *jid_number)
{
   char        *ptr,
               job_id_data_file[MAX_PATH_LENGTH],
               id_file[MAX_PATH_LENGTH],
               dir_name_file[MAX_PATH_LENGTH];
   size_t      new_size;
   struct stat stat_buf;

   (void)strcpy(job_id_data_file, p_work_dir);
   (void)strcat(job_id_data_file, FIFO_DIR);
   (void)strcpy(dir_name_file, job_id_data_file);
   (void)strcat(dir_name_file, DIR_NAME_FILE);
   (void)strcat(job_id_data_file, JOB_ID_DATA_FILE);
   (void)strcpy(msg_dir, p_work_dir);
   (void)strcat(msg_dir, AFD_MSG_DIR);
   (void)strcat(msg_dir, "/");
   p_msg_dir = msg_dir + strlen(msg_dir);

   /* Attach job ID data. */
   new_size = (JOB_ID_DATA_STEP_SIZE * sizeof(struct job_id_data)) +
              AFD_WORD_OFFSET;
   if ((ptr = attach_buf(job_id_data_file, &jd_fd, new_size, "AMG")) == (caddr_t) -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to mmap() to %s : %s (%s %d)\n",
                job_id_data_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_of_job_ids = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   jd = (struct job_id_data *)ptr;
   lock_region_w(jd_fd, (off_t)1);

   /* Attach directory names. */
   new_size = (DIR_NAME_BUF_SIZE * sizeof(struct dir_name_buf)) +
              AFD_WORD_OFFSET;
   if ((ptr = attach_buf(dir_name_file, &dnb_fd, new_size, "AMG")) == (caddr_t) -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to mmap() to %s : %s (%s %d)\n",
                dir_name_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_of_dir_names = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   dnb = (struct dir_name_buf *)ptr;

   /*
    * Get the highest current job ID.
    */
   (void)sprintf(id_file, "%s%s%s", p_work_dir, FIFO_DIR, JOB_ID_FILE);
   *jid_number = 0;
   if (stat(id_file, &stat_buf) == -1)
   {
      if (errno == ENOENT)
      {
         if ((jid_fd = open(id_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) == -1)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Failed to open() %s : %s (%s %d)\n",
                      id_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if (*no_of_job_ids > 0)
         {
            int i;

            for (i = 0; i < *no_of_job_ids; i++)
            {
               if (jd[i].job_id > *jid_number)
               {
                  *jid_number = jd[i].job_id;
               }
            }
            (*jid_number)++;
         }
         else
         {
            char message_dir[MAX_PATH_LENGTH];
            DIR  *dp;

            (void)sprintf(message_dir, "%s%s", p_work_dir, AFD_MSG_DIR);

            if ((dp = opendir(message_dir)) == NULL)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to opendir() %s : %s (%s %d)\n",
                         message_dir, strerror(errno), __FILE__, __LINE__);
            }
            else
            {
               int           msg_number = -1;
               struct dirent *p_dir;

               errno = 0;
               while ((p_dir = readdir(dp)) != NULL)
               {
                  if (p_dir->d_name[0] == '.')
                  {
                     continue;
                  }

                  msg_number = strtol(p_dir->d_name, (char **)NULL, 10);
                  if (msg_number > *jid_number)
                  {
                     *jid_number = msg_number;
                  }
                  errno = 0;
               }
               if (errno)
               {
                  (void)rec(sys_log_fd, FATAL_SIGN,
                            "Could not readdir() %s : %s (%s %d)\n",
                            message_dir, strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
               if (msg_number != -1)
               {
                  (*jid_number)++;
               }

               if (closedir(dp) == -1)
               {
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "Could not close directory %s : %s (%s %d)\n",
                            message_dir, strerror(errno), __FILE__, __LINE__);
               }
            }
         }
      }
      else
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to stat() %s : %s (%s %d)\n",
                   id_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   else
   {
      char str_number[MAX_INT_LENGTH];

      if ((jid_fd = open(id_file, O_RDWR)) == -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to open() %s : %s (%s %d)\n",
                   id_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (stat_buf.st_size > (MAX_INT_LENGTH - 1))
      {
         if (read(jid_fd, str_number, (MAX_INT_LENGTH - 1)) != (MAX_INT_LENGTH - 1))
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "read() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         str_number[(MAX_INT_LENGTH - 1)] = '\0';
      }
      else
      {
         if (read(jid_fd, str_number, stat_buf.st_size) != stat_buf.st_size)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "read() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         str_number[stat_buf.st_size] = '\0';
      }
      *jid_number = atoi(str_number);
   }

   if ((*jid_number > 2147483647) || (*jid_number < 0))
   {
      *jid_number = 0;
   }

   return;
}

/*
 *  rm_job.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M1
/*
 ** NAME
 **   rm_job - removes job(s) from the internal AFD queue
 **
 ** SYNOPSIS
 **   rm_job [-w <AFD work dir>] [--version] <job 1> [... <job n>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Returns INCORRECT (-1) when it fails to send the remove command.
 **   Otherwise SUCCESS (0) is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.05.1998 H.Kiehl Created
 **   27.06.1998 H.Kiehl Remove directory information from job name.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                 /* exit()                            */
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                 /* rmdir()                           */
#include <sys/mman.h>               /* mmap()                            */
#include <dirent.h>                 /* opendir(), closedir(), readdir(), */
                                    /* DIR, struct dirent                */
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* write()                           */
#include <errno.h>
#include "fddefs.h"
#include "version.h"

/* Global variables */
int                        fsa_fd = -1,
                           fsa_id,
                           no_of_hosts,
                           *no_msg_cached,
                           *no_msg_queued,
                           sys_log_fd = STDERR_FILENO;
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
char                       *p_work_dir;
struct afd_status          *p_afd_status;
struct queue_buf           *qb = NULL;
struct msg_cache_buf       *mdb;
struct filetransfer_status *fsa;

/* Local function prototypes */
static int                 is_msgname(char *);
static void                attach_to_queue_buffer(void),
                           remove_job(char *, int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int    fd;
   size_t length;
   char   *p_job_name,
          delete_fifo[MAX_PATH_LENGTH],
          err_file_dir[MAX_PATH_LENGTH],
          file_dir[MAX_PATH_LENGTH],
          work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc < 2)
   {
      (void)fprintf(stderr,
                    "Usage: %s [-w <AFD work dir>] [--version] <job 1> [... <job n>]\n",
                    argv[0]);
      exit(INCORRECT);
   }

   /* Attach to FSA and the AFD Status Area */
   if (fsa_attach() < 0)
   {
      (void)fprintf(stderr, "Failed to attach to FSA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (attach_afd_status() < 0)
   {
      (void)fprintf(stderr,
                    "Failed to map to AFD status area. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   (void)sprintf(file_dir, "%s%s/", p_work_dir, AFD_FILE_DIR);
   (void)sprintf(err_file_dir, "%s%s%s/", p_work_dir, AFD_FILE_DIR, ERROR_DIR);
   (void)strcpy(delete_fifo, work_dir);
   (void)strcat(delete_fifo, FIFO_DIR);
   (void)strcat(delete_fifo, DELETE_JOBS_FIFO);
   if ((fd = open(delete_fifo, O_RDWR)) == -1)
   {
      (void)fprintf(stderr, "Failed to open %s : %s (%s %d)\n",
                    delete_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   while (argc > 1)
   {
      length = strlen(argv[1]);
      p_job_name = argv[1] + length - 1;
      while ((*p_job_name != '/') && (length > 0))
      {
         p_job_name--; length--;
      }
      if (*p_job_name == '/')
      {
         p_job_name++;
      }
      else
      {
         p_job_name = argv[1];
      }
      if (is_msgname(p_job_name) == SUCCESS)
      {
         length = strlen(p_job_name) + 1;

         if (p_afd_status->fd == ON)
         {
            if (write(fd, p_job_name, length) != length)
            {
               (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
         }
         else /* FD is currently not active */
         {
            int i;

            if (qb == NULL)
            {
               attach_to_queue_buffer();
            }

            for (i = 0; i < *no_msg_queued; i++)
            {
               if (strcmp(qb[i].msg_name, p_job_name) == 0)
               {
                  char *p_job_dir,
                       *ptr;

                  if (qb[i].in_error_dir == YES)
                  {
                     p_job_dir = err_file_dir;
                     ptr = p_job_dir + strlen(p_job_dir);
                     (void)sprintf(ptr, "%s/%s",
                                   mdb[qb[i].pos].host_name,
                                   qb[i].msg_name);
                  }
                  else
                  {
                     p_job_dir = file_dir;
                     ptr = p_job_dir + strlen(p_job_dir);
                     (void)strcpy(ptr, qb[i].msg_name);
                  }
                  remove_job(p_job_dir, mdb[qb[i].pos].fsa_pos);
                  *ptr = '\0';

                  if (i != (*no_msg_queued - 1))
                  {
                     size_t move_size;

                     move_size = (*no_msg_queued - 1 - i) * sizeof(struct queue_buf);
                     (void)memmove(&qb[i], &qb[i + 1], move_size);
                  }
                  (*no_msg_queued)--;
                  break;
               }
            }
         }
      }
      else
      {
         (void)fprintf(stderr, "%s is not an AFD job name!\n", p_job_name);
      }
      argv++;
      argc--;
   }
   (void)close(fd);

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++ is_msgname() ++++++++++++++++++++++++++++*/
static int
is_msgname(char *name)
{
   if (isdigit(name[0]) != 0)
   {
      if (name[1] == '_')
      {
         int i = 2;

         while (name[i] != '\0')
         {
            if (isdigit(name[i]) == 0)
            {
               if (name[i] != '_')
               {
                  return(INCORRECT);
               }
            }
            i++;
         }
         return(SUCCESS);
      }
   }

   return(INCORRECT);
}


/*++++++++++++++++++++++++ attach_to_queue_buffer() +++++++++++++++++++++*/
static void
attach_to_queue_buffer(void)
{
   int         fd;
   char        file[MAX_PATH_LENGTH],
               *ptr;
   struct stat stat_buf;

   (void)sprintf(file, "%s%s%s", p_work_dir, FIFO_DIR, MSG_QUEUE_FILE);
   if ((fd = open(file, O_RDWR)) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to open() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (fstat(fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to fstat() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
   {
      (void)fprintf(stderr,
                    "Failed to mmap() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_msg_queued = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   qb = (struct queue_buf *)ptr;

   (void)sprintf(file, "%s%s%s", p_work_dir, FIFO_DIR, MSG_CACHE_FILE);
   if ((fd = open(file, O_RDWR)) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to open() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (fstat(fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to fstat() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
   {
      (void)fprintf(stderr,
                    "Failed to mmap() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_msg_cached = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   mdb = (struct msg_cache_buf *)ptr;

   return;
}


/*+++++++++++++++++++++++++++++ remove_job() ++++++++++++++++++++++++++++*/
static void
remove_job(char *del_dir, int fsa_pos)
{
   int           number_deleted = 0;
   off_t         file_size_deleted = 0;
   char          *ptr;
   DIR           *dp;
   struct dirent *p_dir;
   struct stat   stat_buf;

   if ((dp = opendir(del_dir)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to opendir() %s : %s (%s %d)\n",
                del_dir, strerror(errno), __FILE__, __LINE__);
      return;
   }
   ptr = del_dir + strlen(del_dir);
   *(ptr++) = '/';

   while ((p_dir = readdir(dp)) != NULL)
   {
      if (p_dir->d_name[0] == '.')
      {
         continue;
      }
      (void)strcpy(ptr, p_dir->d_name);

      if (stat(del_dir, &stat_buf) == -1)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Failed to stat() %s : %s (%s %d)\n",
                   del_dir, strerror(errno), __FILE__, __LINE__);
         if (remove(del_dir) == -1)
         {
            *(ptr - 1) = '\0';
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to remove() file in %s : %s (%s %d)\n",
                      del_dir, strerror(errno), __FILE__, __LINE__);
         }
      }
      else
      {
         if (remove(del_dir) == -1)
         {
            *(ptr - 1) = '\0';
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to remove() file in %s : %s (%s %d)\n",
                      del_dir, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            number_deleted++;
            file_size_deleted += stat_buf.st_size;
         }
      }
   }

   *(ptr - 1) = '\0';
   if (closedir(dp) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not closedir() %s : %s (%s %d)\n",
                del_dir, strerror(errno), __FILE__, __LINE__);
   }
   if (rmdir(del_dir) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not rmdir() %s : %s (%s %d)\n",
                del_dir, strerror(errno), __FILE__, __LINE__);
   }

   if ((number_deleted > 0) && (fsa_pos != -1))
   {
#ifdef _VERIFY_FSA
      unsigned int ui_variable;
#endif

      /* Total file counter */
      lock_region_w(fsa_fd, (char *)&fsa[fsa_pos].total_file_counter - (char *)fsa);
      fsa[fsa_pos].total_file_counter -= number_deleted;
#ifdef _VERIFY_FSA
      if (fsa[fsa_pos].total_file_counter < 0)
      {
         (void)rec(sys_log_fd, INFO_SIGN,
                   "Total file counter for host %s less then zero. Correcting. (%s %d)\n",
                   fsa[fsa_pos].host_dsp_name, __FILE__, __LINE__);
         fsa[fsa_pos].total_file_counter = 0;
      }
#endif
      unlock_region(fsa_fd, (char *)&fsa[fsa_pos].total_file_counter - (char *)fsa);

      /* Total file size */
      lock_region_w(fsa_fd, (char *)&fsa[fsa_pos].total_file_size - (char *)fsa);
#ifdef _VERIFY_FSA
      ui_variable = fsa[fsa_pos].total_file_size;
#endif
      fsa[fsa_pos].total_file_size -= file_size_deleted;
#ifdef _VERIFY_FSA
      if (fsa[fsa_pos].total_file_size > ui_variable)
      {
         (void)rec(sys_log_fd, INFO_SIGN,
                   "Total file size for host %s overflowed. Correcting. (%s %d)\n",
                   fsa[fsa_pos].host_dsp_name, __FILE__, __LINE__);
         fsa[fsa_pos].total_file_size = 0;
      }
      else if ((fsa[fsa_pos].total_file_counter == 0) &&
               (fsa[fsa_pos].total_file_size > 0))
           {
              (void)rec(sys_log_fd, INFO_SIGN,
                        "fc for host %s is zero but fs is not zero. Correcting. (%s %d)\n",
                        fsa[fsa_pos].host_dsp_name, __FILE__, __LINE__);
              fsa[fsa_pos].total_file_size = 0;
           }
#endif
      unlock_region(fsa_fd, (char *)&fsa[fsa_pos].total_file_size - (char *)fsa);
   }

   return;
}

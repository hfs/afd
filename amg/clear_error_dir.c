/*
 *  clear_pool_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   clear_error_dir - removes host that are no longer in the FSA
 **                     from the error directory
 **
 ** SYNOPSIS
 **   void clear_error_dir(void)
 **
 ** DESCRIPTION
 **   The function clear_error_dir() will remove all hostnames from
 **   the error directory that it does not find in the FSA.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.01.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                /* sprintf()                            */
#include <stdlib.h>               /* strtoul()                            */
#include <string.h>               /* strcpy(), strerror()                 */
#include <ctype.h>                /* isdigit()                            */
#include <sys/stat.h>             /* stat(), S_ISDIR()                    */
#include <unistd.h>               /* read(), close()                      */
#include <dirent.h>               /* opendir(), closedir(), readdir(),    */
                                  /* DIR, struct dirent                   */
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int                        no_of_hosts,
                                  sys_log_fd;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif

/* Local function prototypes */
static void                       remove_error_directory(char *, char *),
                                  remove_error_files(char *, char *, unsigned int);


/*########################## clear_error_dir() ##########################*/
void
clear_error_dir(void)
{
   char        error_dir[MAX_PATH_LENGTH];
   struct stat stat_buf;

   (void)sprintf(error_dir, "%s%s%s", p_work_dir, AFD_FILE_DIR, ERROR_DIR);

   if (stat(error_dir, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Failed to stat() %s : %s (%s %d)\n",
                error_dir, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      DIR *dp;

      if ((dp = opendir(error_dir)) == NULL)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Failed to opendir() %s : %s (%s %d)\n",
                   error_dir, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         if (fsa_attach() != INCORRECT)
         {
            register int  gotcha,
                          i;
            char          *work_ptr;
            struct dirent *p_dir;

            work_ptr = error_dir + strlen(error_dir);
            *(work_ptr++) = '/';

            errno = 0;
            while ((p_dir = readdir(dp)) != NULL)
            {
               if (p_dir->d_name[0] != '.')
               {
                  gotcha = NO;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     if (CHECK_STRCMP(fsa[i].host_alias , p_dir->d_name) == 0)
                     {
                        gotcha = YES;
                        break;
                     }
                  }
                  if (gotcha == NO)
                  {
                     (void)strcpy(work_ptr, p_dir->d_name);
#ifdef _DELETE_LOG
                     remove_error_directory(error_dir, p_dir->d_name);
#else
                     (void)rec_rmdir(error_dir);
#endif
                     (void)rec(sys_log_fd, DEBUG_SIGN,
                               "Removed error directory for %s since it's no longer in FSA. (%s %d)\n",
                               p_dir->d_name, __FILE__, __LINE__);
                  }
               }
               errno = 0;
            }

            if (errno)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Could not readdir() %s : %s (%s %d)\n",
                         error_dir, strerror(errno), __FILE__, __LINE__);
            }
            (void)fsa_detach(NO);
         }
         if (closedir(dp) == -1)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Could not close directory %s : %s (%s %d)\n",
                      error_dir, strerror(errno), __FILE__, __LINE__);
         }
      }
   }

   return;
}


#ifdef _DELETE_LOG
/*++++++++++++++++++++++ remove_error_directory() +++++++++++++++++++++++*/
static void
remove_error_directory(char *error_dir, char *hostname)
{
   DIR *dp;

   if ((dp = opendir(error_dir)) == NULL)
   {
      /* Don't complain if error_dir is a file, this way we save a stat(). */
      if (errno != ENOTDIR)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Failed to opendir() %s : %s (%s %d)\n",
                   error_dir, strerror(errno), __FILE__, __LINE__);
      }
   }
   else
   {
      char          *ptr,
                    *work_ptr;
      struct dirent *p_dir;

      work_ptr = error_dir + strlen(error_dir);
      *(work_ptr++) = '/';

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] != '.')
         {
            if (check_job_name(p_dir->d_name) == SUCCESS)
            {
               ptr = p_dir->d_name + strlen(p_dir->d_name) - 1;
               while (isdigit(*ptr))
               {
                  ptr++;
               }
               (void)strcpy(work_ptr, p_dir->d_name);
               remove_error_files(error_dir, hostname,
                                  (unsigned int)strtoul(ptr, NULL, 10));
            }
         }
         errno = 0;
      }

      if (errno)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not readdir() %s : %s (%s %d)\n",
                   error_dir, strerror(errno), __FILE__, __LINE__);
      }
      if (closedir(dp) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not close directory %s : %s (%s %d)\n",
                   error_dir, strerror(errno), __FILE__, __LINE__);
      }
      *(work_ptr - 1) = '\0';
   }
   (void)rec_rmdir(error_dir);

   return;
}


/*+++++++++++++++++++++++++ remove_error_files() ++++++++++++++++++++++++*/
static void
remove_error_files(char *error_dir, char *hostname, unsigned int job_no)
{
   DIR *dp;

   if ((dp = opendir(error_dir)) == NULL)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Failed to opendir() %s : %s (%s %d)\n",
                error_dir, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      char          *work_ptr;
      struct dirent *p_dir;
      struct stat   stat_buf;

      work_ptr = error_dir + strlen(error_dir);
      *(work_ptr++) = '/';

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] != '.')
         {
            (void)strcpy(work_ptr, p_dir->d_name);
            if (stat(error_dir, &stat_buf) < 0)
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Can't access file %s : %s (%s %d)\n",
                         error_dir, strerror(errno), __FILE__, __LINE__);
            }
            else
            {
               /* Sure it is a normal file? */
               if (S_ISDIR(stat_buf.st_mode) == 0)
               {
                  if (unlink(error_dir) == -1)
                  {
                     (void)rec(sys_log_fd, ERROR_SIGN,
                               "Failed to unlink() file %s : %s (%s %d)\n",
                               p_dir->d_name, strerror(errno),
                               __FILE__, __LINE__);
                  }
                  else
                  {
                     int    prog_name_length;
                     size_t dl_real_size;

                     (void)strcpy(dl.file_name, p_dir->d_name);
                     (void)sprintf(dl.host_name, "%-*s %x",
                                   MAX_HOSTNAME_LENGTH, hostname, OTHER_DEL);
                     *dl.file_size = stat_buf.st_size;
                     *dl.job_number = job_no;
                     *dl.file_name_length = strlen(p_dir->d_name);
                     prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                                "AMG Hostname no longer in FSA.");
                     dl_real_size = *dl.file_name_length + dl.size + prog_name_length;
                     if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                     {
                        (void)rec(sys_log_fd, ERROR_SIGN,
                                  "write() error : %s (%s %d)\n",
                                  strerror(errno), __FILE__, __LINE__);
                     }
                  }
               }
            }
         }
         errno = 0;
      }

      if (errno)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not readdir() %s : %s (%s %d)\n",
                   error_dir, strerror(errno), __FILE__, __LINE__);
      }
      if (closedir(dp) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not close directory %s : %s (%s %d)\n",
                   error_dir, strerror(errno), __FILE__, __LINE__);
      }
      *(work_ptr - 1) = '\0';
      (void)rec_rmdir(error_dir);
   }

   return;
}
#endif /* _DELETE_LOG */

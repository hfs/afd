/*
 *  save_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   save_files - saves files from user directory
 **
 ** SYNOPSIS
 **   int save_files(char                   *src_path,
 **                  char                   *dest_path,
 **                  struct directory_entry *p_de,
 **                  int                    pos_in_fm,
 **                  int                    no_of_files,
 **                  char                   link_flag
 **                  int                    time_job)
 **
 ** DESCRIPTION
 **   When the queue has been stopped for a host, this function saves
 **   all files in the user directory into the directory .<hostname>
 **   so that no files are lost for this host. This function is also
 **   used to save time jobs.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when all files have been saved. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.03.1996 H.Kiehl Created
 **   15.09.1997 H.Kiehl Remove files when they already do exist.
 **   17.10.1997 H.Kiehl When pool is a different file system files
 **                      should be copied.
 **   18.10.1997 H.Kiehl Introduced inverse filtering.
 **   09.06.2002 H.Kiehl Return number of files and size that have been
 **                      saved.
 **   28.01.2002 H.Kiehl Forgot to update files and bytes received and
 **                      reverted above change.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include <string.h>                /* strcmp(), strerror(), strcpy(),    */
                                   /* strcat(), strlen()                 */
#include <sys/types.h>
#include <sys/stat.h>              /* stat(), S_ISDIR()                  */
#include <fcntl.h>                 /* S_IRUSR, ...                       */
#include <unistd.h>                /* link(), mkdir(), unlink()          */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int                        fra_fd,
                                  sys_log_fd;
#ifndef _WITH_PTHREAD
extern off_t                      *file_size_pool;
extern char                       **file_name_pool;
#endif
extern struct fileretrieve_status *fra;


/*########################### save_files() ##############################*/
int
save_files(char                   *src_path,
           char                   *dest_path,
#ifdef _WITH_PTHREAD
           off_t                  *file_size_pool,
           char                   **file_name_pool,
#endif
           struct directory_entry *p_de,
           int                    pos_in_fm,
           int                    no_of_files,
           char                   link_flag,
           int                    time_job)
{
   register int i,
                j;
   int          files_deleted = 0,
                files_saved = 0,
                retstat;
   off_t        file_size_deleted = 0,
                file_size_saved = 0;
   char         *p_src,
                *p_dest;
   struct stat  stat_buf;

   if ((stat(dest_path, &stat_buf) < 0) || (S_ISDIR(stat_buf.st_mode) == 0))
   {
      /*
       * Only the AFD may read and write in this directory!
       */
      if (mkdir(dest_path, (S_IRUSR | S_IWUSR | S_IXUSR)) < 0)
      {
         if (errno != EEXIST)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Could not mkdir() <%s> to save files : %s (%s %d)\n",
                      dest_path, strerror(errno), __FILE__, __LINE__);
            errno = 0;
            return(INCORRECT);
         }

         /*
          * For now lets assume that another process has created
          * this directory just a little bit faster then this
          * process.
          */
      }
   }

   p_src = src_path + strlen(src_path);
   p_dest = dest_path + strlen(dest_path);
   *p_dest++ = '/';

   for (i = 0; i < no_of_files; i++)
   {
      for (j = 0; j < p_de->fme[pos_in_fm].nfm; j++)
      {
         /*
          * Actually we could just read the source directory
          * and rename all files to the new directory.
          * This however involves several system calls (opendir(),
          * readdir(), closedir()), ie high system load. This
          * we hopefully reduce by using the array file_name_pool
          * and pmatch() to get the names we need. Let's see
          * how things work.
          */
         if ((retstat = pmatch(p_de->fme[pos_in_fm].file_mask[j],
                               file_name_pool[i])) == 0)
         {
            (void)strcpy(p_src, file_name_pool[i]);
            (void)strcpy(p_dest, file_name_pool[i]);

            if (link_flag & IN_SAME_FILESYSTEM)
            {
               if (p_de->flag & RENAME_ONE_JOB_ONLY)
               {
                  if (stat(dest_path, &stat_buf) != -1)
                  {
                     if (unlink(dest_path) == -1)
                     {
                        (void)rec(sys_log_fd, WARN_SIGN,
                                  "Failed to unlink() file <%s> : %s (%s %d)\n",
                                  dest_path, strerror(errno),
                                  __FILE__, __LINE__);
                     }
                     else
                     {
                        files_deleted++;
                        file_size_deleted += stat_buf.st_size;
                     }
                  }
                  if ((retstat = rename(src_path, dest_path)) == -1)
                  {
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "Failed to rename() file <%s> to <%s> (%s %d)\n",
                               src_path, dest_path, __FILE__, __LINE__);
                     errno = 0;
                  }
                  else
                  {
                     files_saved++;
                     file_size_saved += file_size_pool[i];
                  }
               }
               else
               {
                  if ((retstat = link(src_path, dest_path)) == -1)
                  {
                     if (errno == EEXIST)
                     {
                        off_t del_file_size = 0;

                        if (stat(dest_path, &stat_buf) == -1)
                        {
                           (void)rec(sys_log_fd, WARN_SIGN,
                                     "Failed to stat() %s : %s (%s %d)\n",
                                     dest_path, strerror(errno),
                                     __FILE__, __LINE__);
                        }
                        else
                        {
                           del_file_size = stat_buf.st_size;
                        }

                        /*
                         * A file with the same name already exists. Remove
                         * this file and try to link again.
                         */
                        if (unlink(dest_path) == -1)
                        {
                           (void)rec(sys_log_fd, WARN_SIGN,
                                     "Failed to unlink() file <%s> : %s (%s %d)\n",
                                     dest_path, strerror(errno),
                                     __FILE__, __LINE__);
                           errno = 0;
                        }
                        else
                        {
                           files_deleted++;
                           file_size_deleted += del_file_size;
                           if ((retstat = link(src_path, dest_path)) == -1)
                           {
                              (void)rec(sys_log_fd, WARN_SIGN,
                                        "Failed to link file <%s> to <%s> : %s (%s %d)\n",
                                        src_path, dest_path, strerror(errno),
                                        __FILE__, __LINE__);
                              errno = 0;
                           }
                           else
                           {
                              files_saved++;
                              file_size_saved += file_size_pool[i];
                           }
                        }
                     }
                     else
                     {
                        (void)rec(sys_log_fd, WARN_SIGN,
                                  "Failed to link file <%s> to <%s> : %s (%s %d)\n",
                                  src_path, dest_path, strerror(errno),
                                  __FILE__, __LINE__);
                        errno = 0;
                     }
                  }
                  else
                  {
                     files_saved++;
                     file_size_saved += file_size_pool[i];
                  }
               }
            }
            else
            {
               if ((retstat = copy_file(src_path, dest_path, NULL)) < 0)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Failed to copy file <%s> to <%s> (%s %d)\n",
                            src_path, dest_path, __FILE__, __LINE__);
                  errno = 0;
               }
               else
               {
                  files_saved++;
                  file_size_saved += file_size_pool[i];
                  if (p_de->flag & RENAME_ONE_JOB_ONLY)
                  {
                     if (unlink(src_path) == -1)
                     {
                        (void)rec(sys_log_fd, WARN_SIGN,
                                  "Failed to unlink() file <%s> : %s (%s %d)\n",
                                  src_path, strerror(errno),
                                  __FILE__, __LINE__);
                        errno = 0;
                     }
                  }
               }
            }

            /* No need to test any further filters. */
            break;
         }
         else if (retstat == 1)
              {
                 /*
                  * This file is definitely NOT wanted, no matter what the
                  * following filters say.
                  */
                 break;
              }
      }
   }
   *(p_dest - 1) = '\0';
   *p_src = '\0';

   if (time_job == NO)
   {
      int   files_changed;
      off_t size_changed;

      files_changed = files_saved - files_deleted;
      size_changed = file_size_saved - file_size_deleted;

      if ((files_changed != 0) || (size_changed != 0))
      {
         lock_region_w(fra_fd,
                       (char *)&fra[p_de->fra_pos].files_queued - (char *)fra);
         if ((fra[p_de->fra_pos].dir_flag & FILES_IN_QUEUE) == 0)
         {
            fra[p_de->fra_pos].dir_flag ^= FILES_IN_QUEUE;
         }
         fra[p_de->fra_pos].files_queued += files_changed;
         fra[p_de->fra_pos].bytes_in_queue += size_changed;
         unlock_region(fra_fd,
                       (char *)&fra[p_de->fra_pos].files_queued - (char *)fra);
      }
   }
   fra[p_de->fra_pos].files_received -= files_saved;
   fra[p_de->fra_pos].bytes_received -= file_size_saved;

   return(SUCCESS);
}

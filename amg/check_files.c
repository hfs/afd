/*
 *  check_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_files - moves all files that are to be distributed
 **                 to a temporary directory
 **
 ** SYNOPSIS
 **   int check_files(struct directory_entry *p_de,
 **                   char                   *src_file_path,
 **                   char                   *afd_file_path,
 **                   char                   *tmp_file_dir,
 **                   int                    count_files,
 **                   time_t                 current_time,
 **                   off_t                  *total_file_size)
 **
 ** DESCRIPTION
 **   The function check_files() searches the directory 'p_de->dir'
 **   for files 'p_de->fme[].file_mask[]'. If it finds them they get
 **   moved to a unique directory of the following format:
 **            nnnnnnnnnn_llll
 **                |       |
 **                |       +-------> Counter which is set to zero
 **                |                 after each second. This
 **                |                 ensures that no message can
 **                |                 have the same name in one
 **                |                 second.
 **                +---------------> creation time in seconds
 **
 **   check_files() will only copy 'max_copied_files' (100) or
 **   'max_copied_file_size' Bytes. If there are more files or the
 **   size limit has been reached, these will be handled with the next
 **   call to check_files(). Otherwise it will take too long before files
 **   get transmitted and other directories get their turn. Since local
 **   copying is always faster than copying something to a remote machine,
 **   the AMG will have finished its task before the FD has finished.
 **
 ** RETURN VALUES
 **   Returns the number of files that have been copied and the directory
 **   where the files have been moved in 'tmp_file_dir'. If it fails it
 **   will return INCORRECT.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.10.1995 H.Kiehl Created
 **   18.10.1997 H.Kiehl Introduced inverse filtering.
 **   04.03.1999 H.Kiehl When copying files don't only limit the number
 **                      of files, also limit the size that may be copied.
 **   05.06.1999 H.Kiehl Option to check for last character in file.
 **   25.04.2001 H.Kiehl Check if we need to move or copy a file.
 **   27.08.2001 H.Kiehl Extra parameter to enable or disable counting
 **                      of files on input, so we don't count files twice
 **                      when queue is opened.
 **   03.05.2002 H.Kiehl Count total number of files and bytes in directory
 **                      regardless if they are to be retrieved or not.
 **   12.07.2002 H.Kiehl Check if we can already delete any old files that
 **                      have no distribution rule.
 **   13.07.2003 H.Kiehl Store mtime of file.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* fprintf(), sprintf()               */
#include <string.h>                /* strcmp(), strcpy(), strerror()     */
#include <stdlib.h>                /* exit()                             */
#include <time.h>                  /* time()                             */
#include <unistd.h>                /* write(), read(), lseek(), close()  */
#ifdef _WITH_PTHREAD
#include <pthread.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>              /* stat(), S_ISREG()                  */
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

extern int                        counter_fd,
                                  fra_fd, /* Needed by ABS_REDUCE_QUEUE()*/
                                  max_copied_files,
                                  sys_log_fd;
extern off_t                      max_copied_file_size;
extern uid_t                      afd_uid;
extern gid_t                      afd_gid;
#ifdef _INPUT_LOG
extern int                        il_fd;
extern unsigned int               *il_dir_number;
extern size_t                     il_size;
extern off_t                      *il_file_size;
extern char                       *il_file_name,
                                  *il_data;
#endif /* _INPUT_LOG */
#ifndef _WITH_PTHREAD
extern off_t                      *file_size_pool;
extern time_t                     *file_mtime_pool;
extern char                       **file_name_pool;
#endif
#ifdef _WITH_PTHREAD
extern pthread_mutex_t            fsa_mutex;
#endif
extern struct fileretrieve_status *fra;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif

/* Local function prototype */
static int                        get_last_char(char *, off_t);


/*########################### check_files() #############################*/
int
check_files(struct directory_entry *p_de,
            char                   *src_file_path,
            char                   *afd_file_path, /* Full path of the   */
                                                   /* AFD file directory.*/
            char                   *tmp_file_dir,  /* Return directory   */
                                                   /* where files are    */
                                                   /* stored.            */
            int                    count_files,
            time_t                 current_time,
#ifdef _WITH_PTHREAD
            off_t                  *file_size_pool,
            time_t                 *file_mtime_pool,
            char                   **file_name_pool,
#endif
            off_t                  *total_file_size)
{
   int            files_copied = 0,
                  files_in_dir = 0,
                  i,
                  ret;
   off_t          bytes_in_dir = 0;
   char           fullname[MAX_PATH_LENGTH],
                  *ptr = NULL,
                  *work_ptr;
   struct stat    stat_buf;
   DIR            *dp;
   struct dirent  *p_dir;
#ifdef _INPUT_LOG
   size_t         il_real_size;
#endif

   (void)strcpy(fullname, src_file_path);
   work_ptr = fullname + strlen(fullname);
   *work_ptr++ = '/';
   *work_ptr = '\0';
   tmp_file_dir[0] = '\0';

   if ((dp = opendir(fullname)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Can't opendir() <%s> : %s (%s %d)\n",
                fullname, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   if (p_de->flag & ALL_FILES)
   {
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            continue;
         }

         (void)strcpy(work_ptr, p_dir->d_name);
         if (stat(fullname, &stat_buf) < 0)
         {
            if (errno == ENOENT)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Can't stat() file <%s> : %s (%s %d)\n",
                         fullname, strerror(errno), __FILE__, __LINE__);
            }
            else
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Can't stat() file <%s> : %s (%s %d)\n",
                         fullname, strerror(errno), __FILE__, __LINE__);
            }
            continue;
         }

         /* Sure it is a normal file? */
         if (S_ISREG(stat_buf.st_mode))
         {
            files_in_dir++;
            bytes_in_dir += stat_buf.st_size;
#ifdef _POSIX_SAVED_IDS
            if (((stat_buf.st_mode & S_IROTH) ||
                ((stat_buf.st_gid == afd_gid) && (stat_buf.st_mode & S_IRGRP)) ||
                ((stat_buf.st_uid == afd_uid) && (stat_buf.st_mode & S_IRUSR))))
#else
            if ((eaccess(fullname, R_OK) == 0))
#endif
            {
               if ((fra[p_de->fra_pos].end_character == -1) ||
                   (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.st_size)))
               {
                  if (tmp_file_dir[0] == '\0')
                  {
                     (void)strcpy(tmp_file_dir, afd_file_path);
                     (void)strcat(tmp_file_dir, AFD_TMP_DIR);
                     ptr = tmp_file_dir + strlen(tmp_file_dir);
                     *(ptr++) = '/';
                     *ptr = '\0';

                     /* Create a unique name */
                     if (create_name(tmp_file_dir, NO_PRIORITY, current_time,
                                     p_de->dir_no, NULL, ptr) < 0)
                     {
                        if (errno == ENOSPC)
                        {
                           (void)rec(sys_log_fd, ERROR_SIGN,
                                     "DISK FULL!!! Will retry in %d second interval. (%s %d)\n",
                                     DISK_FULL_RESCAN_TIME,
                                     __FILE__, __LINE__);

                           while (errno == ENOSPC)
                           {
                              (void)sleep(DISK_FULL_RESCAN_TIME);
                              errno = 0;
                              if (create_name(tmp_file_dir, NO_PRIORITY,
                                              current_time,
                                              p_de->dir_no, NULL, ptr) < 0)
                              {
                                 if (errno != ENOSPC)
                                 {
                                    (void)rec(sys_log_fd, FATAL_SIGN,
                                              "Failed to create a unique name : %s (%s %d)\n",
                                              strerror(errno),
                                              __FILE__, __LINE__);
                                    exit(INCORRECT);
                                 }
                              }
                           }
                           (void)rec(sys_log_fd, INFO_SIGN,
                                     "Continuing after disk was full. (%s %d)\n",
                                     __FILE__, __LINE__);

                           /*
                            * If the disk is full, lets stop copying/moving
                            * files so we can send data as quickly as possible
                            * to get more free disk space.
                            */
                           break;
                        }
                        else
                        {
                           (void)rec(sys_log_fd, FATAL_SIGN,
                                     "Failed to create a unique name : %s (%s %d)\n",
                                     strerror(errno), __FILE__, __LINE__);
                           exit(INCORRECT);
                        }
                     }

                     while (*ptr != '\0')
                     {
                        ptr++;
                     }
                     *(ptr++) = '/';
                     *ptr = '\0';
                  } /* if (tmp_file_dir[0] == '\0') */

                  /* Generate name for the new file */
                  (void)strcpy(ptr, p_dir->d_name);

                  if ((fra[p_de->fra_pos].remove == YES) ||
                      (count_files == NO) ||  /* Paused directory. */
                      (fra[p_de->fra_pos].protocol != LOC))
                  {
                     if (p_de->flag & IN_SAME_FILESYSTEM)
                     {
                        ret = move_file(fullname, tmp_file_dir);
                     }
                     else
                     {
                        ret = copy_file(fullname, tmp_file_dir, &stat_buf);
                        if (unlink(fullname) == -1)
                        {
                           (void)rec(sys_log_fd, ERROR_SIGN,
                                     "Failed to unlink() file <%s> : %s (%s %d)\n",
                                     fullname, strerror(errno),
                                     __FILE__, __LINE__);
                        }
                     }
                  }
                  else
                  {
                     /* Leave original files in place. */
                     ret = copy_file(fullname, tmp_file_dir, &stat_buf);
                  }
                  if (ret != SUCCESS)
                  {
                     (void)rec(sys_log_fd, ERROR_SIGN,
                               "Failed to move/copy file. (%s %d)\n",
                               __FILE__, __LINE__);
                  }
                  else
                  {
#ifdef _INPUT_LOG
                     if (count_files == YES)
                     {
                        /* Log the file name in the input log. */
                        (void)strcpy(il_file_name, p_dir->d_name);
                        *il_file_size = stat_buf.st_size;
                        *il_dir_number = p_de->dir_no;
                        il_real_size = strlen(p_dir->d_name) + il_size;
                        if (write(il_fd, il_data, il_real_size) != il_real_size)
                        {
                           (void)rec(sys_log_fd, ERROR_SIGN,
                                     "write() error : %s (%s %d)\n",
                                     strerror(errno), __FILE__, __LINE__);

                           /*
                            * Since the input log is not critical, no
                            * need to exit here.
                            */
                        }
                     }
#endif

                     /* Store file name of file that has just been  */
                     /* moved. So one does not always have to walk  */
                     /* with the directory pointer through all      */
                     /* files every time we want to know what files */
                     /* are available. Hope this will reduce the    */
                     /* system time of the process dir_check.       */
                     (void)strcpy(file_name_pool[files_copied], p_dir->d_name);
                     file_mtime_pool[files_copied] = stat_buf.st_mtime;
                     file_size_pool[files_copied] = stat_buf.st_size;

                     *total_file_size += stat_buf.st_size;
                     if ((++files_copied >= max_copied_files) ||
                         (*total_file_size >= max_copied_file_size))
                     {
                        break;
                     }
                  }
               }
               else
               {
                  if (fra[p_de->fra_pos].end_character != -1)
                  {
                     p_de->search_time -= 5;
                  }
               }
            }
         } /* if (S_ISREG(stat_buf.st_mode)) */
      } /* while ((p_dir = readdir(dp)) != NULL) */
   }
   else
   {
      int j;

      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            continue;
         }

         (void)strcpy(work_ptr, p_dir->d_name);
         if (stat(fullname, &stat_buf) < 0)
         {
            if (errno != ENOENT)
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Can't access file <%s> : %s (%s %d)\n",
                         fullname, strerror(errno), __FILE__, __LINE__);
            }
            continue;
         }

         /* Sure it is a normal file? */
         if (S_ISREG(stat_buf.st_mode))
         {
            files_in_dir++;
            bytes_in_dir += stat_buf.st_size;
#ifdef _POSIX_SAVED_IDS
            if (((stat_buf.st_mode & S_IROTH) ||
                ((stat_buf.st_gid == afd_gid) && (stat_buf.st_mode & S_IRGRP)) ||
                ((stat_buf.st_uid == afd_uid) && (stat_buf.st_mode & S_IRUSR))))
#else
            if ((eaccess(fullname, R_OK) == 0))
#endif
            {
               int gotcha = NO;

               /* Filter out only those files we need for this directory */
               for (i = 0; i < p_de->nfg; i++)
               {
                  for (j = 0; ((j < p_de->fme[i].nfm) && (i < p_de->nfg)); j++)
                  {
                     if ((ret = pmatch(p_de->fme[i].file_mask[j], p_dir->d_name)) == 0)
                     {
                        if ((fra[p_de->fra_pos].end_character == -1) ||
                            (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.st_size)))
                        {
                           if (tmp_file_dir[0] == '\0')
                           {
                              (void)strcpy(tmp_file_dir, afd_file_path);
                              (void)strcat(tmp_file_dir, AFD_TMP_DIR);
                              ptr = tmp_file_dir + strlen(tmp_file_dir);
                              *(ptr++) = '/';
                              *ptr = '\0';

                              /* Create a unique name */
                              if (create_name(tmp_file_dir, NO_PRIORITY,
                                              current_time,
                                              p_de->dir_no, NULL, ptr) < 0)
                              {
                                 if (errno == ENOSPC)
                                 {
                                    (void)rec(sys_log_fd, ERROR_SIGN,
                                              "DISK FULL!!! Will retry in %d second interval. (%s %d)\n",
                                              DISK_FULL_RESCAN_TIME,
                                              __FILE__, __LINE__);

                                    while (errno == ENOSPC)
                                    {
                                       (void)sleep(DISK_FULL_RESCAN_TIME);
                                       errno = 0;
                                       if (create_name(tmp_file_dir,
                                                       NO_PRIORITY,
                                                       current_time,
                                                       p_de->dir_no,
                                                       NULL, ptr) < 0)
                                       {
                                          if (errno != ENOSPC)
                                          {
                                             (void)rec(sys_log_fd, FATAL_SIGN,
                                                       "Failed to create a unique name in %s : %s (%s %d)\n",
                                                       tmp_file_dir,
                                                       strerror(errno),
                                                       __FILE__, __LINE__);
                                             exit(INCORRECT);
                                          }
                                       }
                                    }
                                    (void)rec(sys_log_fd, INFO_SIGN,
                                              "Continuing after disk was full. (%s %d)\n",
                                              __FILE__, __LINE__);

                                    /*
                                     * If the disk is full, lets stop copying/moving
                                     * files so we can send data as quickly as
                                     * possible to get more free disk space.
                                     */
                                    goto done;
                                 }
                                 else
                                 {
                                    (void)rec(sys_log_fd, FATAL_SIGN,
                                              "Failed to create a unique name : %s (%s %d)\n",
                                              strerror(errno), __FILE__, __LINE__);
                                    exit(INCORRECT);
                                 }
                              }

                              while (*ptr != '\0')
                              {
                                ptr++;
                              }
                              *(ptr++) = '/';
                              *ptr = '\0';
                           }

                           /* Generate name for the new file. */
                           (void)strcpy(ptr, p_dir->d_name);
                           if ((fra[p_de->fra_pos].remove == YES) ||
                               (count_files == NO) ||  /* Paused directory. */
                               (fra[p_de->fra_pos].protocol != LOC))
                           {
                              if (p_de->flag & IN_SAME_FILESYSTEM)
                              {
                                 ret = move_file(fullname, tmp_file_dir);
                              }
                              else
                              {
                                 ret = copy_file(fullname, tmp_file_dir, &stat_buf);
                                 if (unlink(fullname) == -1)
                                 {
                                    (void)rec(sys_log_fd, ERROR_SIGN,
                                              "Failed to unlink() file <%s> : %s (%s %d)\n",
                                              fullname, strerror(errno),
                                              __FILE__, __LINE__);
                                 }
                              }
                           }
                           else
                           {
                              /* Leave original files in place. */
                              ret = copy_file(fullname, tmp_file_dir, &stat_buf);
                           }
                           if (ret != SUCCESS)
                           {
                              (void)rec(sys_log_fd, WARN_SIGN,
                                        "Failed to move/copy file <%s> to <%s>. (%s %d)\n",
                                        fullname, tmp_file_dir, __FILE__, __LINE__);
                           }
                           else
                           {
#ifdef _INPUT_LOG
                              if (count_files == YES)
                              {
                                 /* Log the file name in the input log. */
                                 (void)strcpy(il_file_name, p_dir->d_name);
                                 *il_file_size = stat_buf.st_size;
                                 *il_dir_number = p_de->dir_no;
                                 il_real_size = strlen(p_dir->d_name) + il_size;
                                 if (write(il_fd, il_data, il_real_size) != il_real_size)
                                 {
                                    (void)rec(sys_log_fd, ERROR_SIGN,
                                              "write() error : %s (%s %d)\n",
                                              strerror(errno), __FILE__, __LINE__);

                                    /*
                                     * Since the input log is not critical, no need to
                                     * exit here.
                                     */
                                 }
                              }
#endif

                              /* Store file name of file that has just been  */
                              /* moved. So one does not always have to walk  */
                              /* with the directory pointer through all      */
                              /* files every time we want to know what files */
                              /* are available. Hope this will reduce the    */
                              /* system time of the process dir_check.       */
                              (void)strcpy(file_name_pool[files_copied], p_dir->d_name);
                              file_mtime_pool[files_copied] = stat_buf.st_mtime;
                              file_size_pool[files_copied] = stat_buf.st_size;

                              *total_file_size += stat_buf.st_size;
                              if ((++files_copied >= max_copied_files) ||
                               (*total_file_size >= max_copied_file_size))
                              {
                                 goto done;
                              }
                              gotcha = YES;
                           }

                           /*
                            * Since the file is in the file pool, it is not
                            * necessary to test all other masks.
                            */
                           i = p_de->nfg;
                        }
                        else
                        {
                           if (fra[p_de->fra_pos].end_character != -1)
                           {
                              p_de->search_time -= 5;
                           }
                        }
                     } /* if file in file mask */
                     else if (ret == 1)
                          {
                             /*
                              * This file is definitely NOT wanted, for this
                              * file group! However, the next file group
                              * might state that it does want this file. So
                              * only ignore all entries for this file group!
                              */
                             break;
                          }
                  } /* for (j = 0; ((j < p_de->fme[i].nfm) && (i < p_de->nfg)); j++) */
               } /* for (i = 0; i < p_de->nfg; i++) */

               if ((gotcha == NO) &&
                   (fra[p_de->fra_pos].delete_files_flag & UNKNOWN_FILES))
               {
                  time_t diff_time;

                  diff_time = current_time - stat_buf.st_mtime;
                  if (diff_time > fra[p_de->fra_pos].unknown_file_time)
                  {
                     if (unlink(fullname) == -1)
                     {
                        (void)rec(sys_log_fd, WARN_SIGN,
                                  "Failed to unlink() %s : %s (%s %d)\n",
                                  fullname, strerror(errno), __FILE__, __LINE__);
                     }
                     else
                     {
#ifdef _DELETE_LOG
                        size_t dl_real_size;

                        (void)strcpy(dl.file_name, p_dir->d_name);
                        (void)sprintf(dl.host_name, "%-*s %x",
                                      MAX_HOSTNAME_LENGTH, "-", AGE_INPUT);
                        *dl.file_size = stat_buf.st_size;
                        *dl.job_number = p_de->dir_no;
                        *dl.file_name_length = strlen(p_dir->d_name);
                        dl_real_size = *dl.file_name_length + dl.size +
                                       sprintf((dl.file_name + *dl.file_name_length + 1),
                                       "dir_check() >%ld", diff_time);
                        if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                        {
                           (void)rec(sys_log_fd, ERROR_SIGN,
                                     "write() error : %s (%s %d)\n",
                                     strerror(errno), __FILE__, __LINE__);
                        }
#endif /* _DELETE_LOG */
                        files_in_dir--;
                        bytes_in_dir -= stat_buf.st_size;
                     }
                  }
               }
            }
         } /* if (S_ISREG(stat_buf.st_mode)) */
      } /* while ((p_dir = readdir(dp)) != NULL) */
   }

done:

   /* When using readdir() it can happen that it always returns     */
   /* NULL, due to some error. We want to know if this is the case. */
   if (errno == EBADF)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "readdir() error from <%s> : %s (%s %d)\n",
                fullname, strerror(errno), __FILE__, __LINE__);
   }

   /* So that we return only the directory name where */
   /* the files have been stored.                    */
   if (ptr != NULL)
   {
      *ptr = '\0';
   }

   /* Don't forget to close the directory */
   if (closedir(dp) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not close directory <%s> : %s (%s %d)\n",
                src_file_path, strerror(errno), __FILE__, __LINE__);
   }

#ifdef _WITH_PTHREAD
   if ((ret = pthread_mutex_lock(&fsa_mutex)) != 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "pthread_mutex_lock() error : %s (%s %d)\n",
                strerror(ret), __FILE__, __LINE__);
   }
#endif
   if ((files_copied >= max_copied_files) ||
       (*total_file_size >= max_copied_file_size))
   {
      if (count_files == YES)
      {
         if (fra[p_de->fra_pos].files_in_dir < files_in_dir)
         {
            fra[p_de->fra_pos].files_in_dir = files_in_dir;
         }
         if (fra[p_de->fra_pos].bytes_in_dir < bytes_in_dir)
         {
            fra[p_de->fra_pos].bytes_in_dir = bytes_in_dir;
         }
      }
      if ((fra[p_de->fra_pos].dir_flag & MAX_COPIED) == 0)
      {
         fra[p_de->fra_pos].dir_flag ^= MAX_COPIED;
      }
   }
   else
   {
      if (count_files == YES)
      {
         fra[p_de->fra_pos].files_in_dir = files_in_dir;
         fra[p_de->fra_pos].bytes_in_dir = bytes_in_dir;
      }
      if ((fra[p_de->fra_pos].dir_flag & MAX_COPIED))
      {
         fra[p_de->fra_pos].dir_flag ^= MAX_COPIED;
      }
   }

   if (files_copied > 0)
   {
      if (count_files == YES)
      {
         fra[p_de->fra_pos].files_received += files_copied;
         fra[p_de->fra_pos].bytes_received += *total_file_size;
         fra[p_de->fra_pos].last_retrieval = current_time;
         receive_log(INFO_SIGN, NULL, 0, current_time,
                     "Received %d files with %lu Bytes.",
                     files_copied, *total_file_size);
      }
      else
      {
         ABS_REDUCE_QUEUE(p_de->fra_pos, files_copied, *total_file_size);
      }
   }
#ifdef _WITH_PTHREAD
   if ((ret = pthread_mutex_unlock(&fsa_mutex)) != 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "pthread_mutex_unlock() error : %s (%s %d)\n",
                strerror(ret), __FILE__, __LINE__);
   }
#endif

   return(files_copied);
}


/*+++++++++++++++++++++++++++ get_last_char() +++++++++++++++++++++++++++*/
static int
get_last_char(char *file_name, off_t file_size)
{
   int fd,
       last_char = -1;

   if (file_size > 0)
   {
      if ((fd = open(file_name, O_RDONLY)) != -1)
      {
         if (lseek(fd, file_size - 1, SEEK_SET) != -1)
         {
            char tmp_char;

            if (read(fd, &tmp_char, 1) == 1)
            {
               last_char = (int)tmp_char;
            }
            else
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Failed to read() last character from %s : %s (%s %d)\n",
                         file_name, strerror(errno), __FILE__, __LINE__);
            }
         }
         else
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Failed to lseek() in %s : %s (%s %d)\n",
                      file_name, strerror(errno), __FILE__, __LINE__);
         }
         if (close(fd) == -1)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Failed to close() %s : %s (%s %d)\n",
                      file_name, strerror(errno), __FILE__, __LINE__);
         }
      }
   }

   return(last_char);
}

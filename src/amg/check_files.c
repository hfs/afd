/*
 *  check_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   15.07.2002 H.Kiehl Option to ignore files which have a certain size.
 **   06.02.2003 H.Kiehl Put max_copied_files and max_copied_file_size into
 **                      FRA.
 **   13.07.2003 H.Kiehl Store mtime of file.
 **   16.08.2003 H.Kiehl Added options to start using the given directory
 **                      when a certain file has arrived and/or a certain
 **                      number of files and/or the files in the directory
 **                      have a certain size.
 **   02.09.2004 H.Kiehl Added option "igore file time".
 **   17.04.2005 H.Kiehl When it is a paused directory do not wait for
 **                      a certain file, file size or make any date check.
 **   01.06.2005 H.Kiehl Build in check for duplicate files.
 **   15.03.2006 H.Kiehl When checking if we have sufficient file permissions
 **                      we must check supplementary groups as well.
 **   29.03.2006 H.Kiehl Support for pattern matching with expanding
 **                      filters.
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
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"

extern int                        amg_counter_fd,
                                  fra_fd; /* Needed by ABS_REDUCE_QUEUE()*/
#ifdef _POSIX_SAVED_IDS
extern int                        no_of_sgids;
extern uid_t                      afd_uid;
extern gid_t                      afd_gid,
                                  *afd_sgids;
#endif
#ifdef _INPUT_LOG
extern int                        il_fd,
                                  *il_unique_number;
extern unsigned int               *il_dir_number;
extern size_t                     il_size;
extern off_t                      *il_file_size;
extern char                       *il_file_name,
                                  *il_data;
#endif /* _INPUT_LOG */
#ifdef WITH_DUP_CHECK
extern char                       *p_work_dir;
#endif
#ifndef _WITH_PTHREAD
extern unsigned int               max_file_buffer;
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
#ifdef _POSIX_SAVED_IDS
static int                        check_sgids(gid_t);
#endif


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
            int                    *unique_number,
            time_t                 current_time,
#ifdef _WITH_PTHREAD
            off_t                  *file_size_pool,
            time_t                 *file_mtime_pool,
            char                   **file_name_pool,
#endif
            off_t                  *total_file_size)
{
   int           files_copied = 0,
                 files_in_dir = 0,
                 i,
                 ret;
   unsigned int  split_job_counter = 0;
   off_t         bytes_in_dir = 0;
   time_t        diff_time,
                 pmatch_time;
   char          fullname[MAX_PATH_LENGTH],
                 *ptr = NULL,
                 *work_ptr;
   struct stat   stat_buf;
   DIR           *dp;
   struct dirent *p_dir;
#ifdef _INPUT_LOG
   size_t        il_real_size;
#endif

   (void)strcpy(fullname, src_file_path);
   work_ptr = fullname + strlen(fullname);
   *work_ptr++ = '/';
   *work_ptr = '\0';

   /*
    * Check if this is the special case when we have a dummy remote dir
    * and its queue is stopped. In this case it is just necessary to
    * move the files to the paused directory which is in tmp_file_dir
    * or visa versa.
    */
   if (afd_file_path != NULL)
   {
      tmp_file_dir[0] = '\0';
   }
   else
   {
      if (count_files == PAUSED_REMOTE)
      {
         (void)strcpy(tmp_file_dir, p_de->paused_dir);
         ptr = tmp_file_dir + strlen(tmp_file_dir);
         *ptr = '/';
         ptr++;
         *ptr = '\0';

         /* If remote paused directory does not exist, create it. */
         if ((stat(tmp_file_dir, &stat_buf) < 0) ||
             (S_ISDIR(stat_buf.st_mode) == 0))
         {
            /*
             * Only the AFD may read and write in this directory!
             */
#ifdef GROUP_CAN_WRITE
            if (mkdir(tmp_file_dir, (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP)) < 0)
#else
            if (mkdir(tmp_file_dir, (S_IRUSR | S_IWUSR | S_IXUSR)) < 0)
#endif
            {
               if (errno != EEXIST)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not mkdir() <%s> to save files : %s",
                             tmp_file_dir, strerror(errno));
                  errno = 0;
                  return(INCORRECT);
               }
            }
         }
      }
      else
      {
         (void)strcpy(tmp_file_dir, p_de->dir);
         ptr = tmp_file_dir + strlen(tmp_file_dir);
         *ptr = '/';
         ptr++;
      }
   }

   if ((dp = opendir(fullname)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Can't opendir() `%s' : %s", fullname, strerror(errno));
      return(INCORRECT);
   }

   /*
    * See if we have to wait for a certain file name before we
    * can check this directory.
    */
   if ((fra[p_de->fra_pos].wait_for_filename[0] != '\0') &&
       (count_files != NO))
   {
      int          gotcha = NO;
      unsigned int dummy_files_in_dir = 0;
      off_t        dummy_bytes_in_dir = 0;

      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] != '.')
         {
            (void)strcpy(work_ptr, p_dir->d_name);
            if (stat(fullname, &stat_buf) < 0)
            {
               if (errno != ENOENT)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Can't stat() file `%s' : %s",
                             fullname, strerror(errno));
               }
               continue;
            }

            if (fra[p_de->fra_pos].ignore_file_time != 0)
            {
               diff_time = current_time - stat_buf.st_mtime;
            }

            /* Sure it is a normal file? */
            if (S_ISREG(stat_buf.st_mode))
            {
               dummy_files_in_dir++;
               dummy_bytes_in_dir += stat_buf.st_size;
               if (((fra[p_de->fra_pos].ignore_size == 0) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
                     (fra[p_de->fra_pos].ignore_size == stat_buf.st_size)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign == ISIZE_LESS_THEN) &&
                     (fra[p_de->fra_pos].ignore_size < stat_buf.st_size)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign == ISIZE_GREATER_THEN) &&
                     (fra[p_de->fra_pos].ignore_size > stat_buf.st_size))) &&
                   ((fra[p_de->fra_pos].ignore_file_time == 0) ||
                     ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                      (fra[p_de->fra_pos].ignore_file_time == diff_time)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign == IFTIME_LESS_THEN) &&
                     (fra[p_de->fra_pos].ignore_file_time < diff_time)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign == IFTIME_GREATER_THEN) &&
                     (fra[p_de->fra_pos].ignore_file_time > diff_time))))
               {
#ifdef _POSIX_SAVED_IDS
                  if ((stat_buf.st_mode & S_IROTH) ||
                      ((stat_buf.st_gid == afd_gid) && (stat_buf.st_mode & S_IRGRP)) ||
                      ((stat_buf.st_uid == afd_uid) && (stat_buf.st_mode & S_IRUSR)) ||
                      ((stat_buf.st_mode & S_IRGRP) && (no_of_sgids > 0) &&
                       (check_sgids(stat_buf.st_gid) == YES)))
#else
                  if ((eaccess(fullname, R_OK) == 0))
#endif
                  {
                     if ((ret = pmatch(fra[p_de->fra_pos].wait_for_filename, p_dir->d_name, &current_time)) == 0)
                     {
                        if ((fra[p_de->fra_pos].end_character == -1) ||
                            (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.st_size)))
                        {
                           gotcha = YES;
                        }
                        else
                        {
                           if (fra[p_de->fra_pos].end_character != -1)
                           {
                              p_de->search_time -= 5;
                           }
                        }
                        break;
                     } /* if file in file mask */
                  }
               }
            }
         }
      } /* while ((p_dir = readdir(dp)) != NULL) */

      if (gotcha == NO)
      {
         files_in_dir = dummy_files_in_dir;
         bytes_in_dir = dummy_bytes_in_dir;
         goto done;
      }
      else
      {
         rewinddir(dp);
      }
   }

   /*
    * See if we have to wait for a certain number of files or
    * total file size before we can check this directory.
    */
   if (((fra[p_de->fra_pos].accumulate != 0) ||
        (fra[p_de->fra_pos].accumulate_size != 0)) &&
       (count_files != NO))
   {
      int          gotcha = NO;
      unsigned int accumulate = 0,
                   dummy_files_in_dir = 0;
      off_t        accumulate_size = 0,
                   dummy_bytes_in_dir = 0;

      while (((p_dir = readdir(dp)) != NULL) && (gotcha == NO))
      {
         if (p_dir->d_name[0] != '.')
         {
            (void)strcpy(work_ptr, p_dir->d_name);
            if (stat(fullname, &stat_buf) < 0)
            {
               if (errno != ENOENT)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Can't stat() file `%s' : %s",
                             fullname, strerror(errno));
               }
               continue;
            }

            if (fra[p_de->fra_pos].ignore_file_time != 0)
            {
               diff_time = current_time - stat_buf.st_mtime;
            }

            /* Sure it is a normal file? */
            if (S_ISREG(stat_buf.st_mode))
            {
               dummy_files_in_dir++;
               dummy_bytes_in_dir += stat_buf.st_size;
               if (((fra[p_de->fra_pos].ignore_size == 0) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
                     (fra[p_de->fra_pos].ignore_size == stat_buf.st_size)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign == ISIZE_LESS_THEN) &&
                     (fra[p_de->fra_pos].ignore_size < stat_buf.st_size)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign == ISIZE_GREATER_THEN) &&
                     (fra[p_de->fra_pos].ignore_size > stat_buf.st_size))) &&
                   ((fra[p_de->fra_pos].ignore_file_time == 0) ||
                     ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                      (fra[p_de->fra_pos].ignore_file_time == diff_time)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign == IFTIME_LESS_THEN) &&
                     (fra[p_de->fra_pos].ignore_file_time < diff_time)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign == IFTIME_GREATER_THEN) &&
                     (fra[p_de->fra_pos].ignore_file_time > diff_time))))
               {
#ifdef _POSIX_SAVED_IDS
                  if ((stat_buf.st_mode & S_IROTH) ||
                      ((stat_buf.st_gid == afd_gid) && (stat_buf.st_mode & S_IRGRP)) ||
                      ((stat_buf.st_uid == afd_uid) && (stat_buf.st_mode & S_IRUSR)) ||
                      ((stat_buf.st_mode & S_IRGRP) && (no_of_sgids > 0) &&
                       (check_sgids(stat_buf.st_gid) == YES)))
#else
                  if ((eaccess(fullname, R_OK) == 0))
#endif
                  {
                     if (p_de->flag & ALL_FILES)
                     {
                        if ((fra[p_de->fra_pos].end_character == -1) ||
                            (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.st_size)))
                        {
                           if (fra[p_de->fra_pos].accumulate != 0)
                           {
                              accumulate++;
                           }
                           if (fra[p_de->fra_pos].accumulate_size != 0)
                           {
                              accumulate_size += stat_buf.st_size;
                           }
                           if ((accumulate >= fra[p_de->fra_pos].accumulate) ||
                               (accumulate_size >= fra[p_de->fra_pos].accumulate_size))
                           {
                              gotcha = YES;
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
                     else
                     {
                        int j;

                        /* Filter out only those files we need for this directory. */
                        for (i = 0; i < p_de->nfg; i++)
                        {
                           for (j = 0; ((j < p_de->fme[i].nfm) && (i < p_de->nfg)); j++)
                           {
                              if (p_de->paused_dir == NULL)
                              {
                                 pmatch_time = current_time;
                              }
                              else
                              {
                                 pmatch_time = stat_buf.st_mtime;
                              }
                              if ((ret = pmatch(p_de->fme[i].file_mask[j], p_dir->d_name, &pmatch_time)) == 0)
                              {
                                 if ((fra[p_de->fra_pos].end_character == -1) ||
                                     (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.st_size)))
                                 {
                                    if (fra[p_de->fra_pos].accumulate != 0)
                                    {
                                       accumulate++;
                                    }
                                    if (fra[p_de->fra_pos].accumulate_size != 0)
                                    {
                                       accumulate_size += stat_buf.st_size;
                                    }
                                    if ((accumulate >= fra[p_de->fra_pos].accumulate) ||
                                        (accumulate_size >= fra[p_de->fra_pos].accumulate_size))
                                    {
                                       gotcha = YES;
                                    }
                                 }
                                 else
                                 {
                                    if (fra[p_de->fra_pos].end_character != -1)
                                    {
                                       p_de->search_time -= 5;
                                    }
                                 }

                                 /*
                                  * Since the file is in the file pool, it is not
                                  * necessary to test all other masks.
                                  */
                                 i = p_de->nfg;
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
                     }
                  }
               }
            }
         }
      } /* while ((p_dir = readdir(dp)) != NULL) */

      if (gotcha == NO)
      {
         files_in_dir = dummy_files_in_dir;
         bytes_in_dir = dummy_bytes_in_dir;
         goto done;
      }
      else
      {
         rewinddir(dp);
      }
   }

   if (p_de->flag & ALL_FILES)
   {
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            errno = 0;
            continue;
         }

         (void)strcpy(work_ptr, p_dir->d_name);
         if (stat(fullname, &stat_buf) < 0)
         {
            if (errno != ENOENT)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Can't stat() file `%s' : %s",
                          fullname, strerror(errno));
            }
            continue;
         }

         if (fra[p_de->fra_pos].ignore_file_time != 0)
         {
            diff_time = current_time - stat_buf.st_mtime;
         }

         /* Sure it is a normal file? */
         if (S_ISREG(stat_buf.st_mode))
         {
            files_in_dir++;
            bytes_in_dir += stat_buf.st_size;
            if ((count_files == NO) || /* Paused dir */
                (((fra[p_de->fra_pos].ignore_size == 0) ||
                  ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
                   (fra[p_de->fra_pos].ignore_size == stat_buf.st_size)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign == ISIZE_LESS_THEN) &&
                   (fra[p_de->fra_pos].ignore_size < stat_buf.st_size)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign == ISIZE_GREATER_THEN) &&
                   (fra[p_de->fra_pos].ignore_size > stat_buf.st_size))) &&
                 ((fra[p_de->fra_pos].ignore_file_time == 0) ||
                   ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                    (fra[p_de->fra_pos].ignore_file_time == diff_time)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign == IFTIME_LESS_THEN) &&
                   (fra[p_de->fra_pos].ignore_file_time < diff_time)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign == IFTIME_GREATER_THEN) &&
                   (fra[p_de->fra_pos].ignore_file_time > diff_time)))))
            {
#ifdef _POSIX_SAVED_IDS
               if ((stat_buf.st_mode & S_IROTH) ||
                   ((stat_buf.st_gid == afd_gid) && (stat_buf.st_mode & S_IRGRP)) ||
                   ((stat_buf.st_uid == afd_uid) && (stat_buf.st_mode & S_IRUSR)) ||
                   ((stat_buf.st_mode & S_IRGRP) && (no_of_sgids > 0) &&
                    (check_sgids(stat_buf.st_gid) == YES)))
#else
               if ((eaccess(fullname, R_OK) == 0))
#endif
               {
#ifdef WITH_DUP_CHECK
                  int is_duplicate = NO;

                  if ((fra[p_de->fra_pos].dup_check_timeout == 0L) ||
                      (((is_duplicate = isdup(fullname, p_de->dir_id,
                                              fra[p_de->fra_pos].dup_check_timeout,
                                              fra[p_de->fra_pos].dup_check_flag, NO)) == NO) ||
                       (((fra[p_de->fra_pos].dup_check_flag & DC_DELETE) == 0) &&
                        ((fra[p_de->fra_pos].dup_check_flag & DC_STORE) == 0))))
                  {
                     if ((is_duplicate == YES) &&
                         (fra[p_de->fra_pos].dup_check_flag & DC_WARN))
                     {
                        receive_log(WARN_SIGN, NULL, 0, current_time,
                                    "File %s is duplicate.", p_dir->d_name);
                     }
#endif
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
                                        p_de->dir_id, &split_job_counter,
                                        unique_number, ptr, amg_counter_fd) < 0)
                        {
                           if (errno == ENOSPC)
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "DISK FULL!!! Will retry in %d second interval.",
                                         DISK_FULL_RESCAN_TIME);

                              while (errno == ENOSPC)
                              {
                                 (void)sleep(DISK_FULL_RESCAN_TIME);
                                 errno = 0;
                                 if (create_name(tmp_file_dir, NO_PRIORITY,
                                                 current_time, p_de->dir_id,
                                                 &split_job_counter,
                                                 unique_number, ptr,
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

                              /*
                               * If the disk is full, lets stop copying/moving
                               * files so we can send data as quickly as possible
                               * to get more free disk space.
                               */
                              break;
                           }
                           else
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         "Failed to create a unique name : %s",
                                         strerror(errno));
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
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "Failed to unlink() file `%s' : %s",
                                         fullname, strerror(errno));
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
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to move/copy file.");
#ifdef WITH_DUP_CHECK
                        /*
                         * We have already stored the CRC value for
                         * this file but failed pick up the file.
                         * So we must remove the CRC value!
                         */
                        if ((fra[p_de->fra_pos].dup_check_timeout > 0L) &&
                            (is_duplicate == NO))
                        {
                           (void)isdup(fullname, p_de->dir_id,
                                       fra[p_de->fra_pos].dup_check_timeout,
                                       fra[p_de->fra_pos].dup_check_flag, YES);
                        }
#endif
                     }
                     else
                     {
#ifdef _INPUT_LOG
                        if ((count_files == YES) ||
                            (count_files == PAUSED_REMOTE))
                        {
                           /* Log the file name in the input log. */
                           (void)strcpy(il_file_name, p_dir->d_name);
                           *il_file_size = stat_buf.st_size;
                           *il_dir_number = p_de->dir_id;
                           *il_unique_number = *unique_number;
                           il_real_size = strlen(p_dir->d_name) + il_size;
                           if (write(il_fd, il_data, il_real_size) != il_real_size)
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "write() error : %s", strerror(errno));

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
#ifndef _WITH_PTHREAD
                        if ((files_copied + 1) > max_file_buffer)
                        {
                           if ((files_copied + 1) > fra[p_de->fra_pos].max_copied_files)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         "Hmmm, files_copied %d is larger then max_copied_files %u.",
                                         files_copied + 1,
                                         fra[p_de->fra_pos].max_copied_files);
                              max_file_buffer = files_copied + 1;
                           }
                           else
                           {
                              max_file_buffer = fra[p_de->fra_pos].max_copied_files;
                           }
                           REALLOC_RT_ARRAY(file_name_pool, max_file_buffer,
                                            MAX_FILENAME_LENGTH, char);
                           if ((file_mtime_pool = realloc(file_mtime_pool,
                                                          max_file_buffer * sizeof(off_t))) == NULL)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         "realloc() error : %s",
                                         strerror(errno));
                              exit(INCORRECT);
                           }
                           if ((file_size_pool = realloc(file_size_pool,
                                                         max_file_buffer * sizeof(off_t))) == NULL)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         "realloc() error : %s",
                                         strerror(errno));
                              exit(INCORRECT);
                           }
                        }
#endif
                        (void)strcpy(file_name_pool[files_copied], p_dir->d_name);
                        file_mtime_pool[files_copied] = stat_buf.st_mtime;
                        file_size_pool[files_copied] = stat_buf.st_size;

                        *total_file_size += stat_buf.st_size;
                        if ((++files_copied >= fra[p_de->fra_pos].max_copied_files) ||
                            (*total_file_size >= fra[p_de->fra_pos].max_copied_file_size))
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
#ifdef WITH_DUP_CHECK
                  }
                  else
                  {
                     if (is_duplicate == YES)
                     {
                        if (fra[p_de->fra_pos].dup_check_flag & DC_DELETE)
                        {
                           if (unlink(fullname) == -1)
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Failed to unlink() %s : %s",
                                         fullname, strerror(errno));
                           }
                           else
                           {
# ifdef _DELETE_LOG
                              size_t dl_real_size;

                              (void)strcpy(dl.file_name, p_dir->d_name);
                              (void)sprintf(dl.host_name, "%-*s %x",
                                            MAX_HOSTNAME_LENGTH, "-", DUP_INPUT);
                              *dl.file_size = stat_buf.st_size;
                              *dl.job_number = p_de->dir_id;
                              *dl.file_name_length = strlen(p_dir->d_name);
                              dl_real_size = *dl.file_name_length + dl.size +
                                             sprintf((dl.file_name + *dl.file_name_length + 1),
                                             "dir_check()");
                              if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                              {
                                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                                            "write() error : %s", strerror(errno));
                              }
# endif /* _DELETE_LOG */
                              files_in_dir--;
                              bytes_in_dir -= stat_buf.st_size;
                           }
                        }
                        else if (fra[p_de->fra_pos].dup_check_flag & DC_STORE)
                             {
                                char *p_end,
                                     save_dir[MAX_PATH_LENGTH];

                                p_end = save_dir +
                                        sprintf(save_dir, "%s%s%s/%x/",
                                                p_work_dir, AFD_FILE_DIR,
                                                STORE_DIR, p_de->dir_id);
                                if ((mkdir(save_dir, DIR_MODE) == -1) &&
                                    (errno != EEXIST))
                                {
                                   system_log(ERROR_SIGN, __FILE__, __LINE__,
                                              "Failed to mkdir() `%s' : %s",
                                              save_dir, strerror(errno));
                                   (void)unlink(fullname);
                                }
                                else
                                {
                                   (void)strcpy(p_end, p_dir->d_name);
                                   if (rename(fullname, save_dir) == -1)
                                   {
                                      system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                 "Failed to rename() `%s' to `%s' : %s",
                                                 fullname, save_dir,
                                                 strerror(errno));
                                      (void)unlink(fullname);
                                   }
                                }
                                files_in_dir--;
                                bytes_in_dir -= stat_buf.st_size;
                             }
                        if (fra[p_de->fra_pos].dup_check_flag & DC_WARN)
                        {
                           receive_log(WARN_SIGN, NULL, 0, current_time,
                                       "File %s is duplicate.", p_dir->d_name);
                        }
                     }
                  }
#endif /* WITH_DUP_CHECK */
               }
            }
         } /* if (S_ISREG(stat_buf.st_mode)) */
         errno = 0;
      } /* while ((p_dir = readdir(dp)) != NULL) */
   }
   else
   {
      int j;

      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            errno = 0;
            continue;
         }

         (void)strcpy(work_ptr, p_dir->d_name);
         if (stat(fullname, &stat_buf) < 0)
         {
            if (errno != ENOENT)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Can't access file `%s' : %s",
                          fullname, strerror(errno));
            }
            continue;
         }

         if (fra[p_de->fra_pos].ignore_file_time != 0)
         {
            diff_time = current_time - stat_buf.st_mtime;
         }

         /* Sure it is a normal file? */
         if (S_ISREG(stat_buf.st_mode))
         {
            files_in_dir++;
            bytes_in_dir += stat_buf.st_size;
            if ((count_files == NO) || /* Paused dir. */
                (((fra[p_de->fra_pos].ignore_size == 0) ||
                  ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
                   (fra[p_de->fra_pos].ignore_size == stat_buf.st_size)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign == ISIZE_LESS_THEN) &&
                   (fra[p_de->fra_pos].ignore_size < stat_buf.st_size)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign == ISIZE_GREATER_THEN) &&
                   (fra[p_de->fra_pos].ignore_size > stat_buf.st_size))) &&
                 ((fra[p_de->fra_pos].ignore_file_time == 0) ||
                   ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                    (fra[p_de->fra_pos].ignore_file_time == diff_time)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign == IFTIME_LESS_THEN) &&
                   (fra[p_de->fra_pos].ignore_file_time < diff_time)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign == IFTIME_GREATER_THEN) &&
                   (fra[p_de->fra_pos].ignore_file_time > diff_time)))))
            {
#ifdef _POSIX_SAVED_IDS
               if ((stat_buf.st_mode & S_IROTH) ||
                   ((stat_buf.st_gid == afd_gid) && (stat_buf.st_mode & S_IRGRP)) ||
                   ((stat_buf.st_uid == afd_uid) && (stat_buf.st_mode & S_IRUSR)) ||
                   ((stat_buf.st_mode & S_IRGRP) && (no_of_sgids > 0) &&
                    (check_sgids(stat_buf.st_gid) == YES)))
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
                        if (p_de->paused_dir == NULL)
                        {
                           pmatch_time = current_time;
                        }
                        else
                        {
                           pmatch_time = stat_buf.st_mtime;
                        }
                        if ((ret = pmatch(p_de->fme[i].file_mask[j], p_dir->d_name, &pmatch_time)) == 0)
                        {
#ifdef WITH_DUP_CHECK
                           int is_duplicate = NO;

                           if ((fra[p_de->fra_pos].dup_check_timeout == 0L) ||
                               (((is_duplicate = isdup(fullname, p_de->dir_id,
                                                       fra[p_de->fra_pos].dup_check_timeout,
                                                       fra[p_de->fra_pos].dup_check_flag, NO)) == NO) ||
                                (((fra[p_de->fra_pos].dup_check_flag & DC_DELETE) == 0) &&
                                 ((fra[p_de->fra_pos].dup_check_flag & DC_STORE) == 0))))
                           {
                              if ((is_duplicate == YES) &&
                                  (fra[p_de->fra_pos].dup_check_flag & DC_WARN))
                              {
                                 receive_log(WARN_SIGN, NULL, 0, current_time,
                                             "File %s is duplicate.", p_dir->d_name);
                              }
#endif
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
                                                 current_time, p_de->dir_id,
                                                 &split_job_counter,
                                                 unique_number, ptr,
                                                 amg_counter_fd) < 0)
                                 {
                                    if (errno == ENOSPC)
                                    {
                                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                  "DISK FULL!!! Will retry in %d second interval.",
                                                  DISK_FULL_RESCAN_TIME);

                                       while (errno == ENOSPC)
                                       {
                                          (void)sleep(DISK_FULL_RESCAN_TIME);
                                          errno = 0;
                                          if (create_name(tmp_file_dir,
                                                          NO_PRIORITY,
                                                          current_time,
                                                          p_de->dir_id,
                                                          &split_job_counter,
                                                          unique_number,
                                                          ptr,
                                                          amg_counter_fd) < 0)
                                          {
                                             if (errno != ENOSPC)
                                             {
                                                system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                           "Failed to create a unique name in %s : %s",
                                                           tmp_file_dir,
                                                           strerror(errno));
                                                exit(INCORRECT);
                                             }
                                          }
                                       }
                                       system_log(INFO_SIGN, __FILE__, __LINE__,
                                                  "Continuing after disk was full.");

                                       /*
                                        * If the disk is full, lets stop copying/moving
                                        * files so we can send data as quickly as
                                        * possible to get more free disk space.
                                        */
                                       goto done;
                                    }
                                    else
                                    {
                                       system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                  "Failed to create a unique name : %s",
                                                  strerror(errno));
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
                                  (count_files == NO) || /* Paused directory. */
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
                                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                  "Failed to unlink() file `%s' : %s",
                                                  fullname, strerror(errno));
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
                                 system_log(WARN_SIGN, __FILE__, __LINE__,
                                            "Failed to move/copy file `%s' to `%s'.",
                                            fullname, tmp_file_dir);
#ifdef WITH_DUP_CHECK
                                 /*
                                  * We have already stored the CRC value for
                                  * this file but failed pick up the file.
                                  * So we must remove the CRC value!
                                  */
                                 if ((fra[p_de->fra_pos].dup_check_timeout > 0L) &&
                                     (is_duplicate == NO))
                                 {
                                    (void)isdup(fullname, p_de->dir_id,
                                                fra[p_de->fra_pos].dup_check_timeout,
                                                fra[p_de->fra_pos].dup_check_flag, YES);

                                    /*
                                     * We must set gotcha to yes, otherwise
                                     * the file will be deleted because
                                     * we then think it is something
                                     * not to be distributed.
                                     */
                                    gotcha = YES;
                                 }
#endif
                              }
                              else
                              {
#ifdef _INPUT_LOG
                                 if ((count_files == YES) ||
                                     (count_files == PAUSED_REMOTE))
                                 {
                                    /* Log the file name in the input log. */
                                    (void)strcpy(il_file_name, p_dir->d_name);
                                    *il_file_size = stat_buf.st_size;
                                    *il_dir_number = p_de->dir_id;
                                    *il_unique_number = *unique_number;
                                    il_real_size = strlen(p_dir->d_name) + il_size;
                                    if (write(il_fd, il_data, il_real_size) != il_real_size)
                                    {
                                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                  "write() error : %s",
                                                  strerror(errno));

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
#ifndef _WITH_PTHREAD
                                 if ((files_copied + 1) > max_file_buffer)
                                 {
                                    if ((files_copied + 1) > fra[p_de->fra_pos].max_copied_files)
                                    {
                                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                  "Hmmm, files_copied %d is larger then max_copied_files %u.",
                                                  files_copied + 1,
                                                  fra[p_de->fra_pos].max_copied_files);
                                       max_file_buffer = files_copied + 1;
                                    }
                                    else
                                    {
                                       max_file_buffer = fra[p_de->fra_pos].max_copied_files;
                                    }
                                    REALLOC_RT_ARRAY(file_name_pool, max_file_buffer,
                                                     MAX_FILENAME_LENGTH, char);
                                    if ((file_mtime_pool = realloc(file_mtime_pool,
                                                                   max_file_buffer * sizeof(off_t))) == NULL)
                                    {
                                       system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                  "realloc() error : %s",
                                                  strerror(errno));
                                       exit(INCORRECT);
                                    }
                                    if ((file_size_pool = realloc(file_size_pool,
                                                                  max_file_buffer * sizeof(off_t))) == NULL)
                                    {
                                       system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                  "realloc() error : %s",
                                                  strerror(errno));
                                       exit(INCORRECT);
                                    }
                                 }
#endif
                                 (void)strcpy(file_name_pool[files_copied], p_dir->d_name);
                                 file_mtime_pool[files_copied] = stat_buf.st_mtime;
                                 file_size_pool[files_copied] = stat_buf.st_size;

                                 *total_file_size += stat_buf.st_size;
                                 if ((++files_copied >= fra[p_de->fra_pos].max_copied_files) ||
                                     (*total_file_size >= fra[p_de->fra_pos].max_copied_file_size))
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
#ifdef WITH_DUP_CHECK
                           }
                           else
                           {
                              if (is_duplicate == YES)
                              {
                                 if (fra[p_de->fra_pos].dup_check_flag & DC_DELETE)
                                 {
                                    if (unlink(fullname) == -1)
                                    {
                                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                                  "Failed to unlink() %s : %s",
                                                  fullname, strerror(errno));
                                    }
                                    else
                                    {
# ifdef _DELETE_LOG
                                       size_t dl_real_size;

                                       (void)strcpy(dl.file_name, p_dir->d_name);
                                       (void)sprintf(dl.host_name, "%-*s %x",
                                                     MAX_HOSTNAME_LENGTH, "-", DUP_INPUT);
                                       *dl.file_size = stat_buf.st_size;
                                       *dl.job_number = p_de->dir_id;
                                       *dl.file_name_length = strlen(p_dir->d_name);
                                       dl_real_size = *dl.file_name_length + dl.size +
                                                      sprintf((dl.file_name + *dl.file_name_length + 1),
                                                      "dir_check()");
                                       if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                                       {
                                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                     "write() error : %s",
                                                     strerror(errno));
                                       }
# endif /* _DELETE_LOG */
                                       files_in_dir--;
                                       bytes_in_dir -= stat_buf.st_size;

                                       gotcha = YES;
                                       i = p_de->nfg;
                                    }
                                 }
                                 else if (fra[p_de->fra_pos].dup_check_flag & DC_STORE)
                                      {
                                         char *p_end,
                                              save_dir[MAX_PATH_LENGTH];

                                         p_end = save_dir +
                                                 sprintf(save_dir, "%s%s%s/%x/",
                                                         p_work_dir,
                                                         AFD_FILE_DIR,
                                                         STORE_DIR,
                                                         p_de->dir_id);
                                         if ((mkdir(save_dir, DIR_MODE) == -1) &&
                                             (errno != EEXIST))
                                         {
                                            system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                       "Failed to mkdir() `%s' : %s",
                                                       save_dir,
                                                       strerror(errno));
                                            (void)unlink(fullname);
                                         }
                                         else
                                         {
                                            (void)strcpy(p_end, p_dir->d_name);
                                            if (rename(fullname, save_dir) == -1)
                                            {
                                               system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                          "Failed to rename() `%s' to `%s' : %s",
                                                          fullname, save_dir,
                                                          strerror(errno));
                                               (void)unlink(fullname);
                                            }
                                         }
                                         files_in_dir--;
                                         bytes_in_dir -= stat_buf.st_size;

                                         gotcha = YES;
                                         i = p_de->nfg;
                                      }
                                 if (fra[p_de->fra_pos].dup_check_flag & DC_WARN)
                                 {
                                    receive_log(WARN_SIGN, NULL, 0, current_time,
                                                "File %s is duplicate.",
                                                p_dir->d_name);
                                 }
                              }
                           }
#endif /* WITH_DUP_CHECK */
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
                     diff_time = current_time - stat_buf.st_mtime;
                     if (diff_time > fra[p_de->fra_pos].unknown_file_time)
                     {
                        if (unlink(fullname) == -1)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "Failed to unlink() %s : %s",
                                      fullname, strerror(errno));
                        }
                        else
                        {
#ifdef _DELETE_LOG
                           size_t dl_real_size;

                           (void)strcpy(dl.file_name, p_dir->d_name);
                           (void)sprintf(dl.host_name, "%-*s %x",
                                         MAX_HOSTNAME_LENGTH, "-", OTHER_DEL);
                           *dl.file_size = stat_buf.st_size;
                           *dl.job_number = p_de->dir_id;
                           *dl.file_name_length = strlen(p_dir->d_name);
                           dl_real_size = *dl.file_name_length + dl.size +
                                          sprintf((dl.file_name + *dl.file_name_length + 1),
# if SIZEOF_TIME_T == 4
                                          "dir_check() >%ld", diff_time);
# else
                                          "dir_check() >%lld", diff_time);
# endif
                           if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "write() error : %s", strerror(errno));
                           }
#endif /* _DELETE_LOG */
                           files_in_dir--;
                           bytes_in_dir -= stat_buf.st_size;
                        }
                     }
                  }
               }
            }
         } /* if (S_ISREG(stat_buf.st_mode)) */
         errno = 0;
      } /* while ((p_dir = readdir(dp)) != NULL) */
   }

done:

   /* When using readdir() it can happen that it always returns     */
   /* NULL, due to some error. We want to know if this is the case. */
   if (errno == EBADF)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "readdir() error from `%s' : %s", fullname, strerror(errno));
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
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not close directory `%s' : %s",
                 src_file_path, strerror(errno));
   }

#ifdef _WITH_PTHREAD
   if ((ret = pthread_mutex_lock(&fsa_mutex)) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "pthread_mutex_lock() error : %s", strerror(ret));
   }
#endif
   if ((files_copied >= fra[p_de->fra_pos].max_copied_files) ||
       (*total_file_size >= fra[p_de->fra_pos].max_copied_file_size))
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
      if ((count_files == YES) || (count_files == PAUSED_REMOTE))
      {
         fra[p_de->fra_pos].files_received += files_copied;
         fra[p_de->fra_pos].bytes_received += *total_file_size;
         fra[p_de->fra_pos].last_retrieval = current_time;
         receive_log(INFO_SIGN, NULL, 0, current_time,
#if SIZEOF_OFF_T == 4
                     "Received %d files with %ld Bytes.",
#else
                     "Received %d files with %lld Bytes.",
#endif
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
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "pthread_mutex_unlock() error : %s", strerror(ret));
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
#ifdef O_LARGEFILE
      if ((fd = open(file_name, O_RDONLY | O_LARGEFILE)) != -1)
#else
      if ((fd = open(file_name, O_RDONLY)) != -1)
#endif
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
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Failed to read() last character from %s : %s",
                          file_name, strerror(errno));
            }
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to lseek() in %s : %s",
                       file_name, strerror(errno));
         }
         if (close(fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to close() %s : %s", file_name, strerror(errno));
         }
      }
   }

   return(last_char);
}


#ifdef _POSIX_SAVED_IDS
/*++++++++++++++++++++++++++++ check_sgids() ++++++++++++++++++++++++++++*/
static int
check_sgids(gid_t file_gid)
{
   int i;

   for (i = 0; i < no_of_sgids; i++)
   {
      if (file_gid == afd_sgids[i])
      {
         return(YES);
      }
   }
   return(NO);
}
#endif

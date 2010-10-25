/*
 *  get_file_names.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2010 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_file_names - gets the name of all files in a directory
 **
 ** SYNOPSIS
 **   int get_file_names(char *file_path, off_t *file_size_to_send)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   The number of files, total file size it has found in the directory
 **   and the directory where the files have been found. If all files
 **   are deleted due to age limit, it will return -1. Otherwise if
 **   an error occurs it will exit.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.12.1996 H.Kiehl Created
 **   31.01.1997 H.Kiehl Support for age limit.
 **   14.10.1998 H.Kiehl Free unused memory.
 **   03.02.2001 H.Kiehl Sort the files by date.
 **   21.08.2001 H.Kiehl Return -1 if all files have been deleted due
 **                      to age limit.
 **   20.12.2004 H.Kiehl Addition of delete file name buffer.
 **   14.02.2009 H.Kiehl Log to OUTPUT_LOG when we delete a file.
 **
 */
DESCR__E_M3

#include <stdio.h>                     /* NULL                           */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* realloc(), malloc(), free()    */
#include <time.h>                      /* time()                         */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>                    /* opendir(), closedir(), DIR,    */
                                       /* readdir(), struct dirent       */
#include <fcntl.h>
#include <unistd.h>                    /* unlink(), getpid()             */
#include <errno.h>
#include "fddefs.h"

/* Global variables. */
extern int                        files_to_delete,
                                  fsa_fd;
#ifdef _OUTPUT_LOG
extern int                        ol_fd;
# ifdef WITHOUT_FIFO_RW_SUPPORT
extern int                        ol_readfd;
# endif
extern unsigned int               *ol_job_number,
                                  *ol_retries;
extern char                       *ol_data,
                                  *ol_file_name,
                                  *ol_output_type;
extern unsigned short             *ol_archive_name_length,
                                  *ol_file_name_length,
                                  *ol_unl;
extern off_t                      *ol_file_size;
extern size_t                     ol_size,
                                  ol_real_size;
extern clock_t                    *ol_transfer_time;
#endif
extern off_t                      *file_size_buffer;
extern time_t                     *file_mtime_buffer;
extern char                       *p_work_dir,
                                  tr_hostname[MAX_HOSTNAME_LENGTH + 1],
                                  *del_file_name_buffer,
                                  *file_name_buffer;
extern struct filetransfer_status *fsa;
extern struct job                 db;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif

#if defined (_DELETE_LOG) || defined (_OUTPUT_LOG)
/* Local function prototypes. */
# ifdef WITH_DUP_CHECK
static void                       log_data(char *, off_t, time_t, int, char);
# else
static void                       log_data(char *, off_t, time_t, char);
# endif
#endif


/*########################## get_file_names() ###########################*/
int
get_file_names(char *file_path, off_t *file_size_to_send)
{
   int           files_not_send = 0;
   off_t         file_size_not_send = 0;
#ifdef WITH_DUP_CHECK
   int           dup_counter = 0;
   off_t         dup_counter_size = 0;
#endif
   time_t        diff_time,
                 now;
   int           files_to_send = 0,
                 new_size,
                 offset;
   off_t         *p_file_size;
   time_t        *p_file_mtime;
   char          fullname[MAX_PATH_LENGTH],
                 *p_del_file_name,
                 *p_file_name,
                 *p_source_file;
   struct stat   stat_buf;
   struct dirent *p_dir;
   DIR           *dp;

   /*
    * Create directory name in which we can find the files for this job.
    */
   (void)strcpy(file_path, p_work_dir);
   (void)strcat(file_path, AFD_FILE_DIR);
   (void)strcat(file_path, OUTGOING_DIR);
   (void)strcat(file_path, "/");
   (void)strcat(file_path, db.msg_name);
   db.p_unique_name = db.msg_name;
   while ((*db.p_unique_name != '/') && (*db.p_unique_name != '\0'))
   {
      db.p_unique_name++; /* Away with the job ID. */
   }
   if (*db.p_unique_name == '/')
   {
      db.p_unique_name++;
      while ((*db.p_unique_name != '/') && (*db.p_unique_name != '\0'))
      {
         db.p_unique_name++; /* Away with the dir number. */
      }
      if (*db.p_unique_name == '/')
      {
         int  i;
         char str_num[MAX_INT_HEX_LENGTH + 1];

         db.p_unique_name++;
         i = 0;
         while ((*(db.p_unique_name + i) != '_') &&
                (*(db.p_unique_name + i) != '\0') && (i < MAX_INT_HEX_LENGTH))
         {
            str_num[i] = *(db.p_unique_name + i);
            i++;
         }
         if ((*(db.p_unique_name + i) != '_') || (i == 0))
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not determine message name from `%s'",
                       db.msg_name);
            exit(SYNTAX_ERROR);
         }
         str_num[i] = '\0';
         db.creation_time = (time_t)str2timet(str_num, NULL, 16);
         db.unl = i + 1;
         i = 0;
         while ((*(db.p_unique_name + db.unl + i) != '_') &&
                (*(db.p_unique_name + db.unl + i) != '\0') &&
                (i < MAX_INT_HEX_LENGTH))
         {
            str_num[i] = *(db.p_unique_name + db.unl + i);
            i++;
         }
         if ((*(db.p_unique_name + db.unl + i) != '_') || (i == 0))
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not determine message name from `%s'",
                       db.msg_name);
            exit(SYNTAX_ERROR);
         }
         str_num[i] = '\0';
         db.unique_number = (unsigned int)strtoul(str_num, NULL, 16);
         db.unl = db.unl + i + 1;
         i = 0;
         while ((*(db.p_unique_name + db.unl + i) != '\0') &&
                (i < MAX_INT_HEX_LENGTH))
         {
            str_num[i] = *(db.p_unique_name + db.unl + i);
            i++;
         }
         if (i == 0)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not determine message name from `%s'",
                       db.msg_name);
            exit(SYNTAX_ERROR);
         }
         str_num[i] = '\0';
         db.split_job_counter = (unsigned int)strtoul(str_num, NULL, 16);
         db.unl = db.unl + i;
         if (db.unl == 0)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not determine message name from `%s'",
                       db.msg_name);
            exit(SYNTAX_ERROR);
         }
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not determine message name from `%s'",
                    db.msg_name);
         exit(SYNTAX_ERROR);
      }
   }
   else
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not determine message name from `%s'", db.msg_name);
      exit(SYNTAX_ERROR);
   }

   if ((dp = opendir(file_path)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not opendir() %s [%s %d] : %s",
                 file_path, db.host_alias, (int)db.job_no, strerror(errno));
      exit(OPEN_FILE_DIR_ERROR);
   }

   /* Prepare pointers and directory name. */
   (void)strcpy(fullname, file_path);
   p_source_file = fullname + strlen(fullname);
   *p_source_file++ = '/';
   if (file_name_buffer != NULL)
   {
      free(file_name_buffer);
      file_name_buffer = NULL;
   }
   p_file_name = file_name_buffer;
   if (file_size_buffer != NULL)
   {
      free(file_size_buffer);
      file_size_buffer = NULL;
   }
   p_file_size = file_size_buffer;
   if (file_mtime_buffer != NULL)
   {
      free(file_mtime_buffer);
      file_mtime_buffer = NULL;
   }
   p_file_mtime = file_mtime_buffer;
   if (del_file_name_buffer != NULL)
   {
      free(del_file_name_buffer);
      del_file_name_buffer = NULL;
   }
   p_del_file_name = del_file_name_buffer;
   files_to_delete = 0;
   now = time(NULL);

   /*
    * Now let's determine the number of files that have to be
    * transmitted and the total size.
    */
   errno = 0;
   while ((p_dir = readdir(dp)) != NULL)
   {
      if (((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '\0')) ||
          ((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '.') &&
          (p_dir->d_name[2] == '\0')))
      {
         continue;
      }
      (void)strcpy(p_source_file, p_dir->d_name);
      if (stat(fullname, &stat_buf) < 0)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Can't stat() file `%s' : %s",
                    fullname, strerror(errno));
         continue;
      }

      /* Sure it is a normal file? */
      if (S_ISREG(stat_buf.st_mode))
      {
         /* Don't send files older then age_limit! */
#ifdef WITH_DUP_CHECK
         int is_duplicate = NO;

         if (now < stat_buf.st_mtime)
         {
            diff_time = 0L;
         }
         else
         {
            diff_time = now - stat_buf.st_mtime;
         }
# ifdef DEBUG_DUP_CHECK
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
#  if SIZEOF_TIME_T == 4
                    "fullname=%s crc_id=%x timeout=%ld flag=%u",
#  else
                    "fullname=%s crc_id=%x timeout=%lld flag=%u",
#  endif
                    fullname, db.crc_id, (pri_time_t)db.dup_check_timeout,
                    db.dup_check_flag);
# endif
         if (((db.age_limit > 0) &&
              ((fsa->host_status & DO_NOT_DELETE_DATA) == 0) &&
              (diff_time > db.age_limit)) ||
             ((db.dup_check_timeout > 0) &&
              ((db.special_flag & OLD_ERROR_JOB) == 0) &&
              (((is_duplicate = isdup(fullname, stat_buf.st_size, db.crc_id,
                                      db.dup_check_timeout,
                                      db.dup_check_flag, NO)) == YES) &&
               ((db.dup_check_flag & DC_DELETE) ||
                (db.dup_check_flag & DC_STORE)))))
#else
         if (now < stat_buf.st_mtime)
         {
            diff_time = 0L;
         }
         else
         {
            diff_time = now - stat_buf.st_mtime;
         }
         if ((db.age_limit > 0) &&
             ((fsa->host_status & DO_NOT_DELETE_DATA) == 0) &&
             (diff_time > db.age_limit))
#endif
         {
            char file_to_remove[MAX_FILENAME_LENGTH];

            file_to_remove[0] = '\0';
            if (db.no_of_restart_files > 0)
            {
               int  ii;
               char initial_filename[MAX_FILENAME_LENGTH];

               if ((db.lock == DOT) || (db.lock == DOT_VMS))
               {
                  (void)strcpy(initial_filename, db.lock_notation);
                  (void)strcat(initial_filename, p_dir->d_name);
               }
               else
               {
                  (void)strcpy(initial_filename, p_dir->d_name);
               }

               for (ii = 0; ii < db.no_of_restart_files; ii++)
               {
                  if ((CHECK_STRCMP(db.restart_file[ii], initial_filename) == 0) &&
                      (append_compare(db.restart_file[ii], fullname) == YES))
                  {
                     (void)strcpy(file_to_remove, db.restart_file[ii]);
                     remove_append(db.job_id, db.restart_file[ii]);
                     break;
                  }
               }
            }
#ifdef WITH_DUP_CHECK
            if (is_duplicate == YES)
            {
               dup_counter++;
               dup_counter_size += stat_buf.st_size;
               if (db.dup_check_flag & DC_WARN)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "File `%s' is duplicate.", p_dir->d_name);
               }
            }
            if ((is_duplicate == YES) && (db.dup_check_flag & DC_STORE))
            {
               char *p_end,
                    save_dir[MAX_PATH_LENGTH];

               p_end = save_dir +
                       sprintf(save_dir, "%s%s%s/%x/",
                               p_work_dir, AFD_FILE_DIR, STORE_DIR, db.job_id);
               if ((mkdir(save_dir, DIR_MODE) == -1) && (errno != EEXIST))
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to mkdir() `%s' : %s",
                             save_dir, strerror(errno));
                  if (unlink(fullname) == -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to unlink() file `%s' due to duplicate check : %s",
                                p_dir->d_name, strerror(errno));
                  }
# if defined (_DELETE_LOG) || defined (_OUTPUT_LOG)
                  else
                  {
                     log_data(p_dir->d_name, stat_buf.st_size, diff_time,
                              YES, OT_DUPLICATE_DELETE + '0');
                  }
# endif
               }
               else
               {
                  (void)strcpy(p_end, p_dir->d_name);
                  if (rename(fullname, save_dir) == -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to rename() `%s' to `%s' : %s",
                                fullname, save_dir, strerror(errno));
                     if (unlink(fullname) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to unlink() file `%s' due to duplicate check : %s",
                                   p_dir->d_name, strerror(errno));
                     }
# if defined (_DELETE_LOG) || defined (_OUTPUT_LOG)
                     else
                     {
                        log_data(p_dir->d_name, stat_buf.st_size, diff_time,
                                 YES, OT_DUPLICATE_STORED + '0');
                     }
# endif
                  }
               }
               files_not_send++;
               file_size_not_send += stat_buf.st_size;
            }
            else
            {
#endif /* WITH_DUP_CHECK */
               if (unlink(fullname) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to unlink() file `%s' due to age : %s",
                             p_dir->d_name, strerror(errno));
               }
               else
               {
#if defined (_DELETE_LOG) || defined (_OUTPUT_LOG)
                  log_data(p_dir->d_name, stat_buf.st_size, diff_time,
# ifdef WITH_DUP_CHECK
                           is_duplicate, (is_duplicate == YES) ? (OT_DUPLICATE_DELETE + '0') : (OT_AGE_LIMIT_DELETE + '0'));
# else
                           OT_AGE_LIMIT_DELETE + '0');
# endif
#endif
#ifndef _DELETE_LOG
                  if ((db.protocol & FTP_FLAG) || (db.protocol & SFTP_FLAG))
                  {
                     if (db.no_of_restart_files > 0)
                     {
                        int append_file_number = -1,
                            ii;

                        for (ii = 0; ii < db.no_of_restart_files; ii++)
                        {
                           if (CHECK_STRCMP(db.restart_file[ii], p_dir->d_name) == 0)
                           {
                              append_file_number = ii;
                              break;
                           }
                        }
                        if (append_file_number != -1)
                        {
                           remove_append(db.job_id,
                                         db.restart_file[append_file_number]);
                        }
                     }
                  }
#endif
                  if (file_to_remove[0] != '\0')
                  {
                     if ((files_to_delete % 20) == 0)
                     {
                        /* Increase the space for the delete file name buffer. */
                        new_size = ((files_to_delete / 20) + 1) *
                                   20 * MAX_FILENAME_LENGTH;
                        offset = p_del_file_name - del_file_name_buffer;
                        if ((del_file_name_buffer = realloc(del_file_name_buffer,
                                                            new_size)) == NULL)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "Could not realloc() memory : %s",
                                      strerror(errno));
                           exit(ALLOC_ERROR);
                        }
                        p_del_file_name = del_file_name_buffer + offset;
                     }
                     (void)strcpy(p_del_file_name, file_to_remove);
                     p_del_file_name += MAX_FILENAME_LENGTH;
                     files_to_delete++;
                  }

                  files_not_send++;
                  file_size_not_send += stat_buf.st_size;
               }
#ifdef WITH_DUP_CHECK
            }
#endif
         }
         else
         {
#ifdef WITH_DUP_CHECK
            if ((is_duplicate == YES) && (db.dup_check_flag & DC_WARN))
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "File `%s' is duplicate.", p_dir->d_name);
            }
#endif
            if ((files_to_send % 20) == 0)
            {
               /* Increase the space for the file name buffer. */
               new_size = ((files_to_send / 20) + 1) *
                          20 * MAX_FILENAME_LENGTH;
               offset = p_file_name - file_name_buffer;
               if ((file_name_buffer = realloc(file_name_buffer,
                                               new_size)) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not realloc() memory : %s",
                             strerror(errno));
                  exit(ALLOC_ERROR);
               }
               p_file_name = file_name_buffer + offset;

               /* Increase the space for the file size buffer. */
               new_size = ((files_to_send / 20) + 1) * 20 * sizeof(off_t);
               offset = (char *)p_file_size - (char *)file_size_buffer;
               if ((file_size_buffer = realloc(file_size_buffer,
                                               new_size)) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not realloc() memory : %s",
                             strerror(errno));
                  exit(ALLOC_ERROR);
               }
               p_file_size = (off_t *)((char *)file_size_buffer + offset);

               if ((fsa->protocol_options & SORT_FILE_NAMES) ||
#ifdef WITH_EUMETSAT_HEADERS
                   (db.special_flag & ADD_EUMETSAT_HEADER) ||
#endif
                   (fsa->protocol_options & KEEP_TIME_STAMP))
               {
                  /* Increase the space for the file mtime buffer. */
                  new_size = ((files_to_send / 20) + 1) * 20 * sizeof(time_t);
                  offset = (char *)p_file_mtime - (char *)file_mtime_buffer;
                  if ((file_mtime_buffer = realloc(file_mtime_buffer,
                                                   new_size)) == NULL)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Could not realloc() memory : %s",
                                strerror(errno));
                     exit(ALLOC_ERROR);
                  }
                  p_file_mtime = (time_t *)((char *)file_mtime_buffer + offset);
               }
            }

            /* Sort the files, newest must be last (FIFO). */
            if ((fsa->protocol_options & SORT_FILE_NAMES) &&
                (files_to_send > 1) &&
                (*(p_file_mtime - 1) > stat_buf.st_mtime))
            {
               int    i;
               off_t  *sp_file_size = p_file_size - 1;
               time_t *sp_mtime = p_file_mtime - 1;
               char   *sp_file_name = p_file_name - MAX_FILENAME_LENGTH;

               for (i = files_to_send; i > 0; i--)
               {
                  if (*sp_mtime <= stat_buf.st_mtime)
                  {
                     break;
                  }
                  sp_mtime--; sp_file_size--;
                  sp_file_name -= MAX_FILENAME_LENGTH;
               }
               (void)memmove(sp_mtime + 2, sp_mtime + 1,
                             (files_to_send - i) * sizeof(time_t));
               *(sp_mtime + 1) = stat_buf.st_mtime;
               (void)memmove(sp_file_size + 2, sp_file_size + 1,
                             (files_to_send - i) * sizeof(off_t));
               *(sp_file_size + 1) = stat_buf.st_size;
               (void)memmove(sp_file_name + (2 * MAX_FILENAME_LENGTH),
                             sp_file_name + MAX_FILENAME_LENGTH,
                             (files_to_send - i) * MAX_FILENAME_LENGTH);
               (void)strcpy(sp_file_name + MAX_FILENAME_LENGTH,
                            p_dir->d_name);
            }
            else
            {
               (void)strcpy(p_file_name, p_dir->d_name);
               *p_file_size = stat_buf.st_size;
               if (file_mtime_buffer != NULL)
               {
                  *p_file_mtime = stat_buf.st_mtime;
               }
            }
            p_file_name += MAX_FILENAME_LENGTH;
            p_file_size++;
            p_file_mtime++;
            files_to_send++;
            *file_size_to_send += stat_buf.st_size;
         }
      }
      errno = 0;
   }

   if (errno)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not readdir() `%s' : %s", file_path, strerror(errno));
   }

   if ((file_mtime_buffer != NULL) &&
#ifdef WITH_EUMETSAT_HEADERS
       ((db.special_flag & ADD_EUMETSAT_HEADER) == 0) &&
#endif
       ((fsa->protocol_options & KEEP_TIME_STAMP) == 0))
   {
      free(file_mtime_buffer);
      file_mtime_buffer = NULL;
   }

   if (files_not_send > 0)
   {
      /* Total file counter. */
#ifdef LOCK_DEBUG
      lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
      lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
      fsa->total_file_counter -= files_not_send;
#ifdef _VERIFY_FSA
      if (fsa->total_file_counter < 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Total file counter for host `%s' less then zero. Correcting to 0.",
                    fsa->host_dsp_name);
         fsa->total_file_counter = 0;
      }
#endif

      /* Total file size. */
      fsa->total_file_size -= file_size_not_send;
#ifdef _VERIFY_FSA
      if (fsa->total_file_size < 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Total file size for host `%s' overflowed. Correcting to 0.",
                    fsa->host_dsp_name);
         fsa->total_file_size = 0;
      }
      else if ((fsa->total_file_counter == 0) &&
               (fsa->total_file_size > 0))
           {
              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                         "fc for host `%s' is zero but fs is not zero. Correcting to 0.",
                         fsa->host_dsp_name);
              fsa->total_file_size = 0;
           }
#endif

      if ((fsa->total_file_counter == 0) && (fsa->total_file_size == 0) &&
          (fsa->error_counter > 0))
      {
         int j;

#ifdef LOCK_DEBUG
         lock_region_w(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
         lock_region_w(fsa_fd, db.lock_offset + LOCK_EC);
#endif
         fsa->error_counter = 0;

         /*
          * Remove the error condition (NOT_WORKING) from all jobs
          * of this host.
          */
         for (j = 0; j < fsa->allowed_transfers; j++)
         {
            if ((j != db.job_no) &&
                (fsa->job_status[j].connect_status == NOT_WORKING))
            {
               fsa->job_status[j].connect_status = DISCONNECT;
            }
         }
         fsa->error_history[0] = 0;
         fsa->error_history[1] = 0;
#ifdef LOCK_DEBUG
         unlock_region(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
         unlock_region(fsa_fd, db.lock_offset + LOCK_EC);
#endif
      }
#ifdef LOCK_DEBUG
      unlock_region(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
      unlock_region(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
#ifdef WITH_DUP_CHECK
      if (dup_counter > 0)
      {
         trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
# if SIZEOF_OFF_T == 4
                   "Deleted %d duplicate file(s) (%ld bytes).",
# else
                   "Deleted %d duplicate file(s) (%lld bytes).",
# endif
                   dup_counter, (pri_off_t)dup_counter_size);
      }
      if ((files_not_send - dup_counter) > 0)
      {
         trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
# if SIZEOF_OFF_T == 4
                   "Deleted %d file(s) (%ld bytes) due to age.",
# else
                   "Deleted %d file(s) (%lld bytes) due to age.",
# endif
                   files_not_send - dup_counter,
                   (pri_off_t)(file_size_not_send - dup_counter_size));
      }
#else
      trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
# if SIZEOF_OFF_T == 4
                "Deleted %d file(s) (%ld bytes) due to age.",
# else
                "Deleted %d file(s) (%lld bytes) due to age.",
# endif
                files_not_send, (pri_off_t)file_size_not_send);
#endif
#ifdef WITH_ERROR_QUEUE
      if ((files_to_send == 0) && (fsa->host_status & ERROR_QUEUE_SET) &&
          (check_error_queue(db.job_id, -1, 0, 0) == YES))
      {
         (void)remove_from_error_queue(db.job_id, fsa, db.fsa_pos, fsa_fd);
      }
#endif
   }

   if (closedir(dp) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not closedir() `%s' : %s", file_path, strerror(errno));
      exit(OPEN_FILE_DIR_ERROR);
   }

   /*
    * If we just return zero here when all files have been deleted
    * due to age and sf_xxx is bursting it does not know if this
    * is an error situation or not. So return -1 if we deleted all
    * files.
    */
   if ((files_to_send == 0) && (files_not_send > 0))
   {
      files_to_send = -1;
   }
   return(files_to_send);
}


#if defined (_DELETE_LOG) || defined (_OUTPUT_LOG)
/*++++++++++++++++++++++++++++++ log_data() +++++++++++++++++++++++++++++*/
static void
log_data(char   *d_name,
         off_t  st_size,
         time_t diff_time,
# ifdef WITH_DUP_CHECK
         int    is_duplicate,
# endif
         char   output_type)
{
# ifdef _DELETE_LOG
   int    prog_name_length;
   size_t dl_real_size;
   char   str_diff_time[2 + MAX_LONG_LENGTH + 1];
# endif

# ifdef _OUTPUT_LOG
   if (db.output_log == YES)
   {
      if (ol_fd == -2)
      {
#  ifdef WITHOUT_FIFO_RW_SUPPORT
         output_log_fd(&ol_fd, &ol_readfd);
#  else
         output_log_fd(&ol_fd);
#  endif
      }
      if (ol_fd > -1)
      {
         if (ol_data == NULL)
         {
            int current_toggle,
                protocol;

            if (db.protocol & FTP_FLAG)
            {
#  ifdef WITH_SSL
               if (db.auth == NO)
               {
                  protocol = FTP;
               }
               else
               {
                  protocol = FTPS;
               }
#  else
               protocol = FTP;
#  endif
            }
            else if (db.protocol & LOC_FLAG)
                 {
                    protocol = LOC;
                 }
            else if (db.protocol & HTTP_FLAG)
                 {
#  ifdef WITH_SSL
                    if (db.auth == NO)
                    {
                       protocol = HTTP;
                    }
                    else
                    {
                       protocol = HTTPS;
                    }
#  else
                    protocol = HTTP;
#  endif
                 }
            else if (db.protocol & SFTP_FLAG)
                 {
                    protocol = SFTP;
                 }
#  ifdef _WITH_SCP_SUPPORT
            else if (db.protocol & SCP_FLAG)
                 {
                    protocol = SCP;
                 }
#  endif
#  ifdef _WITH_WMO_SUPPORT
            else if (db.protocol & WMO_FLAG)
                 {
                    protocol = WMO;
                 }
#  endif
#  ifdef _WITH_MAP_SUPPORT
            else if (db.protocol & MAP_FLAG)
                 {
                    protocol = MAP;
                 }
#  endif
            else if (db.protocol & SMTP_FLAG)
                 {
                    protocol = SMTP;
                 }

            if (fsa->real_hostname[1][0] == '\0')
            {
               current_toggle = HOST_ONE;
            }
            else
            {
               if (db.toggle_host == YES)
               {
                  if (fsa->host_toggle == HOST_ONE)
                  {
                     current_toggle = HOST_TWO;
                  }
                  else
                  {
                     current_toggle = HOST_ONE;
                  }
               }
               else
               {
                  current_toggle = (int)fsa->host_toggle;
               }
            }

            output_log_ptrs(&ol_retries,
                            &ol_job_number,
                            &ol_data,   /* Pointer to buffer. */
                            &ol_file_name,
                            &ol_file_name_length,
                            &ol_archive_name_length,
                            &ol_file_size,
                            &ol_unl,
                            &ol_size,
                            &ol_transfer_time,
                            &ol_output_type,
                            db.host_alias,
                            (current_toggle - 1),
                            protocol);
         }
         (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
         (void)strcpy(ol_file_name + db.unl, d_name);
         *ol_file_name_length = (unsigned short)strlen(ol_file_name);
         ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
         ol_file_name[*ol_file_name_length + 1] = '\0';
         (*ol_file_name_length)++;
         *ol_file_size = st_size;
         *ol_job_number = db.job_id;
         *ol_retries = db.retries;
         *ol_unl = db.unl;
         *ol_transfer_time = 0L;
         *ol_archive_name_length = 0;
         *ol_output_type = output_type;
         ol_real_size = *ol_file_name_length + ol_size;
         if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "write() error : %s", strerror(errno));
         }
      }
   }
# endif

# ifdef _DELETE_LOG
   if (dl.fd == -1)
   {
      delete_log_ptrs(&dl);
   }
   (void)strcpy(dl.file_name, d_name);
   (void)sprintf(dl.host_name, "%-*s %03x",
                 MAX_HOSTNAME_LENGTH, fsa->host_alias,
#  ifdef WITH_DUP_CHECK
                                (is_duplicate == YES) ? DUP_OUTPUT : AGE_OUTPUT);
#  else
                 AGE_OUTPUT);
#  endif
   *dl.file_size = st_size;
   *dl.job_id = db.job_id;
   *dl.dir_id = 0;
   *dl.input_time = db.creation_time;
   *dl.split_job_counter = db.split_job_counter;
   *dl.unique_number = db.unique_number;
   *dl.file_name_length = strlen(d_name);
#  ifdef WITH_DUP_CHECK
   if (is_duplicate == YES)
   {
      str_diff_time[0] = '\0';
   }
   else
   {
#  endif
#  if SIZEOF_TIME_T == 4
      (void)sprintf(str_diff_time, "%c>%ld (%s %d)",
#  else
      (void)sprintf(str_diff_time, "%c>%lld (%s %d)",
#  endif
                    SEPARATOR_CHAR, (pri_time_t)diff_time,
                    __FILE__, __LINE__);
#  ifdef WITH_DUP_CHECK
   }
#  endif
   if (db.protocol & FTP_FLAG)
   {
      if (db.no_of_restart_files > 0)
      {
         int append_file_number = -1,
             ii;

         for (ii = 0; ii < db.no_of_restart_files; ii++)
         {
            if (CHECK_STRCMP(db.restart_file[ii], d_name) == 0)
            {
               append_file_number = ii;
               break;
            }
         }
         if (append_file_number != -1)
         {
            remove_append(db.job_id,
                          db.restart_file[append_file_number]);
         }
      }
      prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                 "%s%s",
                                 SEND_FILE_FTP, str_diff_time);
   }
   else if (db.protocol & LOC_FLAG)
        {
           prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                      "%s%s",
                                      SEND_FILE_LOC,
                                      str_diff_time);
        }
   else if (db.protocol & HTTP_FLAG)
        {
           prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                      "%s%s",
                                      SEND_FILE_HTTP,
                                      str_diff_time);
        }
   else if (db.protocol & SFTP_FLAG)
        {
           if (db.no_of_restart_files > 0)
           {
              int append_file_number = -1,
                  ii;

              for (ii = 0; ii < db.no_of_restart_files; ii++)
              {
                 if (CHECK_STRCMP(db.restart_file[ii], d_name) == 0)
                 {
                    append_file_number = ii;
                    break;
                 }
              }
              if (append_file_number != -1)
              {
                 remove_append(db.job_id,
                               db.restart_file[append_file_number]);
              }
           }
           prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                      "%s%s",
                                      SEND_FILE_SFTP,
                                      str_diff_time);
        }
#  ifdef _WITH_SCP_SUPPORT
   else if (db.protocol & SCP_FLAG)
        {
           prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                      "%s%s",
                                      SEND_FILE_SCP,
                                      str_diff_time);
        }
#  endif
#  ifdef _WITH_WMO_SUPPORT
   else if (db.protocol & WMO_FLAG)
        {
           prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                      "%s%s",
                                      SEND_FILE_WMO,
                                      str_diff_time);
        }
#  endif
#  ifdef _WITH_MAP_SUPPORT
   else if (db.protocol & MAP_FLAG)
        {
           prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                      "%s%s",
                                      SEND_FILE_MAP,
                                      str_diff_time);
        }
#  endif
   else if (db.protocol & SMTP_FLAG)
        {
           prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                      "%s%s",
                                      SEND_FILE_SMTP,
                                      str_diff_time);
        }
        else
        {
           prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                      "sf_???%s", str_diff_time);
        }

   dl_real_size = *dl.file_name_length + dl.size +
                  prog_name_length;
   if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "write() error : %s", strerror(errno));
   }
# endif

   return;
}
#endif

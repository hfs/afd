/*
 *  get_file_names.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **
 */
DESCR__E_M3

#include <stdio.h>                     /* NULL                           */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* realloc(), malloc(), free()    */
#ifdef _AGE_LIMIT
#include <time.h>                      /* time()                         */
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>                    /* opendir(), closedir(), DIR,    */
                                       /* readdir(), struct dirent       */
#include <fcntl.h>
#include <unistd.h>                    /* unlink(), getpid()             */
#include <errno.h>
#include "fddefs.h"

/* Global variables */
extern int                        transfer_log_fd,
                                  fsa_fd;
extern off_t                      *file_size_buffer;
extern char                       *p_work_dir,
#ifdef _AGE_LIMIT
                                  tr_hostname[MAX_HOSTNAME_LENGTH + 1],
#endif
                                  *file_name_buffer;
extern struct filetransfer_status *fsa;
extern struct job                 db;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif

#define _WITH_FILE_NAME_SORTING


/*########################## get_file_names() ###########################*/
int
get_file_names(char *file_path, off_t *file_size_to_send)
{
#ifdef _AGE_LIMIT
   int           files_not_send = 0;
   off_t         file_size_not_send = 0;
   time_t        diff_time,
                 now;
#endif /* _AGE_LIMIT */
   int           files_to_send = 0,
                 new_size,
                 offset;
   off_t         *p_file_size;
#ifdef _WITH_FILE_NAME_SORTING
   time_t        *mtime_buffer = NULL,
                 *p_mtime;
#endif /* _WITH_FILE_NAME_SORTING */
   char          *p_file_name,
                 *p_source_file,
                 fullname[MAX_PATH_LENGTH];
   struct stat   stat_buf;
   struct dirent *p_dir;
   DIR           *dp;

   /*
    * Create directory name in which we can find the files for this job.
    */
   (void)strcpy(file_path, p_work_dir);
   (void)strcat(file_path, AFD_FILE_DIR);
   if (db.error_file == YES)
   {
#ifdef _BURST_MODE
      fsa[db.fsa_pos].job_status[(int)db.job_no].error_file = YES;
#endif
      (void)strcat(file_path, ERROR_DIR);
      (void)strcat(file_path, "/");
      (void)strcat(file_path, db.host_alias);
   }
#ifdef _BURST_MODE
   else
   {
      fsa[db.fsa_pos].job_status[(int)db.job_no].error_file = NO;
   }
#endif
   (void)strcat(file_path, "/");
   (void)strcat(file_path, db.msg_name);

   if ((dp = opendir(file_path)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not opendir() %s : %s", file_path, strerror(errno));
      exit(OPEN_FILE_DIR_ERROR);
   }

   /* Prepare pointers and directory name */
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
#ifdef _WITH_FILE_NAME_SORTING
   p_mtime = mtime_buffer;
#endif /* _WITH_FILE_NAME_SORTING */
#ifdef _AGE_LIMIT
   if (db.age_limit > 0)
   {
      now = time(NULL);
   }
#endif /* _AGE_LIMIT */

   /*
    * Now let's determine the number of files that have to be
    * transmitted and the total size.
    */
   errno = 0;
   while ((p_dir = readdir(dp)) != NULL)
   {
      if (p_dir->d_name[0] == '.')
      {
         continue;
      }

      (void)strcpy(p_source_file, p_dir->d_name);
      if (stat(fullname, &stat_buf) < 0)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Can't stat() file %s : %s", fullname, strerror(errno));
         continue;
      }

      /* Sure it is a normal file? */
      if (S_ISREG(stat_buf.st_mode))
      {
#ifdef _AGE_LIMIT
         /* Don't send files older then age_limit! */
         if ((db.age_limit > 0) &&
             ((diff_time = (now - stat_buf.st_mtime)) > db.age_limit))
         {
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
                     remove_append(db.job_id, db.restart_file[ii]);
                     break;
                  }
               }
            }
            if (unlink(fullname) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to unlink() file %s due to age : %s",
                          p_dir->d_name, strerror(errno));
            }
            else
            {
#ifdef _DELETE_LOG
               int    prog_name_length;
               size_t dl_real_size;

               if (dl.fd == -1)
               {
                  delete_log_ptrs(&dl);
               }
               (void)strcpy(dl.file_name, p_dir->d_name);
               (void)sprintf(dl.host_name, "%-*s %x",
                             MAX_HOSTNAME_LENGTH,
                             fsa[db.fsa_pos].host_dsp_name, AGE_OUTPUT);
               *dl.file_size = stat_buf.st_size;
               *dl.job_number = db.job_id;
               *dl.file_name_length = strlen(p_dir->d_name);
               if (db.protocol & FTP_FLAG)
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
                  prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                             "%s >%ld",
                                             SEND_FILE_FTP, diff_time);
               }
               else if (db.protocol & LOC_FLAG)
                    {
                       prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                                  "%s >%ld",
                                                  SEND_FILE_LOC, diff_time);
                    }
#ifdef _WITH_SCP1_SUPPORT
              else if (db.protocol & SCP1_FLAG)
                   {
                       prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                                  "%s >%ld",
                                                  SEND_FILE_SCP1, diff_time);
                   }
#endif /* _WITH_SCP1_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
              else if (db.protocol & WMO_FLAG)
                   {
                       prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                                  "%s >%ld",
                                                  SEND_FILE_WMO, diff_time);
                   }
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
              else if (db.protocol & MAP_FLAG)
                   {
                       prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                                  "%s >%ld",
                                                  SEND_FILE_MAP, diff_time);
                   }
#endif /* _WITH_MAP_SUPPORT */
               else if (db.protocol & SMTP_FLAG)
                    {
                       prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                                  "%s >%ld",
                                                  SEND_FILE_SMTP, diff_time);
                    }
                    else
                    {
                       prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                                  "sf_??? >%ld", diff_time);
                    }

               dl_real_size = *dl.file_name_length + dl.size + prog_name_length;
               if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "write() error : %s", strerror(errno));
               }
#endif /* _DELETE_LOG */
               files_not_send++;
               file_size_not_send += stat_buf.st_size;
            }
         }
         else
         {
#endif /* _AGE_LIMIT */
            if ((files_to_send % 10) == 0)
            {
               /* Increase the space for the file name buffer */
               new_size = ((files_to_send / 10) + 1) *
                          10 * MAX_FILENAME_LENGTH;
               offset = p_file_name - file_name_buffer;
               if ((file_name_buffer = realloc(file_name_buffer, new_size)) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not realloc() memory : %s",
                             strerror(errno));
                  exit(ALLOC_ERROR);
               }
               p_file_name = file_name_buffer + offset;

               /* Increase the space for the file size buffer */
               new_size = ((files_to_send / 10) + 1) * 10 * sizeof(off_t);
               offset = (char *)p_file_size - (char *)file_size_buffer;
               if ((file_size_buffer = realloc(file_size_buffer, new_size)) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not realloc() memory : %s",
                             strerror(errno));
                  free(file_name_buffer);
                  exit(ALLOC_ERROR);
               }
               p_file_size = (off_t *)((char *)file_size_buffer + offset);

#ifdef _WITH_FILE_NAME_SORTING
               /* Increase the space for the mtime buffer. */
               new_size = ((files_to_send / 10) + 1) * 10 * sizeof(time_t);
               offset = (char *)p_mtime - (char *)mtime_buffer;
               if ((mtime_buffer = realloc(mtime_buffer, new_size)) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not realloc() memory : %s",
                             strerror(errno));
                  free(file_name_buffer);
                  free(file_size_buffer);
                  exit(ALLOC_ERROR);
               }
               p_mtime = (time_t *)((char *)mtime_buffer + offset);
#endif /* _WITH_FILE_NAME_SORTING */
            }

#ifdef _WITH_FILE_NAME_SORTING
            /* Sort the files, newest must be last (FIFO). */
            if ((files_to_send > 1) && (*(p_mtime - 1) > stat_buf.st_mtime))
            {
               int    i;
               off_t  *sp_file_size = p_file_size - 1;
               time_t *sp_mtime = p_mtime - 1;
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
               (void)strcpy(sp_file_name + MAX_FILENAME_LENGTH, p_dir->d_name);
            }
            else
            {
#endif /* _WITH_FILE_NAME_SORTING */
               (void)strcpy(p_file_name, p_dir->d_name);
               *p_file_size = stat_buf.st_size;
               *p_mtime = stat_buf.st_mtime;
#ifdef _WITH_FILE_NAME_SORTING
            }
#endif /* _WITH_FILE_NAME_SORTING */
            p_file_name += MAX_FILENAME_LENGTH;
            p_file_size++;
            p_mtime++;
            files_to_send++;
            *file_size_to_send += stat_buf.st_size;
#ifdef _AGE_LIMIT
         }
#endif
      }
      errno = 0;
   }

   if (errno)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not readdir() %s : %s", file_path, strerror(errno));
   }

#ifdef _WITH_FILE_NAME_SORTING
   if (mtime_buffer != NULL)
   {
      free(mtime_buffer);
   }
#endif /* _WITH_FILE_NAME_SORTING */

#ifdef _AGE_LIMIT
   if (files_not_send > 0)
   {
#ifdef _VERIFY_FSA
      unsigned int ui_variable;
#endif

      /* Total file counter */
      lock_region_w(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_counter - (char *)fsa);
      fsa[db.fsa_pos].total_file_counter -= files_not_send;
#ifdef _VERIFY_FSA
      if (fsa[db.fsa_pos].total_file_counter < 0)
      {
         system_log(INFO_SIGN, __FILE__, __LINE__,
                    "Total file counter for host %s less then zero. Correcting.",
                    fsa[db.fsa_pos].host_dsp_name);
         fsa[db.fsa_pos].total_file_counter = 0;
      }
#endif
      unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_counter - (char *)fsa);

      /* Total file size */
      lock_region_w(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_size - (char *)fsa);
#ifdef _VERIFY_FSA
      ui_variable = fsa[db.fsa_pos].total_file_size;
#endif
      fsa[db.fsa_pos].total_file_size -= file_size_not_send;
#ifdef _VERIFY_FSA
      if (fsa[db.fsa_pos].total_file_size > ui_variable)
      {
         system_log(INFO_SIGN, __FILE__, __LINE__,
                    "Total file size for host %s overflowed. Correcting.",
                    fsa[db.fsa_pos].host_dsp_name);
         fsa[db.fsa_pos].total_file_size = 0;
      }
      else if ((fsa[db.fsa_pos].total_file_counter == 0) &&
               (fsa[db.fsa_pos].total_file_size > 0))
           {
              system_log(INFO_SIGN, __FILE__, __LINE__,
                         "fc for host %s is zero but fs is not zero. Correcting.",
                         fsa[db.fsa_pos].host_dsp_name);
              fsa[db.fsa_pos].total_file_size = 0;
           }
#endif
      unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_size - (char *)fsa);

      (void)rec(transfer_log_fd, INFO_SIGN,
                "%-*s[%c]: Deleted %d files (%d Bytes) due to age.\n",
                MAX_HOSTNAME_LENGTH, tr_hostname, db.job_no + 48,
                files_not_send, file_size_not_send);
   }
#endif /* _AGE_LIMIT */

   if (closedir(dp) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not closedir() %s : %s", file_path, strerror(errno));
      free(file_name_buffer); free(file_size_buffer);
      file_name_buffer = NULL;
      file_size_buffer = NULL;
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

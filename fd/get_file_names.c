/*
 *  get_file_names.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   and the directory where the files have been found.  Otherwise if
 **   an error occurs it will exit.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.12.1996 H.Kiehl Created
 **   31.01.1997 H.Kiehl Support for age limit.
 **   14.10.1998 H.Kiehl Free unused memory.
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
#include <unistd.h>                    /* remove(), getpid()             */
#include <errno.h>
#include "fddefs.h"

/* Global variables */
extern int                        sys_log_fd,
                                  transfer_log_fd,
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


/*########################## get_file_names() ###########################*/
int
get_file_names(char *file_path, off_t *file_size_to_send)
{
#ifdef _AGE_LIMIT
   int           files_not_send = 0;
   off_t         file_size_not_send = 0;
   time_t        diff_time;
#endif /* _AGE_LIMIT */
   int           files_to_send = 0,
                 new_size,
                 offset;
   off_t         *p_file_size;
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
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not opendir() %s : %s (%s %d)\n",
                file_path, strerror(errno), __FILE__, __LINE__);
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
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Can't access file %s : %s (%s %d)\n",
                   fullname, strerror(errno), __FILE__, __LINE__);
         continue;
      }

      /* Sure it is a normal file? */
      if (S_ISREG(stat_buf.st_mode))
      {
#ifdef _AGE_LIMIT
         /* Don't send files older then age_limit! */
         if ((db.age_limit > 0) &&
             ((diff_time = (time(NULL) - stat_buf.st_mtime)) > db.age_limit))
         {
            if (remove(fullname) == -1)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to remove file %s due to age : %s (%s %d)\n",
                         p_dir->d_name, strerror(errno), __FILE__, __LINE__);
            }
            else
            {
#ifdef _DELETE_LOG
               int    prog_name_length;
               size_t dl_real_size;

               (void)strcpy(dl.file_name, p_dir->d_name);
               (void)sprintf(dl.host_name, "%-*s %x",
                             MAX_HOSTNAME_LENGTH,
                             fsa[db.fsa_pos].host_dsp_name,
                             AGE_OUTPUT);
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
                        if (strcmp(db.restart_file[ii], p_dir->d_name) == 0)
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
                                             SEND_FILE_FTP,
                                             diff_time);
               }
               else if (db.protocol & LOC_FLAG)
                    {
                       prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                                  "%s >%ld",
                                                  SEND_FILE_LOC,
                                                  diff_time);
                    }
#ifdef _WITH_WMO_SUPPORT
              else if (db.protocol & WMO_FLAG)
                   {
                       prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                                  "%s >%ld",
                                                  SEND_FILE_WMO,
                                                  diff_time);
                   }
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
              else if (db.protocol & MAP_FLAG)
                   {
                       prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                                  "%s >%ld",
                                                  SEND_FILE_MAP,
                                                  diff_time);
                   }
#endif /* _WITH_MAP_SUPPORT */
               else if (db.protocol & SMTP_FLAG)
                    {
                       prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                                  "%s >%ld",
                                                  SEND_FILE_SMTP,
                                                  diff_time);
                    }
                    else
                    {
                       prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                                  "sf_??? >%ld",
                                                  diff_time);
                    }

               dl_real_size = *dl.file_name_length + dl.size + prog_name_length;
               if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
               {
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "write() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
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
               /* Calculate new size of file name buffer */
               new_size = ((files_to_send / 10) + 1) *
                          10 * MAX_FILENAME_LENGTH;

               /* Increase the space for the file name buffer */
               offset = p_file_name - file_name_buffer;
               if ((file_name_buffer = realloc(file_name_buffer, new_size)) == NULL)
               {
                  (void)rec(sys_log_fd, FATAL_SIGN,
                            "Could not realloc() memory : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
                  exit(ALLOC_ERROR);
               }

               /* After realloc, don't forget to position */
               /* pointer correctly.                      */
               p_file_name = file_name_buffer + offset;

               /* Calculate new size of file size buffer */
               new_size = ((files_to_send / 10) + 1) * 10 * sizeof(off_t);

               /* Increase the space for the file size buffer */
               offset = (char *)p_file_size - (char *)file_size_buffer;
               if ((file_size_buffer = realloc(file_size_buffer, new_size)) == NULL)
               {
                  (void)rec(sys_log_fd, FATAL_SIGN,
                            "Could not realloc() memory : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
                  free(file_name_buffer);
                  file_name_buffer = NULL;
                  exit(ALLOC_ERROR);
               }

               /* After realloc, don't forget to position */
               /* pointer correctly.                      */
               p_file_size = (off_t *)((char *)file_size_buffer + offset);
            }
            (void)strcpy(p_file_name, p_dir->d_name);
            *p_file_size = stat_buf.st_size;
            p_file_name += MAX_FILENAME_LENGTH;
            p_file_size++;
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
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not readdir() %s : %s (%s %d)\n",
                file_path, strerror(errno), __FILE__, __LINE__);
   }

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
         (void)rec(sys_log_fd, INFO_SIGN,
                   "Total file counter for host %s less then zero. Correcting. (%s %d)\n",
                   fsa[db.fsa_pos].host_dsp_name, __FILE__, __LINE__);
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
         (void)rec(sys_log_fd, INFO_SIGN,
                   "Total file size for host %s overflowed. Correcting. (%s %d)\n",
                   fsa[db.fsa_pos].host_dsp_name, __FILE__, __LINE__);
         fsa[db.fsa_pos].total_file_size = 0;
      }
      else if ((fsa[db.fsa_pos].total_file_counter == 0) &&
               (fsa[db.fsa_pos].total_file_size > 0))
           {
              (void)rec(sys_log_fd, INFO_SIGN,
                        "fc for host %s is zero but fs is not zero. Correcting. (%s %d)\n",
                        fsa[db.fsa_pos].host_dsp_name, __FILE__, __LINE__);
              fsa[db.fsa_pos].total_file_size = 0;
           }
#endif
      unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_size - (char *)fsa);

      (void)rec(transfer_log_fd, INFO_SIGN,
                "%-*s[%c]: Deleted %d files (%d Bytes) due to age.\n",
                MAX_HOSTNAME_LENGTH, tr_hostname, db.job_no + 48,
                files_not_send,
                file_size_not_send);
   }
#endif /* _AGE_LIMIT */

   if (closedir(dp) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not closedir() %s : %s (%s %d)\n",
                file_path, strerror(errno), __FILE__, __LINE__);
      free(file_name_buffer); free(file_size_buffer);
      file_name_buffer = NULL;
      file_size_buffer = NULL;
      exit(OPEN_FILE_DIR_ERROR);
   }

   return(files_to_send);
}

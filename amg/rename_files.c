/*
 *  rename_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   rename_files - renames files from one directory to another
 **                  unique directory
 **
 ** SYNOPSIS
 **   int rename_files(char                   *src_file_path,
 **                    char                   *dest_file_path,
 **                    struct directory_entry *p_de,
 **                    struct instant_db      *p_db,
 **                    time_t                 *creation_time,
 **                    unsigned short         *unique_number,
 **                    int                    pos_in_fm,
 **                    int                    no_of_files,
 **                    char                   *unique_name,
 **                    off_t                  *file_size_renamed,
 **                    int                    file_offset)
 **
 ** DESCRIPTION
 **   The function rename_files() renames 'no_of_files' from the directory
 **   'src_file_path' to the AFD file directory. To keep files of each
 **   job unique it has to create a unique directory name. This name is
 **   later also used to create the message name.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to rename the user files. Otherwise
 **   it returns the number of files that were renamed, the total size of
 **   all files 'file_size_renamed' and the unique name 'unique_name'
 **   that was generated.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.12.1996 H.Kiehl Created
 **   18.10.1997 H.Kiehl Introduced inverse filtering.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include <string.h>                /* strcpy(), strlen(), strerror()     */
#include <time.h>                  /* time()                             */
#include <sys/types.h>
#include <unistd.h>                /* link()                             */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int             counter_fd,
                       sys_log_fd;
#ifndef _WITH_PTHREAD
extern off_t           *file_size_pool;
extern char            **file_name_pool;
#endif


/*########################## rename_files() #############################*/
int
rename_files(char                   *src_file_path,
             char                   *dest_file_path,
#ifdef _WITH_PTHREAD
             off_t                  *file_size_pool,
             char                   **file_name_pool,
#endif
             struct directory_entry *p_de,
             struct instant_db      *p_db,
             time_t                 *creation_time,
             unsigned short         *unique_number,
             int                    pos_in_fm,
             int                    no_of_files,
             char                   *unique_name, /* Storage to return unique name. */
             off_t                  *file_size_renamed,
             int                    file_offset)
{
   int          files_renamed = 0;
   register int i,
                j,
                ret;
   char         *p_src = NULL,
                *p_dest = NULL,
                *p_dest_end = NULL;

   *file_size_renamed = 0;

   for (i = file_offset; i < (file_offset + no_of_files); i++)
   {
      for (j = 0; j < p_de->fme[pos_in_fm].nfm; j++)
      {
         /*
          * Actually we could just read the source directory
          * and rename 'no_of_files' to the new directory.
          * This however involves several system calls (opendir(),
          * readdir(), closedir()), ie high system load. This
          * we hopefully reduce by using the array file_name_pool
          * and pmatch() to get the names we need. Let's see
          * how things work.
          */
         if ((ret = pmatch(p_de->fme[pos_in_fm].file_mask[j],
                           file_name_pool[i])) == 0)
         {
            /* Only create a unique name and the corresponding */
            /* directory when we have found a file that is to  */
            /* distributed.                                    */
            if (p_src == NULL)
            {
               /* Create a new message name and directory. */
               *creation_time = time(NULL);
               if (create_name(dest_file_path, p_db->priority,
                               *creation_time, p_db->job_id,
                               unique_number, unique_name) < 0)
               {
                  if (errno == ENOSPC)
                  {
                     (void)rec(sys_log_fd, ERROR_SIGN,
                               "DISK FULL!!! Will retry in %d second interval. (%s %d)\n",
                               DISK_FULL_RESCAN_TIME, __FILE__, __LINE__);

                     while (errno == ENOSPC)
                     {
                        (void)sleep(DISK_FULL_RESCAN_TIME);
                        *creation_time = time(NULL);
                        errno = 0;
                        if (create_name(dest_file_path, p_db->priority,
                                        *creation_time, p_db->job_id,
                                        unique_number, unique_name) < 0)
                        {
                           if (errno != ENOSPC)
                           {
                              (void)rec(sys_log_fd, FATAL_SIGN,
                                        "Failed to create a unique name : %s (%s %d)\n",
                                        strerror(errno), __FILE__, __LINE__);
                              exit(INCORRECT);
                           }
                        }
                     }

                     (void)rec(sys_log_fd, INFO_SIGN,
                               "Continuing after disk was full. (%s %d)\n",
                               __FILE__, __LINE__);
                  }
                  else
                  {
                     (void)rec(sys_log_fd, ERROR_SIGN,
                               "Failed to create a unique name : %s (%s %d)\n",
                               strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
               }
               p_src = src_file_path + strlen(src_file_path);
               p_dest_end = dest_file_path + strlen(dest_file_path);
               (void)strcpy(p_dest_end, (unique_name - 1));
               p_dest = p_dest_end + strlen((unique_name - 1));
               *(p_dest++) = '/';
               *p_dest = '\0';
            }

            (void)strcpy(p_src, file_name_pool[i]);
            (void)strcpy(p_dest, file_name_pool[i]);

            /* rename the file */
            if (rename(src_file_path, dest_file_path) < 0)
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Failed to rename() file %s to %s : %s (%s %d)\n",
                         src_file_path, dest_file_path, strerror(errno),
                         __FILE__, __LINE__);
            }
            else
            {
               files_renamed++;
               *file_size_renamed += file_size_pool[i];
            }

            /*
             * Since the file is already in the file directory
             * no need to test any further filters.
             */
            break;
         }
         else if (ret == 1)
              {
                 /*
                  * This file is definitely NOT wanted, no matter what the
                  * following filters say.
                  */
                 break;
              }
      } /* for (j = 0; j < p_de->fme[pos_in_fm].nfm; j++) */
   } /* for (i = 0; i < (file_offset + no_of_files); i++) */

   /* Keep source and destination directories clean so */
   /* that other functions can work with them.         */
   if (p_dest_end != NULL)
   {
      *p_dest_end = '\0';
   }
   if (p_src != NULL)
   {
      *p_src = '\0';
   }

   return(files_renamed);
}

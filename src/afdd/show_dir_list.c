/*
 *  show_dir_list.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_dir_list - Tells which directories are being monitored
 **
 ** SYNOPSIS
 **   void show_dir_list(FILE *p_data)
 **
 ** DESCRIPTION
 **   This function prints a list of all directories currently being
 **   monitored by this AFD in the following format:
 **     DL <dir_number> <dir ID> <dir alias> <dir name> [<original dir name>]
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.10.2006 H.Kiehl Created
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
#include <unistd.h>
#include <errno.h>
#include "afdddefs.h"

/* External global variables */
extern int                        no_of_dirs;
extern char                       *p_work_dir;
extern struct fileretrieve_status *fra;


/*########################### show_dir_list() ###########################*/
void
show_dir_list(FILE *p_data)
{
   int  fd;
   char fullname[MAX_PATH_LENGTH];

   (void)fprintf(p_data, "211- AFD directory list:\r\n");
   (void)fflush(p_data);

   (void)fprintf(p_data, "ND %d\r\n", no_of_dirs);
   (void)fflush(p_data);

   (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR, DIR_NAME_FILE);
   if ((fd = open(fullname, O_RDONLY)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s", fullname, strerror(errno));
   }
   else
   {
      struct stat stat_buf;

      if (fstat(fd, &stat_buf) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to fstat() `%s' : %s", fullname, strerror(errno));
      }
      else
      {
         if (stat_buf.st_size > AFD_WORD_OFFSET)
         {
            char *ptr;

#ifdef HAVE_MMAP
            if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                            MAP_SHARED, fd, 0)) == (caddr_t)-1)
#else
            if ((ptr = mmap_emu(0, stat_buf.st_size, PROT_READ,
                                MAP_SHARED, fullname, 0)) == (caddr_t)-1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to mmap() to `%s' : %s",
                          fullname, strerror(errno));
            }
            else
            {
               int                 *no_of_dir_names;
               struct dir_name_buf *dnb;

               no_of_dir_names = (int *)ptr;
               ptr += AFD_WORD_OFFSET;
               dnb = (struct dir_name_buf *)ptr;

               if (*no_of_dir_names > 0)
               {
                  int i, k;

                  for (i = 0; i < no_of_dirs; i++)
                  {
                     for (k = 0; k < *no_of_dir_names; k++)
                     {
                        if (fra[i].dir_id == dnb[k].dir_id)
                        {
                           (void)fprintf(p_data, "DL %d %x %s %s %s\r\n",
                                         i, fra[i].dir_id, fra[i].dir_alias,
                                         dnb[k].dir_name, dnb[k].orig_dir_name);
                           (void)fflush(p_data);
                           break;
                        }
                     }
                  }
               }

               ptr -= AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
               if (munmap(ptr, stat_buf.st_size) == -1)
#else
               if (munmap_emu(ptr) == -1)
#endif
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to munmap() `%s' : %s",
                             fullname, strerror(errno));
               }
            }
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Hmmm, `%s' is less then %d bytes long.",
                       fullname, AFD_WORD_OFFSET);
         }
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to close() `%s' : %s",
                    fullname, strerror(errno));
      }
   }

   return;
}

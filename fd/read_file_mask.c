/*
 *  read_file_mask.c - Part of AFD, an automatic file distribution
 *                     program.
 *  Copyright (c) 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   read_file_mask - reads all file masks from a file
 **
 ** SYNOPSIS
 **   int read_file_mask(char *dir_alias, int *nfg, struct file_mask **fml)
 **
 ** DESCRIPTION
 **   The function read_file_mask reads all file masks from the
 **   file:
 **       $AFD_WORK_DIR/files/incoming/filters/<fra_pos>
 **   It reads the data into a buffer that this function will
 **   create.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when it manages to store the data. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>            /* sprintf()                               */
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* malloc()                                */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern int  sys_log_fd;
extern char *p_work_dir;


/*########################### read_file_mask() ##########################*/
int
read_file_mask(char *dir_alias, int *nfg, struct file_mask **fml)
{
   int         fd,
               i;
   char        file_mask_file[MAX_PATH_LENGTH];
   struct stat stat_buf;

   (void)sprintf(file_mask_file, "%s%s%s%s/%s", p_work_dir, AFD_FILE_DIR,
                 INCOMING_DIR, FILE_MASK_DIR, dir_alias);

   if ((fd = lock_file(file_mask_file, ON)) < 0)
   {
      return(INCORRECT);
   }
   if (fstat(fd, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to fstat() %s : %s (%s %d)\n",
                file_mask_file, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }
   *nfg = stat_buf.st_size / sizeof(struct file_mask);
   if ((*fml = malloc(stat_buf.st_size)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   for (i = 0; i < *nfg; i++)
   {
      if (read(fd, fml[i], sizeof(struct file_mask)) != sizeof(struct file_mask))
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to read() from %s : %s (%s %d)\n",
                   file_mask_file, strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
   }
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "Failed to close() %s : %s (%s %d)\n",
                file_mask_file, strerror(errno), __FILE__, __LINE__);
   }

   return(SUCCESS);
}

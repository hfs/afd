/*
 *  store_file_mask.c - Part of AFD, an automatic file distribution
 *                      program.
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
 **   store_file_mask - stores all file mask to a file
 **
 ** SYNOPSIS
 **   void store_file_mask(int dir_no, struct dir_group *dir)
 **
 ** DESCRIPTION
 **   The function store_file_mask stores all file mask for the
 **   directory 'dir_no' into the file:
 **       $AFD_WORK_DIR/files/incoming/filters/<dir_alias>
 **   For each file group it stores all file mask in a block
 **   of MAX_FILE_MASK_BUFFER Bytes, where each file mask is
 **   separated by a binary zero.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>            /* sprintf()                               */
#include <string.h>           /* strerror()                              */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int  sys_log_fd;
extern char *p_work_dir;


/*########################## store_file_mask() ##########################*/
void
store_file_mask(char *dir_alias, struct dir_group *dir)
{
   int              fd,
                    i;
   char             file_mask_file[MAX_PATH_LENGTH];
   struct stat      stat_buf;
   struct file_mask fm;

   (void)sprintf(file_mask_file, "%s%s%s%s/%s", p_work_dir, AFD_FILE_DIR,
                 INCOMING_DIR, FILE_MASK_DIR, dir_alias);

   if ((stat(file_mask_file, &stat_buf) == -1) && (errno == ENOENT))
   {
      if ((fd = coe_open(file_mask_file, (O_RDWR | O_CREAT | O_TRUNC),
                         (S_IRUSR | S_IWUSR))) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to coe_open() %s : %s (%s %d)\n",
                   file_mask_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
      lock_region_w(fd, 0);
   }
   else
   {
      if ((fd = lock_file(file_mask_file, ON)) < 0)
      {
         return;
      }
   }

   for (i = 0; i < dir->fgc; i++)
   {
      fm.nfm = dir->file[i].fc;
      (void)memcpy(fm.file_list, dir->file[i].files, MAX_FILE_MASK_BUFFER);
      if (write(fd, &fm, sizeof(struct file_mask)) != sizeof(struct file_mask))
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to write() to %s : %s (%s %d)\n",
                   file_mask_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
   }
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "Failed to close() %s : %s (%s %d)\n",
                file_mask_file, strerror(errno), __FILE__, __LINE__);
   }

   return;
}

/*
 *  attach_buf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   attach_buf - maps to a file
 **
 ** SYNOPSIS
 **   void *attach_buf(char   *file,
 **                    int    *fd,
 **                    size_t new_size,
 **                    char   *prog_name)
 **
 ** DESCRIPTION
 **   The function attach_buf() attaches to the file 'file'. If
 **   the file does not exist, it is created and a binary int
 **   zero is written to the first four bytes.
 **
 ** RETURN VALUES
 **   On success, attach_buf() returns a pointer to the mapped area.
 **   On error, MAP_FAILED (-1) is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.01.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>
#include <stdlib.h>                 /* exit()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* fstat(), lseek(), write()         */
#include <sys/mman.h>               /* mmap()                            */
#include <errno.h>

/* External global variables */
extern int  sys_log_fd;


/*############################ attach_buf() #############################*/
void *
attach_buf(char *file, int *fd, size_t new_size, char *prog_name)
{
   struct stat stat_buf;

   /*
    * This buffer has no open file descriptor, so it we do not
    * need to unmap from the area. However this can be the
    * first time that the this process is started and no buffer
    * exists, then we need to create it.
    */
   if ((*fd = coe_open(file, O_RDWR | O_CREAT, FILE_MODE)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to open() and create %s : %s (%s %d)\n",
                file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (fstat(*fd, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to fstat() %s : %s (%s %d)\n",
                file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (stat_buf.st_size < new_size)
   {
      int buf_size = 0;

      if (write(*fd, &buf_size, sizeof(int)) != sizeof(int))
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to write() to %s : %s (%s %d)\n",
                   file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (lseek(*fd, new_size - 1, SEEK_SET) == -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to lseek() %s : %s (%s %d)\n",
                   file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (write(*fd, "", 1) != 1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to write() to %s : %s (%s %d)\n",
                   file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

      /*
       * Lock the file. That way it is not possible for two same
       * processes to run at the same time.
       */
      if (prog_name != NULL)
      {
         if (lock_region(*fd, 0) == IS_LOCKED)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Another %s is already running. Terminating. (%s %d)\n",
                      prog_name, __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
   }
   else
   {
      new_size = stat_buf.st_size;
   }

   return(mmap(0, new_size, (PROT_READ | PROT_WRITE), MAP_SHARED, *fd, 0));
}

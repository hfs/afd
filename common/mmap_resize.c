/*
 *  mmap_resize.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mmap_resize - resizes a memory mapped area
 **
 ** SYNOPSIS
 **   void *mmap_resize(int fd, void *area, size_t new_size)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   On success, mmap_resize() returns a pointer to the mapped area.
 **   On error, MAP_FAILED (-1) is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.01.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>
#include <unistd.h>     /* lseek(), write(), ftruncate()                 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>   /* mmap(), msync(), munmap()                     */
#include <errno.h>
#include "fddefs.h"


/*############################ mmap_resize() ############################*/
void *
mmap_resize(int fd, void *area, size_t new_size)
{
   struct stat stat_buf;

   if (fstat(fd, &stat_buf) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "fstat() error : %s", strerror(errno));
      return((void *)-1);
   }

   /* Always unmap from current mmapped area. */
   if (stat_buf.st_size > 0)
   {
      if (msync(area, stat_buf.st_size, MS_ASYNC) == -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "msync() error : %s", strerror(errno));
         return((void *)-1);
      }
      if (munmap(area, stat_buf.st_size) == -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "munmap() error : %s", strerror(errno));
         return((void *)-1);
      }
   }

   if (new_size > stat_buf.st_size)
   {
      if (lseek(fd, new_size - 1, SEEK_SET) == -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "lseek() error : %s", strerror(errno));
         return((void *)-1);
      }
      if (write(fd, "", 1) != 1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
         return((void *)-1);
      }
   }
   else if (new_size < stat_buf.st_size)
        {
           if (ftruncate(fd, new_size) == -1)
           {
              system_log(FATAL_SIGN, __FILE__, __LINE__,
                         "ftruncate() error : %s", strerror(errno));
              return((void *)-1);
           }
        }

   return(mmap(0, new_size, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, 0));
}

/*
 *  map_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   map_file - uses mmap() to map to a file
 **
 ** SYNOPSIS
 **   void *map_file(char *file, int *fd)
 **
 ** DESCRIPTION
 **   The function map_file() attaches to the file 'file'. If
 **   the file does not exist, it is created.
 **
 ** RETURN VALUES
 **   On success, map_file() returns a pointer to the mapped area.
 **   On error, INCORRECT (-1) is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.06.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>                 /* strerror()                        */
#include <stdlib.h>                 /* exit()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* fstat()                           */
#include <sys/mman.h>               /* mmap()                            */
#include <errno.h>


/*############################# map_file() ##############################*/
void *
map_file(char *file, int *fd)
{
   struct stat stat_buf;

   if ((*fd = coe_open(file, O_RDWR | O_CREAT, FILE_MODE)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to open() + create <%s> : %s", file, strerror(errno));
      exit(INCORRECT);
   }

   if (fstat(*fd, &stat_buf) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to fstat() <%s> : %s", file, strerror(errno));
      exit(INCORRECT);
   }

   if (stat_buf.st_size == 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "It doesn't make sense to mmap() to a file of zero length.");
      exit(INCORRECT);
   }

   return(mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE), MAP_SHARED, *fd, 0));
}

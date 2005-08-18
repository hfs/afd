/*
 *  read_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   read_file - reads the contents of a file into a buffer
 **
 ** SYNOPSIS
 **   off_t read_file(char *filename, char **buffer)
 **
 ** DESCRIPTION
 **   The function reads the contents of the file filename into a
 **   buffer for which it allocates memory. The caller of this function
 **   is responsible for releasing the memory.
 **
 ** RETURN VALUES
 **   The size of file filename when successful, otherwise INCORRECT
 **   will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.12.1997 H.Kiehl Created
 **   01.08.1999 H.Kiehl Return the size of the file.
 **   19.07.2001 H.Kiehl Removed fstat().
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>               /* calloc(), free()                    */
#include <unistd.h>               /* close(), read()                     */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <errno.h>

#define MAX_HUNK 4096


/*############################# read_file() #############################*/
off_t
read_file(char *filename, char **buffer)
{
   int   bytes_read,
         fd;
   off_t bytes_buffered;
   char  *read_ptr;

   /* Open file. */
   if ((fd = open(filename, O_RDONLY)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open `%s' : %s", filename, strerror(errno));
      return(INCORRECT);
   }

   /* Allocate enough memory for the contents of the file. */
   if ((*buffer = malloc(MAX_HUNK + 1)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not malloc() memory : %s", strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }
   bytes_buffered = 0;
   read_ptr = *buffer;

   /* Read file into buffer. */
   do
   {
      if ((bytes_read = read(fd, read_ptr, MAX_HUNK)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to read `%s' : %s", filename, strerror(errno));
         (void)close(fd);
         free(*buffer);
         return(INCORRECT);
      }
      bytes_buffered += bytes_read;
      if (bytes_read == MAX_HUNK)
      {
         if ((*buffer = realloc(*buffer,
                                bytes_buffered + MAX_HUNK + 1)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not realloc() memory : %s", strerror(errno));
            (void)close(fd);
            return(INCORRECT);
         }
         read_ptr = *buffer + bytes_buffered;
      }
   } while (bytes_read == MAX_HUNK);
   (*buffer)[bytes_buffered] = '\0';

   /* Close file. */
   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   return(bytes_buffered);
}

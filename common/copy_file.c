/*
 *  copy_file.c - Part of AFD, an automatic file distribution program.
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
 **   copy_file - copies a file from one location to another
 **
 ** SYNOPSIS
 **   int copy_file(char *from, char *to)
 **
 ** DESCRIPTION
 **   The function copy_file() copies the file 'from' to the file
 **   'to'. The files are copied with the mmap() function when _NO_MMAP
 **   is NOT specified. Oterwise it will copy blockwise using te read()
 **   and write() functions.
 **
 ** RETURN VALUES
 **   SUCCESS when file 'from' was copied successful or else INCORRECT
 **   when it fails.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.01.1996 H.Kiehl Created
 **   03.07.2001 H.Kiehl When copying via mmap(), copy in chunks.
 **
 */
DESCR__E_M3

#include <stdio.h>      /* NULL                                          */
#include <stddef.h>
#include <unistd.h>     /* lseek(), write(), close()                     */
#include <string.h>     /* memcpy(), strerror()                          */
#include <sys/types.h>
#include <sys/stat.h>
#if defined (SIZE_WHEN_TO_MMAP_COPY) && !defined (_NO_MMAP)
#include <sys/mman.h>   /* mmap(), munmap()                              */
#endif
#include <stdlib.h>     /* malloc(), free()                              */
#include <fcntl.h>
#include <errno.h>

#ifndef MAP_FILE    /* Required for BSD          */
#define MAP_FILE 0  /* All others do not need it */
#endif


/*############################ copy_file() ##############################*/
int
copy_file(char *from, char *to)
{
   int         from_fd,
               to_fd;
   struct stat stat_buf;

   /* Open source file. */
   if ((from_fd = open(from, O_RDONLY)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open <%s> for copying : %s",
                 from, strerror(errno));
      return(INCORRECT);
   }

   /* Need size of input file. */
   if (fstat(from_fd, &stat_buf) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not fstat() on <%s> : %s", from, strerror(errno));
      (void)close(from_fd);
      return(INCORRECT);
   }

   if (stat_buf.st_size > 0)
   {
      size_t hunk;
#if defined (_NO_MMAP) || !defined (SIZE_WHEN_TO_MMAP_COPY)
      off_t  left;
      char   *buffer;

      /* Open destination file. */
      if ((to_fd = open(to, O_WRONLY | O_CREAT | O_TRUNC,
                        stat_buf.st_mode)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not open <%s> for copying : %s",
                    to, strerror(errno));
         (void)close(from_fd);
         return(INCORRECT);
      }

      left = stat_buf.st_size;
      if (left > stat_buf.st_blksize)
      {
         hunk = stat_buf.st_blksize;
      }
      else
      {
         hunk = left;
      }
      if ((buffer = (char *)malloc(hunk)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to allocate memory : %s", strerror(errno));
         (void)close(from_fd); (void)close(to_fd);
         return(INCORRECT);
      }

      while (left > 0)
      {
         /* Try read file in one hunk */
         if (read(from_fd, buffer, hunk) != hunk)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to read() <%s> : %s", from, strerror(errno));
            free(buffer);
            (void)close(from_fd); (void)close(to_fd);
            return(INCORRECT);
         }

         /* Try write file in one hunk */
         if (write(to_fd, buffer, hunk) != hunk)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to write() <%s> : %s", to, strerror(errno));
            free(buffer);
            (void)close(from_fd); (void)close(to_fd);
            return(INCORRECT);
         }
         left -= hunk;
         if (left < hunk)
         {
            hunk = left;
         }
      }
      free(buffer);
#else /* We have mmap() and should use it. */
      static int mmap_hunk_max = 0;

      if (mmap_hunk_max == 0)
      {
         int max_hunk;

         if ((max_hunk = sysconf(_SC_PAGESIZE)) < 1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "sysconf() error for _SC_PAGESIZE : %s",
                       strerror(errno));
            max_hunk = 8192;
         }
         mmap_hunk_max = max_hunk * 20;
      }

      if (stat_buf.st_size > SIZE_WHEN_TO_MMAP_COPY)
      {
         off_t bytes_done;
         char  *src, *dst;

         /* Open destination file. */
         if ((to_fd = open(to, O_RDWR | O_CREAT | O_TRUNC,
                           stat_buf.st_mode)) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not open <%s> for copying : %s",
                       to, strerror(errno));
            (void)close(from_fd);
            return(INCORRECT);
         }

         /* Set size of output file. */
         if (lseek(to_fd, stat_buf.st_size - 1, SEEK_SET) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not seek() on <%s> : %s", to, strerror(errno));
            (void)close(from_fd); (void)close(to_fd);
            return(INCORRECT);
         }
         if (write(to_fd, "", 1) != 1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not write() to <%s> : %s", to, strerror(errno));
            (void)close(from_fd); (void)close(to_fd);
            return(INCORRECT);
         }
         if (stat_buf.st_size > mmap_hunk_max)
         {
            hunk = mmap_hunk_max;
         }
         else
         {
            hunk = stat_buf.st_size;
         }
         bytes_done = 0;

         do
         {
            if ((src = mmap(0, hunk, PROT_READ, (MAP_FILE | MAP_SHARED),
                            from_fd, bytes_done)) == (caddr_t) -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not mmap() file <%s> : %s",
                          from, strerror(errno));
               (void)close(from_fd); (void)close(to_fd);
               return(INCORRECT);
            }
            if ((dst = mmap(0, hunk, (PROT_READ | PROT_WRITE),
                            (MAP_FILE | MAP_SHARED),
                            to_fd, bytes_done)) == (caddr_t) -1)
            {
               if (munmap(src, hunk) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "munmap() error : %s", strerror(errno));
               }
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not mmap() file <%s> : %s",
                          to, strerror(errno));
               (void)close(from_fd); (void)close(to_fd);
               return(INCORRECT);
            }

            /* Copy the chunk. */
            (void)memcpy(dst, src, hunk);

            /* Unmap areas. */
            if (munmap(src, hunk) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "munmap() error : %s", strerror(errno));
            }
            if (munmap(dst, hunk) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "munmap() error : %s", strerror(errno));
            }
            bytes_done += hunk;
            if ((stat_buf.st_size - bytes_done) < hunk)
            {
               hunk = stat_buf.st_size - bytes_done;
            }
         } while (bytes_done < stat_buf.st_size);
      }
      else /* Use normal read()/write() loop, its faster. */
      {
         off_t left;
         char  *buffer;

         /* Open destination file. */
         if ((to_fd = open(to, O_WRONLY | O_CREAT | O_TRUNC,
                           stat_buf.st_mode)) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not open <%s> for copying : %s",
                       to, strerror(errno));
            (void)close(from_fd);
            return(INCORRECT);
         }

         left = stat_buf.st_size;
         if (left > stat_buf.st_blksize)
         {
            hunk = stat_buf.st_blksize;
         }
         else
         {
            hunk = left;
         }
         if ((buffer = (char *)malloc(hunk)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to allocate memory : %s", strerror(errno));
            (void)close(from_fd); (void)close(to_fd);
            return(INCORRECT);
         }

         while (left > 0)
         {
            /* Try read file in one hunk. */
            if (read(from_fd, buffer, hunk) != hunk)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to read() <%s> : %s", from, strerror(errno));
               free(buffer);
               (void)close(from_fd); (void)close(to_fd);
               return(INCORRECT);
            }

            /* Try write file in one hunk. */
            if (write(to_fd, buffer, hunk) != hunk)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to write() <%s> : %s", to, strerror(errno));
               free(buffer);
               (void)close(from_fd); (void)close(to_fd);
               return(INCORRECT);
            }
            left -= hunk;
            if (left < hunk)
            {
               hunk = left;
            }
         }
         free(buffer);
      }
#endif
      if (close(to_fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to close() <%s> : %s", to, strerror(errno));
      }
   }

   if (close(from_fd) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to close() <%s> : %s", from, strerror(errno));
   }

   return(SUCCESS);
}


/*++++++++++++++++++++++++++++ normal_copy() ++++++++++++++++++++++++++++*/
int
normal_copy(char *from, char *to)
{
   int         from_fd,
               to_fd;
   struct stat stat_buf;

   /* Open source file. */
   if ((from_fd = open(from, O_RDONLY)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to open() <%s> : %s", from, strerror(errno));
      return(INCORRECT);
   }

   if (fstat(from_fd, &stat_buf) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to fstat() <%s> : %s", from, strerror(errno));
      (void)close(from_fd);
      return(INCORRECT);
   }

   /* Open destination file. */
   if ((to_fd = open(to, O_WRONLY | O_CREAT | O_TRUNC,
                     stat_buf.st_mode)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to open() <%s> : %s", to, strerror(errno));
      (void)close(from_fd);
      return(INCORRECT);
   }

   if (stat_buf.st_size > 0)
   {
      off_t  left;
      size_t hunk;
      char   *buffer;

      left = stat_buf.st_size;
      if (left > stat_buf.st_blksize)
      {
         hunk = stat_buf.st_blksize;
      }
      else
      {
         hunk = left;
      }
      if ((buffer = (char *)malloc(hunk)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to allocate memory : %s", strerror(errno));
         (void)close(from_fd); (void)close(to_fd);
         return(INCORRECT);
      }

      while (left > 0)
      {
         /* Try read file in one hunk */
         if (read(from_fd, buffer, hunk) != hunk)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to read() <%s> : %s", from, strerror(errno));
            free(buffer);
            (void)close(from_fd); (void)close(to_fd);
            return(INCORRECT);
         }

         /* Try write file in one hunk */
         if (write(to_fd, buffer, hunk) != hunk)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to write() <%s> : %s", to, strerror(errno));
            free(buffer);
            (void)close(from_fd); (void)close(to_fd);
            return(INCORRECT);
         }
         left -= hunk;
         if (left < hunk)
         {
            hunk = left;
         }
      }
      free(buffer);
   }
   if (close(from_fd) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to close() <%s> : %s", from, strerror(errno));
   }
   if (close(to_fd) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to close() <%s> : %s", to, strerror(errno));
   }

   return(SUCCESS);
}

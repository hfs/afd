/*
 *  copy_file.c - Part of AFD, an automatic file distribution program.
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
 **
 */
DESCR__E_M3

#include <stdio.h>      /* NULL                                          */
#include <stddef.h>
#include <unistd.h>     /* lseek(), write(), close()                     */
#include <string.h>     /* memcpy(), strerror()                          */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _NO_MMAP
#include <sys/mman.h>   /* mmap(), munmap()                              */
#endif
#include <stdlib.h>     /* malloc(), free()                              */
#include <fcntl.h>
#include <errno.h>

#ifndef MAP_FILE    /* Required for BSD          */
#define MAP_FILE 0  /* All others do not need it */
#endif

/* External global variables */
extern int sys_log_fd,
           amg_flag;


/*############################ copy_file() ##############################*/
int
copy_file(char *from, char *to)
{
   int         from_fd,
               to_fd;
   struct stat stat_buf;
#ifdef _NO_MMAP
#define HUNK_MAX 20480
   size_t      hunk,
               left;
   char        *buffer;

   /* Open source file */
   if ((from_fd = open(from, O_RDONLY)) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to open() %s : %s (%s %d)\n",
                from, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   if (fstat(from_fd, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to fstat() %s : %s (%s %d)\n",
                from, strerror(errno), __FILE__, __LINE__);
      (void)close(from_fd);
      return(INCORRECT);
   }

   left = hunk = stat_buf.st_size;

   if (hunk > HUNK_MAX)
   {
      hunk = HUNK_MAX;
   }

   if ((buffer = (char *)malloc(hunk)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to allocate memory : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      (void)close(from_fd);
      return(INCORRECT);
   }

   /* Open destination file */
   if ((to_fd = open(to, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to open() %s : %s (%s %d)\n",
                to, strerror(errno), __FILE__, __LINE__);
      free(buffer);
      (void)close(from_fd);
      return(INCORRECT);
   }

   while (left > 0)
   {
      /* Try read file in one hunk */
      if (read(from_fd, buffer, hunk) != hunk)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to read() %s : %s (%s %d)\n",
                   from, strerror(errno), __FILE__, __LINE__);
         free(buffer);
         (void)close(from_fd);
         (void)close(to_fd);
         return(INCORRECT);
      }

      /* Try write file in one hunk */
      if (write(to_fd, buffer, hunk) != hunk)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to write() %s : %s (%s %d)\n",
                   to, strerror(errno), __FILE__, __LINE__);
         free(buffer);
         (void)close(from_fd);
         (void)close(to_fd);
         return(INCORRECT);
      }
      left -= hunk;
      if (left < hunk)
      {
         hunk = left;
      }
   }
   free(buffer);
   if (close(from_fd) == -1)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Failed to close() %s : %s (%s %d)\n",
                from, strerror(errno), __FILE__, __LINE__);
   }
   if (close(to_fd) == -1)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Failed to close() %s : %s (%s %d)\n",
                to, strerror(errno), __FILE__, __LINE__);
   }
#else
   char        *src = NULL,
               *dst = NULL;

   if ((from_fd = open(from, O_RDONLY)) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not open %s for copying : %s (%s %d)\n",
                from, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   if (fstat(from_fd, &stat_buf) == -1)   /* need size of input file */
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not fstat() on %s : %s (%s %d)\n",
                from, strerror(errno), __FILE__, __LINE__);
      (void)close(from_fd);
      return(INCORRECT);
   }

   if ((to_fd = open(to, O_RDWR | O_CREAT | O_TRUNC, stat_buf.st_mode)) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not open %s for copying : %s (%s %d)\n",
                to, strerror(errno), __FILE__, __LINE__);
      (void)close(from_fd);
      return(INCORRECT);
   }

   if (stat_buf.st_size > 0)
   {
      /* set size of output file */
      if (lseek(to_fd, stat_buf.st_size - 1, SEEK_SET) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not seek() on %s : %s (%s %d)\n",
                   to, strerror(errno), __FILE__, __LINE__);
         (void)close(from_fd);
         (void)close(to_fd);
         return(INCORRECT);
      }
      if (write(to_fd, "", 1) != 1)
      {
         if ((errno == ENOSPC) && (amg_flag == YES))
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "DISK FULL!!! Will retry in %d second interval. (%s %d)\n",
                      DISK_FULL_RESCAN_TIME, __FILE__, __LINE__);

            while (errno == ENOSPC)
            {
               (void)sleep(DISK_FULL_RESCAN_TIME);
               errno = 0;
               if (write(to_fd, "", 1) != 1)
               {
                  if (errno != ENOSPC)
                  {
                     (void)rec(sys_log_fd, ERROR_SIGN,
                               "Could not write() to %s : %s (%s %d)\n",
                               to, strerror(errno), __FILE__, __LINE__);
                     (void)close(from_fd);
                     (void)close(to_fd);
                     return(INCORRECT);
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
                      "Could not write() to %s : %s (%s %d)\n",
                      to, strerror(errno), __FILE__, __LINE__);
            (void)close(from_fd);
            (void)close(to_fd);
            return(INCORRECT);
         }
      }

      if ((src = mmap(0, stat_buf.st_size, PROT_READ, (MAP_FILE | MAP_SHARED),
                      from_fd, 0)) == (caddr_t) -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not mmap() file %s : %s (%s %d)\n",
                   from, strerror(errno), __FILE__, __LINE__);
         (void)rec(sys_log_fd, INFO_SIGN,
                   "Will try an ordinary copy.\n");
         (void)close(from_fd);
         (void)close(to_fd);
         if (normal_copy(from, to) == SUCCESS)
         {
            return(SUCCESS);
         }
         else
         {
            return(INCORRECT);
         }
      }

      if ((dst = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                      (MAP_FILE | MAP_SHARED), to_fd, 0)) == (caddr_t) -1)
      {
         if (munmap(src, stat_buf.st_size) == -1)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "munmap() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not mmap() file %s : %s (%s %d)\n",
                   to, strerror(errno), __FILE__, __LINE__);
         (void)rec(sys_log_fd, INFO_SIGN,
                   "Will try an ordinary copy.\n");
         (void)close(from_fd);
         (void)close(to_fd);
         if (normal_copy(from, to) == SUCCESS)
         {
            return(SUCCESS);
         }
         else
         {
            return(INCORRECT);
         }
      }

      /* Copy the file */
      memcpy(dst, src, stat_buf.st_size);
   }

   if (close(from_fd) == -1)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Failed to close() %s : %s (%s %d)\n",
                from, strerror(errno), __FILE__, __LINE__);
   }
   if (close(to_fd) == -1)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Failed to close() %s : %s (%s %d)\n",
                to, strerror(errno), __FILE__, __LINE__);
   }

   /* Unmap areas!!! */
   if (stat_buf.st_size > 0)
   {
      if (munmap(src, stat_buf.st_size) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "munmap() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
      if (munmap(dst, stat_buf.st_size) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "munmap() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
   }
#endif

   return(SUCCESS);
}


/*++++++++++++++++++++++++++++ normal_copy() ++++++++++++++++++++++++++++*/
int
normal_copy(char *from, char *to)
{
   int         from_fd,
               to_fd;
   struct stat stat_buf;
#define HUNK_MAX 20480
   size_t      hunk,
               left;
   char        *buffer;

   /* Open source file */
   if ((from_fd = open(from, O_RDONLY)) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to open() %s : %s (%s %d)\n",
                from, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   if (fstat(from_fd, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to fstat() %s : %s (%s %d)\n",
                from, strerror(errno), __FILE__, __LINE__);
      (void)close(from_fd);
      return(INCORRECT);
   }

   left = hunk = stat_buf.st_size;

   if (hunk > HUNK_MAX)
   {
      hunk = HUNK_MAX;
   }

   if ((buffer = (char *)malloc(hunk)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to allocate memory : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      (void)close(from_fd);
      return(INCORRECT);
   }

   /* Open destination file */
   if ((to_fd = open(to, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to open() %s : %s (%s %d)\n",
                to, strerror(errno), __FILE__, __LINE__);
      free(buffer);
      (void)close(from_fd);
      return(INCORRECT);
   }

   while (left > 0)
   {
      /* Try read file in one hunk */
      if (read(from_fd, buffer, hunk) != hunk)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to read() %s : %s (%s %d)\n",
                   from, strerror(errno), __FILE__, __LINE__);
         free(buffer);
         (void)close(from_fd);
         (void)close(to_fd);
         return(INCORRECT);
      }

      /* Try write file in one hunk */
      if (write(to_fd, buffer, hunk) != hunk)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to write() %s : %s (%s %d)\n",
                   to, strerror(errno), __FILE__, __LINE__);
         free(buffer);
         (void)close(from_fd);
         (void)close(to_fd);
         return(INCORRECT);
      }
      left -= hunk;
      if (left < hunk)
      {
         hunk = left;
      }
   }
   free(buffer);
   if (close(from_fd) == -1)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Failed to close() %s : %s (%s %d)\n",
                from, strerror(errno), __FILE__, __LINE__);
   }
   if (close(to_fd) == -1)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Failed to close() %s : %s (%s %d)\n",
                to, strerror(errno), __FILE__, __LINE__);
   }

   return(SUCCESS);
}

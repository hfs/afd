/*
 *  convert.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   convert - converts a file from one format to another
 **
 ** SYNOPSIS
 **   int convert(char *file_path, char *file_name, int type, off_t *file_size)
 **
 ** DESCRIPTION
 **   The function converts the file from one format to another. The
 **   following conversions are currently implemented:
 **
 **     sohetx2wmo1 - converts a file that contains many ascii bulletins
 **                   starting with SOH and ending with ETX to the WMO
 **                   standart (8 byte ascii length and 2 bytes type
 **                   indicators). The SOH and ETX will NOT be copied
 **                   to the new file.
 **     sohetx2wmo0 - same as above only that here the SOH and ETX will
 **                   copied to the new file.
 **
 **   The original file will be overwritten with the new format.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to read any valid data from the
 **   file. On success SUCCESS will be returned and the size of the
 **   newly created file.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.09.2003 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                       /* snprintf(), rename()         */
#include <stdlib.h>                      /* strtoul()                    */
#include <string.h>                      /* strerror()                   */
#include <unistd.h>                      /* close(), unlink()            */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _NO_MMAP
#include <sys/mman.h>                    /* mmap(), munmap()             */
#endif
#include <fcntl.h>                       /* open()                       */
#include <errno.h>
#include "amgdefs.h"

#ifndef MAP_FILE    /* Required for BSD          */
#define MAP_FILE 0  /* All others do not need it */
#endif


/*############################### convert() #############################*/
int
convert(char *file_path, char *file_name, int type, off_t *file_size)
{
   int         from_fd,
               to_fd;
   off_t       new_length = 0;
   char        fullname[MAX_PATH_LENGTH],
               new_name[MAX_PATH_LENGTH],
               *src_ptr;
   struct stat stat_buf;

   (void)snprintf(fullname, MAX_PATH_LENGTH, "%s/%s", file_path, file_name);
   if ((from_fd = open(fullname, O_RDONLY)) < 0)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "Could not open %s for extracting : %s",
                  fullname, strerror(errno));
      return(INCORRECT);
   }

   if (fstat(from_fd, &stat_buf) < 0)   /* need size of input file */
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "fstat() error : %s", strerror(errno));
      (void)close(from_fd);
      return(INCORRECT);
   }

   /*
    * If the size of the file is less then 10 forget it. There can't
    * be a WMO bulletin in it.
    */
   if (stat_buf.st_size < 10)
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  "Got a file for extracting that is %ld bytes long!",
                  stat_buf.st_size);
      (void)close(from_fd);
      return(INCORRECT);
   }

#ifdef _NO_MMAP
   if ((src_ptr = mmap_emu(0, stat_buf.st_size, PROT_READ,
                           (MAP_FILE | MAP_SHARED),
                           fullname, 0)) == (caddr_t) -1)
#else
   if ((src_ptr = mmap(0, stat_buf.st_size, PROT_READ, (MAP_FILE | MAP_SHARED),
                       from_fd, 0)) == (caddr_t) -1)
#endif
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "mmap() error : %s", strerror(errno));
      (void)close(from_fd);
      return(INCORRECT);
   }

   (void)snprintf(new_name, MAX_PATH_LENGTH, "%s.tmpnewname", fullname);
   if ((to_fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
                     stat_buf.st_mode)) == -1)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "Failed to open() %s : %s", new_name, strerror(errno));
      (void)close(from_fd);
      return(INCORRECT);
   }

   switch (type)
   {
      case SOHETX2WMO0 :
      case SOHETX2WMO1 :
         {
            size_t length;
            char   length_indicator[10],
                   *ptr,
                   *ptr_start;

            ptr = src_ptr;
            do
            {
               while ((*ptr != 1) && ((ptr - src_ptr + 1) < stat_buf.st_size))
               {
                  ptr++;
               }
               if (*ptr == 1)
               {
                  if (type == SOHETX2WMO1)
                  {
                     ptr++; /* Away with SOH */
                  }
                  ptr_start = ptr;
                  while ((*ptr != 3) && ((ptr - src_ptr + 1) < stat_buf.st_size))
                  {
                     ptr++;
                  }
                  if (*ptr == 3)
                  {
                     if (type == SOHETX2WMO1)
                     {
                        length = ptr - ptr_start;
                        length_indicator[9] = '1';
                     }
                     else
                     {
                        length = ptr - ptr_start + 1;
                        length_indicator[9] = '0';
                     }
                     (void)sprintf(length_indicator, "%08lu",
                                   (unsigned long)length);
                     length_indicator[8] = '0';
                     if (write(to_fd, length_indicator, 10) != 10)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    "Failed to write() to %s : %s",
                                    new_name, strerror(errno));
                        (void)close(from_fd);
                        (void)close(to_fd);
                        return(INCORRECT);
                     }
                     if (write(to_fd, ptr_start, length) != length)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    "Failed to write() to %s : %s",
                                    new_name, strerror(errno));
                        (void)close(from_fd);
                        (void)close(to_fd);
                        return(INCORRECT);
                     }
                     new_length += (10 + length);
                  }
               }
            } while ((ptr - src_ptr + 1) < stat_buf.st_size);
         }
         break;

      default            : /* Impossible! */
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     "Unknown convert type (%d).", type);
#ifdef _NO_MMAP
         (void)munmap_emu((void *)src_ptr);
#else
         (void)munmap((void *)src_ptr, stat_buf.st_size);
#endif
         (void)close(from_fd);
         (void)close(to_fd);
         return(INCORRECT);
   }

#ifdef _NO_MMAP
   if (munmap_emu((void *)src_ptr) == -1)
#else
   if (munmap((void *)src_ptr, stat_buf.st_size) == -1)
#endif
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  "Failed to munmap() %s : %s", fullname, strerror(errno));
   }

   if ((close(from_fd) == -1) || (close(to_fd) == -1))
   {
      receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                  "close() error : %s", strerror(errno));
   }

   /* Remove the file that has just been extracted. */
   if (unlink(fullname) < 0)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "Failed to unlink() %s : %s", fullname, strerror(errno));
   }
   else
   {
      if (rename(new_name, fullname) == -1)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     "Failed to rename() %s to %s : %s",
                     new_name, fullname, strerror(errno));
         *file_size += new_length;
      }
      else
      {
         *file_size += (new_length - stat_buf.st_size);
      }
   }

   return(SUCCESS);
}

/*
 *  convert.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **     sohetx      - Adds <SOH><CR><CR><LF> to the beginning of the
 **                   file and <CR><CR><LF><ETX> to the end of the file.
 **     wmo         - Just add WMO 8 byte ascii length and 2 bytes type
 **                   indicator. If the message contains <SOH><CR><CR><LF>
 **                   at start and <CR><CR><LF><ETX> at the end, these
 **                   will be removed.
 **     sohetxwmo   - Adds WMO 8 byte ascii length and 2 bytes type
 **                   indicator plus <SOH><CR><CR><LF> at start and
 **                   <CR><CR><LF><ETX> to end if they are not there.
 **     sohetx2wmo1 - converts a file that contains many ascii bulletins
 **                   starting with SOH and ending with ETX to the WMO
 **                   standart (8 byte ascii length and 2 bytes type
 **                   indicators). The SOH and ETX will NOT be copied
 **                   to the new file.
 **     sohetx2wmo0 - same as above only that here the SOH and ETX will be
 **                   copied to the new file.
 **     mrz2wmo     - Converts GRIB, BUFR and BLOK files to files with
 **                   8 byte ascii length and 2 bytes type indicator plus
 **                   <SOH><CR><CR><LF> at start and <CR><CR><LF><ETX> to
 **                   the end, for the individual fields.
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
 **   10.05.2005 H.Kiehl Don't just check for SOH and ETX, check for
 **                      <SOH><CR><CR><LF> and <CR><CR><LF><ETX>.
 **   20.07.2006 H.Kiehl Added mrz2wmo.
 **
 */
DESCR__E_M3

#include <stdio.h>                       /* snprintf(), rename()         */
#include <stdlib.h>                      /* strtoul()                    */
#include <string.h>                      /* strerror()                   */
#include <unistd.h>                      /* close(), unlink()            */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>                    /* mmap(), munmap()             */
#endif
#include <fcntl.h>                       /* open()                       */
#include <errno.h>
#include "amgdefs.h"

#ifndef MAP_FILE     /* Required for BSD          */
# define MAP_FILE 0  /* All others do not need it */
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

#ifdef HAVE_SNPRINTF
   (void)snprintf(fullname, MAX_PATH_LENGTH, "%s/%s", file_path, file_name);
#else
   (void)sprintf(fullname, "%s/%s", file_path, file_name);
#endif
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

#ifdef HAVE_MMAP
   if ((src_ptr = mmap(0, stat_buf.st_size, PROT_READ, (MAP_FILE | MAP_SHARED),
                       from_fd, 0)) == (caddr_t) -1)
#else
   if ((src_ptr = mmap_emu(0, stat_buf.st_size, PROT_READ,
                           (MAP_FILE | MAP_SHARED),
                           fullname, 0)) == (caddr_t) -1)
#endif
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "mmap() error : %s", strerror(errno));
      (void)close(from_fd);
      return(INCORRECT);
   }

#ifdef HAVE_SNPRINTF
   (void)snprintf(new_name, MAX_PATH_LENGTH, "%s.tmpnewname", fullname);
#else
   (void)sprintf(new_name, "%s.tmpnewname", fullname);
#endif
   if ((to_fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
                     stat_buf.st_mode)) == -1)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "Failed to open() %s : %s", new_name, strerror(errno));
      (void)close(from_fd);
#ifdef HAVE_MMAP
      (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
      (void)munmap_emu((void *)src_ptr);
#endif
      return(INCORRECT);
   }

   switch (type)
   {
      case SOHETX :
         if ((*src_ptr != 1) && (*(src_ptr + stat_buf.st_size - 1) != 3))
         {
            char buffer[4];

            buffer[0] = 1;
            buffer[1] = 13;
            buffer[2] = 13;
            buffer[3] = 10;
            if (write(to_fd, buffer, 4) != 4)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           "Failed to write() to %s : %s",
                           new_name, strerror(errno));
               (void)close(from_fd);
               (void)close(to_fd);
#ifdef HAVE_MMAP
               (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
               (void)munmap_emu((void *)src_ptr);
#endif
               return(INCORRECT);
            }
            new_length += 4;

            if (write(to_fd, src_ptr, stat_buf.st_size) != stat_buf.st_size)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           "Failed to write() to %s : %s",
                           new_name, strerror(errno));
               (void)close(from_fd);
               (void)close(to_fd);
#ifdef HAVE_MMAP
               (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
               (void)munmap_emu((void *)src_ptr);
#endif
               return(INCORRECT);
            }
            new_length += stat_buf.st_size;

            buffer[0] = 13;
            buffer[2] = 10;
            buffer[3] = 3;
            if (write(to_fd, buffer, 4) != 4)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           "Failed to write() to %s : %s",
                           new_name, strerror(errno));
               (void)close(from_fd);
               (void)close(to_fd);
#ifdef HAVE_MMAP
               (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
               (void)munmap_emu((void *)src_ptr);
#endif
               return(INCORRECT);
            }
            new_length += 4;
         }
         break;

      case ONLY_WMO :
         {
            int   offset;
            off_t size;
            char  length_indicator[10];

            if ((*src_ptr == 1) &&
                (*(src_ptr + 1) == 13) &&
                (*(src_ptr + 2) == 13) &&
                (*(src_ptr + 3) == 10) &&
                (*(src_ptr + stat_buf.st_size - 4) == 13) &&
                (*(src_ptr + stat_buf.st_size - 3) == 13) &&
                (*(src_ptr + stat_buf.st_size - 2) == 10) &&
                (*(src_ptr + stat_buf.st_size - 1) == 3))
            {
               offset = 4;
               size = stat_buf.st_size - 8;
            }
            else
            {
               size = stat_buf.st_size;
               offset = 0;
            }
            (void)sprintf(length_indicator, "%08lu", (unsigned long)size);
            length_indicator[8] = '0';
            length_indicator[9] = '1';
            if (write(to_fd, length_indicator, 10) != 10)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           "Failed to write() to %s : %s",
                           new_name, strerror(errno));
               (void)close(from_fd);
               (void)close(to_fd);
#ifdef HAVE_MMAP
               (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
               (void)munmap_emu((void *)src_ptr);
#endif
               return(INCORRECT);
            }
            new_length += 10;

            if (write(to_fd, (src_ptr + offset), size) != size)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           "Failed to write() to %s : %s",
                           new_name, strerror(errno));
               (void)close(from_fd);
               (void)close(to_fd);
#ifdef HAVE_MMAP
               (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
               (void)munmap_emu((void *)src_ptr);
#endif
               return(INCORRECT);
            }
            new_length += size;
         }
         break;

      case SOHETXWMO :
         {
            size_t length;
            char   length_indicator[14];

            if ((*src_ptr != 1) && (*(src_ptr + stat_buf.st_size - 1) != 3))
            {
               (void)sprintf(length_indicator, "%08lu",
                             (unsigned long)stat_buf.st_size + 8);
               length_indicator[10] = 1;
               length_indicator[11] = 13;
               length_indicator[12] = 13;
               length_indicator[13] = 10;
               length = 14;
            }
            else
            {
               (void)sprintf(length_indicator, "%08lu",
                             (unsigned long)stat_buf.st_size);
               length = 10;
            }
            length_indicator[8] = '0';
            length_indicator[9] = '0';
            if (write(to_fd, length_indicator, length) != length)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           "Failed to write() to %s : %s",
                           new_name, strerror(errno));
               (void)close(from_fd);
               (void)close(to_fd);
#ifdef HAVE_MMAP
               (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
               (void)munmap_emu((void *)src_ptr);
#endif
               return(INCORRECT);
            }
            new_length += length;

            if (write(to_fd, src_ptr, stat_buf.st_size) != stat_buf.st_size)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           "Failed to write() to %s : %s",
                           new_name, strerror(errno));
               (void)close(from_fd);
               (void)close(to_fd);
#ifdef HAVE_MMAP
               (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
               (void)munmap_emu((void *)src_ptr);
#endif
               return(INCORRECT);
            }
            new_length += stat_buf.st_size;

            if ((*src_ptr != 1) && (*(src_ptr + stat_buf.st_size - 1) != 3))
            {
               length_indicator[10] = 13;
               length_indicator[11] = 13;
               length_indicator[12] = 10;
               length_indicator[13] = 3;
               if (write(to_fd, &length_indicator[10], 4) != 4)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              "Failed to write() to %s : %s",
                              new_name, strerror(errno));
                  (void)close(from_fd);
                  (void)close(to_fd);
#ifdef HAVE_MMAP
                  (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
                  (void)munmap_emu((void *)src_ptr);
#endif
                  return(INCORRECT);
               }
               new_length += 4;
            }
         }
         break;

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
               do
               {
                  while ((*ptr != 1) &&
                         ((ptr - src_ptr + 1) < stat_buf.st_size))
                  {
                     ptr++;
                  }
                  if (*ptr == 1)
                  {
                     if (((ptr - src_ptr + 3) < stat_buf.st_size) &&
                         (*(ptr + 1) == 13) && (*(ptr + 2) == 13) &&
                         (*(ptr + 3) == 10))
                     {
                        break;
                     }
                     else
                     {
                        ptr++;
                     }
                  }
               } while ((ptr - src_ptr + 1) < stat_buf.st_size);

               if (*ptr == 1)
               {
                  if (type == SOHETX2WMO1)
                  {
                     ptr++; /* Away with SOH */
                  }
                  ptr_start = ptr;
                  do
                  {
                     while ((*ptr != 3) &&
                            ((ptr - src_ptr + 1) < stat_buf.st_size))
                     {
                        ptr++;
                     }
                     if (*ptr == 3)
                     {
                        if (((ptr - ptr_start) > 2) &&
                            (*(ptr - 1) == 10) && (*(ptr - 2) == 13) &&
                            (*(ptr - 3) == 13))
                        {
                           break;
                        }
                        else
                        {
                           ptr++;
                        }
                     }
                  } while ((ptr - src_ptr + 1) < stat_buf.st_size);

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

      case MRZ2WMO :
         if (bin_file_convert(src_ptr, stat_buf.st_size, to_fd) < 0)
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        "Failed to convert MRZ file %s to WMO-format.",
                        file_name);
         }
         break;

      default            : /* Impossible! */
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     "Unknown convert type (%d).", type);
#ifdef HAVE_MMAP
         (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
         (void)munmap_emu((void *)src_ptr);
#endif
         (void)close(from_fd);
         (void)close(to_fd);
         return(INCORRECT);
   }

#ifdef HAVE_MMAP
   if (munmap((void *)src_ptr, stat_buf.st_size) == -1)
#else
   if (munmap_emu((void *)src_ptr) == -1)
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

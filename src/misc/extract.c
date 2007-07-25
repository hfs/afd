/*
 *  extract.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2007 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   extract - breaks up a file containing bulletins to one file per
 **             bulletin
 **
 ** SYNOPSIS
 **   int extract(char           *file_name,
 **               char           *dest_dir,
 **               char           *p_filter,
 **               time_t         creation_time,
 **               unsigned short unique_number,
 **               unsigned int   split_job_counter,
 **               char           *extract_id,
 **               int            type,
 **               int            options,
 **               int            *files_to_send,
 **               off_t          *file_size)
 **
 ** DESCRIPTION
 **   The function extract reads a WMO bulletin file, and writes each
 **   bulletin into a separate file. The bulletin must have the following
 **   format:
 ** 
 **       <length indicator><SOH><CR><CR><LF>nnn<CR><CR><LF>
 **       WMO header<CR><CR><LF>WMO message<CR><CR><LF><ETX>
 **
 **   Four length indicators with the following length are currently
 **   recognised by extract():
 **
 **                   2 Byte - Vax standard
 **                   4 Byte - Low byte first
 **                   4 Byte - High byte first
 **                   4 Byte - MSS standard
 **                   8 Byte - WMO standard (plus 2 Bytes type indicator)
 **
 **   It is also possible without a length indicator, it must then
 **   have the following format:
 **
 **        <SOH><CR><CR><LF>nnn<CR><CR><LF>
 **        WMO header<CR><CR><LF>WMO message<CR><CR><LF><ETX>
 **
 **   The file name of the new file will be the WMO header: 
 **
 **       TTAAii_CCCC_YYGGgg[_BBB]
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to read any valid data from the
 **   file. On success SUCCESS will be returned and the number of files
 **   that have been created and the sum of their size.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.09.1997 H.Kiehl Created
 **   05.04.1999 H.Kiehl Added WMO standard.
 **   03.10.2004 H.Kiehl Added production log.
 **   16.08.2005 H.Kiehl Added ASCII option.
 **   13.07.2007 H.Kiehl Added option to filter out only those bulletins
 **                      we really nead.
 **
 */
DESCR__E_M3

#include <stdio.h>                       /* sprintf()                    */
#include <stdlib.h>                      /* strtoul()                    */
#include <string.h>                      /* strerror()                   */
#include <unistd.h>                      /* close()                      */
#include <ctype.h>                       /* isdigit()                    */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>                    /* mmap(), munmap()             */
#endif
#include <fcntl.h>                       /* open()                       */
#include <errno.h>
#include "amgdefs.h"

#ifndef MAP_FILE    /* Required for BSD          */
#define MAP_FILE 0  /* All others do not need it */
#endif

/* Local global variables */
static int            counter_fd,
                      extract_options;
static mode_t         file_mode;
#ifdef _PRODUCTION_LOG
static char           *p_extract_id;
static time_t         tmp_creation_time;
static unsigned short tmp_unique_number;
static unsigned int   tmp_split_job_counter;
#endif
static char           *extract_p_filter,
                      *p_full_file_name,
                      *p_file_name,
                      *p_orig_name;

/* Local function prototypes */
static void           ascii_sohetx(char *, off_t, char *),
                      two_byte_vax(char *, off_t),
                      two_byte_vax_swap(char *, off_t),
                      four_byte(char *, off_t),
                      four_byte_swap(char *, off_t),
                      four_byte_mss(char *, off_t),
                      four_byte_mss_swap(char *, off_t),
                      wmo_standard(char *, off_t);
static int            write_file(char *, unsigned int, int),
                      *p_files_to_send;
static off_t          *p_file_size;

/* Local definitions */
#define MAX_WMO_HEADER_LENGTH 25


/*############################### extract() #############################*/
int
extract(char           *file_name,
        char           *dest_dir,
        char           *p_filter,
#ifdef _PRODUCTION_LOG
        time_t         creation_time,
        unsigned short unique_number,
        unsigned int   split_job_counter,
        char           *extract_id,
#endif
        int            type,
        int            options,
        int            *files_to_send,
        off_t          *file_size)
{
   int         byte_order = 1,
               from_fd;
   char        *src_ptr,
               fullname[MAX_PATH_LENGTH];
   struct stat stat_buf;

   (void)sprintf(fullname, "%s/%s", dest_dir, file_name);
   if ((from_fd = open(fullname, O_RDONLY)) == -1)
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

   /*
    * When creating new files take the permissions from the original
    * files. This gives the originator the possibility to set the
    * permissions on the destination site.
    */
   file_mode = stat_buf.st_mode;

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

   if (options & EXTRACT_ADD_UNIQUE_NUMBER)
   {
      /* Open counter file. */
      if ((counter_fd = open_counter_file(COUNTER_FILE)) == -1)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     "Failed to open counter file!");
         (void)close(from_fd);
         return(INCORRECT);
      }
   }
   extract_options = options;
   extract_p_filter = p_filter;

   /*
    * Prepare file_name pointer for function write_file(), so we
    * only one simple copy to have the full file name for the
    * new file. Same goes for the file_size and files_to_send.
    * Just to lazy to handle these values right down to write_file().
    */
   p_full_file_name = dest_dir;
   p_file_name = dest_dir + strlen(dest_dir);
   *(p_file_name++) = '/';
   *p_file_name = '\0';
   p_file_size = file_size;
   p_files_to_send = files_to_send;
#ifdef _PRODUCTION_LOG
   tmp_creation_time = creation_time;
   tmp_unique_number = unique_number;
   tmp_split_job_counter = split_job_counter;
   p_extract_id = extract_id;
#endif
   p_orig_name = file_name;

   switch (type)
   {
      case ASCII_STANDARD: /* No length indicator, just locate SOH + ETX */
         ascii_sohetx(src_ptr, stat_buf.st_size, file_name);
         break;

      case TWO_BYTE      : /* Vax Standard */
         if (*(char *)&byte_order == 1)
         {
            /* little-endian */
            two_byte_vax(src_ptr, stat_buf.st_size);
         }
         else
         {
            /* big-endian */
            two_byte_vax_swap(src_ptr, stat_buf.st_size);
         }
         break;

      case FOUR_BYTE_LBF : /* Low byte first */
         if (*(char *)&byte_order == 1)
         {
            /* little-endian */
            four_byte(src_ptr, stat_buf.st_size);
         }
         else
         {
            /* big-endian */
            four_byte_swap(src_ptr, stat_buf.st_size);
         }
         break;

      case FOUR_BYTE_HBF : /* High byte first */
         if (*(char *)&byte_order == 1)
         {
            /* little-endian */
            four_byte_swap(src_ptr, stat_buf.st_size);
         }
         else
         {
            /* big-endian */
            four_byte(src_ptr, stat_buf.st_size);
         }
         break;

      case FOUR_BYTE_MSS : /* MSS Standard */
         if (*(char *)&byte_order == 1)
         {
            /* little-endian */
            four_byte_mss_swap(src_ptr, stat_buf.st_size);
         }
         else
         {
            /* big-endian */
            four_byte_mss(src_ptr, stat_buf.st_size);
         }
         break;

      case WMO_STANDARD  : /* WMO Standard */
         wmo_standard(src_ptr, stat_buf.st_size);
         break;

      default            : /* Impossible! */
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     "Unknown length type (%d) for extracting bulletins.",
                     type);
#ifdef HAVE_MMAP
         (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
         (void)munmap_emu((void *)src_ptr);
#endif
         (void)close(from_fd);
         if (options & EXTRACT_ADD_UNIQUE_NUMBER)
         {
            (void)close(counter_fd);
         }
         *(p_file_name - 1) = '\0';
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

   if (options & EXTRACT_ADD_UNIQUE_NUMBER)
   {
      if (close(counter_fd) == -1)
      {
         receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                     "close() error : %s", strerror(errno));
      }
   }
   if (close(from_fd) == -1)
   {
      receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                  "close() error : %s", strerror(errno));
   }

   /* Remove the file that has just been extracted. */
   if (unlink(fullname) < 0)
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  "Failed to unlink() %s : %s", fullname, strerror(errno));
   }
   else
   {
      *file_size -= stat_buf.st_size;
      (*files_to_send)--;
   }
   *(p_file_name - 1) = '\0';

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++ ascii_sohetx() ++++++++++++++++++++++++++*/
static void
ascii_sohetx(char *src_ptr, off_t total_length, char *file_name)
{
   char *ptr = src_ptr,
        *ptr_start;

   do
   {
      while ((*ptr != 1) && ((ptr - src_ptr) <= total_length))
      {
         ptr++;
      }
      if (*ptr == 1)
      {
         ptr_start = ptr;
         while ((*ptr != 3) && ((ptr - src_ptr) <= total_length))
         {
            ptr++;
         }
         if (*ptr == 3)
         {
            ptr++;
            if (write_file(ptr_start, (unsigned int)(ptr - ptr_start), YES) < 0)
            {
               return;
            }
            if ((ptr - src_ptr) >= total_length)
            {
               return;
            }
         }
         else
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        "Failed to locate terminating ETX in %s.", file_name);
            return;
         }
      }
   } while ((ptr - src_ptr) <= total_length);

   return;
}


/*+++++++++++++++++++++++++++++ two_byte_vax() ++++++++++++++++++++++++++*/
static void
two_byte_vax(char *src_ptr, off_t total_length)
{
   unsigned short length;
   char           *ptr = src_ptr;

   ((char *)&length)[0] = *ptr;
   ((char *)&length)[1] = *(ptr + 1);
   if (((ptr + length) - src_ptr) <= total_length)
   {
      for (;;)
      {
         if (write_file(ptr + 2, (unsigned int)length, YES) < 0)
         {
            return;
         }
         ptr += length + 3;
         if ((ptr - src_ptr) >= total_length)
         {
            return;
         }
         ((char *)&length)[0] = *ptr;
         ((char *)&length)[1] = *(ptr + 1);
      }
   }

   return;
}


/*++++++++++++++++++++++++++ two_byte_vax_swap() ++++++++++++++++++++++++*/
static void
two_byte_vax_swap(char *src_ptr, off_t total_length)
{
   unsigned short length;
   char           *ptr = src_ptr;

   ((char *)&length)[0] = *(ptr + 1);
   ((char *)&length)[1] = *ptr;
   if (((ptr + length) - src_ptr) <= total_length)
   {
      for (;;)
      {
         if (write_file(ptr + 2, (unsigned int)length, YES) < 0)
         {
            return;
         }
         ptr += length + 3;
         if ((ptr - src_ptr) >= total_length)
         {
            return;
         }
         ((char *)&length)[0] = *(ptr + 1);
         ((char *)&length)[1] = *ptr;
      }
   }

   return;
}


/*++++++++++++++++++++++++++++++ four_byte() ++++++++++++++++++++++++++++*/
static void
four_byte(char *src_ptr, off_t total_length)
{
   unsigned int length;
   char         *ptr = src_ptr;

   ((char *)&length)[0] = *ptr;
   ((char *)&length)[1] = *(ptr + 1);
   ((char *)&length)[2] = *(ptr + 2);
   ((char *)&length)[3] = *(ptr + 3);
   if (((ptr + length) - src_ptr) <= total_length)
   {
      for (;;)
      {
         if (write_file(ptr + 4, length, YES) < 0)
         {
            return;
         }
         ptr += length + 4;
         if ((ptr - src_ptr) >= total_length)
         {
            return;
         }
         ((char *)&length)[0] = *ptr;
         ((char *)&length)[1] = *(ptr + 1);
         ((char *)&length)[2] = *(ptr + 2);
         ((char *)&length)[3] = *(ptr + 3);
      }
   }

   return;
}


/*+++++++++++++++++++++++++++ four_byte_swap() ++++++++++++++++++++++++++*/
static void
four_byte_swap(char *src_ptr, off_t total_length)
{
   unsigned int length;
   char         *ptr = src_ptr;

   ((char *)&length)[0] = *(ptr + 3);
   ((char *)&length)[1] = *(ptr + 2);
   ((char *)&length)[2] = *(ptr + 1);
   ((char *)&length)[3] = *ptr;
   if (((ptr + length) - src_ptr) <= total_length)
   {
      for (;;)
      {
         if (write_file(ptr + 4, length, YES) < 0)
         {
            return;
         }
         ptr += length + 4;
         if ((ptr - src_ptr) >= total_length)
         {
            return;
         }
         ((char *)&length)[0] = *(ptr + 3);
         ((char *)&length)[1] = *(ptr + 2);
         ((char *)&length)[2] = *(ptr + 1);
         ((char *)&length)[3] = *ptr;
      }
   }

   return;
}


/*++++++++++++++++++++++++++++ four_byte_mss() ++++++++++++++++++++++++++*/
static void
four_byte_mss(char *src_ptr, off_t total_length)
{
   unsigned int length;
   char         *ptr = src_ptr;

   ((char *)&length)[0] = 0;
   ((char *)&length)[1] = *(ptr + 1);
   ((char *)&length)[2] = *(ptr + 2);
   ((char *)&length)[3] = *(ptr + 3);
   if (((ptr + length) - src_ptr) <= total_length)
   {
      for (;;)
      {
         if (write_file(ptr + 4, length, YES) < 0)
         {
            return;
         }
         ptr += length + 4;
         if ((ptr - src_ptr) >= total_length)
         {
            return;
         }
         ((char *)&length)[0] = 0;
         ((char *)&length)[1] = *(ptr + 1);
         ((char *)&length)[2] = *(ptr + 2);
         ((char *)&length)[3] = *(ptr + 3);
      }
   }
   return;
}


/*+++++++++++++++++++++++++ four_byte_mss_swap() ++++++++++++++++++++++++*/
static void
four_byte_mss_swap(char *src_ptr, off_t total_length)
{
   unsigned int length;
   char         *ptr = src_ptr;

   ((char *)&length)[0] = *(ptr + 3);
   ((char *)&length)[1] = *(ptr + 2);
   ((char *)&length)[2] = *(ptr + 1);
   ((char *)&length)[3] = 0;
   if (((ptr + length) - src_ptr) <= total_length)
   {
      for (;;)
      {
         if (write_file(ptr + 4, length, YES) < 0)
         {
            return;
         }
         ptr += length + 4;
         if ((ptr - src_ptr) >= total_length)
         {
            return;
         }
         ((char *)&length)[0] = *(ptr + 3);
         ((char *)&length)[1] = *(ptr + 2);
         ((char *)&length)[2] = *(ptr + 1);
         ((char *)&length)[3] = 0;
      }
   }
   return;
}


/*+++++++++++++++++++++++++++ wmo_standard() ++++++++++++++++++++++++++++*/
static void
wmo_standard(char *src_ptr, off_t total_length)
{
   int          soh_etx;
   unsigned int length;
   char         *ptr = src_ptr,
                str_length[9];

   str_length[8] = '\0';
   do
   {
      str_length[0] = *ptr;
      str_length[1] = *(ptr + 1);
      str_length[2] = *(ptr + 2);
      str_length[3] = *(ptr + 3);
      str_length[4] = *(ptr + 4);
      str_length[5] = *(ptr + 5);
      str_length[6] = *(ptr + 6);
      str_length[7] = *(ptr + 7);
      length = (unsigned int)strtoul(str_length, NULL, 10);
      if (length > 0)
      {
         if (*(ptr + 9) == '1')
         {
            soh_etx = NO;
         }
         else
         {
            soh_etx = YES;
         }
         if (write_file(ptr + 10, length, soh_etx) < 0)
         {
            return;
         }
      }
      ptr += length + 10;
   } while ((ptr - src_ptr) < total_length);

   return;
}


/*------------------------------ write_file() ---------------------------*/
static int
write_file(char *msg, unsigned int length, int soh_etx)
{
   int  i,
        fd;
   char *ptr,
        *p_start;

   /*
    * Build the file name from the bulletin header.
    */
   if ((soh_etx == YES) && (msg[0] != 1)) /* Must start with SOH */
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  "Failed to read bulletin header. No SOH at start.");
      return(INCORRECT);
   }

   /*
    * Position to start of header ie. after <SOH><CR><CR><LF>nnn<CR><CR><LF>
    * and then store the heading in heading_str. The end of heading is when
    * we hit an unprintable character, in most cases this should be the
    * <CR><CR><LF> after the heading.
    */
   ptr = msg;
   while ((*ptr < 32) && ((ptr - msg) < length))
   {
      ptr++;
   }
   if ((ptr + 3 - msg) >= length)
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  "Failed to read bulletin header. No header found.");
      return(INCORRECT);
   }
   if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
       (isdigit((int)(*(ptr + 2)))) &&
       ((*(ptr + 3) == 13) || (*(ptr + 3) == 10)))
   {
      ptr += 3;
   }
   while (((*ptr == 13) || (*ptr == 10)) && ((ptr - msg) < length))
   {
      ptr++;
   }
   p_start = ptr;
   i = 0;
   while ((*ptr > 31) && (i < MAX_WMO_HEADER_LENGTH) && ((ptr - msg) < length))
   {
      if ((*ptr == ' ') || (*ptr == '/') || (*ptr < ' ') || (*ptr > 'z'))
      {
         p_file_name[i] = '_';
      }
      else
      {
         p_file_name[i] = *ptr;
      }
      ptr++; i++;
   }
   if (i == 0)
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  "Length of WMO header is 0!? Discarding file.");
      return(INCORRECT);
   }
   if (extract_p_filter != NULL)
   {
      if (pmatch(extract_p_filter, p_file_name, NULL) != 0)
      {
         /* Ignore this bulletin. */
         return(SUCCESS);
      }
   }
   if (extract_options & EXTRACT_ADD_CRC_CHECKSUM)
   {
      i += sprintf(&p_file_name[i], "-%x", get_checksum(msg, length));
   }
   if (extract_options & EXTRACT_ADD_UNIQUE_NUMBER)
   {
      int unique_number;

      (void)next_counter(counter_fd, &unique_number);
      i += sprintf(&p_file_name[i], "-%04d", unique_number);
   }
   p_file_name[i] = '\0';

   if ((fd = open(p_full_file_name, (O_RDWR | O_CREAT | O_TRUNC),
                  file_mode)) < 0)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "Failed to open() %s while extracting bulletins : %s",
                  p_full_file_name, strerror(errno));
      *p_file_name = '\0';
      return(INCORRECT);
   }

   if (extract_options & EXTRACT_ADD_SOH_ETX)
   {
      if (soh_etx == NO)
      {
         if (write(fd, "\001", 1) != 1)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        "Failed to write() SOH : %s", strerror(errno));
            (void)close(fd);
            return(INCORRECT);
         }
      }
      if (write(fd, msg, (size_t)length) != (ssize_t)length)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     "Failed to write() message : %s", strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }
      if (soh_etx == NO)
      {
         if (write(fd, "\003", 1) != 1)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        "Failed to write() ETX : %s", strerror(errno));
            (void)close(fd);
            return(INCORRECT);
            }
         *p_file_size += length + 2;
      }
      else
      {
         *p_file_size += length;
      }
   }
   else
   {
      if (msg[length - 1] == 3)
      {
         length--;
         while ((length > 0) &&
                ((msg[length - 1] == '\015') || (msg[length - 1] == '\012')))
         {
            length--;
         }
      }
      length -= (p_start - msg);
      if (write(fd, p_start, (size_t)length) != (ssize_t)length)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     "Failed to write() message : %s", strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }
      *p_file_size += length;
   }
   (*p_files_to_send)++;

   if (close(fd) == -1)
   {
      receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                  "close() error : %s", strerror(errno));
   }
#ifdef _PRODUCTION_LOG
   production_log(tmp_creation_time, tmp_unique_number,
                  tmp_split_job_counter, "%s|%s|extract(%s)",
                  p_orig_name, p_file_name, p_extract_id);
#endif
   *p_file_name = '\0';

   return(SUCCESS);
}

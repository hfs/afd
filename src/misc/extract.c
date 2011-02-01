/*
 *  extract.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2010 Deutscher Wetterdienst (DWD),
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
 **   int extract(char         *file_name,
 **               char         *dest_dir,
 **               char         *p_filter,
 **               time_t       creation_time,
 **               unsigned int unique_number,
 **               unsigned int split_job_counter,
 **               unsigned int job_id,
 **               unsigned int dir_id,
 **               char         *full_option,
 **               int          type,
 **               int          options,
 **               int          *files_to_send,
 **               off_t        *file_size)
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
 **   Or a ZCZC at the beginning and an NNNN at the end as shown in
 **   the following format:
 **
 **        <CR><CR><LF>ZCZC<WMO message>NNNN
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
 **   08.04.2008 H.Kiehl Added ZCZC_NNNN option.
 **   07.12.2008 H.Kiehl Added option to extract reports.
 **
 */
DESCR__E_M3

#include <stdio.h>                       /* sprintf()                    */
#include <stdlib.h>                      /* strtoul(), malloc(), free()  */
#include <string.h>                      /* strerror()                   */
#include <unistd.h>                      /* close()                      */
#include <ctype.h>                       /* isdigit(), isupper()         */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                   /* mmap(), munmap()             */
#endif
#include <fcntl.h>                       /* open()                       */
#include <errno.h>
#include "amgdefs.h"

#ifndef MAP_FILE    /* Required for BSD          */
#define MAP_FILE 0  /* All others do not need it */
#endif

#ifdef _PRODUCTION_LOG
#define LOG_ENTRY_STEP_SIZE 25
struct prod_log_db
       {
          char  file_name[MAX_FILENAME_LENGTH + 1];
          off_t size;
       };
#endif

/* Local global variables. */
static int                *counter,
                          counter_fd,
                          extract_options,
                          *p_files_to_send;
static mode_t             file_mode;
static off_t              *p_file_size;
static char               *extract_p_filter,
                          *p_full_file_name,
                          *p_file_name,
                          *p_orig_name;
#ifdef _PRODUCTION_LOG
static int                no_of_log_entries = 0;
static struct prod_log_db *pld = NULL;
#endif

/* Local function prototypes. */
static void               ascii_sohetx(char *, off_t, char *),
                          ascii_zczc_nnnn(char *, off_t, char *),
                          binary_sohetx(char *, off_t, char *),
                          four_byte(char *, off_t),
                          four_byte_swap(char *, off_t),
                          four_byte_mss(char *, off_t),
                          four_byte_mss_swap(char *, off_t),
                          show_unknown_report(char *, int, char *, char *, int),
                          two_byte_vax(char *, off_t),
                          two_byte_vax_swap(char *, off_t),
                          wmo_standard(char *, off_t);
static int                check_report(char *, unsigned int, int *),
                          write_file(char *, unsigned int, int);

/* Local definitions. */
#define MAX_WMO_HEADER_LENGTH  25
#define MAX_REPORT_LINE_LENGTH 80


/*############################### extract() #############################*/
int
extract(char         *file_name,
        char         *dest_dir,
        char         *p_filter,
#ifdef _PRODUCTION_LOG
        time_t       creation_time,
        unsigned int unique_number,
        unsigned int split_job_counter,
        unsigned int job_id,
        unsigned int dir_id,
        char         *full_option,
#endif
        int          type,
        int          options,
        int          *files_to_send,
        off_t        *file_size)
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
                  _("Could not openi() `%s' for extracting : %s"),
                  fullname, strerror(errno));
      return(INCORRECT);
   }

   if (fstat(from_fd, &stat_buf) < 0)   /* need size of input file */
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("fstat() error : %s"), strerror(errno));
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
                  _("Got a file for extracting that is %ld bytes long!"),
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
   if ((src_ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                       (MAP_FILE | MAP_SHARED),
                       from_fd, 0)) == (caddr_t) -1)
#else
   if ((src_ptr = mmap_emu(NULL, stat_buf.st_size, PROT_READ,
                           (MAP_FILE | MAP_SHARED),
                           fullname, 0)) == (caddr_t) -1)
#endif
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("mmap() error : %s"), strerror(errno));
      (void)close(from_fd);
      return(INCORRECT);
   }

   if (options & EXTRACT_ADD_UNIQUE_NUMBER)
   {
      /* Open counter file. */
      if ((counter_fd = open_counter_file(COUNTER_FILE, &counter)) == -1)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to open counter file!"));
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
   p_orig_name = file_name;

   switch (type)
   {
      case ASCII_STANDARD: /* No length indicator, just locate SOH + ETX. */
         ascii_sohetx(src_ptr, stat_buf.st_size, file_name);
         break;

      case BINARY_STANDARD: /* No length indicator, just cut away header. */
         binary_sohetx(src_ptr, stat_buf.st_size, file_name);
         break;

      case ZCZC_NNNN: /* No length indicator, just locate ZCZC + NNNN. */
         ascii_zczc_nnnn(src_ptr, stat_buf.st_size, file_name);
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
                     _("Unknown length type (%d) for extracting bulletins."),
                     type);
#ifdef HAVE_MMAP
         (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
         (void)munmap_emu((void *)src_ptr);
#endif
         (void)close(from_fd);
         if (options & EXTRACT_ADD_UNIQUE_NUMBER)
         {
            close_counter_file(counter_fd, &counter);
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
                  _("Failed to munmap() `%s' : %s"), fullname, strerror(errno));
   }

   if (options & EXTRACT_ADD_UNIQUE_NUMBER)
   {
      close_counter_file(counter_fd, &counter);
   }
   if (close(from_fd) == -1)
   {
      receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                  _("close() error : %s"), strerror(errno));
   }

   /* Remove the file that has just been extracted. */
   if (unlink(fullname) < 0)
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to unlink() `%s' : %s"), fullname, strerror(errno));
   }
   else
   {
      *file_size -= stat_buf.st_size;
      (*files_to_send)--;
   }
   *(p_file_name - 1) = '\0';

#ifdef _PRODUCTION_LOG
   if ((pld != NULL) && (no_of_log_entries))
   {
      for (byte_order = 0; byte_order < no_of_log_entries; byte_order++)
      {
         production_log(creation_time, 1, no_of_log_entries, unique_number,
                        split_job_counter, job_id, dir_id,
# if SIZEOF_OFF_T == 4
                        "%s%c%s%c%lx%c0%c%s",
# else
                        "%s%c%s%c%llx%c0%c%s",
# endif
                        p_orig_name, SEPARATOR_CHAR, pld[byte_order].file_name,
                        SEPARATOR_CHAR, (pri_off_t)pld[byte_order].size,
                        SEPARATOR_CHAR, SEPARATOR_CHAR, full_option);
      }
      free(pld);
      no_of_log_entries = 0;
      pld = NULL;
   }
#endif

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
                        _("Failed to locate terminating ETX in %s."),
                        file_name);
            return;
         }
      }
   } while ((ptr - src_ptr) <= total_length);

   return;
}


/*++++++++++++++++++++++++++++ binary_sohetx() ++++++++++++++++++++++++++*/
static void
binary_sohetx(char *src_ptr, off_t total_length, char *file_name)
{
   char *ptr = src_ptr,
        *ptr_start;

   while ((*ptr != 1) && ((ptr - src_ptr) <= total_length))
   {
      ptr++;
   }
   if (*ptr == 1)
   {
      ptr_start = ptr;
      ptr += (total_length - 1);
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
                     _("Failed to locate terminating ETX in %s."),
                     file_name);
         return;
      }
   }

   return;
}


/*+++++++++++++++++++++++++++ ascii_zczc_nnnn() +++++++++++++++++++++++++*/
static void
ascii_zczc_nnnn(char *src_ptr, off_t total_length, char *file_name)
{
   char *ptr = src_ptr,
        *ptr_start;

   do
   {
      ptr_start = ptr;
      while (((*ptr == 13) || (*ptr == 10)) &&
             ((ptr - src_ptr) <= total_length))
      {
         ptr++;
      }
      if (((ptr - src_ptr + 4) <= total_length) &&
          (*ptr == 'Z') && (*(ptr + 1) == 'C') && (*(ptr + 2) == 'Z') &&
          (*(ptr + 3) == 'C'))
      {
         ptr += 4;
         while (((ptr - src_ptr + 5) <= total_length) &&
                (!(((*ptr == 13) || (*ptr == 10)) &&
                   (*(ptr + 1) == 'N') && (*(ptr + 2) == 'N') &&
                   (*(ptr + 3) == 'N') && (*(ptr + 4) == 'N'))))
         {
            ptr++;
         }
         if (((*ptr == 13) || (*ptr == 10)) &&
             (*(ptr + 1) == 'N') && (*(ptr + 2) == 'N') &&
             (*(ptr + 3) == 'N') && (*(ptr + 4) == 'N'))
         {
            ptr += 5;
            if (write_file(ptr_start, (unsigned int)(ptr - ptr_start), NEITHER) < 0)
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
                        _("Failed to locate terminating NNNN in %s."),
                        file_name);
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
   int   i,
         fd,
         offset;
   off_t size;
   char  *ptr,
         *p_start;

   /*
    * Build the file name from the bulletin header.
    */
   if ((soh_etx == YES) && (msg[0] != 1)) /* Must start with SOH */
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to read bulletin header. No SOH at start in %s."),
                  p_orig_name);
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
                  _("Failed to read bulletin header. No header found in %s (%d >= %u)."),
                  p_orig_name, (int)(ptr + 3 - msg), length);
      return(INCORRECT);
   }
   if (((ptr + 4 - msg) <= length) && (*ptr == 'Z') && (*(ptr + 1) == 'C') &&
       (*(ptr + 2) == 'Z') && (*(ptr + 3) == 'C'))
   {
      ptr += 4;
      while ((*ptr == ' ') && ((ptr - msg) < length))
      {
         ptr++;
      }
      if ((ptr + 3 - msg) >= length)
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to read bulletin header. No header found in %s."),
                     p_orig_name);
         return(INCORRECT);
      }
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
                  _("Length of WMO header is 0 in %s!? Discarding file."),
                  p_orig_name);
      return(INCORRECT);
   }
   else
   {
      if (p_file_name[i - 1] == '_')
      {
         p_file_name[i - 1] = '\0';
         i--;
      }
      else
      {
         p_file_name[i] = '\0';
      }
   }

   if (extract_options & EXTRACT_REPORTS)
   {
      while (((*ptr == 13) || (*ptr == 10)) && ((ptr - msg) < length))
      {
         ptr++;
      }
   }
   if (extract_options & EXTRACT_REPORTS)
   {
      if ((((p_file_name[0] == 'F') &&
            ((p_file_name[1] == 'T') || (p_file_name[1] == 'C'))) ||
           ((p_file_name[0] == 'S') &&
            ((p_file_name[1] == 'A') || (p_file_name[1] == 'H') ||
             (p_file_name[1] == 'I') || (p_file_name[1] == 'M') ||
             (p_file_name[1] == 'N') || (p_file_name[1] == 'P') ||
             (p_file_name[1] == 'X'))) ||
           ((p_file_name[0] == 'U') &&
            ((p_file_name[1] == 'S') || (p_file_name[1] == 'K') ||
             (p_file_name[1] == 'L') || (p_file_name[1] == 'E') ||
             (p_file_name[1] == 'P') || (p_file_name[1] == 'G') ||
             (p_file_name[1] == 'H') || (p_file_name[1] == 'Q')))) &&
          (check_report(ptr, length - (ptr - msg), &offset) == SUCCESS))
      {
         int end_offset,
             file_name_offset = i,
             not_wanted = NO;

         p_file_name[file_name_offset] = '_';
         file_name_offset++;
         ptr += offset;

         do
         {
            p_start = ptr;

            /* Ignore any spaces at start. */
            while ((*ptr == ' ') && ((ptr - msg) < length))
            {
               ptr++;
            }

            /* TAF */
            if (((ptr + 9 - msg) < length) && (*ptr == 'T') &&
                (*(ptr + 1) == 'A') && (*(ptr + 2) == 'F') &&
                (*(ptr + 3) == ' ') &&
                (isupper((int)(*(ptr + 4)))) && (isupper((int)(*(ptr + 5)))) &&
                (isupper((int)(*(ptr + 6)))) && (isupper((int)(*(ptr + 7)))) &&
                (*(ptr + 8) == ' '))
            {
               p_file_name[file_name_offset] = *(ptr + 4);
               p_file_name[file_name_offset + 1] = *(ptr + 5);
               p_file_name[file_name_offset + 2] = *(ptr + 6);
               p_file_name[file_name_offset + 3] = *(ptr + 7);
               end_offset = 4;
               ptr += 9;
            }
                 /* TAF AMD or COR */
            else if (((ptr + 13 - msg) < length) && (isupper((int)(*ptr))) &&
                     (isupper((int)(*(ptr + 1)))) &&
                     (isupper((int)(*(ptr + 2)))) && (*(ptr + 3) == ' ') &&
                     (isupper((int)(*(ptr + 4)))) &&
                     (isupper((int)(*(ptr + 5)))) &&
                     (isupper((int)(*(ptr + 6)))) && (*(ptr + 7) == ' ') &&
                     (isupper((int)(*(ptr + 8)))) &&
                     (isupper((int)(*(ptr + 9)))) &&
                     (isupper((int)(*(ptr + 10)))) &&
                     (isupper((int)(*(ptr + 11)))) && (*(ptr + 12) == ' '))
                 {
                    p_file_name[file_name_offset] = *(ptr + 8);
                    p_file_name[file_name_offset + 1] = *(ptr + 9);
                    p_file_name[file_name_offset + 2] = *(ptr + 10);
                    p_file_name[file_name_offset + 3] = *(ptr + 11);
                    end_offset = 4;
                    ptr += 13;
                 }
                 /* METAR or SPECI */
            else if (((ptr + 6 - msg) < length) &&
                     (((*ptr == 'M') && (*(ptr + 1) == 'E') &&
                       (*(ptr + 2) == 'T') && (*(ptr + 3) == 'A') &&
                       (*(ptr + 4) == 'R')) || 
                      ((*ptr == 'S') && (*(ptr + 1) == 'P') &&
                       (*(ptr + 2) == 'E') && (*(ptr + 3) == 'C') &&
                       (*(ptr + 4) == 'I'))) && 
                     (*(ptr + 5) == ' '))
                 {
                    while (((ptr + 6 - msg) < length) && (*(ptr + 6) == ' '))
                    {
                       ptr++;
                    }
                    if (((ptr + 4 - msg) < length) &&
                        (*(ptr + 6) == 'C') && (*(ptr + 7) == 'O') &&
                        (*(ptr + 8) == 'R') && (*(ptr + 9) == ' '))
                    {
                       ptr += 4;
                    }
                    if (((ptr + 5 - msg) < length) &&
                        ((isupper((int)(*(ptr + 6)))) ||
                         (isdigit((int)(*(ptr + 6))))) &&
                        ((isupper((int)(*(ptr + 7)))) ||
                         (isdigit((int)(*(ptr + 6))))) &&
                        ((isupper((int)(*(ptr + 8)))) ||
                         (isdigit((int)(*(ptr + 6))))) &&
                        ((isupper((int)(*(ptr + 9)))) ||
                         (isdigit((int)(*(ptr + 6))))) &&
                        (*(ptr + 10) == ' '))
                    {
                       p_file_name[file_name_offset] = *(ptr + 6);
                       p_file_name[file_name_offset + 1] = *(ptr + 7);
                       p_file_name[file_name_offset + 2] = *(ptr + 8);
                       p_file_name[file_name_offset + 3] = *(ptr + 9);
                       end_offset = 4;
                       ptr += 11;
                    }
                    else if (((ptr + 6 - msg) < length) &&
                             ((isupper((int)(*(ptr + 6)))) ||
                              (isdigit((int)(*(ptr + 6))))) &&
                             ((isupper((int)(*(ptr + 7)))) ||
                              (isdigit((int)(*(ptr + 7))))) &&
                             ((isupper((int)(*(ptr + 8)))) ||
                              (isdigit((int)(*(ptr + 8))))) &&
                             ((isupper((int)(*(ptr + 9)))) ||
                              (isdigit((int)(*(ptr + 9))))) &&
                             ((isupper((int)(*(ptr + 10)))) ||
                              (isdigit((int)(*(ptr + 10))))) &&
                             (*(ptr + 11) == ' '))
                         {
                            p_file_name[file_name_offset] = *(ptr + 6);
                            p_file_name[file_name_offset + 1] = *(ptr + 7);
                            p_file_name[file_name_offset + 2] = *(ptr + 8);
                            p_file_name[file_name_offset + 3] = *(ptr + 9);
                            p_file_name[file_name_offset + 4] = *(ptr + 10);
                            end_offset = 5;
                            ptr += 12;
                         }
                         else
                         {
                            show_unknown_report(ptr, length, msg, __FILE__, __LINE__);
                            break;
                         }
                 }
                 /* METAR, SPECI, TAF AMD, AAXX or BBXX (in a group) */
            else if (((ptr + 5 - msg) < length) &&
                     ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
                     ((isupper((int)(*(ptr + 1)))) ||
                      (isdigit((int)(*(ptr + 1))))) &&
                     ((isupper((int)(*(ptr + 2)))) ||
                      (isdigit((int)(*(ptr + 2))))) &&
                     ((isupper((int)(*(ptr + 3)))) ||
                      (isdigit((int)(*(ptr + 3))))) &&
                     (*(ptr + 4) == ' '))
                 {
                    p_file_name[file_name_offset] = *ptr;
                    p_file_name[file_name_offset + 1] = *(ptr + 1);
                    p_file_name[file_name_offset + 2] = *(ptr + 2);
                    p_file_name[file_name_offset + 3] = *(ptr + 3);
                    end_offset = 4;
                    ptr += 5;
                 }
                 /* German METAR */
            else if (((ptr + 13 - p_start) < length) &&
                     (isupper((int)(*ptr))) && (isupper((int)(*(ptr + 1)))) &&
                     (isupper((int)(*(ptr + 2)))) && (isupper((int)(*(ptr + 3)))) &&
                     (*(ptr + 4) == ' ') && (isdigit((int)(*(ptr + 5)))) &&
                     (isdigit((int)(*(ptr + 6)))) && (isdigit((int)(*(ptr + 7)))) &&
                     (isdigit((int)(*(ptr + 8)))) && (isdigit((int)(*(ptr + 9)))) &&
                     (isdigit((int)(*(ptr + 10)))) && (*(ptr + 11) == 'Z') &&
                     (*(ptr + 12) == ' '))
                 {
                    p_file_name[file_name_offset] = *ptr;
                    p_file_name[file_name_offset + 1] = *(ptr + 1);
                    p_file_name[file_name_offset + 2] = *(ptr + 2);
                    p_file_name[file_name_offset + 3] = *(ptr + 3);
                    end_offset = 4;
                    ptr += 13;
                 }
                 /* AAXX or BBXX (in a group) */
            else if (((ptr + 7 - msg) < length) &&
                     ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
                     ((isupper((int)(*(ptr + 1)))) ||
                      (isdigit((int)(*(ptr + 1))))) &&
                     ((isupper((int)(*(ptr + 2)))) ||
                      (isdigit((int)(*(ptr + 2))))) &&
                     ((isupper((int)(*(ptr + 3)))) ||
                      (isdigit((int)(*(ptr + 3))))) &&
                     ((isupper((int)(*(ptr + 4)))) ||
                      (isdigit((int)(*(ptr + 4))))) &&
                     ((isupper((int)(*(ptr + 5)))) ||
                      (isdigit((int)(*(ptr + 5))))) &&
                     (*(ptr + 6) == ' '))
                 {
                    p_file_name[file_name_offset] = *ptr;
                    p_file_name[file_name_offset + 1] = *(ptr + 1);
                    p_file_name[file_name_offset + 2] = *(ptr + 2);
                    p_file_name[file_name_offset + 3] = *(ptr + 3);
                    p_file_name[file_name_offset + 4] = *(ptr + 4);
                    p_file_name[file_name_offset + 5] = *(ptr + 5);
                    end_offset = 6;
                    ptr += 7;
                 }
                 /* AAXX or BBXX (in a group) */
            else if (((ptr + 8 - msg) < length) &&
                     ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
                     ((isupper((int)(*(ptr + 1)))) ||
                      (isdigit((int)(*(ptr + 1))))) &&
                     ((isupper((int)(*(ptr + 2)))) ||
                      (isdigit((int)(*(ptr + 2))))) &&
                     ((isupper((int)(*(ptr + 3)))) ||
                      (isdigit((int)(*(ptr + 3))))) &&
                     ((isupper((int)(*(ptr + 4)))) ||
                      (isdigit((int)(*(ptr + 4))))) &&
                     ((isupper((int)(*(ptr + 5)))) ||
                      (isdigit((int)(*(ptr + 5))))) &&
                     ((isupper((int)(*(ptr + 6)))) ||
                      (isdigit((int)(*(ptr + 6))))) &&
                     (*(ptr + 7) == ' '))
                 {
                    p_file_name[file_name_offset] = *ptr;
                    p_file_name[file_name_offset + 1] = *(ptr + 1);
                    p_file_name[file_name_offset + 2] = *(ptr + 2);
                    p_file_name[file_name_offset + 3] = *(ptr + 3);
                    p_file_name[file_name_offset + 4] = *(ptr + 4);
                    p_file_name[file_name_offset + 5] = *(ptr + 5);
                    p_file_name[file_name_offset + 6] = *(ptr + 6);
                    end_offset = 7;
                    ptr += 8;
                 }
                 /* SYNOP, AAXX or BBXX (in a group) */
            else if (((ptr + 6 - msg) < length) &&
                     ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
                     ((isupper((int)(*(ptr + 1)))) ||
                      (isdigit((int)(*(ptr + 1))))) &&
                     ((isupper((int)(*(ptr + 2)))) ||
                      (isdigit((int)(*(ptr + 2))))) &&
                     ((isupper((int)(*(ptr + 3)))) ||
                      (isdigit((int)(*(ptr + 3))))) &&
                     ((isupper((int)(*(ptr + 4)))) ||
                      (isdigit((int)(*(ptr + 4))))) &&
                     (*(ptr + 5) == ' '))
                 {
                    p_file_name[file_name_offset] = *ptr;
                    p_file_name[file_name_offset + 1] = *(ptr + 1);
                    p_file_name[file_name_offset + 2] = *(ptr + 2);
                    p_file_name[file_name_offset + 3] = *(ptr + 3);
                    p_file_name[file_name_offset + 4] = *(ptr + 4);
                    end_offset = 5;
                    ptr += 6;
                 }
                 /* SHDL */
            else if (((ptr + 12 - msg) < length) &&
                     (isupper((int)(*ptr))) && (isupper((int)(*(ptr + 1)))) &&
                     (isupper((int)(*(ptr + 2)))) &&
                     (isupper((int)(*(ptr + 3)))) && (*(ptr + 4) == ' ') &&
                     (isdigit((int)(*(ptr + 5)))) &&
                     (isdigit((int)(*(ptr + 6)))) &&
                     (isdigit((int)(*(ptr + 7)))) &&
                     (isdigit((int)(*(ptr + 8)))) &&
                     (isdigit((int)(*(ptr + 9)))) &&
                     (isdigit((int)(*(ptr + 10)))) &&
                     (isdigit((int)(*(ptr + 11)))))
                 {
                    ptr += 12;
                    while (((*ptr == 13) || (*ptr == 10)) &&
                           ((ptr - msg) < length))
                    {
                       ptr++;
                    }
                    p_start = ptr;
                    p_file_name[file_name_offset] = *ptr;
                    p_file_name[file_name_offset + 1] = *(ptr + 1);
                    p_file_name[file_name_offset + 2] = *(ptr + 2);
                    p_file_name[file_name_offset + 3] = *(ptr + 3);
                    p_file_name[file_name_offset + 4] = *(ptr + 4);
                    end_offset = 5;
                 }
                 /* NIL */
            else if (((ptr + 4 - msg) < length) && (*ptr == 'N') &&
                     (*(ptr + 1) == 'I') && (*(ptr + 2) == 'L') &&
                     ((*(ptr + 3) == 13) || (*(ptr + 3) == 10)))
                 {
                    ptr += 4;
                    while (((*ptr == 13) || (*ptr == 10)) &&
                           ((ptr - msg) < length))
                    {
                       ptr++;
                    }
                    continue;
                 }
                 /* NIL= */
            else if (((ptr + 5 - msg) < length) && (*ptr == 'N') &&
                     (*(ptr + 1) == 'I') && (*(ptr + 2) == 'L') &&
                     (*(ptr + 3) == '=') &&
                     ((*(ptr + 4) == 13) || (*(ptr + 4) == 10)))
                 {
                    ptr += 5;
                    while (((*ptr == 13) || (*ptr == 10)) &&
                           ((ptr - msg) < length))
                    {
                       ptr++;
                    }
                    continue;
                 }
                 /* TAF NIL= */
            else if (((ptr + 9 - msg) < length) && (*ptr == 'T') &&
                     (*(ptr + 1) == 'A') && (*(ptr + 2) == 'F') &&
                     (*(ptr + 3) == ' ') && (*(ptr + 4) == 'N') &&
                     (*(ptr + 5) == 'I') && (*(ptr + 6) == 'L') &&
                     (*(ptr + 7) == '=') &&
                     ((*(ptr + 8) == 13) || (*(ptr + 8) == 10)))
                 {
                    ptr += 9;
                    while (((*ptr == 13) || (*ptr == 10)) &&
                           ((ptr - msg) < length))
                    {
                       ptr++;
                    }
                    continue;
                 }
                 else
                 {
                    show_unknown_report(ptr, length, msg, __FILE__, __LINE__);
                    break;
                 }
   
            if (extract_p_filter != NULL)
            {
               p_file_name[file_name_offset + end_offset] = '\0';
               if (pmatch(extract_p_filter, p_file_name, NULL) != 0)
               {
                  /* Ignore this report. */
                  not_wanted = YES;
               }
               else
               {
                  not_wanted = NO;
               }
            }
            while ((*ptr != '=') && ((ptr - msg) < length))
            {
               ptr++;
            }
            while (*ptr == '=')
            {
               ptr++;
            }
            while (((*ptr == 13) || (*ptr == 10)) && ((ptr - msg) < length))
            {
               ptr++;
            }
            if (not_wanted == NO)
            {
               size = ptr - p_start;
               if (extract_options & EXTRACT_ADD_CRC_CHECKSUM)
               {
                  end_offset += sprintf(&p_file_name[file_name_offset + end_offset],
                                        "-%x", get_checksum(p_start, (int)size));
               }
               if (extract_options & EXTRACT_ADD_UNIQUE_NUMBER)
               {
                  (void)next_counter(counter_fd, counter, MAX_MSG_PER_SEC);
                  end_offset += sprintf(&p_file_name[file_name_offset + end_offset],
                                        "-%04x", *counter);
               }
               p_file_name[file_name_offset + end_offset] = '\0';

               if ((fd = open(p_full_file_name, (O_RDWR | O_CREAT | O_TRUNC),
                              file_mode)) < 0)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("Failed to open() `%s' while extracting reports : %s"),
                              p_full_file_name, strerror(errno));
                  *p_file_name = '\0';
                  return(INCORRECT);
               }
               if (writen(fd, p_start, (size_t)size, 0) != (ssize_t)size)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("Failed to writen() report : %s"), strerror(errno));
                  (void)close(fd);
                  return(INCORRECT);
               }
               (*p_files_to_send)++;
               *p_file_size += size;

               if (close(fd) == -1)
               {
                  receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                              _("close() error : %s"), strerror(errno));
               }
#ifdef _PRODUCTION_LOG
               if (no_of_log_entries == 0)
               {
                  if ((pld = malloc((LOG_ENTRY_STEP_SIZE * sizeof(struct prod_log_db)))) == NULL)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("malloc() error : %s"), strerror(errno));
                  }
               }
               else
               {
                  if ((no_of_log_entries % LOG_ENTRY_STEP_SIZE) == 0)
                  {
                     size_t new_size;

                     new_size = ((no_of_log_entries / LOG_ENTRY_STEP_SIZE) + 1) *
                                LOG_ENTRY_STEP_SIZE * sizeof(struct prod_log_db);
                     if ((pld = realloc(pld, new_size)) == NULL)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   _("realloc() error : %s"), strerror(errno));
                     }
                  }
               }
               if (pld != NULL)
               {
                  pld[no_of_log_entries].size = size;
                  (void)strcpy(pld[no_of_log_entries].file_name, p_file_name);
                  no_of_log_entries++;
               }
#endif
            }
         } while ((ptr + 6 - msg) < length);
      }
      else
      {
         receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                     _("%s not marked as a report"), p_file_name);
      }
   }
   else
   {
      if (extract_p_filter != NULL)
      {
         p_file_name[i] = '\0';
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
         (void)next_counter(counter_fd, counter, MAX_MSG_PER_SEC);
         i += sprintf(&p_file_name[i], "-%04x", *counter);
      }
      p_file_name[i] = '\0';

      if ((fd = open(p_full_file_name, (O_RDWR | O_CREAT | O_TRUNC),
                     file_mode)) < 0)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to open() `%s' while extracting bulletins : %s"),
                     p_full_file_name, strerror(errno));
         *p_file_name = '\0';
         return(INCORRECT);
      }

      if ((extract_options & EXTRACT_ADD_SOH_ETX) &&
          ((extract_options & EXTRACT_REMOVE_WMO_HEADER) == 0))
      {
         if (soh_etx == NO)
         {
            if (write(fd, "\001", 1) != 1)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("Failed to write() SOH : %s"), strerror(errno));
               (void)close(fd);
               return(INCORRECT);
            }
         }
         if (writen(fd, msg, (size_t)length, 0) != (ssize_t)length)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to writen() message : %s"), strerror(errno));
            (void)close(fd);
            return(INCORRECT);
         }
         if (soh_etx == NO)
         {
            if (write(fd, "\003", 1) != 1)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("Failed to write() ETX : %s"), strerror(errno));
               (void)close(fd);
               return(INCORRECT);
            }
            size = length + 2;
         }
         else
         {
            size = length;
         }
      }
      else
      {
         if (extract_options & EXTRACT_REMOVE_WMO_HEADER)
         {
            p_start = ptr;
            while ((*p_start == '\012') || (*p_start == '\015'))
            {
               p_start++;
            }
         }

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
         if (writen(fd, p_start, (size_t)length, 0) != (ssize_t)length)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to writen() message : %s"), strerror(errno));
            (void)close(fd);
            return(INCORRECT);
         }
         size = length;
      }
      (*p_files_to_send)++;
      *p_file_size += size;

      if (close(fd) == -1)
      {
         receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                     _("close() error : %s"), strerror(errno));
      }
#ifdef _PRODUCTION_LOG
      if (no_of_log_entries == 0)
      {
         if ((pld = malloc((LOG_ENTRY_STEP_SIZE * sizeof(struct prod_log_db)))) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("malloc() error : %s"), strerror(errno));
         }
      }
      else
      {
         if ((no_of_log_entries % LOG_ENTRY_STEP_SIZE) == 0)
         {
            size_t new_size;

            new_size = ((no_of_log_entries / LOG_ENTRY_STEP_SIZE) + 1) *
                       LOG_ENTRY_STEP_SIZE * sizeof(struct prod_log_db);
            if ((pld = realloc(pld, new_size)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("realloc() error : %s"), strerror(errno));
            }
         }
      }
      if (pld != NULL)
      {
         pld[no_of_log_entries].size = size;
         (void)strcpy(pld[no_of_log_entries].file_name, p_file_name);
         no_of_log_entries++;
      }
#endif
   }
   *p_file_name = '\0';

   return(SUCCESS);
}


/*---------------------------- check_report() ---------------------------*/
static int
check_report(char *ptr, unsigned int length, int *offset)
{
   int  xxtype = NO;
   char *p_start = ptr;

   /* Ignore any spaces at start. */
   while ((*ptr == ' ') && ((ptr - p_start) < length))
   {
      ptr++;
   }

   /*
    * Lets first check if this is a SYNOP, SPECI, TAF, 'TAF AMD', AAXX,
    * BBXX which all have an extra line.
    */
   if (((ptr + 11 - p_start) < length) && (isupper((int)(*ptr))) &&
       (isupper((int)(*(ptr + 1)))) && (isupper((int)(*(ptr + 2)))) &&
       (isupper((int)(*(ptr + 3)))) && (*(ptr + 4) == ' ') &&
       (isdigit((int)(*(ptr + 5)))) && (isdigit((int)(*(ptr + 6)))) &&
       (isdigit((int)(*(ptr + 7)))) && (isdigit((int)(*(ptr + 8)))) &&
       (isdigit((int)(*(ptr + 9)))) &&
       ((*(ptr + 10) == 13) || (*(ptr + 10) == 10)))
   {
      ptr += 11;
      while (((*ptr == 13) || (*ptr == 10)) && ((ptr - p_start) < length))
      {
         ptr++;
      }
      *offset = ptr - p_start;
   }
        /* SPECI, METAR */
   else if (((ptr + 6 - p_start) < length) &&
            ((*(ptr + 5) == 13) || (*(ptr + 5) == 10)) &&
            (((*ptr == 'S') && (*(ptr + 1) == 'P') && (*(ptr + 2) == 'E') &&
              (*(ptr + 3) == 'C') && (*(ptr + 4) == 'I')) ||
             ((*ptr == 'M') && (*(ptr + 1) == 'E') && (*(ptr + 2) == 'T') &&
              (*(ptr + 3) == 'A') && (*(ptr + 4) == 'R'))))
        {
           ptr += 6;
           while (((*ptr == 13) || (*ptr == 10)) && ((ptr - p_start) < length))
           {
              ptr++;
           }
           *offset = ptr - p_start;
        }
        /* METAR YYGGggZ */
   else if (((ptr + 14 - p_start) < length) &&
            (*ptr == 'M') && (*(ptr + 1) == 'E') && (*(ptr + 2) == 'T') &&
            (*(ptr + 3) == 'A') && (*(ptr + 4) == 'R') &&
            (*(ptr + 5) == ' ') && (isdigit((int)(*(ptr + 6)))) &&
            (isdigit((int)(*(ptr + 7)))) && (isdigit((int)(*(ptr + 8)))) &&
            (isdigit((int)(*(ptr + 9)))) && (isdigit((int)(*(ptr + 10)))) &&
            (isdigit((int)(*(ptr + 11)))) && (*(ptr + 12) == 'Z') &&
            ((*(ptr + 13) == 13) || (*(ptr + 13) == 10)))
        {
           ptr += 14;
           while (((*ptr == 13) || (*ptr == 10)) && ((ptr - p_start) < length))
           {
              ptr++;
           }
           *offset = ptr - p_start;
        }
        /* METAR COR */
   else if (((ptr + 10 - p_start) < length) &&
            (*ptr == 'M') && (*(ptr + 1) == 'E') && (*(ptr + 2) == 'T') &&
            (*(ptr + 3) == 'A') && (*(ptr + 4) == 'R') &&
            (*(ptr + 5) == ' ') && (*(ptr + 6) == 'C') &&
            (*(ptr + 7) == 'O') && (*(ptr + 8) == 'R') &&
            ((*(ptr + 9) == 13) || (*(ptr + 9) == 10)))
        {
           ptr += 10;
           while (((*ptr == 13) || (*ptr == 10)) && ((ptr - p_start) < length))
           {
              ptr++;
           }
           *offset = ptr - p_start;
        }
        /* AUTOTREND */
   else if (((ptr + 10 - p_start) < length) &&
            (*ptr == 'A') && (*(ptr + 1) == 'U') && (*(ptr + 2) == 'T') &&
            (*(ptr + 3) == 'O') && (*(ptr + 4) == 'T') &&
            (*(ptr + 5) == 'R') && (*(ptr + 6) == 'E') &&
            (*(ptr + 7) == 'N') && (*(ptr + 8) == 'D') &&
            ((*(ptr + 9) == 13) || (*(ptr + 9) == 10)))
        {
           ptr += 10;
           while (((*ptr == 13) || (*ptr == 10)) && ((ptr - p_start) < length))
           {
              ptr++;
           }
           *offset = ptr - p_start;
        }
        /* AAXX or BBXX */
   else if (((ptr + 6 - p_start) < length) &&
            (((*ptr == 'A') && (*(ptr + 1) == 'A')) ||
             ((*ptr == 'B') && (*(ptr + 1) == 'B'))) &&
            (*(ptr + 2) == 'X') && (*(ptr + 3) == 'X') &&
            ((*(ptr + 4) == 13) || (*(ptr + 4) == 10)))
        {
           ptr += 5;
           while (((*ptr == 13) || (*ptr == 10)) && ((ptr - p_start) < length))
           {
              ptr++;
           }
           xxtype = YES;
           *offset = ptr - p_start;
        }
        /* TAF */
   else if (((ptr + 4 - p_start) < length) && (*ptr == 'T') &&
            (*(ptr + 1) == 'A') && (*(ptr + 2) == 'F') &&
            ((*(ptr + 3) == 13) || (*(ptr + 3) == 10)))
        {
           ptr += 4;
           while (((*ptr == 13) || (*ptr == 10)) && ((ptr - p_start) < length))
           {
              ptr++;
           }
           *offset = ptr - p_start;
        }
        /* TAF AMD or COR */
   else if (((ptr + 8 - p_start) < length) && (*ptr == 'T') &&
            (*(ptr + 1) == 'A') && (*(ptr + 2) == 'F') &&
            (*(ptr + 3) == ' ') &&
            (((*(ptr + 4) == 'A') && (*(ptr + 5) == 'M') &&
              (*(ptr + 6) == 'D')) ||
             ((*(ptr + 4) == 'C') && (*(ptr + 5) == 'O') &&
              (*(ptr + 6) == 'R'))) &&
            ((*(ptr + 7) == 13) || (*(ptr + 7) == 10)))
        {
           ptr += 8;
           while (((*ptr == 13) || (*ptr == 10)) && ((ptr - p_start) < length))
           {
              ptr++;
           }
           *offset = ptr - p_start;
        }
        /* Identify german TEXT as bulletins. */
   else if (((ptr + 5 - p_start) < length) && (*ptr == 'T') &&
            (*(ptr + 1) == 'E') && (*(ptr + 2) == 'X') &&
            (*(ptr + 3) == 'T') && (*(ptr + 4) == ' '))
        {
           return(INCORRECT);
        }
        /* GAFOR */
   else if (((ptr + 6 - p_start) < length) && (*ptr == 'G') &&
            (*(ptr + 1) == 'A') && (*(ptr + 2) == 'F') &&
            (*(ptr + 3) == 'O') && (*(ptr + 4) == 'R') &&
            (*(ptr + 5) == ' '))
        {
           return(INCORRECT);
        }
        else
        {
           *offset = 0;
        }

   /* Ignore any spaces at start. */
   while ((*ptr == ' ') && ((ptr - p_start) < length))
   {
      ptr++;
   }

   /* TAF */
   if (((ptr + 9 - p_start) < length) && (*ptr == 'T') &&
       (*(ptr + 1) == 'A') && (*(ptr + 2) == 'F') &&
       (*(ptr + 3) == ' ') && (*(ptr + 8) == ' ') &&
       (isupper((int)(*(ptr + 4)))) && (isupper((int)(*(ptr + 5)))) &&
       (isupper((int)(*(ptr + 6)))) && (isupper((int)(*(ptr + 7)))))
   {
      return(SUCCESS);
   }
        /* TAF AMD or COR */
   else if (((ptr + 13 - p_start) < length) && (isupper((int)(*ptr))) &&
            (isupper((int)(*(ptr + 1)))) &&
            (isupper((int)(*(ptr + 2)))) && (*(ptr + 3) == ' ') &&
            (isupper((int)(*(ptr + 4)))) &&
            (isupper((int)(*(ptr + 5)))) &&
            (isupper((int)(*(ptr + 6)))) && (*(ptr + 7) == ' ') &&
            (isupper((int)(*(ptr + 8)))) &&
            (isupper((int)(*(ptr + 9)))) &&
            (isupper((int)(*(ptr + 10)))) &&
            (isupper((int)(*(ptr + 11)))) && (*(ptr + 12) == ' '))
        {
           return(SUCCESS);
        }
        /* METAR or SPECI */
   else if (((ptr + 6 - p_start) < length) &&
            (((*ptr == 'M') && (*(ptr + 1) == 'E') &&
              (*(ptr + 2) == 'T') && (*(ptr + 3) == 'A') &&
              (*(ptr + 4) == 'R')) || 
             ((*ptr == 'S') && (*(ptr + 1) == 'P') &&
              (*(ptr + 2) == 'E') && (*(ptr + 3) == 'C') &&
              (*(ptr + 4) == 'I'))) && 
            (*(ptr + 5) == ' '))
        {
           while (((ptr + 6 - p_start) < length) && (*(ptr + 6) == ' '))
           {
              ptr++;
           }
           if (((ptr + 4 - p_start) < length) &&
               (*(ptr + 6) == 'C') && (*(ptr + 7) == 'O') &&
               (*(ptr + 8) == 'R') && (*(ptr + 9) == ' '))
           {
              ptr += 4;
           }
           if (((ptr + 5 - p_start) < length) &&
               ((isupper((int)(*(ptr + 6)))) || (isdigit((int)(*(ptr + 6))))) &&
               ((isupper((int)(*(ptr + 7)))) || (isdigit((int)(*(ptr + 7))))) &&
               ((isupper((int)(*(ptr + 8)))) || (isdigit((int)(*(ptr + 8))))) &&
               ((isupper((int)(*(ptr + 9)))) || (isdigit((int)(*(ptr + 9))))) &&
               (*(ptr + 10) == ' '))
           {
              return(SUCCESS);
           }
           else if (((ptr + 6 - p_start) < length) &&
                    ((isupper((int)(*(ptr + 6)))) ||
                     (isdigit((int)(*(ptr + 6))))) &&
                    ((isupper((int)(*(ptr + 7)))) ||
                     (isdigit((int)(*(ptr + 7))))) &&
                    ((isupper((int)(*(ptr + 8)))) ||
                     (isdigit((int)(*(ptr + 8))))) &&
                    ((isupper((int)(*(ptr + 9)))) ||
                     (isdigit((int)(*(ptr + 9))))) &&
                    ((isupper((int)(*(ptr + 10)))) ||
                     (isdigit((int)(*(ptr + 10))))) &&
                    (*(ptr + 11) == ' '))
                {
                   return(SUCCESS);
                }
        }
        /* METAR, SPECI, TAF AMD, AAXX or BBXX (in a group) */
   else if (((ptr + 5 - p_start) < length) &&
            ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
            ((isupper((int)(*(ptr + 1)))) || (isdigit((int)(*(ptr + 1))))) &&
            ((isupper((int)(*(ptr + 2)))) || (isdigit((int)(*(ptr + 2))))) &&
            ((isupper((int)(*(ptr + 3)))) || (isdigit((int)(*(ptr + 3))))) &&
            (*(ptr + 4) == ' '))
        {
           return(SUCCESS);
        }
        /* AAXX or BBXX (in a group) */
   else if ((xxtype == YES) &&
            ((ptr + 7 - p_start) < length) &&
            ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
            ((isupper((int)(*(ptr + 1)))) || (isdigit((int)(*(ptr + 1))))) &&
            ((isupper((int)(*(ptr + 2)))) || (isdigit((int)(*(ptr + 2))))) &&
            ((isupper((int)(*(ptr + 3)))) || (isdigit((int)(*(ptr + 3))))) &&
            ((isupper((int)(*(ptr + 4)))) || (isdigit((int)(*(ptr + 4))))) &&
            ((isupper((int)(*(ptr + 5)))) || (isdigit((int)(*(ptr + 5))))) &&
            (*(ptr + 6) == ' '))
        {
           return(SUCCESS);
        }
        /* AAXX or BBXX (in a group) */
   else if ((xxtype == YES) &&
            ((ptr + 8 - p_start) < length) &&
            ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
            ((isupper((int)(*(ptr + 1)))) || (isdigit((int)(*(ptr + 1))))) &&
            ((isupper((int)(*(ptr + 2)))) || (isdigit((int)(*(ptr + 2))))) &&
            ((isupper((int)(*(ptr + 3)))) || (isdigit((int)(*(ptr + 3))))) &&
            ((isupper((int)(*(ptr + 4)))) || (isdigit((int)(*(ptr + 4))))) &&
            ((isupper((int)(*(ptr + 5)))) || (isdigit((int)(*(ptr + 5))))) &&
            ((isupper((int)(*(ptr + 6)))) || (isdigit((int)(*(ptr + 6))))) &&
            (*(ptr + 7) == ' '))
        {
           return(SUCCESS);
        }
        /* SYNOP, AAXX or BBXX (in a group) */
   else if (((ptr + 6 - p_start) < length) &&
            ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
            ((isupper((int)(*(ptr + 1)))) || (isdigit((int)(*(ptr + 1))))) &&
            ((isupper((int)(*(ptr + 2)))) || (isdigit((int)(*(ptr + 2))))) &&
            ((isupper((int)(*(ptr + 3)))) || (isdigit((int)(*(ptr + 3))))) &&
            ((isupper((int)(*(ptr + 4)))) || (isdigit((int)(*(ptr + 4))))) &&
            (*(ptr + 5) == ' '))
        {
           return(SUCCESS);
        }
        /* German METAR */
   else if (((ptr + 13 - p_start) < length) &&
            (isupper((int)(*ptr))) && (isupper((int)(*(ptr + 1)))) &&
            (isupper((int)(*(ptr + 2)))) && (isupper((int)(*(ptr + 3)))) &&
            (*(ptr + 4) == ' ') && (isdigit((int)(*(ptr + 5)))) &&
            (isdigit((int)(*(ptr + 6)))) && (isdigit((int)(*(ptr + 7)))) &&
            (isdigit((int)(*(ptr + 8)))) && (isdigit((int)(*(ptr + 9)))) &&
            (isdigit((int)(*(ptr + 10)))) && (*(ptr + 11) == 'Z') &&
            (*(ptr + 12) == ' '))
        {
           return(SUCCESS);
        }

   return(INCORRECT);
}


/*------------------------- show_unknown_report() -----------------------*/
static void
show_unknown_report(char *ptr, int length, char *msg, char *file, int line)
{
   int  i = 0;
   char unknown_report[MAX_REPORT_LINE_LENGTH + 1];

   while ((*ptr >= ' ') && (*ptr <= 126) &&
          ((ptr - msg) < length) && (i < MAX_REPORT_LINE_LENGTH))
   {
      unknown_report[i] = *ptr;
      i++; ptr++;
   }
   if (i == 0)
   {
      while ((*ptr != 10) && (i < MAX_REPORT_LINE_LENGTH) &&
             ((ptr - msg) < length))
      {
         if ((*ptr >= ' ') && (*ptr <= 126))
         {
            unknown_report[i] = *ptr;
            i++; ptr++;
         }
         else
         {
            i += sprintf(&unknown_report[i], "<%d>", (int)*ptr);
            ptr++;
         }
      }
      if ((*ptr == 10) && ((i + 4) < MAX_REPORT_LINE_LENGTH))
      {
         unknown_report[i] = '<';
         unknown_report[i + 1] = '1';
         unknown_report[i + 2] = '0';
         unknown_report[i + 3] = '>';
         unknown_report[i + 4] = '\0';
      }
   }
   else
   {
      if ((*ptr == 13) && ((i + 4) < MAX_REPORT_LINE_LENGTH) &&
          ((ptr - msg) < length))
      {
         unknown_report[i] = '<';
         unknown_report[i + 1] = '1';
         unknown_report[i + 2] = '3';
         unknown_report[i + 3] = '>';
         unknown_report[i + 4] = '\0';
         i += 4;
         ptr++;
      }
      if ((*ptr == 13) && ((i + 4) < MAX_REPORT_LINE_LENGTH) &&
          ((ptr - msg) < length))
      {
         unknown_report[i] = '<';
         unknown_report[i + 1] = '1';
         unknown_report[i + 2] = '3';
         unknown_report[i + 3] = '>';
         unknown_report[i + 4] = '\0';
         i += 4;
         ptr++;
      }
      if ((*ptr == 10) && ((i + 4) < MAX_REPORT_LINE_LENGTH) &&
          ((ptr - msg) < length))
      {
         unknown_report[i] = '<';
         unknown_report[i + 1] = '1';
         unknown_report[i + 2] = '0';
         unknown_report[i + 3] = '>';
         unknown_report[i + 4] = '\0';
         i += 4;
         ptr++;
      }
   }
   receive_log(DEBUG_SIGN, file, line, 0L, _("Unknown report type `%s' in %s"),
               unknown_report, p_orig_name);

   return;
}

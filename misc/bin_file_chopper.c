/*
 *  bin_file_chopper.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2002 Deutscher Wetterdienst (DWD),
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
 **   bin_file_chopper - breaks up a file containing bulletins to
 **                      one file per bulletin
 **
 ** SYNOPSIS
 **   int bin_file_chopper(char  *bin_file,
 **                        int   *files_to_send,
 **                        off_t *file_size,
 **                        char  wmo_header_file_name)
 **
 ** DESCRIPTION
 **   The function bin_file_chopper reads a binary WMO bulletin file,
 **   and writes each bulletin (GRIB, BUFR, BLOK) into a separate file.
 **   These files will have the following file name:
 **
 **       xxxx_yyyy_YYYYMMDDhhmmss_zzzz
 **        |    |         |         |
 **        |    |         |         +----> counter
 **        |    |         +--------------> date when created
 **        |    +------------------------> originator
 **        +-----------------------------> data type
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
 **   10.08.1996 H.Kiehl Created
 **   28.05.1998 H.Kiehl Added support for GRIB edition 1, ie using
 **                      length indicator to find end.
 **   18.11.2002 H.Kiehl WMO header file names.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>              /* strlen(), strcpy()                   */
#include <time.h>                /* time(), strftime(), gmtime()         */
#include <stdlib.h>              /* malloc(), free()                     */
#include <unistd.h>              /* close(), read()                      */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define DATA_TYPES 3

/* Global local variables */
static int  counter_fd,
            id_length[DATA_TYPES] = { 4, 4, 4 },
            end_id_length[DATA_TYPES] = { 4, 4, 4 };
static char bul_format[DATA_TYPES][5] =
            {
               "GRIB",
               "BUFR",
               "BLOK"
            },
            end_id[DATA_TYPES][5] =
            {
               "7777",
               "7777",
               "7777"
            };

/* External global variables */
extern char *p_work_dir;

/* Local functions */
static char *bin_search_start(char *, int, int *, size_t *);
static int  bin_search_end(char *, char *, size_t);


/*########################## bin_file_chopper() #########################*/
int
bin_file_chopper(char  *bin_file,
                 int   *files_to_send,
                 off_t *file_size,
                 char  wmo_header_file_name)
{
   int         i,
               fd,
               first_time = YES,
               data_length = 0,
               counter;     /* counter to keep file name unique          */
   size_t      length,
               total_length;
   time_t      tvalue;
   mode_t      file_mode;
   char        *buffer,
               *p_file,
               *ptr,
               new_file[MAX_PATH_LENGTH],
               *p_new_file,
               date_str[16],
               originator[MAX_FILENAME_LENGTH];
   struct stat stat_buf;

   if (stat(bin_file, &stat_buf) != 0)
   {
      if (errno == ENOENT)
      {
         /*
          * If the file is not there, why should we bother?
          */
         return(SUCCESS);
      }
      else
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     "Failed to stat() %s : %s", bin_file, strerror(errno));
         return(INCORRECT);
      }
   }

   /*
    * If the size of the file is less then 10 forget it. There must be
    * something really wrong.
    */
   if (stat_buf.st_size < 10)
   {
      return(INCORRECT);
   }
   file_mode = stat_buf.st_mode;

   if ((fd = open(bin_file, O_RDONLY)) < 0)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "Failed to open() %s : %s", bin_file, strerror(errno));
      return(INCORRECT);
   }

   if ((buffer = malloc(stat_buf.st_size)) == NULL)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "malloc() error (size = %d) : %s",
                  stat_buf.st_size, strerror(errno));
      return(INCORRECT);
   }
   p_file = buffer;

   /*
    * Read the hole file in one go. We can do this here since the
    * files in question are not larger then appr. 500KB.
    */
   if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "read() error : %s", strerror(errno));
      free(p_file);
      return(INCORRECT);
   }
   total_length = stat_buf.st_size;

   /* Close the file since we do not need it anymore */
   if (close(fd) == -1)
   {
      receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                  "close() error : %s", strerror(errno));
   }

   /* Get directory where files are to be stored. */
   (void)strcpy(new_file, bin_file);
   p_new_file = new_file + strlen(new_file);
   while ((*p_new_file != '/') && (p_new_file != new_file))
   {
      p_new_file--;
   }
   if (*p_new_file != '/')
   {
      /* Impossible */
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "Cannot determine directory where to store files!");
      return(INCORRECT);
   }

   /* Extract the originator */
#ifdef _DIR_ORIGINATOR
   ptr = p_new_file - 1;
   while ((*ptr != '/') && (ptr != new_file))
   {
      ptr--;
   }
   if (*ptr != '/')
   {
      /* Impossible */
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "Cannot determine directory where to store files!");
      return(INCORRECT);
   }
   ptr++;
   *p_new_file = '\0';
   (void)strcpy(originator, ptr);
   *p_new_file = '/';
#else
   (void)strcpy(originator, "XXX");
#endif
   p_new_file++;

   /*
    * Open AFD counter file to create unique numbers for the new
    * file names.
    */
   if ((counter_fd = open_counter_file(COUNTER_FILE)) < 0)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "Failed to open AFD counter file.");
      return(INCORRECT);
   }

   while (total_length > 9)
   {
      if ((ptr = bin_search_start(buffer, total_length, &i, &total_length)) != NULL)
      {
         unsigned int message_length = 0;

         /*
          * When data type is GRIB and it is still using edition
          * 0 we cannot use the length indicator.
          */
         if ((i == 0) && (*(ptr + 3) == 0))
         {
            /*
             * Let's look for the end. If we don't find an end marker
             * try get the next data type. Maybe this is not a good
             * idea and it would be better to discard this file.
             * Experience will show which is the better solution.
             */
            if ((data_length = bin_search_end(end_id[i], ptr, total_length)) == 0)
            {
#ifdef _END_DIFFER
               /*
                * Since we did not find a valid end_marker, it does not
                * mean that all data in this file is incorrect. Ignore
                * this bulletin and try search for the next data type
                * identifier. Since we have no clue where this might start
                * we have to extend the search across the whole file.
                */
               buffer = ptr;
               continue;
#else
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           "Failed to extract data from %s", bin_file);
               (void)close(counter_fd);
               free(p_file);
               return(INCORRECT);
#endif
            }
         }
         else /* When GRIB it has to be at least edition 1 */
         {
            /*
             * Determine length by reading byte 4 - 6.
             */
            message_length = 0;
            message_length |= (unsigned char)*ptr;
            message_length <<= 8;
            message_length |= (unsigned char)*(ptr + 1);
            message_length <<= 8;
            message_length |= (unsigned char)*(ptr + 2);

            if (message_length > (total_length + id_length[i]))
            {
               if (first_time == YES)
               {
                  receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                              "Hey! Whats this? Message length (%u) > then total length (%u) [%s]",
                              message_length, total_length + id_length[i],
                              bin_file);
                  first_time = NO;
               }
               buffer = ptr;
               continue;
            }
            else
            {
               char *tmp_ptr = ptr - id_length[i] +
                               message_length -
                               end_id_length[i];

               if (memcmp(tmp_ptr, end_id[i], end_id_length[i]) != 0)
               {
                  receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                              "Hey! Whats this? End locator not where it should be!");
                  buffer = ptr;
                  continue;
               }
            }
         }

         if (wmo_header_file_name == YES)
         {
            char *p_end;

            wmoheader_from_grib(ptr - 4, p_new_file, NULL);
            p_end = p_new_file + strlen(p_new_file);
            counter = 0;
            while (eaccess(new_file, F_OK) == 0)
            {
               (void)sprintf(p_end, ";%d", counter);
               counter++;
            }
         }
         else
         {
            /*
             * Create a unique file name of the following format:
             *
             *       xxxx_yyyy_YYYYMMDDhhmmss_zzzz
             *        |    |         |         |
             *        |    |         |         +----> counter
             *        |    |         +--------------> date when created
             *        |    +------------------------> originator
             *        +-----------------------------> data type
             */
            if (next_counter(&counter) < 0)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           "Failed to get the next number.");
               free(p_file);
               (void)close(counter_fd);
               return(INCORRECT);
            }
            if (time(&tvalue) == -1)
            {
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                           "Failed to get time() : %s", strerror(errno));
               (void)strcpy(date_str, "YYYYMMDDhhmmss");
            }
            else
            {
               length = strftime(date_str, sizeof(date_str) + 1, "%Y%m%d%H%M%S",
                                 gmtime(&tvalue));
               date_str[length] = '\0';
            }
            (void)sprintf(p_new_file, "%s_%s_%s_%d", bul_format[i],
                          originator, date_str, counter);
         }

         /*
          * Store data of each bulletin into an extra file.
          */
         if ((fd = open(new_file, (O_WRONLY | O_CREAT | O_TRUNC),
                        file_mode)) < 0)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, tvalue,
                        "Failed to open() %s : %s", new_file, strerror(errno));
            free(p_file);
            (void)close(counter_fd);
            return(INCORRECT);
         }

         /* Add data type and end identifier to file */
         ptr -= id_length[i];
         if (message_length == 0)
         {
            data_length = data_length + id_length[i] + end_id_length[i];
         }
         else
         {
            data_length = message_length;
         }

         if (write(fd, ptr, data_length) != data_length)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, tvalue,
                        "write() error : %s", strerror(errno));
            free(p_file);
            (void)close(fd);
            (void)unlink(new_file); /* End user should not get any junk! */
            (void)close(counter_fd);
            return(INCORRECT);
         }
         if (close(fd) == -1)
         {
            receive_log(DEBUG_SIGN, __FILE__, __LINE__, tvalue,
                        "close() error : %s", strerror(errno));
         }
         *file_size += data_length;
         (*files_to_send)++;
         length = data_length;
/*         length = data_length + end_id_length[i]; */
         if (data_length > total_length)
         {
            if ((data_length - total_length) > 5)
            {
               receive_log(DEBUG_SIGN, __FILE__, __LINE__, tvalue,
                           "Hmmm. data_length (%d) > total_length (%u)?",
                           data_length, total_length);
            }
            total_length = 0;
         }
         else
         {
/*            total_length -= data_length; */
            total_length -= (data_length - end_id_length[i]);
         }
         if (message_length != 0)
         {
            int rest;

            if ((rest = (message_length % 4)) == 0)
            {
               buffer = ptr + length;
            }
            else
            {
               buffer = ptr + length - rest;
               total_length += rest;
            }
         }
         else
         {
            buffer = ptr + length;
         }
      }
      else
      {
         /*
          * Since we did not find a valid data type identifier, lets
          * forget it.
          */
         break;
      }
   } /* while (total_length > 9) */

   /* Remove the original file */
   if (unlink(bin_file) < 0)
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  "Failed to unlink() original file %s : %s",
                  bin_file, strerror(errno));
   }
   else
   {
      *file_size -= stat_buf.st_size;
      (*files_to_send)--;
   }
   if (close(counter_fd) == -1)
   {
      receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                  "close() error : %s", strerror(errno));
   }
   free(p_file);

   return(SUCCESS);
}


/*+++++++++++++++++++++++++ bin_search_start() ++++++++++++++++++++++++++*/
static char *
bin_search_start(char   *search_text,
                 int    search_length,
                 int    *i,
                 size_t *total_length)
{
   int    hit[DATA_TYPES] = { 0, 0, 0 },
          count[DATA_TYPES] = { 0, 0, 0 },
          counter = 0;
   size_t tmp_length = *total_length;

   while (counter != search_length)
   {
      for (*i = 0; *i < DATA_TYPES; (*i)++)
      {
         if (*search_text == bul_format[*i][count[*i]])
         {
            if (++hit[*i] == 4)
            {
               (*total_length)--;
               return(++search_text);
            }
            (count[*i])++;
         }
         else
         {
            count[*i] = hit[*i] = 0;
         }
      }

      search_text++; counter++;
      (*total_length)--;
   }
   *total_length = tmp_length;

   return(NULL); /* Found nothing */
}


/*++++++++++++++++++++++++++ bin_search_end() +++++++++++++++++++++++++++*/
static int
bin_search_end(char   *search_string,
               char   *search_text,
               size_t total_length)
{
   int        hit = 0;
   static int counter;
   size_t     string_length = strlen(search_string);

   counter = 0;
   while (counter != total_length)
   {
      if (*(search_text++) == *(search_string++))
      {
         if (++hit == string_length)
         {
            counter -= (string_length - 1);
            return(counter);
         }
      }
      else
      {
         search_string -= hit + 1;
         hit = 0;
      }

      counter++;
   }

   return(0); /* Found nothing */
}

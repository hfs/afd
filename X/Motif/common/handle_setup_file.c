/*
 *  handle_setup_file.c - Part of AFD, an automatic file distribution
 *                        program.
 *  Copyright (c) 1997 - 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_setup_file - reads or writes the initial setup file
 **
 ** SYNOPSIS
 **   void read_setup(char *file_name,
 **                   int  *filename_display_length,
 **                   int  *his_log_set)
 **   void write_setup(int filename_display_length)
 **
 ** DESCRIPTION
 **   read_setup() looks in the home directory for the file
 **   file_name. If it finds it, it reads the values of the
 **   font, number of rows and the line style and sets them
 **   as default.
 **
 **   write_setup() writes the above values back to this file.
 **
 **   Since both function lock the setup file, there is no
 **   problem when two users with the same home directory read
 **   or write that file.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.06.1997 H.Kiehl Created
 **   10.09.2000 H.Kiehl Added history log for mon_ctrl dialog.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                  /* strcpy(), strcat()               */
#include <stdlib.h>                  /* getenv()                         */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "x_common_defs.h"

/* External global variables */
extern int    no_of_rows_set;
extern char   font_name[],
              line_style,
              user[];
extern Widget appshell;

/* Local global variables */
static char setup_file[MAX_PATH_LENGTH] = { 0 };


/*############################## read_setup() ###########################*/
void
read_setup(char *file_name,
           int  *filename_display_length,
           int  *his_log_set)
{
   int         fd;
   char        *ptr,
               *buffer = NULL;
   struct stat stat_buf;

   if (setup_file[0] == '\0')
   {
      if ((ptr = getenv("HOME")) != NULL)
      {
         char hostname[MAX_AFD_NAME_LENGTH];

         (void)strcpy(setup_file, ptr);
         ptr = user;
         while ((*ptr != '@') && (*ptr != '\0'))
         {
            ptr++;
         }
         (void)strcat(setup_file, "/.");
         (void)strcat(setup_file, file_name);
         if ((*ptr == '@') && (*(ptr + 1) != '\0') && (*(ptr + 1) != ':'))
         {
            (void)strcat(setup_file, ".setup.");
            (void)strcat(setup_file, (ptr + 1));
         }
         else
         {
            (void)strcat(setup_file, ".setup");
         }
         if (get_afd_name(hostname) != INCORRECT)
         {
            (void)strcat(setup_file, ".");
            (void)strcat(setup_file, hostname);
         }
      }
      else
      {
         return;
      }
   }

   if (stat(setup_file, &stat_buf) == -1)
   {
      if (errno != ENOENT)
      {
         setup_file[0] = '\0';
      }
      return;
   }

   if ((fd = lock_file(setup_file, ON)) < 0)
   {
      setup_file[0] = '\0';
      return;
   }

   if ((buffer = malloc(stat_buf.st_size)) == NULL)
   {
      (void)close(fd);
      return;
   }
   if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
   {
      free(buffer);
      (void)close(fd);
      return;
   }
   (void)close(fd); /* This will release the lock as well. */

   /* Get the default font. */
   if ((ptr = posi(buffer, FONT_ID)) != NULL)
   {
      int i = 0;

      ptr--;
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      while ((*ptr != '\n') && (*ptr != '\0'))
      {
         font_name[i] = *ptr;
         i++; ptr++;
      }
      font_name[i] = '\0';
   }

   /* Get the number of rows. */
   if ((ptr = posi(buffer, ROW_ID)) != NULL)
   {
      int  i = 0;
      char tmp_buffer[MAX_INT_LENGTH + 1];

      ptr--;
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      while ((*ptr != '\n') && (*ptr != '\0') && (i < MAX_INT_LENGTH))
      {
         tmp_buffer[i] = *ptr;
         i++; ptr++;
      }
      tmp_buffer[i] = '\0';
      no_of_rows_set = atoi(tmp_buffer);
   }

   /* Get the line style. */
   if ((ptr = posi(buffer, STYLE_ID)) != NULL)
   {
      int  i = 0;
      char tmp_buffer[MAX_INT_LENGTH + 1];

      ptr--;
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      while ((*ptr != '\n') && (*ptr != '\0') && (i < MAX_INT_LENGTH))
      {
         tmp_buffer[i] = *ptr;
         i++; ptr++;
      }
      tmp_buffer[i] = '\0';
      line_style = atoi(tmp_buffer);
      if ((line_style != CHARACTERS_AND_BARS) &&
          (line_style != CHARACTERS_ONLY) &&
          (line_style != BARS_ONLY))
      {
         line_style = CHARACTERS_AND_BARS;
      }
   }

   /* Get the filename display length. */
   if (filename_display_length != NULL)
   {
      if ((ptr = posi(buffer, FILENAME_DISPLAY_LENGTH_ID)) != NULL)
      {
         int  i = 0;
         char tmp_buffer[MAX_INT_LENGTH + 1];

         ptr--;
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while ((*ptr != '\n') && (*ptr != '\0') && (i < MAX_INT_LENGTH))
         {
            tmp_buffer[i] = *ptr;
            i++; ptr++;
         }
         tmp_buffer[i] = '\0';
         *filename_display_length = atoi(tmp_buffer);
         if (*filename_display_length > MAX_FILENAME_LENGTH)
         {
            *filename_display_length = MAX_FILENAME_LENGTH;
         }
      }
      else
      {
         *filename_display_length = DEFAULT_FILENAME_DISPLAY_LENGTH;
      }
   }

   /* Get the number of history log entries. */
   if (his_log_set != NULL)
   {
      if ((ptr = posi(buffer, NO_OF_HISTORY_LENGTH_ID)) != NULL)
      {
         int  i = 0;
         char tmp_buffer[MAX_INT_LENGTH + 1];

         ptr--;
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while ((*ptr != '\n') && (*ptr != '\0') && (i < MAX_INT_LENGTH))
         {
            tmp_buffer[i] = *ptr;
            i++; ptr++;
         }
         tmp_buffer[i] = '\0';
         *his_log_set = atoi(tmp_buffer);
         if (*his_log_set > MAX_LOG_HISTORY)
         {
            *his_log_set = MAX_LOG_HISTORY;
         }
      }
      else
      {
         *his_log_set = DEFAULT_NO_OF_HISTORY_LOGS;
      }
   }

   if (buffer != NULL)
   {
      free(buffer);
   }

   return;
}


/*############################# write_setup() ###########################*/
void
write_setup(int filename_display_length, int his_log_set)
{
   int         fd = -1,
               length;
   char        buffer[100];
   struct stat stat_buf;
   
   if (setup_file[0] == '\0')
   {
      /*
       * Since we have failed to find the users home directory,
       * there is no need to continue.
       */
      return;
   }

   if (stat(setup_file, &stat_buf) == -1)
   {
      if (errno == ENOENT)
      {
         if ((fd = open(setup_file, (O_WRONLY | O_CREAT | O_TRUNC),
                        (S_IRUSR | S_IWUSR))) < 0)
         {
            (void)xrec(appshell, ERROR_DIALOG,
                       "Failed to open() setup file %s : %s (%s %d)",
                       setup_file, strerror(errno),
                       __FILE__, __LINE__);
            return;
         }
      }
   }

   if (fd == -1)
   {
      if ((fd = lock_file(setup_file, ON)) < 0)
      {
         return;
      }
      length = sprintf(buffer, "%s %s\n%s %d\n%s %d\n",
                       FONT_ID, font_name, ROW_ID, no_of_rows_set,
                       STYLE_ID, line_style);
      if (filename_display_length != -1)
      {
         length += sprintf(&buffer[length], "%s %d\n",
                           FILENAME_DISPLAY_LENGTH_ID, filename_display_length);
      }
      if (his_log_set != -1)
      {
         length += sprintf(&buffer[length], "%s %d\n",
                           NO_OF_HISTORY_LENGTH_ID, his_log_set);
      }
      if (write(fd, buffer, length) != length)
      {
         (void)xrec(appshell, ERROR_DIALOG,
                    "Failed to write to setup file %s : %s (%s %d)",
                    setup_file, strerror(errno),
                    __FILE__, __LINE__);
      }
      (void)close(fd);
   }
   else
   {
      int lock_fd;

      if ((lock_fd = lock_file(setup_file, ON)) < 0)
      {
         (void)close(fd);
         return;
      }
      length = sprintf(buffer, "%s %s\n%s %d\n%s %d\n",
                       FONT_ID, font_name, ROW_ID, no_of_rows_set,
                       STYLE_ID, line_style);
      if (filename_display_length != -1)
      {
         length += sprintf(&buffer[length], "%s %d\n",
                           FILENAME_DISPLAY_LENGTH_ID, filename_display_length);
      }
      if (his_log_set != -1)
      {
         length += sprintf(&buffer[length], "%s %d\n",
                           NO_OF_HISTORY_LENGTH_ID, his_log_set);
      }
      if (write(fd, buffer, length) != length)
      {
         (void)xrec(appshell, ERROR_DIALOG,
                    "Failed to write to setup file %s : %s (%s %d)",
                    setup_file, strerror(errno),
                    __FILE__, __LINE__);
      }
      (void)close(fd);
      (void)close(lock_fd);
   }

   return;
}

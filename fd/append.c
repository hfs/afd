/*
 *  append.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   append - functions for appending files in FTP
 **
 ** SYNOPSIS
 **   void log_append(int job_id, char *file_name)
 **   void remove_append(int job_id, char *file_name)
 **   void remove_all_appends(int job_id)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None. If we fail to log the fail name, all that happens the next
 **   time is that the complete file gets send again.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.01.1998 H.Kiehl Created
 **   21.09.1998 H.Kiehl Added function remove_all_appends()
 **
 */
DESCR__E_M3

#include <stdio.h>                /* sprintf()                           */
#include <string.h>               /* strcmp(), strerror()                */
#include <unistd.h>               /* close(), ftruncate()                */
#include <stdlib.h>               /* malloc(), free()                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables */
extern char *p_work_dir;


/*############################ log_append() #############################*/
void
log_append(int job_id, char *file_name)
{
   int         fd;
   size_t      buf_size;
   char        *buffer,
               *ptr,
               msg[MAX_PATH_LENGTH];
   struct stat stat_buf;

   (void)sprintf(msg, "%s%s/%d", p_work_dir, AFD_MSG_DIR, job_id);

   if ((fd = lock_file(msg, ON)) < 0)
   {
      return;
   }
   if (fstat(fd, &stat_buf) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to stat() message %s : %s", msg, strerror(errno));
      (void)close(fd);
      return;
   }
   buf_size = stat_buf.st_size + strlen(OPTION_IDENTIFIER) +
              RESTART_FILE_ID_LENGTH + strlen(file_name) + 4;
   if ((buffer = malloc(buf_size)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      (void)close(fd);
      return;
   }
   if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to read() message %s : %s", msg, strerror(errno));
      free(buffer);
      (void)close(fd);
      return;
   }
   buffer[stat_buf.st_size] = '\0';

   /* First determine if there is an option identifier. */
   if ((ptr = posi(buffer, OPTION_IDENTIFIER)) == NULL)
   {
      /* Add the option and restart identifier. */
      ptr = buffer + stat_buf.st_size;
      ptr += sprintf(ptr, "\n%s\n%s", OPTION_IDENTIFIER, RESTART_FILE_ID);
   }
   else
   {
      char *tmp_ptr;

      /* Check if the append option is already in the message. */
      if ((tmp_ptr = posi(ptr, RESTART_FILE_ID)) != NULL)
      {
         char *end_ptr,
              tmp_char;

         while (*tmp_ptr == ' ')
         {
            tmp_ptr++;
         }

         /* Now check if the file name is already in the list. */
         do
         {
            end_ptr = tmp_ptr;
            while ((*end_ptr != ' ') && (*end_ptr != '\n'))
            {
               end_ptr++;
            }
            tmp_char = *end_ptr;
            *end_ptr = '\0';
            if (strcmp(tmp_ptr, file_name) == 0)
            {
               /* File name is already in message. */
               free(buffer);
               if (close(fd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "close() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               }
               return;
            }
            *end_ptr = tmp_char;
            tmp_ptr = end_ptr;
            while (*tmp_ptr == ' ')
            {
               tmp_ptr++;
            }
         } while (*tmp_ptr != '\n');

         ptr = tmp_ptr;
      }
      else
      {
         ptr = buffer + stat_buf.st_size;
         ptr += sprintf(ptr, "%s", RESTART_FILE_ID);
      }
   }

   /* Append the file name. */
   *(ptr++) = ' ';
   ptr += sprintf(ptr, "%s\n", file_name);
   *ptr = '\0';
   buf_size = strlen(buffer);

   if (lseek(fd, 0, SEEK_SET) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to lseek() %s : %s", msg, strerror(errno));
   }
   else
   {
      if (write(fd, buffer, buf_size) != buf_size)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to write() to %s : %s", msg, strerror(errno));
      }
      else
      {
         if (stat_buf.st_size > buf_size)
         {
            if (ftruncate(fd, buf_size) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to ftruncate() %s : %s",
                          msg, strerror(errno));
            }
         }
      }
   }
   free(buffer);
   if (close(fd) < 0)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to close() %s : %s", msg, strerror(errno));
   }

   return;
}


/*########################## remove_append() ############################*/
void
remove_append(int job_id, char *file_name)
{
   int         fd;
   size_t      length;
   char        *buffer,
               *ptr,
               *tmp_ptr,
               msg[MAX_PATH_LENGTH];
   struct stat stat_buf;

   (void)sprintf(msg, "%s%s/%d", p_work_dir, AFD_MSG_DIR, job_id);

   if ((fd = lock_file(msg, ON)) < 0)
   {
      return;
   }
   if (fstat(fd, &stat_buf) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to stat() message %s : %s", msg, strerror(errno));
      (void)close(fd);
      return;
   }
   if ((buffer = malloc(stat_buf.st_size)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      (void)close(fd);
      return;
   }
   if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to read() message %s : %s", msg, strerror(errno));
      free(buffer);
      (void)close(fd);
      return;
   }
   buffer[stat_buf.st_size] = '\0';

   if ((ptr = posi(buffer, RESTART_FILE_ID)) == NULL)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Failed to locate <%s> identifier in message %s.",
                 RESTART_FILE_ID, msg);
      free(buffer);
      (void)close(fd);
      return;
   }

   /* Locate the file name */
   for (;;)
   {
      if ((tmp_ptr = posi(ptr, file_name)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to locate %s in restart option of message %s.",
                    file_name, msg);
         free(buffer);
         (void)close(fd);
         return;
      }
      if ((*(tmp_ptr - 1) == ' ') || (*(tmp_ptr - 1) == '\n') ||
          (*(tmp_ptr - 1) == '\0'))
      {
         break;
      }
      else
      {
         ptr = tmp_ptr;
      }
   }
   tmp_ptr--;

   length = strlen(file_name);
   if (((tmp_ptr - length) == ptr) && (*tmp_ptr == '\n'))
   {
      /* This is the only file name, so lets remove the option. */
      while (*ptr != '\n')
      {
         ptr--;     /* Go to the beginning of the option. */
      }
      *(ptr + 1) = '\0';
   }
   else
   {
      if (*tmp_ptr == '\n')
      {
         ptr = tmp_ptr - length - 1 /* to remove the space sign */;
         *ptr = '\n';
         *(ptr + 1) = '\0';
      }
      else
      {
         int n = strlen((tmp_ptr + 1));

         (void)memmove((tmp_ptr - length), (tmp_ptr + 1), n);
         ptr = tmp_ptr - length + n;
         *ptr = '\0'; /* to remove any junk after the part we just moved */
      }
   }
   length = strlen(buffer);

   if (lseek(fd, 0, SEEK_SET) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to lseek() %s : %s", msg, strerror(errno));
   }
   else
   {
      if (write(fd, buffer, length) != length)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to write() to %s : %s", msg, strerror(errno));
      }
      else
      {
         if (stat_buf.st_size > length)
         {
            if (ftruncate(fd, length) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to ftruncate() %s : %s",
                          msg, strerror(errno));
            }
         }
      }
   }
   free(buffer);
   if (close(fd) < 0)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to close() %s : %s", msg, strerror(errno));
   }

   return;
}


/*######################## remove_all_appends() #########################*/
void
remove_all_appends(int job_id)
{
   int         fd;
   size_t      length;
   char        *buffer,
               *ptr,
               msg[MAX_PATH_LENGTH];
   struct stat stat_buf;

   (void)sprintf(msg, "%s%s/%d", p_work_dir, AFD_MSG_DIR, job_id);

   if ((fd = lock_file(msg, ON)) < 0)
   {
      return;
   }
   if (fstat(fd, &stat_buf) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to stat() message %s : %s", msg, strerror(errno));
      (void)close(fd);
      return;
   }
   if ((buffer = malloc(stat_buf.st_size)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      (void)close(fd);
      return;
   }
   if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to read() message %s : %s", msg, strerror(errno));
      free(buffer);
      (void)close(fd);
      return;
   }
   buffer[stat_buf.st_size] = '\0';

   if ((ptr = posi(buffer, RESTART_FILE_ID)) == NULL)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Hmmm. <%s> identifier is already gone in message %d.",
                 RESTART_FILE_ID, job_id);
      free(buffer);
      (void)close(fd);
      return;
   }

   /* Lets remove the restart option. */
   while (*ptr != '\n')
   {
      ptr--;     /* Go to the beginning of the option. */
   }
   *(ptr + 1) = '\0';
   length = strlen(buffer);

   if (lseek(fd, 0, SEEK_SET) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to lseek() %s : %s", msg, strerror(errno));
   }
   else
   {
      if (write(fd, buffer, length) != length)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to write() to %s : %s", msg, strerror(errno));
      }
      else
      {
         if (stat_buf.st_size > length)
         {
            if (ftruncate(fd, length) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to ftruncate() %s : %s",
                          msg, strerror(errno));
            }
         }
      }
   }
   free(buffer);
   if (close(fd) < 0)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to close() %s : %s", msg, strerror(errno));
   }
   system_log(DEBUG_SIGN, __FILE__, __LINE__,
              "Hmm. Removed all append options for JID %d.", job_id);

   return;
}

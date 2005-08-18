/*
 *  create_message.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   create_message - creates a message that is used by the sf_xxx
 **                    process
 **
 ** SYNOPSIS
 **   int create_message(unsigned int job_id,
 **                      char         *recipient,
 **                      char         *options)
 **
 ** DESCRIPTION
 **   This function creates a message in th AFD message directory.
 **   The name of the message is the job ID. From the contents of
 **   this message the sf_xxx process no where to send the files
 **   and with what options.
 **
 ** RETURN VALUES
 **   SUCCESS when it managed to create the message, otherwise
 **   INCORRECT will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.01.1998 H.Kiehl Created
 **   09.08.1998 H.Kiehl Return SUCCESS when we managed to create the
 **                      message.
 **   11.05.2003 H.Kiehl Support for storing the passwords in a separate
 **                      file.
 **
 */
DESCR__E_M3

#include <stdio.h>           /* sprintf()                                */
#include <string.h>          /* strlen(), strerror()                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>          /* write(), close()                         */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>           /* open()                                   */
#endif
#include <errno.h>

/* External global variables */
extern char msg_dir[],
            *p_msg_dir;


/*########################### create_message() ##########################*/
int
create_message(unsigned int job_id, char *recipient, char *options)
{
   int  fd,
        length,
        ret = SUCCESS;
   char buffer[MAX_RECIPIENT_LENGTH + 3];
#ifdef WITH_PASSWD_IN_MSG
   char password[MAX_USER_NAME_LENGTH],
        *p_rest,
        *ptr;

   ptr = recipient;
   if (((*ptr == 'f') && (*(ptr + 1) == 'i') && (*(ptr + 2) == 'i') &&
        (*(ptr + 3) == 'l') && (*(ptr + 4) == 'e') && (*(ptr + 5) == ':')) ||
       ((*ptr == 'm') && (*(ptr + 1) == 'a') && (*(ptr + 2) == 'i') &&
        (*(ptr + 3) == 'l') && (*(ptr + 4) == 't') && (*(ptr + 5) == 'o') &&
        (*(ptr + 6) == ':')))
   {
      /* No need to store any password for scheme file and mailto. */
      password[0] = '\0';
   }
   else
   {
      length = 1;
      buffer[0] = '\n';
      while ((*ptr != ':') && (*ptr != '\0'))
      {
         buffer[length] = *ptr;
         ptr++; length++;
      }
      if ((*ptr == ':') && (*(ptr + 1) == '/') && (*(ptr + 2) == '/'))
      {
         ptr += 3; /* Away with '://' */
         buffer[length] = ':';
         buffer[length + 1] = buffer[length + 2] = '/';
         length += 3;
         if ((*ptr != MAIL_GROUP_IDENTIFIER) && (*ptr != '@') &&
             (*ptr != '\0'))
         {
            char uh_name[MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH + 1];

            fd = 0;
            while ((*ptr != ':') && (*ptr != '@') && (*ptr != '\0') &&
                   (fd < MAX_USER_NAME_LENGTH))
            {
               if (*ptr == '\\')
               {
                  buffer[length] = *ptr;
                  ptr++; length++;
               }
               uh_name[fd] = *ptr;
               buffer[length] = *ptr;
               ptr++; fd++; length++;
            }
            if (fd > 0)
            {
               if (fd == MAX_USER_NAME_LENGTH)
               {
                  while ((*ptr != ':') && (*ptr != '@') && (*ptr != '\0'))
                  {
                     if (*ptr == '\\')
                     {
                        buffer[length] = *ptr;
                        ptr++; length++;
                     }
                     buffer[length] = *ptr;
                     ptr++; length++;
                  }
               }
               if (*ptr == '@')
               {
                  int uh_name_length = fd;

                  fd = 0;
                  p_rest = ptr;
                  ptr++; /* Away with the '@' */
                  while ((*ptr != '\0') && (*ptr != '/') &&
                         (*ptr != ':') && (*ptr != ';') &&
                         (fd < MAX_REAL_HOSTNAME_LENGTH))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     uh_name[uh_name_length + fd] = *ptr;
                     ptr++; fd++;
                  }
                  uh_name[uh_name_length + fd] = '\0';
                  (void)get_pw(uh_name, password);
               }
               else
               {
                  password[0] = '\0';
               }
            }
            else
            {
               password[0] = '\0';
            }
         }
         else
         {
            password[0] = '\0';
         }
      }
      else
      {
         password[0] = '\0';
      }
   }
#endif

   (void)sprintf(p_msg_dir, "%x", job_id);
   if ((fd = open(msg_dir, O_RDWR | O_CREAT | O_TRUNC,
#ifdef GROUP_CAN_WRITE
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) != -1)
#else
                  S_IRUSR | S_IWUSR)) != -1)
#endif
   {
      if (write(fd, DESTINATION_IDENTIFIER, DESTINATION_IDENTIFIER_LENGTH) == DESTINATION_IDENTIFIER_LENGTH)
      {
#ifdef WITH_PASSWD_IN_MSG
         if (password[0] != '\0')
         {
            buffer[length] = ':';
            length++;
            length += sprintf(&buffer[length], "%s", password);
            length += sprintf(&buffer[length], "%s\n\n", p_rest);
         }
         else
         {
            length = sprintf(buffer, "\n%s\n\n", recipient);
         }
#else
         length = sprintf(buffer, "\n%s\n\n", recipient);
#endif
         if (write(fd, buffer, length) == length)
         {
            if (options != NULL)
            {
               length = sprintf(buffer, "%s\n", OPTION_IDENTIFIER);
               if (write(fd, buffer, length) == length)
               {
                  length = strlen(options);
                  if (write(fd, options, length) == length)
                  {
                     /* Don't forget the newline, options does not have it. */
                     if (write(fd, "\n", 1) != 1)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Failed to write to `%s' : %s",
                                   msg_dir, strerror(errno));
                        ret = INCORRECT;
                     }
                  }
                  else
                  {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                "Failed to write to `%s' : %s",
                                msg_dir, strerror(errno));
                     ret = INCORRECT;
                  }
               }
               else
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "Failed to write to `%s' : %s",
                             msg_dir, strerror(errno));
                  ret = INCORRECT;
               }
            }
         }
         else
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to write to `%s' : %s",
                       msg_dir, strerror(errno));
            ret = INCORRECT;
         }
      }
      else
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to write to `%s' : %s", msg_dir, strerror(errno));
         ret = INCORRECT;
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
   }
   else
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s", msg_dir, strerror(errno));
      ret = INCORRECT;
   }

   return(ret);
}

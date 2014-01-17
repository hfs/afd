/*
 *  create_message.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2013 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   This function creates a message in the AFD message directory.
 **   The name of the message is the job ID. From the contents of
 **   this message the sf_xxx process know where to send the files
 **   and what options to use.
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
# include <fcntl.h>          /* open()                                   */
#endif
#include <errno.h>

/* External global variables. */
extern char msg_dir[],
            *p_msg_dir;


/*########################### create_message() ##########################*/
int
create_message(unsigned int job_id, char *recipient, char *options)
{
   int          fd,
                ret = SUCCESS;
   char         buffer[1 + MAX_RECIPIENT_LENGTH + 2 + 1];
#ifdef EXPAND_PATH_IN_MESSAGE
   char         *p_recipient;
#endif
#if defined WITH_PASSWD_IN_MSG || defined EXPAND_PATH_IN_MESSAGE
   char         new_recipient[MAX_RECIPIENT_LENGTH + 1];
#endif

#ifdef WITH_PASSWD_IN_MSG
   (void)strcpy(new_recipient, recipient);
   url_insert_password(new_recipient, NULL);
#endif
#ifdef EXPAND_PATH_IN_MESSAGE
   /* LOC_SHEME : file */
   if ((recipient[0] == 'f') && (recipient[1] == 'i') &&
       (recipient[2] == 'l') && (recipient[3] == 'e') &&
       (recipient[4] == ':'))
   {
      char *p_directory,
           user[MAX_USER_NAME_LENGTH + 1];

# ifndef WITH_PASSWD_IN_MSG
      (void)strcpy(new_recipient, recipient);
# endif
      p_recipient = new_recipient;
      if ((url_evaluate(new_recipient, NULL, user, NULL, NULL,
# ifdef WITH_SSH_FINGERPRINT
                        NULL, NULL,
# endif
                        NULL, NO, NULL, NULL, NULL, &p_directory,
                        NULL, NULL, NULL, NULL) == 0) &&
          (*p_directory != '/'))
      {
         if (expand_path(user, p_directory) == INCORRECT)
         {
            return(INCORRECT);
         }
      }
   }
   else
   {
# ifdef WITH_PASSWD_IN_MSG
      p_recipient = new_recipient;
# else
      p_recipient = recipient;
# endif
   }
#endif

#ifdef HAVE_SNPRINTF
   (void)snprintf(p_msg_dir, MAX_PATH_LENGTH - (p_msg_dir - msg_dir), "%x",
#else
   (void)sprintf(p_msg_dir, "%x",
#endif
                 job_id);
   if ((fd = open(msg_dir, O_RDWR | O_CREAT | O_TRUNC,
#ifdef GROUP_CAN_WRITE
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) != -1)
#else
                  S_IRUSR | S_IWUSR)) != -1)
#endif
   {
      if (write(fd, DESTINATION_IDENTIFIER, DESTINATION_IDENTIFIER_LENGTH) == DESTINATION_IDENTIFIER_LENGTH)
      {
         int length;

#ifdef EXPAND_PATH_IN_MESSAGE
# ifdef HAVE_SNPRINTF
         length = snprintf(buffer, 1 + MAX_RECIPIENT_LENGTH + 2 + 1, "\n%s\n\n", p_recipient);
# else
         length = sprintf(buffer, "\n%s\n\n", p_recipient);
# endif
#else
# ifdef WITH_PASSWD_IN_MSG
#  ifdef HAVE_SNPRINTF
         length = snprintf(buffer, 1 + MAX_RECIPIENT_LENGTH + 2 + 1, "\n%s\n\n", new_recipient);
#  else
         length = sprintf(buffer, "\n%s\n\n", new_recipient);
#  endif
# else
#  ifdef HAVE_SNPRINTF
         length = snprintf(buffer, 1 + MAX_RECIPIENT_LENGTH + 2 + 1, "\n%s\n\n", recipient);
#  else
         length = sprintf(buffer, "\n%s\n\n", recipient);
#  endif
# endif
#endif
         if (write(fd, buffer, length) == length)
         {
            if (options != NULL)
            {
#ifdef HAVE_SNPRINTF
               length = snprintf(buffer, 1 + MAX_RECIPIENT_LENGTH + 2 + 1,
#else
               length = sprintf(buffer,
#endif
                                "%s\n", OPTION_IDENTIFIER);
               if (write(fd, buffer, length) == length)
               {
                  length = strlen(options);
                  if (write(fd, options, length) == length)
                  {
                     /* Don't forget the newline, options does not have it. */
                     if (write(fd, "\n", 1) != 1)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   _("Failed to write to `%s' : %s"),
                                   msg_dir, strerror(errno));
                        ret = INCORRECT;
                     }
                  }
                  else
                  {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                _("Failed to write to `%s' : %s"),
                                msg_dir, strerror(errno));
                     ret = INCORRECT;
                  }
               }
               else
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             _("Failed to write to `%s' : %s"),
                             msg_dir, strerror(errno));
                  ret = INCORRECT;
               }
            }
         }
         else
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("Failed to write to `%s' : %s"),
                       msg_dir, strerror(errno));
            ret = INCORRECT;
         }
      }
      else
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Failed to write to `%s' : %s"),
                    msg_dir, strerror(errno));
         ret = INCORRECT;
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("close() error : %s"), strerror(errno));
      }
   }
   else
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Failed to open() `%s' : %s"), msg_dir, strerror(errno));
      ret = INCORRECT;
   }

   return(ret);
}

/*
 *  create_remote_dir.c - Part of AFD, an automatic file distribution
 *                        program.
 *  Copyright (c) 2000 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   create_remote_dir - creates a directory name from a url
 **
 ** SYNOPSIS
 **   int create_remote_dir(char *url, char *remote_dir)
 **
 ** DESCRIPTION
 **   This function creates a directory name from a url (remote_dir)
 **   of the following format:
 **
 **   $AFD_WORK_DIR/files/incoming/<user>@<hostname>/[<user>/]<remote dir>
 **
 **   When the remote directory is an absolute path the second <user>
 **   will NOT be inserted.
 **
 ** RETURN VALUES
 **   When remote_dir has the correct format it will return SUCCESS
 **   and the new directory name will be returned by overwritting
 **   the variable remote_dir. On error INCORRECT will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   13.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>           /* sprintf()                                */

/* External global variables. */
extern char *p_work_dir;


/*######################### create_remote_dir() #########################*/
int
create_remote_dir(char *url, char *remote_dir)
{
   int  absolute_path = NO,
        i;
   char directory[MAX_RECIPIENT_LENGTH],
        host_alias[MAX_HOSTNAME_LENGTH + 1],
        *ptr = url,
        user[MAX_USER_NAME_LENGTH];

   i = 0;
   while ((*ptr != ':') && (*ptr != '\0') && (i < 10))
   {
      i++; ptr++;
   }
   if (*ptr != '\0')
   {
      /* Away with '://' */
      ptr += 3;

      /* Save user name. */
      i = 0;
      while ((*ptr != ':') && (*ptr != '@') && (*ptr != '\0') &&
             (i < MAX_USER_NAME_LENGTH))
      {
         user[i] = *ptr;
         i++; ptr++;
      }
      if (*ptr != '\0')
      {
         user[i] = '\0';

         /* Away with the password. */
         while ((*ptr != '@') && (*ptr != '\0'))
         {
            ptr++;
         }
         if (*ptr == '@')
         {
            ptr++;

            /* Store the host_alias. */
            i = 0;
            while ((*ptr != '/') && (i < (MAX_HOSTNAME_LENGTH + 1)) &&
                   (*ptr != ':') && (*ptr != ';') && (*ptr != '\0'))
            {
               host_alias[i] = *ptr;
               i++; ptr++;
            }
            if (i > 0)
            {
               host_alias[i] = '\0';

               /* Possibly there could be a port, away with it. */
               while ((*ptr != '/') && (*ptr != '\0'))
               {
                  ptr++;
               }
               if (*ptr == '/')
               {
                  if (*(ptr + 1) == '/')
                  {
                     absolute_path = YES;
                     ptr++;
                  }

                  /* Store the remote directory. */
                  i = 0;
                  while ((*ptr != '\0') && (*ptr != ';') &&
                         (i < MAX_RECIPIENT_LENGTH))
                  {
                     directory[i] = *ptr;
                     i++; ptr++;
                  }
                  directory[i] = '\0';
               }
               else
               {
                  directory[0] = '\0';
               }
            }
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Unable to locate host name in URL <%s>", url);
            return(INCORRECT);
         }
      }
      else
      {
         if (i == 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Unable to locate user name in URL <%s>", url);
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Unable to locate host name in URL <%s>", url);
         }
         return(INCORRECT);
      }
   }
   else
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "There is no more data after the scheme of the URL <%s>", url);
      return(INCORRECT);
   }
   if (absolute_path == NO)
   {
      if (directory[0] == '\0')
      {
         (void)sprintf(remote_dir, "%s%s%s/%s@%s", p_work_dir,
                       AFD_FILE_DIR, INCOMING_DIR, user, host_alias);
      }
      else
      {
         (void)sprintf(remote_dir, "%s%s%s/%s@%s%s", p_work_dir, AFD_FILE_DIR,
                       INCOMING_DIR, user, host_alias, directory);
      }
   }
   else
   {
      if (directory[0] == '\0')
      {
         (void)sprintf(remote_dir, "%s%s%s/%s@%s/%s", p_work_dir, AFD_FILE_DIR,
                       INCOMING_DIR, user, host_alias, user);
      }
      else
      {
         (void)sprintf(remote_dir, "%s%s%s/%s@%s/%s%s", p_work_dir,
                       AFD_FILE_DIR, INCOMING_DIR, user, host_alias, user,
                       directory);
      }
   }

   return(SUCCESS);
}

/*
 *  get_user.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_user - gets contents of environment variables LOGNAME and
 **              DISPLAY
 **
 ** SYNOPSIS
 **   void get_user(char *user)
 **
 ** DESCRIPTION
 **   Gets the username and display of the current process. This
 **   is done by reading te environment variables 'LOGNAME' and
 **   'DISPLAY' respectivly. If not specified 'unknown' will be
 **   used for either 'LOGNAME' or 'DISPLAY'.
 **
 ** RETURN VALUES
 **   Will return the username and display in 'user'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.04.1996 H.Kiehl Created
 **   18.08.2003 H.Kiehl Build in length checks.
 **
 */
DESCR__E_M3

#include <stdio.h>              /* NULL                                  */
#include <string.h>             /* strcat()                              */
#include <stdlib.h>             /* getenv()                              */
#include <sys/types.h>
#include <pwd.h>                /* getpwuid()                            */
#include <unistd.h>             /* getuid()                              */


/*############################ get_user() ###############################*/
void
get_user(char *user)
{
   size_t        length;
   char          *ptr;
   struct passwd *pwd;

   if ((pwd = getpwuid(getuid())) != NULL)
   {
      length = strlen(pwd->pw_name);
      if (length > 0)
      {
         if ((length + 1) < MAX_FULL_USER_ID_LENGTH)
         {
            (void)strcpy(user, pwd->pw_name);
            (void)strcat(user, "@");
            length++;
         }
         else
         {
            memcpy(user, pwd->pw_name, MAX_FULL_USER_ID_LENGTH - 1);
            user[MAX_FULL_USER_ID_LENGTH - 1] = '\0';
            length = MAX_FULL_USER_ID_LENGTH;
         }
      }
      else
      {
         length = 0;
         user[0] = '\0';
      }
   }
   else
   {
      if (8 < MAX_FULL_USER_ID_LENGTH)
      {
         (void)strcpy(user, "unknown@");
         length = 8;
      }
      else
      {
         length = 0;
         user[0] = '\0';
      }
   }

   if (length < MAX_FULL_USER_ID_LENGTH)
   {
      if ((ptr = getenv("DISPLAY")) != NULL)
      {
         size_t display_length;

         display_length = strlen(ptr);
         if ((length + display_length) < MAX_FULL_USER_ID_LENGTH)
         {
            (void)strcpy(&user[length], ptr);
         }
      }
      else
      {
         if ((length + 7) < MAX_FULL_USER_ID_LENGTH)
         {
            (void)strcpy(&user[length], "unknown");
         }
      }
   }

   return;
}

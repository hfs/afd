/*
 *  get_user.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **
 */
DESCR__E_M3

#include <stdio.h>              /* NULL                                  */
#include <string.h>             /* strcat()                              */
#include <stdlib.h>             /* getenv()                              */
#ifdef _WITH_UID_CHECK
#include <sys/types.h>
#include <pwd.h>                /* getpwuid()                            */
#include <unistd.h>             /* getuid()                              */
#endif


/*############################ get_user() ###############################*/
void
get_user(char *user)
{
#ifdef _WITH_UID_CHECK
   struct passwd *pwd;
#endif
   char          *ptr = NULL;

   user[0] = '\0';
#ifdef _WITH_UID_CHECK
   if ((pwd = getpwuid(getuid())) != NULL)
   {
      (void)strcpy(user, pwd->pw_name);
      (void)strcat(user, "@");
   }
#else
   if ((ptr = getenv("LOGNAME")) != NULL)
   {
      (void)strcat(user, ptr);
      (void)strcat(user, "@");
   }
#endif
   else
   {
      (void)strcat(user, "unknown");
      (void)strcat(user, "@");
   }

   if ((ptr = getenv("DISPLAY")) != NULL)
   {
      (void)strcat(user, ptr);
   }
   else
   {
      (void)strcat(user, "unknown");
   }

   return;
}

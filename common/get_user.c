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
 **   is done by reading the environment variables 'LOGNAME' and
 **   'DISPLAY' respectivly. If not specified 'unknown' will be
 **   used for either 'LOGNAME' or 'DISPLAY'. When 'DISPLAY' is
 **   set to 'localhost' it will try to get the remote host by
 **   looking at the output of 'who am i'.
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
 **   20.10.2003 H.Kiehl If DISPLAY is localhost (ssh) get real hostname
 **                      with 'who am i'.
 **   29.10.2003 H.Kiehl Try to evaluate SSH_CLIENT as well when
 **                      DISPLAY is localhost.
 **
 */
DESCR__E_M3

#include <stdio.h>              /* NULL                                  */
#include <string.h>             /* strcat(), strlen(), memcpy()          */
#include <stdlib.h>             /* getenv(), malloc(), free()            */
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
         if (8 < MAX_FULL_USER_ID_LENGTH)
         {
            (void)strcpy(user, "unknown@");
            length = 8;
         }
         else
         {
            user[0] = '\0';
            length = 0;
         }
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
         user[0] = '\0';
         length = 0;
      }
   }

   if (length < MAX_FULL_USER_ID_LENGTH)
   {
      if ((ptr = getenv("DISPLAY")) != NULL)
      {
         size_t display_length;

         display_length = strlen(ptr);
         if ((display_length >= 9) && (*ptr == 'l') && (*(ptr + 1) == 'o') &&
             (*(ptr + 2) == 'c') && (*(ptr + 3) == 'a') &&
             (*(ptr + 4) == 'l') && (*(ptr + 5) == 'h') &&
             (*(ptr + 6) == 'o') && (*(ptr + 7) == 's') &&
             (*(ptr + 8) == 't'))
         {
            if ((ptr = getenv("SSH_CLIENT")) != NULL)
            {
               char *search_ptr = ptr;

               while ((*search_ptr != ' ') && (*search_ptr != '\0') &&
                      (*search_ptr != '\t'))
               {
                  search_ptr++;
               }
               display_length = search_ptr - ptr;
               if ((length + display_length) >= MAX_FULL_USER_ID_LENGTH)
               {
                  display_length = MAX_FULL_USER_ID_LENGTH - length;
               }
               (void)memcpy(&user[length], ptr, display_length);
               user[length + display_length] = '\0';
               return;
            }
            else
            {
               char *buffer;

               if ((buffer = malloc(MAX_PATH_LENGTH + MAX_PATH_LENGTH)) != NULL)
               {
                  int ret;

                  if ((ret = exec_cmd("who am i", buffer)) != INCORRECT)
                  {
                     char *search_ptr = buffer;

                     while ((*search_ptr != '(') && (*search_ptr != '\0') &&
                            ((search_ptr - buffer) < (MAX_PATH_LENGTH + MAX_PATH_LENGTH)))
                     {
                        search_ptr++;
                     }
                     if (*search_ptr == '(')
                     {
                        char *start_ptr = search_ptr + 1;

                        search_ptr++;
                        while ((*search_ptr != ')') && (*search_ptr != '\0') &&
                               ((search_ptr - buffer) < (MAX_PATH_LENGTH + MAX_PATH_LENGTH)))
                        {
                           search_ptr++;
                        }
                        if (*search_ptr == ')')
                        {
                           int real_display_length = search_ptr - start_ptr;

                           if ((length + real_display_length) < MAX_FULL_USER_ID_LENGTH)
                           {
                              (void)memcpy(&user[length], start_ptr,
                                           real_display_length);
                              user[length + real_display_length] = '\0';
                              free(buffer);
                              return;
                           }
                        }
                     }
                  }
                  free(buffer);
                  ptr = getenv("DISPLAY");
               }
            }
         }
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

/*
 *  get_hostname.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Deutscher Wetterdienst (DWD),
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
 **   get_hostname - extracts hostname from recipient string
 **
 ** SYNOPSIS
 **   char *get_hostname(char *recipient)
 **
 ** DESCRIPTION
 **   This function returns the hostname from the recipient string
 **   'recipient'. This string has the following format:
 **
 **        <sheme>://<user>:<password>@<host>:<port>/<url-path>
 **
 ** RETURN VALUES
 **   Returns NULL if it is not able to extract the hostname.
 **   Otherwise it will return the hostname.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.02.1996 H.Kiehl Created
 **   15.12.1996 H.Kiehl Switched to URL format.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include "amgdefs.h"


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ get_hostname() $$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
char *
get_hostname(char *recipient)
{
   char        *ptr;
   static char hostname[MAX_REAL_HOSTNAME_LENGTH + 1];

   ptr = recipient;
   while ((*ptr != '\0') && (*ptr != '@'))
   {
      if (*ptr == '\\')
      {
         ptr++;
      }
      ptr++;
   }

   if (*ptr == '@')
   {
      int i = 0;

      /* Store the hostname */
      ptr++;
      while ((*ptr != '\0') &&
             (*ptr != '/') &&
             (*ptr != ':') &&
             (*ptr != ';') &&
             (i < MAX_REAL_HOSTNAME_LENGTH))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         hostname[i] = *ptr;
         i++; ptr++;
      }
      if ((i > 0) && (i < MAX_REAL_HOSTNAME_LENGTH))
      {
         hostname[i] = '\0';
         return(hostname);
      }
   }

   return(NULL);
}

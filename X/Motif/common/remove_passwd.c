/*
 *  remove_passwd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   remove_passwd - removes the passwd from the given url
 **
 ** SYNOPSIS
 **   void remove_passwd(char *url)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   If the url contains a passwd it gets replaced with XXXXX.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.09.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>     /* strcat()                                      */
#include "x_common_defs.h"


/*############################ remove_passwd() ##########################*/
void
remove_passwd(char *url)
{
   char *ptr = url;

   while (*ptr != ':')
   {
      ptr++;
   }
   ptr++;
   while ((*ptr != ':') && (*ptr != '@') && (*ptr != '\0'))
   {
      if (*ptr == '\\')
      {
         ptr++;
      }
      ptr++;
   }
   if (*ptr == ':')
   {
      char *p_end = ptr + 1,
           tmp_buffer[MAX_RECIPIENT_LENGTH];

      ptr++;
      while (*ptr != '@')
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         ptr++;
      }
      (void)strcpy(tmp_buffer, ptr);
      *p_end = '\0';
      (void)strcat(url, "XXXXX");
      (void)strcat(url, tmp_buffer);
   }

   return;
}

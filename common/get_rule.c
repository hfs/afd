/*
 *  get_rule.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2002 Deutscher Wetterdienst (DWD),
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
 **   get_rule - locates a given rule and returns its position
 **
 ** SYNOPSIS
 **   int get_rule(int fd, off_t offset)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.10.1997 H.Kiehl Created
 **   10.12.2002 H.Kiehl Catch the case that someone has empty characters
 **                      behind the rule identifier. This is required for
 **                      the new overwrite option.
 **
 */
DESCR__E_M3

#include <string.h>                     /* strcmp(), strlen()            */
#include <stdlib.h>                     /* malloc(), free()              */
#include <errno.h>

/* External global variables */
extern struct rule *rule;


/*############################ get_rule() ###############################*/
int
get_rule(char *wanted_rule, int no_of_rule_headers)
{
   size_t length;

   if ((length = strlen(wanted_rule)) < 1025)
   {
      char *rule_buffer;

      if ((rule_buffer = malloc(length + 1)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
      }
      else
      {
         int  i = 0;
         char *ptr = wanted_rule;

         while ((*ptr != '\0') && (*ptr != ' ') &&
                (*ptr != '\t') && (i != length))
         {
            rule_buffer[i] = *ptr;
            i++; ptr++;
         }
         rule_buffer[i] = '\0';

         for (i = 0; i < no_of_rule_headers; i++)
         {
            if (CHECK_STRCMP(rule[i].header, rule_buffer) == 0)
            {
               free(rule_buffer);
               return(i);
            }
         }
         free(rule_buffer);
      }
   }
   else
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Refuse to allocate %d of memory for a rule identifier, limit is 1024",
                 length);
   }

   return(INCORRECT);
}

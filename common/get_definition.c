/*
 *  get_definition.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_definition - extracts a definition from a buffer
 **
 ** SYNOPSIS
 **   char *get_definition(char *buffer,
 **                        char *search_value,
 **                        char *definition,
 **                        int  max_definition_length)
 **
 ** DESCRIPTION
 **   Extracts a definition from the AFD_CONFIG file and stores its
 **   value in the buffer 'definition'.
 **
 ** RETURN VALUES
 **   Returns a pointer positioned after the value just found.
 **   Otherwise NULL is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.09.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* External global variables */
extern int sys_log_fd;


/*########################### get_definition() ##########################*/
char *
get_definition(char *buffer,
               char *search_value,
               char *definition,
               int  max_definition_length)
{
   int  i = 0;
   char *ptr = buffer,
        *real_search_value;

   if ((real_search_value = malloc(strlen(search_value) + 2)) == NULL)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      return(NULL);
   }
   real_search_value[0] = '\n';
   (void)strcpy(&real_search_value[1], search_value);

   if ((ptr = posi(buffer, real_search_value)) == NULL)
   {
      free(real_search_value);
      return(NULL);
   }
   free(real_search_value);

   while ((*ptr == ' ') || (*ptr == '\t'))
   {
      ptr++;
   }
   if (*ptr == '"')
   {
      ptr++;
      while ((*ptr != '\n') && (*ptr != '\0') && (*ptr != '"') &&
             (i < max_definition_length))
      {
         definition[i] = *ptr;
         ptr++; i++;
      }
   }
   else
   {
      while ((*ptr != '\n') && (*ptr != '\0') &&
             (*ptr != ' ') && (*ptr != '\t') &&
             (i < max_definition_length))
      {
         definition[i] = *ptr;
         ptr++; i++;
      }
   }
   if (i >= max_definition_length)
   {
      return(NULL);
   }
   definition[i] = '\0';

   return(ptr);
}

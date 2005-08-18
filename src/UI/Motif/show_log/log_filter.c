/*
 *  log_filter.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   log_filter - checks string if it matches a certain pattern specified
 **                with wild cards
 **
 ** SYNOPSIS
 **   int log_filter(char *p_filter, char *p_file)
 **
 ** DESCRIPTION
 **   The function log_filter() checks if 'p_file' matches 'p_filter'.
 **   'p_filter' may have the wild cards '*' and '?' anywhere and
 **   in any order. Where '*' matches any string and '?' matches
 **   any single character.
 **
 ** RETURN VALUES
 **   Returns 0 when pattern matches, else it will return -1.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.04.1997 H.Kiehl Created
 **   01.12.2003 H.Kiehl Make it work for monitor log file types.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                    /* strncmp()                      */
#include "show_log.h"
#include "x_common_defs.h"

/* local functions */
static char *find(char *, register char *, register int);


/*############################# log_filter() ############################*/
int
log_filter(char *p_filter, char *p_file)
{
   register int  length = 0,
                 count = 0,
                 inverse = NO;
   register char *ptr = p_filter,
                 *p_tmp = NULL,
                 *p_buffer = NULL,
                 *p_file_buffer = p_file,
                 buffer;

   if (*ptr == '!')
   {
      ptr++;
      inverse = YES;
   }

   while (*ptr != '\0')
   {
      length = 0;
      p_tmp = ptr;
      switch(*ptr)
      {
         case '*' :
            ptr++;
            while ((*ptr != '*') && (*ptr != '?') && (*ptr != '\0'))
            {
               length++;
               ptr++;
            }
            if (length == 0)
            {
               if ((*ptr == '*') || (*ptr == '?'))
               {
                  break;
               }
               else
               {
                  return((inverse == NO) ? 0 : -1);
               }
            }
            buffer = *ptr;
            if (*ptr != '\0')
            {
               *ptr = '\0';
            }
            if ((buffer == '?') && (*(ptr + 1) == '\0'))
            {
               count = 0;
               while ((p_file_buffer = find(p_file_buffer, p_tmp + 1, length)) != NULL)
               {
                  p_buffer = p_file_buffer;
                  count++;
               }
               if (count == 0)
               {
                  *ptr = buffer;
                  return((inverse == NO) ? -1 : 0);
               }
               else
               {
                  p_file_buffer = p_buffer;
               }
               *ptr = buffer;
            }
            else if ((p_file_buffer = find(p_file_buffer, p_tmp + 1, length)) == NULL)
                 {
                    *ptr = buffer;
                    return((inverse == NO) ? -1 : 0);
                 }
                 else
                 {
                    *ptr = buffer;
                    if ((buffer == '\0') && (*p_file != '\0'))
                    {
                       ptr = p_tmp;
                    }
                 }
            if ((buffer == '\0') && (*p_file_buffer == ' '))
            {
               return((inverse == NO) ? 0 : -1);
            }
            break;

         case '?' :
            if ((*p_file_buffer == ' ') || (*p_file_buffer == '['))
            {
               return((inverse == NO) ? -1 : 0);
            }
            p_file_buffer++;
            if ((*(++ptr) == '\0') &&
                ((*p_file_buffer == ' ') || (*p_file_buffer == '[')))
            {
               return((inverse == NO) ? 0 : -1);
            }
            break;

         default  :
            while ((*ptr != '*') && (*ptr != '?') && (*ptr != '\0'))
            {
               length++;
               ptr++;
            }
            buffer = *ptr;
            *ptr = '\0';
            if (strncmp(p_file_buffer, p_tmp, length) != 0)
            {
               *ptr = buffer;
               return((inverse == NO) ? -1 : 0);
            }
            *ptr = buffer;
            p_file_buffer += length;
            if ((buffer == '\0') &&
                ((*p_file_buffer == ' ') || (*p_file_buffer == '[') ||
                 (*p_file_buffer == ':')))
            {
               return((inverse == NO) ? 0 : -1);
            }
            break;
      }
   }

   return(-1);
}


/*++++++++++++++++++++++++++++++++ find() +++++++++++++++++++++++++++++++*/
/*
 * Description  : Searches in "search_text" for "search_string". If
 *                found, it returns the address of the last character
 *                in "search_string".
 * Input values : char *search_text
 *                char *search_string
 *                int  string_length
 * Return value : char *search_text when it finds search_text otherwise
 *                NULL is returned.
 */
static char *
find(char          *search_text,
     register char *search_string,
     register int  string_length)
{
   register int hit = 0;

   while ((*search_text != ' ') && (*search_text != '['))
   {
      if (*(search_text++) == *(search_string++))
      {
         if (++hit == string_length)
         {
            return(search_text);
         }
      }
      else
      {
         search_string -= (hit + 1);
         if (hit != 0)
         {
            search_text -= hit;
            hit = 0;
         }
      }
   }

   return(NULL);
}

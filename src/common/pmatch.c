/*
 *  pmatch.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   pmatch - checks string if it matches a certain pattern specified
 **            with wild cards
 **
 ** SYNOPSIS
 **   int pmatch(char *p_filter, char *p_file)
 **
 ** DESCRIPTION
 **   The function pmatch() checks if 'p_file' matches 'p_filter'.
 **   'p_filter' may have the wild cards '*' and '?' anywhere and
 **   in any order. Where '*' matches any string and '?' matches
 **   any single character.
 **
 ** RETURN VALUES
 **   Returns 0 when pattern matches, if this file is definitly
 **   not wanted 1 and otherwise -1.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.10.1995 H.Kiehl  Created
 **   18.10.1997 H.Kiehl  Introduced inverse filtering.
 **   03.02.2000 H.Lepper Fix the case "*xxx*??" sdsfxxxbb
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                    /* strncmp()                      */

/* local functions */
static char *find(char *, register char *, register int);


/*################################ pmatch() #############################*/
int
pmatch(char *p_filter, char *p_file)
{
   register int  length;
   register char *p_gap_file = NULL,
                 *p_gap_filter,
                 *ptr = p_filter,
                 *p_tmp = NULL,
                 buffer;

   if (*ptr == '!')
   {
      ptr++;
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
               p_gap_filter = ptr;
               while (*ptr == '*')
               {
                  ptr++;
               }
               if (*ptr != '\0')
               {
                  ptr = p_gap_filter;
               }
               if ((*ptr == '*') || (*ptr == '?'))
               {
                  p_gap_file = p_file + 1;
                  p_gap_filter = p_tmp;
                  break;
               }
               else
               {
                  return((*p_filter != '!') ? 0 : 1);
               }
            }
            buffer = *ptr;
            if ((p_file = find(p_file, p_tmp + 1, length)) == NULL)
            {
               return(-1);
            }
            else
            {
               if (*ptr == '?')
               {
                  if (length > 1 )
                  {
                     p_gap_file = p_file - length + 1;
                  }
                  else
                  {
                     p_gap_file = p_file + 1;
                  }
                  p_gap_filter = p_tmp;
               }
               if ((*ptr == '\0') && (*p_file != '\0'))
               {
                  ptr = p_tmp;
               }
            }
            if ((buffer == '\0') && (*p_file == '\0'))
            {
               return((*p_filter != '!') ? 0 : 1);
            }
            break;

         case '?' :
            if (*(p_file++) == '\0')
            {
               return(-1);
            }
            if ((*(++ptr) == '\0') && (*p_file == '\0'))
            {
               return((*p_filter != '!') ? 0 : 1);
            }
            if ((*ptr == '\0') && (p_gap_file != NULL))
            {
               p_file = p_gap_file;
               ptr = p_gap_filter;
            }
            break;

         default  :
            while ((*ptr != '*') && (*ptr != '?') && (*ptr != '\0'))
            {
               length++;
               ptr++;
            }
            if (strncmp(p_file, p_tmp, length) != 0)
            {
               if (p_gap_file != NULL)
               {
                  p_file = p_gap_file;
                  ptr = p_gap_filter;
                  break;
               }
               return(-1);
            }
            p_file += length;
            if ((*ptr == '\0') && (*p_file == '\0'))
            {
               return((*p_filter != '!') ? 0 : 1);
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

   while (*search_text != '\0')
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
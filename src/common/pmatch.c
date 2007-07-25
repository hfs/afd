/*
 *  pmatch.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   int pmatch(char const *p_filter, char const *p_file, time_t *pmatch_time)
 **
 ** DESCRIPTION
 **   The function pmatch() checks if 'p_file' matches 'p_filter'.
 **   'p_filter' may have the wild cards '*' and '?' anywhere and
 **   in any order. Where '*' matches any string and '?' matches
 **   any single character. It also may list characters enclosed in
 **   []. A pair of characters separated by a hyphen denotes a range
 **   expression; any character that sorts between those two characters
 **   is matched. If the first character following the [ is a ! then
 **   any character not enclosed is matched.
 **
 **   The code was taken from wildmatch() by Karl Heuer.
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
 **   29.03.2006 H.Kiehl  Added support for expanding filters.
 **   01.04.2007 H.Kiehl  Fix the case "r10013*00" r10013301000.
 **   29.05.2007 H.Kiehl  Fix the case "*7????" abcd77abcd.
 **   30.05.2007 H.Kiehl  Replaced code with wildmatch() code from
 **                       Karl Heuer.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>                    /* malloc()                       */
#include <ctype.h>                     /* isdigit()                      */
#include <time.h>                      /* time(), strftime()             */
#include <unistd.h>                    /* gethostname()                  */


/* Local variables. */
static char *tmp_filter = NULL;

/* Local function prototypes. */
static int  pmatch2(char const *, char const *, time_t *);
static void expand_filter(char *, time_t);


/*################################ pmatch() #############################*/
int
pmatch(char const *p_filter, char const *p_file, time_t *pmatch_time)
{
   int ret;

   ret = pmatch2((*p_filter == '!') ? p_filter + 1 : p_filter, p_file,
                 pmatch_time);
   if ((ret == 0) && (*p_filter == '!'))
   {
      ret = 1;
   }

   return(ret);
}


/*++++++++++++++++++++++++++++++ pmatch2() ++++++++++++++++++++++++++++++*/
static int
pmatch2(char const *p, char const *s, time_t *pmatch_time)
{
   register char c;

   while ((c = *p++) != '\0')
   {
      if (c == '*')
      {
         if (*p == '\0')
         {
            return(0); /* optimize common case */
         }
         do
         {
            if (pmatch2(p, s, pmatch_time) == 0)
            {
               return(0);
            }
         } while (*s++ != '\0');
         return(-1);
      }
      else if (c == '?')
           {
               if (*s++ == '\0')
               {
                  return(-1);
               }
           }
      else if (c == '[')
           {
              register int wantit;
              register int seenit = NO;

              if (*p == '!')
              {
                 wantit = NO;
                 ++p;
              }
              else
              {
                 wantit = YES;
              }
              c = *p++;
              do
              {
                 if (c == '\0')
                 {
                    return(-1);
                 }
                 if ((*p == '-') && (p[1] != '\0'))
                 {
                    if ((*s >= c) && (*s <= p[1]))
                    {
                       seenit = YES;
                    }
                    p += 2;
                 }
                 else
                 {
                    if (c == *s)
                    {
                       seenit = YES;
                    }
                 }
              } while ((c = *p++) != ']');

              if (wantit != seenit)
              {
                 return(-1);
              }
              ++s;
           }
      else if (c == '\\')
           {
              if ((*p == '\0') || (*p++ != *s++))
              {
                 return(-1);
              }
           }
      else if ((c == '%') && ((*p == 't') || (*p == 'T') || (*p == 'h')))
           {
              expand_filter((char *)(p - 1),
                            (pmatch_time == NULL) ? time(NULL) : *pmatch_time);
              if (tmp_filter == NULL)
              {
                 /* We failed to allocate memory, lets ignore expanding. */
                 if (c != *s++)
                 {
                    return(-1);
                 }
              }
              else
              {
                 p = tmp_filter;
              }
           }
           else
           {
              if (c != *s++)
              {
                 return(-1);
              }
           }
   }

   return((*s == '\0') ? 0 : -1);
}


/*--------------------------- expand_filter() ---------------------------*/
static void
expand_filter(char *orig_filter, time_t check_time)
{
   if ((tmp_filter != NULL) ||
       ((tmp_filter = malloc(MAX_FILENAME_LENGTH)) != NULL))
   {
      time_t time_modifier = 0;
      char   *rptr,
             time_mod_sign = '+',
             *wptr;

      rptr = orig_filter;
      wptr = tmp_filter;
      while ((*rptr != '\0') && (wptr < (tmp_filter + MAX_FILENAME_LENGTH - 1)))
      {
         if ((*rptr == '%') && ((*(rptr + 1) == 't') || (*(rptr + 1) == 'T') ||
             (*(rptr + 1) == 'h')) &&
             ((rptr == orig_filter) || ((*(rptr - 1) != '\\'))))
         {
            if (*(rptr + 1) == 't')
            {
               time_t time_buf;

               time_buf = check_time;

               if (time_modifier > 0)
               {
                  switch (time_mod_sign)
                  {
                     case '-' :
                        time_buf = time_buf - time_modifier;
                        break;
                     case '*' :
                     time_buf = time_buf * time_modifier;
                        break;
                     case '/' :
                        time_buf = time_buf / time_modifier;
                        break;
                     case '%' :
                        time_buf = time_buf % time_modifier;
                        break;
                     case '+' :
                     default :
                        time_buf = time_buf + time_modifier;
                        break;
                  }
               }
               switch (*(rptr + 2))
               {
                  case 'a': /* short day of the week 'Tue' */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%a", localtime(&time_buf));
                     break;
                  case 'b': /* short month 'Jan' */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%b", localtime(&time_buf));
                     break;
                  case 'j': /* day of year [001,366] */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%j", localtime(&time_buf));
                     break;
                  case 'd': /* day of month [01,31] */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%d", localtime(&time_buf));
                     break;
                  case 'M': /* minute [00,59] */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%M", localtime(&time_buf));
                     break;
                  case 'm': /* month [01,12] */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%m", localtime(&time_buf));
                     break;
                  case 'y': /* year 2 chars [01,99] */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%y", localtime(&time_buf));
                     break;
                  case 'H': /* hour [00,23] */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%H", localtime(&time_buf));
                     break;
                  case 'S': /* second [00,59] */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%S", localtime(&time_buf));
                     break;
                  case 'Y': /* year 4 chars 2002 */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%Y", localtime(&time_buf));
                     break;
                  case 'A': /* long day of the week 'Tuesday' */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%A", localtime(&time_buf));
                     break;
                  case 'B': /* month 'January' */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%B", localtime(&time_buf));
                     break;
                  case 'U': /* Unix time. */
#if SIZEOF_TIME_T == 4
                     wptr += sprintf(wptr, "%ld", (pri_time_t)time_buf);
#else
                     wptr += sprintf(wptr, "%lld", (pri_time_t)time_buf);
#endif
                     break;
                  default :
                     *wptr = '%';
                     *(wptr + 1) = 't';
                     *(wptr + 2) = *(rptr + 2);
                     wptr += 3;
                     break;
               }
               rptr += 3;
            }
            else if (*(rptr + 1) == 'T')
                 {
                    int  m,
                         time_unit;
                    char string[MAX_INT_LENGTH + 1];

                    rptr += 2;
                    switch (*rptr)
                    {
                       case '+' :
                       case '-' :
                       case '*' :
                       case '/' :
                       case '%' :
                          time_mod_sign = *rptr;
                          rptr++;
                          break;
                       default  :
                          time_mod_sign = '+';
                          break;
                    }
                    m = 0;
                    while ((isdigit((int)(*rptr))) && (m < MAX_INT_LENGTH))
                    {
                       string[m++] = *rptr++;
                    }
                    if ((m > 0) && (m < MAX_INT_LENGTH))
                    {
                       string[m] = '\0';
                       time_modifier = atoi(string);
                    }
                    else
                    {
                       if (m == MAX_INT_LENGTH)
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "The time modifier specified in the filter %s is to long.",
                                     orig_filter);
                       }
                       else
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "There is no time modifier specified in filter %s",
                                     orig_filter);
                       }
                       time_modifier = 0;
                    }
                    switch (*rptr)
                    {
                       case 'S' : /* Second */
                          time_unit = 1;
                          rptr++;
                          break;
                       case 'M' : /* Minute */
                          time_unit = 60;
                          rptr++;
                          break;
                       case 'H' : /* Hour */
                          time_unit = 3600;
                          rptr++;
                          break;
                       case 'd' : /* Day */
                          time_unit = 86400;
                          rptr++;
                          break;
                       default :
                          time_unit = 1;
                          break;
                    }
                    if (time_modifier > 0)
                    {
                       time_modifier = time_modifier * time_unit;
                    }
                 }
                 else /* It must be the hostname modifier. */
                 {
#ifdef HAVE_GETHOSTNAME
                    char hostname[40];

                    if (gethostname(hostname, 40) == -1)
                    {
#endif
                       char *p_hostname;

                       if ((p_hostname = getenv("HOSTNAME")) != NULL)
                       {
                          wptr += sprintf(wptr, "%s", p_hostname);
                       }
                       else
                       {
                          *wptr = '%';
                          *(wptr + 1) = 'h';
                          wptr += 2;
                       }
#ifdef HAVE_GETHOSTNAME
                    }
                    else
                    {
                       wptr += sprintf(wptr, "%s", hostname);
                    }
#endif
                    rptr += 2;
                 }
         }
         else
         {
            *wptr = *rptr;
            wptr++; rptr++;
         }
      }
      *wptr = '\0';
   }
   return;
}

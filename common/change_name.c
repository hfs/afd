/*
 *  change_name.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2001 Deutscher Wetterdienst (DWD),
 *                            Tobias Freyberg <>
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
 **   change_name - changes a file name according a given rule
 **
 ** SYNOPSIS
 **   void change_name(char *orig_file_name,
 **                    char *filter,
 **                    char *rename_to_rule,
 **                    char *new_name)
 **
 ** DESCRIPTION
 **   'change_name' takes the 'filter' and analyses the 'orig_file_name'
 **   to chop them in to pieces. Then the pieces are glued together as
 **   described in 'rename_to_rule' and stored in to 'new_name'. To
 **   insert special terms in the new filename the '%' sign is
 **   interpreted as followed:
 **            %*n   - address n-th asterisk
 **            %?n   - address n-th question mark
 **            %on   - n-th character from the original filename
 **            %On-m - range n to m of characters from original file
 **                    name. n='^' - from the beginning
 **                          m='$' - to the end
 **            %n    - generate a 4 character unique number
 **            %tx   - insert the actual time (x = like strftime)
 **                    (a - short day "Tue",      A - long day "Tuesday",
 **                     b - short month "Jan",    B - long month "January",
 **                     d - day of month [01,31], m - month [01,12],
 **                     j - day of the year [001,366],
 **                     y - year [01,99],         Y - year 1997,
 **                     H - hour [00,23],         M - minute [00,59],
 **                     S - second [00,60]                                 )
 **            %h    - insert the hostname
 **            %%    - the '%' sign   
 **            \     - are ignored
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   T.Freyberg
 **
 ** HISTORY
 **   03.02.1997 T.Freyberg Created
 **   15.11.1997 H.Kiehl    Insert option day of the year.
 **   07.03.2001 H.Kiehl    Put in some more syntax checks.
 **   09.10.2001 H.Kiehl    Insert option for inserting hostname.
 **
 */
DESCR__E_M3

#include <stdio.h>           /*                                          */
#include <string.h>          /* strcpy(), strlen()                       */
#include <stdlib.h>          /* atoi(), getenv()                         */
#include <ctype.h>           /* isdigit()                                */
#include <unistd.h>          /* gethostname()                            */
#include <errno.h>
#include <time.h>
#include "amgdefs.h"


/*############################# change_name() ###########################*/
void
change_name(char *orig_file_name,
            char *filter,
            char *rename_to_rule,
            char *new_name)
{
   char   buffer[MAX_FILENAME_LENGTH],
          string[MAX_INT_LENGTH + 1],
          *ptr_asterix[10] =
          {
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
          },
          *ptr_questioner[50] =
          {
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
          },
          *ptr_oldname,
          *ptr_oldname_tmp,
          *ptr_filter,
          *ptr_filter_tmp,
          *ptr_rule = NULL,
          *ptr_newname = NULL;
   int    count_asterix = 0,
          count_questioner = 0,
          act_asterix = 0,
          act_questioner = 0,
          i,
          number,
          start = 0,
          end,
          pattern_found = NO;
   time_t time_buf;

   /* copy original filename to a temporary buffer */
   (void)strcpy(buffer, orig_file_name);

   ptr_oldname = ptr_oldname_tmp = buffer;
   ptr_filter  = ptr_filter_tmp  = filter;

   /* taking 'orig_file_name' in pieces like 'filter' */
   while (*ptr_filter != '\0')
   {
      switch (*ptr_filter)
      {
         case '*' : /* '*' in filter detected -> mark position in array */

            ptr_asterix[count_asterix++] = ptr_oldname;
            ptr_filter++;
            break;

         case '?' : /* '?' in filter -> skip one character in both words */
                    /*                  and mark questioner              */

            ptr_filter++;
            ptr_questioner[count_questioner++] = ptr_oldname++;
            break;

         default  : /* search the char, found in filter, in oldname */

            pattern_found = NO;

            /* mark the latest position */
            ptr_filter_tmp = ptr_filter;
            while (*ptr_filter_tmp != '\0')
            {
               /* position the orig_file_name pointer to the next      */
               /* place where filter pointer and orig_file_name pointer*/
               /* are equal                                            */
               while ((*ptr_oldname != *ptr_filter_tmp) &&
                      (*ptr_oldname != '\0'))
               {
                  ptr_oldname++;
               }
               /* mark the latest position */
               ptr_oldname_tmp = ptr_oldname;

               /* test the rest of the pattern */
               while ((*ptr_filter_tmp != '*') && (*ptr_filter_tmp != '\0'))
               {
                  if ((*ptr_filter_tmp == *ptr_oldname_tmp) ||
                      (*ptr_filter_tmp == '?'))
                  {
                     if (*ptr_filter_tmp == '?')
                     {
                        /* mark questioner */
                        ptr_questioner[count_questioner++] = ptr_oldname_tmp;
                     }
                     ptr_filter_tmp++;
                     ptr_oldname_tmp++;
                     pattern_found = YES;
                  }
                  else
                  {
                     pattern_found = NO;
                     break;
                  }
               }
               if (pattern_found == YES)
               {
                  /* mark end of string 'ptr_asterix[count_asterix]' */
                  /* and position the pointer to next char           */
                  *ptr_oldname = '\0';
                  ptr_oldname = ptr_oldname_tmp;

                  ptr_filter = ptr_filter_tmp;
                  break;
               }
               else
               {
                  ptr_filter_tmp = ptr_filter;
                  ptr_oldname++;
                  if (count_questioner > 0)
                  {
                     count_questioner = 0;
                  }
               }
            }
            break;
      }
   }

#ifdef _DEBUG
   system_log(INFO_SIGN, NULL, 0, "found %d *", count_asterix);
   for (i = 0; i < count_asterix; i++)
   {
      system_log(INFO_SIGN, NULL, 0, "ptr_asterix[%d]=%s", i, ptr_asterix[i]);
   }
   system_log(INFO_SIGN, NULL, 0, "found %d ?", count_questioner);
   for (i = 0; i < count_questioner; i++)
   {
      system_log(INFO_SIGN, NULL, 0, "ptr_questioner[%d]=%c",
                 i, *ptr_questioner[i]);
   }
#endif

   /* Create new_name as specified in rename_to_rule */
   ptr_rule = rename_to_rule;
   ptr_newname = new_name;
   while (*ptr_rule != '\0')
   {
      /* work trough the rule and paste the name */
      switch (*ptr_rule)
      {
         case '%' : /* locate the '%' -> handle the rule */
            ptr_rule++;
            switch (*ptr_rule)
            {
               case '*' : /* insert the addressed ptr_asterix */

                  ptr_rule++;

                  /* test for number */
                  i = 0;
                  while ((isdigit(*ptr_rule)) && (i < MAX_INT_LENGTH))
                  {
                     string[i++] = *ptr_rule++;
                  }
                  string[i] = '\0';
                  number = atoi(string) - 1; /* human count from 1 and computer from 0 */
                  if (number >= count_asterix)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "illegal '*' addressed: %d", number + 1);
                  }
                  else
                  {
                     (void)strcpy(ptr_newname, ptr_asterix[number]);
                     ptr_newname += strlen(ptr_asterix[number]);
                  }
                  break;

               case '?' : /* insert the addressed ptr_questioner */

                  ptr_rule++;

                  /* test for a number */
                  i = 0;
                  while ((isdigit(*ptr_rule)) && (i < MAX_INT_LENGTH))
                  {
                     string[i++] = *ptr_rule++;
                  }
                  string[i] = '\0';
                  number = atoi(string) - 1; /* human count from 1 and computer from 0 */
                  if (number >= count_questioner)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "illegal '?' addressed: %d", number + 1);

                  }
                  else
                  {
                     *ptr_newname = *ptr_questioner[number];
                     ptr_newname++;
                  }
                  break;

               case 'o' : /* insert the addressed character from orig_file_name */

                  ptr_rule++;

                  /* test for a number */
                  i = 0;
                  while ((isdigit(*ptr_rule)) && (i < MAX_INT_LENGTH))
                  {
                     string[i++] = *ptr_rule++;
                  }
                  string[i] = '\0';
                  number = atoi(string) - 1; /* human count from 1 and computer from 0 */
                  *ptr_newname++ = *(orig_file_name + number);
                  break;

               case 'O' : /* insert the addressed range of characters from orig_file_name */

                  ptr_rule++;

                  /* read the begin */
                  if (*ptr_rule == '^') /* means start from the beginning */
                  {
                     ptr_oldname = orig_file_name;
                     ptr_rule++;
                  }
                  else /* read the start number */
                  {
                     i = 0;
                     while ((isdigit(*ptr_rule)) && (i < MAX_INT_LENGTH))
                     {
                        string[i++] = *ptr_rule++;
                     }
                     string[i] = '\0';
                     start = atoi(string) - 1; /* human count from 1 and computer from 0 */
                     ptr_oldname = orig_file_name + start;
                  }
                  if (*ptr_rule == '-')
                  {
                     /* skip the '-' */
                     ptr_rule++;
                     /* read the end */
                     if (*ptr_rule == '$') /* means up to the end */
                     {
                        (void)strcpy(ptr_newname, ptr_oldname);
                        ptr_newname += strlen(ptr_oldname);
                        ptr_rule++;
                     }
                     else
                     {
                        i = 0;
                        while ((isdigit(*ptr_rule)) && (i < MAX_INT_LENGTH))
                        {
                           string[i++] = *ptr_rule++;
                        }
                        string[i] = '\0';
                        end = atoi(string) - 1; /* human count from 1 and computer from 0 */
                        number = end - start + 1;  /* including the first and last */
                        if (number < 0)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "The start (%d) and end (%d) range do not make sense in rule %s. Start must be larger then end!",
                                      rename_to_rule);
                        }
                        else
                        {
                           strncpy(ptr_newname, ptr_oldname, number);
                           ptr_newname += number;
                        }
                     }
                  }
                  else
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "There is no end range specified for rule %s",
                                rename_to_rule);
                  }
                  break;

               case 'n' : /* generate a unique number 4 characters */

                  ptr_rule++;
                  next_counter(&number);
                  (void)sprintf(ptr_newname, "%04d", number);
                  ptr_newname += 4;
                  break;

               case 'h' : /* Insert local hostname. */

                  {
                     char hostname[40];

                     ptr_rule++;
                     if (gethostname(hostname, 40) == -1)
                     {
                        char *p_hostname;

                        if ((p_hostname = getenv("HOSTNAME")) != NULL)
                        {
                           ptr_newname += sprintf(ptr_newname, "%s", p_hostname);
                        }
                     }
                     else
                     {
                        ptr_newname += sprintf(ptr_newname, "%s", hostname);
                     }
                  }
                  break;

               case 't' : /* insert the actual time like strftime */

                  time_buf = time(NULL);
                  ptr_rule++;
                  switch (*ptr_rule)
                  {
                     case 'a' : /* short day of the week 'Tue' */
                        number = strftime (ptr_newname, MAX_FILENAME_LENGTH,
                                           "%a", gmtime(&time_buf));
                        break;
                     case 'A' : /* long day of the week 'Tuesday' */
                        number = strftime (ptr_newname, MAX_FILENAME_LENGTH,
                                           "%A", gmtime(&time_buf));
                        break;
                     case 'b' : /* short month 'Jan' */
                        number = strftime (ptr_newname, MAX_FILENAME_LENGTH,
                                           "%b", gmtime(&time_buf));
                        break;
                     case 'B' : /* month 'January' */
                        number = strftime (ptr_newname, MAX_FILENAME_LENGTH,
                                           "%B", gmtime(&time_buf));
                        break;
                     case 'd' : /* day of month [01,31] */
                        number = strftime (ptr_newname, MAX_FILENAME_LENGTH,
                                           "%d", gmtime(&time_buf));
                        break;
                     case 'j' : /* day of year [001,366] */
                        number = strftime (ptr_newname, MAX_FILENAME_LENGTH,
                                           "%j", gmtime(&time_buf));
                        break;
                     case 'm' : /* month [01,12] */
                        number = strftime (ptr_newname, MAX_FILENAME_LENGTH,
                                           "%m", gmtime(&time_buf));
                        break;
                     case 'y' : /* year 2char [01,99] */
                        number = strftime (ptr_newname, MAX_FILENAME_LENGTH,
                                           "%y", gmtime(&time_buf));
                        break;
                     case 'Y' : /* year 4char 1997 */
                        number = strftime (ptr_newname, MAX_FILENAME_LENGTH,
                                           "%Y", gmtime(&time_buf));
                        break;
                     case 'H' : /* hour [00,23] */
                        number = strftime (ptr_newname, MAX_FILENAME_LENGTH,
                                           "%H", gmtime(&time_buf));
                        break;
                     case 'M' : /* minute [00,59] */
                        number = strftime (ptr_newname, MAX_FILENAME_LENGTH,
                                           "%M", gmtime(&time_buf));
                        break;
                     case 'S' : /* minute [00,59] */
                        number = strftime (ptr_newname, MAX_FILENAME_LENGTH,
                                           "%S", gmtime(&time_buf));
                        break;
                     default  : /* unknown character - ignore */
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Illegal time option (%c) in rule %s",
                                   *ptr_rule, rename_to_rule);
                        number = 0;
                        break;
                  }
                  ptr_newname += number;
                  ptr_rule++;
                  break;

               case '%' : /* insert the '%' sign */

                  *ptr_newname = '%';
                  ptr_newname++;
                  ptr_rule++;
                  break;

               default  : /* no action be specified, write an error message */

                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Illegal option (%d) in rule %s",
                             *ptr_rule, rename_to_rule);
                  ptr_rule++;
                  break;
            }
            break;
            
         case '*' : /* locate the '*' -> insert the next 'ptr_asterix' */
            if (act_asterix == count_asterix)
            {
               system_log(WARN_SIGN, NULL, 0,
                          "can not paste more '*' as defined before -> ignored");
               system_log(WARN_SIGN, NULL, 0,
                          "orig_file_name = %s | filter = %s | rename_to_rule = %s | new_name = %s",
                          orig_file_name, filter, rename_to_rule, new_name);
            }
            else
            {
               (void)strcpy(ptr_newname, ptr_asterix[act_asterix]);
               ptr_newname += strlen(ptr_asterix[act_asterix]);
               act_asterix++;
            }
            ptr_rule++;
            break;

         case '?' : /* locate the '?' -> insert the next 'ptr_questioner' */
            if (act_questioner == count_questioner)
            {
               system_log(WARN_SIGN, NULL, 0,
                          "can not paste more '?' as defined before -> ignored");
               system_log(WARN_SIGN, NULL, 0,
                          "orig_file_name = %s | filter = %s | rename_to_rule = %s | new_name = %s",
                          orig_file_name, filter, rename_to_rule, new_name);
            }
            else
            {
               *ptr_newname = *ptr_questioner[act_questioner];
               ptr_newname++;
               act_questioner++;
            }
            ptr_rule++;
            break;

         case '\\' : /* ignore */

            ptr_rule++;
            break;

         default  : /* found an ordinary character -> append it */
            *ptr_newname = *ptr_rule;
            ptr_newname++;
            ptr_rule++;
            break;
      }
   }
   *ptr_newname = '\0';

   return;
}

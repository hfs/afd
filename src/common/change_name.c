/*
 *  change_name.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2009 Deutscher Wetterdienst (DWD),
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
 **   void change_name(char         *orig_file_name,
 **                    char         *filter,
 **                    char         *rename_to_rule,
 **                    char         *new_name,
 **                    int          *counter_fd,
 **                    int          **counter,
 **                    unsigned int job_id)
 **
 ** DESCRIPTION
 **   'change_name' takes the 'filter' and analyses the 'orig_file_name'
 **   to chop them in to pieces. Then the pieces are glued together as
 **   described in 'rename_to_rule' and stored in to 'new_name'. To
 **   insert special terms in the new filename the '%' sign is
 **   interpreted as followed:
 **     %ax   - Alternateing character and x can be one of the following:
 **               b  - binary (0 or 1)
 **               dn - character alternates up to n decimal numbers
 **               hn - character alternates up to n hexadecimal numbers
 **     %*n   - address n-th asterisk
 **     %?n   - address n-th question mark
 **     %on   - n-th character from the original filename
 **     %On-m - range n to m of characters from original file
 **             name. n='^' - from the beginning
 **                   m='$' - to the end
 **     %n    - generate a 4 character unique number
 **     %tx   - insert the actual time (x = as strftime())
 **             a - short day "Tue",           A - long day "Tuesday",
 **             b - short month "Jan",         B - long month "January",
 **             d - day of month [01,31],      m - month [01,12],
 **             j - day of the year [001,366], y - year [01,99],
 **             Y - year 1997,                 H - hour [00,23],
 **             M - minute [00,59],            S - second [00,60],
 **             U - Unix time, number of seconds since 00:00:00 01/01/1970 UTC
 **     %T[+|-|*|/|%]xS|M|H|d - Time modifier
 **            |     |   |
 **            |     |   +----> Time unit: S - second
 **            |     |                     M - minute
 **            |     |                     H - hour
 **            |     |                     d - day
 **            |     +--------> Time modifier
 **            +--------------> How time modifier should be applied.
 **     %h    - insert the hostname
 **     %%    - the '%' sign   
 **     \     - are ignored
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
 **   16.06.2002 H.Kiehl    Insert option Unix time.
 **   28.06.2003 H.Kiehl    Added time modifier.
 **   10.04.2005 H.Kiehl    Added binary alternating numbers.
 **   14.02.2008 H.Kiehl    Added decimal and hexadecimal alternating numbers.
 **   09.05.2008 H.Kiehl    For %o or %O check that the original file name
 **                         is long enough.
 **
 */
DESCR__E_M3

#include <stdio.h>           /*                                          */
#include <string.h>          /* strcpy(), strlen()                       */
#include <stdlib.h>          /* atoi(), getenv()                         */
#include <ctype.h>           /* isdigit()                                */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>          /* gethostname()                            */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include <time.h>

/* External global variables. */
extern char *p_work_dir;

/* Local function prototypes. */
static int  get_alternate_number(unsigned int);


/*############################# change_name() ###########################*/
void
change_name(char         *orig_file_name,
            char         *filter,
            char         *rename_to_rule,
            char         *new_name,
            int          *counter_fd,
            int          **counter,
            unsigned int job_id)
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
          *ptr_newname = NULL,
          time_mod_sign = '+';
   int    act_asterix = 0,
          act_questioner = 0,
          alternate,
          count_asterix = 0,
          count_questioner = 0,
          end,
          i,
          number,
          original_filename_length = -1,
          pattern_found = NO,
          start,
          tmp_count_questioner;
   time_t time_buf,
          time_modifier = 0;

   /* Copy original filename to a temporary buffer. */
   (void)strcpy(buffer, orig_file_name);

   ptr_oldname = ptr_oldname_tmp = buffer;
   ptr_filter  = ptr_filter_tmp  = filter;

   /* Taking 'orig_file_name' in pieces like 'filter'. */
   while (*ptr_filter != '\0')
   {
      switch (*ptr_filter)
      {
         case '*' : /* '*' in filter detected -> mark position in array. */
            ptr_asterix[count_asterix++] = ptr_oldname;
            ptr_filter++;
            break;

         case '?' : /* '?' in filter -> skip one character in both words */
                    /*                  and mark questioner.             */
            ptr_filter++;
            ptr_questioner[count_questioner++] = ptr_oldname++;
            break;

         default  : /* Search the char, found in filter, in oldname. */
            pattern_found = NO;
            tmp_count_questioner = 0;

            /* Mark the latest position. */
            ptr_filter_tmp = ptr_filter;
            while (*ptr_filter_tmp != '\0')
            {
               /* Position the orig_file_name pointer to the next      */
               /* place where filter pointer and orig_file_name pointer*/
               /* are equal.                                           */
               while ((*ptr_oldname != *ptr_filter_tmp) &&
                      (*ptr_oldname != '\0'))
               {
                  ptr_oldname++;
               }
               /* Mark the latest position. */
               ptr_oldname_tmp = ptr_oldname;

               /* Test the rest of the pattern. */
               while ((*ptr_filter_tmp != '*') && (*ptr_filter_tmp != '\0'))
               {
                  if ((*ptr_filter_tmp == *ptr_oldname_tmp) ||
                      (*ptr_filter_tmp == '?'))
                  {
                     if (*ptr_filter_tmp == '?')
                     {
                        /* Mark questioner. */
                        ptr_questioner[count_questioner + tmp_count_questioner++] = ptr_oldname_tmp;
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
               if ((pattern_found == YES) && (*ptr_filter_tmp == '\0') &&
                   (*ptr_oldname_tmp != '\0'))
               {
                  pattern_found = NO;
               }
               if (pattern_found == YES)
               {
                  /* Mark end of string 'ptr_asterix[count_asterix]' */
                  /* and position the pointer to next char.          */
                  *ptr_oldname = '\0';
                  ptr_oldname = ptr_oldname_tmp;
                  ptr_filter = ptr_filter_tmp;
                  count_questioner += tmp_count_questioner;
                  break;
               }
               else
               {
                  ptr_filter_tmp = ptr_filter;
                  ptr_oldname++;
                  if (tmp_count_questioner > 0)
                  {
                     tmp_count_questioner = 0;
                  }
               }
            }
            break;
      }
   }

#ifdef _DEBUG
   system_log(INFO_SIGN, NULL, 0, _("found %d *"), count_asterix);
   for (i = 0; i < count_asterix; i++)
   {
      system_log(INFO_SIGN, NULL, 0, "ptr_asterix[%d]=%s", i, ptr_asterix[i]);
   }
   system_log(INFO_SIGN, NULL, 0, _("found %d ?"), count_questioner);
   for (i = 0; i < count_questioner; i++)
   {
      system_log(INFO_SIGN, NULL, 0, "ptr_questioner[%d]=%c",
                 i, *ptr_questioner[i]);
   }
#endif

   /* Create new_name as specified in rename_to_rule. */
   ptr_rule = rename_to_rule;
   ptr_newname = new_name;
   while (*ptr_rule != '\0')
   {
      /* Work trough the rule and paste the name. */
      switch (*ptr_rule)
      {
         case '%' : /* Locate the '%' -> handle the rule. */
            ptr_rule++;
            switch (*ptr_rule)
            {
               case '*' : /* Insert the addressed ptr_asterix. */
                  ptr_rule++;

                  /* Test for number. */
                  i = 0;
                  while ((isdigit((int)(*(ptr_rule + i)))) &&
                         (i < MAX_INT_LENGTH))
                  {
                     string[i] = *(ptr_rule + i);
                     i++;
                  }
                  ptr_rule += i;
                  string[i] = '\0';
                  number = atoi(string) - 1; /* Human count from 1 and computer from 0. */
                  if (number >= count_asterix)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("illegal '*' addressed: %d"), number + 1);
                  }
                  else
                  {
                     (void)strcpy(ptr_newname, ptr_asterix[number]);
                     ptr_newname += strlen(ptr_asterix[number]);
                  }
                  break;

               case '?' : /* Insert the addressed ptr_questioner. */
                  ptr_rule++;

                  /* Test for number. */
                  i = 0;
                  while ((isdigit((int)(*(ptr_rule + i)))) &&
                         (i < MAX_INT_LENGTH))
                  {
                     string[i] = *(ptr_rule + i);
                     i++;
                  }
                  ptr_rule += i;
                  string[i] = '\0';
                  number = atoi(string) - 1; /* Human count from 1 and computer from 0. */
                  if (number >= count_questioner)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("illegal '?' addressed: %d (%s %s)"),
                                number + 1, filter, rename_to_rule);

                  }
                  else
                  {
                     *ptr_newname = *ptr_questioner[number];
                     ptr_newname++;
                  }
                  break;

               case 'o' : /* Insert the addressed character from orig_file_name. */
                  ptr_rule++;

                  /* Test for number. */
                  i = 0;
                  while ((isdigit((int)(*(ptr_rule + i)))) &&
                         (i < MAX_INT_LENGTH))
                  {
                     string[i] = *(ptr_rule + i);
                     i++;
                  }
                  if (original_filename_length == -1)
                  {
                     original_filename_length = strlen(orig_file_name);
                  }
                  ptr_rule += i;
                  string[i] = '\0';
                  number = atoi(string) - 1; /* Human count from 1 and computer from 0. */
                  if (number <= original_filename_length)
                  {
                     *ptr_newname++ = *(orig_file_name + number);
                  }
                  break;

               case 'O' : /* Insert the addressed range of characters from orig_file_name. */
                  ptr_rule++;

                  /* Read from start. */
                  if (*ptr_rule == '^') /* Means start from the beginning. */
                  {
                     ptr_oldname = orig_file_name;
                     ptr_rule++;
                     start = 0;
                  }
                  else /* Read the start number. */
                  {
                     i = 0;
                     while ((isdigit((int)(*(ptr_rule + i)))) &&
                            (i < MAX_INT_LENGTH))
                     {
                        string[i] = *(ptr_rule + i);
                        i++;
                     }
                     ptr_rule += i;
                     string[i] = '\0';
                     start = atoi(string) - 1; /* Human count from 1 and computer from 0. */
                     ptr_oldname = orig_file_name + start;
                  }
                  if (*ptr_rule == '-')
                  {
                     if (original_filename_length == -1)
                     {
                        original_filename_length = strlen(orig_file_name);
                     }
                     if (start <= original_filename_length)
                     {
                        /* Skip the '-'. */
                        ptr_rule++;
                        /* Read the end. */
                        if (*ptr_rule == '$') /* Means up to the end. */
                        {
                           (void)strcpy(ptr_newname, ptr_oldname);
                           ptr_newname += strlen(ptr_oldname);
                           ptr_rule++;
                        }
                        else
                        {
                           i = 0;
                           while ((isdigit((int)(*(ptr_rule + i)))) &&
                                  (i < MAX_INT_LENGTH))
                           {
                              string[i] = *(ptr_rule + i);
                              i++;
                           }
                           ptr_rule += i;
                           string[i] = '\0';
                           end = atoi(string) - 1; /* Human count from 1 and computer from 0. */
                           if (end > original_filename_length)
                           {
                              end = original_filename_length;
                           }
                           number = end - start + 1;  /* Including the first and last. */
                           if (number < 0)
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         _("The start (%d) and end (%d) range do not make sense in rule %s. Start must be larger then end!"),
                                         start, end, rename_to_rule);
                           }
                           else
                           {
                              my_strncpy(ptr_newname, ptr_oldname, number);
                              ptr_newname += number;
                           }
                        }
                     }
                  }
                  else
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                _("There is no end range specified for rule %s"),
                                rename_to_rule);
                  }
                  break;

               case 'n' : /* Generate a unique number 4 characters. */
                  ptr_rule++;
                  if (*counter_fd == -1)
                  {
                     if ((*counter_fd = open_counter_file(COUNTER_FILE, counter)) < 0)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   _("Failed to open counter file, ignoring %n."));
                        break;
                     }
                  }
                  next_counter(*counter_fd, *counter);
                  (void)sprintf(ptr_newname, "%04x", **counter);
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

               case 'T' : /* Time modifier. */
                  {
                     int time_unit;

                     ptr_rule++;
                     switch (*ptr_rule)
                     {
                        case '+' :
                        case '-' :
                        case '*' :
                        case '/' :
                        case '%' :
                           time_mod_sign = *ptr_rule;
                           ptr_rule++;
                           break;
                        default  :
                           time_mod_sign = '+';
                           break;
                     }
                     i = 0;
                     while ((isdigit((int)(*(ptr_rule + i)))) &&
                            (i < MAX_INT_LENGTH))
                     {
                        string[i] = *(ptr_rule + i);
                        i++;
                     }
                     ptr_rule += i;
                     if ((i > 0) && (i < MAX_INT_LENGTH))
                     {
                        string[i] = '\0';
                        time_modifier = atoi(string);
                     }
                     else
                     {
                        if (i == MAX_INT_LENGTH)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      _("The time modifier specified for rule %s is to large."),
                                      rename_to_rule);
                        }
                        else
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      _("There is no time modifier specified for rule %s"),
                                      rename_to_rule);
                        }
                        time_modifier = 0;
                     }
                     switch (*ptr_rule)
                     {
                        case 'S' : /* Second */
                           time_unit = 1;
                           ptr_rule++;
                           break;
                        case 'M' : /* Minute */
                           time_unit = 60;
                           ptr_rule++;
                           break;
                        case 'H' : /* Hour */
                           time_unit = 3600;
                           ptr_rule++;
                           break;
                        case 'd' : /* Day */
                           time_unit = 86400;
                           ptr_rule++;
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
                  break;

               case 't' : /* Insert the actual time as strftime. */
                  time_buf = time(NULL);
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
                  ptr_rule++;
                  switch (*ptr_rule)
                  {
                     case 'a' : /* Short day of the week 'Tue'. */
                        number = strftime(ptr_newname, MAX_FILENAME_LENGTH,
                                          "%a", gmtime(&time_buf));
                        break;
                     case 'A' : /* Long day of the week 'Tuesday'. */
                        number = strftime(ptr_newname, MAX_FILENAME_LENGTH,
                                          "%A", gmtime(&time_buf));
                        break;
                     case 'b' : /* Short month 'Jan'. */
                        number = strftime(ptr_newname, MAX_FILENAME_LENGTH,
                                          "%b", gmtime(&time_buf));
                        break;
                     case 'B' : /* Month 'January'. */
                        number = strftime(ptr_newname, MAX_FILENAME_LENGTH,
                                          "%B", gmtime(&time_buf));
                        break;
                     case 'd' : /* Day of month [01,31]. */
                        number = strftime(ptr_newname, MAX_FILENAME_LENGTH,
                                          "%d", gmtime(&time_buf));
                        break;
                     case 'j' : /* Day of year [001,366]. */
                        number = strftime(ptr_newname, MAX_FILENAME_LENGTH,
                                          "%j", gmtime(&time_buf));
                        break;
                     case 'm' : /* Month [01,12]. */
                        number = strftime(ptr_newname, MAX_FILENAME_LENGTH,
                                          "%m", gmtime(&time_buf));
                        break;
                     case 'y' : /* Year 2 chars [01,99]. */
                        number = strftime(ptr_newname, MAX_FILENAME_LENGTH,
                                          "%y", gmtime(&time_buf));
                        break;
                     case 'Y' : /* Year 4 chars 1997. */
                        number = strftime(ptr_newname, MAX_FILENAME_LENGTH,
                                          "%Y", gmtime(&time_buf));
                        break;
                     case 'H' : /* Hour [00,23]. */
                        number = strftime(ptr_newname, MAX_FILENAME_LENGTH,
                                          "%H", gmtime(&time_buf));
                        break;
                     case 'M' : /* Minute [00,59]. */
                        number = strftime(ptr_newname, MAX_FILENAME_LENGTH,
                                          "%M", gmtime(&time_buf));
                        break;
                     case 'S' : /* Second [00,59]. */
                        number = strftime(ptr_newname, MAX_FILENAME_LENGTH,
                                          "%S", gmtime(&time_buf));
                        break;
                     case 'U' : /* Unix time. */
#if SIZEOF_TIME_T == 4
                        number = sprintf(ptr_newname, "%ld",
#else
                        number = sprintf(ptr_newname, "%lld",
#endif
                                         (pri_time_t)time_buf);
                        break;
                     default  : /* Unknown character - ignore. */
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   _("Illegal time option (%c) in rule %s"),
                                   *ptr_rule, rename_to_rule);
                        number = 0;
                        break;
                  }
                  ptr_newname += number;
                  ptr_rule++;
                  break;

               case '%' : /* Insert the '%' sign. */
                  *ptr_newname = '%';
                  ptr_newname++;
                  ptr_rule++;
                  break;

               case 'a' : /* Insert an alternating character. */
                  ptr_rule++;
                  if ((alternate = get_alternate_number(job_id)) == INCORRECT)
                  {
                     alternate = 0;
                  }
                  switch (*ptr_rule)
                  {
                     case 'b' : /* Binary. */
                        if ((alternate % 2) == 0)
                        {
                           *ptr_newname = '0';
                        }
                        else
                        {
                           *ptr_newname = '1';
                        }
                        ptr_rule++;
                        ptr_newname++;
                        break;

                     case 'd' : /* Decimal. */
                        ptr_rule++;
                        if (isdigit((int)*ptr_rule))
                        {
                           *ptr_newname = '0' + (alternate % (*ptr_rule + 1 - '0'));
                           ptr_rule++;
                           ptr_newname++;
                        }
                        else
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      _("Illegal character (%c - not a decimal digit) found in rule %s"),
                                      *ptr_rule, rename_to_rule);
                        }
                        break;

                     case 'h' : /* Hexadecimal. */
                        {
                           char ul_char; /* Upper/lower character. */

                           ptr_rule++;
                           if ((*ptr_rule >= '0') && (*ptr_rule <= '9'))
                           {
                              number = *ptr_rule + 1 - '0';
                           }
                           else if ((*ptr_rule >= 'A') && (*ptr_rule <= 'F'))
                                {
                                   number = 10 + *ptr_rule + 1 - 'A';
                                   ul_char = 'A';
                                }
                           else if ((*ptr_rule >= 'a') && (*ptr_rule <= 'f'))
                                {
                                   number = 10 + *ptr_rule + 1 - 'a';
                                   ul_char = 'a';
                                }
                                else
                                {
                                   system_log(WARN_SIGN, __FILE__, __LINE__,
                                              _("Illegal character (%c - not a hexadecimal digit) found in rule %s"),
                                              *ptr_rule, rename_to_rule);
                                   number = -1;
                                }

                           if (number != -1)
                           {
                              number = alternate % number;
                              if (number >= 10)
                              {
                                 *ptr_newname = ul_char + number - 10;
                              }
                              else
                              {
                                 *ptr_newname = '0' + number;
                              }
                              ptr_rule++;
                              ptr_newname++;
                           }
                        }
                        break;

                     default : /* Unknown character - ignore. */
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   _("Illegal character (%c) found in rule %s"),
                                   *ptr_rule, rename_to_rule);
                        ptr_rule++;
                        break;
                  }
                  break;

               default  : /* No action specified, write an error message. */
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             _("Illegal option (%d) in rule %s"),
                             *ptr_rule, rename_to_rule);
                  ptr_rule++;
                  break;
            }
            break;

         case '*' : /* Locate the '*' -> insert the next 'ptr_asterix'. */
            if (act_asterix == count_asterix)
            {
               system_log(WARN_SIGN, NULL, 0,
                          _("can not pass more '*' as found before -> ignored"));
               system_log(DEBUG_SIGN, NULL, 0,
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

         case '?' : /* Locate the '?' -> insert the next 'ptr_questioner' */
            if (act_questioner == count_questioner)
            {
               system_log(WARN_SIGN, NULL, 0,
                          _("can not pass more '?' as found before -> ignored"));
               system_log(DEBUG_SIGN, NULL, 0,
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

         case '\\' : /* Ignore. */
            ptr_rule++;
            break;

         default  : /* Found an ordinary character -> append it. */
            *ptr_newname = *ptr_rule;
            ptr_newname++;
            ptr_rule++;
            break;
      }
   }
   *ptr_newname = '\0';

   return;
}


/*++++++++++++++++++++++++ get_alternate_number() +++++++++++++++++++++++*/
static int
get_alternate_number(unsigned int job_id)
{
   int  fd,
        ret;
   char alternate_file[MAX_PATH_LENGTH];

   (void)sprintf(alternate_file, "%s%s%s%x",
                 p_work_dir, FIFO_DIR, ALTERNATE_FILE, job_id);
   if ((fd = open(alternate_file, O_RDWR | O_CREAT, FILE_MODE)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to open() `%s' : %s"),
                 alternate_file, strerror(errno));
      ret = INCORRECT;
   }
   else
   {
      struct flock wlock = {F_WRLCK, SEEK_SET, (off_t)0, (off_t)1};

      if (fcntl(fd, F_SETLKW, &wlock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to lock `%s' : %s"),
                    alternate_file, strerror(errno));
         ret = INCORRECT;
      }
      else
      {
         struct stat stat_buf;

         if (fstat(fd, &stat_buf) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' : %s"),
                       alternate_file, strerror(errno));
            ret = INCORRECT;
         }
         else
         {
            if (stat_buf.st_size == 0)
            {
               ret = 0;
            }
            else
            {
               if (read(fd, &ret, sizeof(int)) != sizeof(int))
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to read() from `%s' : %s"),
                             alternate_file, strerror(errno));
                  ret = INCORRECT;
               }
               else
               {
                  if (lseek(fd, 0, SEEK_SET) == -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("Failed to lseek() in `%s' : %s"),
                                alternate_file, strerror(errno));
                     ret = INCORRECT;
                  }
                  else
                  {
                     ret++;
                     if (ret < 0)
                     {
                        ret = 0;
                     }
                  }
               }
            }
            if (ret != INCORRECT)
            {
               if (write(fd, &ret, sizeof(int)) != sizeof(int))
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to write() to `%s' : %s"),
                             alternate_file, strerror(errno));
               }
            }
         }
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("Failed to close() `%s' : %s"),
                    alternate_file, strerror(errno));
      }
   }
   return(ret);
}

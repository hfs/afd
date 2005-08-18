/*
 *  eval_time_str.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   eval_time_str - evaluates crontab like time entry
 **
 ** SYNOPSIS
 **   int eval_time_str(char *time_str, struct bd_time_entry *te);
 **
 ** DESCRIPTION
 **   Evaluates the time and date fields as follows:
 **
 **   field         | <minute> <hour> <day of month> <month> <day of week>
 **   --------------+-----------------------------------------------------
 **   allowed values|  0-59    0-23       1-31        1-12        1-7
 **
 **   These values are stored by the function eval_time_str() into the
 **   structure bd_time_entry te as a bit array. That is for example the
 **   minute value 15 the 15th bit is set by this function.
 **
 ** RETURN VALUES
 **   SUCCESS when time_str was successfully evaluated. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.04.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>                 /* memset()                          */
#include <ctype.h>                  /* isdigit()                         */
#include "amgdefs.h"
#include "bit_array.h"

/* Local function prototypes */
static char *get_number(char *, char *);


/*########################## eval_time_str() ############################*/
int
eval_time_str(char *time_str, struct bd_time_entry *te)
{
   int  continuous = YES,
        first_number = -1,
        number,
        step_size = 0;
   char *ptr = time_str,
        str_number[3];

   memset(te, 0, sizeof(struct bd_time_entry));

   /* Evaluate 'minute' field (0-59) */
   for (;;)
   {
      if ((step_size != 0) ||
          ((ptr = get_number(ptr, str_number)) != NULL))
      {
         if (*ptr == ',')
         {
            if (str_number[0] == '*')
            {
               if ((str_number[1] == '\0') && (first_number == -1))
               {
                  if ((step_size == 1) && (continuous == YES))
                  {
#ifdef _WORKING_LONG_LONG
                     te->continuous_minute = ALL_MINUTES;
#else
                     int i;

                     for (i = 0; i < 60; i++)
                     {
                        bitset(te->continuous_minute, i);
                     }
#endif
                  }
                  else
                  {
#ifdef _WORKING_LONG_LONG
                     te->minute = ALL_MINUTES;
#else
                     int i;

                     for (i = 0; i < 60; i++)
                     {
                        bitset(te->minute, i);
                     }
#endif
                     continuous = YES;
                  }
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Combination of '*' and other numeric values in minute field not possible. Ignoring time entry.");
                  return(INCORRECT);
               }
            }
            else
            {
               if (str_number[1] == '\0')
               {
                  number = str_number[0] - '0';
               }
               else
               {
                  if (str_number[0] > '5')
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Possible values for minute field : 0-59. Ignoring time entry!");
                     return(INCORRECT);
                  }
                  number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
               }
               if (first_number == -1)
               {
#ifdef _WORKING_LONG_LONG
                  te->minute |= bit_array_long[number];
#else
                  bitset(te->minute, number);
#endif
               }
               else
               {
                  int i;

                  if ((step_size == 1) && (continuous == YES))
                  {
                     for (i = first_number; i <= number; i = i + step_size)
                     {
#ifdef _WORKING_LONG_LONG
                        te->continuous_minute |= bit_array_long[i];
#else
                        bitset(te->continuous_minute, i);
#endif
                     }
                     step_size = 0;
                  }
                  else
                  {
                     if (step_size == 0)
                     {
                        for (i = first_number; i <= number; i++)
                        {
#ifdef _WORKING_LONG_LONG
                           te->minute |= bit_array_long[i];
#else
                           bitset(te->minute, i);
#endif
                        }
                     }
                     else
                     {
                        for (i = first_number; i <= number; i = i + step_size)
                        {
#ifdef _WORKING_LONG_LONG
                           te->minute |= bit_array_long[i];
#else
                           bitset(te->minute, i);
#endif
                        }
                        step_size = 0;
                     }
                  }
                  first_number = -1;
               }
            }
            ptr++;
         }
         else if (*ptr == '-')
              {
                 if (str_number[1] == '\0')
                 {
                    first_number = str_number[0] - '0';
                 }
                 else
                 {
                    if (str_number[0] > '5')
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Possible values for minute field : 0-59. Ignoring time entry!");
                       return(INCORRECT);
                    }
                    first_number = ((str_number[0] - '0') * 10) +
                                   str_number[1] - '0';
                 }
                 ptr++;
              }
         else if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 if (str_number[0] == '*')
                 {
                    if (str_number[1] == '\0')
                    {
#ifndef _WORKING_LONG_LONG
                       int i = 0;

#endif
                       if ((step_size == 1) && (continuous == YES))
                       {
#ifdef _WORKING_LONG_LONG
                          te->continuous_minute = ALL_MINUTES;
#else
                          do
                          {
                             bitset(te->continuous_minute, i);
                             i = i + step_size;
                          } while (i < 60);
#endif
                       }
                       else
                       {
#ifdef _WORKING_LONG_LONG
                          te->minute = ALL_MINUTES;
#else
                          if (step_size == 0)
                          {
                             step_size = 1;
                          }
                          do
                          {
                             bitset(te->minute, i);
                             i = i + step_size;
                          } while (i < 60);
#endif
                          continuous = YES;
                       }
                       step_size = 0;
                    }
                    else
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Combination of '*' and other numeric values in minute field not possible. Ignoring time entry.");
                       return(INCORRECT);
                    }
                 }
                 else
                 {
                    if (str_number[1] == '\0')
                    {
                       number = str_number[0] - '0';
                    }
                    else
                    {
                       if (str_number[0] > '5')
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Possible values for minute field : 0-59. Ignoring time entry!");
                          return(INCORRECT);
                       }
                       number = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                    }
                    if (first_number == -1)
                    {
#ifdef _WORKING_LONG_LONG
                       te->minute |= bit_array_long[number];
#else
                       bitset(te->minute, number);
#endif
                    }
                    else
                    {
                       int i;

                       if ((step_size == 1) && (continuous == YES))
                       {
                          for (i = first_number; i <= number; i = i + step_size)
                          {
#ifdef _WORKING_LONG_LONG
                             te->continuous_minute |= bit_array_long[i];
#else
                             bitset(te->continuous_minute, i);
#endif
                          }
                          step_size = 0;
                       }
                       else
                       {
                          if (step_size == 0)
                          {
                             for (i = first_number; i <= number; i++)
                             {
#ifdef _WORKING_LONG_LONG
                                te->minute |= bit_array_long[i];
#else
                                bitset(te->minute, i);
#endif
                             }
                          }
                          else
                          {
                             for (i = first_number; i <= number; i = i + step_size)
                             {
#ifdef _WORKING_LONG_LONG
                                te->minute |= bit_array_long[i];
#else
                                bitset(te->minute, i);
#endif
                             }
                             step_size = 0;
                          }
                          continuous = YES;
                       }
                       first_number = -1;
                    }
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 break;
              }
         else if (*ptr == '\0')
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Premature end of time entry. Ignoring time entry.");
                 return(INCORRECT);
              }
         else if (*ptr == '/')
              {
                 char tmp_str_number[3];

                 (void)memcpy(tmp_str_number, str_number, 3);
                 ptr++;
                 if ((ptr = get_number(ptr, str_number)) == NULL)
                 {
                    return(INCORRECT);
                 }
                 if (str_number[1] == '\0')
                 {
                    if (isdigit((int)str_number[0]) == 0)
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Division by non numeric value <%c>. Ignoring time entry.",
                                  str_number[0]);
                       return(INCORRECT);
                    }
                    step_size = str_number[0] - '0';
                 }
                 else
                 {
                    step_size = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                 }
                 if ((step_size == 0) || (step_size > 59))
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Invalid step size %d in minute field. Ignoring time entry.",
                               step_size);
                    return(INCORRECT);
                 }
                 if (step_size == 1)
                 {
                    continuous = NO;
                 }
                 (void)memcpy(str_number, tmp_str_number, 3);
              }
      }
      else
      {
         return(INCORRECT);
      }
   } /* for (;;) */


   /* Evaluate 'hour' field (0-23) */
   for (;;)
   {
      if ((step_size != 0) ||
          ((ptr = get_number(ptr, str_number)) != NULL))
      {
         if (*ptr == ',')
         {
            if (str_number[0] == '*')
            {
               if ((str_number[1] == '\0') && (first_number == -1))
               {
                  te->hour = ALL_HOURS;
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Combination of '*' and other numeric values in hour field not possible. Ignoring time entry.");
                  return(INCORRECT);
               }
            }
            else
            {
               if (str_number[1] == '\0')
               {
                  number = str_number[0] - '0';
               }
               else
               {
                  if ((str_number[0] > '2') ||
                      ((str_number[0] == '2') && (str_number[1] > '3')))
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Possible values for hour field : 0-23. Ignoring time entry!");
                     return(INCORRECT);
                  }
                  number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
               }
               if (first_number == -1)
               {
                  te->hour |= bit_array[number];
               }
               else
               {
                  int i;

                  if (step_size == 0)
                  {
                     for (i = first_number; i <= number; i++)
                     {
                        te->hour |= bit_array[i];
                     }
                  }
                  else
                  {
                     for (i = first_number; i <= number; i = i + step_size)
                     {
                        te->hour |= bit_array[i];
                     }
                     step_size = 0;
                  }
                  first_number = -1;
               }
            }
            ptr++;
         }
         else if (*ptr == '-')
              {
                 if (str_number[1] == '\0')
                 {
                    first_number = str_number[0] - '0';
                 }
                 else
                 {
                    if ((str_number[0] > '2') ||
                        ((str_number[0] == '2') && (str_number[1] > '3')))
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Possible values for hour field : 0-23. Ignoring time entry!");
                       return(INCORRECT);
                    }
                    first_number = ((str_number[0] - '0') * 10) +
                                   str_number[1] - '0';
                 }
                 ptr++;
              }
         else if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 if (str_number[0] == '*')
                 {
                    if (str_number[1] == '\0')
                    {
                       te->hour = ALL_HOURS;
                    }
                    else
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Combination of '*' and other numeric values in hour field not possible. Ignoring time entry.");
                       return(INCORRECT);
                    }
                 }
                 else
                 {
                    if (str_number[1] == '\0')
                    {
                       number = str_number[0] - '0';
                    }
                    else
                    {
                       if ((str_number[0] > '2') ||
                           ((str_number[0] == '2') && (str_number[1] > '3')))
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Possible values for hour field : 0-23. Ignoring time entry!");
                          return(INCORRECT);
                       }
                       number = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                    }
                    if (first_number == -1)
                    {
                       te->hour |= bit_array[number];
                    }
                    else
                    {
                       int i;

                       if (step_size == 0)
                       {
                          for (i = first_number; i <= number; i++)
                          {
                             te->hour |= bit_array[i];
                          }
                       }
                       else
                       {
                          for (i = first_number; i <= number; i = i + step_size)
                          {
                             te->hour |= bit_array[i];
                          }
                          step_size = 0;
                       }
                       first_number = -1;
                    }
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 break;
              }
         else if (*ptr == '\0')
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Premature end of time entry. Ignoring time entry.");
                 return(INCORRECT);
              }
         else if (*ptr == '/')
              {
                 char tmp_str_number[3];

                 (void)memcpy(tmp_str_number, str_number, 3);
                 ptr++;
                 if ((ptr = get_number(ptr, str_number)) == NULL)
                 {
                    return(INCORRECT);
                 }
                 if (str_number[1] == '\0')
                 {
                    if (isdigit((int)str_number[0]) == 0)
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Division by non numeric value <%c>. Ignoring time entry.",
                                  str_number[0]);
                       return(INCORRECT);
                    }
                    step_size = str_number[0] - '0';
                 }
                 else
                 {
                    step_size = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                 }
                 if ((step_size == 0) || (step_size > 23))
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Invalid step size %d in hour field. Ignoring time entry.",
                               step_size);
                    return(INCORRECT);
                 }
                 (void)memcpy(str_number, tmp_str_number, 3);
              }
      }
      else
      {
         return(INCORRECT);
      }
   } /* for (;;) */
   
   /* Evaluate 'day of month' field (1-31) */
   for (;;)
   {
      if ((step_size != 0) ||
          ((ptr = get_number(ptr, str_number)) != NULL))
      {
         if (*ptr == ',')
         {
            if (str_number[0] == '*')
            {
               if ((str_number[1] == '\0') && (first_number == -1))
               {
                  te->day_of_month = ALL_DAY_OF_MONTH;
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Combination of '*' and other numeric values in day of month field not possible. Ignoring time entry.");
                  return(INCORRECT);
               }
            }
            else
            {
               if (str_number[1] == '\0')
               {
                  if (str_number[0] == '0')
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Possible values for day of month field : 1-31. Ignoring time entry!");
                     return(INCORRECT);
                  }
                  number = str_number[0] - '0';
               }
               else
               {
                  if ((str_number[0] > '3') ||
                      ((str_number[0] == '3') && (str_number[1] > '1')))
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Possible values for day of month field : 1-31. Ignoring time entry!");
                     return(INCORRECT);
                  }
                  number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
               }
               if (first_number == -1)
               {
                  te->day_of_month |= bit_array[number - 1];
               }
               else
               {
                  int i;

                  if (step_size == 0)
                  {
                     for (i = first_number; i <= number; i++)
                     {
                        te->day_of_month |= bit_array[i - 1];
                     }
                  }
                  else
                  {
                     for (i = first_number; i <= number; i = i + step_size)
                     {
                        te->day_of_month |= bit_array[i - 1];
                     }
                     step_size = 0;
                  }
                  first_number = -1;
               }
            }
            ptr++;
         }
         else if (*ptr == '-')
              {
                 if (str_number[1] == '\0')
                 {
                    if (str_number[0] == '0')
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Possible values for day of month field : 1-31. Ignoring time entry!");
                       return(INCORRECT);
                    }
                    first_number = str_number[0] - '0';
                 }
                 else
                 {
                    if ((str_number[0] > '3') ||
                        ((str_number[0] == '3') && (str_number[1] > '1')))
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Possible values for day of month field : 1-31. Ignoring time entry!");
                       return(INCORRECT);
                    }
                    first_number = ((str_number[0] - '0') * 10) +
                                   str_number[1] - '0';
                 }
                 ptr++;
              }
         else if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 if (str_number[0] == '*')
                 {
                    if (str_number[1] == '\0')
                    {
                       te->day_of_month = ALL_DAY_OF_MONTH;
                    }
                    else
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Combination of '*' and other numeric values in day of month field not possible. Ignoring time entry.");
                       return(INCORRECT);
                    }
                 }
                 else
                 {
                    if (str_number[1] == '\0')
                    {
                       if (str_number[0] == '0')
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Possible values for day of month field : 1-31. Ignoring time entry!");
                          return(INCORRECT);
                       }
                       number = str_number[0] - '0';
                    }
                    else
                    {
                       if ((str_number[0] > '3') ||
                           ((str_number[0] == '3') && (str_number[1] > '1')))
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Possible values for day of month field : 1-31. Ignoring time entry!");
                          return(INCORRECT);
                       }
                       number = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                    }
                    if (first_number == -1)
                    {
                       te->day_of_month |= bit_array[number - 1];
                    }
                    else
                    {
                       int i;

                       if (step_size == 0)
                       {
                          for (i = first_number; i <= number; i++)
                          {
                             te->day_of_month |= bit_array[i - 1];
                          }
                       }
                       else
                       {
                          for (i = first_number; i <= number; i = i + step_size)
                          {
                             te->day_of_month |= bit_array[i - 1];
                          }
                          step_size = 0;
                       }
                       first_number = -1;
                    }
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 break;
              }
         else if (*ptr == '\0')
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Premature end of time entry. Ignoring time entry.");
                 return(INCORRECT);
              }
         else if (*ptr == '/')
              {
                 char tmp_str_number[3];

                 (void)memcpy(tmp_str_number, str_number, 3);
                 ptr++;
                 if ((ptr = get_number(ptr, str_number)) == NULL)
                 {
                    return(INCORRECT);
                 }
                 if (str_number[1] == '\0')
                 {
                    if (isdigit((int)str_number[0]) == 0)
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Division by non numeric value <%c>. Ignoring time entry.",
                                  str_number[0]);
                       return(INCORRECT);
                    }
                    step_size = str_number[0] - '0';
                 }
                 else
                 {
                    step_size = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                 }
                 if ((step_size == 0) || (step_size > 31))
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Invalid step size %d in day of month field. Ignoring time entry.",
                               step_size);
                    return(INCORRECT);
                 }
                 (void)memcpy(str_number, tmp_str_number, 3);
              }
      }
      else
      {
         return(INCORRECT);
      }
   } /* for (;;) */
   
   /* Evaluate 'month' field (1-12) */
   for (;;)
   {
      if ((step_size != 0) ||
          ((ptr = get_number(ptr, str_number)) != NULL))
      {
         if (*ptr == ',')
         {
            if (str_number[0] == '*')
            {
               if ((str_number[1] == '\0') && (first_number == -1))
               {
                  te->month = ALL_MONTH;
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Combination of '*' and other numeric values in time string field not possible. Ignoring time entry.");
                  return(INCORRECT);
               }
            }
            else
            {
               if (str_number[1] == '\0')
               {
                  if (str_number[0] == '0')
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Possible values for month field : 1-12. Ignoring time entry!");
                     return(INCORRECT);
                  }
                  number = str_number[0] - '0';
               }
               else
               {
                  if ((str_number[0] > '1') || (str_number[1] > '2'))
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Possible values for month field : 1-12. Ignoring time entry!");
                     return(INCORRECT);
                  }
                  number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
               }
               if (first_number == -1)
               {
                  te->month |= bit_array[number - 1];
               }
               else
               {
                  int i;

                  if (step_size == 0)
                  {
                     for (i = first_number; i <= number; i++)
                     {
                        te->month |= bit_array[i - 1];
                     }
                  }
                  else
                  {
                     for (i = first_number; i <= number; i = i + step_size)
                     {
                        te->month |= bit_array[i - 1];
                     }
                     step_size = 0;
                  }
                  first_number = -1;
               }
            }
            ptr++;
         }
         else if (*ptr == '-')
              {
                 if (str_number[1] == '\0')
                 {
                    if (str_number[0] == '0')
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Possible values for month field : 1-12. Ignoring time entry!");
                       return(INCORRECT);
                    }
                    first_number = str_number[0] - '0';
                 }
                 else
                 {
                    if ((str_number[0] > '1') || (str_number[1] > '2'))
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Possible values for month field : 1-12. Ignoring time entry!");
                       return(INCORRECT);
                    }
                    first_number = ((str_number[0] - '0') * 10) +
                                   str_number[1] - '0';
                 }
                 ptr++;
              }
         else if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 if (str_number[0] == '*')
                 {
                    if (str_number[1] == '\0')
                    {
                       te->month = ALL_MONTH;
                    }
                    else
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Combination of '*' and other numeric values in time string field not possible. Ignoring time entry.");
                       return(INCORRECT);
                    }
                 }
                 else
                 {
                    if (str_number[1] == '\0')
                    {
                       if (str_number[0] == '0')
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Possible values for month field : 1-12. Ignoring time entry!");
                          return(INCORRECT);
                       }
                       number = str_number[0] - '0';
                    }
                    else
                    {
                       if ((str_number[0] > '1') || (str_number[1] > '2'))
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Possible values for month field : 1-12. Ignoring time entry!");
                          return(INCORRECT);
                       }
                       number = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                    }
                    if (first_number == -1)
                    {
                       te->month |= bit_array[number - 1];
                    }
                    else
                    {
                       int i;

                       if (step_size == 0)
                       {
                          for (i = first_number; i <= number; i++)
                          {
                             te->month |= bit_array[i - 1];
                          }
                       }
                       else
                       {
                          for (i = first_number; i <= number; i = i + step_size)
                          {
                             te->month |= bit_array[i - 1];
                          }
                          step_size = 0;
                       }
                       first_number = -1;
                    }
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 break;
              }
         else if (*ptr == '\0')
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Premature end of time entry. Ignoring time entry.");
                 return(INCORRECT);
              }
         else if (*ptr == '/')
              {
                 char tmp_str_number[3];

                 (void)memcpy(tmp_str_number, str_number, 3);
                 ptr++;
                 if ((ptr = get_number(ptr, str_number)) == NULL)
                 {
                    return(INCORRECT);
                 }
                 if (str_number[1] == '\0')
                 {
                    if (isdigit((int)str_number[0]) == 0)
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Division by non numeric value <%c>. Ignoring time entry.",
                                  str_number[0]);
                       return(INCORRECT);
                    }
                    step_size = str_number[0] - '0';
                 }
                 else
                 {
                    step_size = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                 }
                 if ((step_size == 0) || (step_size > 12))
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Invalid step size %d in month field. Ignoring time entry.",
                               step_size);
                    return(INCORRECT);
                 }
                 (void)memcpy(str_number, tmp_str_number, 3);
              }
      }
      else
      {
         return(INCORRECT);
      }
   } /* for (;;) */

   /* Evaluate 'day of week' field (1-7) */
   for (;;)
   {
      if ((step_size != 0) ||
          ((ptr = get_number(ptr, str_number)) != NULL))
      {
         if (*ptr == ',')
         {
            if (str_number[0] == '*')
            {
               if ((str_number[1] == '\0') && (first_number == -1))
               {
                  te->day_of_week = ALL_DAY_OF_WEEK;
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Combination of '*' and other numeric values in time string field not possible. Ignoring time entry.");
                  return(INCORRECT);
               }
            }
            else
            {
               if (str_number[1] == '\0')
               {
                  if ((str_number[0] < '1') || (str_number[0] > '7'))
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Possible values for day of week field : 1-7. Ignoring time entry!");
                     return(INCORRECT);
                  }
                  number = str_number[0] - '0';
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Possible values for day of week field : 1-7. Ignoring time entry!");
                  return(INCORRECT);
               }
               if (first_number == -1)
               {
                  te->day_of_week |= bit_array[number - 1];
               }
               else
               {
                  int i;

                  if (step_size == 0)
                  {
                     for (i = first_number; i <= number; i++)
                     {
                        te->day_of_week |= bit_array[i - 1];
                     }
                  }
                  else
                  {
                     for (i = first_number; i <= number; i = i + step_size)
                     {
                        te->day_of_week |= bit_array[i - 1];
                     }
                     step_size = 0;
                  }
                  first_number = -1;
               }
            }
            ptr++;
         }
         else if (*ptr == '-')
              {
                 if (str_number[1] == '\0')
                 {
                    if ((str_number[0] < '1') || (str_number[0] > '7'))
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Possible values for day of week field : 1-7. Ignoring time entry!");
                       return(INCORRECT);
                    }
                    first_number = str_number[0] - '0';
                 }
                 else
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Possible values for day of week field : 1-7. Ignoring time entry!");
                    return(INCORRECT);
                 }
                 ptr++;
              }
         else if ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\0'))
              {
                 if (str_number[0] == '*')
                 {
                    if (str_number[1] == '\0')
                    {
                       te->day_of_week = ALL_DAY_OF_WEEK;
                    }
                    else
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Combination of '*' and other numeric values in time string field not possible. Ignoring time entry.");
                       return(INCORRECT);
                    }
                 }
                 else
                 {
                    if (str_number[1] == '\0')
                    {
                       if ((str_number[0] < '1') || (str_number[0] > '7'))
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Possible values for day of week field : 1-7. Ignoring time entry!");
                          return(INCORRECT);
                       }
                       number = str_number[0] - '0';
                    }
                    else
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Possible values for day of week field : 1-7. Ignoring time entry!");
                       return(INCORRECT);
                    }
                    if (first_number == -1)
                    {
                       te->day_of_week |= bit_array[number - 1];
                    }
                    else
                    {
                       int i;

                       if (step_size == 0)
                       {
                          for (i = first_number; i <= number; i++)
                          {
                             te->day_of_week |= bit_array[i - 1];
                          }
                       }
                       else
                       {
                          for (i = first_number; i <= number; i = i + step_size)
                          {
                             te->day_of_week |= bit_array[i - 1];
                          }
                          step_size = 0;
                       }
                       first_number = -1;
                    }
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 break;
              }
         else if (*ptr == '/')
              {
                 char tmp_str_number[3];

                 (void)memcpy(tmp_str_number, str_number, 3);
                 ptr++;
                 if ((ptr = get_number(ptr, str_number)) == NULL)
                 {
                    return(INCORRECT);
                 }
                 if (str_number[1] == '\0')
                 {
                    if (isdigit((int)str_number[0]) == 0)
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Division by non numeric value <%c>. Ignoring time entry.",
                                  str_number[0]);
                       return(INCORRECT);
                    }
                    step_size = str_number[0] - '0';
                 }
                 else
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Invalid step size %d in day of week field. Ignoring time entry.",
                               step_size);
                    return(INCORRECT);
                 }
                 if ((step_size == 0) || (step_size > 7))
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Invalid step size %d in day of week field. Ignoring time entry.",
                               step_size);
                    return(INCORRECT);
                 }
                 (void)memcpy(str_number, tmp_str_number, 3);
              }
      }
      else
      {
         return(INCORRECT);
      }
   } /* for (;;) */

   return(SUCCESS);
}


/*++++++++++++++++++++++++++++ get_number() +++++++++++++++++++++++++++++*/
static char *
get_number(char *ptr, char *str_number)
{
   int i = 0;

   while ((*ptr != ' ') && (*ptr != ',') && (*ptr != '-') &&
          (*ptr != '\0') && (*ptr != '\t') && (i < 3) && (*ptr != '/'))
   {
      if ((isdigit((int)(*ptr)) == 0) && (*ptr != '*'))
      {
         if ((*ptr > ' ') && (*ptr < '?'))
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Invalid character %c [%d] in time string! Ignoring time entry.",
                       *ptr, *ptr);
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Invalid character [%d] in time string! Ignoring time entry.",
                       *ptr);
         }
         return(NULL);
      }
      str_number[i] = *ptr;
      ptr++; i++;
   }

   if (i == 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Hmm, no values entered. Ignoring time entry.");
      return(NULL);
   }
   else if (i > 2)
        {
           system_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Hmm, number with more then two digits. Ignoring time entry.");
           return(NULL);
        }

   str_number[i] = '\0';
   return(ptr);
}

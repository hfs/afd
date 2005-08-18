/*
 *  eval_dupcheck_options.c - Part of AFD, an automatic file distribution
 *                            program.
 *  Copyright (c) 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   eval_dupcheck_options - evaluate dupcheck options
 **
 ** SYNOPSIS
 **   char *eval_dupcheck_options(char         *ptr,
 **                               time_t       *timeout,
 **                               unsigned int *flag)
 **
 ** DESCRIPTION
 **   This function evaluates the dupcheck option that can be specified
 **   under [dir options] and [options] which has the following format:
 **
 **   dupcheck[ <timeout>[ <check type>[ <action>[ <CRC type>]]]]
 **
 **        <timeout>    - Time in seconds when this CRC value is to be
 **                       discarded. (Default 3600)
 **        <check type> - What type of check is to be performed, the
 **                       following values are possible:
 **                         1 - Filename only. (default)
 **                         2 - File content.
 **                         3 - Filename and file content.
 **        <action>     - What action is to be taken when we find a
 **                       duplicate. The following values are possible:
 **                        24 - Delete. (default)
 **                        25 - Store the duplicate file in the following
 **                             directory: $AFD_WORK_DIR/files/store/<id>
 **                             Where <id> is the directoy id when this
 **                             is a input duplicate check or the job id
 **                             when it is an output duplicate check.
 **                        26 - Only warn is SYSTEM_LOG.
 **                        33 - Delete and warn.
 **                        34 - Store and warn.
 **        <CRC type>   - What type of CRC check is to be performed.
 **                       Currently only 16 (CRC32) is supported.
 **
 ** RETURN VALUES
 **   Ruturns a ptr to the end of the given string.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.06.2005 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdlib.h>
#include <ctype.h>


/*####################### eval_dupcheck_options() #######################*/
char *
eval_dupcheck_options(char *ptr, time_t *timeout, unsigned int *flag)
{
   int  length = 0;
   char number[MAX_LONG_LENGTH + 1];

   ptr += DUPCHECK_ID_LENGTH;
   while ((*ptr == ' ') || (*ptr == '\t'))
   {
      ptr++;
   }
   while ((isdigit((int)(*ptr))) && (length < MAX_LONG_LENGTH))
   {
      number[length] = *ptr;
      ptr++; length++;
   }
   if ((length > 0) && (length != MAX_LONG_LENGTH))
   {
      number[length] = '\0';
      *timeout = atol(number);
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      length = 0;
      while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
      {
         number[length] = *ptr;
         ptr++; length++;
      }
      if ((length > 0) && (length != MAX_INT_LENGTH))
      {
         int val;

         number[length] = '\0';
         val = atoi(number);
         if (val == DC_FILENAME_ONLY_BIT)
         {
            *flag = DC_FILENAME_ONLY;
         }
         else if (val == DC_FILE_CONTENT_BIT)
              {
                 *flag = DC_FILE_CONTENT;
              }
         else if (val == DC_FILE_CONT_NAME_BIT)
              {
                 *flag = DC_FILE_CONT_NAME;
              }
              else
              {
                 *flag = DC_FILENAME_ONLY;
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Unkown duplicate check type %d using default %d.",
                            val, DC_FILENAME_ONLY_BIT);
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Possible types are: %d (filename only), %d (file content only) and %d (filename and content).",
                            DC_FILENAME_ONLY_BIT, DC_FILE_CONTENT_BIT,
                            DC_FILE_CONT_NAME_BIT);
              }
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         length = 0;
         while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
         {
            number[length] = *ptr;
            ptr++; length++;
         }
         if ((length > 0) && (length != MAX_INT_LENGTH))
         {
            number[length] = '\0';
            val = atoi(number);
            if (val == DC_DELETE_BIT)
            {
               *flag |= DC_DELETE;
            }
            else if (val == DC_STORE_BIT)
                 {
                    *flag |= DC_STORE;
                 }
            else if (val == DC_WARN_BIT)
                 {
                    *flag |= DC_WARN;
                 }
            else if (val == DC_DELETE_WARN_BIT)
                 {
                    *flag |= (DC_DELETE | DC_WARN);
                 }
            else if (val == DC_STORE_WARN_BIT)
                 {
                    *flag |= (DC_STORE | DC_WARN);
                 }
                 else
                 {
                    *flag |= DC_DELETE;
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               "Unkown duplicate check action %d using default %d.",
                               val, DC_DELETE);
                 }

            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
            length = 0;
            while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
            {
               number[length] = *ptr;
               ptr++; length++;
            }
            if ((length > 0) && (length != MAX_INT_LENGTH))
            {
               number[length] = '\0';
               val = atoi(number);
               if (val != DC_CRC32_BIT)
               {
                  *flag |= DC_CRC32;
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Unkown duplicate check CRC type %d using default %d.",
                             val, DC_CRC32_BIT);
               }
               else
               {
                  *flag |= DC_CRC32;
               }
            }
            else
            {
               *flag |= DC_CRC32;
               if (length == MAX_INT_LENGTH)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Integer value for duplicate check CRC type to large.");
               }
            }
         }
         else
         {
            *flag |= (DC_DELETE | DC_CRC32);
            if (length == MAX_INT_LENGTH)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Integer value for duplicate check action to large.");
            }
         }
      }
      else
      {
         *flag = DC_FILENAME_ONLY | DC_CRC32 | DC_DELETE;
         if (length == MAX_INT_LENGTH)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Integer value for duplicate check type to large.");
         }
      }
   }
   else
   {
      *timeout = DEFAULT_DUPCHECK_TIMEOUT;
      *flag = DC_FILENAME_ONLY | DC_CRC32 | DC_DELETE;
      if (length == MAX_LONG_LENGTH)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Long integer value for duplicate check timeout to large.");
      }
   }
   while ((*ptr != '\n') && (*ptr != '\0'))
   {
      ptr++;
   }

   return(ptr);
}

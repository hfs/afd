/*
 *  eval_dir_options.c - Part of AFD, an automatic file distribution
 *                       program.
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
 **   eval_dir_options - evaluates the directory options
 **
 ** SYNOPSIS
 **   void eval_dir_options(int  dir_pos,
 **                         char *dir_options,
 **                         char *old_dir_options)
 **
 ** DESCRIPTION
 **   Reads and evaluates the directory options from one directory
 **   of the DIR_CONFIG file. It currently knows the following options:
 **
 **        delete unknown files
 **        do not delete unknown files           [DEFAULT]
 **        report unknown files                  [DEFAULT]
 **        do not report unknown files
 **        old file time <value in hours>        [DEFAULT 24]
 **        end character <decimal number>
 **        important dir
 **        time * * * * *
 **        do not remove
 **        store remote list
 **        priority <value>                      [DEFAULT 9]
 **        force rereads
 **        max process <value>                   [DEFAULT 10]
 **
 **   For the string old_dir_options it is possible to define the
 **   following values:
 **        <hours> <DIRS*>
 **                 |||||
 **                 ||||+----> important directory
 **                 |||+-----> do not report
 **                 ||+------> report
 **                 |+-------> do not delete
 **                 +--------> delete
 **
 ** RETURN VALUES
 **   None, will just enter the values found into the structure
 **   dir_data.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.03.2000 H.Kiehl Created
 **   12.08.2000 H.Kiehl Addition of priority.
 **   31.08.2000 H.Kiehl Addition of option "force rereads".
 **
 */
DESCR__E_M3

#include <stdio.h>                /* stderr, fprintf()                   */
#include <stdlib.h>               /* atoi(), malloc(), free(), strtoul() */
#include <string.h>               /* strcmp(), strncmp(), strerror()     */
#include <ctype.h>                /* isdigit()                           */
#include <sys/types.h>
#include <sys/stat.h>             /* fstat()                             */
#include <unistd.h>               /* read(), close(), setuid()           */
#include <fcntl.h>                /* O_RDONLY, etc                       */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int             max_process_per_dir,
                       sys_log_fd;
extern struct dir_data *dd;

#define DEL_UNKNOWN_FILES_FLAG           1
#define OLD_FILE_TIME_FLAG               2
#define DONT_REP_UNKNOWN_FILES_FLAG      4
#define DIRECTORY_PRIORITY_FLAG          8
#define END_CHARACTER_FLAG               16
#define TIME_FLAG                        32
#define MAX_PROCESS_FLAG                 64
#define DO_NOT_REMOVE_FLAG               128
#define STORE_REMOTE_LIST_FLAG           256
#define DONT_DEL_UNKNOWN_FILES_FLAG      512
#define REP_UNKNOWN_FILES_FLAG           1024
#define FORCE_REREAD_FLAG                2048
#define IMPORTANT_DIR_FLAG               4096

#define DEL_UNKNOWN_FILES_ID             "delete unknown files"
#define DEL_UNKNOWN_FILES_ID_LENGTH      20
#define OLD_FILE_TIME_ID                 "old file time"
#define OLD_FILE_TIME_ID_LENGTH          13
#define DONT_REP_UNKNOWN_FILES_ID        "do not report unknown files"
#define DONT_REP_UNKNOWN_FILES_ID_LENGTH 27
#define END_CHARACTER_ID                 "end character"
#define END_CHARACTER_ID_LENGTH          13
#define TIME_ID                          "time"
#define TIME_ID_LENGTH                   4
#define MAX_PROCESS_ID                   "max process"
#define MAX_PROCESS_ID_LENGTH            11
#define DO_NOT_REMOVE_ID                 "do not remove"
#define DO_NOT_REMOVE_ID_LENGTH          13
#define STORE_REMOTE_LIST                "store remote list"
#define STORE_REMOTE_LIST_LENGTH         17
#define DONT_DEL_UNKNOWN_FILES_ID        "do not delete unknown files"
#define DONT_DEL_UNKNOWN_FILES_ID_LENGTH 20
#define REP_UNKNOWN_FILES_ID             "report unknown files"
#define REP_UNKNOWN_FILES_ID_LENGTH      20
#define FORCE_REREAD_ID                  "force reread"
#define FORCE_REREAD_ID_LENGTH           12
#define IMPORTANT_DIR_ID                 "important dir"
#define IMPORTANT_DIR_ID_LENGTH          13


/*########################## eval_dir_options() #########################*/
void
eval_dir_options(int  dir_pos,
                 char *dir_options,
                 char *old_dir_options)
{
   int  used = 0;          /* Used to see whether option has */
                           /* already been set.              */
   char *ptr,
        *end_ptr = NULL,
        byte_buf;

   /* Set some default directory options. */
   dd[dir_pos].delete_unknown_files = NO;
   dd[dir_pos].old_file_time = OLD_FILE_TIME * 3600;
   dd[dir_pos].report_unknown_files = YES;
   dd[dir_pos].end_character = -1;
#ifndef _WITH_PTHREAD
   dd[dir_pos].important_dir = NO;
#endif /* _WITH_PTHREAD */
   dd[dir_pos].time_option = NO;
   dd[dir_pos].max_process = max_process_per_dir;
   dd[dir_pos].remove = YES;
   dd[dir_pos].stupid_mode = YES;
   dd[dir_pos].priority = DEFAULT_PRIORITY;
   dd[dir_pos].force_reread = NO;

   /*
    * First evaluate the old directory option so we
    * so we can later override them with the new options.
    */
   if ((*old_dir_options != '\n') && (*old_dir_options != '\0'))
   {
      int  length = 0;
      char number[MAX_INT_LENGTH + 1];

      ptr = old_dir_options;
      while ((isdigit(*ptr)) && (length < MAX_INT_LENGTH))
      {
         number[length] = *ptr;
         ptr++; length++;
      }
      if ((length > 0) && (length != MAX_INT_LENGTH))
      {
         number[length] = '\0';
         dd[dir_pos].old_file_time = atoi(number) * 3600;
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         end_ptr = ptr;
      }
      else
      {
         end_ptr = ptr;
      }
      while ((*end_ptr != '\n') && (*end_ptr != '\0'))
      {
        switch(*end_ptr)
        {
           case 'd' :
           case 'D' : /* Delete unknown files. */
              dd[dir_pos].delete_unknown_files = YES;
              break;

           case 'i' :
           case 'I' : /* Do NOT delete unknown files. */
              dd[dir_pos].delete_unknown_files = NO;
              break;

           case 'r' :
           case 'R' : /* Report unknown files. */
              dd[dir_pos].report_unknown_files = YES;
              break;

           case 's' :
           case 'S' : /* Do NOT report unknown files. */
              dd[dir_pos].report_unknown_files = NO;
              break;

           case 'E' : /* Check end character of file. */
              if ((*(end_ptr + 1) == 'C') && (*(end_ptr + 2) == '='))
              {
                 end_ptr += 3;
                 length = 0;
                 while ((isdigit(*end_ptr)) &&
                        (length < MAX_INT_LENGTH))
                 {
                    number[length] = *end_ptr;
                    end_ptr++; length++;
                 }
                 if (length != 0)
                 {
                    number[length] = '\0';
                    dd[dir_pos].end_character = atoi(number);
                 }
              }
              break;

#ifndef _WITH_PTHREAD
           case '*' : /* This is an important directory! */
              dd[dir_pos].important_dir = YES;
              break;
#endif

           case '\t':
           case ' ' : /* Ignore any spaces. */
              break;

           default : /* Give a warning about an unknown */
                     /* character option.               */
             (void)rec(sys_log_fd, WARN_SIGN,
                       "Unknown option character %c <%d> for directory option. (%s %d)\n",
                       *end_ptr, (int)*end_ptr, __FILE__, __LINE__);
             break;
        }
        end_ptr++;
      }
   }

   ptr = dir_options;
   while (*ptr != '\0')
   {
      if (((used & DEL_UNKNOWN_FILES_FLAG) == 0) &&
          (strncmp(ptr, DEL_UNKNOWN_FILES_ID, DEL_UNKNOWN_FILES_ID_LENGTH) == 0))
      {
         used |= DEL_UNKNOWN_FILES_FLAG;
         ptr += DEL_UNKNOWN_FILES_ID_LENGTH;
         while ((*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
         dd[dir_pos].delete_unknown_files = YES;
      }
      else if (((used & OLD_FILE_TIME_FLAG) == 0) &&
               (strncmp(ptr, OLD_FILE_TIME_ID, OLD_FILE_TIME_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= OLD_FILE_TIME_FLAG;
              ptr += OLD_FILE_TIME_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit(*ptr)) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].old_file_time = atoi(number) * 3600;
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 end_ptr = ptr;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & DIRECTORY_PRIORITY_FLAG) == 0) &&
               (strncmp(ptr, PRIORITY_ID, PRIORITY_ID_LENGTH) == 0))
           {
              used |= DIRECTORY_PRIORITY_FLAG;
              ptr += PRIORITY_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\n'))
              {
                 ptr++;
              }
              if (isdigit(*ptr))
              {
                 dd[dir_pos].priority = *ptr;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & DONT_REP_UNKNOWN_FILES_FLAG) == 0) &&
               (strncmp(ptr, DONT_REP_UNKNOWN_FILES_ID, DONT_REP_UNKNOWN_FILES_ID_LENGTH) == 0))
           {
              used |= DONT_REP_UNKNOWN_FILES_FLAG;
              ptr += DONT_REP_UNKNOWN_FILES_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].report_unknown_files = NO;
           }
      else if (((used & END_CHARACTER_FLAG) == 0) &&
               (strncmp(ptr, END_CHARACTER_ID, END_CHARACTER_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= END_CHARACTER_FLAG;
              ptr += END_CHARACTER_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit(*ptr)) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].end_character = atoi(number);
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 end_ptr = ptr;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & MAX_PROCESS_FLAG) == 0) &&
               (strncmp(ptr, MAX_PROCESS_ID, MAX_PROCESS_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= MAX_PROCESS_FLAG;
              ptr += MAX_PROCESS_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit(*ptr)) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].max_process = atoi(number);
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 end_ptr = ptr;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & TIME_FLAG) == 0) &&
               (strncmp(ptr, TIME_ID, TIME_ID_LENGTH) == 0))
           {
              char tmp_char;

              used |= TIME_FLAG;
              ptr += TIME_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              end_ptr = ptr;
              while ((*end_ptr != '\n') && (*end_ptr != '\0'))
              {
                 end_ptr++;
              }
              tmp_char = *end_ptr;
              *end_ptr = '\0';
              if (eval_time_str(ptr, &dd[dir_pos].te) == SUCCESS)
              {
                 dd[dir_pos].time_option = YES;
              }
              else
              {
                 (void)rec(sys_log_fd, WARN_SIGN,
                           "Invalid %s string <%s> (%s %d)\n",
                           TIME_ID, ptr, __FILE__, __LINE__);
              }
              *end_ptr = tmp_char;
              ptr = end_ptr;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & DO_NOT_REMOVE_FLAG) == 0) &&
               (strncmp(ptr, DO_NOT_REMOVE_ID, DO_NOT_REMOVE_ID_LENGTH) == 0))
           {
              used |= DO_NOT_REMOVE_FLAG;
              ptr += DO_NOT_REMOVE_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].remove = NO;
           }
      else if (((used & STORE_REMOTE_LIST_FLAG) == 0) &&
               (strncmp(ptr, STORE_REMOTE_LIST, STORE_REMOTE_LIST_LENGTH) == 0))
           {
              used |= STORE_REMOTE_LIST_FLAG;
              ptr += STORE_REMOTE_LIST_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].stupid_mode = NO;
           }
      else if (((used & DONT_DEL_UNKNOWN_FILES_FLAG) == 0) &&
               (strncmp(ptr, DONT_DEL_UNKNOWN_FILES_ID, DONT_DEL_UNKNOWN_FILES_ID_LENGTH) == 0))
           {
              used |= DONT_DEL_UNKNOWN_FILES_FLAG;
              ptr += DONT_DEL_UNKNOWN_FILES_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].delete_unknown_files = NO;
           }
      else if (((used & REP_UNKNOWN_FILES_FLAG) == 0) &&
               (strncmp(ptr, REP_UNKNOWN_FILES_ID, REP_UNKNOWN_FILES_ID_LENGTH) == 0))
           {
              used |= REP_UNKNOWN_FILES_FLAG;
              ptr += REP_UNKNOWN_FILES_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].report_unknown_files = YES;
           }
      else if (((used & FORCE_REREAD_FLAG) == 0) &&
               (strncmp(ptr, FORCE_REREAD_ID, FORCE_REREAD_ID_LENGTH) == 0))
           {
              used |= REP_UNKNOWN_FILES_FLAG;
              ptr += FORCE_REREAD_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].force_reread = YES;
           }
#ifndef _WITH_PTHREAD
      else if (((used & IMPORTANT_DIR_FLAG) == 0) &&
               (strncmp(ptr, IMPORTANT_DIR_ID, IMPORTANT_DIR_ID_LENGTH) == 0))
           {
              used |= IMPORTANT_DIR_FLAG;
              ptr += IMPORTANT_DIR_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].important_dir = YES;
           }
#endif /* _WITH_PTHREAD */
           else
           {
              /* Ignore this option */
              end_ptr = ptr;
              while ((*end_ptr != '\n') && (*end_ptr != '\0'))
              {
                 end_ptr++;
              }
              byte_buf = *end_ptr;
              *end_ptr = '\0';
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Unknown or duplicate option <%s> in DIR_CONFIG file. (%s %d)\n",
                        ptr, __FILE__, __LINE__);
              *end_ptr = byte_buf;
              ptr = end_ptr;
           }

      while (*ptr == '\n')
      {
         ptr++;
      }
   } /* while (*ptr != '\0') */

   return;
}

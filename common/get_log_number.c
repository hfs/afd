/*
 *  get_log_number.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_log_number - gets the largest log number in log directory
 **
 ** SYNOPSIS
 **   void get_log_number(int  *log_number,
 **                       int  max_log_number,
 **                       char *log_name,
 **                       int log_name_length)
 **
 ** DESCRIPTION
 **   The function get_log_number() looks in the AFD log directory
 **   for the highest log number of the log file type 'log_name'.
 **   If the log number gets larger than 'max_log_number', these log
 **   files will be removed.
 **
 ** RETURN VALUES
 **   Returns the highest log number 'log_number' it finds in the
 **   AFD log directory.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.02.1996 H.Kiehl Created
 **   12.01.1997 H.Kiehl Remove log files with their log number being
 **                      larger or equal to max_log_number.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* fprintf(), sprintf(), rename()     */
#include <string.h>                /* strcmp(), strncmp(), strerror()    */
#include <stdlib.h>                /* atoi()                             */
#include <ctype.h>                 /* isdigit()                          */
#include <sys/types.h>
#include <sys/stat.h>              /* stat(), S_ISREG()                  */
#include <unistd.h>                /* unlink()                           */
#include <dirent.h>                /* readdir(), closedir(), DIR,        */
                                   /* struct dirent                      */
#include <errno.h>

/* Global variables */
extern int  sys_log_fd;
extern char *p_work_dir;


/*########################### get_log_number() ##########################*/
void
get_log_number(int  *log_number,
               int  max_log_number,
               char *log_name,
               int  log_name_length)
{
   int           i,
                 tmp_number;
   char          *ptr,
                 str_number[MAX_INT_LENGTH],
                 fullname[MAX_PATH_LENGTH],
                 log_dir[MAX_PATH_LENGTH];
   struct stat   stat_buf;
   struct dirent *p_dir;
   DIR           *dp;

   /* Initialise log directory */
   (void)strcpy(log_dir, p_work_dir);
   (void)strcat(log_dir, LOG_DIR);
   if ((dp = opendir(log_dir)) == NULL)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not opendir() %s : %s (%s %d)\n",
                log_dir, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   errno = 0;
   while ((p_dir = readdir(dp)) != NULL)
   {
      if (p_dir->d_name[0] == '.')
      {
         continue;
      }

      if (strncmp(p_dir->d_name, log_name, log_name_length) == 0)
      {
         (void)sprintf(fullname, "%s/%s", log_dir, p_dir->d_name);
         if (stat(fullname, &stat_buf) < 0)
         {
            if (errno != ENOENT)
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Can't access file %s : %s (%s %d)\n",
                         fullname, strerror(errno), __FILE__, __LINE__);
            }
            continue;
         }

         /* Sure it is a normal file? */
         if (S_ISREG(stat_buf.st_mode))
         {
            ptr = p_dir->d_name;
            ptr += log_name_length;
            if (*ptr != '\0')
            {
               i = 0;
               while (isdigit(*ptr) != 0)
               {
                  str_number[i] = *ptr;
                  ptr++; i++;
               }
               if (i > 0)
               {
                  str_number[i] = '\0';
                  if ((tmp_number = atoi(str_number)) > *log_number)
                  {
                     if (tmp_number > max_log_number)
                     {
                        if (unlink(fullname) < 0)
                        {
                           (void)rec(sys_log_fd, WARN_SIGN,
                                     "Failed to unlink() %s : %s (%s %d)\n",
                                     fullname, strerror(errno),
                                     __FILE__, __LINE__);
                        }
                        else
                        {
                           (void)rec(sys_log_fd, INFO_SIGN,
                                     "Removing log file %s (%s %d)\n",
                                     fullname, __FILE__, __LINE__);
                        }
                     }
                     else
                     {
                        *log_number = tmp_number;
                     }
                  }
               }
            }
         }
      }
      errno = 0;
   } /*  while (readdir(dp) != NULL) */

   if (errno)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "readdir() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   if (closedir(dp) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "closedir() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   return;
}

/*
 *  inspect_archive.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   inspect_archive - walks through a directory and deletes old
 **                     archives
 **
 ** SYNOPSIS
 **   void inspect_archive(char *archive_dir)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.02.1996 H.Kiehl Created
 **   11.05.1998 H.Kiehl Adapted to new message names.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                  /* strcpy(), strcmp(), strlen(),    */
                                     /* strerror()                       */
#include <stdlib.h>                  /* atoi()                           */
#include <ctype.h>                   /* isdigit()                        */
#include <sys/types.h>
#include <sys/stat.h>                /* stat(), S_ISDIR()                */
#include <dirent.h>                  /* opendir(), readdir(), closedir() */
#include <errno.h>
#include "awdefs.h"

#define TIME_UP  1

extern int    sys_log_fd,
              removed_archives;
extern time_t current_time;

static int    is_msgname(char *),
              check_time(char *);


/*########################### inspect_archive() #########################*/
void
inspect_archive(char *archive_dir)
{
   char          *ptr = NULL;
   struct stat   stat_buf;
   struct dirent *p_struct_dir;
   DIR           *p_dir;

   if ((p_dir = opendir(archive_dir)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to opendir() %s : %s (%s %d)\n",
                archive_dir, strerror(errno), __FILE__, __LINE__);
      return;
   }

   errno = 0;
   while ((p_struct_dir = readdir(p_dir)) != NULL)
   {
      if (p_struct_dir->d_name[0]  == '.')
      {
         continue;
      }

      ptr = archive_dir + strlen(archive_dir);
      *(ptr++) = '/';
      (void)strcpy(ptr, p_struct_dir->d_name);

      if (stat(archive_dir, &stat_buf) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to stat() %s : %s (%s %d)\n",
                   archive_dir, strerror(errno), __FILE__, __LINE__);
         return;
      }

      if (S_ISDIR(stat_buf.st_mode))
      {
         if (is_msgname(p_struct_dir->d_name) != INCORRECT)
         {
            if (check_time(p_struct_dir->d_name) == TIME_UP)
            {
               if (rec_rmdir(archive_dir) != INCORRECT)
               {
                  removed_archives++;
#ifdef _LOCK_REMOVE_INFO
                  (void)rec(sys_log_fd, INFO_SIGN,
                            "Removed archive %s. (%s %d)\n",
                            archive_dir, __FILE__, __LINE__);
#endif
               }
               else
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Failed to remove archive %s. (%s %d)\n",
                            archive_dir, __FILE__, __LINE__);
               }
            }
         }
         else /* host- or username (or some other junk) */
         {
            inspect_archive(archive_dir);
         }
      }
      if (ptr != NULL)
      {
         ptr[-1] = '\0';
      }
      errno = 0;
   }

   if (ptr != NULL)
   {
      ptr[-1] = '\0';
   }

   if (errno)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to readdir() %s : %s (%s %d)\n",
                archive_dir, strerror(errno), __FILE__, __LINE__);
   }
   if (closedir(p_dir) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to closedir() %s : %s (%s %d)\n",
                archive_dir, strerror(errno), __FILE__, __LINE__);
   }

   return;
}


/*+++++++++++++++++++++++++++++ is_msgname() ++++++++++++++++++++++++++++*/
static int
is_msgname(char *name)
{
   if (isdigit(name[0]) != 0)
   {
      if (name[1] == '_')
      {
         int i = 2;

         while (name[i] != '\0')
         {
            if (isdigit(name[i]) == 0)
            {
               if (name[i] != '_')
               {
                  return(INCORRECT);
               }
            }
            i++;
         }
         return(SUCCESS);
      }
   }

   return(INCORRECT);
}


/*++++++++++++++++++++++++++++ check_time() +++++++++++++++++++++++++++++*/
static int
check_time(char *name)
{
   int  i = 2;
   char str_time[MAX_INT_LENGTH];

   while ((name[i] != '_') && (name[i] != '\0'))
   {
      str_time[i - 2] = name[i];
      i++;
   }
   str_time[i - 2] = '\0';

   if (current_time < atoi(str_time))
   {
      return(0);
   }

   return(TIME_UP);
}

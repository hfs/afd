/*
 *  inspect_archive.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2000 Deutscher Wetterdienst (DWD),
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
 **   29.11.2000 H.Kiehl Optimized the removing of files.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                  /* strcpy(), strcmp(), strlen(),    */
                                     /* strerror()                       */
#include <stdlib.h>                  /* atoi()                           */
#include <ctype.h>                   /* isdigit()                        */
#include <unistd.h>                  /* rmdir()                          */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>                  /* opendir(), readdir(), closedir() */
#include <errno.h>
#include "awdefs.h"

#define TIME_UP  1

/* External global variables. */
extern int          sys_log_fd;
extern unsigned int removed_archives,
                    removed_files;
extern time_t       current_time;

/* Local function prototypes. */
static int          check_time(char *),
                    is_msgname(char *),
                    remove_archive(char *);


/*########################### inspect_archive() #########################*/
void
inspect_archive(char *archive_dir)
{
   char          *ptr = NULL;
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

      if (is_msgname(p_struct_dir->d_name) != INCORRECT)
      {
         if (check_time(p_struct_dir->d_name) == TIME_UP)
         {
            if (remove_archive(archive_dir) != INCORRECT)
            {
               removed_archives++;
#ifdef _LOG_REMOVE_INFO
               (void)rec(sys_log_fd, INFO_SIGN,
                         "Removed archive %s. (%s %d)\n",
                         archive_dir, __FILE__, __LINE__);
#endif
            }
         }
      }
      else /* host- or username (or some other junk) */
      {
         inspect_archive(archive_dir);
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


/*+++++++++++++++++++++++++++ remove_archive() ++++++++++++++++++++++++++*/
static int
remove_archive(char *dirname)
{
   char          *ptr;
   struct dirent *dirp;
   DIR           *dp;

   ptr = dirname + strlen(dirname);
   *ptr++ = '/';
   *ptr = '\0';

   if ((dp = opendir(dirname)) == NULL)
   {
      if (errno != ENOTDIR)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to opendir() %s : %s (%s %d)\n",
                   dirname, strerror(errno), __FILE__, __LINE__);
      }
      return(INCORRECT);
   }

   while ((dirp = readdir(dp)) != NULL)
   {
      if ((dirp->d_name[0] == '.') && ((dirp->d_name[1] == '\0') ||
          ((dirp->d_name[1] == '.') && (dirp->d_name[2] == '\0'))))
      {
         continue;
      }
      (void)strcpy(ptr, dirp->d_name);
#ifdef _WORKING_UNLINK
      if (unlink(dirname) == -1)
#else
      if (remove(dirname) == -1)
#endif /* _WORKING_UNLINK */
      {
         if (errno == ENOENT)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Failed to delete %s : %s (%s %d)\n",
                      dirname, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to delete %s : %s (%s %d)\n",
                      dirname, strerror(errno), __FILE__, __LINE__);
            (void)closedir(dp);
            return(INCORRECT);
         }
      }
      else
      {
         removed_files++;
      }
   }
   ptr[-1] = 0;
   if ((rmdir(dirname) == -1) && (errno != EEXIST))
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to rmdir() %s : %s (%s %d)\n",
                dirname, strerror(errno), __FILE__, __LINE__);
      (void)closedir(dp);
      return(INCORRECT);
   }
   if (closedir(dp) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to closedir() %s : %s (%s %d)\n",
                dirname, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   return(SUCCESS);
}

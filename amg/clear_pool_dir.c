/*
 *  clear_pool_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   clear_pool_dir - moves files left in the pool directory back
 **                    to their original directories
 **
 ** SYNOPSIS
 **   void clear_pool_dir(void)
 **
 ** DESCRIPTION
 **   The function clear_pool_dir() tries to moves files back to
 **   their original directories from the pool directory that have
 **   been left after a crash. If it cannot determine the original
 **   directory or this directory simply does not exist, these files
 **   will be deleted.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.05.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                /* sprintf()                            */
#include <stdlib.h>               /* malloc(), free()                     */
#include <string.h>               /* strcpy(), strerror()                 */
#include <ctype.h>                /* isdigit()                            */
#include <sys/stat.h>             /* stat(), S_ISREG()                    */
#include <unistd.h>               /* read(), close(), rmdir(), R_OK ...   */
#include <dirent.h>               /* opendir(), closedir(), readdir(),    */
                                  /* DIR, struct dirent                   */
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

/* Local global variables */
static int                 dir_id;

/* External global variables */
extern int                 *no_of_dir_names,
                           sys_log_fd;
extern struct dir_name_buf *dnb;
extern char                *p_work_dir;

/* Local function prototypes */
static int                 get_source_dir(char *, char *);
static void                move_files_back(char *, char *);


/*########################## clear_pool_dir() ##########################*/
void
clear_pool_dir(void)
{
   char        pool_dir[MAX_PATH_LENGTH],
               orig_dir[MAX_PATH_LENGTH];
   struct stat stat_buf;

   (void)sprintf(pool_dir, "%s%s%s", p_work_dir, AFD_FILE_DIR, AFD_TMP_DIR);

   if (stat(pool_dir, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Failed to stat() %s : %s (%s %d)\n",
                pool_dir, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      DIR *dp;

      if ((dp = opendir(pool_dir)) == NULL)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Failed to opendir() %s : %s (%s %d)\n",
                   pool_dir, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         char          *work_ptr;
         struct dirent *p_dir;

         work_ptr = pool_dir + strlen(pool_dir);
         *(work_ptr++) = '/';
         *work_ptr = '\0';

         errno = 0;
         while ((p_dir = readdir(dp)) != NULL)
         {
            if (p_dir->d_name[0] == '.')
            {
               continue;
            }

            (void)strcpy(work_ptr, p_dir->d_name);
            if (get_source_dir(p_dir->d_name, orig_dir) == INCORRECT)
            {
               /* Remove it, no matter what it is. */
#ifdef _DELETE_LOG
               remove_pool_directory(pool_dir, dir_id);
#else
               (void)rec_rmdir(pool_dir);
#endif
            }
            else
            {
               move_files_back(pool_dir, orig_dir);
            }
            errno = 0;
         }

         if (errno)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Could not readdir() %s : %s (%s %d)\n",
                      pool_dir, strerror(errno), __FILE__, __LINE__);
         }
         if (closedir(dp) == -1)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Could not close directory %s : %s (%s %d)\n",
                      pool_dir, strerror(errno), __FILE__, __LINE__);
         }
      }
   }

   return;
}


/*+++++++++++++++++++++++++++ get_source_dir() ++++++++++++++++++++++++++*/
static int
get_source_dir(char *dir_name, char *orig_dir)
{
   register int i = 0;

   dir_id = -1;
   while ((dir_name[i] != '\0') && (dir_name[i] != '_'))
   {
      if (isdigit(dir_name[i]) == 0)
      {
         return(INCORRECT);
      }
      i++;
   }
   if (dir_name[i] == '_')
   {
      i++;
      while ((dir_name[i] != '\0') && (dir_name[i] != '_'))
      {
         if (isdigit(dir_name[i]) == 0)
         {
            return(INCORRECT);
         }
         i++;
      }
      if (dir_name[i] == '_')
      {
         int start;

         start = ++i;
         while (dir_name[i] != '\0')
         {
            if (isdigit(dir_name[i]) == 0)
            {
               return(INCORRECT);
            }
            i++;
         }
         dir_id = atoi(&dir_name[start]);

         for (i = 0; i < *no_of_dir_names; i++)
         {
            if (dir_id == dnb[i].dir_id)
            {
               /*
                * Before we say this is it, check if it still does
                * exist!
                */
               if (eaccess(dnb[i].dir_name, R_OK | W_OK | X_OK) == -1)
               {
                  (void)rec(sys_log_fd, INFO_SIGN,
                            "Cannot move files back to %s : %s (%s %d)\n",
                            dnb[i].dir_name, strerror(errno),
                            __FILE__, __LINE__);
                  return(INCORRECT);
               }

               (void)strcpy(orig_dir, dnb[i].dir_name);
               return(SUCCESS);
            }
         }
      } /* if (dir_name[i] == '_') */
   } /* if (dir_name[i] == '_') */

   return(INCORRECT);
}


/*+++++++++++++++++++++++++++ move_files_back() +++++++++++++++++++++++++*/
static void
move_files_back(char *pool_dir, char *orig_dir)
{
   DIR *dp;

   if ((dp = opendir(pool_dir)) == NULL)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Failed to opendir() %s : %s (%s %d)\n",
                pool_dir, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      char          *orig_ptr,
                    *pool_ptr;
      struct dirent *p_dir;

      pool_ptr = pool_dir + strlen(pool_dir);
      if (*(pool_ptr - 1) != '/')
      {
         *(pool_ptr++) = '/';
      }
      orig_ptr = orig_dir + strlen(orig_dir);
      if (*(orig_ptr - 1) != '/')
      {
         *(orig_ptr++) = '/';
      }

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            continue;
         }

         (void)strcpy(pool_ptr, p_dir->d_name);
         (void)strcpy(orig_ptr, p_dir->d_name);

         if (move_file(pool_dir, orig_dir) < 0)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Failed to move_file() %s to %s (%s %d)\n",
                      pool_dir, orig_dir, __FILE__, __LINE__);
         }
         errno = 0;
      }
      *pool_ptr = '\0';
      *orig_ptr = '\0';

      if (errno)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not readdir() %s : %s (%s %d)\n",
                   pool_dir, strerror(errno), __FILE__, __LINE__);
      }
      if (closedir(dp) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not close directory %s : %s (%s %d)\n",
                   pool_dir, strerror(errno), __FILE__, __LINE__);
      }

      /*
       * After all files have been moved back to their original
       * directory, remove the directory from the pool directory.
       */
      if (rmdir(pool_dir) == -1)
      {
         if (errno == ENOTEMPTY)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Hmm. Directory %s is not empty?! Will remove it! (%s %d)\n",
                      pool_dir, __FILE__, __LINE__);
            (void)rec_rmdir(pool_dir);
         }
         else
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Could not remove directory %s : %s (%s %d)\n",
                      pool_dir, strerror(errno), __FILE__, __LINE__);
         }
      }
   }

   return;
}

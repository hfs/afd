/*
 *  clear_pool_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   count_pool_files - counts the number of files in the pool directory
 **
 ** SYNOPSIS
 **   int count_pool_files(int *dir_no, char *pool_dir)
 **
 ** DESCRIPTION
 **   The function count_pool_files() counts the files in a pool directory.
 **   This is useful in a situation when the disk is full and dir_check
 **   dies with a SIGBUS when trying to copy files via mmap.
 **
 ** RETURN VALUES
 **   Returns the number of files in the directory and the directory
 **   number.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.04.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                /* sprintf()                            */
#include <stdlib.h>               /* malloc(), free()                     */
#include <string.h>               /* strcpy(), strerror()                 */
#include <ctype.h>                /* isdigit()                            */
#include <sys/stat.h>             /* stat(), S_ISREG()                    */
#include <unistd.h>               /* read(), close(), rmdir()             */
#include <dirent.h>               /* opendir(), closedir(), readdir(),    */
                                  /* DIR, struct dirent                   */
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int   sys_log_fd;
#ifndef _WITH_PTHREAD
extern off_t *file_size_pool;
extern char  **file_name_pool;
#endif

/* Local function prototypes */
static int   get_dir_no(char *);


/*######################### count_pool_files() #########################*/
int
#ifndef _WITH_PTHREAD
count_pool_files(int *dir_no, char *pool_dir)
#else
count_pool_files(int *dir_no, char *pool_dir, off_t *file_size_pool, char **file_name_pool)
#endif
{
   int file_counter = 0;

   /* First determine the directory number. */
   if ((*dir_no = get_dir_no(pool_dir)) != INCORRECT)
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
         struct stat   stat_buf;
         struct dirent *p_dir;

         work_ptr = pool_dir + strlen(pool_dir);
         *(work_ptr++) = '/';
         errno = 0;
         while ((p_dir = readdir(dp)) != NULL)
         {
            if (p_dir->d_name[0] != '.')
            {
               (void)strcpy(work_ptr, p_dir->d_name);
               if (stat(pool_dir, &stat_buf) == -1)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Failed to stat() %s : %s (%s %d)\n",
                            pool_dir, strerror(errno), __FILE__, __LINE__);
               }
               else
               {
                  (void)strcpy(file_name_pool[file_counter], p_dir->d_name);
                  file_size_pool[file_counter] = stat_buf.st_size;
                  file_counter++;
               }
            }
            errno = 0;
         }
         work_ptr[-1] = '\0';

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
         if (file_counter == 0)
         {
            /*
             * If there are no files remove the directoy, handle_dir()
             * will not do either, so it must be done here.
             */
            if (rmdir(pool_dir) == -1)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Could not rmdir() %s : %s (%s %d)\n",
                         pool_dir, strerror(errno), __FILE__, __LINE__);
            }
         }
      }
   }
   else
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "%s does not look like a normal pool directory. (%s %d)\n",
                pool_dir, __FILE__, __LINE__);
   }

   return(file_counter);
}


/*+++++++++++++++++++++++++++++ get_dir_no() ++++++++++++++++++++++++++++*/
static int
get_dir_no(char *pool_dir)
{
   int length;

   if ((length = strlen(pool_dir)) > 0)
   {
      char *ptr;

      length -= 2;
      ptr = pool_dir + length;
      while ((*ptr != '_') && (isdigit(*ptr)) && (length > 0))
      {
         ptr--; length--;
      }
      if (*ptr == '_')
      {
         ptr++;
         return(atoi(ptr));
      }
   }

   return(INCORRECT);
}

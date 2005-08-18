/*
 *  check_create_path.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_create_path - checks if the path exists, if not it is created
 **
 ** SYNOPSIS
 **   int check_create_path(char *path, mode_t permissions)
 **
 ** DESCRIPTION
 **   The function check_create_path() checks if the given path exists
 **   and if not it is created recursively.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when the directory does already exist and access
 **   permissions are correct. It returns the following other values:
 **   CREATED_DIR - if it had to create one or more elements of the directory.
 **   NO_ACCESS   - is returned when it does not have access permissions.
 **   STAT_ERROR  - if stat() fails.
 **   MKDIR_ERROR - fails to create the path.
 **   CHOWN_ERROR - the chown() call failed.
 **   ALLOC_ERROR - fails to allocate memory.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.03.2004 H.Kiehl Created.
 **
 */
DESCR__E_M3

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>


/*######################## check_create_path() ##########################*/
int
check_create_path(char *path, mode_t permissions)
{
   int ret = SUCCESS;

   if (eaccess(path, R_OK | W_OK | X_OK) < 0)
   {
      if (errno == ENOENT)
      {
         int   ii = 0,
               new_size,
               dir_length,
               dir_counter = 0,
               do_chown = NO,
               failed_to_create_dir = NO,
               error_condition = 0;
         uid_t owner;
         gid_t group;
         char  **dir_ptr = NULL;

         do
         {
            if ((ii % 10) == 0)
            {
               new_size = ((ii / 10) + 1) * 10 * sizeof(char *);
               if ((dir_ptr = realloc(dir_ptr, new_size)) == NULL)
               {
                  return(ALLOC_ERROR);
               }
            }
            dir_length = strlen(path);
            dir_ptr[ii] = path + dir_length;
            while (dir_length > 0)
            {
               dir_ptr[ii]--; dir_length--;
               if ((*dir_ptr[ii] == '/') && (*(dir_ptr[ii] + 1) != '\0'))
               {
                  *dir_ptr[ii] = '\0';
                  dir_counter++; ii++;
                  break;
               }
            }
            if (dir_length <= 0)
            {
               break;
            }
         } while (((error_condition = eaccess(path, R_OK | W_OK | X_OK)) < 0) &&
                  (errno == ENOENT));

         if ((error_condition < 0) && (errno != ENOENT))
         {
            if (dir_ptr != NULL)
            {
               free(dir_ptr);
            }
            return(NO_ACCESS);
         }

         if (permissions == 0)
         {
            struct stat stat_buf;

            if (stat(path, &stat_buf) == -1)
            {
               return(STAT_ERROR);
            }
            permissions = stat_buf.st_mode;
            owner = stat_buf.st_uid;
            group = stat_buf.st_gid;
            do_chown = YES;
         }

         /*
          * Try to create all missing directory names recursively.
          */
         for (ii = (dir_counter - 1); ii >= 0; ii--)
         {
            *dir_ptr[ii] = '/';
            if (mkdir(path, permissions) == -1)
            {
               failed_to_create_dir = YES;
               break;
            }
            if (do_chown == YES)
            {
               if (chown(path, owner, group) == -1)
               {
                  ret = CHOWN_ERROR;
               }
            }
         }
         if (dir_ptr != NULL)
         {
            free(dir_ptr);
         }
         if (failed_to_create_dir == NO)
         {
            ret = CREATED_DIR;
         }
         else
         {
            ret = MKDIR_ERROR;
         }
      }
      else
      {
         ret = NO_ACCESS;
      }
   }
   return(ret);
}

/*
 *  eaccess.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 Deutscher Wetterdienst (DWD),
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
 **   eaccess - check user's permissions for a file based on the
 **             effective user ID
 **
 ** SYNOPSIS
 **   int eaccess(char *pathname, int mode)
 **
 ** DESCRIPTION
 **   The function eaccess() checks whether the process would be allowd
 **   to read, write or test for existence of the file <pathname> based
 **   on the effective user ID of the process.
 **
 ** RETURN VALUES
 **   On success, zero is returned. On error -1 is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.01.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>            /* geteuid(), getegid()                   */
#include <errno.h>

#ifndef _SCO
/*############################# eaccess() ###############################*/
int
eaccess(char *pathname, int access_mode)
{
   if ((access_mode & R_OK) || (access_mode & W_OK) || (access_mode & X_OK))
   {
      int         access_ctrl = 0;
      struct stat stat_buf;

      if (stat(pathname, &stat_buf) == -1)
      {
         access_ctrl = -1;
      }
      else
      {
         uid_t euid;
         gid_t egid;

         euid = geteuid();
         egid = getegid();
         if (access_mode & R_OK)
         {
            if ((stat_buf.st_mode & S_IROTH) ||
                ((euid == stat_buf.st_uid) && (stat_buf.st_mode & S_IRUSR)) |
                ((egid == stat_buf.st_gid) && (stat_buf.st_mode & S_IRGRP)))
            {
               access_ctrl = 0;
            }
            else
            {
               access_ctrl = -1;
            }
         }
         if ((access_ctrl == 0) && (access_mode & W_OK))
         {
            if ((stat_buf.st_mode & S_IWOTH) ||
                ((euid == stat_buf.st_uid) && (stat_buf.st_mode & S_IWUSR)) |
                ((egid == stat_buf.st_gid) && (stat_buf.st_mode & S_IWGRP)))
            {
               if (access_ctrl != -1)
               {
                  access_ctrl = 0;
               }
            }
            else
            {
               access_ctrl = -1;
            }
         }
         if ((access_ctrl == 0) && (access_mode & X_OK))
         {
            if ((stat_buf.st_mode & S_IXOTH) ||
                ((euid == stat_buf.st_uid) && (stat_buf.st_mode & S_IXUSR)) |
                ((egid == stat_buf.st_gid) && (stat_buf.st_mode & S_IXGRP)))
            {
               if (access_ctrl != -1)
               {
                  access_ctrl = 0;
               }
            }
            else
            {
               access_ctrl = -1;
            }
         }
      }
      return(access_ctrl);
   }
   else if (access_mode == F_OK)
        {
           int fd;

           if ((fd = open(pathname, O_RDONLY)) == -1)
           {
              return(-1);
           }
           else
           {
              (void)close(fd);
           }
           return(0);
        }
        else
        {
           errno = EINVAL;
           return(-1);
        }
}
#endif /* !_SCO */

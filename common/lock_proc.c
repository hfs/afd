/*
 *  lock_proc.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   lock_proc - locks a process so only one process can be active
 **
 ** SYNOPSIS
 **   int lock_proc(int proc_id, int test_lock)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Returns NULL when it succeeds to lock process 'proc_id' or
 **   the user that currently is using this process. When fcntl()
 **   fails it will exit().
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.09.1997 H.Kiehl Created
 **   28.02.1998 H.Kiehl Tests if a lock does exist.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                      /* exit()                       */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* External global variables */
extern char *p_work_dir;

/* Local global variables */
static int  fd;

/* Local definitions */
#define LOCK_PROC_FILE  "/LOCK_FILE"


/*############################ lock_proc() ##############################*/
char *
lock_proc(int proc_id, int test_lock)
{
   off_t        offset;
   static char  user_str[MAX_FULL_USER_ID_LENGTH];
   char         file[MAX_PATH_LENGTH];
   struct stat  stat_buf;
   struct flock wlock;

   (void)sprintf(file, "%s%s%s", p_work_dir, FIFO_DIR, LOCK_PROC_FILE);

   /* See if the lock file does already exist. */
   errno = 0;
   if ((stat(file, &stat_buf) == -1) && (errno == ENOENT))
   {
      if ((fd = coe_open(file, (O_WRONLY | O_CREAT | O_TRUNC),
                         (S_IRUSR | S_IWUSR))) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open() <%s> : %s", file, strerror(errno));
         exit(INCORRECT);
      }
   }
   else if (errno != 0)
        {
           system_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to stat() <%s> : %s", file, strerror(errno));
           exit(INCORRECT);
        }
        else /* The file already exists. */
        {
           if ((fd = coe_open(file, O_RDWR)) == -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to open() %s : %s", file, strerror(errno));
              exit(INCORRECT);
           }
        }

   /* Position file desciptor over user */
   offset = NO_OF_LOCK_PROC + ((proc_id + 1) * MAX_FULL_USER_ID_LENGTH);
   if (lseek(fd, offset, SEEK_SET) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "lseek() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   wlock.l_type   = F_WRLCK;
   wlock.l_whence = SEEK_SET;
   wlock.l_start  = (off_t)proc_id;
   wlock.l_len    = 1;
   if (test_lock == YES)
   {
      if (fcntl(fd, F_GETLK, &wlock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not get write lock : %s", strerror(errno));
         exit(INCORRECT);
      }
      if (wlock.l_type == F_UNLCK)
      {
         if (close(fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "close() error : %s", strerror(errno));
         }
         return(NULL);
      }

      if (read(fd, user_str, MAX_FULL_USER_ID_LENGTH) != MAX_FULL_USER_ID_LENGTH)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "read() error : %s", strerror(errno));
         exit(INCORRECT);
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }

      return(user_str);
   }
   else
   {
      if (fcntl(fd, F_SETLK, &wlock) == -1)
      {
         if ((errno == EACCES) || (errno == EAGAIN))
         {
            /* The file is already locked. */
            if (read(fd, user_str, MAX_FULL_USER_ID_LENGTH) != MAX_FULL_USER_ID_LENGTH)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "read() error : %s", strerror(errno));
               exit(INCORRECT);
            }
            if (close(fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "close() error : %s", strerror(errno));
            }

            return(user_str);
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not set write lock : %s", strerror(errno));
            exit(INCORRECT);
         }
      }

      get_user(user_str);
      if (write(fd, user_str, MAX_FULL_USER_ID_LENGTH) != MAX_FULL_USER_ID_LENGTH)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
         exit(INCORRECT);
      }
   }

   return(NULL);
}

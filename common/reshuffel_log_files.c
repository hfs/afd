/*
 *  reshuffel_log_files.c - Part of AFD, an automatic file distribution
 *                          program.
 *  Copyright (c) 1997 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   reshuffel_log_files -
 **
 ** SYNOPSIS
 **   void reshuffel_log_files(int log_number, char *log_file, char *p_end)
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
 **   11.01.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>             /* sprintf()                              */
#include <string.h>            /* strcpy(), strerror()                   */
#include <unistd.h>            /* rename()                               */
#include <errno.h>

/* External global variables */
extern int sys_log_fd;


/*######################## reshuffel_log_files() ########################*/
void
reshuffel_log_files(int log_number, char *log_file, char *p_end)
{
   register int i;
   char         dst[MAX_PATH_LENGTH];

   for (i = log_number; i > 0; i--)
   {
      (void)sprintf(p_end, "%d", i);
      (void)strcpy(dst, log_file);
      (void)sprintf(p_end, "%d", i - 1);

      if (rename(log_file, dst) < 0)
      {
         if (errno == ENOSPC)
         {
            int error_flag = NO;

            (void)rec(sys_log_fd, ERROR_SIGN,
                      "DISK FULL!!! Will retry in %d second interval. (%s %d)\n",
                      DISK_FULL_RESCAN_TIME, __FILE__, __LINE__);

            while (errno == ENOSPC)
            {
               (void)sleep(DISK_FULL_RESCAN_TIME);
               errno = 0;
               if (rename(log_file, dst) < 0)
               {
                  if (errno != ENOSPC)
                  {
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "Failed to rename() %s to %s : %s (%s %d)\n",
                               log_file, dst, strerror(errno),
                               __FILE__, __LINE__);
                     error_flag = YES;
                     break;
                  }
               }
            }
            if (error_flag == NO)
            {
               (void)rec(sys_log_fd, INFO_SIGN,
                         "Continuing after disk was full. (%s %d)\n",
                         __FILE__, __LINE__);
            }
         }
         else if (errno != ENOENT)
              {
                 (void)rec(sys_log_fd, WARN_SIGN,
                           "Failed to rename() %s to %s : %s (%s %d)\n",
                           log_file, dst, strerror(errno),
                           __FILE__, __LINE__);
              }
      }
   }

   return;
}

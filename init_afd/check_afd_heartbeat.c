/*
 *  check_afd_heartbeat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_afd_heartbeat - Checks if the heartbeat of process init_afd
 **                         is still working.
 **
 ** SYNOPSIS
 **   int check_afd_heartbeat(long wait_time)
 **
 ** DESCRIPTION
 **   This function checks if the heartbeat of init_afd is still
 **   going.
 **
 ** RETURN VALUES
 **   Returns 1 if another AFD is active, otherwise it returns 0.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.06.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>               /* NULL                                 */
#include <string.h>              /* strerror()                           */
#include <sys/types.h>
#ifndef _NO_MMAP
#include <sys/mman.h>            /* mmap(), munmap()                     */
#endif
#include <sys/stat.h>
#include <unistd.h>              /* sleep()                              */
#include <fcntl.h>               /* O_RDWR                               */
#include <errno.h>

/* External global variables */
extern char afd_active_file[];


/*######################## check_afd_heartbeat() ########################*/
int
check_afd_heartbeat(long wait_time)
{
   int         afd_active = 0;
   struct stat stat_buf;

   if ((stat(afd_active_file, &stat_buf) == 0) &&
       (stat_buf.st_size > ((NO_OF_PROCESS + 1) * sizeof(pid_t))))
   {
      int afd_active_fd;

      if ((afd_active_fd = open(afd_active_file, O_RDWR)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__, "Failed to open() %s : %s",
                    afd_active_file, strerror(errno));
      }
      else
      {
         char *ptr;

#ifdef _NO_MMAP
         if ((ptr = mmap_emu(0, stat_buf.st_size,
                             (PROT_READ | PROT_WRITE),
                             MAP_SHARED, afd_active_file, 0)) == (caddr_t) -1) 
#else
         if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                         MAP_SHARED, afd_active_fd, 0)) == (caddr_t) -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to mmap() to %s : %s",
                       afd_active_file, strerror(errno));
         }
         else
         {
            unsigned int current_value,
                         *heartbeat;
            long         elapsed_time = 0L;

            heartbeat = (unsigned int *)(ptr + ((NO_OF_PROCESS + 1) * sizeof(pid_t)));
            current_value = *heartbeat;

            while (elapsed_time < wait_time)
            {
               if (current_value != *heartbeat)
               {
                  afd_active = 1;
                  break;
               }
               elapsed_time++;
               (void)sleep(1L);
            }
#ifdef _NO_MMAP
            if (munmap_emu(ptr) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to munmap_emu() from %s", afd_active_file);
            }
#else
            if (munmap(ptr, stat_buf.st_size) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to munmap() from %s : %s",
                          afd_active_file, strerror(errno));
            }
#endif
         }
         if (close(afd_active_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to close() %s : %s",
                       afd_active_file, strerror(errno));
         }
      }
   }

   return(afd_active);
}

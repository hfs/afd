/*
 *  check_fra.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_fra - checks if FRA has been updated
 **
 ** SYNOPSIS
 **   int check_fra(void)
 **
 ** DESCRIPTION
 **   This function checks if the FRA (Fileretrieve Status Area)
 **   which is a memory mapped area, is still in use. If not
 **   it will detach from the old memory area and attach
 **   to the new one with the function fra_attach().
 **
 ** RETURN VALUES
 **   Returns NO if the FRA is still in use. Returns YES if a
 **   new FRA has been created. It will then also return new
 **   values for 'fra_id' and 'no_of_dirs'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                       /* stderr, NULL                 */
#include <string.h>                      /* strerror()                   */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _NO_MMAP
#include <sys/mman.h>                    /* munmap()                     */
#endif
#include <errno.h>

/* Global variables */
extern int                        sys_log_fd,
                                  fra_id;
#ifndef _NO_MMAP
extern off_t                      fra_size;
#endif
extern char                       *p_work_dir;
extern struct fileretrieve_status *fra;


/*############################ check_fra() ##############################*/
int
check_fra(void)
{
   if (fra != NULL)
   {
      char *ptr;

      ptr = (char *)fra;
      ptr -= AFD_WORD_OFFSET;

      if (*(int *)ptr == STALE)
      {
#ifdef _NO_MMAP
         if (munmap_emu(ptr) == -1)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to munmap_emu() from FRA (%d) : %s (%s %d)\n",
                      fra_id, strerror(errno), __FILE__, __LINE__);
         }
#else
         if (munmap(ptr, fra_size) == -1)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to munmap() from FRA [fra_id = %d fra_size = %d] : %s (%s %d)\n",
                      fra_id, fra_size, strerror(errno), __FILE__, __LINE__);
         }
#endif

         if (fra_attach() < 0)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to attach to FRA. (%s %d)\n",
                      __FILE__, __LINE__);
            exit(INCORRECT);
         }

         return(YES);
      }
   }

   return(NO);
}

/*
 *  fra_detach.c - Part of AFD, an automatic file distribution program.
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
 **   fra_detach - detaches from the FRA
 **
 ** SYNOPSIS
 **   int fra_detach(void)
 **
 ** DESCRIPTION
 **   Detaches from the memory mapped area of the FRA (File Retrieve
 **   Status Area).
 **
 ** RETURN VALUES
 **   SUCCESS when detaching from the FRA successful. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                      /* strerror(), strlen()         */
#include <unistd.h>                      /* close()                      */
#ifndef _NO_MMAP
#include <sys/mman.h>                    /* mmap(), munmap()             */
#endif
#include <errno.h>

/* Global variables */
extern int                        sys_log_fd,
                                  fra_fd,
                                  fra_id,
                                  no_of_dirs;
#ifndef _NO_MMAP
extern off_t                      fra_size;
#endif
extern char                       *p_work_dir;
extern struct fileretrieve_status *fra;


/*############################ fra_detach() #############################*/
int
fra_detach(void)
{
   if (fra_fd > 0)
   {
      if (close(fra_fd) == -1)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
      fra_fd = -1;
   }

   /* Make sure this is not the case when the */
   /* no_of_dirs is stale.                    */
   if (no_of_dirs > 0)
   {
      /* Detach from FRA */
#ifdef _NO_MMAP
      if (munmap_emu((void *)((char *)fra - AFD_WORD_OFFSET)) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to munmap_emu() FRA : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
#else
      if (munmap(((char *)fra - AFD_WORD_OFFSET), fra_size) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to munmap() FRA : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
#endif
      fra = NULL;
   }

   return(SUCCESS);
}

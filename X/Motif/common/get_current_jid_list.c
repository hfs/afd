/*
 *  get_current_jid_list.c - Part of AFD, an automatic file distribution
 *                           program.
 *  Copyright (c) 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_current_jid_list - creates a list of current job ID's
 **
 ** SYNOPSIS
 **   int get_current_jid_list(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS when it managed to create an array with the current job
 **   ID's, otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.02.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>      /* NULL                                          */
#include <string.h>     /* strerror()                                    */
#include <unistd.h>     /* close()                                       */
#include <sys/mman.h>   /* mmap()                                        */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>     /* malloc(), free()                              */
#include <fcntl.h>
#include <errno.h>
#include "x_common_defs.h"

/* External global variables */
extern int    *current_jid_list,
              no_of_current_jobs,
              sys_log_fd;
extern char   *p_work_dir;
extern Widget toplevel_w;


/*######################## get_current_jid_list() #######################*/
int
get_current_jid_list(void)
{
   int         fd;
   char        file[MAX_PATH_LENGTH],
               *ptr,
               *tmp_ptr;
   struct stat stat_buf;

   (void)sprintf(file, "%s%s%s", p_work_dir, FIFO_DIR, CURRENT_MSG_LIST_FILE);
   if ((fd = open(file, O_RDONLY)) == -1)
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Failed to open() %s. Will not be able to get all information. : %s (%s %d)",
                 file, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   if (fstat(fd, &stat_buf) == -1)
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Failed to fstat() %s. Will not be able to get all information. : %s (%s %d)",
                 file, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return(INCORRECT);
   }

   if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Failed to mmap() to %s. Will not be able to get all information. : %s (%s %d)",
                 file, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return(INCORRECT);
   }
   tmp_ptr = ptr;
   no_of_current_jobs = *(int *)ptr;
   ptr += sizeof(int);

   if (no_of_current_jobs > 0)
   {
      int i;

      if ((current_jid_list = malloc(no_of_current_jobs * sizeof(int))) == NULL)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to malloc() memory. Will not be able to get all information. : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return(INCORRECT);
      }
      for (i = 0; i < no_of_current_jobs; i++)
      {
         current_jid_list[i] = *(int *)ptr;
         ptr += sizeof(int);
      }
   }

   if (munmap(tmp_ptr, stat_buf.st_size) == -1)
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Failed to munmap() %s : %s (%s %d)",
                 file, strerror(errno), __FILE__, __LINE__);
   }
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   return(SUCCESS);
}

/*
 *  msync_emu.c - Part of AFD, an automatic file distribution program.
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
 **
 ** SYNOPSIS
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.10.1994 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>         /* fprintf()                                  */
#include <string.h>        /* strlen(), strcpy(), strlen()               */
#include <unistd.h>        /* write(), close()                           */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "mmap_emu.h"

/* External global variables */
extern int  sys_log_fd;
extern char *p_work_dir;


/*++++++++++++++++++++++++++++ msync_emu() ++++++++++++++++++++++++++++++*/
int
msync_emu(char *shmptr)
{
   int      i = 0,
            write_fd;
   size_t   buf_length;
   char     buf[BUFSIZE],
            request_fifo[MAX_PATH_LENGTH];

   (void)strcpy(request_fifo, p_work_dir);
   (void)strcat(request_fifo, FIFO_DIR);
   (void)strcat(request_fifo, REQUEST_FIFO);
   if ((write_fd = open(request_fifo, 1)) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "MSYNC_EMU : Failed to open() %s : %s (%s %d)\n",
                request_fifo, strerror(errno), __FILE__, __LINE__);
      return(-1);
   }

   buf[i++] = '2';
   buf[i++] = '\t';
   shmptr -= MAX_PATH_LENGTH;
   while ((*shmptr != '\n') && (i < BUFSIZE))
   {
      buf[i++] = *(shmptr++);
   }
   if (i == BUFSIZE)
   {
      (void)close(write_fd);
      (void)rec(sys_log_fd, ERROR_SIGN,
                "MSYNC_EMU : Could not extract the filename. (%s %d)\n",
                __FILE__, __LINE__);
      return(-1);
   }
   buf[i++] = '\n';
   buf[i] = '\0';
#ifdef _MMAP_EMU_DEBUG
   (void)fprintf(stderr,"MSYNC_EMU : %s", buf);
#endif
   buf_length = strlen((char *)buf);
   if (write(write_fd, buf, buf_length) != buf_length)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "write() fifo error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   if (close(write_fd) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to close() %s : %s (%s %d)\n",
                request_fifo, strerror(errno), __FILE__, __LINE__);
   }

   return(0);
}

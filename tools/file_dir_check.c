/*
 *  file_dir_check.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   file_dir_check - shows the messages queued by the FD
 **
 ** SYNOPSIS
 **   file_dir_check [-w <AFD work dir>] [--version]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.05.1998 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>                 /* strcat(), strerror()              */
#include <stdlib.h>                 /* exit()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* write()                           */
#include <errno.h>
#include "version.h"

/* Global variables */
int sys_log_fd = STDERR_FILENO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  fd;
   char fd_cmd_fifo[MAX_PATH_LENGTH],
        message = CHECK_FILE_DIR;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD */
   if (get_afd_path(&argc, argv, fd_cmd_fifo) < 0) 
   {
      exit(INCORRECT);
   }

   (void)strcat(fd_cmd_fifo, FIFO_DIR);
   (void)strcat(fd_cmd_fifo, FD_CMD_FIFO);
   if ((fd = open(fd_cmd_fifo, O_RDWR)) == -1)
   {
      (void)fprintf(stderr,
                    "ERROR  : Failed to open() %s : %s (%s %d)\n",
                    fd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (write(fd, &message, 1) != 1)
   {
      (void)fprintf(stderr,
                    "WARNING: write() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }

   if (close(fd) == -1)
   {
      (void)fprintf(stderr,
                    "WARNING: close() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }

   exit(INCORRECT);
}

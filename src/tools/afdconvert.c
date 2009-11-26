/*
 *  afdconvert.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 Deutscher Wetterdienst (DWD),
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

/*
 ** NAME
 **   afdconvert - converts a file from one format to another
 **
 ** SYNOPSIS
 **   afdconvert <format> <file name to convert>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   0 on normal exit and 1 when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.07.2009 H.Kiehl Created
 **
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "amgdefs.h"

/* Global variables. */
int        production_log_fd = STDERR_FILENO,
           receive_log_fd = STDERR_FILENO,
           sys_log_fd = STDERR_FILENO;
char       *p_work_dir;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int         type;
   off_t       file_size;
   struct stat stat_buf;

   if (argc != 3)
   {
      (void)fprintf(stderr,
                    "Usage: %s <format> <file name to convert>\n", argv[0]);
      exit(1);
   }
   if (strcmp(argv[1], "sohetx") == 0)
   {
      type = SOHETX;
   }
   else if (strcmp(argv[1], "wmo") == 0)
        {
           type = ONLY_WMO;
        }
   else if (strcmp(argv[1], "sohetxwmo") == 0)
        {
           type = SOHETXWMO;
        }
   else if (strcmp(argv[1], "sohetx2wmo1") == 0)
        {
           type = SOHETX2WMO0;
        }
   else if (strcmp(argv[1], "sohetx2wmo0") == 0)
        {
           type = SOHETX2WMO1;
        }
   else if (strcmp(argv[1], "mrz2wmo") == 0)
        {
           type = MRZ2WMO;
        }
   else if (strcmp(argv[1], "unix2dos") == 0)
        {
           type = UNIX2DOS;
        }
   else if (strcmp(argv[1], "dos2unix") == 0)
        {
           type = DOS2UNIX;
        }
   else if (strcmp(argv[1], "lf2crcrlf") == 0)
        {
           type = LF2CRCRLF;
        }
   else if (strcmp(argv[1], "crcrlf2lf") == 0)
        {
           type = CRCRLF2LF;
        }
        else
        {
           (void)fprintf(stderr, "Unknown convert format %s\n", argv[1]);
           (void)fprintf(stderr, "Known formats are: sohetx, wmo, sohetxwmo, sohetx2wmo1, sohetx2wmo0\n");
           (void)fprintf(stderr, "                   mrz2wmo, unix2dos, dos2unix, lf2crcrlf and crcrlf2lf\n");
           return(1);
        }

   if (stat(argv[2], &stat_buf) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to stat() %s : %s\n", argv[2], strerror(errno));
      return(1);
   }
   file_size = stat_buf.st_size;
   if (convert(".", argv[2], type, &file_size) != SUCCESS)
   {
      (void)fprintf(stderr, "Failed to convert %s\n", argv[2]);
      return(1);
   }

   return(0);
}

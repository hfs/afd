/*
 *  test_time.c - Part of AFD, an automatic file distribution program.
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
 **   test_time -
 **
 ** SYNOPSIS
 **   test_time <crontab like time entry>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.05.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "amgdefs.h"

/* Global variables */
int  sys_log_fd = STDERR_FILENO;
char *p_work_dir = NULL;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ test_time() $$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   time_t               next_time;
   struct bd_time_entry te;

   if (argc != 2)
   {
      (void)fprintf(stderr, "Usage: %s <crontab like time entry>\n",
                    argv[0]);
      exit(INCORRECT);
   }

   if (eval_time_str(argv[1], &te) == INCORRECT)
   {
      exit(INCORRECT);
   }

   next_time = calc_next_time(&te);

   (void)fprintf(stdout, "%s", ctime(&next_time));

   exit(SUCCESS);
}

/*
 *  test_in_time.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   test_in_time -
 **
 ** SYNOPSIS
 **   test_in_time <crontab like time entry> [<unix time>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.05.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "amgdefs.h"

/* Global variables */
int sys_log_fd = STDERR_FILENO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ test_in_time() $$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   time_t               current_time;
   struct bd_time_entry te;

   if ((argc != 2) && (argc != 3))
   {
      (void)fprintf(stderr, "Usage: %s <crontab like time entry> [<unix time>]\n",
                    argv[0]);
      exit(INCORRECT);
   }
   if (argc == 2)
   {
      current_time = time(NULL);
   }
   else
   {
      current_time = atol(argv[2]);
   }

   if (eval_time_str(argv[1], &te) == INCORRECT)
   {
      exit(INCORRECT);
   }
   if (in_time(current_time, &te) == YES)
   {
      (void)fprintf(stdout, "IS in time : %s", ctime(&current_time));
   }
   else
   {
      (void)fprintf(stdout, "IS NOT in time : %s", ctime(&current_time));
   }

   exit(SUCCESS);
}

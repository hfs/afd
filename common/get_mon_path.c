/*
 *  get_mon_path.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 1999 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_mon_path - gets the working directory of the AFD or the
 **                  afd_monitor process
 **
 ** SYNOPSIS
 **   int get_mon_path(int *argc, char *argv[], char *work_dir)
 **
 ** DESCRIPTION
 **   The function get_mon_path() gets the working directory of
 **   mafd process or the AFD. It does this by first looking if
 **   the working directory is not passed on with the '-w' parameter
 **   in 'argv'. If this is not the case it tries to get the work-
 **   ing directory from the environment variable MON_WD_ENV_NAME
 **   (MON_WORK_DIR) and then WD_ENV_NAME (AFD_WORK_DIR). If these
 **   are not set it will test if HOME is set and will then use $HOME/.afd
 **   as the working directory. If this too should fail it sets the
 **   working directory to DEFAULT_AFD_DIR. In all three cases it
 **   checks if the directory exists and if not it tries to create
 **   the directory.
 **
 ** RETURN VALUES
 **   SUCCESS and the working directory of the mafd process
 **   or the AFD in 'work_dir', if it finds a valid directory.
 *+   Otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.09.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>             /* NULL                                   */
#include <string.h>            /* strcmp(), strcpy(), strcat()           */
#include <stdlib.h>            /* getenv()                               */
#include <unistd.h>            /* R_OK, X_OK                             */
#include "mondefs.h"


/*########################### get_mon_path() ############################*/
int
get_mon_path(int *argc, char *argv[], char *work_dir)
{
   char *ptr;

   ptr = work_dir;

   /* See if directory is passed as argument */
   if ((*argc > 2) && (strcmp(argv[1], WORK_DIR_ID) == 0))
   {
      (void)strcpy(work_dir, argv[2]);
      if (*argc > 3)
      {
         register int i;

         for (i = 1; i < (*argc - 2); i++)
         {
            argv[i] = argv[i + 2];
         }
      }
      else
      {
         argv[1] = NULL;
      }
      *argc -= 2;
   }
        /* Check if the environment variable is set */
   else if ((ptr = getenv(MON_WD_ENV_NAME)) != NULL)
        {
           (void)strcpy(work_dir, ptr);
        }
        else if ((ptr = getenv(WD_ENV_NAME)) != NULL)
             {
                (void)strcpy(work_dir, ptr);
             }
             else
             {
                (void)fprintf(stderr,
                              "ERROR   : Failed to determine woking directory. (%s %d)\n",
                              __FILE__, __LINE__);
                return(INCORRECT);
             }

   if (check_dir(work_dir, R_OK | X_OK) == SUCCESS)
   {
      return(SUCCESS);
   }

   return(INCORRECT);
}

/*
 *  eval_input_as.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Deutscher Wetterdienst (DWD),
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
 **   eval_input_as - checks syntax of input for process afd_stat
 **
 ** SYNOPSIS
 **   void eval_input_as(int  argc,
 **                      char *argv[],
 **                      char *work_dir,
 **                      char *statistic_file_name)
 **
 ** DESCRIPTION
 **   This module checks whether the syntax is correct. So far it
 **   accepts the following parameters:
 **           -w <working directory> working directory of the AFD
 **           -f <statistic file>    path and name of the statistic file
 **
 ** RETURN VALUES
 **   If any of the above parameters have been specified it returns
 **   them in the appropriate variable (work_dir, statistic_file_name,
 **   show_century, show_day, show_year, host_counter, hosts).
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.10.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                        /* stderr, fprintf()           */
#include <stdlib.h>                       /* exit(), atoi()              */
#include <string.h>                       /* strcpy()                    */
#include "statdefs.h"

/* Global variables */
extern int  other_file;

/* local functions */
static void usage(void);


/*########################## eval_input_as() ############################*/
void
eval_input_as(int  argc,
              char *argv[],
              char *work_dir,
              char *statistic_file_name)
{
   int correct = YES;       /* Was input/syntax correct?      */

   /**********************************************************/
   /* The following while loop checks for the parameters:    */
   /*                                                        */
   /*         - w : Working directory of the AFD.            */
   /*                                                        */
   /*         - f : Path and name of the statistic file.     */
   /*                                                        */
   /**********************************************************/
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      switch(*(argv[0] + 1))
      {
         case 'f': /* Name of the statistic file */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             "ERROR  : You did not specify the name of the statistics file.\n");
               correct = NO;
            }
            else
            {
               while ((--argc > 0) && ((*++argv)[0] != '-'))
               {
                  (void)strcpy(statistic_file_name, argv[0]);
               }
               other_file = YES;
               argv--;
               argc++;
            }
            break;

         case 'w': /* Working directory of the AFD */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             "ERROR  : You did not specify the working directory of the AFD.\n");
               correct = NO;
            }
            else
            {
               while ((--argc > 0) && ((*++argv)[0] != '-'))
               {
                  (void)strcpy(work_dir, argv[0]);
               }
               argv--;
               argc++;
            }
            break;

         default : /* Unknown parameter */

            (void)fprintf(stderr, "ERROR  : Unknown parameter %c. (%s %d)\n",
                          *(argv[0] + 1), __FILE__, __LINE__);
            correct = NO;
            break;

      } /* switch(*(argv[0] + 1)) */
   }

   /* If input is not correct show syntax */
   if (correct == NO)
   {
      usage();
      exit(0);
   }

   return;
} /* eval_input_stat() */


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(void)
{
   (void)fprintf(stderr,
                 "SYNTAX  : afd_stat [options] [hostname 1 ....]\n");
   (void)fprintf(stderr,
                 "            -w <work dir> Working directory of the AFD.\n");
   (void)fprintf(stderr,
                 "            -f <name>     Path and name of the statistics file.\n");
   (void)fprintf(stderr,
                 "            --version     Show current version.\n");

   return;
}

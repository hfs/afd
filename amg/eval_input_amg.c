/*
 *  eval_input_amg.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2004 Deutscher Wetterdienst (DWD),
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
 **   eval_input_amg - checks syntax of input for process amg
 **
 ** SYNOPSIS
 **   void eval_input_amg(int  argc,
 **                       char *argv[],
 **                       char *work_dir,
 **                       char *dir_config_file,
 **                       char *host_config_file,
 **                       int  *rescan_time)
 **
 ** DESCRIPTION
 **   This module checks whether the syntax is correct. So far it
 **   accepts the following parameters:
 **           -d <dir config name>   path and name of the DIR_CONFIG
 **           -h <host config name>  path and name of the HOST_CONFIG
 **           -r <rescan time>       time when user directories should
 **                                  be resend
 **           -w <path>              AFD working directory
 **
 ** RETURN VALUES
 **   If any of the above parameters have been specified it returns
 **   them in the appropriate variable (dir_config_file, max_no_proc, rescan_time,
 **   work_dir).
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.08.1995 H.Kiehl Created
 **   16.12.1997 H.Kiehl Added HOST_CONFIG file.
 **
 */
DESCR__E_M3

#include <stdio.h>                        /* stderr, fprintf()         */
#include <stdlib.h>                       /* exit(), atoi()            */
#include <string.h>                       /* strcpy()                  */
#include "amgdefs.h"

/* local functions */
static void usage(void);


/*########################## eval_input_amg() ##########################*/
void
eval_input_amg(int  argc,
               char *argv[],
               char *work_dir,
               char *dir_config_file,
               char *host_config_file,
               int  *rescan_time)
{
   int   correct = YES;       /* Was input/syntax correct?      */

   /**********************************************************/
   /* The following while loop checks for the parameters:    */
   /*                                                        */
   /*         -d : Path and name of the DIR_CONFIG file.     */
   /*                                                        */
   /*         -h : Path and name of the HOST_CONFIG file.    */
   /*                                                        */
   /*         -r : The time in seconds, when to check for    */
   /*               a change in the database.                */
   /*                                                        */
   /*         -w : The working (home) directory of the AMG.  */
   /*                                                        */
   /**********************************************************/
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      switch(*(argv[0] + 1))
      {
         case 'd': /* DIR_CONFIG File */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, "ERROR  : You did not specify the name of the DIR_CONFIG file.\n");
               correct = NO;
            }
            else
            {
               while ((--argc > 0) && ((*++argv)[0] != '-'))
               {
                  (void)strcpy(dir_config_file, argv[0]);
               }
               argv--;
               argc++;
            }
            break;

         case 'h': /* HOST_CONFIG File */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, "ERROR  : You did not specify the name of the HOST_CONFIG file.\n");
               correct = NO;
            }
            else
            {
               while ((--argc > 0) && ((*++argv)[0] != '-'))
               {
                  (void)strcpy(host_config_file, argv[0]);
               }
               argv--;
               argc++;
            }
            break;


         case 'r': /* Rescan time */

            if (argc == 1 || *(argv + 1)[0] == '-')
            {
               (void)fprintf(stderr, "ERROR  : You did not specify the rescan time.\n");
               correct = NO;
            }
            else
            {
               *rescan_time = atoi(*(argv + 1));
               argc--;
               argv++;
            }
            break;

         case 'w': /* Working Directory */

            if (argc == 1 || *(argv + 1)[0] == '-')
            {
               (void)fprintf(stderr, "ERROR  : You did not specify the working (home) directory.\n");
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
                          *(argv[0]+1), __FILE__, __LINE__);
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
} /* eval_input_amg() */


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(void)
{
   (void)fprintf(stderr, "SYNTAX  : amg\n          -d         Path and name of DIR_CONFIG file.\n");
   (void)fprintf(stderr, "          -h         Path and name of HOST_CONFIG file.\n");
   (void)fprintf(stderr, "          -r         The time in seconds, when to check for change in database.\n");
   (void)fprintf(stderr, "          %s         Path of the working (home) directory.\n", WORK_DIR_ID);
   (void)fprintf(stderr, "          --version  Show current version.\n");

   return;
}

/*
 *  eval_input_ss.c - Part of AFD, an automatic file distribution program.
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
 **   eval_input_ss - checks syntax of input for process show_stat
 **
 ** SYNOPSIS
 **   void eval_input_ss(int  argc,
 **                      char *argv[],
 **                      char *status_file_name,
 **                      int  *show_day,
 **                      int  *show_day_summary,
 **                      int  *show_hour,
 **                      int  *show_hour_summary,
 **                      int  *show_min_summary,
 **                      int  *show_year,
 **                      int  *host_counter)
 **
 ** DESCRIPTION
 **   This module checks whether the syntax is correct. So far it
 **   accepts the following parameters:
 **           -f <statistic file>    path and name of the statistic file
 **           -d <x>                 show all information of day minus x
 **           -D                     show total summary on a per day basis
 **           -h <x>                 show all information of hour minus x
 **           -H                     show total summary of last 24 hours
 **           -M [<x>]               show total summary of last hour
 **           -y <x>                 show all information of year minus x
 **           -w <working directory> working directory of the AFD
 **
 ** RETURN VALUES
 **   If any of the above parameters have been specified it returns
 **   them in the appropriate variable (work_dir, status_file_name,
 **   show_day, show_year, host_counter).
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.09.1996 H.Kiehl Created
 **   19.04.1998 H.Kiehl Added -D option.
 **   26.05.1998 H.Kiehl Added -H option.
 **   09.08.2002 H.Kiehl Added -M option.
 **
 */
DESCR__E_M3

#include <stdio.h>                        /* stderr, fprintf()           */
#include <stdlib.h>                       /* exit(), atoi()              */
#include <string.h>                       /* strcpy()                    */
#include <ctype.h>                        /* isdigit()                   */
#include <unistd.h>                       /* STDERR_FILENO               */
#include <errno.h>                        /* RT_ARRAY                    */
#include "statdefs.h"

/* Global variables */
extern char **hosts;
extern int  sys_log_fd;                   /* Used by macro RT_ARRAY      */

/* local functions */
static void usage(void);


/*########################## eval_input_ss() ############################*/
void
eval_input_ss(int  argc,
              char *argv[],
              char *work_dir,
              char *status_file_name,
              int  *show_day,
              int  *show_day_summary,
              int  *show_hour,
              int  *show_hour_summary,
              int  *show_min_summary,
              int  *show_year,
              int  *host_counter)
{
   int i,
       correct = YES;       /* Was input/syntax correct?      */

   /**********************************************************/
   /* The following while loop checks for the parameters:    */
   /*                                                        */
   /*         - f : Path and name of the statistic file.     */
   /*                                                        */
   /*         - d : Show all information of day minus x.     */
   /*                                                        */
   /*         - D : Show total summary on a per day basis.   */
   /*                                                        */
   /*         - h : Show all information of hour minus x.    */
   /*                                                        */
   /*         - H : Show total summary of last 24 hours.     */
   /*                                                        */
   /*         - M : Show total summary of last hour.         */
   /*                                                        */
   /*         - y : Show all information of year minus x.    */
   /*                                                        */
   /*         - w : Working directory of the AFD.            */
   /*                                                        */
   /**********************************************************/
   if ((argc > 1) && (argv[1][0] == '-'))
   {
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
                     (void)strcpy(status_file_name, argv[0]);
                  }
                  argv--;
                  argc++;
               }
               break;

            case 'd': /* Show day minus x */

               if ((argc == 1) || (*(argv + 1)[0] == '-') ||
                   (isdigit(*(argv + 1)[0]) == 0))
               {
                  *show_day = 0;
               }
               else
               {
                  *show_day = atoi(*(argv + 1));
                  argc--;
                  argv++;
               }
               break;

            case 'D': /* Show summary information on a per day basis. */

               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  *show_day_summary = 0;
                  argc--;
                  argv++;
               }
               else
               {
                  (void)fprintf(stderr,
                                "ERROR  : Can only show summary on a per day basis.\n");
                  correct = NO;
                  argc--;
                  argv++;
               }
               break;

            case 'h': /* Show hour minus x */

               if ((argc == 1) || (*(argv + 1)[0] == '-') ||
                   (isdigit(*(argv + 1)[0]) == 0))
               {
                  *show_hour = 0;
               }
               else
               {
                  *show_hour = atoi(*(argv + 1));
                  argc--;
                  argv++;
               }
               break;

            case 'H': /* Show summary information of last 24 hours. */

               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  *show_hour_summary = 0;
                  argc--;
                  argv++;
               }
               else
               {
                  (void)fprintf(stderr,
                                "ERROR  : Can only show summary of last 24 hours.\n");
                  correct = NO;
                  argc--;
                  argv++;
               }
               break;

            case 'M': /* Show summary information of last hour. */

               if ((argc == 1) || (*(argv + 1)[0] == '-') ||
                   (isdigit(*(argv + 1)[0]) == 0))
               {
                  *show_min_summary = 0;
                  argc--;
                  argv++;
               }
               else
               {
                  *show_min_summary = atoi(*(argv + 1));
                  argc--;
                  argv++;
               }
               break;

            case 'y': /* Show year minus x */

               if ((argc == 1) || (*(argv + 1)[0] == '-') ||
                   (isdigit(*(argv + 1)[0]) == 0))
               {
                  *show_year = 0;
               }
               else
               {
                  *show_year = atoi(*(argv + 1));
                  argc--;
                  argv++;
               }
               break;

            default : /* Unknown parameter */

               (void)fprintf(stderr,
                             "ERROR  : Unknown parameter %c. (%s %d)\n",
                             *(argv[0]+1), __FILE__, __LINE__);
               correct = NO;
               break;

         } /* switch(*(argv[0] + 1)) */
      }
   }
   else
   {
      argc--; argv++;
   }

   /*
    * Now lets collect all the host names and store them somewhere
    * save and snug ;-)
    */
   if (argc > 0)
   {
      *host_counter = argc;
      RT_ARRAY(hosts, argc, MAX_FILENAME_LENGTH, char);
      for (i = 0; i < argc; i++)
      {
         t_hostname(argv[0], hosts[i]);
         argv++;
      }
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
   (void)fprintf(stderr, "SYNTAX  : show_stat [options] [hostname 1 ....]\n");
   (void)fprintf(stderr, "            -w <work dir>   Working directory of the AFD.\n");
   (void)fprintf(stderr, "            -f <name>       Path and name of the statistics file.\n");
   (void)fprintf(stderr, "            -d [<x>]        Show information of all days [or day minus x].\n");
   (void)fprintf(stderr, "            -D              Show total summary on a per day basis.\n");
   (void)fprintf(stderr, "            -h [<x>]        Show information of all hours [or hour minus x].\n");
   (void)fprintf(stderr, "            -H              Show total summary of last 24 hours.\n");
   (void)fprintf(stderr, "            -M              Show total summary of last hour.\n");
   (void)fprintf(stderr, "            -y [<x>]        Show information of all years [or year minus x].\n");
   (void)fprintf(stderr, "            --version       Show current version.\n");

   return;
}

/*
 *  eval_input_alda.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   eval_input_alda - checks syntax of input for process alda
 **
 ** SYNOPSIS
 **   void eval_input_alda(int argc, char *argv[])
 **
 ** DESCRIPTION
 **   This module checks whether the syntax is correct. The syntax is as
 **   follows:
 **           alda [options] <file name pattern>
 **
 **   Mode options
 **           -l                        local log data
 **           -r                        remote log data
 **           -b                        back trace data
 **           -f                        forward trace data
 **   Parameters
 **           -s <AFD host name/alias>  Starting host/AFD.
 **           -e <AFD host name/alias>  Ending host/AFD.
 **           -t <start>[-<end>]        Time frame at starting point.
 **           -T <start>[-<end>]        Time frame at end point.
 **           -c                        Continous
 **           -o <format>               Specifies the output format.
 **           -d <directory name/alias> Directory name or alias.
 **           -h <host name/alias>      Host name or alias.
 **           -j <job ID>               Job identifier.
 **           -S <size>                 File size in byte.
 **           -L <log type>             Search only in given log type.
 **           -w <work dir>             working directory of the AFD
 **
 ** RETURN VALUES
 **   If any of the above parameters have been specified it returns
 **   them in the appropriate variable.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.04.2007 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                        /* stderr, fprintf()           */
#include <stdlib.h>                       /* exit()                      */
#include "logdefs.h"


/* External global variables. */
extern int  mode;
extern char search_pattern[];

/* Local function prototypes. */
static void usage(char *);


/*########################## eval_input_alda() ##########################*/
void
eval_input_alda(int argc, char *argv[])
{
   char *progname;

   progname = argv[0];
   if ((argc > 1) && (argv[1][0] == '-'))
   {
      while ((--argc > 0) && ((*++argv)[0] == '-'))
      {
         switch (*(argv[0] + 1))
         {
            case 'l' : /* Local mode. */
               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  mode |= ALDA_LOCAL_MODE;
               }
               else
               {
                  (void)fprintf(stderr,
                                "ERROR  : Cannot specify any additional parameter with -l option.");
                  correct = NO;
                  argc--;
                  argv++;
               }
               break;

            case 'r' : /* Remote mode. */
               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  mode |= ALDA_REMOTE_MODE;
               }
               else
               {
                  (void)fprintf(stderr,
                                "ERROR  : Cannot specify any additional parameter with -r option.");
                  correct = NO;
                  argc--;
                  argv++;
               }
               break;

            case 'b' : /* Back trace data. */
               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  mode |= ALDA_BACKWARD_MODE;
               }
               else
               {
                  (void)fprintf(stderr,
                                "ERROR  : Cannot specify any additional parameter with -b option.");
                  correct = NO;
                  argc--;
                  argv++;
               }
               break;

            case 'f' : /* Forward trace data. */
               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  mode |= ALDA_FORWAR_MODE;
               }
               else
               {
                  (void)fprintf(stderr,
                                "ERROR  : Cannot specify any additional parameter with -f option.");
                  correct = NO;
                  argc--;
                  argv++;
               }
               break;

            default : /* Unknown parameter. */
               (void)fprintf(stderr,
                             "ERROR  : Unknown parameter %c. (%s %d)\n",
                             *(argv[0] + 1), __FILE__, __LINE__);
               correct = NO;
               break,
         }
      }
   }

   if (argc == 1)
   {
      (void)my_strncpy(search_pattern, argv[1], MAX_FILENAME_LENGTH);
   }
   else
   {
      (void)fprintf(stderr, "ERROR  : No file name pattern specified.\n");
      correct = NO;
   }
   if (correct == NO)
   {
      usage(progname);
      exit(INCORRECT);
   }

   return;
}


/*+++++++++++++++++++++++++++++ usage() +++++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "Usage: %s [options] <file name pattern>\n");

   return;
}

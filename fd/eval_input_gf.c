/*
 *  eval_input_gf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   eval_input_gf - checks syntax of input for process gf_xxx
 **
 ** SYNOPSIS
 **   int eval_input_gf(int        argc,
 **                     char       *argv[],
 **                     struct job *p_db)
 **
 ** DESCRIPTION
 **   This module checks whether the syntax of gf_xxx is correct.
 **   Input value : int  argc          - number of arguments
 **                 char *argv[]       - pointer array with arguments
 **
 ** RETURN VALUES
 **   struct job *p_db   -
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* stderr, fprintf()                  */
#include <stdlib.h>                /* exit(), atoi()                     */
#include <string.h>                /* strcpy(), strerror(), strcmp()     */
#include <ctype.h>                 /* isdigit()                          */
#include "fddefs.h"

/* Global variables */
extern int                        sys_log_fd;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;

/* local functions */
static void                       usage(char *);


/*########################### eval_input_gf() ###########################*/
int
eval_input_gf(int        argc,
              char       *argv[],
              struct job *p_db)
{
   char name[30];                     /* Storage for program name.      */
   int  correct = YES;                /* Was input/syntax correct?      */

   (void)strcpy(name, argv[0]);

   /* Evaluate all arguments with '-' */
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      switch (*(argv[0] + 1))
      {
         case 'd' : /* Directory alias (only gf_xxx). */
             if ((argc == 1) || (*(argv + 1)[0] == '-'))
             {
                (void)fprintf(stderr, "ERROR   : You did not specify the directory alias.\n");
                correct = NO;
             }
             else
             {
                argv++;
                if (strlen(argv[0]) > MAX_DIR_ALIAS_LENGTH)
                {
                   (void)fprintf(stderr, "ERROR   : Directory alias longer then %d bytes.\n",
                                 MAX_DIR_ALIAS_LENGTH);
                   correct = NO;
                }
                else
                {
                   (void)strcpy(p_db->dir_alias, argv[0]);
                }
                argc--;
             }
             break;

         case 'j' : /* Job Number. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, "ERROR   : You did not specify the Job Number.\n");
               correct = NO;
            }
            else
            {
               if (isdigit(*(argv + 1)[0]) == 0)
               {
                  p_db->job_no = -1;
               }
               else
               {
                  p_db->job_no = (char)atoi(*(argv + 1));
               }
               argc--;
               argv++;
            }
            break;

         case 't' : /* Toggle host */
            p_db->toggle_host = YES;
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
      usage(name);
      return(-SYNTAX_ERROR);
   }

   return(SUCCESS);
} /* eval_input_gf() */


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *name)
{
   (void)fprintf(stderr, "SYNTAX: %s [options] -m message\n\n", name);
   (void)fprintf(stderr, "OPTIONS                 DESCRIPTION\n");
   (void)fprintf(stderr, "  --version           - Show current version\n");
   (void)fprintf(stderr, "  -d <dir alias>      - Directory alias.\n");
   (void)fprintf(stderr, "  -j <process number> - the process number under which this job is to be displayed.\n");
   (void)fprintf(stderr, "  -t                  - use other host\n");
   (void)fprintf(stderr, "  %s                  - the working directory of the\n", WORK_DIR_ID);
   (void)fprintf(stderr, "                        AFD.\n");

   return;
}

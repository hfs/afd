/*
 *  eval_input_sf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2000 Deutscher Wetterdienst (DWD),
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
 **   eval_input_sf - checks syntax of input for process sf_xxx
 **
 ** SYNOPSIS
 **   int eval_input_sf(int        argc,
 **                      char       *argv[],
 **                      struct job *p_db)
 **
 ** DESCRIPTION
 **   This module checks whether the syntax of sf_xxx is correct.
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
 **   02.11.1995 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* stderr, fprintf()                  */
#include <stdlib.h>                /* exit(), atoi()                     */
#include <string.h>                /* strcpy(), strerror(), strcmp()     */
#include <ctype.h>                 /* isdigit()                          */
#include "fddefs.h"

/* Global variables */
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;

/* local functions */
static void                       usage(char *);


/*########################### eval_input_sf() ###########################*/
int
eval_input_sf(int        argc,
              char       *argv[],
              struct job *p_db)
{
   char *ptr = NULL,
        message_flag = NO,            /* Message name specified?        */
        name[30];                     /* Storage for program name.      */
   int  correct = YES;                /* Was input/syntax correct?      */

   (void)strcpy(name, argv[0]);

   /* Evaluate all arguments with '-' */
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      switch (*(argv[0] + 1))
      {
         case 'f' : /* Error message file. */
            p_db->error_file = YES;
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

         case 'm' : /* The name of the message that holds the options. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, "ERROR   : You did not specify how many times the transfer should be repeated when an error occurs.\n");
               correct = NO;
            }
            else
            {
               argv++;

               if (message_flag == YES)
               {
                  (void)fprintf(stderr, "ERROR   : You already specified a message/hostname string.\n");
                  correct = NO;
               }
               else
               {
                  char fullname[MAX_PATH_LENGTH];

                  if (fsa_attach() < 0)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to attach to FSA.");
                     return(-SYNTAX_ERROR);
                  }

                  ptr = argv[0] + strlen(argv[0]);
                  while ((*ptr != '_') && (ptr != argv[0]))
                  {
                     ptr--;
                  }
                  if (*ptr != '_')
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to locate job ID in message name %s",
                                argv[0]);
                     return(-JID_NUMBER_ERROR);
                  }
                  ptr++;
                  p_db->job_id = atoi(ptr);
                  (void)sprintf(fullname, "%s%s/%s",
                                p_work_dir, AFD_MSG_DIR, ptr);
                  if (eval_message(fullname, p_db) < 0)
                  {
                     return(-SYNTAX_ERROR);
                  }
                  else
                  {
                     /* Store message name in database structure */
                     (void)strcpy(p_db->msg_name, argv[0]);
                  }
                  message_flag = YES;
               }
               argc--;
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

   /* If no message was specified, we assume the next string */
   /* gives us all the necessary information about the       */
   /* recipient and the files to be send.                    */
   if (message_flag == NO)
   {
      (void)fprintf(stderr,
                    "ERROR   : No message was supplied with the -m option.\n");
      correct = NO;
   }

   /* If input is not correct show syntax */
   if (correct == NO)
   {
      usage(name);
      if (message_flag == YES)
      {
         return(-SYNTAX_ERROR);
      }
      exit(SYNTAX_ERROR);
   }

   return(SUCCESS);
} /* eval_input_sf() */


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *name)
{
   (void)fprintf(stderr, "SYNTAX: %s [options] -m message\n\n", name);
   (void)fprintf(stderr, "OPTIONS                 DESCRIPTION\n");
   (void)fprintf(stderr, "  --version           - Show current version\n");
   (void)fprintf(stderr, "  -f                  - error message\n");
   (void)fprintf(stderr, "  -j <process number> - the process number under which this job is to be displayed.\n");
   (void)fprintf(stderr, "  -m <message name>   - Message name.\n");
   (void)fprintf(stderr, "  -t                  - use other host\n");
   (void)fprintf(stderr, "  %s                  - the working directory of the\n", WORK_DIR_ID);
   (void)fprintf(stderr, "                        AFD.\n");

   return;
}

/*
 *  eval_input_gf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   int eval_input_gf(int argc, char *argv[], struct job *p_db)
 **
 ** DESCRIPTION
 **   This function evaluates the parameters given to the process
 **   gf_xxx which may have the following format:
 **
 **    gf_xxx <work dir> <job no.> <FSA id> <FSA pos> <dir alias> [options]
 **        OPTIONS
 **          -o               Old/Error message.
 **          -t               Temp toggle.
 **
 ** RETURN VALUES
 **   struct job *p_db   -
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.08.2000 H.Kiehl Created
 **   12.04.2003 H.Kiehl Rewrite to accomodate to new syntax.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* stderr, fprintf()                  */
#include <stdlib.h>                /* exit(), atoi()                     */
#include <string.h>                /* strcpy(), strerror(), strcmp()     */
#include <ctype.h>                 /* isdigit()                          */
#include "fddefs.h"

/* Global variables */
extern int                        fsa_id;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;

/* local functions */
static void                       usage(char *);


/*########################### eval_input_gf() ###########################*/
int
eval_input_gf(int argc, char *argv[], struct job *p_db)
{
   int ret = SUCCESS;

   if (argc < 6)
   {
      usage(argv[0]);
      ret = SYNTAX_ERROR;
   }
   else
   {
      STRNCPY(p_work_dir, argv[1], MAX_PATH_LENGTH);
      if (isdigit(argv[2][0]) == 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : None nummeric value for job number : %s.\n",
                       argv[2]);
         usage(argv[0]);
         ret = SYNTAX_ERROR;
      }
      else
      {
         int i;

         p_db->job_no = (char)atoi(argv[2]);

         /* Check if FSA ID is correct. */
         i = 0;
         do
         {
            if (isdigit(argv[3][i]) == 0)
            {
               i = MAX_INT_LENGTH;
            }
            i++;
         } while ((argv[3][i] != '\0') && (i < MAX_INT_LENGTH));
         if ((i > 0) && (i < MAX_INT_LENGTH))
         {
            fsa_id = atoi(argv[3]);

            /* Check if FSA position is correct. */
            i = 0;
            do
            {
               if (isdigit(argv[4][i]) == 0)
               {
                  i = MAX_INT_LENGTH;
               }
               i++;
            } while ((argv[4][i] != '\0') && (i < MAX_INT_LENGTH));
            if ((i > 0) && (i < MAX_INT_LENGTH))
            {
               p_db->fsa_pos = atoi(argv[4]);

               if (strlen(argv[5]) <= MAX_DIR_ALIAS_LENGTH)
               {
                  (void)strcpy(p_db->dir_alias, argv[5]);
                  if (fsa_attach_pos(p_db->fsa_pos) == SUCCESS)
                  {
                     /*
                      * Now lets evaluate the options.
                      */
                     for (i = 6; i < argc; i++)
                     {
                        if (argv[i][0] == '-')
                        {
                           switch (argv[i][1])
                           {
                              case 'o' : /* This is an old/erro job. */
                                 p_db->special_flag |= OLD_ERROR_JOB;
                                 break;
                              case 't' : /* Toggle host */
                                 p_db->toggle_host = YES;
                                 break;
                              default  : /* Unknown parameter */
                                 (void)fprintf(stderr,
                                               "ERROR  : Unknown parameter %c. (%s %d)\n",
                                               argv[i][1], __FILE__, __LINE__);
                                 break;
                           }
                        }
                     }
                  }
                  else
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to attach to FSA.");
                     ret = SYNTAX_ERROR;
                  }
               }
               else
               {
                  (void)fprintf(stderr,
                                "ERROR   : Directory alias longer then %d bytes.\n",
                                MAX_DIR_ALIAS_LENGTH);
                  usage(argv[0]);
                  ret = SYNTAX_ERROR;
               }
            }
            else
            {
               (void)fprintf(stderr,
                             "ERROR   : Wrong value for FSA position : %s.\n",
                             argv[4]);
               usage(argv[0]);
               ret = SYNTAX_ERROR;
            }
         }
         else
         {
            (void)fprintf(stderr,
                          "ERROR   : Wrong value for FSA ID : %s.\n", argv[3]);
            usage(argv[0]);
            ret = SYNTAX_ERROR;
         }
      }
   }
   if (ret != SUCCESS)
   {
      ret = -ret;
   }
   return(ret);
} /* eval_input_gf() */


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *name)
{
   (void)fprintf(stderr, "SYNTAX: %s <work dir> <job no.> <FSA id> <FSA pos> <dir alias> [options]\n\n", name);
   (void)fprintf(stderr, "OPTIONS                 DESCRIPTION\n");
   (void)fprintf(stderr, "  --version           - Show current version\n");
   (void)fprintf(stderr, "  -o                  - old/error message\n");
   (void)fprintf(stderr, "  -t                  - use other host\n");

   return;
}

/*
 *  eval_input_sf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2005 Deutscher Wetterdienst (DWD),
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
 **   int eval_input_sf(int argc, char *argv[], struct job *p_db)
 **
 ** DESCRIPTION
 **   This function evaluates the parameters given to the process
 **   sf_xxx which may have the following format:
 **
 **    sf_xxx <work dir> <job no.> <FSA id> <FSA pos> <msg name> [options]
 **
 **          -a <age limit>   The age limit for the files being send.
 **          -A               Disable archiving of files.
 **          -o               Old/Error message.
 **          -r               Resend from archive (job from show_olog).
 **          -t               Temp toggle.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when it successfully decoded the parameters.
 **   SYNTAX_ERROR is returned when it thinks one of the parameter
 **   is wrong or it did not manage to attach to the FSA. JID_NUMBER_ERROR
 **   is returned when it failed to determine the job id number.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.11.1995 H.Kiehl Created
 **   30.10.2002 H.Kiehl Added -r option.
 **   12.11.2002 H.Kiehl Option -a to supply the default age-limit.
 **   19.03.2003 H.Kiehl Added -A to disable archiving.
 **   21.03.2003 H.Kiehl Rewrite to accomodate to new syntax.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* stderr, fprintf()                  */
#include <stdlib.h>                /* exit(), strtoul()                  */
#include <string.h>                /* strerror()                         */
#include <ctype.h>                 /* isdigit()                          */
#include "fddefs.h"

/* Global variables */
extern int                        fsa_id,
                                  *p_no_of_hosts;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;

/* local functions */
static void                       usage(char *);


/*########################### eval_input_sf() ###########################*/
int
eval_input_sf(int argc, char *argv[], struct job *p_db)
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

         p_db->job_no = (char)strtol(argv[2], (char **)NULL, 10);

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
            fsa_id = (int)strtol(argv[3], (char **)NULL, 10);

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
               int msg_length;

               p_db->fsa_pos = (int)strtol(argv[4], (char **)NULL, 10);

               /* Check if the supplied message name is correct. */
               i = 0;
               do
               {
                  if ((isxdigit(argv[5][i])) || (argv[5][i] == '_') ||
                      (argv[5][i] == '/'))
                  {
                     p_db->msg_name[i] = argv[5][i];
                  }
                  else
                  {
                     i = MAX_MSG_NAME_LENGTH;
                  }
                  i++;
               } while ((argv[5][i] != '\0') && (i < MAX_MSG_NAME_LENGTH));
               msg_length = i;
               if ((i > 0) && (i < MAX_MSG_NAME_LENGTH))
               {
                  char *ptr;

                  p_db->msg_name[msg_length] = '\0';
                  ptr = p_db->msg_name;
                  while ((*ptr != '/') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  if (*ptr != '/')
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to locate job ID in message name %s",
                                argv[5]);
                     ret = JID_NUMBER_ERROR;
                  }
                  else
                  {
                     *ptr = '\0';
                     p_db->job_id = (unsigned int)strtoul(p_db->msg_name, (char **)NULL, 16);
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
                                 case 'a' : /* Default age-limit. */
                                    if (((i + 1) < argc) &&
                                        (argv[i + 1][0] != '-'))
                                    {
                                       int k = 0;

                                       i++;
                                       do
                                       {
                                          if (isdigit(argv[i][k]) == 0)
                                          {
                                             k = MAX_INT_LENGTH;
                                          }
                                          k++;
                                       } while ((argv[i][k] != '\0') &&
                                                (k < MAX_INT_LENGTH));
                                       if ((k > 0) && (k < MAX_INT_LENGTH))
                                       {
                                          p_db->age_limit = (unsigned int)strtoul(argv[i], (char **)NULL, 10);
                                       }
                                       else
                                       {
                                          (void)fprintf(stderr,
                                                        "ERROR   : Hmm, could not find the age limit for -a option.\n");
                                       }
                                    }
                                    else
                                    {
                                       (void)fprintf(stderr,
                                                     "ERROR   : No age limit specified for -a option.\n");
                                       usage(argv[0]);
                                       ret = SYNTAX_ERROR;
                                    }
                                    break;
                                 case 'A' : /* Archiving is disabled. */
                                    p_db->archive_time = -1;
                                    break;
                                 case 'r' : /* This is a resend from archive. */
                                    p_db->resend = YES;
                                    break;
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
                        if (*(unsigned char *)((char *)p_no_of_hosts + AFD_FEATURE_FLAG_OFFSET) & ENABLE_CREATE_TARGET_DIR)
                        {
                           p_db->special_flag |= CREATE_TARGET_DIR;
                        }
                        if (ret == SUCCESS)
                        {
                           char fullname[MAX_PATH_LENGTH];

                           (void)sprintf(fullname, "%s%s/%s",
                                         p_work_dir, AFD_MSG_DIR,
                                         p_db->msg_name);
                           if (eval_message(fullname, p_db) < 0)
                           {
                              ret = SYNTAX_ERROR;
                           }
                           else
                           {
                              *ptr = '/';
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
               }
               else
               {
                  (void)fprintf(stderr,
                                "ERROR   : Wrong message name : %s.\n",
                                argv[5]);
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
} /* eval_input_sf() */


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *name)
{
   (void)fprintf(stderr,
                 "SYNTAX: %s <work dir> <job no.> <FSA id> <FSA pos> <msg name> [options]\n\n",
                 name);
   (void)fprintf(stderr, "OPTIONS                 DESCRIPTION\n");
   (void)fprintf(stderr, "  --version           - Show current version\n");
   (void)fprintf(stderr, "  -a <age limit>      - set the default age limit in seconds.\n");
   (void)fprintf(stderr, "  -A                  - Archiving is disabled.\n");
   (void)fprintf(stderr, "  -o                  - old/error message\n");
   (void)fprintf(stderr, "  -r                  - Resend from archive.\n");
   (void)fprintf(stderr, "  -t                  - use other host\n");

   return;
}
/*
 *  reset_fsa.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   reset_fsa - 
 **
 ** SYNOPSIS
 **   reset_fsa [-w <working directory>] hostname|position
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.11.1997 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <sys/types.h>
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "version.h"

/* Local functions */
static void                usage(char *);

/* Global variables */
int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fsa_fd = -1,
                           fsa_id,
                           no_of_hosts = 0;
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
char                       *p_work_dir;
struct filetransfer_status *fsa;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ reset_fsa() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  i,
        position = -1;
   char hostname[MAX_HOSTNAME_LENGTH + 1],
        work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc == 2)
   {
      if (isdigit(argv[1][0]) != 0)
      {
         position = atoi(argv[1]);
      }
      else
      {
         t_hostname(argv[1], hostname);
      }
   }
   else
   {
      usage(argv[0]);
      exit(INCORRECT);
   }

   if (fsa_attach() < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to FSA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (position < 0)
   {
      if ((position = get_host_position(fsa, hostname, no_of_hosts)) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not find host %s in FSA. (%s %d)\n",
                       hostname, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   fsa[position].total_file_counter = 0;
   fsa[position].total_file_size = 0;
   for (i = 0; i < fsa[position].allowed_transfers; i++)
   {
      fsa[position].job_status[i].connect_status = DISCONNECT;
   }

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{                    
   (void)fprintf(stderr,
                 "SYNTAX  : %s [-w working directory] hostname|position\n",
                 progname);
   return;                                                                 
}

/*
 *  fra_view.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M1
/*
 ** NAME
 **   fra_view - shows all information in the FRA about a specific
 **              directory
 **
 ** SYNOPSIS
 **   fra_view [--version] [-w <working directory>] dir-alias|position
 **
 ** DESCRIPTION
 **   This program shows all information about a specific directory in
 **   the FRA.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.03.2000 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <time.h>                        /* ctime()                      */
#include <sys/types.h>
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "version.h"

/* Global variables. */
int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fra_id,
                           fra_fd = -1,
                           no_of_dirs = 0;
#ifndef _NO_MMAP
off_t                      fra_size;
#endif
char                       *p_work_dir;
struct fileretrieve_status *fra;

/* Local function prototypes. */
static void usage(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ fra_view() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  i,
        last,
        position = -1;
   char dir_alias[MAX_DIR_ALIAS_LENGTH + 1],
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
         last = position + 1;
      }
      else
      {
         (void)strcpy(dir_alias, argv[1]);
      }
   }
   else if (argc == 1)
        {
           position = -2;
        }
        else
        {
           usage();
           exit(INCORRECT);
        }

   if (fra_attach() < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to FRA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (position == -1)
   {
      if ((position = get_dir_position(fra, dir_alias, no_of_dirs)) < 0)
      {
         (void)fprintf(stderr,
                       "WARNING : Could not find directory %s in FRA. (%s %d)\n",
                       dir_alias, __FILE__, __LINE__);
         exit(INCORRECT);
      }
      last = position + 1;
   }
   else if (position == -2)
        {
           last = no_of_dirs;
           position = 0;
        }
   else if (position >= no_of_dirs)
        {
           (void)fprintf(stderr,
                         "WARNING : There are only %d directories in the FRA. (%s %d)\n",
                         no_of_dirs, __FILE__, __LINE__);
           exit(INCORRECT);
        }

   (void)fprintf(stdout,
                 "            Number of directories: %d   FRA ID: %d\n\n",
                 no_of_dirs, fra_id);
   for (i = position; i < last; i++)
   {
      (void)fprintf(stdout, "=============================> %s <=============================\n",
                    fra[i].dir_alias);
      (void)fprintf(stdout, "Directory alias      : %s\n", fra[i].dir_alias);
      (void)fprintf(stdout, "Directory position   : %d\n", fra[i].dir_pos);
      (void)fprintf(stdout, "URL                  : %s\n", fra[i].url);
      (void)fprintf(stdout, "Host alias           : %s\n", fra[i].host_alias);
      (void)fprintf(stdout, "FSA position         : %d\n", fra[i].fsa_pos);
      (void)fprintf(stdout, "Priority             : %c\n", fra[i].priority);
      (void)fprintf(stdout, "Number of process    : %d\n", fra[i].no_of_process);
      (void)fprintf(stdout, "Bytes received       : %lu\n", fra[i].bytes_received);
      (void)fprintf(stdout, "Files received       : %u\n", fra[i].files_received);
      if (fra[i].dir_status == NORMAL_STATUS)
      {
         (void)fprintf(stdout, "Directory status(%3d): NORMAL_STATUS\n",
                       fra[i].dir_status);
      }
      else if (fra[i].dir_status == DISABLED)
           {
              (void)fprintf(stdout, "Directory status(%3d): DISABLED\n",
                            fra[i].dir_status);
           }
           else
           {
              (void)fprintf(stdout, "Directory status(%3d): UNKNOWN\n",
                            fra[i].dir_status);
           }
      if (fra[i].force_reread == NO)
      {
         (void)fprintf(stdout, "Force reread         : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Force reread         : YES\n");
      }
      if (fra[i].queued == NO)
      {
         (void)fprintf(stdout, "Queued               : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Queued               : YES\n");
      }
      if (fra[i].remove == NO)
      {
         (void)fprintf(stdout, "Remove files         : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Remove files         : YES\n");
      }
      if (fra[i].stupid_mode == NO)
      {
         (void)fprintf(stdout, "Stupid mode          : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Stupid mode          : YES\n");
      }
      (void)fprintf(stdout, "Protocol (%4d)      : ", fra[i].protocol);
      if (fra[i].protocol == FTP)
      {
         (void)fprintf(stdout, "FTP\n");
      }
      else if (fra[i].protocol == LOC)
           {
              (void)fprintf(stdout, "LOC\n");
           }
      else if (fra[i].protocol == SMTP)
           {
              (void)fprintf(stdout, "SMTP\n");
           }
#ifdef _WITH_WMO_SUPPORT
      else if (fra[i].protocol == WMO)
           {
              (void)fprintf(stdout, "WMO\n");
           }
#endif
           else
           {
              (void)fprintf(stdout, "Unknown\n");
           }
      (void)fprintf(stdout, "Old file time (hours): %d\n",
                    fra[i].old_file_time / 3600);
      if (fra[i].delete_unknown_files == NO)
      {
         (void)fprintf(stdout, "Delete unknown files : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Delete unknown files : YES\n");
      }
      if (fra[i].report_unknown_files == NO)
      {
         (void)fprintf(stdout, "Report unknown files : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Report unknown files : YES\n");
      }
      if (fra[i].important_dir == NO)
      {
         (void)fprintf(stdout, "Important directory  : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Important directory  : YES\n");
      }
      if (fra[i].end_character == -1)
      {
         (void)fprintf(stdout, "End character        : NONE\n");
      }
      else
      {
         (void)fprintf(stdout, "End character        : %d\n",
                       fra[i].end_character);
      }
      if (fra[i].time_option == NO)
      {
         (void)fprintf(stdout, "Time option          : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Time option          : YES\n");
         (void)fprintf(stdout, "Next check time      : %s",
                       ctime(&fra[i].next_check_time));
      }
      (void)fprintf(stdout, "Last retrieval       : %s",
                    ctime(&fra[i].last_retrieval));
   }

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(void)
{
   (void)fprintf(stderr,
                 "SYNTAX  : fra_view [--version] [-w working directory] dir-alias|position\n");
   return;
}

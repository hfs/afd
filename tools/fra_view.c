/*
 *  fra_view.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000, 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   20.07.2001 H.Kiehl Show which input files are to be deleted, unknown
 **                      and/or queued.
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
static void convert2bin(char *, unsigned char),
            usage(void);


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
      if (fra[i].delete_files_flag == 0)
      {
         (void)fprintf(stdout, "Delete input files   : NO\n");
      }
      else
      {
         if ((fra[i].delete_files_flag & UNKNOWN_FILES) &&
             (fra[i].delete_files_flag & QUEUED_FILES))
         {
            (void)fprintf(stdout, "Delete input files   : Unknown and queued files\n");
         }
         else
         {
            if (fra[i].delete_files_flag & UNKNOWN_FILES)
            {
               (void)fprintf(stdout, "Delete input files   : Unknown files\n");
            }
            else if (fra[i].delete_files_flag & QUEUED_FILES)
                 {
                    (void)fprintf(stdout, "Delete input files   : Queued files\n");
                 }
                 else
                 {
                    (void)fprintf(stdout, "Delete input files   : ?\n");
                 }
         }
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
         int  byte_order = 1;
         char binstr[72],
              *ptr;

         (void)fprintf(stdout, "Time option          : YES\n");
         (void)fprintf(stdout, "Next check time      : %s",
                       ctime(&fra[i].next_check_time));
#ifdef _WORKING_LONG_LONG
         ptr = (char *)&fra[i].te.minute;
         if (*(char *)&byte_order == 1)
         {
            ptr += 7;
         }
         convert2bin(binstr, *ptr);
         binstr[8] = ' ';
         if (*(char *)&byte_order == 1)
         {
            ptr--;
         }
         else
         {
            ptr++;
         }
         convert2bin(&binstr[9], *ptr);
         binstr[17] = ' ';
         if (*(char *)&byte_order == 1)
         {
            ptr--;
         }
         else
         {
            ptr++;
         }
         convert2bin(&binstr[18], *ptr);
         binstr[26] = ' ';
         if (*(char *)&byte_order == 1)
         {
            ptr--;
         }
         else
         {
            ptr++;
         }
         convert2bin(&binstr[27], *ptr);
         binstr[35] = ' ';
         if (*(char *)&byte_order == 1)
         {
            ptr--;
         }
         else
         {
            ptr++;
         }
         convert2bin(&binstr[36], *ptr);
         binstr[44] = ' ';
         if (*(char *)&byte_order == 1)
         {
            ptr--;
         }
         else
         {
            ptr++;
         }
         convert2bin(&binstr[45], *ptr);
         binstr[53] = ' ';
         if (*(char *)&byte_order == 1)
         {
            ptr--;
         }
         else
         {
            ptr++;
         }
         convert2bin(&binstr[54], *ptr);
         binstr[62] = ' ';
         if (*(char *)&byte_order == 1)
         {
            ptr--;
         }
         else
         {
            ptr++;
         }
         convert2bin(&binstr[63], *ptr);
         binstr[71] = '\0';
         (void)fprintf(stdout, "Minute (long long)   : %s\n", binstr);
         (void)fprintf(stdout, "Continues (long long): %s\n", binstr);
#else
         ptr = (char *)&fra[i].te.minute[7];
         convert2bin(binstr, *ptr);
         binstr[8] = ' ';
         ptr--;
         convert2bin(&binstr[9], *ptr);
         binstr[17] = ' ';
         ptr--;
         convert2bin(&binstr[18], *ptr);
         binstr[26] = ' ';
         ptr--;
         convert2bin(&binstr[27], *ptr);
         binstr[35] = ' ';
         ptr--;
         convert2bin(&binstr[36], *ptr);
         binstr[44] = ' ';
         ptr--;
         convert2bin(&binstr[45], *ptr);
         binstr[53] = ' ';
         ptr--;
         convert2bin(&binstr[54], *ptr);
         binstr[62] = ' ';
         ptr--;
         convert2bin(&binstr[63], *ptr);
         binstr[71] = ' ';
         (void)fprintf(stdout, "Minute (uchar[8])    : %s\n", binstr);
         (void)fprintf(stdout, "Continues (uchar[8]) : %s\n", binstr);
#endif /* _WORKING_LONG_LONG */
         ptr = (char *)&fra[i].te.hour;
         if (*(char *)&byte_order == 1)
         {
            ptr += 3;
         }
         convert2bin(binstr, *ptr);
         binstr[8] = ' ';
         if (*(char *)&byte_order == 1)
         {
            ptr--;
         }
         else
         {
            ptr++;
         }
         convert2bin(&binstr[9], *ptr);
         binstr[17] = ' ';
         if (*(char *)&byte_order == 1)
         {
            ptr--;
         }
         else
         {
            ptr++;
         }
         convert2bin(&binstr[18], *ptr);
         binstr[26] = ' ';
         if (*(char *)&byte_order == 1)
         {
            ptr--;
         }
         else
         {
            ptr++;
         }
         convert2bin(&binstr[27], *ptr);
         binstr[35] = '\0';
         (void)fprintf(stdout, "Hour (uint)          : %s\n", binstr);
         ptr = (char *)&fra[i].te.day_of_month;
         if (*(char *)&byte_order == 1)
         {
            ptr += 3;
         }
         convert2bin(binstr, *ptr);
         binstr[8] = ' ';
         if (*(char *)&byte_order == 1)
         {
            ptr--;
         }
         else
         {
            ptr++;
         }
         convert2bin(&binstr[9], *ptr);
         binstr[17] = ' ';
         if (*(char *)&byte_order == 1)
         {
            ptr--;
         }
         else
         {
            ptr++;
         }
         convert2bin(&binstr[18], *ptr);
         binstr[26] = ' ';
         if (*(char *)&byte_order == 1)
         {
            ptr--;
         }
         else
         {
            ptr++;
         }
         convert2bin(&binstr[27], *ptr);
         binstr[35] = '\0';
         (void)fprintf(stdout, "Day of month (uint)  : %s\n", binstr);
         ptr = (char *)&fra[i].te.month;
         if (*(char *)&byte_order == 1)
         {
            ptr++;
         }
         convert2bin(binstr, *ptr);
         binstr[8] = ' ';
         if (*(char *)&byte_order == 1)
         {
            ptr--;
         }
         else
         {
            ptr++;
         }
         convert2bin(&binstr[9], *ptr);
         binstr[17] = '\0';
         (void)fprintf(stdout, "Month (short)        : %s\n", binstr);
         convert2bin(binstr, fra[i].te.day_of_week);
         binstr[8] = '\0';
         (void)fprintf(stdout, "Day of week (uchar)  : %s\n", binstr);
      }
      (void)fprintf(stdout, "Last retrieval       : %s",
                    ctime(&fra[i].last_retrieval));
   }

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++ convert2bin() ++++++++++++++++++++++++++++*/ 
static void
convert2bin(char *buf, unsigned char val)
{
   register int i;

   for (i = 0; i < 8; i++)
   {
      if (val & 1)
      {
         buf[7 - i] = '1';
      }
      else
      {
         buf[7 - i] = '0';
      }
      val = val >> 1;
   }

   return;
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(void)
{
   (void)fprintf(stderr,
                 "SYNTAX  : fra_view [--version] [-w working directory] dir-alias|position\n");
   return;
}

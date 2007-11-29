/*
 *  alda.c - Part of AFD, an automatic file distribution program.
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
 **   alda - AFD log data analyser
 **
 ** SYNOPSIS
 **   alda [options] <file name pattern>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.04.2007 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>      /* exit()                                      */
#include <unistd.h>      /* STDERR_FILENO                               */
#include "logdefs.h"
#include "version.h"

/* External global variables. */
int        mode = 0,
           sys_log_fd = STDERR_FILENO; /* Used by get_afd_path() */
char       *p_log_file,
           *p_work_dir;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ alda $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   time_t end,
          start;
   char   log_dir[MAX_PATH_LENGTH],
          *p_log_dir,
          work_dir[MAX_PATH_LENGTH];

   /* Evaluate input arguments. */
   CHECK_FOR_VERSION(argc, argv);
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   eval_input_alda(&argc, argv);

   /* Initialize variables. */
   p_work_dir = work_dir;
   p_log_dir = log_dir + sprintf(log_dir, "%s%s/", p_work_dir,
                                 (mode == ALDA_REMOTE_MODE) ? RLOG_DIR : LOG_DIR);

   start = time(NULL);

   /* Lets determine what log files we need to search. */
   if (mode & ALDA_REMOTE_MODE)
   {
      int i;

      get_current_afd_mon_list();
      check_start_afds();
      check_end_afds();

      for (i = 0; i < no_of_start_afds; i++)
      {
         search_afd(start_afd[i], search_pattern, start_time);
      }
   }
   else
   {
      search_afd(NULL, search_pattern, start_time);
   }
   end = time(NULL);

   exit(SUCCESS);
}


/*############################ search_afd() #############################*/
static int
search_afd(char *search_afd, char *search_pattern, time_t start_time)
{
   int          i, j, k;
   unsigned int last_no_of_pf,/* Last number of production files. */
                no_of_if,     /* Number of input files. */
                no_of_pf;     /* Number of production files. */
   char         **ifl = NULL, /* Input file list. */
                **last_pfl,   /* Last production file list. */
                **pfl = NULL; /* Production file list. */

   if (search_afd == NULL)
   {
      p_log_file = p_log_dir;
   }
   else
   {
      p_log_file = p_log_dir + sprintf(p_log_dir, "%s/", search_afd);
   }
   if ((no_of_if = check_input_log(search_pattern, start_time, &ifl)) > 0)
   {
      for (i = 0; i < no_of_if; i++)
      {
         last_no_of_pf = 0;
         last_pfl = NULL;
         while ((no_of_pf = check_production_log(&ifl[i], &pfl)) > 0)
         {
            print_data();
            if (last_pfl != NULL)
            {
               free(last_pfl);
            }
            last_pfl = pfl;
            last_no_of_pf = no_of_pf;
         }
         no_of_pf = last_no_of_pf;
         pfl = last_pfl;
         if (no_of_pf > 0)
         {
            for (j = 0; j < no_of_pf; j++)
            {
               if ((no_of_of = check_output_log(&pfl[j], &ofl)) > 0)
               {
                  for (k = 0; k < no_of_of; k++)
                  {
                     print_data();
                     if (search_afd != NULL)
                     {
                        search_afd(ofl[k].host_name, ofl[k].file_name, ofl[k].data_time);
                     }
                  }
               }
               else
               {
                  if ((no_of_df = check_delete_log(&pfl[j], &dfl)) > 0)
                  {
                  }
                  else
                  {
                     /* End! File not found! */;
                  }
               }
            }
         }
         else
         {
            if ((no_of_of = check_output_log(&ifl[i], &ofl)) > 0)
            {
               for (j = 0; j < no_of_of; j++)
               {
                  print_data();
                  if (search_afd != NULL)
                  {
                     search_afd(ofl[j].host_name, ofl[j].file_name, ofl[k].data_time);
                  }
               }
            }
            else
            {
               if ((no_of_df = check_delete_log(&ifl[i], &dfl)) > 0)
               {
                  print_data();
               }
               else
               {
                  /* End! File not found! */;
               }
            }
         }
      }
   }
   else
   {
      if ((no_of_df = check_delete_log(search_pattern, &dfl)) > 0)
      {
      }
   }

   return;
}


/*########################## eval_input_alda() ##########################*/
static void
eval_input_alda(int *argc, char **argv)
{
   return;
}


/*##################### get_current_afd_mon_list() ######################*/
static void
get_current_afd_mon_list(void)
{
   return;
}


/*######################### check_start_afds() ##########################*/
static void
check_start_afds(void)
{
   return;
}


/*########################## check_end_afds() ###########################*/
static void
check_end_afds(void)
{
   return;
}


/*++++++++++++++++++++++++++ check_input_log() ++++++++++++++++++++++++++*/
static unsigned int
check_input_log(char   *search_pattern,
                time_t start_time,
                time_t end_time,
                char   ***ifl)
{
   int         end_file_no,
               i,
               start_file_no;
   struct stat stat_buf;

   end_file_no = start_file_no = -1;
   no_of_log_files = max_input_log_files;
   p_log_file += sprintf(p_log_dir,"%s", INPUT_BUFFER_FILE);

   for (i = 0; i < no_of_log_files; i++)
   {
      (void)sprintf(p_log_file, "%d", i);
      if (stat(log_file, &stat_buf) == 0)
      {
         if (((stat_buf.st_mtime + SWITCH_FILE_TIME) >= start_time) ||
             (start_file_no == -1))
         {
            start_file_no = i;
         }
         if (end_time == -1)
         {
            end_file_no = i;
         }
         else if ((stat_buf.st_mtime >= end_time) || (end_file_no == -1))
               {
                  end_file_no = i;
               }
      }
   }
   no_of_log_files = start_file_no - end_file_no + 1;

   j = 0;
   for (i = start_file_no; i >= end_file_no; i--, j++)
   {
      (void)sprintf(p_log_file, "%d", i);
      (void)extract_data(log_file, j, i);
      total_file_size += file_size;
   }
}

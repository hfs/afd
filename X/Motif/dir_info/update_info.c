/*
 *  update_info.c - Part of AFD, an automatic file distribution program.
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
 **   update_info - updates any information that changes for module
 **                 dir_info
 **
 ** SYNOPSIS
 **   void update_info(Widget w)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* exit()                                  */
#include <time.h>             /* strftime(), localtime()                 */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include "afd_ctrl.h"
#include "dir_info.h"

/* external global variables */
extern int                        dir_position,
                                  view_passwd;
extern char                       label_l[NO_OF_DIR_ROWS][23],
                                  label_r[NO_OF_DIR_ROWS][23],
                                  *p_work_dir;
extern Display                    *display;
extern XtIntervalId               interval_id_dir;
extern XtAppContext               app;
extern Widget                     dirname_text_w,
                                  toplevel_w,
                                  label_l_widget[NO_OF_DIR_ROWS],
                                  label_r_widget[NO_OF_DIR_ROWS],
                                  text_wl[NO_OF_DIR_ROWS],
                                  text_wr[NO_OF_DIR_ROWS],
                                  url_text_w;
extern struct fileretrieve_status *fra;
extern struct prev_values         prev;


/*############################ update_info() ############################*/
void
update_info(w)
Widget w;
{
   signed char flush = NO;
   char        str_line[MAX_INFO_STRING_LENGTH],
               tmp_str_line[MAX_INFO_STRING_LENGTH],
               yesno[4];

   /* Check if FRA changed */
   (void)check_fra();

   if (strcmp(prev.dir_alias, fra[dir_position].dir_alias) != 0)
   {
      (void)xrec(toplevel_w, FATAL_DIALOG,
                 "Hmmm, looks like dir alias %s is gone. Terminating! (%s %d)",
                 prev.dir_alias, __FILE__, __LINE__);
      return;
   }

   if (prev.dir_pos != fra[dir_position].dir_pos)
   {
      int                 dnb_fd;
      char                dir_name_file[MAX_PATH_LENGTH],
                          *ptr;
      struct stat         stat_buf;
      struct dir_name_buf *dnb;

      /* Map to directory name buffer. */
      (void)sprintf(dir_name_file, "%s%s%s", p_work_dir, FIFO_DIR,
                    DIR_NAME_FILE);
      if ((dnb_fd = open(dir_name_file, O_RDONLY)) == -1)
      {
         (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                       dir_name_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (fstat(dnb_fd, &stat_buf) == -1)
      {
         (void)fprintf(stderr, "Failed to fstat() %s : %s (%s %d)\n",
                       dir_name_file, strerror(errno), __FILE__, __LINE__);
         (void)close(dnb_fd);
         exit(INCORRECT);
      }
      if (stat_buf.st_size > 0)
      {
         if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                         MAP_SHARED, dnb_fd, 0)) == (caddr_t) -1)
         {
            (void)fprintf(stderr, "Failed to mmap() to %s : %s (%s %d)\n",
                          dir_name_file, strerror(errno), __FILE__, __LINE__);
            (void)close(dnb_fd);
            exit(INCORRECT);
         }
         ptr += AFD_WORD_OFFSET;
         dnb = (struct dir_name_buf *)ptr;
      }
      else
      {
         (void)fprintf(stderr, "Job ID database file is empty. (%s %d)\n",
                       __FILE__, __LINE__);
         (void)close(dnb_fd);
         exit(INCORRECT);
      }
      prev.dir_pos = fra[dir_position].dir_pos;
      (void)strcpy(prev.real_dir_name, dnb[prev.dir_pos].dir_name);
      if (close(dnb_fd) == -1)
      {
         (void)fprintf(stderr, "Failed to close() %s : %s (%s %d)\n",
                       dir_name_file, strerror(errno), __FILE__, __LINE__);
      }
      ptr -= AFD_WORD_OFFSET;
      if (munmap(ptr, stat_buf.st_size) == -1)
      {
         (void)fprintf(stderr, "Failed to munmap() from %s : %s (%s %d)\n",
                       dir_name_file, strerror(errno), __FILE__, __LINE__);
      }
      (void)sprintf(str_line, "%*s",
                    MAX_INFO_STRING_LENGTH, prev.real_dir_name);
      XmTextSetString(dirname_text_w, str_line);
      flush = YES;
   }

   if (fra[dir_position].host_alias[0] != '\0')
   {
      if (strcmp(prev.url, fra[dir_position].url) != 0)
      {
         (void)strcpy(prev.url, fra[dir_position].url);
         (void)strcpy(prev.display_url, prev.url);
         if (view_passwd != YES)
         {
            remove_passwd(prev.display_url);
         }
         (void)sprintf(str_line, "%*s",
                       MAX_INFO_STRING_LENGTH, prev.display_url);
         XmTextSetString(url_text_w, str_line);
         flush = YES;
      }
   }

   if (prev.priority != fra[dir_position].priority)
   {
      prev.priority = fra[dir_position].priority;
      (void)sprintf(str_line, "%*d", DIR_INFO_LENGTH_L, prev.priority - '0');
      XmTextSetString(text_wl[3], str_line);
      flush = YES;
   }

   if (prev.stupid_mode != fra[dir_position].stupid_mode)
   {
      prev.stupid_mode = fra[dir_position].stupid_mode;
      if (prev.stupid_mode == YES)
      {
         yesno[0] = 'Y'; yesno[1] = 'e'; yesno[2] = 's'; yesno[3] = '\0';
      }
      else
      {
         yesno[0] = 'N'; yesno[1] = 'o'; yesno[2] = '\0';
      }
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, yesno);
      XmTextSetString(text_wl[1], str_line);
      flush = YES;
   }

   if (prev.remove != fra[dir_position].remove)
   {
      prev.remove = fra[dir_position].remove;
      if (prev.remove == YES)
      {
         yesno[0] = 'Y'; yesno[1] = 'e'; yesno[2] = 's'; yesno[3] = '\0';
      }
      else
      {
         yesno[0] = 'N'; yesno[1] = 'o'; yesno[2] = '\0';
      }
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, yesno);
      XmTextSetString(text_wr[1], str_line);
      flush = YES;
   }

   if (prev.force_reread != fra[dir_position].force_reread)
   {
      prev.force_reread = fra[dir_position].force_reread;
      if (prev.force_reread == YES)
      {
         yesno[0] = 'Y'; yesno[1] = 'e'; yesno[2] = 's'; yesno[3] = '\0';
      }
      else
      {
         yesno[0] = 'N'; yesno[1] = 'o'; yesno[2] = '\0';
      }
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, yesno);
      XmTextSetString(text_wl[2], str_line);
      flush = YES;
   }

   if (prev.report_unknown_files != fra[dir_position].report_unknown_files)
   {
      prev.report_unknown_files = fra[dir_position].report_unknown_files;
      if (prev.report_unknown_files == YES)
      {
         yesno[0] = 'Y'; yesno[1] = 'e'; yesno[2] = 's'; yesno[3] = '\0';
      }
      else
      {
         yesno[0] = 'N'; yesno[1] = 'o'; yesno[2] = '\0';
      }
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, yesno);
      XmTextSetString(text_wr[2], str_line);
      flush = YES;
   }

   if (prev.old_file_time != fra[dir_position].old_file_time)
   {
      prev.old_file_time = fra[dir_position].old_file_time;
      (void)sprintf(str_line, "%*d", DIR_INFO_LENGTH_L,
                    prev.old_file_time / 3600);
      XmTextSetString(text_wl[3], str_line);
      flush = YES;
   }

   if (prev.delete_unknown_files != fra[dir_position].delete_unknown_files)
   {
      prev.delete_unknown_files = fra[dir_position].delete_unknown_files;
      if (prev.delete_unknown_files == YES)
      {
         yesno[0] = 'Y'; yesno[1] = 'e'; yesno[2] = 's'; yesno[3] = '\0';
      }
      else
      {
         yesno[0] = 'N'; yesno[1] = 'o'; yesno[2] = '\0';
      }
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, yesno);
      XmTextSetString(text_wr[3], str_line);
      flush = YES;
   }

   if (prev.bytes_received != fra[dir_position].bytes_received)
   {
      prev.bytes_received = fra[dir_position].bytes_received;
      (void)sprintf(str_line, "%*lu", DIR_INFO_LENGTH_L, prev.bytes_received);
      XmTextSetString(text_wl[4], str_line);
      flush = YES;
   }

   if (prev.files_received != fra[dir_position].files_received)
   {
      prev.files_received = fra[dir_position].files_received;
      (void)sprintf(str_line, "%*u", DIR_INFO_LENGTH_R, prev.files_received);
      XmTextSetString(text_wr[4], str_line);
      flush = YES;
   }

   if (prev.last_retrieval != fra[dir_position].last_retrieval)
   {
      prev.last_retrieval = fra[dir_position].last_retrieval;
      (void)strftime(tmp_str_line, MAX_INFO_STRING_LENGTH, "%d.%m.%Y  %H:%M:%S",
                     localtime(&prev.last_retrieval));
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, tmp_str_line);
      XmTextSetString(text_wl[5], str_line);
      flush = YES;
   }

   if (prev.time_option != fra[dir_position].time_option)
   {
      if (prev.time_option == YES)
      {
         (void)strftime(tmp_str_line, MAX_INFO_STRING_LENGTH, "%d.%m.%Y  %H:%M:%S",
                        localtime(&prev.next_check_time));
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, tmp_str_line);
      }
      else
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "No time entry.");
      }
      XmTextSetString(text_wr[5], str_line);
      prev.time_option = fra[dir_position].time_option;
   }
   else if ((prev.time_option == YES) &&
            (prev.next_check_time != fra[dir_position].next_check_time))
        {
           prev.next_check_time = fra[dir_position].next_check_time;
           (void)strftime(tmp_str_line, MAX_INFO_STRING_LENGTH, "%d.%m.%Y  %H:%M:%S",
                          localtime(&prev.next_check_time));
           (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, tmp_str_line);
           XmTextSetString(text_wr[5], str_line);
           flush = YES;
        }

   if (flush == YES)
   {
      XFlush(display);
   }

   /* Call update_info() after UPDATE_INTERVAL ms */
   interval_id_dir = XtAppAddTimeOut(app, UPDATE_INTERVAL,
                                     (XtTimerCallbackProc)update_info,
                                     NULL);

   return;
}

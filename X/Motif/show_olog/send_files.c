/*
 *  send_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   send_files - sends files from the AFD archive to a host not
 **                in the FSA
 **
 ** SYNOPSIS
 **   void send_files(int no_selected, int *select_list)
 **
 ** DESCRIPTION
 **   send_files() will send all files selected in the show_olog
 **   dialog to a host not specified in the FSA. Only files that
 **   have been archived will be resend.
 **   Since the selected list can be rather long, this function
 **   will try to optimise the resending of files by collecting
 **   all jobs in a list with the same ID and generate a single
 **   message for all of them. If this is not done, far to many
 **   messages will be generated.
 **
 **   Every time a complete list with the same job ID has been
 **   resend, will cause this function to deselect those items.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.03.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>         /* strerror(), strcmp(), strcpy(), strcat()  */
#include <stdlib.h>         /* calloc(), free()                          */
#include <unistd.h>         /* rmdir(), sysconf()                        */
#include <time.h>           /* time()                                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>         /* opendir(), readdir(), closedir()          */
#include <fcntl.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/List.h>
#include "afd_ctrl.h"
#include "show_olog.h"
#include "fddefs.h"

/* External global variables */
extern Display          *display;
extern Widget           toplevel_w,
                        special_button_w,
                        scrollbar_w,
                        statusbox_w,
                        listbox_w;
extern int              no_of_log_files,
                        sys_log_fd,
                        special_button_flag;
extern char             *p_work_dir;
extern struct item_list *il;
extern struct send_data *db;
extern struct sol_perm  perm;

/* Global variables */
int                     timeout_flag = OFF;
long                    transfer_timeout;
clock_t                 clktck;
char                    archive_dir[MAX_PATH_LENGTH],
                        msg_str[MAX_RET_MSG_LENGTH],
                        *ascii_buffer = NULL,
                        *p_archive_name;


/*############################## send_files() ###########################*/
void
send_files(int no_selected, int *select_list)
{
   int                failed_to_send = 0,
                      i,
                      length = 0,
                      to_do = 0,    /* Number still to be done */
                      no_done = 0,  /* Number done             */
                      not_found = 0,
                      not_archived = 0,
                      not_in_archive = 0,
                      select_done = 0,
                      *select_done_list,
                      total_no_of_items;
   static int         user_limit = 0;
   char               user_message[256];
   XmString           xstr;
   struct resend_list *rl;

   if ((perm.send_limit > 0) && (user_limit >= perm.send_limit))
   {
      (void)sprintf(user_message, "User limit (%d) for resending reached!",
                    perm.send_limit);
      show_message(statusbox_w, user_message);
      return;
   }
   if (((rl = (struct resend_list *)calloc(no_selected, sizeof(struct resend_list))) == NULL) ||
       ((select_done_list = (int *)calloc(no_selected, sizeof(int))) == NULL))
   {
      (void)xrec(toplevel_w, FATAL_DIALOG, "calloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   /* Prepare the archive directory name */
   p_archive_name = archive_dir;
   p_archive_name += sprintf(archive_dir, "%s%s/",
                             p_work_dir, AFD_ARCHIVE_DIR);

   /* Block all input and change button name. */
   special_button_flag = STOP_BUTTON;
   xstr = XmStringCreateLtoR("Stop", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);
   CHECK_INTERRUPT();

   /*
    * Get the job ID, file number and position in that file for all
    * selected items. If the file was not archived mark it as done
    * immediately.
    */
   for (i = 0; i < no_selected; i++)
   {
      /* Determine log file and position in this log file. */
      total_no_of_items = 0;
      rl[i].pos = -1;
      for (rl[i].file_no = 0; rl[i].file_no < no_of_log_files; rl[i].file_no++)
      {
         total_no_of_items += il[rl[i].file_no].no_of_items;

         if (select_list[i] <= total_no_of_items)
         {
            rl[i].pos = select_list[i] - (total_no_of_items - il[rl[i].file_no].no_of_items) - 1;
            if (rl[i].pos > il[rl[i].file_no].no_of_items)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "pos (%d) is greater then no_of_items (%d), ignoring this.",
                          rl[i].pos, il[rl[i].file_no].no_of_items);
               rl[i].pos = -1;
            }
            break;
         }
      }

      /* Check if the file is in fact archived. */
      if (rl[i].pos > -1)
      {
         if (il[rl[i].file_no].archived[rl[i].pos] == 1)
         {
            to_do++;
         }
         else
         {
            rl[i].status = NOT_ARCHIVED;
            not_archived++;
         }
      }
      else
      {
         rl[i].status = NOT_FOUND;
         not_found++;
      }
   }
   if ((clktck = sysconf(_SC_CLK_TCK)) < 0)
   {
      if (clktck == -1)
      {
         (void)sprintf(user_message, "sysconf() error : %s", strerror(errno));
      }
      else
      {
         (void)sprintf(user_message,
                       "sysconf() error, option _SC_CLK_TCK not available.");
      }
      show_message(statusbox_w, user_message);
      free((void *)rl);
      free((void *)select_done_list);
      return;
   }

   /*
    * Now we have the job ID of every file that is to be resend.
    * Plus we have removed those that have not been archived or
    * could not be found.
    * Lets lookup the archive directory for each job ID and then
    * collect all files that are to be resend for this ID. When
    * all files have been collected we start sending all of them
    * to the given destination.
    */
   if (to_do > 0)
   {
      /*
       * If neccessary connect to the destination.
       */
      if (db->protocol == FTP)
      {
         send_files_ftp(no_selected, &not_in_archive, &failed_to_send,
                        &no_done, &select_done, rl, select_list, &user_limit);
      } /* FTP */
      else if (db->protocol == SMTP)
           {
              send_files_smtp(no_selected, &not_in_archive, &failed_to_send,
                              &no_done, &select_done, rl, select_list,
                              &user_limit);
           }
           else
           {
              (void)fprintf(stderr,
                            "Hmm, something is wrong here, unknown protocol ID (%d)\n",
                            db->protocol);
           }
   } /* if (to_do > 0) */

   /* Show user a summary of what was done. */
   user_message[0] = ' ';
   user_message[1] = '\0';
   if (no_done > 0)
   {
      if (no_done == 1)
      {
         length = sprintf(user_message, "1 file send");
      }
      else
      {
         length = sprintf(user_message, "%d files send", no_done);
      }
   }
   if (not_archived > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d not archived",
                           not_archived);
      }
      else
      {
         length = sprintf(user_message, "%d not archived", not_archived);
      }
   }
   if (not_in_archive > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d not in archive",
                           not_in_archive);
      }
      else
      {
         length = sprintf(user_message, "%d not in archive", not_in_archive);
      }
   }
   if (failed_to_send > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d failed to send",
                           failed_to_send);
      }
      else
      {
         length = sprintf(user_message, "%d failed to send", failed_to_send);
      }
   }
   if (not_found > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d not found", not_found);
      }
      else
      {
         length = sprintf(user_message, "%d not found", not_found);
      }
   }
   if ((perm.resend_limit >= 0) && (user_limit >= perm.resend_limit))
   {
      (void)sprintf(&user_message[length], " USER LIMIT (%d) REACHED",
                    perm.resend_limit);
   }
   show_message(statusbox_w, user_message);

   free((void *)rl);
   free((void *)select_done_list);

   /* Button back to normal. */
   special_button_flag = SEARCH_BUTTON;
   xstr = XmStringCreateLtoR("Search", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   return;
}

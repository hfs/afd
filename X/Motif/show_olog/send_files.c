/*
 *  send_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
#include <unistd.h>         /* rmdir()                                   */
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
                        listbox_w;
extern int              no_of_log_files,
                        sys_log_fd,
                        special_button_flag;
extern char             *p_work_dir;
extern struct item_list *il;
extern struct info_data id;
extern struct sol_perm  perm;

/* Local global variables */
static int              job_priority,
                        max_copied_files,
                        overwrite;
static char             archive_dir[MAX_PATH_LENGTH],
                        *p_file_name,
                        *p_archive_name,
                        *p_dest_dir_end = NULL,
                        dest_dir[MAX_PATH_LENGTH];

/* Local function prototypes */
static int              get_archive_data(int, int),
                        get_file(char *, char *);
static void             get_afd_config_value(void);


/*############################## send_files() ###########################*/
void
send_files(int no_selected, int *select_list)
{
   int                i,
                      k,
                      total_no_of_items,
                      current_job_id = 0,
                      last_job_id = 0,
                      length = 0,
                      to_do = 0,    /* Number still to be done */
                      no_done = 0,  /* Number done             */
                      not_found = 0,
                      not_archived = 0,
                      not_in_archive = 0,
                      select_done = 0,
                      *select_done_list;
   static int         user_limit = 0;
   char               *p_msg_name,
                      user_message[256];
   XmString           xstr;
   struct resend_list *rl;

   if ((perm.send_limit > 0) && (user_limit >= perm.send_limit))
   {
      (void)sprintf(user_message, "User limit (%d) for resending reached!",
                    perm.send_limit);
      show_message(user_message);
      return;
   }
   overwrite = 0;
   dest_dir[0] = '\0';
   if (((rl = (struct resend_list *)calloc(no_selected, sizeof(struct resend_list))) == NULL) ||
       ((select_done_list = (int *)calloc(no_selected, sizeof(int))) == NULL))
   {
      (void)xrec(toplevel_w, FATAL_DIALOG, "calloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   /* See how many files we may copy in one go. */
   get_afd_config_value();

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
      for (rl[i].file_no = 0; rl[i].file_no < no_of_log_files; rl[i].file_no++)
      {
         total_no_of_items += il[rl[i].file_no].no_of_items;

         if (select_list[i] <= total_no_of_items)
         {
            rl[i].pos = select_list[i] - (total_no_of_items - il[rl[i].file_no].no_of_items) - 1;
            break;
         }
      }

      /* Get the job ID and file name. */
      if (rl[i].pos > -1)
      {
         if (il[rl[i].file_no].archived[rl[i].pos] == 1)
         {
            /*
             * Read the job ID from the output log file.
             */
            int  j;
            char buffer[15];

            if (fseek(il[rl[i].file_no].fp, (long)il[rl[i].file_no].offset[rl[i].pos], SEEK_SET) == -1)
            {
               (void)xrec(toplevel_w, FATAL_DIALOG,
                          "fseek() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
               free((void *)rl);
               free((void *)select_done_list);
               return;
            }

            j = 0;
            while (((buffer[j] = fgetc(il[rl[i].file_no].fp)) != '\n') &&
                   (buffer[j] != ' '))
            {
               j++;
            }
            buffer[j] = '\0';

            rl[i].job_id = (unsigned int)strtoul(buffer, NULL, 10);
            rl[i].status = FILE_PENDING;
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

   /*
    * Now we have the job ID of every file that is to be resend.
    * Plus we have removed those that have not been archived or
    * could not be found.
    * Lets lookup the archive directory for each job ID and then
    * collect all files that are to be resend for this ID. When
    * all files have been collected we send a message for this
    * job ID and then deselect all selected items that have just
    * been resend.
    */
   while (to_do > 0)
   {
      for (i = 0; i < no_selected; i++)
      {
         if (rl[i].status == FILE_PENDING)
         {
            id.job_no = current_job_id = rl[i].job_id;
            break;
         }
      }

      for (k = i; k < no_selected; k++)
      {
         if ((rl[k].status == FILE_PENDING) &&
             (current_job_id == rl[k].job_id))
         {
            if (get_archive_data(rl[k].pos, rl[k].file_no) < 0)
            {
               rl[k].status = NOT_IN_ARCHIVE;
               not_in_archive++;
            }
            else
            {
               if ((select_done % max_copied_files) == 0)
               {
                  /* Copy a message so FD can pick up the job. */
                  if (select_done != 0)
                  {
                     int m;

                     /* Deselect all those that where resend */
                     for (m = 0; m < select_done; m++)
                     {
                        XmListDeselectPos(listbox_w, select_done_list[m]);
                     }
                     select_done = 0;
                  }
                  else if ((select_done == 0) && (dest_dir[0] != '\0') && (p_dest_dir_end != NULL))
                       {
                          /* Remove the directory we created in the files dir */
                          *p_dest_dir_end = '\0';
                          if (rmdir(dest_dir) < 0)
                          {
                             (void)xrec(toplevel_w, WARN_DIALOG,
                                        "Failed to remove directory %s : %s (%s %d)",
                                        dest_dir, strerror(errno), __FILE__, __LINE__);
                          }
                       }
                  p_dest_dir_end = p_msg_name;
                  while (*p_dest_dir_end != '\0')
                  {
                     p_dest_dir_end++;
                  }
                  *(p_dest_dir_end++) = '/';
                  *p_dest_dir_end = '\0';
               }
               if (get_file(dest_dir, p_dest_dir_end) < 0)
               {
                  rl[k].status = NOT_IN_ARCHIVE;
                  not_in_archive++;
               }
               else
               {
                  rl[k].status = DONE;
                  no_done++;
                  select_done_list[select_done] = select_list[k];
                  select_done++;
                  last_job_id = current_job_id;

                  if (perm.resend_limit >= 0)
                  {
                     user_limit++;
                     if ((user_limit - overwrite) >= perm.resend_limit)
                     {
                        break;
                     }
                  }
               }
            }
            to_do--;
         }
      } /* for (k = i; k < no_selected; k++) */

      /* Copy a message so FD can pick up the job. */
      if ((select_done > 0) && ((select_done % max_copied_files) != 0))
      {
         int m;

         /* Deselect all those that where resend */
         for (m = 0; m < select_done; m++)
         {
            XmListDeselectPos(listbox_w, select_done_list[m]);
         }
         select_done = 0;
      }

      CHECK_INTERRUPT();

      if ((special_button_flag == STOP_BUTTON_PRESSED) ||
          ((perm.resend_limit >= 0) &&
           ((user_limit - overwrite) >= perm.resend_limit)))
      {
         break;
      }
   } /* while (to_do > 0) */

   if ((no_done == 0) && (dest_dir[0] != '\0') && (p_dest_dir_end != NULL))
   {
      /* Remove the directory we created in the files dir */
      *p_dest_dir_end = '\0';
   }

   /* Show user a summary of what was done. */
   if (no_done > 0)
   {
      if ((no_done - overwrite) == 1)
      {
         length = sprintf(user_message, "1 file resend");
      }
      else
      {
         length = sprintf(user_message, "%d files resend",
                          no_done - overwrite);
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
   if (overwrite > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d overwrites",
                           overwrite);
      }
      else
      {
         length = sprintf(user_message, "%d overwrites", overwrite);
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
   if ((perm.resend_limit >= 0) && ((user_limit - overwrite) >= perm.resend_limit))
   {
      (void)sprintf(&user_message[length], " USER LIMIT (%d) REACHED",
                    perm.resend_limit);
   }
   show_message(user_message);

   free((void *)rl);
   free((void *)select_done_list);

   /* Button back to normal. */
   special_button_flag = SEARCH_BUTTON;
   xstr = XmStringCreateLtoR("Search", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   return;
}


/*+++++++++++++++++++++++++++ get_archive_data() ++++++++++++++++++++++++*/
/*                            ------------------                         */
/* Description: From the output log file, this function gets the file    */
/*              name and the name of the archive directory.              */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int
get_archive_data(int pos, int file_no)
{
   int  i,
        slash_count = 0;
   char *ptr,
        buffer[MAX_FILENAME_LENGTH + MAX_PATH_LENGTH];

   if (fseek(il[file_no].fp, (long)il[file_no].line_offset[pos], SEEK_SET) == -1)
   {
      (void)xrec(toplevel_w, FATAL_DIALOG, "fseek() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }
   if (fgets(buffer, MAX_FILENAME_LENGTH + MAX_PATH_LENGTH, il[file_no].fp) == NULL)
   {
      (void)xrec(toplevel_w, FATAL_DIALOG, "fgets() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   ptr = buffer;

   /* Mark end of file name */
   while (*ptr != ' ')
   {
      ptr++;
   }
   *(ptr++) = '\0';

   /* Away with the size */
   while (*ptr != ' ')
   {
      ptr++;
   }
   ptr++;

   /* Away with transfer duration */
   while (*ptr != ' ')
   {
      ptr++;
   }
   ptr++;

   /* Away with the job ID */
   while (*ptr != ' ')
   {
      ptr++;
   }
   ptr++;

   /* Ahhh. Now here is the archive directory we are looking for. */
   i = 0;
   while (*ptr != '\n')
   {
      *(p_archive_name + i) = *ptr;
      if (*ptr == '/')
      {
         slash_count++;
         if (slash_count == 3)
         {
            job_priority = *(ptr + 1);
         }
      }
      i++; ptr++;
   }
   *(p_archive_name + i++) = '/';

   /* Copy the file name to the archive directory. */
   (void)strcpy((p_archive_name + i), &buffer[0]);
   p_file_name = p_archive_name + i;

   return(SUCCESS);
}


/*++++++++++++++++++++++++++++++ get_file() +++++++++++++++++++++++++++++*/
/*                               ----------                              */
/* Description: Will try to link a file from the archive directory to    */
/*              new file directory. If it fails to link them and errno   */
/*              is EXDEV (file systems diver) or EEXIST (file exists),   */
/*              it will try to copy the file (ie overwrite it in case    */
/*              errno is EEXIST).                                        */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int
get_file(char *dest_dir, char *p_dest_dir_end)
{
   (void)strcpy(p_dest_dir_end, p_file_name);
   if (link(archive_dir, dest_dir) < 0)
   {
      switch (errno)
      {
         case EEXIST : /* File already exists. Overwrite it. */
                       overwrite++;
                       /* NOTE: Falling through! */
         case EXDEV  : /* File systems diver. */
                       if (copy_file(archive_dir, dest_dir) < 0)
                       {
                          (void)xrec(toplevel_w, ERROR_DIALOG,
                                     "Failed to copy %s to %s (%s %d)",
                                     archive_dir, dest_dir,
                                     __FILE__, __LINE__);
                          return(INCORRECT);
                       }
                       break;
         case ENOENT : /* File is not in archive dir. */
                       return(INCORRECT);
         default:      /* All other errors go here. */
                       (void)xrec(toplevel_w, ERROR_DIALOG,
                                  "Failed to link() %s to %s : %s (%s %d)",
                                  archive_dir, dest_dir, strerror(errno),
                                  __FILE__, __LINE__);
                       return(INCORRECT);
      }
   }

   return(SUCCESS);
}


/*++++++++++++++++++++++++ get_afd_config_value() +++++++++++++++++++++++*/
static void
get_afd_config_value(void)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH];

   (void)sprintf(config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((access(config_file, F_OK) == 0) &&
       (read_file(config_file, &buffer) != INCORRECT))
   {
      char value[MAX_INT_LENGTH];

      if (get_definition(buffer, MAX_COPIED_FILES_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         max_copied_files = atoi(value);
         if ((max_copied_files < 1) || (max_copied_files > 10240))
         {
            max_copied_files = MAX_COPIED_FILES;
         }
      }
      else
      {
         max_copied_files = MAX_COPIED_FILES;
      }
      free(buffer);
   }
   else
   {
      max_copied_files = MAX_COPIED_FILES;
   }

   return;
}

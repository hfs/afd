/*
 *  send_files_smtp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   send_files_smtp - sends files from the archive via SMTP
 **
 ** SYNOPSIS
 **   void send_files_smtp(int no_selected, int *select_list)
 **
 ** DESCRIPTION
 **   send_files_smtp() will send all files selected in the show_olog
 **   dialog to a host not specified in the FSA. Only files that
 **   have been archived will be resend.
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
 **   27.03.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>         /* strerror(), strcmp(), strcpy(), strcat()  */
#include <stdlib.h>         /* malloc(), free()                          */
#include <unistd.h>         /* sysconf()                                 */
#include <time.h>           /* time()                                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>      /* times(), struct tms                       */
#include <dirent.h>         /* opendir(), readdir(), closedir()          */
#include <fcntl.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/List.h>
#include "afd_ctrl.h"
#include "show_olog.h"
#include "fddefs.h"         /* LOCK_FILENAME                             */
#include "smtpdefs.h"

/* External global variables */
extern Display          *display;
extern Widget           toplevel_w,
                        special_button_w,
                        scrollbar_w,
                        send_statusbox_w,
                        listbox_w;
extern int              no_of_log_files,
                        sys_log_fd,
                        special_button_flag,
                        timeout_flag;
extern long             transfer_timeout;
extern clock_t          clktck;
extern char             archive_dir[],
                        msg_str[],
                        *ascii_buffer,
                        *p_archive_name;
extern struct item_list *il;
extern struct send_data *db;
extern struct sol_perm  perm;

/* Local global variables */
static char             *p_file_name;

/* Local function prototypes */
static int              get_archive_data(int, int),
                        send_file_smtp(char *, char *, char *);


/*########################### send_files_smtp() #########################*/
void
send_files_smtp(int                no_selected,
                int                *not_in_archive,
                int                *failed_to_send,
                int                *no_done,
                int                *select_done,
                struct resend_list *rl,
                int                *select_list,
                int                *user_limit)
{
   int i;

   if ((ascii_buffer = malloc((DEFAULT_TRANSFER_BLOCKSIZE * 2))) == NULL)
   {
      msg_str[0] = '\0';
      trans_log(INFO_SIGN, __FILE__, __LINE__, "malloc() error : %s",
                strerror(errno));
      return;
   }

   transfer_timeout = db->timeout;
   if (db->smtp_server[0] == '\0')
   {
      (void)strcpy(db->smtp_server, SMTP_HOST_NAME);
   }

   if (smtp_connect(db->smtp_server, db->port) == SUCCESS)
   {
      char host_name[256];

      if (db->debug == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__,
                   "Connected to %s at port %d", db->smtp_server, db->port);
      }

      /* Now send HELO */
      if (gethostname(host_name, 255) == 0)
      {
         int status;

         if ((status = smtp_helo(host_name)) == SUCCESS)
         {
            char *ptr,
                 local_user[MAX_FILENAME_LENGTH],
                 remote_user[MAX_FILENAME_LENGTH];

            if (db->debug == YES)
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__, "Send HELO.");
            }

            /* Prepare local and remote user name */
            if ((ptr = getenv("LOGNAME")) != NULL)
            {
               (void)sprintf(local_user, "%s@%s", ptr, host_name);
            }
            else
            {
               (void)sprintf(local_user, "%s@%s", AFD_USER_NAME, host_name);
            }
            (void)sprintf(remote_user, "%s@%s", db->user, db->hostname);

            /*
             * Distribute each file from the archive.
             */
            for (i = 0; i < no_selected; i++)
            {
               if (get_archive_data(rl[i].pos, rl[i].file_no) < 0)
               {
                  (*not_in_archive)++;
                  msg_str[0] = '\0';
                  trans_log(WARN_SIGN, __FILE__, __LINE__,
                            "%s not in archive", p_file_name);
               }
               else
               {
                  if (send_file_smtp(p_file_name, local_user, remote_user) < 0)
                  {
                     (*failed_to_send)++;
                  }
                  else
                  {
                     (*no_done)++;
                     XmListDeselectPos(listbox_w, select_list[i]);
                     (*select_done)++;

                     if (perm.send_limit >= 0)
                     {
                        (*user_limit)++;
                        if (*user_limit >= perm.send_limit)
                        {
                           break;
                        }
                     }
                  }
               }
               CHECK_INTERRUPT();

               if ((special_button_flag == STOP_BUTTON_PRESSED) ||
                   ((perm.resend_limit >= 0) &&
                    (*user_limit >= perm.resend_limit)))
               {
                  break;
               }
            } /* for (i = 0; i < no_selected; i++) */
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to send HELO to <%s> (%d).",
                      db->smtp_server, status);
         }
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "gethostname() error : %s", strerror(errno));
      }

      /*
       * Disconnect from remote host.
       */
      if (smtp_quit() == INCORRECT)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__,
                   "Failed to close connection.");
      }
      else if (db->debug == YES)
           {
              trans_log(INFO_SIGN, __FILE__, __LINE__, "Logged out.");
           }
   }
   else
   {
      /* Failed to connect to remote host. */
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to connect to %s at port %d",
                db->smtp_server, db->port);
   }

   if (ascii_buffer != NULL)
   {
      free(ascii_buffer);
      ascii_buffer = NULL;
   }

   return;
}


/*+++++++++++++++++++++++++++ get_archive_data() ++++++++++++++++++++++++*/
/*                            ------------------                         */
/* Description: From the output log file, this function gets the file    */
/*              name and the name of the archive directory and stores    */
/*              these in archive_dir[].                                  */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int
get_archive_data(int pos, int file_no)
{
   int  i;
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

   ptr = &buffer[11 + MAX_HOSTNAME_LENGTH + 3];

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
      i++; ptr++;
   }
   *(p_archive_name + i++) = '/';

   /* Copy the file name to the archive directory. */
   (void)strcpy((p_archive_name + i), &buffer[11 + MAX_HOSTNAME_LENGTH + 3]);
   p_file_name = p_archive_name + i;

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++ send_file_smtp() ++++++++++++++++++++++++++*/
static int
send_file_smtp(char *file_name, char *local_user, char *remote_user)
{
   int         fd, i,
               loops,
               rest,
               status;
   clock_t     start_time;
   double      transfer_time;
   char        buffer[DEFAULT_TRANSFER_BLOCKSIZE + 4];
   struct tms  tmsdummy;
   struct stat stat_buf;

   /* Send local user name */
   if ((status = smtp_user(local_user)) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to send local user <%s> (%d).", local_user, status);
      return(INCORRECT);
   }
   else
   {
      if (db->debug == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__,
                   "Entered local user name <%s>.", local_user);
      }
   }

   /* Send remote user name */
   if ((status = smtp_rcpt(remote_user)) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to send remote user <%s> (%d).", remote_user, status);
      return(INCORRECT);
   }
   else
   {
      if (db->debug == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__,
                   "Entered remote user name <%s>.", remote_user);
      }
   }

   /* Enter data mode */
   if ((status = smtp_open()) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to set DATA mode (%d).", status);
      return(INCORRECT);
   }
   else
   {
      if (db->debug == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__, "Set DATA mode.");
      }
   }

   if (db->subject[0] != '\0')
   {
      char   buffer[MAX_PATH_LENGTH + 13];
      size_t length;

      length = sprintf(buffer, "Subject : %s\r\n", db->subject);
      if ((status = smtp_write(buffer, buffer, length)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to write subject to SMTP-server (%d).", status);
         (void)smtp_close();
         return(INCORRECT);
      }
      else
      {
         if (db->debug == YES)
         {
            trans_log(INFO_SIGN, __FILE__, __LINE__,
                      "Send Subject: %s", db->subject);
         }
      }
   }

   /*
    * We need a second CRLF to indicate end of header. The stuff
    * that follows is the message body.
    */
   if (smtp_write("\r\n", NULL, 2) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to write second CRLF to indicate end of header.");
      (void)smtp_close();
      return(INCORRECT);
   }

   /* Open local file */
   if ((fd = open(archive_dir, O_RDONLY)) == -1)
   {
      msg_str[0] = '\0';
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to open() locale file %s : %s",
                archive_dir, strerror(errno));
      (void)smtp_close();
      return(INCORRECT);
   }
   else
   {
      if (db->debug == YES)
      {
         msg_str[0] = '\0';
         trans_log(INFO_SIGN, __FILE__, __LINE__,
                   "Open locale file %s.", archive_dir);
      }
   }
   if (fstat(fd, &stat_buf) == -1)
   {
      msg_str[0] = '\0';
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to fstat() locale file %s : %s",
                archive_dir, strerror(errno));
      (void)smtp_close();
      return(INCORRECT);
   }
   start_time = times(&tmsdummy);

   /* Read (local) and write (remote) file */
   loops = stat_buf.st_size / DEFAULT_TRANSFER_BLOCKSIZE;
   rest = stat_buf.st_size % DEFAULT_TRANSFER_BLOCKSIZE;

   for (i = 0; i < loops; i++)
   {
      if (read(fd, buffer, DEFAULT_TRANSFER_BLOCKSIZE) != DEFAULT_TRANSFER_BLOCKSIZE)
      {
         msg_str[0] = '\0';
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to read() locale file %s : %s",
                   archive_dir, strerror(errno));
         (void)smtp_close();
         return(INCORRECT);
      }
      if ((status = smtp_write(buffer, ascii_buffer, DEFAULT_TRANSFER_BLOCKSIZE)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to write data from file %s (%d).",
                   file_name, status);
         (void)smtp_close();
         return(INCORRECT);
      }
   }
   if (rest > 0)
   {
      if (read(fd, buffer, rest) != rest)
      {
         msg_str[0] = '\0';
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to read() locale file %s : %s",
                   archive_dir, strerror(errno));
         (void)smtp_close();
         return(INCORRECT);
      }
      if ((status = smtp_write(buffer, ascii_buffer, rest)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to write rest from file %s (%d).",
                   file_name, status);
         (void)smtp_close();
         return(INCORRECT);
      }
   }

   /* Close local file */
   if (close(fd) == -1)
   {
      msg_str[0] = '\0';
      trans_log(WARN_SIGN, __FILE__, __LINE__, "Failed to close() file %s",
                file_name);
   }

   if ((status = smtp_close()) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to close data mode (%d).", status);
      return(INCORRECT);
   }
   else
   {
      if (db->debug == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__, "Closed data mode");
      }
   }
   transfer_time = (double)(times(&tmsdummy) - start_time) / (double)clktck;
   msg_str[0] = '\0';
   trans_log(INFO_SIGN, NULL, 0, "%s %.2fs %.0f Bytes/s",
             (file_name[0] == '.') ? &file_name[1] : file_name,
             transfer_time, stat_buf.st_size / transfer_time);

   return(SUCCESS);
}

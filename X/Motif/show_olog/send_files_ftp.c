/*
 *  send_files_ftp.c - Part of AFD, an automatic file distribution program.
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
 **   send_files_ftp - sends files from the archive via FTP
 **
 ** SYNOPSIS
 **   void send_files_ftp(int no_selected, int *select_list)
 **
 ** DESCRIPTION
 **   send_files_ftp() will send all files selected in the show_olog
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
 **   26.06.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>         /* strerror(), strcmp(), strcpy(), strcat()  */
#include <stdlib.h>         /* calloc(), free()                          */
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
#include "ftpdefs.h"

/* External global variables */
extern Display          *display;
extern Widget           toplevel_w,
                        special_button_w,
                        scrollbar_w,
                        send_statusbox_w,
                        listbox_w;
extern int              no_of_log_files,
                        sys_log_fd,
                        special_button_flag;
extern char             archive_dir[],
                        *p_archive_name;
extern struct item_list *il;
extern struct send_data *db;
extern struct sol_perm  perm;

/* Local variables. */
int                     timeout_flag = OFF;
long                    transfer_timeout;
char                    *ascii_buffer = NULL,
                        msg_str[MAX_RET_MSG_LENGTH];

/* Local global variables */
static char             *p_file_name;
static clock_t          clktck;

/* Local function prototypes */
static int              get_archive_data(int, int),
                        send_file_ftp(char *);


/*############################ send_files_ftp() #########################*/
void
send_files_ftp(int                no_selected,
               int                *not_in_archive,
               int                *failed_to_send,
               int                *no_done,
               int                *select_done,
               struct resend_list *rl,
               int                *select_list,
               int                *user_limit)
{
   int i;

   if ((db->transfer_mode == 'A') || (db->transfer_mode == 'D'))
   {
      if (db->transfer_mode == 'D')
      {
         db->transfer_mode = 'I';
      }
      if ((ascii_buffer = (char *)malloc((DEFAULT_TRANSFER_BLOCKSIZE * 2))) == NULL)
      {
         msg_str[0] = '\0';
         trans_log(INFO_SIGN, __FILE__, __LINE__, "malloc() error : %s",
                   strerror(errno));
         return;
      }
   }

   transfer_timeout = db->timeout;
   if (ftp_connect(db->hostname, db->port) == SUCCESS)
   {
      if (db->debug == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__,
                   "Connected to %s at port %d", db->hostname, db->port);
      }
      if (ftp_user(db->user) == SUCCESS)
      {
         if (db->debug == YES)
         {
            trans_log(INFO_SIGN, __FILE__, __LINE__,
                      "Logged in as %s", db->user);
         }
         if (ftp_pass(db->password) == SUCCESS)
         {
            if (db->debug == YES)
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__,
                         "Entered password.");
            }
            if (ftp_type(db->transfer_mode) == SUCCESS)
            {
               int ret;

               if (db->debug == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__,
                            "Changed mode.");
               }
               if (db->target_dir[0] != '\0')
               {
                  if ((ret = ftp_cd(db->target_dir)) == SUCCESS)
                  {
                     if (db->debug == YES)
                     {
                        trans_log(INFO_SIGN, __FILE__, __LINE__,
                                  "Changed directory to %s.", db->target_dir);
                     }
                  }
                  else
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to change directory to %s.",
                               db->target_dir);
                  }
               }
               else
               {
                  ret = SUCCESS;
               }

               /* Prepare some data for send_file_ftp() */
               if ((clktck = sysconf(_SC_CLK_TCK)) > 0)
               {
                  int  lstatus;
                  char file_name[MAX_FILENAME_LENGTH];

                  if (db->lock == SET_LOCK_LOCKFILE)
                  {
                     /* Create lock file on remote host */
                     if ((lstatus = ftp_data(LOCK_FILENAME, 0, ACTIVE_MODE,
                                             DATA_WRITE)) != SUCCESS)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Failed to send lock file %s.",
                                  LOCK_FILENAME);
                     }
                     else
                     {
                        if (db->debug == YES)
                        {
                           trans_log(INFO_SIGN, __FILE__, __LINE__,
                                     "Opened remote lockfile %s.",
                                     LOCK_FILENAME);
                        }

                        /* Close remote lock file */
                        if (ftp_close_data(DATA_WRITE) != SUCCESS)
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Failed to close data connection for lock file %s.",
                                     LOCK_FILENAME);
                        }
                        else
                        {
                           if (db->debug == YES)
                           {
                              trans_log(INFO_SIGN, __FILE__, __LINE__,
                                        "Closed data connection for remote lock file %s.",
                                        LOCK_FILENAME);
                           }
                        }
                     }
                  }

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
                        if ((db->lock == SET_LOCK_DOT) ||
                            (db->lock == SET_LOCK_DOT_VMS))
                        {
                           file_name[0] = '.';
                           (void)strcpy(&file_name[1], p_file_name);
                        }
                        else if (db->lock == SET_LOCK_PREFIX)
                             {
                                (void)strcpy(file_name, db->prefix);
                                (void)strcat(file_name, p_file_name);
                             }
                             else
                             {
                                (void)strcpy(file_name, p_file_name);
                             }
                        if (send_file_ftp(file_name) < 0)
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
                           if ((db->lock == SET_LOCK_DOT) ||
                               (db->lock == SET_LOCK_PREFIX) ||
                               (db->lock == SET_LOCK_DOT_VMS))
                           {
                              if (db->lock == SET_LOCK_DOT_VMS)
                              {
                                 (void)strcat(p_file_name, ".");
                              }
                              if (ftp_move(file_name, p_file_name) != SUCCESS)
                              {
                                 trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                           "Failed to move remote file %s to %s.",
                                           file_name, p_file_name);
                              }
                              else
                              {
                                 if (db->debug == YES)
                                 {
                                    trans_log(INFO_SIGN, __FILE__, __LINE__,
                                              "Moved remote file %s to %s.",
                                              file_name, p_file_name);
                                 }
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

                  if ((db->lock == SET_LOCK_LOCKFILE) && (lstatus == SUCCESS))
                  {
                     if (ftp_dele(LOCK_FILENAME) != SUCCESS)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Failed to remove remote lock file %s.",
                                  LOCK_FILENAME);
                     }
                     else
                     {
                        if (db->debug == YES)
                        {
                           trans_log(INFO_SIGN, __FILE__, __LINE__,
                                     "Removed remote lock file %s.",
                                     LOCK_FILENAME);
                        }
                     }
                  }
               }
               else
               {
                  msg_str[0] = '\0';
                  if (clktck == -1)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__,
                               "sysconf() error : %s", strerror(errno));
                  }
                  else
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__,
                               "sysconf() error, option _SC_CLK_TCK not available.");
                  }
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to change mode.");
            }
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to enter password.");
         }
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to enter user %s.", db->user);
      }

      /*
       * Disconnect from remote host.
       */
      if (ftp_quit() == INCORRECT)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__,
                   "Failed to close connection.");
      }
   }
   else
   {
      /* Failed to connect to remote host. */
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to connect to %s at port %d",
                db->hostname, db->port);
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
      i++; ptr++;
   }
   *(p_archive_name + i++) = '/';

   /* Copy the file name to the archive directory. */
   (void)strcpy((p_archive_name + i), &buffer[0]);
   p_file_name = p_archive_name + i;

   return(SUCCESS);
}


/*++++++++++++++++++++++++++++ send_file_ftp() ++++++++++++++++++++++++++*/
static int
send_file_ftp(char *file_name)
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

   /* Open file on remote site */
   if ((status = ftp_data(file_name, 0, ACTIVE_MODE, DATA_WRITE)) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to open remote file %s (%d).", file_name, status);
      return(INCORRECT);
   }
   else
   {
      if (db->debug == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__,
                   "Open remote file %s.", file_name);
      }
   }

   /* Open local file */
   if ((fd = open(archive_dir, O_RDONLY)) == -1)
   {
      msg_str[0] = '\0';
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to open() locale file %s : %s",
                archive_dir, strerror(errno));
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
         return(INCORRECT);
      }
      if ((status = ftp_write(buffer, ascii_buffer, DEFAULT_TRANSFER_BLOCKSIZE)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to write to remote file %s (%d).",
                   file_name, status);
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
         return(INCORRECT);
      }
      if ((status = ftp_write(buffer, ascii_buffer, DEFAULT_TRANSFER_BLOCKSIZE)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to write rest to remote file %s (%d).",
                   file_name, status);
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

   if ((status = ftp_close_data(DATA_WRITE)) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to close remote file %s (%d).", file_name, status);
      return(INCORRECT);
   }
   else
   {
      if (db->debug == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__, "Closed remote file %s",
                   file_name);
      }
   }
   transfer_time = (double)(times(&tmsdummy) - start_time) / (double)clktck;
   msg_str[0] = '\0';
   trans_log(INFO_SIGN, NULL, 0, "%s %.2fs %.0fBytes/s",
             file_name, transfer_time, stat_buf.st_size / transfer_time);

   return(SUCCESS);
}

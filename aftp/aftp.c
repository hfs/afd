/*
 *  aftp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2001 Deutscher Wetterdienst (DWD),
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

#include "aftpdefs.h"

DESCR__S_M1
/*
 ** NAME
 **   aftp - send files via FTP automaticaly
 **
 ** SYNOPSIS
 **   [r]aftp [options] -h <host name | IP number> files
 **
 **   options
 **       --version               - Show current version
 **       -a <file size offset>   - offset of file name when doing a
 **                                 list command on the remote side.
 **       -b <block size>         - FTP block size in bytes.
 **       -c <config file>        - Use this configuration file instead
 **                                 of the -dpmu options.
 **       -d <remote directory>   - Directory where file(s) are to be
 **                                 stored.
 **       -f <filename file>      - List of filenames to send.
 **       -l <DOT | OFF | xyz.>   - How to lock the file on the remote
 **                                 site.
 **       -m <A | I>              - FTP transfer mode, ASCII or binary.
 **                                 Default is binary.
 **       -p <port number>        - Remote port number of FTP-server.
 **       -u <user> <password>    - Remote user name and password.
 **       -r                      - Remove transmitted file.
 **       -t <timout>             - FTP timeout in seconds.
 **       -v                      - Verbose. Shows all FTP commands
 **                                 and the reply from the remote server.
 **
 ** DESCRIPTION
 **   aftp sends the given files to the defined recipient via FTP
 **   It does so by using it's own FTP-client.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.09.1997 H.Kiehl Created
 **   23.03.1999 H.Kiehl Integrated aftp into AFD source tree.
 **   17.10.2000 H.Kiehl Added support to retrieve files from a remote
 **                      host.
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), sprintf()           */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* malloc(), free()               */
#include <ctype.h>                     /* isdigit()                      */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* unlink(), close()              */
#include <errno.h>
#include "ftpdefs.h"
#include "version.h"

/* Global variables */
int                  *no_of_listed_files,
                     sys_log_fd = STDERR_FILENO,
                     transfer_log_fd = STDERR_FILENO,
                     trans_db_log_fd = STDERR_FILENO,
                     timeout_flag,
                     sigpipe_flag;
long                 transfer_timeout;
char                 host_deleted = NO,
                     msg_str[MAX_RET_MSG_LENGTH],
                     tr_hostname[MAX_HOSTNAME_LENGTH + 1];
struct data          db;
struct filename_list *rl;

/* Local functions */
static void          aftp_exit(void),
                     sig_bus(int),
                     sig_segv(int),
                     sig_pipe(int),
                     sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int         fd,
               files_to_retrieve,
               j,
               status,
               no_of_bytes,
               loops,
               rest,
               files_send = 0,
               file_size_done = 0,
               no_of_files_done = 0;
   size_t      length;
   off_t       append_offset = 0,
               file_size_to_retrieve,
               local_file_size;
   char        *ascii_buffer = NULL,
               append_count = 0,
               *buffer,
               *file_ptr,
               initial_filename[MAX_FILENAME_LENGTH],
               final_filename[MAX_FILENAME_LENGTH];
   struct stat stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   db.filename = NULL;

   /* Do some cleanups when we exit */
   if (atexit(aftp_exit) != 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not register exit function : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR) ||
       (signal(SIGPIPE, sig_pipe) == SIG_ERR))
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "signal() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialise variables */
   init_aftp(argc, argv, &db);
   msg_str[0] = '\0';

   t_hostname(db.hostname, tr_hostname);

   /* Set FTP timeout value */
   transfer_timeout = db.transfer_timeout;

   /*
    * In ASCII-mode an extra buffer is needed to convert LF's
    * to CRLF. By creating this buffer the function ftp_write()
    * knows it has to send the data in ASCII-mode.
    */
   if (db.transfer_mode == 'A')
   {
      if ((ascii_buffer = (char *)malloc((db.blocksize * 2))) == NULL)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "malloc() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(ALLOC_ERROR);
      }
   }

   sigpipe_flag = timeout_flag = OFF;

   /* Connect to remote FTP-server */
   if (((status = ftp_connect(db.hostname, db.port)) != SUCCESS) &&
       (status != 230))
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "FTP connection to %s at port %d failed (%d).",
                db.hostname, db.port, status);
      if (timeout_flag == OFF)
      {
         exit(CONNECT_ERROR);
      }
      else
      {
         exit(TIMEOUT_ERROR);
      }
   }
   else
   {
      if (db.verbose == YES)
      {
         if (status == 230)
         {
            (void)rec(trans_db_log_fd, INFO_SIGN,
                      "%-*s: Connected. No login required. (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, __FILE__, __LINE__);
         }
         else
         {
            (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s: Connected. (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, __FILE__, __LINE__);
         }
         (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s: %s\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname, msg_str);
      }
   }

   /* Login */
   if (status != 230)
   {
      /* Send user name */
      if ((status = ftp_user(db.user)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to send user <%s> (%d).", db.user, status);
         (void)ftp_quit();
         if (timeout_flag == OFF)
         {
            exit(USER_ERROR);
         }
         else
         {
            exit(TIMEOUT_ERROR);
         }
      }
      else
      {
         if (db.verbose == YES)
         {
            trans_log(INFO_SIGN, __FILE__, __LINE__,
                      "Entered user name %s", db.user);
         }
      }

      /* Send password (if required) */
      if (status != 230)
      {
         if ((status = ftp_pass(db.password)) != SUCCESS)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to send password for user <%s> (%d).",
                      db.user, status);
            (void)ftp_quit();
            if (timeout_flag == OFF)
            {
               exit(PASSWORD_ERROR);
            }
            else
            {
               exit(TIMEOUT_ERROR);
            }
         }
         else
         {
            if (db.verbose == YES)
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__,
                         "Logged in as %s", db.user);
            }
         }
      } /* if (status != 230) */
   } /* if (status != 230) */

   /* Set transfer mode */
   if ((status = ftp_type(db.transfer_mode)) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to set transfer mode to %c (%d).",
                db.transfer_mode, status);
      (void)ftp_quit();
      if (timeout_flag == OFF)
      {
         exit(TYPE_ERROR);
      }
      else
      {
         exit(TIMEOUT_ERROR);
      }
   }
   else
   {
      if (db.verbose == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__,
                   "Changed transfer mode to %c", db.transfer_mode);
      }
   }

   /* Change directory if necessary */
   if (db.remote_dir[0] != '\0')
   {
      if ((status = ftp_cd(db.remote_dir)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to change directory to %s (%d).",
                   db.remote_dir, status);
         (void)ftp_quit();
         if (timeout_flag == OFF)
         {
            exit(CHDIR_ERROR);
         }
         else
         {
            exit(TIMEOUT_ERROR);
         }
      }
      else
      {
         if (db.verbose == YES)
         {
            trans_log(INFO_SIGN, __FILE__, __LINE__,
                      "Changed directory to %s", db.remote_dir);
         }
      }
   } /* if (db.remote_dir[0] != '\0') */

   /* Allocate buffer to read data from the source file. */
   if ((buffer = malloc(db.blocksize + 4)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(ALLOC_ERROR);
   }

   if (db.aftp_mode == RETRIEVE_MODE)
   {
      if ((files_to_retrieve = get_remote_file_names(&file_size_to_retrieve)) > 0)
      {
         int   i;
         off_t offset;
         char  local_file[MAX_PATH_LENGTH];

         local_file[0] = '.';
         for (i = 0; i < *no_of_listed_files; i++)
         {
            (void)strcpy(&local_file[1], rl[i].file_name);
            if (stat(local_file, &stat_buf) == -1)
            {
               offset = 0;
            }
            else
            {
               offset = stat_buf.st_size;
            }
            if (((status = ftp_data(rl[i].file_name, offset, db.ftp_mode,
                                    DATA_READ)) != SUCCESS) &&
                (status != -550))
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to open remote file %s (%d).",
                         rl[i].file_name, status);
               (void)ftp_quit();
               exit(OPEN_REMOTE_ERROR);
            }
            if (status == -550) /* ie. file has been deleted or is NOT a file. */
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__,
                         "Failed to open remote file %s (%d).",
                         rl[i].file_name, status);
            }
            else /* status == SUCCESS */
            {
               off_t bytes_done;

               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__,
                            "Opened data connection for file %s.",
                            rl[i].file_name);
               }

               if (offset > 0)
               {
                  fd = open(local_file, O_WRONLY | O_APPEND);
               }
               else
               {
                  fd = open(local_file, O_WRONLY | O_CREAT, FILE_MODE);
               }
               if (fd == -1)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to open local file %s : %s",
                            local_file, strerror(errno));
                  (void)ftp_quit();
                  exit(OPEN_LOCAL_ERROR);
               }
               else
               {
                  if (db.verbose == YES)
                  {
                     msg_str[0] = '\0';
                     trans_log(INFO_SIGN, __FILE__, __LINE__,
                               "Opened local file %s.", local_file);
                  }
               }
               bytes_done = 0;
               do
               {
                  if ((status = ftp_read(buffer, db.blocksize)) == INCORRECT)
                  {
                     msg_str[0] = '\0';
                     if ((sigpipe_flag == ON) && (status != EPIPE))
                     {
                        (void)ftp_get_reply();
                     }
                     trans_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to read from remote file %s",
                               rl[i].file_name);
                     if (status == EPIPE)
                     {
                        (void)rec(transfer_log_fd, DEBUG_SIGN,
                                  "%-*s: Hmm. Pipe is broken. Will NOT send a QUIT. (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  __FILE__, __LINE__);
                     }
                     else
                     {
                        (void)ftp_quit();
                     }
                     exit(READ_REMOTE_ERROR);
                  }
                  else if (status > 0)
                       {
                          if (write(fd, buffer, status) != status)
                          {
                             trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                       "Failed to write() to file %s : %s",
                                       local_file, strerror(errno));
                             (void)ftp_quit();
                             exit(WRITE_LOCAL_ERROR);
                          }
                          bytes_done += status;
                       }
               } while (status != 0);

               /* Close the FTP data connection. */
               if ((status = ftp_close_data(DATA_READ)) != SUCCESS)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to close data connection (%d).", status);
                  (void)ftp_quit();
                  exit(CLOSE_REMOTE_ERROR);
               }
               else
               {
                  if (db.verbose == YES)
                  {
                     trans_log(INFO_SIGN, __FILE__, __LINE__,
                               "Closed data connection for file %s.",
                               rl[i].file_name);
                  }
               }

               /* Close the local file. */
               if (close(fd) == -1)
               {
                  msg_str[0] = '\0';
                  trans_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to close() local file %s.", local_file);
               }
               else
               {
                  if (db.verbose == YES)
                  {
                     msg_str[0] = '\0';
                     trans_log(INFO_SIGN, __FILE__, __LINE__,
                               "Closed local file %s.", local_file);
                  }
               }
               /* Check if remote file is to be deleted. */
               if (db.remove == YES)
               {
                  if ((status = ftp_dele(rl[i].file_name)) != SUCCESS)
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__,
                               "Failed to delete remote file %s (%d).",
                               rl[i].file_name, status);
                  }
                  else
                  {
                     if (db.verbose == YES)
                     {
                        trans_log(INFO_SIGN, __FILE__, __LINE__,
                                  "Deleted remote file %s.", rl[i].file_name);
                     }
                  }
               }

               /*
                * If the file size is not the same as the one when we did
                * the remote ls command, give a warning in the transfer log
                * so some action can be taken against the originator.
                */
               if ((rl[i].size != -1) && ((bytes_done + offset) != rl[i].size))
               {
                  msg_str[0] = '\0';
                  trans_log(WARN_SIGN, __FILE__, __LINE__,
                            "File size of file %s changed from %u to %u when it was retrieved.",
                            rl[i].file_name, rl[i].size, bytes_done + offset);
                  rl[i].size = bytes_done;
               }

               /* Rename the file to indicate that download is done. */
               if (rename(local_file, &local_file[1]) == -1)
               {
                  msg_str[0] = '\0';
                  trans_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to rename() %s to %s : %s",
                            local_file, &local_file[1], strerror(errno));
               }
               else
               {
                  no_of_files_done++;
                  file_size_done += bytes_done;
                  if (offset > 0)
                  {
                     append_count++;
                  }
               }
            }
         }
      }
      (void)sprintf(msg_str, "%-*s: %d Bytes retrieved in %d file(s).",
                    MAX_HOSTNAME_LENGTH, tr_hostname,
                    file_size_done, no_of_files_done);

       if (append_count == 1)
       {
          (void)strcat(msg_str, " [APPEND]");
       }
       else if (append_count > 1)
            {
               char tmp_buffer[40];

               (void)sprintf(tmp_buffer, " [APPEND * %d]", append_count);
               (void)strcat(msg_str, tmp_buffer);
            }
      (void)rec(transfer_log_fd, INFO_SIGN, "%s\n", msg_str);
      msg_str[0] = '\0';
   }
   else
   {
      /* Send all files */
      for (files_send = 0; files_send < db.no_of_files; files_send++)
      {
         if (db.aftp_mode == TEST_MODE)
         {
            (void)sprintf(final_filename, "%s%010d",
                          db.filename[0], files_send);
         }
         else
         {
            file_ptr = db.filename[files_send];
            length = strlen(file_ptr);
            while (length != 0)
            {
               if (db.filename[files_send][length - 1] == '/')
               {
                  file_ptr = &db.filename[files_send][length];
                  break;
               }
               length--;
            }
            (void)strcpy(final_filename, file_ptr);
         }

         /* Send file in dot notation? */
         if (db.lock == DOT)
         {
            (void)strcpy(initial_filename, db.lock_notation);
            (void)strcat(initial_filename, final_filename);
         }
         else
         {
            (void)strcpy(initial_filename, final_filename);
         }

         /*
          * Check if the file has not already been partly
          * transmitted. If so, lets first get the size of the
          * remote file, to append it.
          */
         append_offset = 0;
         if (db.file_size_offset != -1)
         {
            if (db.file_size_offset == AUTO_SIZE_DETECT)
            {
               off_t remote_size;

               if ((status = ftp_size(initial_filename, &remote_size)) != SUCCESS)
               {
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                            "Failed to send SIZE command for file %s (%d).",
                            initial_filename, status);
                  if (timeout_flag == ON)
                  {
                     timeout_flag = OFF;
                  }
               }
               else
               {
                  append_offset = remote_size;
                  if ((db.verbose == YES) && (trans_db_log_fd != -1))
                  {
                     trans_log(INFO_SIGN, __FILE__, __LINE__,
                               "Remote size of %s is %d.",
                               initial_filename, status);
                  }
               }
            }
            else
            {
               char line_buffer[MAX_RET_MSG_LENGTH];

               if ((status = ftp_list(db.ftp_mode, LIST_CMD, initial_filename,
                                      line_buffer)) != SUCCESS)
               {
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                            "Failed to send LIST command for file %s (%d).",
                            initial_filename, status);
                  if (timeout_flag == ON)
                  {
                     timeout_flag = OFF;
                  }
               }
               else
               {
                  int  space_count = 0;
                  char *p_end_line,
                       *ptr = line_buffer;

                  /*
                   * Cut out remote file size, from ls command.
                   */
                  p_end_line = line_buffer + strlen(line_buffer);
                  do
                  {
                     while ((*ptr != ' ') && (*ptr != '\t') &&
                            (ptr < p_end_line))
                     {
                        ptr++;
                     }
                     if ((*ptr == ' ') || (*ptr == '\t'))
                     {
                        space_count++;
                        while (((*ptr == ' ') || (*ptr == '\t')) &&
                               (ptr < p_end_line))
                        {
                           ptr++;
                        }
                     }
                     else
                     {
                        (void)rec(sys_log_fd, WARN_SIGN,
                                  "The <file size offset> for host %s is to large! (%s %d)\n",
                                  tr_hostname, __FILE__, __LINE__);
                        space_count = -1;
                        break;
                     }
                  } while (space_count != db.file_size_offset);

                  if ((space_count > -1) && (space_count == db.file_size_offset))
                  {
                     char *p_end = ptr;

                     while ((isdigit(*p_end) != 0) && (p_end < p_end_line))
                     {
                        p_end++;
                     }
                     *p_end = '\0';
                     append_offset = atoi(ptr);
                  }
               }
            }
         }

         /* Open file on remote site */
         if ((status = ftp_data(initial_filename, append_offset,
                                db.ftp_mode, DATA_WRITE)) != SUCCESS)
         {
            (void)rec(transfer_log_fd, INFO_SIGN,
                      "%-*s: %d Bytes send in %d file(s).\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname,
                      file_size_done, no_of_files_done);
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to open remote file %s (%d).",
                      initial_filename, status);
            (void)ftp_quit();
            if (timeout_flag == OFF)
            {
               exit(OPEN_REMOTE_ERROR);
            }
            else
            {
               exit(TIMEOUT_ERROR);
            }
         }
         else
         {
            if (db.verbose == YES)
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__,
                         "Open remote file %s", initial_filename);
            }
         }

         if (db.aftp_mode == TEST_MODE)
         {
            local_file_size = db.dummy_size;
         }
         else
         {
            /* Open local file */
            if ((fd = open(db.filename[files_send], O_RDONLY)) < 0)
            {
               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__,
                            "Failed to open() local file %s",
                            db.filename[files_send]);
               }
               (void)rec(transfer_log_fd, INFO_SIGN,
                         "%-*s: %d Bytes send in %d file(s).\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname,
                         file_size_done, no_of_files_done);
               (void)ftp_quit();
               exit(OPEN_LOCAL_ERROR);
            }
            if (fstat(fd, &stat_buf) == -1)
            {
               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__,
                            "Failed to fstat() local file %s",
                            db.filename[files_send]);
               }
               (void)rec(transfer_log_fd, INFO_SIGN,
                         "%-*s: %d Bytes send in %d file(s).\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname,
                         file_size_done, no_of_files_done);
               (void)ftp_quit();
               exit(STAT_LOCAL_ERROR);
            }
            local_file_size = stat_buf.st_size;
            if (db.verbose == YES)
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__,
                         "Open local file %s", db.filename[files_send]);
            }
            if (append_offset > 0)
            {
               if ((local_file_size - append_offset) > 0)
               {
                  if (lseek(fd, append_offset, SEEK_SET) < 0)
                  {
                     append_offset = 0;
                     (void)rec(transfer_log_fd, WARN_SIGN,
                               "%-*s: Failed to seek() in %s (Ignoring append): %s (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, final_filename,
                               strerror(errno), __FILE__, __LINE__);
                     if (db.verbose == YES)
                     {
                        trans_log(WARN_SIGN, __FILE__, __LINE__,
                                  "Failed to seek() in %s (Ignoring append): %s",
                                  final_filename, strerror(errno));
                     }
                  }
                  else
                  {
                     append_count++;
                     if (db.verbose == YES)
                     {
                        trans_log(INFO_SIGN, __FILE__, __LINE__,
                                  "Appending file %s.", final_filename);
                     }
                  }
               }
               else
               {
                  append_offset = 0;
               }
            }
         }

         /* Read (local) and write (remote) file */
         no_of_bytes = 0;
         loops = (local_file_size - append_offset) / db.blocksize;
         rest = (local_file_size - append_offset) % db.blocksize;

         if (db.aftp_mode == TRANSFER_MODE)
         {
            for (;;)
            {
               for (j = 0; j < loops; j++)
               {
                  if (read(fd, buffer, db.blocksize) != db.blocksize)
                  {
                     if (db.verbose == YES)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Could not read local file %s : %s",
                                  final_filename, strerror(errno));
                     }
                     (void)rec(transfer_log_fd, INFO_SIGN,
                               "%-*s: %d Bytes send in %d file(s).\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               file_size_done, no_of_files_done);
                     (void)ftp_quit();
                     exit(READ_LOCAL_ERROR);
                  }
                  if ((status = ftp_write(buffer, ascii_buffer,
                                          db.blocksize)) != SUCCESS)
                  {
                     /*
                      * It could be that we have received a SIGPIPE
                      * signal. If this is the case there might be data
                      * from the remote site on the control connection.
                      * Try to read this data into the global variable
                      * 'msg_str'.
                      */
                     msg_str[0] = '\0';
                     if (sigpipe_flag == ON)
                     {
                        (void)ftp_get_reply();
                     }
                     (void)rec(transfer_log_fd, INFO_SIGN,
                               "%-*s: %d Bytes send in %d file(s).\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               file_size_done, no_of_files_done);
                     trans_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to write to remote file %s after writing %d Bytes.",
                               initial_filename, file_size_done);
                     if (status == EPIPE)
                     {
                        (void)rec(transfer_log_fd, DEBUG_SIGN,
                                  "%-*s: Hmm. Pipe is broken. Will NOT send a QUIT. (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  __FILE__, __LINE__);
                     }
                     else
                     {
                        (void)ftp_quit();
                     }
                     if (timeout_flag == OFF)
                     {
                        exit(WRITE_REMOTE_ERROR);
                     }
                     else
                     {
                        exit(TIMEOUT_ERROR);
                     }
                  }

                  file_size_done += db.blocksize;
                  no_of_bytes += db.blocksize;
               }
               if (rest > 0)
               {
                  if (read(fd, buffer, rest) != rest)
                  {
                     if (db.verbose == YES)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Could not read local file %s : %s",
                                  final_filename, strerror(errno));
                     }
                     (void)rec(transfer_log_fd, INFO_SIGN,
                               "%-*s: %d Bytes send in %d file(s).\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               file_size_done, no_of_files_done);
                     (void)ftp_quit();
                     exit(READ_LOCAL_ERROR);
                  }
                  if ((status = ftp_write(buffer, ascii_buffer, rest)) != SUCCESS)
                  {
                     /*
                      * It could be that we have received a SIGPIPE
                      * signal. If this is the case there might be data
                      * from the remote site on the control connection.
                      * Try to read this data into the global variable
                      * 'msg_str'.
                      */
                     msg_str[0] = '\0';
                     if (sigpipe_flag == ON)
                     {
                        (void)ftp_get_reply();
                     }
                     if (db.verbose == YES)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Failed to write to remote file %s",
                                  initial_filename);
                     }
                     (void)rec(transfer_log_fd, INFO_SIGN,
                               "%-*s: %d Bytes send in %d file(s).\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               file_size_done, no_of_files_done);
                     trans_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to write rest to remote file %s",
                               initial_filename);
                     if (status == EPIPE)
                     {
                        (void)rec(transfer_log_fd, DEBUG_SIGN,
                                  "%-*s: Hmm. Pipe is broken. Will NOT send a QUIT. (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  __FILE__, __LINE__);
                     }
                     else
                     {
                        (void)ftp_quit();
                     }
                     if (timeout_flag == OFF)
                     {
                        exit(WRITE_REMOTE_ERROR);
                     }
                     else
                     {
                        exit(TIMEOUT_ERROR);
                     }
                  }

                  file_size_done += rest;
                  no_of_bytes += rest;
               }

               /*
                * Since there are always some users sending files to the
                * AFD not in dot notation, lets check here if this is really
                * the EOF.
                * If not lets continue so long until we hopefully have reached
                * the EOF.
                * NOTE: This is NOT a fool proof way. There must be a better
                *       way!
                */
               if (stat(db.filename[files_send], &stat_buf) == 0)
               {
                  if (stat_buf.st_size > local_file_size)
                  {
                     loops = (stat_buf.st_size - local_file_size) / db.blocksize;
                     rest = (stat_buf.st_size - local_file_size) % db.blocksize;
                     local_file_size = stat_buf.st_size;

                     /*
                      * Give a warning in the system log, so some action
                      * can be taken against the originator.
                      */
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "Someone is still writting to file %s. (%s %d)\n",
                               db.filename[files_send], __FILE__, __LINE__);
                  }
                  else
                  {
                     break;
                  }
               }
               else
               {
                  break;
               }
            } /* for (;;) */

            /* Close local file */
            if (close(fd) < 0)
            {
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s: Failed to close() local file %s : %s (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname,
                         final_filename, strerror(errno), __FILE__, __LINE__);

               /*
                * Hmm. Lets hope this does not happen to offen. Else
                * we will have too many file descriptors open.
                */
            }
         }
         else /* TEST_MODE, write dummy files. */
         {
            for (j = 0; j < loops; j++)
            {
               if ((status = ftp_write(buffer, ascii_buffer,
                                       db.blocksize)) != SUCCESS)
               {
                  /*
                   * It could be that we have received a SIGPIPE
                   * signal. If this is the case there might be data
                   * from the remote site on the control connection.
                   * Try to read this data into the global variable
                   * 'msg_str'.
                   */
                  msg_str[0] = '\0';
                  if (sigpipe_flag == ON)
                  {
                     (void)ftp_get_reply();
                  }
                  (void)rec(transfer_log_fd, INFO_SIGN,
                            "%-*s: %d Bytes send in %d file(s).\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            file_size_done, no_of_files_done);
                  trans_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to write to remote file %s after writing %d Bytes.",
                            initial_filename, file_size_done);
                  if (status == EPIPE)
                  {
                     (void)rec(transfer_log_fd, DEBUG_SIGN,
                               "%-*s: Hmm. Pipe is broken. Will NOT send a QUIT. (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               __FILE__, __LINE__);
                  }
                  else
                  {
                     (void)ftp_quit();
                  }
                  if (timeout_flag == OFF)
                  {
                     exit(WRITE_REMOTE_ERROR);
                  }
                  else
                  {
                     exit(TIMEOUT_ERROR);
                  }
               }

               file_size_done += db.blocksize;
               no_of_bytes += db.blocksize;
            }
            if (rest > 0)
            {
               if ((status = ftp_write(buffer, ascii_buffer, rest)) != SUCCESS)
               {
                  /*
                   * It could be that we have received a SIGPIPE
                   * signal. If this is the case there might be data
                   * from the remote site on the control connection.
                   * Try to read this data into the global variable
                   * 'msg_str'.
                   */
                  msg_str[0] = '\0';
                  if (sigpipe_flag == ON)
                  {
                     (void)ftp_get_reply();
                  }
                  if (db.verbose == YES)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to write to remote file %s",
                               initial_filename);
                  }
                  (void)rec(transfer_log_fd, INFO_SIGN,
                            "%-*s: %d Bytes send in %d file(s).\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            file_size_done, no_of_files_done);
                  trans_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to write rest to remote file %s",
                            initial_filename);
                  if (status == EPIPE)
                  {
                     (void)rec(transfer_log_fd, DEBUG_SIGN,
                               "%-*s: Hmm. Pipe is broken. Will NOT send a QUIT. (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               __FILE__, __LINE__);
                  }
                  else
                  {
                     (void)ftp_quit();
                  }
                  if (timeout_flag == OFF)
                  {
                     exit(WRITE_REMOTE_ERROR);
                  }
                  else
                  {
                     exit(TIMEOUT_ERROR);
                  }
               }

               file_size_done += rest;
               no_of_bytes += rest;
            }
         }

         /* Close remote file */
         if ((status = ftp_close_data(DATA_WRITE)) != SUCCESS)
         {
            /*
             * Closing files that have zero length is not possible
             * on some systems. So if this is the case lets not count
             * this as an error. Just ignore it, but send a message in
             * the transfer log, so the user sees that he is trying
             * to send files with zero length.
             */
            if ((local_file_size > 0) || (timeout_flag == ON))
            {
               (void)rec(transfer_log_fd, INFO_SIGN,
                         "%-*s: %d Bytes send in %d file(s).\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname,
                         file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to close remote file %s", initial_filename);
               (void)ftp_quit();
               if (timeout_flag == OFF)
               {
                  exit(CLOSE_REMOTE_ERROR);
               }
               else
               {
                  exit(TIMEOUT_ERROR);
               }
            }
            else
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__,
                         "Failed to close remote file %s (%d). Ignoring since file size is %d.",
                         initial_filename, status, local_file_size);
            }
         }
         else
         {
            if (db.verbose == YES)
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__,
                         "Closed remote file %s", initial_filename);
            }
         }

         if (db.verbose == YES)
         {
            char line_buffer[MAX_RET_MSG_LENGTH];

            if ((status = ftp_list(db.ftp_mode, LIST_CMD, initial_filename,
                                   line_buffer)) != SUCCESS)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__,
                         "Failed to list remote file %s (%d).",
                         initial_filename, status);
               if (timeout_flag == ON)
               {
                  timeout_flag = OFF;
               }
            }
            else
            {
               (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s: %s\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, line_buffer);
               (void)rec(trans_db_log_fd, INFO_SIGN,
                         "%-*s: Local file size of %s is %d (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, final_filename,
                         (int)stat_buf.st_size, __FILE__, __LINE__);
            }
         }

         /* If we used dot notation, don't forget to rename */
         if (db.lock == DOT)
         {
            if ((status = ftp_move(initial_filename, final_filename)) != SUCCESS)
            {
               (void)rec(transfer_log_fd, INFO_SIGN,
                         "%-*s: %d Bytes send in %d file(s).\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname,
                         file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to move remote file %s to %s (%d)",
                         initial_filename, final_filename, status);
               (void)ftp_quit();
               if (timeout_flag == OFF)
               {
                  exit(MOVE_REMOTE_ERROR);
               }
               else
               {
                  exit(TIMEOUT_ERROR);
               }
            }
            else
            {
               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__,
                            "Renamed remote file %s to %s",
                            initial_filename, final_filename);
               }
            }
         }

#ifdef _WITH_READY_FILES
         if ((db.lock == READY_A_FILE) || (db.lock == READY_B_FILE))
         {
            int  rdy_length;
            char file_type,
                 ready_file_name[MAX_FILENAME_LENGTH],
                 ready_file_buffer[MAX_PATH_LENGTH + 25];

            /* Generate the name for the ready file */
            (void)sprintf(ready_file_name, ".%s_rdy", final_filename);

            /* Open ready file on remote site */
            if ((status = ftp_data(ready_file_name, append_offset,
                                   db.ftp_mode, DATA_WRITE)) != SUCCESS)
            {
               (void)rec(transfer_log_fd, INFO_SIGN,
                         "%-*s: %d Bytes send in %d file(s).\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname,
                         file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to open remote ready file %s (%d).",
                         ready_file_name, status);
               (void)ftp_quit();
               if (timeout_flag == OFF)
               {
                  exit(OPEN_REMOTE_ERROR);
               }
               else
               {
                  exit(TIMEOUT_ERROR);
               }
            }
            else
            {
               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__,
                            "Open remote ready file %s", ready_file_name);
               }
            }

            /* Create contents for ready file */
            if (db.lock == READY_A_FILE)
            {
               file_type = 'A';
            }
            else
            {
               file_type = 'B';
            }
            rdy_length = sprintf(ready_file_buffer,
                                 "%s %c U\n$$end_of_ready_file\n",
                                 initial_filename, file_type);

            /* Write remote ready file in one go. */
            if ((status = ftp_write(ready_file_buffer, NULL,
                                    rdy_length)) != SUCCESS)
            {
               /*
                * It could be that we have received a SIGPIPE
                * signal. If this is the case there might be data
                * from the remote site on the control connection.
                * Try to read this data into the global variable
                * 'msg_str'.
                */
               msg_str[0] = '\0';
               if (sigpipe_flag == ON)
               {
                  (void)ftp_get_reply();
               }
               (void)rec(transfer_log_fd, INFO_SIGN,
                         "%-*s: %d Bytes send in %d file(s).\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname,
                         file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to write to remote ready file %s (%d).",
                         ready_file_name, status);
               if (status == EPIPE)
               {
                  (void)rec(transfer_log_fd, DEBUG_SIGN,
                            "%-*s: Hmm. Pipe is broken. Will NOT send a QUIT. (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            __FILE__, __LINE__);
               }
               else
               {
                  (void)ftp_quit();
               }
               if (timeout_flag == OFF)
               {
                  exit(WRITE_REMOTE_ERROR);
               }
               else
               {
                  exit(TIMEOUT_ERROR);
               }
            }

            /* Close remote ready file */
            if ((status = ftp_close_data(DATA_WRITE)) != SUCCESS)
            {
               (void)rec(transfer_log_fd, INFO_SIGN,
                         "%-*s: %d Bytes send in %d file(s).\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname,
                         file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to close remote ready file %s (%d).",
                         ready_file_name, status);
               (void)ftp_quit();
               if (timeout_flag == OFF)
               {
                  exit(CLOSE_REMOTE_ERROR);
               }
               else
               {
                  exit(TIMEOUT_ERROR);
               }
            }
            else
            {
               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__,
                            "Closed remote ready file %s", ready_file_name);
               }
            }

            /* Remove leading dot from ready file */
            if ((status = ftp_move(ready_file_name, &ready_file_name[1])) != SUCCESS)
            {
               (void)rec(transfer_log_fd, INFO_SIGN,
                         "%-*s: %d Bytes send in %d file(s).\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname,
                         file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to move remote ready file %s to %s (%d)",
                         ready_file_name, &ready_file_name[1], status);
               (void)ftp_quit();
               if (timeout_flag == OFF)
               {
                  exit(MOVE_REMOTE_ERROR);
               }
               else
               {
                  exit(TIMEOUT_ERROR);
               }
            }
            else
            {
               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__,
                            "Renamed remote ready file %s to %s",
                            ready_file_name, &ready_file_name[1]);
               }
            }
         }
#endif /* _WITH_READY_FILES */

         no_of_files_done++;

         if ((db.remove == YES) && (db.aftp_mode == TRANSFER_MODE))
         {
            /* Delete the file we just have send */
            if (unlink(db.filename[files_send]) < 0)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Could not unlink() local file %s after sending it successfully : %s (%s %d)\n",
                         strerror(errno), db.filename[files_send],
                         __FILE__, __LINE__);
            }
         }
      } /* for (files_send = 0; files_send < db.no_of_files; files_send++) */

      (void)sprintf(msg_str, "%-*s: %d Bytes send in %d file(s).",
                    MAX_HOSTNAME_LENGTH, tr_hostname,
                    file_size_done, no_of_files_done);

       if (append_count == 1)
       {
          (void)strcat(msg_str, " [APPEND]");
       }
       else if (append_count > 1)
            {
               char tmp_buffer[40];

               (void)sprintf(tmp_buffer, " [APPEND * %d]", append_count);
               (void)strcat(msg_str, tmp_buffer);
            }
      (void)rec(transfer_log_fd, INFO_SIGN, "%s\n", msg_str);
      msg_str[0] = '\0';
   }

   free(buffer);

   /* Logout again */
   if ((status = ftp_quit()) != SUCCESS)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__,
                "Failed to disconnect from remote host (%d).", status);
   }
   else
   {
      if (db.verbose == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__, "Logged out.");
      }
   }

   /* Don't need the ASCII buffer */
   if (db.transfer_mode == 'A')
   {
      free(ascii_buffer);
   }

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++++ aftp_exit() ++++++++++++++++++++++++++++*/
static void
aftp_exit(void)
{
   if (db.filename != NULL)
   {
      FREE_RT_ARRAY(db.filename);
   }
}


/*++++++++++++++++++++++++++++++ sig_pipe() +++++++++++++++++++++++++++++*/
static void
sig_pipe(int signo)
{
   /* Ignore any future signals of this kind. */
   if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "signal() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   sigpipe_flag = ON;

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   (void)rec(sys_log_fd, DEBUG_SIGN,
             "Aaarrrggh! Received SIGSEGV. Remove the programmer who wrote this! (%s %d)\n",
             __FILE__, __LINE__);
   exit(INCORRECT);
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   (void)rec(sys_log_fd, DEBUG_SIGN,
             "Uuurrrggh! Received SIGBUS. (%s %d)\n",
             __FILE__, __LINE__);
   exit(INCORRECT);
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}

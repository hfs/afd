/*
 *  handle_request.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999, 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_request - handles a TCP request
 **
 ** SYNOPSIS
 **   void handle_request(int sd, int pos)
 **
 ** DESCRIPTION
 **   Handles all request from remote user in a loop. If user is inactive
 **   for AFDD_CMD_TIMEOUT seconds, the connection will be closed.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.01.1999 H.Kiehl Created
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* memset()                                */
#include <stdlib.h>           /* atoi()                                  */
#include <ctype.h>            /* toupper()                               */
#include <signal.h>           /* signal()                                */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>           /* close()                                 */
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include "afdddefs.h"
#include "logdefs.h"
#include "version.h"

/* External global variables. */
extern int  sys_log_fd;
extern char afd_name[],
            hostname[],
            *p_work_dir,
            *p_work_dir_end;

/* Local global variables. */
static int  report_changes = NO;
static FILE *p_data = NULL;

/*  Local function prototypes. */
static void report_shutdown(void);


/*########################### handle_request() ##########################*/
void
handle_request(int sd, int pos)
{
   register int   i,
                  j;
   int            nbytes,
                  status;
   time_t         last,
                  last_time_read,
                  now,
                  report_changes_interval = DEFAULT_CHECK_INTERVAL;
   char           cmd[1024];
   fd_set         rset;
   struct timeval timeout;

   if ((p_data = fdopen(sd, "r+")) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "fdopen() control error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (atexit(report_shutdown) != 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not register exit handler : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   /* Give some information to the user where he is and what service */
   /* is offered here.                                               */
#ifdef PRE_RELEASE
   (void)fprintf(p_data,
                 "220 %s AFD server %s (PRE %d.%d.%d-%d) ready.\r\n",
                 hostname, afd_name, MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
   (void)fprintf(p_data,
                 "220 %s AFD server %s (Version %d.%d.%d) ready.\r\n",
                 hostname, afd_name, MAJOR, MINOR, BUG_FIX);
#endif
   (void)fflush(p_data);

   /*
    * Handle all request until the remote user has entered QUIT_CMD or
    * the connection was idle for AFDD_CMD_TIMEOUT seconds.
    */
   now = last = last_time_read = time(NULL);
   for (;;)
   {
      now = time(NULL);
      nbytes = 0;
      if (report_changes == YES)
      {
         if ((now - last) >= report_changes_interval)
         {
            check_changes(p_data);
            timeout.tv_sec = report_changes_interval;
            last = now = time(NULL);
         }
         else
         {
            timeout.tv_sec = report_changes_interval - (now - last);
            last = now;
         }
      }
      else
      {
         timeout.tv_sec = AFDD_CMD_TIMEOUT;
      }

      if ((now - last_time_read) > AFDD_CMD_TIMEOUT)
      {
         (void)fprintf(p_data,
                       "421 Timeout (%d seconds): closing connection.\r\n",
                       AFDD_CMD_TIMEOUT);
         break;
      }
      FD_ZERO(&rset);
      FD_SET(sd, &rset);
      timeout.tv_usec = 0;

      /* Wait for message x seconds and then continue. */
      status = select(sd + 1, &rset, NULL, NULL, &timeout);

      if (FD_ISSET(sd, &rset))
      {
         if ((nbytes = read(sd, cmd, 1024)) <= 0)
         {
            if (nbytes == 0)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Remote hangup by %s (%s %d)\n",
                         hostname, __FILE__, __LINE__);
            }
            else
            {
#ifdef ECONNRESET
               if (errno == ECONNRESET)
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "read() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
               }
               else
               {
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "read() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
               }
#else
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "read() error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
#endif
            }
            break;
         }
         last_time_read = time(NULL);
      }
      else if ((status == 0) && (report_changes == YES))
           {
              if (report_changes == YES)
              {
                 /*
                  * Check if there have been any changes. If no value has changed
                  * just be silent and do nothing.
                  */
                 check_changes(p_data);
              }
              else
              {
                 (void)fprintf(p_data,
                               "421 Timeout (%d seconds): closing connection.\r\n",
                               AFDD_CMD_TIMEOUT);
                 break;
              }
           }

      if (nbytes > 0)
      {
         i = 0;
         while ((cmd[i] != ' ') && (cmd[i] != '\r'))
         {
            cmd[i] = toupper((int)cmd[i]);
            i++;
         }
         cmd[nbytes] = '\0';
         if (strcmp(cmd, QUIT_CMD) == 0)
         {
            (void)fprintf(p_data, "221 Goodbye.\r\n");
            break;
         }
         else if (strcmp(cmd, HELP_CMD) == 0)
              {
                 (void)fprintf(p_data,
                               "214- The following commands are recognized (* =>'s unimplemented).\n\
   *AFDSTAT *DISC    HELP     ILOG     *INFO    *LDB     LRF      OLOG\n\
   *PROC    QUIT     SLOG     *STAT    TDLOG    TLOG     *TRACEF  *TRACEI\n\
   *TRACEO  SSTAT\n\
214 Direct comments to %s\r\n",
                                  AFD_MAINTAINER);
              }
         else if ((cmd[0] == 'H') && (cmd[1] == 'E') && (cmd[2] == 'L') &&
                  (cmd[3] == 'P') && (cmd[4] == ' ') && (cmd[5] != '\r'))
              {
                 j = 5;
                 while ((cmd[j] != ' ') && (cmd[j] != '\r'))
                 {
                    cmd[j] = toupper((int)cmd[j]);
                    j++;
                 }

                 if (strcmp(&cmd[5], QUIT_CMD) == 0)
                 {
                    (void)fprintf(p_data, "%s\r\n", QUIT_SYNTAX);
                 }
                 else if (strcmp(&cmd[5], HELP_CMD) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", HELP_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], TRACEI_CMDL) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", TRACEI_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], TRACEO_CMDL) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", TRACEO_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], TRACEF_CMDL) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", TRACEF_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], ILOG_CMDL) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", ILOG_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], OLOG_CMDL) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", OLOG_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], SLOG_CMDL) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", SLOG_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], TLOG_CMDL) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", TLOG_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], TDLOG_CMDL) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", TDLOG_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], PROC_CMD) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", PROC_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], DISC_CMD) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", DISC_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], STAT_CMDL) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", STAT_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], START_STAT_CMDL) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", START_STAT_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], LDB_CMD) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", LDB_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], LRF_CMD) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", LRF_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], INFO_CMDL) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", INFO_SYNTAX);
                      }
                 else if (strcmp(&cmd[5], AFDSTAT_CMDL) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", AFDSTAT_SYNTAX);
                      }
                      else
                      {
                         *(cmd + nbytes - 2) = '\0';
                         (void)fprintf(p_data,
                                       "502 Unknown command %s\r\n",
                                       &cmd[5]);
                      }
              }
              else if (strncmp(cmd, TRACEI_CMD, strlen(TRACEI_CMD)) == 0)
                   {
                      (void)fprintf(p_data, "502 Service not implemented. See help for commands.\r\n");
                   }
              else if (strncmp(cmd, TRACEO_CMD, strlen(TRACEO_CMD)) == 0)
                   {
                      (void)fprintf(p_data, "502 Service not implemented. See help for commands.\r\n");
                   }
              else if (strncmp(cmd, TRACEF_CMD, strlen(TRACEF_CMD)) == 0)
                   {
                      (void)fprintf(p_data, "502 Service not implemented. See help for commands.\r\n");
                   }
              else if ((strncmp(cmd, ILOG_CMD, strlen(ILOG_CMD)) == 0) ||
                       (strncmp(cmd, OLOG_CMD, strlen(OLOG_CMD)) == 0) ||
                       (strncmp(cmd, SLOG_CMD, strlen(SLOG_CMD)) == 0) ||
                       (strncmp(cmd, TLOG_CMD, strlen(TLOG_CMD)) == 0) ||
                       (strncmp(cmd, TDLOG_CMD, strlen(TDLOG_CMD)) == 0))
                   {
                      char search_file[MAX_PATH_LENGTH];

                      /*
                       * First lets determine what file we want and then
                       * create the full file name without the number.
                       */
                      (void)sprintf(search_file, "%s%s/", p_work_dir,
                                    LOG_DIR);
                      switch (cmd[0])
                      {
                         case 'I' : /* Input log */
                            (void)strcat(search_file, INPUT_BUFFER_FILE);
                            break;
                         case 'O' : /* Output log */
                            (void)strcat(search_file, OUTPUT_BUFFER_FILE);
                            break;
                         case 'S' : /* System log */
                            (void)strcat(search_file, SYSTEM_LOG_NAME);
                            break;
                         case 'T' : /* Transfer or transfer debug log */
                            if (cmd[1] == 'D')
                            {
                               (void)strcat(search_file, TRANS_DB_LOG_NAME);
                            }
                            else
                            {
                               (void)strcat(search_file, TRANSFER_LOG_NAME);
                            }
                            break;
                         default  : /* Impossible!!! */
                            (void)rec(sys_log_fd, DEBUG_SIGN, "Unknown error! (%s %d)\n",
                                      __FILE__, __LINE__);
                            (void)fprintf(p_data,
                                          "500 Unknown error. (%s %d)\n",
                                          __FILE__, __LINE__);
                            (void)fflush(p_data);
                            (void)fclose(p_data);
                            p_data = NULL;
                            exit(INCORRECT);
                      }

                      if (cmd[i] == ' ')
                      {
                         if ((cmd[i + 1] == '-') || (cmd[i + 1] == '+') ||
                             (cmd[i + 1] == '#'))
                         {
                            int  k = 0,
                                 lines = EVERYTHING,
                                 show_time = EVERYTHING,
                                 file_no = DEFAULT_FILE_NO,
                                 faulty = NO;
                            char numeric_str[MAX_INT_LENGTH];

                            do
                            {
                               i += k;
                               k = 0;
                               if (cmd[i + 1 + k] == '*')
                               {
                                  if (cmd[i + 1] == '#')
                                  {
                                     file_no = EVERYTHING;
                                  }
                               }
                               else
                               {
                                  while ((cmd[i + 1 + k] != ' ') &&
                                         (cmd[i + 1 + k] != '\r') &&
                                         (k < MAX_INT_LENGTH))
                                  {
                                     if (isdigit(cmd[i + 1 + k]))
                                     {
                                        numeric_str[k] = cmd[i + 1 + k];
                                     }
                                     else
                                     {
                                        faulty = YES;
                                        (void)fprintf(p_data,
                                                      "500 Expecting numeric value after \'%c\'\r\n",
                                                      cmd[i + 1]);
                                        break;
                                     }
                                     k++;
                                  }
                                  if (k > 0)
                                  {
                                     numeric_str[k] = '\0';
                                     switch(cmd[i + 1])
                                     {
                                        case '#' : /* File number */
                                           file_no = atoi(numeric_str);
                                           break;
                                        case '-' : /* number of lines */
                                           lines = atoi(numeric_str);
                                           break;
                                        case '+' : /* Duration of displaying data. */
                                           show_time = atoi(numeric_str);
                                           break;
                                        default  : /* Impossible!!! */
                                           faulty = YES;
                                           (void)rec(sys_log_fd, DEBUG_SIGN, "Unknown error! (%s %d)\n",
                                                     __FILE__, __LINE__);
                                           (void)fprintf(p_data,
                                                         "500 Unknown error. (%s %d)\r\n",
                                                         __FILE__, __LINE__);
                                           break;
                                     }
                                  }
                                  else
                                  {
                                     faulty = YES;
                                     (void)fprintf(p_data,
                                                   "500 No numeric value supplied after \'%c\'\r\n",
                                                   cmd[i + 1]);
                                  }
                               }
                            } while ((cmd[i + 1 + k] != '\r') && (faulty == NO));

                            if (faulty == NO)
                            {
                               get_display_data(search_file, NULL, lines, show_time, file_no);
                            }
                         }
                         else if (isascii(cmd[i + 1]))
                              {
                                 int  k = 0;
                                 char *ptr,
			       search_string[256];

                                 /* User has supplied a search string. */
                                 ptr = &cmd[i + 1];
                                 while ((*ptr != ' ') &&
                                        (*ptr != '\r') &&
                                        (*ptr != '\n'))
                                 {
                                    search_string[k] = *ptr;
                                    ptr++; k++;
                                 }
                                 if (*ptr == ' ')
                                 {
                                    search_string[k] = '\0';
                                    if ((cmd[i + k + 1] == '-') || (cmd[i + k + 1] == '+') ||
                                        (cmd[i + k + 1] == '#'))
                                    {
                                       int  m = 0,
                                            lines = EVERYTHING,
                                            show_time = EVERYTHING,
                                            file_no = DEFAULT_FILE_NO,
                                            faulty = NO;
                                       char numeric_str[MAX_INT_LENGTH];

                                       do
                                       {
                                          i += m;
                                          m = 0;
                                          if (cmd[i + k + 1 + m] == '*')
                                          {
                                             if (cmd[i + k + 1] == '#')
                                             {
                                                file_no = EVERYTHING;
                                             }
                                          }
                                          else
                                          {
                                             while ((cmd[i + k + 1 + m] != ' ') &&
                                                    (cmd[i + k + 1 + m] != '\r') &&
                                                    (m < MAX_INT_LENGTH))
                                             {
                                                if (isdigit(cmd[i + k + 1 + m]))
                                                {
                                                   numeric_str[m] = cmd[i + k + 1 + m];
                                                }
                                                else
                                                {
                                                   faulty = YES;
                                                   (void)fprintf(p_data,
                                                                 "500 Expecting numeric value after \'%c\'\r\n",
                                                                 cmd[i + k + 1]);
                                                   break;
                                                }
                                                m++;
                                             }
                                             if (m > 0)
                                             {
                                                numeric_str[m] = '\0';
                                                switch(cmd[i + k + 1])
                                                {
                                                   case '#' : /* File number */
                                                      file_no = atoi(numeric_str);
                                                      break;
                                                   case '-' : /* number of lines */
                                                      lines = atoi(numeric_str);
                                                      break;
                                                   case '+' : /* Duration of displaying data. */
                                                      show_time = atoi(numeric_str);
                                                      break;
                                                   default  : /* Impossible!!! */
                                                      faulty = YES;
                                                      (void)rec(sys_log_fd, DEBUG_SIGN, "Unknown error! (%s %d)\n",
                                                                __FILE__, __LINE__);
                                                      (void)fprintf(p_data,
                                                                    "500 Unknown error. (%s %d)\r\n",
                                                                    __FILE__, __LINE__);
                                                      break;
                                                }
                                             }
                                             else
                                             {
                                                faulty = YES;
                                                (void)fprintf(p_data,
                                                              "500 No numeric value supplied after \'%c\'\r\n",
                                                              cmd[i + k + 1]);
                                             }
                                          }
                                       } while ((cmd[i + k + 1 + m] != '\r') && (faulty == NO));

                                       if (faulty == NO)
                                       {
                                          get_display_data(search_file,
                                                           search_string,
                                                           lines,
                                                           show_time,
                                                           file_no);
                                       }
                                    }
                                    else
                                    {
                                       *(cmd + nbytes - 2) = '\0';
                                       (void)fprintf(p_data,
                                                     "500 \'%s\': Syntax wrong (see HELP).\r\n",
                                                     cmd);
                                    }
                                 }
                              }
                              else
                              {
                                 *(cmd + nbytes - 2) = '\0';
                                 (void)fprintf(p_data,
                                               "500 \'%s\': command not understood.\r\n",
                                               cmd);
                              }
                      } /* if (cmd[i] == ' ') */
                      else if (cmd[i] == '\r')
                           {
                              get_display_data(search_file, NULL,
                                               EVERYTHING,
                                               EVERYTHING,
                                               DEFAULT_FILE_NO);
                           }
                           else
                           {
                               *(cmd + nbytes - 2) = '\0';
                               (void)fprintf(p_data,
                                             "500 \'%s\': command not understood.\r\n",
                                             cmd);
                           }
                   }
                   else if (strncmp(cmd, PROC_CMD, strlen(PROC_CMD)) == 0)
                        {
                           (void)fprintf(p_data,
                                         "502 Service not implemented. See help for commands.\r\n");
                        }
                   else if (strncmp(cmd, DISC_CMD, strlen(DISC_CMD)) == 0)
                        {
                           (void)fprintf(p_data,
                                         "502 Service not implemented. See help for commands.\r\n");
                        }
                   else if (strncmp(cmd, STAT_CMD, strlen(STAT_CMD)) == 0)
                        {
                           show_summary_stat(p_data);
                        }
                   else if (strncmp(cmd, START_STAT_CMD, strlen(START_STAT_CMD)) == 0)
                        {
                           show_summary_stat(p_data);
                           show_host_list(p_data);
                           (void)fprintf(p_data,
                                         "WD %s\r\n", p_work_dir);
                           (void)fprintf(p_data,
#ifdef PRE_RELEASE
                                         "AV PRE%d.%d.%d-%d\r\n",
                                         MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
                                         "AV %d.%d.%d\r\n",
                                         MAJOR, MINOR, BUG_FIX);
#endif
                           report_changes = YES;
                        }
                   else if (strncmp(cmd, LDB_CMD, strlen(LDB_CMD)) == 0)
                        {
                           (void)fprintf(p_data,
                                         "502 Service not implemented. See help for commands.\r\n");
                        }
                   else if (strncmp(cmd, LRF_CMD, strlen(LRF_CMD)) == 0)
                        {
                           (void)sprintf(p_work_dir_end, "%s%s",
                                         ETC_DIR, RENAME_RULE_FILE);
                           display_file(p_data);
                           *p_work_dir_end = '\0';
                        }
                   else if (strncmp(cmd, INFO_CMD, strlen(INFO_CMD)) == 0)
                        {
                           (void)fprintf(p_data,
                                         "502 Service not implemented. See help for commands.\r\n");
                        }
                   else if (strncmp(cmd, AFDSTAT_CMD, strlen(AFDSTAT_CMD)) == 0)
                        {
                           (void)fprintf(p_data,
                                         "502 Service not implemented. See help for commands.\r\n");
                        }
                        else
                        {
                           *(cmd + nbytes - 2) = '\0';
                           (void)fprintf(p_data,
                                         "500 \'%s\': command not understood.\r\n",
                                         cmd);
                        }
         (void)fflush(p_data);
      } /* if (nbytes > 0) */
   } /* for (;;) */

   (void)fflush(p_data);
   if (fclose(p_data) == EOF)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "fclose() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   p_data = NULL;
   exit(SUCCESS);
}


/*++++++++++++++++++++++++ report_shutdown() ++++++++++++++++++++++++++++*/
static void
report_shutdown(void)
{
   if (p_data != NULL)
   {
      if (report_changes == YES)
      {
         show_summary_stat(p_data);
         check_changes(p_data);
      }

      (void)fprintf(p_data, "%s\r\n", AFDD_SHUTDOWN_MESSAGE);
      (void)fflush(p_data);

      if (fclose(p_data) == EOF)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "fclose() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
   }

   return;
}

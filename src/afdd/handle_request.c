/*
 *  handle_request.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   06.04.2005 H.Kiehl Open FSA here and not in afdd.c.
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* memset()                                */
#include <stdlib.h>           /* atoi()                                  */
#include <time.h>             /* time()                                  */
#include <ctype.h>            /* toupper()                               */
#include <signal.h>           /* signal()                                */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>           /* close()                                 */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <netdb.h>
#include <errno.h>
#include "afdddefs.h"
#include "version.h"
#include "logdefs.h"

/* Global variables. */
int                        no_of_hosts,
                           fsa_fd = -1,
                           fsa_id,
                           host_config_counter;
#ifdef HAVE_MMAP
off_t                      fsa_size;
#endif
unsigned char              **old_error_history;
FILE                       *p_data = NULL;
struct filetransfer_status *fsa;



/* External global variables. */
extern char                afd_name[],
                           hostname[],
                           *p_work_dir,
                           *p_work_dir_end;

/* Local global variables. */
static int                 report_changes = NO;

/*  Local function prototypes. */
static void                report_shutdown(void);


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
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "fdopen() control error : %s", strerror(errno));
      exit(INCORRECT);
   }

   if (fsa_attach() < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, "Failed to attach to FSA.");
      exit(INCORRECT);
   }
   host_config_counter = (int)*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET +
                                                 SIZEOF_INT);
   RT_ARRAY(old_error_history, no_of_hosts, ERROR_HISTORY_LENGTH,
            unsigned char);
   for (i = 0; i < no_of_hosts; i++)
   {
      (void)memcpy(old_error_history[i], fsa[i].error_history,
                   ERROR_HISTORY_LENGTH);
   }

   if (atexit(report_shutdown) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not register exit handler : %s", strerror(errno));
   }

   /* Give some information to the user where he is and what service */
   /* is offered here.                                               */
   (void)fprintf(p_data, "220 %s AFD server %s (Version %s) ready.\r\n",
                 hostname, afd_name, PACKAGE_VERSION);
   (void)fflush(p_data);

   /*
    * Handle all request until the remote user has entered QUIT_CMD or
    * the connection was idle for AFDD_CMD_TIMEOUT seconds.
    */
   now = last = last_time_read = time(NULL);
   FD_ZERO(&rset);
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
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Remote hangup by %s", hostname);
            }
            else
            {
#ifdef ECONNRESET
               if (errno == ECONNRESET)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "read() error : %s", strerror(errno));
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "read() error : %s", strerror(errno));
               }
#else
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "read() error : %s", strerror(errno));
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
                               "214- The following commands are recognized (* =>'s unimplemented).\r\n\
   *AFDSTAT *DISC    HELP     ILOG     *INFO    *LDB     LRF      OLOG\r\n\
   *PROC    QUIT     SLOG     STAT     TDLOG    TLOG     *TRACEF  *TRACEI\r\n\
   *TRACEO  SSTAT\r\n\
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
                      (void)fprintf(p_data,
                                    "502 Service not implemented. See help for commands.\r\n");
                   }
              else if (strncmp(cmd, TRACEO_CMD, strlen(TRACEO_CMD)) == 0)
                   {
                      (void)fprintf(p_data,
                                    "502 Service not implemented. See help for commands.\r\n");
                   }
              else if (strncmp(cmd, TRACEF_CMD, strlen(TRACEF_CMD)) == 0)
                   {
                      (void)fprintf(p_data,
                                    "502 Service not implemented. See help for commands.\r\n");
                   }
              else if ((strncmp(cmd, ILOG_CMD, (sizeof(ILOG_CMD) - 1)) == 0) ||
                       (strncmp(cmd, OLOG_CMD, (sizeof(OLOG_CMD) - 1)) == 0) ||
                       (strncmp(cmd, SLOG_CMD, (sizeof(SLOG_CMD) - 1)) == 0) ||
                       (strncmp(cmd, TLOG_CMD, (sizeof(TLOG_CMD) - 1)) == 0) ||
                       (strncmp(cmd, TDLOG_CMD, (sizeof(TDLOG_CMD) - 1)) == 0))
                   {
                      char search_file[MAX_PATH_LENGTH];

                      /*
                       * For now lets just disable the following code.
                       */
                      (void)fprintf(p_data,
                                    "503 Service disabled.\r\n");
                      break;

                      /*
                       * First lets determine what file we want and then
                       * create the full file name without the number.
                       */
                      (void)sprintf(search_file, "%s%s/", p_work_dir,
                                    LOG_DIR);
                      switch (cmd[0])
                      {
#ifdef _INPUT_LOG
                         case 'I' : /* Input log */
                            (void)strcat(search_file, INPUT_BUFFER_FILE);
                            break;
#endif /* _INPUT_LOG */
#ifdef _OUTPUT_LOG
                         case 'O' : /* Output log */
                            (void)strcat(search_file, OUTPUT_BUFFER_FILE);
                            break;
#endif /* _OUTPUT_LOG */
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
                            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                       "Unknown error!");
                            (void)fprintf(p_data,
                                          "500 Unknown error. (%s %d)\r\n",
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
                                     if (isdigit((int)cmd[i + 1 + k]))
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
                                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                      "Unknown error!");
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
                               get_display_data(search_file, NULL, lines,
                                                show_time, file_no, sd);
                            }
                         }
                         else if (isascii(cmd[i + 1]))
                              {
                                 int  k = 0;
                                 char *ptr,
			              search_string[256];

                                 /* User has supplied a search string. */
                                 ptr = &cmd[i + 1];
                                 while ((*ptr != ' ') && (*ptr != '\r') &&
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
                                                if (isdigit((int)cmd[i + k + 1 + m]))
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
                                                      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                                 "Unknown error!");
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
                                                           file_no,
                                                           sd);
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
                                               DEFAULT_FILE_NO,
                                               sd);
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
                      (void)fprintf(p_data, "WD %s\r\nAV %s\r\n",
                                    p_work_dir, PACKAGE_VERSION);
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
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "fclose() error : %s", strerror(errno));
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
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "fclose() error : %s", strerror(errno));
      }
      p_data = NULL;
   }

   return;
}

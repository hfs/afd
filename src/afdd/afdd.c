/*
 *  afdd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   afdd - TCP command daemon for the AFD
 **
 ** SYNOPSIS
 **   afdd [--version][-w <AFD working directory>]
 **
 ** DESCRIPTION
 **   This is a small tcp command server at port AFD_PORT_NO that
 **   returns information on the AFD. It functions very similar to
 **   the ftpd except that it does not use a data connection to transmit
 **   the information. The control connection is used instead.
 **
 **   The following commands are supported:
 **      HELP [<sp> <command>]      - Shows all commands supported or help
 **                                   on a specific command.
 **      QUIT                       - Terminate service.
 **
 ** RETURN VALUES
 **   Will exit with INCORRECT when some system call failed.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.06.1997 H.Kiehl Created
 **   22.08.1998 H.Kiehl Added some more exit handlers.
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* memset()                                */
#include <stdlib.h>           /* atoi(), getenv()                        */
#include <ctype.h>            /* isdigit()                               */
#include <time.h>             /* clock_t                                 */
#include <signal.h>           /* signal()                                */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>         /* waitpid()                               */
#include <netinet/in.h>
#include <arpa/inet.h>        /* inet_ntoa()                             */
#include <unistd.h>           /* close(), select(), sysconf()            */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <netdb.h>
#include <errno.h>
#include "afdddefs.h"
#include "version.h"

/* Global variables */
int               number_of_trusted_hosts,
                  sys_log_fd = STDERR_FILENO;
clock_t           clktck;
char              afd_config_file[MAX_PATH_LENGTH],
                  afd_name[MAX_AFD_NAME_LENGTH],
                  current_msg_list_file[MAX_PATH_LENGTH],
                  hostname[MAX_FULL_USER_ID_LENGTH],
                  *p_work_dir,
                  *p_work_dir_end,
                  **trusted_host = NULL;
struct afd_status *p_afd_status;
const char        *sys_log_name = SYSTEM_LOG_FIFO;

/* Local global variables. */
static int        in_child = NO,
                  new_sockfd,
                  sockfd,
                  no_of_connections;
static pid_t      pid[MAX_AFDD_CONNECTIONS];

/* Local function prototypes. */
static int        get_free_connection(void);
static void       afdd_exit(void),
                  get_afd_config_value(char *),
                  sig_bus(int),
                  sig_exit(int),
                  sig_segv(int),
                  zombie_check(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                length,
                      pos,
                      port,
                      ports_tried = 0,
                      on = 1,
                      status;
   my_socklen_t       peer_addrlen;
   char               *ptr,
                      port_no[MAX_INT_LENGTH],
                      reply[256],
                      work_dir[MAX_PATH_LENGTH];
   fd_set             rset;
   struct servent     *p_service;
   struct protoent    *p_protocol;
   struct sockaddr_in data,
                      peer_address;
   struct timeval     timeout;

   CHECK_FOR_VERSION(argc, argv);

   /* Initialize variables */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
   p_work_dir_end = work_dir + strlen(work_dir);
   no_of_connections = 0;
   (void)memset(pid, 0, MAX_AFDD_CONNECTIONS * sizeof(pid_t));
   if ((ptr = getenv("LOGNAME")) != NULL)
   {
      length = strlen(ptr);
      if (length > 0)
      {
         if ((length + 1) < MAX_FULL_USER_ID_LENGTH)
         {
            (void)strcpy(hostname, ptr);
            (void)strcat(hostname, "@");
            length++;
         }
         else
         {
            memcpy(hostname, ptr, MAX_FULL_USER_ID_LENGTH - 1);
            hostname[MAX_FULL_USER_ID_LENGTH - 1] = '\0';
            length = MAX_FULL_USER_ID_LENGTH;
         }
      }
      else
      {
         if (8 < MAX_FULL_USER_ID_LENGTH)
         {
            (void)strcpy(hostname, "unknown@");
            length = 8;
         }
         else
         {
            hostname[0] = '\0';
            length = 0;
         }
      }
   }
   else
   {
      if (8 < MAX_FULL_USER_ID_LENGTH)
      {
         (void)strcpy(hostname, "unknown@");
         length = 8;
      }
      else
      {
         hostname[0] = '\0';
         length = 0;
      }
   }
   if (length < MAX_FULL_USER_ID_LENGTH)
   {
      if (gethostname(&hostname[length], MAX_FULL_USER_ID_LENGTH - length) != 0)
      {
         if ((length + 7) < MAX_FULL_USER_ID_LENGTH)
         {
            (void)strcpy(&hostname[length], "unknown");
         }
      }
   }
   (void)strcpy(port_no, DEFAULT_AFD_PORT_NO);
   get_afd_config_value(port_no);
   port = atoi(port_no);
   if (get_afd_name(afd_name) == INCORRECT)
   {
      afd_name[0] = '\0';
   }
   peer_addrlen = sizeof(peer_address);

   /* Do some cleanups when we exit */
   if (atexit(afdd_exit) != 0)          
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not register exit handler : %s", strerror(errno));
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGPIPE, SIG_IGN) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not set signal handlers : %s", strerror(errno));
      exit(INCORRECT);
   }

   if ((ptr = lock_proc(AFDD_LOCK_ID, NO)) != NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Process AFDD already started by %s", ptr);
      (void)fprintf(stderr, "Process AFDD already started by %s : (%s %d)\n",
                    ptr, __FILE__, __LINE__);
      _exit(INCORRECT);
   }

   /*
    * Get clock ticks per second, so we can calculate the transfer time.
    */
   if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not get clock ticks per second : %s (%s %d)\n",
                 strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Attach to the AFD Status Area */
   if (attach_afd_status(NULL) < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to map to AFD status area.");
      exit(INCORRECT);
   }

   (void)memset((struct sockaddr *) &data, 0, sizeof(data));
   data.sin_family = AF_INET;
   data.sin_addr.s_addr = INADDR_ANY;

   /* Get full name to AFD_CONFIG file. */
   (void)sprintf(afd_config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   (void)sprintf(current_msg_list_file, "%s%s%s",
                 p_work_dir, FIFO_DIR, CURRENT_MSG_LIST_FILE);

   do
   {
      if ((p_service = getservbyname(port_no, "tcp")))
      {
         data.sin_port =  htons(ntohs((u_short)p_service->s_port));
      }
      else if ((data.sin_port = htons((u_short)atoi(port_no))) == 0)
           {
              system_log(FATAL_SIGN, __FILE__, __LINE__,
                         "Failed to copy service to structure.");
              exit(INCORRECT);
           }

      if ((p_protocol = getprotobyname("tcp")) == 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to get protocol tcp : %s", strerror(errno));
         exit(INCORRECT);
      }

      if ((sockfd = socket(AF_INET, SOCK_STREAM, p_protocol->p_proto)) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not create socket : %s", strerror(errno));
         exit(INCORRECT);
      }

      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "setsockopt() error : %s", strerror(errno));
         (void)close(sockfd);
         exit(INCORRECT);
      }

      if ((status = bind(sockfd, (struct sockaddr *) &data, sizeof(data))) == -1)
      {
         ports_tried++;
         port++;
         (void)sprintf(port_no, "%d", port);
         if ((errno != EADDRINUSE) || (ports_tried > 100))
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "bind() error : %s", strerror(errno));
            (void)close(sockfd);
            exit(INCORRECT);
         }
         if (close(sockfd) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "close() error : %s", strerror(errno));
         }
      }
   } while (status == -1);

   system_log(INFO_SIGN, NULL, 0, "Starting %s at port %d (%s)",
              AFDD, port, PACKAGE_VERSION);

   if (listen(sockfd, 5) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "listen() error : %s", strerror(errno));
      (void)close(sockfd);
      exit(INCORRECT);
   }
   FD_ZERO(&rset);

   for (;;)
   {
      /* Initialise descriptor set */
      FD_SET(sockfd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = 5L;

      if (select(sockfd + 1, &rset, NULL, NULL, &timeout) < 0)
      {
         (void)close(sockfd);
         if (errno != EBADF)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "select() error : %s", strerror(errno));
         }
         exit(INCORRECT);
      }

      if (FD_ISSET(sockfd, &rset))
      {
         if ((new_sockfd = accept(sockfd, (struct sockaddr *)&peer_address, &peer_addrlen)) < 0)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "accept() error : %s", strerror(errno));
            (void)close(sockfd);
            exit(INCORRECT);
         }
         if (number_of_trusted_hosts > 0)
         {
            int  gotcha = NO,
                 i;
            char remote_ip_str[16];

            (void)strcpy(remote_ip_str, inet_ntoa(peer_address.sin_addr));
            for (i = 0; i < number_of_trusted_hosts; i++)
            {
               if (pmatch(trusted_host[i], remote_ip_str) == 0)
               {
                  gotcha = YES;
                  break;
               }
            }
            if (gotcha == NO)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "AFDD: Illegal access from %s", remote_ip_str);
               (void)close(new_sockfd);
               continue;
            }
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "AFDD: Connection from %s", remote_ip_str);
         }
         else
         {
            system_log(DEBUG_SIGN, NULL, 0, "AFDD: Connection from %s",
                       inet_ntoa(peer_address.sin_addr));
         }

         if (no_of_connections >= MAX_AFDD_CONNECTIONS)
         {
            length = sprintf(reply,
                             "421 Service not available. There are currently to many users (%d) connected.\r\n",
                             no_of_connections);
            (void)write(new_sockfd, reply, length);
            (void)close(new_sockfd);
         }
         else
         {
            if ((pos = get_free_connection()) < 0)
            {
               length = sprintf(reply, "421 Service not available.\r\n");
               (void)write(new_sockfd, reply, length);
               (void)close(new_sockfd);
            }
            else
            {
               switch(pid[pos] = fork())
               {
                  case -1 : /* Could not generate process */
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "fork() error : %s", strerror(errno));
                     break;

                  case 0  : /* Child process to serve user. */
                     in_child = YES;
                     (void)close(sockfd);
                     handle_request(new_sockfd, pos);
                     exit(0);

                  default : /* Parent process */
                     (void)close(new_sockfd);
                     no_of_connections++;
                     break;
               }
            }
         }
      } /* if (FD_ISSET(sockfd, &rset)) */

      zombie_check();
   } /* for (;;) */
}


/*++++++++++++++++++++++++++ get_free_connection() ++++++++++++++++++++++*/
static int
get_free_connection(void)
{
   int i;

   for (i = 0; i < MAX_AFDD_CONNECTIONS; i++)
   {
      if (pid[i] == 0)
      {
         return(i);
      }
   }

   return(INCORRECT);
}


/*+++++++++++++++++++++++++++++ zombie_check() +++++++++++++++++++++++++*/
/*                              --------------                          */
/* Description : Checks if any process is finished (zombie), if this    */
/*               is the case it is killed with waitpid().               */
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
zombie_check(void)
{
   int i,
       status;

   /* Did any of the jobs become zombies? */
   for (i = 0; i < MAX_AFDD_CONNECTIONS; i++)
   {
      if ((pid[i] > 0) &&
          (waitpid(pid[i], &status, WNOHANG) > 0))
      {
         if (WIFEXITED(status))
         {
            pid[i] = 0;
            no_of_connections--;
         }
         else  if (WIFSIGNALED(status))
               {
                  /* abnormal termination */
                  pid[i] = 0;
                  no_of_connections--;
               }
          else if (WIFSTOPPED(status))
               {
                  /* Child stopped */;
               }
      }
   }

   return;
}


/*+++++++++++++++++++++++++ get_afd_config_value() ++++++++++++++++++++++*/
static void                                                                
get_afd_config_value(char *port_no)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH];

   (void)sprintf(config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file(config_file, &buffer) != INCORRECT))
   {
      char *ptr = buffer,
           tmp_trusted_host[16];

      if (get_definition(buffer,
                         AFD_TCP_PORT_DEF,
                         port_no, MAX_INT_LENGTH) != NULL)
      {
         int port;

         port = atoi(port_no);
         if ((port < 1024) || (port > 10240))
         {
            (void)strcpy(port_no, DEFAULT_AFD_PORT_NO);
         }
      }

      /*
       * Read all IP-numbers that may connect to AFDD. If none is found
       * all IP's may connect.
       */
      do
      {
         if ((ptr = get_definition(ptr,
                                   TRUSTED_REMOTE_IP_DEF,
                                   tmp_trusted_host, 16)) != NULL)
         {
            int  counter = 0;
            char *check_ptr;

            check_ptr = tmp_trusted_host;
            while (((isdigit((int)(*check_ptr))) || (*check_ptr == '*') ||
                    (*check_ptr == '?')) && (counter < 3))
            {
               check_ptr++; counter++;
            }
            if ((counter < 4) && (*check_ptr == '.'))
            {
               check_ptr++;
               counter = 0;
               while (((isdigit((int)(*check_ptr))) || (*check_ptr == '*') ||
                       (*check_ptr == '?')) && (counter < 3))
               {
                  check_ptr++; counter++;
               }
               if ((counter < 4) && (*check_ptr == '.'))
               {
                  check_ptr++;
                  counter = 0;
                  while ((((int)(isdigit(*check_ptr))) || (*check_ptr == '*') ||
                          (*check_ptr == '?')) && (counter < 3))
                  {
                     check_ptr++; counter++;
                  }
                  if ((counter < 4) && (*check_ptr == '.'))
                  {
                     check_ptr++;
                     counter = 0;
                     while (((isdigit((int)(*check_ptr))) || (*check_ptr == '*') ||
                             (*check_ptr == '?')) && (counter < 4))
                     {
                        check_ptr++; counter++;
                     }
                     if ((counter < 4) && ((*check_ptr == '\n') ||
                         (*check_ptr == '\0')))
                     {
                        number_of_trusted_hosts++;
                        if (number_of_trusted_hosts == 1)
                        {
                           RT_ARRAY(trusted_host, number_of_trusted_hosts,
                                    16, char);
                        }
                        else
                        {
                           REALLOC_RT_ARRAY(trusted_host,
                                            number_of_trusted_hosts,
                                            16,
                                            char);
                        }
                        (void)strcpy(trusted_host[number_of_trusted_hosts - 1],
                                     tmp_trusted_host);
                     }
                  }
               }
            }
         }
      } while (ptr != NULL);
      free(buffer);
   }

   return;
}


/*++++++++++++++++++++++++++++++ afdd_exit() ++++++++++++++++++++++++++++*/
static void
afdd_exit(void)
{
   if (in_child == NO)
   {
      int i;

      /* Kill all child process */
      for (i = 0; i < MAX_AFDD_CONNECTIONS; i++)
      {
         if (pid[i] > 0)
         {
            if (kill(pid[i], SIGINT) == -1)
            {
               if (errno != ESRCH)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to kill() %d : %s",
                             pid[i], strerror(errno));
               }
            }
         }
      }

      system_log(INFO_SIGN, NULL, 0, "Stopped %s.", AFDD);
   }

   (void)close(sockfd);
   (void)close(new_sockfd);
   (void)close(sys_log_fd);

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__, "Aaarrrggh! Received SIGSEGV.");
   afdd_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__, "Uuurrrggh! Received SIGBUS.");
   afdd_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*+++++++++++++++++++++++++++++++ sig_exit() ++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}

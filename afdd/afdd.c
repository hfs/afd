/*
 *  afdd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
#include <ctype.h>            /* toupper()                               */
#include <signal.h>           /* signal()                                */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>         /* waitpid()                               */
#include <netinet/in.h>
#include <arpa/inet.h>        /* inet_ntoa()                             */
#include <unistd.h>           /* close(), select(), sysconf()            */
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include "afdddefs.h"
#include "version.h"

/* Global variables */
int                        no_of_hosts,
                           number_of_trusted_hosts,
                           fsa_fd = -1,
                           fsa_id,
                           host_config_counter,
                           sys_log_fd = STDERR_FILENO;
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
clock_t                    clktck;
char                       afd_config_file[MAX_PATH_LENGTH],
                           afd_name[MAX_AFD_NAME_LENGTH],
                           hostname[50],
                           *p_work_dir,
                           *p_work_dir_end,
                           **trusted_host = NULL;
struct filetransfer_status *fsa;
struct afd_status          *p_afd_status;

/* Local global variables. */
static int                 in_child = NO,
                           new_sockfd,
                           sockfd,
                           no_of_connections;

/* Local function prototypes. */
static int                 get_free_connection(void);
static pid_t               pid[MAX_AFDD_CONNECTIONS];
static void                afdd_exit(void),
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
                      peer_addrlen,
                      pos,
                      port,
                      ports_tried = 0,
                      on = 1,
                      status;
   char               *ptr,
                      port_no[MAX_INT_LENGTH],
                      reply[256],
                      sys_log_fifo[MAX_PATH_LENGTH],
                      work_dir[MAX_PATH_LENGTH];
   fd_set             rset;
   struct servent     *p_service;
   struct protoent    *p_protocol;
   struct sockaddr_in data,
                      peer_address;
   struct timeval     timeout;
   struct stat        stat_buf;

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
      (void)strcat(hostname, ptr);
      (void)strcat(hostname, "@");
   }
   else
   {
      (void)strcat(hostname, "unknown");
      (void)strcat(hostname, "@");
   }
   length = strlen(hostname);
   if (gethostname(&hostname[length], 21) != 0)
   {
      (void)strcat(hostname, "unknown");
   }
   (void)strcpy(port_no, DEFAULT_AFD_PORT_NO);
   get_afd_config_value(port_no);
   port = atoi(port_no);
   if (get_afd_name(afd_name) == INCORRECT)
   {
      afd_name[0] = '\0';
   }
   peer_addrlen = sizeof(peer_address);

   /* If the process AFD has not yet created the */
   /* system log fifo create it now.             */
   (void)sprintf(sys_log_fifo, "%s%s%s",
                 p_work_dir, FIFO_DIR, SYSTEM_LOG_FIFO);
   if ((stat(sys_log_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(sys_log_fifo) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not create fifo %s. (%s %d)\n",
                       sys_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Open system log fifo */
   if ((sys_log_fd = coe_open(sys_log_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                    sys_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Do some cleanups when we exit */
   if (atexit(afdd_exit) != 0)          
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not register exit handler : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
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
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not set signal handlers : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Get clock ticks per second, so we can calculate the transfer time.
    */
   if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not get clock ticks per second : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Get the fsa_id and no of host of the FSA */
   if (fsa_attach() < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Failed to attach to FSA. (%s %d)\n",
                __FILE__, __LINE__);
      exit(INCORRECT);
   }
   host_config_counter = (int)*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + sizeof(int));

   /* Attach to the AFD Status Area */
   if (attach_afd_status() < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to map to AFD status area. (%s %d)\n",
                __FILE__, __LINE__);
      exit(INCORRECT);
   }

   (void)memset((struct sockaddr *) &data, 0, sizeof(data));
   data.sin_family = AF_INET;
   data.sin_addr.s_addr = INADDR_ANY;

   /* Get full name to AFD_CONFIG file. */
   (void)sprintf(afd_config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);

   do
   {
      if ((p_service = getservbyname(port_no, "tcp")))
      {
         data.sin_port =  htons(ntohs((u_short)p_service->s_port));
      }
      else if ((data.sin_port = htons((u_short)atoi(port_no))) == 0)
           {
              (void)rec(sys_log_fd, FATAL_SIGN,
                        "Failed to copy service to structure. (%s %d)\n",
                        __FILE__, __LINE__);
              exit(INCORRECT);
           }

      if ((p_protocol = getprotobyname("tcp")) == 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to get protocol tcp : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

      if ((sockfd = socket(AF_INET, SOCK_STREAM, p_protocol->p_proto)) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create socket : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
      {
         (void)close(sockfd);
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "setsockopt() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

      if ((status = bind(sockfd, (struct sockaddr *) &data, sizeof(data))) == -1)
      {
         if (close(sockfd) == -1)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "close() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }
         ports_tried++;
         port++;
         (void)sprintf(port_no, "%d", port);
         if ((errno != EADDRINUSE) || (ports_tried > 100))
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "bind() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
   } while (status == -1);

#ifdef PRE_RELEASE
   (void)rec(sys_log_fd, INFO_SIGN, "Starting %s at port %d (PRE %d.%d.%d-%d)\n",
             AFDD, port, MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
   (void)rec(sys_log_fd, INFO_SIGN, "Starting %s at port %d (%d.%d.%d)\n",
             AFDD, port, MAJOR, MINOR, BUG_FIX);
#endif

   if (listen(sockfd, 5) == -1)
   {
      (void)close(sockfd);
      (void)rec(sys_log_fd, FATAL_SIGN,
                "listen() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   for (;;)
   {
      /* Initialise descriptor set */
      FD_ZERO(&rset);
      FD_SET(sockfd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = 5L;

      if (select(sockfd + 1, &rset, NULL, NULL, &timeout) < 0)
      {
         (void)close(sockfd);
         if (errno != EBADF)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "select() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }
         exit(INCORRECT);
      }

      if (FD_ISSET(sockfd, &rset))
      {
         if ((new_sockfd = accept(sockfd, (struct sockaddr *)&peer_address, &peer_addrlen)) < 0)
         {
            (void)close(sockfd);
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "accept() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
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
               if (filter(trusted_host[i], remote_ip_str) == 0)
               {
                  gotcha = YES;
                  break;
               }
            }
            if (gotcha == NO)
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "AFDD: Illegal access from %s (%s %d)\n",
                         remote_ip_str, __FILE__, __LINE__);
               (void)close(new_sockfd);
               continue;
            }
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "AFDD: Connection from %s (%s %d)\n",
                      remote_ip_str, __FILE__, __LINE__);
         }
         else
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "AFDD: Connection from %s (%s %d)\n",
                      inet_ntoa(peer_address.sin_addr), __FILE__, __LINE__);
         }

         if (no_of_connections >= MAX_AFDD_CONNECTIONS)
         {
            length = sprintf(reply, "421 Service not available. There are currently to many users (%d) connected.\r\n",
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
                            (void)rec(sys_log_fd, ERROR_SIGN,
                                      "fork() error : %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
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
   static int i;

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
                       /* Child stopped */
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
   if ((access(config_file, F_OK) == 0) &&
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
            while (((isdigit(*check_ptr)) || (*check_ptr == '*') ||
                    (*check_ptr == '?')) && (counter < 3))
            {
               check_ptr++; counter++;
            }
            if ((counter < 4) && (*check_ptr == '.'))
            {
               check_ptr++;
               counter = 0;
               while (((isdigit(*check_ptr)) || (*check_ptr == '*') ||
                       (*check_ptr == '?')) && (counter < 3))
               {
                  check_ptr++; counter++;
               }
               if ((counter < 4) && (*check_ptr == '.'))
               {
                  check_ptr++;
                  counter = 0;
                  while (((isdigit(*check_ptr)) || (*check_ptr == '*') ||
                          (*check_ptr == '?')) && (counter < 3))
                  {
                     check_ptr++; counter++;
                  }
                  if ((counter < 4) && (*check_ptr == '.'))
                  {
                     check_ptr++;
                     counter = 0;
                     while (((isdigit(*check_ptr)) || (*check_ptr == '*') ||
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
   int i;

   if (in_child == NO)
   {
      /* Kill all child process */
      for (i = 0; i < MAX_AFDD_CONNECTIONS; i++)
      {
         if (pid[i] > 0)
         {
            if (kill(pid[i], SIGINT) == -1)
            {
               if (errno != ESRCH)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Failed to kill() %d : %s (%s %d)\n",
                            pid[i], strerror(errno),
                            __FILE__, __LINE__);
               }
            }
         }
      }

      (void)rec(sys_log_fd, INFO_SIGN, "Stopped %s.\n", AFDD);
   }

   if (fsa_detach() == INCORRECT)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Failed to detach from FSA. (%s %d)\n",
                __FILE__, __LINE__);
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
   (void)rec(sys_log_fd, FATAL_SIGN, "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
             __FILE__, __LINE__);
   afdd_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   (void)rec(sys_log_fd, FATAL_SIGN, "Uuurrrggh! Received SIGBUS. (%s %d)\n",
             __FILE__, __LINE__);
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

/*
 *  wmocmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Deutscher Wetterdienst (DWD),
 *                           Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   wmocmd - commands to send files via TCP according WMO regulations
 **
 ** SYNOPSIS
 **   int  wmo_connect(char *hostname, int port)
 **   int  wmo_write(char *block, char *buffer, int size)
 **   int  wmo_check_reply(char *expect_string)
 **   int  wmo_quit(void)
 **
 ** DESCRIPTION
 **   wmocmd provides a set of commands to communicate with a TCP
 **   server via BSD sockets according WMO regulations.
 **   Only three functions are necessary to do the communication:
 **      wmo_connect() - build a TCP connection to the WMO server
 **      wmo_write()   - write data to the socket
 **      wmo_quit()    - disconnect from the WMO server
 **   The function wmo_check_reply() is optional, it checks if the
 **   remote site has received the reply.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when successful. When an error has occurred
 **   it will return INCORRECT. 'timeout_flag' is just a flag to
 **   indicate that the 'connect_timeout' time has been reached.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   28.05.1998 H.Kiehl Created
 **   10.11.1998 H.Kiehl Added function wmp_check_reply().
 **
 */
DESCR__E_M3


#include <stdio.h>            /* fdopen(), fclose()                      */
#include <string.h>           /* memset(), memcpy()                      */
#include <stdlib.h>           /* malloc(), free()                        */
#include <sys/types.h>        /* fd_set                                  */
#include <sys/time.h>         /* struct timeval                          */
#include <sys/socket.h>       /* socket(), shutdown(), bind(),           */
                              /* setsockopt()                            */
#include <netinet/in.h>       /* struct in_addr, sockaddr_in, htons()    */
#include <netdb.h>            /* struct hostent, gethostbyname()         */
#include <arpa/inet.h>        /* inet_addr()                             */
#include <unistd.h>           /* select(), exit(), write(), read(),      */
                              /* close()                                 */
#include <fcntl.h>            /* O_NONBLOCK                              */
#include <errno.h>
#include "wmodefs.h"
#include "fddefs.h"           /* struct job                              */


/* External global variables */
extern int                timeout_flag;
#ifdef LINUX
extern char               *h_errlist[];  /* for gethostbyname()          */
extern int                h_nerr;        /* for gethostbyname()          */
#endif
extern int                sys_log_fd,
                          transfer_log_fd;
extern long               connect_timeout;
extern char               tr_hostname[];
extern struct job         db;

/* Local global variables */
static int                sock_fd;
static FILE               *p_data;
static struct timeval     timeout;

#define MAX_CHARS_IN_LINE 45


/*########################## wmo_connect() ##############################*/
int
wmo_connect(char *hostname, int port)
{
#ifdef _WITH_WMO_SUPPORT
   int                     host_alias = 0,
                           loop_counter = 0;
   struct sockaddr_in      sin;
   register struct hostent *p_host = NULL;
#ifdef FTX
   struct linger           l;
#endif

   (void)memset((struct sockaddr *) &sin, 0, sizeof(sin));
   if ((sin.sin_addr.s_addr = inet_addr(hostname)) != -1)
   {
      sin.sin_family = AF_INET;
   }
   else
   {
      if ((p_host = gethostbyname(hostname)) == NULL)
      {
         if (h_errno != 0)
	 {
#ifdef LINUX
	    if ((h_errno > 0) && (h_errno < h_nerr))
	    {
               (void)rec(transfer_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to gethostbyname() %s : %s #%d (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname,
                         (int)db.job_no, hostname, h_errlist[h_errno],
                         db.job_id, __FILE__, __LINE__);
	    }
	    else
	    {
               (void)rec(transfer_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to gethostbyname() %s (h_errno = %d) : %s #%d (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname,
                         (int)db.job_no, hostname, h_errno, strerror(errno),
                         db.job_id, __FILE__, __LINE__);
	    }
#else
            (void)rec(transfer_log_fd, ERROR_SIGN,
                      "%-*s[%d]: Failed to gethostbyname() %s (h_errno = %d) : %s #%d (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname,
                      (int)db.job_no, hostname, h_errno,
                      strerror(errno), db.job_id, __FILE__, __LINE__);
#endif
	 }
	 else
	 {
            (void)rec(transfer_log_fd, ERROR_SIGN,
                      "%-*s[%d]: Failed to gethostbyname() %s : %s #%d (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname,
                      (int)db.job_no, hostname, strerror(errno),
                      db.job_id, __FILE__, __LINE__);
	 }
         return(INCORRECT);
      }
      sin.sin_family = p_host->h_addrtype;

      /* Copy IP number to socket structure */
      memcpy((char *)&sin.sin_addr, p_host->h_addr, p_host->h_length);
   }

   if ((sock_fd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
   {
      (void)rec(transfer_log_fd, ERROR_SIGN,
                "%-*s[%d]: socket() error : %s #%d (%s %d)\n",
                MAX_HOSTNAME_LENGTH, tr_hostname,
                (int)db.job_no, strerror(errno),
                db.job_id, __FILE__, __LINE__);
      return(INCORRECT);
   }

   sin.sin_port = htons((u_short)port);

   while (connect(sock_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
   {
      if (p_host == NULL)
      {
         (void)rec(transfer_log_fd, ERROR_SIGN,
                   "%-*s[%d]: Failed to connect() to %s : %s #%d (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, hostname, strerror(errno),
                   db.job_id, __FILE__, __LINE__);
         (void)close(sock_fd);
         return(INCORRECT);
      }
      host_alias++;
      if (p_host->h_addr_list[host_alias] == NULL)
      {
         host_alias = 0;
         loop_counter++;

         if (loop_counter <= 8)
         {
            /*
             * Lets not give up to early. When we just have closed
             * the connection and immediatly retry, the other side
             * might not be as quick and still have the socket open.
             */
            (void)sleep(1);
         }
         else
         {
            (void)rec(transfer_log_fd, ERROR_SIGN,
                      "%-*s[%d]: Failed to connect() to %s, have tried %d times : %s #%d (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname,
                      (int)db.job_no, hostname, loop_counter,
                      strerror(errno), db.job_id, __FILE__, __LINE__);
            (void)close(sock_fd);
            return(INCORRECT);
         }
      }
      memcpy((char *)&sin.sin_addr, p_host->h_addr_list[host_alias], p_host->h_length);
      if (close(sock_fd) == -1)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "close() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
      if ((sock_fd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
      {
         (void)rec(transfer_log_fd, ERROR_SIGN,
                   "%-*s[%d]: socket() error : %s #%d (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, strerror(errno),
                   db.job_id, __FILE__, __LINE__);
         (void)close(sock_fd);
         return(INCORRECT);
      }
   }

#ifdef FTX
   l.l_onoff = 1; l.l_linger = 240;
   if (setsockopt(sock_fd, SOL_SOCKET, SO_LINGER, (char *)&l, sizeof(struct linger)) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "setsockopt() error : %s #%d (%s %d)\n",
                strerror(errno), db.job_id, __FILE__, __LINE__);
      return(INCORRECT);
   }
#endif

   if ((p_data = fdopen(sock_fd, "w")) == NULL)
   {
      (void)rec(transfer_log_fd, ERROR_SIGN,
                "%-*s[%d]: fdopen() control error : %s #%d (%s %d)\n",
                MAX_HOSTNAME_LENGTH, tr_hostname,
                (int)db.job_no, strerror(errno), db.job_id,
                __FILE__, __LINE__);
      (void)close(sock_fd);
      return(INCORRECT);
   }
#endif /* _WITH_WMO_SUPPORT */

   return(SUCCESS);
}

#ifdef _WITH_WMO_SUPPORT
/*############################## wmo_write() ############################*/
int
wmo_write(char *block, int size)
{
   int    status;
   fd_set wset;

   /* Initialise descriptor set */
   FD_ZERO(&wset);
   FD_SET(sock_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = connect_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(sock_fd + 1, NULL, &wset, NULL, &timeout);

   if (status == 0)
   {
      /* timeout has arrived */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (FD_ISSET(sock_fd, &wset))
        {
#ifdef _WITH_SEND
           if (send(sock_fd, block, size, 0) != size)
           {
              (void)rec(transfer_log_fd, ERROR_SIGN,
                        "%-*s[%d]: send() error : %s (%s %d)\n",
                        MAX_HOSTNAME_LENGTH, tr_hostname,
                        (int)db.job_no, strerror(errno), __FILE__, __LINE__);
              return(INCORRECT);
           }
#else
           if (write(sock_fd, block, size) != size)
           {
              (void)rec(transfer_log_fd, ERROR_SIGN,
                        "%-*s[%d]: write() error : %s (%s %d)\n",
                        MAX_HOSTNAME_LENGTH, tr_hostname,
                        (int)db.job_no, strerror(errno), __FILE__, __LINE__);
              return(INCORRECT);
           }
#endif
           if (fflush(p_data) == EOF)
           {
              (void)rec(transfer_log_fd, ERROR_SIGN,
                        "%-*s[%d]: Failed to fflush() data buffer : %s (%s %d)\n",
                        MAX_HOSTNAME_LENGTH, tr_hostname,
                        (int)db.job_no, strerror(errno), __FILE__, __LINE__);
              return(INCORRECT);
           }
        }
        else if (status < 0)
             {
                (void)rec(transfer_log_fd, ERROR_SIGN,
                          "%-*s[%d]: select() error : %s (%s %d)\n",
                          MAX_HOSTNAME_LENGTH, tr_hostname,
                          (int)db.job_no, strerror(errno), __FILE__, __LINE__);
                exit(INCORRECT);
             }
             else
             {
                (void)rec(sys_log_fd, ERROR_SIGN,
                          "Unknown condition. (%s %d)\n",
                          __FILE__, __LINE__);
                exit(INCORRECT);
             }
   
   return(SUCCESS);
}


/*########################### wmo_check_reply() #########################*/
int
wmo_check_reply(void)
{
   int  n = 0;
   char buffer[10];

   if ((n = readn(sock_fd, buffer, 10)) == 10)
   {
      if ((buffer[0] == '0') && (buffer[1] == '0') &&
          (buffer[2] == '0') && (buffer[3] == '0') &&
          (buffer[4] == '0') && (buffer[5] == '0') &&
          (buffer[6] == '0') && (buffer[7] == '0'))
      {
         if ((buffer[8] == 'A') && (buffer[9] == 'K'))
         {
            return(SUCCESS);
         }
         else if ((buffer[8] == 'N') && (buffer[9] == 'A'))
              {
                 return(NEGATIV_ACKNOWLEDGE);
              }
      }

      (void)rec(transfer_log_fd, ERROR_SIGN,
                "%-*s[%d]: Incorrect reply from remote site. (%s %d)\n",
                MAX_HOSTNAME_LENGTH, tr_hostname,
                (int)db.job_no, __FILE__, __LINE__);

      /* Show context of what has been returned. */
      {
         int  line_length = 0,
              no_read = 10;
         char *ptr = buffer,
              line[10 + 40];

         do
         {
            if (*ptr < ' ')
            {
               /* Yuck! Not printable. */
#ifdef _SHOW_HEX
               line_length += sprintf(&line[line_length], "<%x>", (int)*ptr);
#else
               line_length += sprintf(&line[line_length], "<%d>", (int)*ptr);
#endif
            }
            else
            {
               line[line_length] = *ptr;
               line_length++;
            }

            ptr++; no_read--;
         } while (no_read > 0);

         line[line_length] = '\0';
         (void)rec(transfer_log_fd, DEBUG_SIGN,
                   "%-*s[%d]: %s\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, line);
      }
   }
   else if (n == -1) /* Read error */
        {
           (void)rec(transfer_log_fd, ERROR_SIGN,
                     "%-*s[%d]: read() error (after reading %d Bytes) : %s (%s %d)\n",
                     MAX_HOSTNAME_LENGTH, tr_hostname,
                     (int)db.job_no, n, strerror(errno),
                     __FILE__, __LINE__);
        }
   else if (n == -2) /* Timeout */
        {
           timeout_flag = ON;
        }
   else if (n == -3) /* Select error */
        {
           (void)rec(transfer_log_fd, ERROR_SIGN,
                     "%-*s[%d]: select() error : %s (%s %d)\n",
                     MAX_HOSTNAME_LENGTH, tr_hostname,
                     (int)db.job_no, strerror(errno),
                     __FILE__, __LINE__);
        }
        else
        {
           (void)rec(transfer_log_fd, ERROR_SIGN,
                     "%-*s[%d]: Remote site closed connection (after reading %d Bytes). (%s %d)\n",
                     MAX_HOSTNAME_LENGTH, tr_hostname,
                     (int)db.job_no, n, __FILE__, __LINE__);
        }

   return(INCORRECT);
}


/*############################## wmo_quit() #############################*/
void
wmo_quit(void)
{
   /*
    * DON'T do a shutdown when timeout_flag is ON!!! Let the kernel
    * handle the last buffers with TIME_WAIT.
    */
   if (p_data != NULL)
   {
      if (fflush(p_data) == EOF)
      {
         /*
          * If we fail to flush here it is not so bad, since wmo_write()
          * already flushed the data. This flush is when an error occured.
          */
         (void)rec(transfer_log_fd, WARN_SIGN,
                   "%-*s[%d]: fflush() error when closing connection : %s (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, strerror(errno), __FILE__, __LINE__);
      }

      if (timeout_flag != ON)
      {
         if (shutdown(fileno(p_data), 1) < 0)
         {
            (void)rec(transfer_log_fd, DEBUG_SIGN,
                      "%-*s[%d]: shutdown() error : %s (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname,
                      (int)db.job_no, strerror(errno), __FILE__, __LINE__);
         }
      }
      if (fclose(p_data) == EOF)
      {
         (void)rec(transfer_log_fd, DEBUG_SIGN,
                   "%-*s[%d]: fclose() error : %s (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, strerror(errno), __FILE__, __LINE__);
      }
   }

   return;
}
#endif /* _WITH_WMO_SUPPORT */

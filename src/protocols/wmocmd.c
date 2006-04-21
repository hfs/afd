/*
 *  wmocmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2006 Deutscher Wetterdienst (DWD),
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

#include "afddefs.h"

DESCR__S_M3
/*
 ** NAME
 **   wmocmd - commands to send files via TCP according WMO regulations
 **
 ** SYNOPSIS
 **   int  wmo_connect(char *hostname, int port, int sndbuf_size)
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
 **   indicate that the 'transfer_timeout' time has been reached.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   28.05.1998 H.Kiehl Created
 **   10.11.1998 H.Kiehl Added function wmp_check_reply().
 **   08.07.2000 H.Kiehl Cleaned up log output to reduce code size.
 **
 */
DESCR__E_M3


#include <stdio.h>
#include <string.h>           /* memset(), memcpy()                      */
#include <stdlib.h>           /* malloc(), free()                        */
#include <sys/types.h>        /* fd_set                                  */
#include <sys/time.h>         /* struct timeval                          */
#include <sys/socket.h>       /* socket(), shutdown(), bind(),           */
                              /* setsockopt()                            */
#include <netinet/in.h>       /* struct in_addr, sockaddr_in, htons()    */
#include <netdb.h>            /* struct hostent, gethostbyname()         */
#include <arpa/inet.h>        /* inet_addr()                             */
#include <unistd.h>           /* select(), write(), read(), close()      */
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
extern long               transfer_timeout;
extern char               tr_hostname[];
extern struct job         db;

/* Local global variables */
static int                sock_fd;
static struct timeval     timeout;

#define MAX_CHARS_IN_LINE 45


/*########################## wmo_connect() ##############################*/
int
wmo_connect(char *hostname, int port, int sndbuf_size)
{
#ifdef _WITH_WMO_SUPPORT
   int                     loop_counter = 0;
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
#if !defined (_HPUX) && !defined (_SCO)
         if (h_errno != 0)
         {
#ifdef LINUX
            if ((h_errno > 0) && (h_errno < h_nerr))
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to gethostbyname() %s : %s",
                         hostname, h_errlist[h_errno]);
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to gethostbyname() %s (h_errno = %d) : %s",
                         hostname, h_errno, strerror(errno));
            }
#else
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "Failed to gethostbyname() %s (h_errno = %d) : %s",
                      hostname, h_errno, strerror(errno));
#endif
         }
         else
         {
#endif /* !_HPUX && !_SCO */
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "Failed to gethostbyname() %s : %s",
                      hostname, strerror(errno));
#if !defined (_HPUX) && !defined (_SCO)
         }
#endif
         return(INCORRECT);
      }
      sin.sin_family = p_host->h_addrtype;

      /* Copy IP number to socket structure */
      memcpy((char *)&sin.sin_addr, p_host->h_addr, p_host->h_length);
   }

   if ((sock_fd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "socket() error : %s", strerror(errno));
      return(INCORRECT);
   }

   sin.sin_port = htons((u_short)port);

   while (connect(sock_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
   {
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
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "Failed to connect() to %s, have tried %d times : %s",
                   hostname, loop_counter, strerror(errno));
         (void)close(sock_fd);
         sock_fd = -1;
         return(INCORRECT);
      }
      if (close(sock_fd) == -1)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                   "close() error : %s", strerror(errno));
      }
      if ((sock_fd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "socket() error : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }
   }

#ifdef FTX
   l.l_onoff = 1; l.l_linger = 240;
   if (setsockopt(sock_fd, SOL_SOCKET, SO_LINGER, (char *)&l,
                  sizeof(struct linger)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "setsockopt() error : %s", strerror(errno));
      return(INCORRECT);
   }
#endif
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
   timeout.tv_sec = transfer_timeout;

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
           if ((status = send(sock_fd, block, size, 0)) != size)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "send() error (%d) : %s", status, strerror(errno));
              return(errno);
           }
#else
           if ((status = write(sock_fd, block, size)) != size)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "write() error (%d) : %s", status, strerror(errno));
              return(errno);
           }
#endif
        }
        else if (status < 0)
             {
                trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                          "select() error : %s", strerror(errno));
                return(INCORRECT);
             }
             else
             {
                trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                          "Unknown condition.");
                return(INCORRECT);
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

      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "Incorrect reply from remote site.");

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
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, "%s", line);
      }
   }
   else if (n == -1) /* Read error */
        {
           if (errno == ECONNRESET)
           {
              timeout_flag = CON_RESET;
           }
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "read() error : %s", strerror(errno));
        }
   else if (n == -2) /* Timeout */
        {
           timeout_flag = ON;
        }
   else if (n == -3) /* Select error */
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "select() error : %s", strerror(errno));
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "Remote site closed connection.");
        }

   return(INCORRECT);
}


/*############################## wmo_quit() #############################*/
void
wmo_quit(void)
{
   if (sock_fd != -1)
   {
      if ((timeout_flag != ON) && (timeout_flag != CON_RESET))
      {
         if (shutdown(sock_fd, 1) < 0)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                      "shutdown() error : %s", strerror(errno));
         }
         else
         {
            int    status;
            char   buffer[32];
            fd_set rset;

            /* Initialise descriptor set */
            FD_ZERO(&rset);
            FD_SET(sock_fd, &rset);
            timeout.tv_usec = 0L;
            timeout.tv_sec = transfer_timeout;

            /* Wait for message x seconds and then continue. */
            status = select(sock_fd + 1, &rset, NULL, NULL, &timeout);

            if (status == 0)
            {
               /* timeout has arrived */
               timeout_flag = ON;
               return;
            }
            else if (FD_ISSET(sock_fd, &rset))
                 {
                    if ((status = read(sock_fd, buffer, 32)) < 0)
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                 "read() error (%d) : %s",
                                 status, strerror(errno));
                       return;
                    }
                 }
            else if (status < 0)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                              "select() error : %s", strerror(errno));
                    return;
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                              "Unknown condition.");
                    return;
                 }
         }
      }
      if (close(sock_fd) == -1)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                   "close() error : %s", strerror(errno));
      }
      sock_fd = -1;
   }

   return;
}
#endif /* _WITH_WMO_SUPPORT */

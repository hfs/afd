/*
 *  tcpcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   tcpcmd - commands to send files via TCP/IP
 **
 ** SYNOPSIS
 **   int  tcp_connect(char *hostname, int port)
 **   int  tcp_cmd(char *cmd)
 **   int  tcp_quit(void)
 **
 ** DESCRIPTION
 **   tcpcmd provides a set of commands to communicate with an TCP
 **   server via BSD sockets.
 **   The procedure to send commands to another TCP server is as
 **   follows:
 **          tcp_connect()
 **             |
 **             +<---------+
 **             |          |
 **             V          |
 **          tcp_cmd()-----+
 **             |
 **             V
 **          tcp_quit()
 **
 **
 ** RETURN VALUES
 **   Returns SUCCESS when successful. When an error has occurred
 **   it will either return INCORRECT or the three digit TCP reply
 **   code when the reply of the server does not conform to the
 **   one expected. The complete reply string of the TCP server
 **   is returned to 'msg_str'. 'timeout_flag' is just a flag to
 **   indicate that the 'tcp_timeout' time has been reached.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.08.1998 H.Kiehl Created
 **
 */
DESCR__E_M3


#include <stdio.h>            /* fprintf(), fdopen(), fclose()           */
#include <stdarg.h>           /* va_start(), va_arg(), va_end()          */
#include <string.h>           /* memset(), memcpy(), strcpy()            */
#include <ctype.h>            /* isdigit()                               */
#include <sys/types.h>        /* fd_set                                  */
#include <sys/time.h>         /* struct timeval                          */
#include <sys/socket.h>       /* socket(), shutdown(), bind(), accept(), */
                              /* setsockopt()                            */
#include <netinet/in.h>       /* struct in_addr, sockaddr_in, htons()    */
#include <netdb.h>            /* struct hostent, gethostbyname()         */
#include <arpa/inet.h>        /* inet_addr()                             */
#include <unistd.h>           /* select(), exit(), write(), read(),      */
                              /* close()                                 */
#include <errno.h>
#include "mondefs.h"


/* External global variables */
extern int                    afd_no,
                              timeout_flag;
extern char                   msg_str[];
#ifdef LINUX
extern char                   *h_errlist[];  /* for gethostbyname()          */
extern int                    h_nerr;        /* for gethostbyname()          */
#endif
extern int                    h_errno,       /* for gethostbyname()          */
                              sock_fd,
                              sys_log_fd,
                              mon_log_fd;
extern long                   tcp_timeout;
extern struct mon_status_area *msa;

/* Local global variables */
static FILE                   *p_control;
static struct sockaddr_in     ctrl;
static struct timeval         timeout;

/* Local functions */
static int                    get_reply(void),
                              check_reply(int, ...);


/*########################## tcp_connect() ##############################*/
int
tcp_connect(char *hostname, int port)
{
   int                     length,
                           reply;
   struct sockaddr_in      sin;
   register struct hostent *p_host = NULL;

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
               (void)rec(mon_log_fd, ERROR_SIGN,
                         "%-*s: Failed to gethostbyname() %s : %s (%s %d)\n",
                         MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                         hostname, h_errlist[h_errno],
                         __FILE__, __LINE__);
	    }
	    else
	    {
               (void)rec(mon_log_fd, ERROR_SIGN,
                         "%-*s: Failed to gethostbyname() %s (h_errno = %d) : %s (%s %d)\n",
                         MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                         hostname, h_errno, strerror(errno),
                         __FILE__, __LINE__);
	    }
#else
            (void)rec(mon_log_fd, ERROR_SIGN,
                      "%-*s: Failed to gethostbyname() %s (h_errno = %d) : %s (%s %d)\n",
                      MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                      hostname, h_errno, strerror(errno),
                      __FILE__, __LINE__);
#endif
	 }
	 else
	 {
            (void)rec(mon_log_fd, ERROR_SIGN,
                      "%-*s: Failed to gethostbyname() %s : %s (%s %d)\n",
                      MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                      hostname, strerror(errno), __FILE__, __LINE__);
	 }
         return(INCORRECT);
      }
      sin.sin_family = p_host->h_addrtype;

      /* Copy IP number to socket structure */
      memcpy((char *)&sin.sin_addr, p_host->h_addr, p_host->h_length);
   }

   if ((sock_fd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
   {
      (void)rec(mon_log_fd, ERROR_SIGN,
                "%-*s: socket() error : %s (%s %d)\n",
                MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   sin.sin_port = htons((u_short)port);

#ifdef _TRY_ALL_HOSTS
   while (connect(sock_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
   {
      if (p_host == NULL)
      {
         (void)rec(mon_log_fd, ERROR_SIGN,
                   "%-*s: Failed to connect() to %s : %s (%s %d)\n",
                   MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                   hostname, strerror(errno), __FILE__, __LINE__);
         (void)close(sock_fd);
         return(INCORRECT);
      }
      p_host->h_addr_list++;
      if (p_host->h_addr_list[0] == NULL)
      {
         (void)rec(mon_log_fd, ERROR_SIGN,
                   "%-*s: Failed to connect() to %s. Have tried all hosts in h_addr_list. (%s %d)\n",
                   MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                   hostname, __FILE__, __LINE__);
         (void)close(sock_fd);
         return(INCORRECT);
      }
      memcpy((char *)&sin.sin_addr, p_host->h_addr_list[0], p_host->h_length);
      if (close(sock_fd) == -1)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "close() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
      if ((sock_fd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
      {
         (void)rec(mon_log_fd, ERROR_SIGN,
                   "%-*s: socket() error : %s (%s %d)\n",
                   MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                   strerror(errno), __FILE__, __LINE__);
         (void)close(sock_fd);
         return(INCORRECT);
      }
   }
#else
   if (connect(sock_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
   {
      (void)rec(mon_log_fd, ERROR_SIGN,
                "%-*s: Failed to connect() to %s : %s (%s %d)\n",
                MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                hostname, strerror(errno), __FILE__, __LINE__);
      (void)close(sock_fd);
      return(INCORRECT);
   }
#endif

   length = sizeof(ctrl);
   if (getsockname(sock_fd, (struct sockaddr *)&ctrl, &length) < 0)
   {
      (void)rec(mon_log_fd, ERROR_SIGN,
                "%-*s: getsockname() error : %s (%s %d)\n",
                MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                strerror(errno), __FILE__, __LINE__);
      (void)close(sock_fd);
      return(INCORRECT);
   }

   if ((p_control = fdopen(sock_fd, "r+")) == NULL)
   {
      (void)rec(mon_log_fd, ERROR_SIGN,
                "%-*s: fdopen() control error : %s (%s %d)\n",
                MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                strerror(errno), __FILE__, __LINE__);
      (void)close(sock_fd);
      return(INCORRECT);
   }

   if ((reply = get_reply()) < 0)
   {
      (void)fclose(p_control);
      p_control = NULL;
      return(INCORRECT);
   }
   if (check_reply(2, reply, 220) < 0)
   {
      (void)fclose(p_control);
      p_control = NULL;
      return(reply);
   }

   return(SUCCESS);
}


/*############################## tcp_cmd() ##############################*/
int
tcp_cmd(char *cmd)
{
   int bytes_buffered;

   (void)fprintf(p_control, "%s\r\n", cmd);
   (void)fflush(p_control);

   if (read_msg() == INCORRECT)
   {
      return(INCORRECT);
   }
   if ((msg_str[0] == '2') && (msg_str[1] == '1') &&
       (msg_str[2] == '1') && (msg_str[3] == '-'))
   {
      if ((bytes_buffered = read_msg()) == INCORRECT)
      {
         return(INCORRECT);
      }
   }
   else
   {
      return(((msg_str[0] - '0') * 100) + ((msg_str[1] - '0') * 10) +
             (msg_str[2] - '0'));
   }

   return(bytes_buffered);
}


/*############################## tcp_quit() #############################*/
int
tcp_quit(void)
{
   if (p_control != NULL)
   {
      int reply;

      (void)fprintf(p_control, "QUIT\r\n");
      if (fflush(p_control) == EOF)
      {
         (void)rec(mon_log_fd, WARN_SIGN,
                   "%-*s: Failed to fflush() connection : %s (%s %d)\n",
                   MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                   strerror(errno), __FILE__, __LINE__);
      }

      /*
       * If timeout_flag is ON, lets NOT check the reply from
       * the QUIT command. Else we are again waiting 'ftp_timeout'
       * seconds for the reply!
       */
      if (timeout_flag != ON)
      {
         if ((reply = get_reply()) < 0)
         {
            return(INCORRECT);
         }

         /*
          * NOTE: Lets not count the 421 warning as an error.
          */
         if (check_reply(3, reply, 221, 421) < 0)
         {
            return(reply);
         }
      }

      /*
       * DON'T do a shutdown when timeout_flag is ON!!! Let the kernel
       * handle the last buffers with TIME_WAIT.
       */
      if (timeout_flag != ON)
      {
         if (shutdown(sock_fd, 1) < 0)
         {
            (void)rec(mon_log_fd, DEBUG_SIGN,
                      "%-*s: shutdown() error : %s (%s %d)\n",
                      MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                      strerror(errno), __FILE__, __LINE__);
         }
      }
      if (fclose(p_control) == EOF)
      {
         (void)rec(mon_log_fd, DEBUG_SIGN,
                   "%-*s: fclose() error : %s (%s %d)\n",
                   MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                   strerror(errno), __FILE__, __LINE__);
      }
      p_control = NULL;
   }

   return(SUCCESS);
}


/*++++++++++++++++++++++++++++++ get_reply() ++++++++++++++++++++++++++++*/
static int
get_reply(void)
{
   for (;;)
   {
      if (read_msg() == INCORRECT)
      {
         return(INCORRECT);
      }

      /*
       * Lets ignore anything we get from the remote site
       * not starting with three digits and a dash.
       */
      if ((isdigit(msg_str[0]) != 0) && (isdigit(msg_str[1]) != 0) &&
          (isdigit(msg_str[2]) != 0) && (msg_str[3] != '-'))
      {
         break;
      }
   }

   return(((msg_str[0] - '0') * 100) + ((msg_str[1] - '0') * 10) +
          (msg_str[2] - '0'));
}


/*+++++++++++++++++++++++++++++ read_msg() ++++++++++++++++++++++++++++++*/
int
read_msg(void)
{
   static int  bytes_buffered,
               bytes_read = 0;
   static char *read_ptr = NULL;

   if (bytes_read == 0)
   {
      bytes_buffered = 0;
   }
   else
   {
      memmove(msg_str, read_ptr + 1, bytes_read);
      bytes_buffered = bytes_read;
      read_ptr = msg_str;
   }

   for (;;)
   {
      if (bytes_read <= 0)
      {
         int    status;
         fd_set rset;

         /* Initialise descriptor set */
         FD_ZERO(&rset);
         FD_SET(sock_fd, &rset);
         timeout.tv_usec = 0L;
         timeout.tv_sec = tcp_timeout;

         /* Wait for message x seconds and then continue. */
         status = select(sock_fd + 1, &rset, NULL, NULL, &timeout);

         if (status == 0)
         {
            /* timeout has arrived */
            timeout_flag = ON;
            bytes_read = 0;
            return(INCORRECT);
         }
         else if (FD_ISSET(sock_fd, &rset))
              {
                 if ((bytes_read = read(sock_fd, &msg_str[bytes_buffered],
                                        (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                 {
                    if (bytes_read == 0)
                    {
                       (void)rec(mon_log_fd, ERROR_SIGN,
                                 "%-*s: Remote hang up. (%s %d)\n",
                                 MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                                 __FILE__, __LINE__);
                    }
                    else
                    {
                       (void)rec(mon_log_fd, ERROR_SIGN,
                                 "%-*s: read() error (after reading %d Bytes) : %s (%s %d)\n",
                                 MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                                 bytes_buffered, strerror(errno), __FILE__, __LINE__);
                    }
                    bytes_read = 0;
                    return(INCORRECT);
                 }
                 read_ptr = &msg_str[bytes_buffered];
                 bytes_buffered += bytes_read;
              }
         else if (status < 0)
              {
                 (void)rec(mon_log_fd, ERROR_SIGN,
                           "%-*s: select() error : %s (%s %d)\n",
                           MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                           strerror(errno), __FILE__, __LINE__);
                 exit(INCORRECT);
              }
              else
              {
                 (void)rec(sys_log_fd, ERROR_SIGN,
                           "Unknown condition. (%s %d)\n",
                           __FILE__, __LINE__);
                 exit(INCORRECT);
              }
      }

      /* Evaluate what we have read. */
      do
      {
         if ((*read_ptr == '\n') && (*(read_ptr - 1) == '\r'))
         {                                                                
            *(read_ptr - 1) = '\0';
            bytes_read--;
            return(bytes_buffered);
         }
         read_ptr++;
         bytes_read--;
      } while(bytes_read > 0);
   } /* for (;;) */
}


/*+++++++++++++++++++++++++++++ check_reply() +++++++++++++++++++++++++++*/
static int
check_reply(int count, ...)
{
   int     i,
           reply;
   va_list ap;

   va_start(ap, count);
   reply = va_arg(ap, int);
   for (i = 1; i < count; i++)
   {
      if (reply == va_arg(ap, int))
      {
         va_end(ap);
         return(SUCCESS);
      }
   }
   va_end(ap);

   return(INCORRECT);
}

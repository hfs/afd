/*
 *  ftpcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   ftpcmd - commands to send files via FTP
 **
 ** SYNOPSIS
 **   int  ftp_connect(char *hostname, int port)
 **   int  ftp_user(char *user)
 **   int  ftp_pass(char *password)
 **   int  ftp_type(char type)
 **   int  ftp_cd(char *directory)
 **   int  ftp_move(char *from, char *to)
 **   int  ftp_dele(char *filename)
 **   int  ftp_list(char *filename, char *msg)
 **   int  ftp_data(char *filename, int seek)
 **   int  ftp_close_data(void)
 **   int  ftp_write(char *block, char *buffer, int size)
 **   int  ftp_quit(void)
 **   int  ftp_get_reply(void)
 **
 ** DESCRIPTION
 **   ftpcmd provides a set of commands to communicate with an FTP
 **   server via BSD sockets.
 **   The procedure to send files to another FTP server is as
 **   follows:
 **          ftp_connect()
 **             |
 **             V
 **          ftp_user()
 **             |
 **             V
 **     +---------------+ YES
 **     | reply = 230 ? |----+
 **     +---------------+    |
 **             |            |
 **             V            |
 **          ftp_pass()      |
 **             |            |
 **             +<-----------+
 **             |
 **             V
 **          ftp_type() -----> ftp_cd()
 **             |                 |
 **             +<----------------+
 **             |
 **             V
 **          ftp_data()<-----------------------+
 **             |                              |
 **             V                              |
 **          ftp_write()<----+                 |
 **             |            |                 |
 **             V            |                 |
 **      +-------------+  NO |                 |
 **      | File done ? |-----+                 |
 **      +-------------+                       |
 **             |                              |
 **             V                              |
 **          ftp_close_data() ---> ftp_move()  |
 **             |                     |        |
 **             +<--------------------+        |
 **             |                              |
 **             V                              |
 **      +-------------+           YES         |
 **      | Next file ? |-----------------------+
 **      +-------------+
 **             |
 **             +-------> ftp_dele()
 **             |            |
 **             +<-----------+
 **             |
 **             V
 **          ftp_quit()
 **
 **
 ** RETURN VALUES
 **   Returns SUCCESS when successful. When an error has occurred
 **   it will either return INCORRECT or the three digit FTP reply
 **   code when the reply of the server does not conform to the
 **   one expected. The complete reply string of the FTP server
 **   is returned to 'msg_str'. 'timeout_flag' is just a flag to
 **   indicate that the 'ftp_timeout' time has been reached.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.02.1996 H.Kiehl Created
 **   02.02.1997 H.Kiehl Appending files when already partly transmitted.
 **   20.04.1997 H.Kiehl Fixed ftp_connect() so it does take an IP number.
 **   22.04.1997 H.Kiehl When in ftp_data() the accept times out, set
 **                      timeout_flag!
 **   05.05.1997 H.Kiehl Added function ftp_get_reply().
 **   08.09.1998 H.Kiehl ftp_user() returns 230, so application can handle
 **                      the case where no password is required.
 **   22.08.1999 H.Kiehl Use type-of-service field to minimize delay for
 **                      control connection and maximize throughput for
 **                      data connection.
 **
 */
DESCR__E_M3


#include <stdio.h>            /* fprintf(), fdopen(), fclose()           */
#include <stdarg.h>           /* va_start(), va_arg(), va_end()          */
#include <string.h>           /* memset(), memcpy(), strcpy()            */
#include <ctype.h>            /* isdigit()                               */
#include <setjmp.h>           /* setjmp(), longjmp()                     */
#include <signal.h>           /* signal()                                */
#include <sys/types.h>        /* fd_set                                  */
#include <sys/time.h>         /* struct timeval                          */
#include <sys/socket.h>       /* socket(), shutdown(), bind(), accept(), */
                              /* setsockopt()                            */
#include <netinet/in.h>       /* struct in_addr, sockaddr_in, htons()    */
#if defined (_WITH_TOS) && defined (IP_TOS)
#if defined (IRIX) || defined (_SUN)
#include <netinet/in_systm.h>
#endif /* IRIX || _SUN */
#include <netinet/ip.h>       /* IPTOS_LOWDELAY, IPTOS_THROUGHPUT        */
#endif
#include <netdb.h>            /* struct hostent, gethostbyname()         */
#include <arpa/inet.h>        /* inet_addr()                             */
#include <unistd.h>           /* select(), exit(), write(), read(),      */
                              /* close(), alarm()                        */
#include <errno.h>
#include "fddefs.h"
#include "ftpdefs.h"


/* External global variables */
extern int                timeout_flag;
extern char               msg_str[];
#ifdef LINUX
extern char               *h_errlist[];  /* for gethostbyname()          */
extern int                h_nerr;        /* for gethostbyname()          */
#endif
extern int                sys_log_fd,
                          transfer_log_fd;
extern long               ftp_timeout;

/* Local global variables */
static int                data_fd;
static FILE               *p_control,
                          *p_data;
static jmp_buf            env_alrm;
static struct sockaddr_in ctrl,
                          data;
static struct timeval     timeout;

/* Local function prototypes */
static int                check_data_socket(int, int, int *),
                          check_reply(int, ...),
                          get_reply(FILE *),
                          read_data_line(FILE *, char *),
                          read_msg(FILE *);
static void               sig_handler(int);


/*########################## ftp_connect() ##############################*/
int
ftp_connect(char *hostname, int port)
{
   int                     length,
                           reply,
                           sock_fd;
#if defined (_WITH_TOS) && defined (IP_TOS)
   int                     tos;
#endif
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
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to gethostbyname() %s : %s",
                         hostname, h_errlist[h_errno]);
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to gethostbyname() %s (h_errno = %d) : %s",
                         hostname, h_errno, strerror(errno));
            }
#else
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to gethostbyname() %s (h_errno = %d) : %s",
                      hostname, h_errno, strerror(errno));
#endif
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to gethostbyname() %s : %s",
                      hostname, strerror(errno));
         }
         return(INCORRECT);
      }
      sin.sin_family = p_host->h_addrtype;

      /* Copy IP number to socket structure */
      memcpy((char *)&sin.sin_addr, p_host->h_addr, p_host->h_length);
   }

   if ((sock_fd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "socket() error : %s", strerror(errno));
      return(INCORRECT);
   }

   sin.sin_port = htons((u_short)port);

   if (connect(sock_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to connect() to %s : %s",
                hostname, strerror(errno));
      (void)close(sock_fd);
      return(INCORRECT);
   }

   length = sizeof(ctrl);
   if (getsockname(sock_fd, (struct sockaddr *)&ctrl, &length) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "getsockname() error : %s", strerror(errno));
      (void)close(sock_fd);
      return(INCORRECT);
   }

#if defined (_WITH_TOS) && defined (IP_TOS)
   tos = IPTOS_LOWDELAY;
   if (setsockopt(sock_fd, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(tos)) < 0)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__,
                "setsockopt() IP_TOS error : %s", strerror(errno));
   }
#endif

   if ((p_control = fdopen(sock_fd, "r+")) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "fdopen() control error : %s", strerror(errno));
      (void)close(sock_fd);
      return(INCORRECT);
   }

   if ((reply = get_reply(p_control)) < 0)
   {
      (void)fclose(p_control);
      return(INCORRECT);
   }
   if (check_reply(3, reply, 120, 220) < 0)
   {
      (void)fclose(p_control);
      return(reply);
   }

   return(SUCCESS);
}


/*############################## ftp_user() #############################*/
int
ftp_user(char *user)
{
   int reply,
       count = 0;

   do
   {
      (void)fprintf(p_control, "USER %s\r\n", user);
      (void)fflush(p_control);

      if ((reply = get_reply(p_control)) < 0)
      {
         return(INCORRECT);
      }

      /*
       * Some brain-damaged implementations think we are still
       * logged on, when we try to log in too quickly after we
       * just did a log-off.
       */
      if (reply == 430)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                   "Hmmm. Still thinks I am logged on. Lets wait for a while.");
         (void)my_usleep(700000L);
      }
   } while ((reply == 430) && (count++ < 10));

   /*
    * NOTE: We delibaretly ignore 230 here, since this means that no
    *       password is required. Thus we do have to return the 230
    *       so the apllication knows what to do with it.
    */
   if (check_reply(3, reply, 331, 332) < 0)
   {
      return(reply);
   }

   return(SUCCESS);
}


/*############################## ftp_pass() #############################*/
int
ftp_pass(char *password)
{
   int reply;

   (void)fprintf(p_control, "PASS %s\r\n", password);
   (void)fflush(p_control);

   if ((reply = get_reply(p_control)) < 0)
   {
      return(INCORRECT);
   }

   if (check_reply(4, reply, 202, 230, 332) < 0)
   {
      return(reply);
   }

   return(SUCCESS);
}


/*############################## ftp_type() #############################*/
int
ftp_type(char type)
{
   int reply;

   (void)fprintf(p_control, "TYPE %c\r\n", type);
   (void)fflush(p_control);

   if ((reply = get_reply(p_control)) < 0)
   {
      return(INCORRECT);
   }

   if (check_reply(2, reply, 200) < 0)
   {
      return(reply);
   }

   return(SUCCESS);
}


/*############################### ftp_cd() ##############################*/
int
ftp_cd(char *directory)
{
   int reply;

   (void)fprintf(p_control, "CWD %s\r\n", directory);
   (void)fflush(p_control);

   if ((reply = get_reply(p_control)) < 0)
   {
      return(INCORRECT);
   }

   if (check_reply(3, reply, 250, 200) < 0)
   {
      return(reply);
   }

   return(SUCCESS);
}


/*############################## ftp_move() #############################*/
int
ftp_move(char *from, char *to)
{
   int reply;

   (void)fprintf(p_control, "RNFR %s\r\n", from);
   (void)fflush(p_control);

   if ((reply = get_reply(p_control)) < 0)
   {
      return(INCORRECT);
   }

   if (check_reply(3, reply, 350, 200) < 0)
   {
      return(reply);
   }

   (void)fprintf(p_control, "RNTO %s\r\n", to);
   (void)fflush(p_control);

   if ((reply = get_reply(p_control)) < 0)
   {
      return(INCORRECT);
   }

   if (check_reply(3, reply, 250, 200) < 0)
   {
      return(reply);
   }

   return(SUCCESS);
}


/*############################## ftp_dele() #############################*/
int
ftp_dele(char *filename)
{
   int reply;

   (void)fprintf(p_control, "DELE %s\r\n", filename);
   (void)fflush(p_control);

   if ((reply = get_reply(p_control)) < 0)
   {
      return(INCORRECT);
   }

   if (check_reply(3, reply, 250, 200) < 0)
   {
      return(reply);
   }

   return(SUCCESS);
}


/*############################## ftp_list() #############################*/
int
ftp_list(char *filename, char *msg)
{
   int                sock_fd,
                      new_sock_fd,
                      on = 1,
                      length,
                      reply;
   register char      *h,
                      *p;
   struct sockaddr_in from;

   data = ctrl;
   data.sin_port = htons((u_short)0);

   if ((sock_fd = socket(data.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "socket() error : %s", strerror(errno));
      return(INCORRECT);
   }

   if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof (on)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "setsockopt() error : %s", strerror(errno));
      (void)close(sock_fd);
      return(INCORRECT);
   }

   length = sizeof(data);
   if (bind(sock_fd, (struct sockaddr *)&data, length) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "bind() error : %s", strerror(errno));
      (void)close(sock_fd);
      return(INCORRECT);
   }

   if (getsockname(sock_fd, (struct sockaddr *)&data, &length) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "getsockname() error : %s", strerror(errno));
      (void)close(sock_fd);
      return(INCORRECT);
   }

   if (listen(sock_fd, 1) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "listen() error : %s", strerror(errno));
      (void)close(sock_fd);
      return(INCORRECT);
   }

   h = (char *)&data.sin_addr;
   p = (char *)&data.sin_port;

   (void)fprintf(p_control, "PORT %d,%d,%d,%d,%d,%d\r\n",
                 (((int)h[0]) & 0xff),
                 (((int)h[1]) & 0xff),
                 (((int)h[2]) & 0xff),
                 (((int)h[3]) & 0xff),
                 (((int)p[0]) & 0xff),
                 (((int)p[1]) & 0xff));
   (void)fflush(p_control);

   if ((reply = get_reply(p_control)) < 0)
   {
      (void)close(sock_fd);
      return(INCORRECT);
   }

   if (check_reply(2, reply, 200) < 0)
   {
      (void)close(sock_fd);
      return(reply);
   }

   (void)fprintf(p_control, "LIST %s\r\n", filename);
   (void)fflush(p_control);

   if ((reply = get_reply(p_control)) < 0)
   {
      return(INCORRECT);
   }

   if (check_reply(3, reply, 125, 150) < 0)
   {
      return(reply);
   }

   /*
    * Juck! Here follows some ugly code.
    * Experience has shown that on very rare occasions the
    * accept() call blocks. This is normal behaviour of the accept()
    * system call. Could make sock_fd non-blocking, but then
    * the next time we use the socket we will get an error. This is
    * not we want. When we have a bad connection it can take quit
    * some time before we get a respond.
    * This is not the best solution. If anyone has a better solution
    * please tell me.
    * I have experienced these problems only with FTX 3.0.x, so it
    * could also be a kernel bug. Maybe the system sometimes forgets
    * to restart this system call?
    */
   if (signal(SIGALRM, sig_handler) == SIG_ERR)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to set signal handler : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      (void)close(sock_fd);
      return(INCORRECT);
   }
   if (setjmp(env_alrm) != 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "accept() timeout");
      timeout_flag = ON;
      (void)close(sock_fd);
      return(INCORRECT);
   }
   (void)alarm(2 * ftp_timeout);

   if ((new_sock_fd = accept(sock_fd, (struct sockaddr *) &from, &length)) < 0)
   {
      (void)alarm(0); /* Maybe it was a real accept() error */
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "accept() error : %s", strerror(errno));
      (void)close(sock_fd);
      return(INCORRECT);
   }
   (void)alarm(0);
   if (close(sock_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   if ((p_data = fdopen(new_sock_fd, "r")) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "fdopen() data error : %s", strerror(errno));
      (void)close(new_sock_fd);
      return(INCORRECT);
   }

   if (read_data_line(p_data, msg) < 0)
   {
      return(INCORRECT);
   }

   /* Read last message: 'Binary Transfer complete' */
   if ((reply = get_reply(p_control)) < 0)
   {
      return(INCORRECT);
   }

   if (check_reply(2, reply, 226) < 0)
   {
      return(reply);
   }

   return(SUCCESS);
}


/*############################## ftp_data() #############################*/
int
ftp_data(char *filename, int seek)
{
   int                new_sock_fd,
                      on = 1,
                      length,
                      reply,
                      retries = 0,
                      ret,
                      sock_fd;
#if defined (_WITH_TOS) && defined (IP_TOS)
   int                tos;
#endif
   register char      *h,
                      *p;
   struct sockaddr_in from;
#ifdef FTX
   struct linger      l;
#endif

   do
   {
      data = ctrl;
      data.sin_port = htons((u_short)0);

      if ((sock_fd = socket(data.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "socket() error : %s", strerror(errno));
         return(INCORRECT);
      }

      if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "setsockopt() error : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }

      length = sizeof(data);
      if (bind(sock_fd, (struct sockaddr *)&data, length) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "bind() error : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }

      if (getsockname(sock_fd, (struct sockaddr *)&data, &length) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "getsockname() error : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }

      if (listen(sock_fd, 1) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "listen() error : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }

      h = (char *)&data.sin_addr;
      p = (char *)&data.sin_port;

      (void)fprintf(p_control, "PORT %d,%d,%d,%d,%d,%d\r\n",
                    (((int)h[0]) & 0xff),
                    (((int)h[1]) & 0xff),
                    (((int)h[2]) & 0xff),
                    (((int)h[3]) & 0xff),
                    (((int)p[0]) & 0xff),
                    (((int)p[1]) & 0xff));
      (void)fflush(p_control);

      if ((reply = get_reply(p_control)) < 0)
      {
         if (timeout_flag != ON)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__,
                      "Failed to get proper reply (%d)", reply);
         }
         (void)close(sock_fd);
         return(INCORRECT);
      }

      if (check_reply(2, reply, 200) < 0)
      {
         (void)close(sock_fd);
         return(reply);
      }

      if (seek != 0)
      {
         (void)fprintf(p_control, "APPE %s\r\n", filename);
      }
      else
      {
         (void)fprintf(p_control, "STOR %s\r\n", filename);
      }
      (void)fflush(p_control);

      if ((reply = get_reply(p_control)) < 0)
      {
         if (timeout_flag != ON)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__,
                      "Failed to get proper reply (%d).", reply);
         }
         (void)close(sock_fd);
         return(INCORRECT);
      }
   } while ((ret = check_data_socket(reply, sock_fd, &retries)) == 1);

   if (ret < 0)
   {
      if (reply < 0)
      {
         return(reply);
      }
      else
      {
         return(-reply);
      }
   }

   /*
    * Juck! Here follows some ugly code.
    * Experience has shown that on very rare occasions the
    * accept() call blocks. This is normal behaviour of the accept()
    * system call. Could make sock_fd non-blocking, but then
    * the next time we use the socket we will get an error. This is
    * not what we want. When we have a bad connection it can take quit
    * some time before we get a respond.
    * This is not the best solution. If anyone has a better solution
    * please tell me.
    * I have experienced these problems only with FTX 3.0.x, so it
    * could also be a kernel bug. Maybe the system sometimes forgets
    * to restart this system call?
    */
   if (signal(SIGALRM, sig_handler) == SIG_ERR)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to set signal handler : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      (void)close(sock_fd);
      return(INCORRECT);
   }
   if (setjmp(env_alrm) != 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "accept() timeout");
      timeout_flag = ON;
      (void)close(sock_fd);
      return(INCORRECT);
   }
   (void)alarm(2 * ftp_timeout);

   if ((new_sock_fd = accept(sock_fd, (struct sockaddr *) &from, &length)) < 0)
   {
      (void)alarm(0); /* Maybe it was a real accept() error */
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "accept() error : %s", strerror(errno));
      (void)close(sock_fd);
      return(INCORRECT);
   }
   (void)alarm(0);
   if (close(sock_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

#ifdef FTX
   l.l_onoff = 1; l.l_linger = 240;
   if (setsockopt(new_sock_fd, SOL_SOCKET, SO_LINGER,
                  (char *)&l, sizeof(struct linger)) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "setsockopt() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      (void)close(new_sock_fd);
      return(INCORRECT);
   }
#endif

#if defined (_WITH_TOS) && defined (IP_TOS)
   tos = IPTOS_THROUGHPUT;
   if (setsockopt(new_sock_fd, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(tos)) < 0)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__,
                "setsockopt() IP_TOS error : %s", strerror(errno));
   }
#endif

   data_fd = new_sock_fd;
   if ((p_data = fdopen(new_sock_fd, "r+")) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "fdopen() data error : %s", strerror(errno));
      (void)close(new_sock_fd);
      return(INCORRECT);
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++ check_data_socket() +++++++++++++++++++++++++*/
static int
check_data_socket(int reply, int sock_fd, int *retries)
{
   /*
    * The replies for STOR and APPE should be the same.
    */
   if (check_reply(6, reply, 125, 150, 120, 250, 200) < 0)
   {
      if (close(sock_fd) == -1)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "close() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }

      /*
       * If we do not get a data connection lets try it again.
       * It could be that the address is already in use.
       */
      if ((reply == 425) && (*retries < MAX_DATA_CONNECT_RETRIES))
      {
         (*retries)++;
         (void)my_usleep(10000L);

         return(1);
      }

      return(-1);
   }

   return(0);
}


#ifdef _BLOCK_MODE
/*############################## ftp_open() #############################*/
int
ftp_open(char *filename, int seek)
{
   int reply;

   if (seek != 0)
   {
      (void)fprintf(p_control, "REST %d\r\n", seek);
      (void)fflush(p_control);

      if ((reply = get_reply(p_control)) < 0)
      {
         return(INCORRECT);
      }

      if (check_reply(2, reply, 350) < 0)
      {
         return(reply);
      }
   }

   (void)fprintf(p_control, "STOR %s\r\n", filename);
   (void)fflush(p_control);

   if ((reply = get_reply(p_control)) < 0)
   {
      return(INCORRECT);
   }

   if (check_reply(6, reply, 125, 150, 120, 250, 200) < 0)
   {
      return(INCORRECT);
   }

   return(SUCCESS);
}
#endif /* _BLOCK_MODE */


/*########################### ftp_close_data() ##########################*/
int
ftp_close_data(void)
{
   int reply;

   if (fflush(p_data) == EOF)
   {
      trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                "Failed to fflush() data : %s", strerror(errno));
      (void)fclose(p_data);
      p_data = NULL;
      return(INCORRECT);
   }

#ifdef _WITH_SHUTDOWN
   if (shutdown(fileno(p_data), 1) < 0)
   {
      trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                "shutdown() error : %s", strerror(errno));
   }
#endif

   if (fclose(p_data) == EOF)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "fclose() error : %s", strerror(errno));
      return(INCORRECT);
   }
   p_data = NULL;

   if (timeout_flag != ON)
   {
      if ((reply = get_reply(p_control)) < 0)
      {
         return(INCORRECT);
      }

      if (check_reply(2, reply, 226) < 0)
      {
         return(reply);
      }
   }

   return(SUCCESS);
}


/*############################## ftp_write() ############################*/
int
ftp_write(char *block, char *buffer, int size)
{
   char   *ptr = block;
   int    status;
   fd_set wset;

   /* Initialise descriptor set */
   FD_ZERO(&wset);
   FD_SET(data_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = ftp_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(data_fd + 1, NULL, &wset, NULL, &timeout);

   if (status == 0)
   {
      /* timeout has arrived */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (FD_ISSET(data_fd, &wset))
        {
           /*
            * When buffer is not NULL we want to sent the
            * data in ASCII-mode.
            */
           if (buffer != NULL)
           {
              register int i,
                           count = 0;

              for (i = 0; i < size; i++)
              {
                 if (*ptr == '\n')
                 {
                    buffer[count++] = '\r';
                    buffer[count++] = '\n';
                 }
                 else
                 {
                    buffer[count++] = *ptr;
                 }
                 ptr++;
              }
              size = count;
              ptr = buffer;
           }

#ifdef _WITH_SEND
           if (send(data_fd, ptr, size, 0) != size)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "send() error : %s", strerror(errno));
              return(errno);
           }
#else
           if (write(data_fd, ptr, size) != size)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "write() error : %s", strerror(errno));
              return(errno);
           }
#endif
#ifdef _FLUSH_FTP_WRITE
           if (fflush(p_data) == EOF)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "Failed to fflush() data buffer : %s",
                        strerror(errno));
              return(errno);
           }
#endif
        }
        else if (status < 0)
             {
                trans_log(ERROR_SIGN, __FILE__, __LINE__,
                          "select() error : %s", strerror(errno));
                return(errno);
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


#ifdef _BLOCK_MODE
/*########################### ftp_block_write() #########################*/
int
ftp_block_write(char *block, unsigned short size, char descriptor)
{
   int    status;
   fd_set wset;

   /* Initialise descriptor set */
   FD_ZERO(&wset);
   FD_SET(data_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = ftp_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(data_fd + 1, NULL, &wset, NULL, &timeout);

   if (status == 0)
   {
      /* timeout has arrived */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (FD_ISSET(data_fd, &wset))
        {
           /* Write descriptor of block header */
           if (write(data_fd, &descriptor, 1) != 1)
           {
                trans_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to write() descriptor of block header : %s",
                          strerror(errno));
                return(INCORRECT);
           }

           /* Write byte counter of block header */
           if (write(data_fd, &size, 2) != 2)
           {
                trans_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to write() byte counter of block header : %s",
                          strerror(errno));
                return(INCORRECT);
           }

           /* Now write the data */
           if (write(data_fd, block, size) != size)
           {
                trans_log(ERROR_SIGN, __FILE__, __LINE__,
                          "write() error : %s", strerror(errno));
                return(INCORRECT);
           }
        }
        else if (status < 0)
             {
                trans_log(ERROR_SIGN, __FILE__, __LINE__,
                          "select() error : %s", strerror(errno));
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


/*############################## ftp_mode() #############################*/
int
ftp_mode(char mode)
{
   int reply;

   (void)fprintf(p_control, "MODE %c\r\n", mode);
   (void)fflush(p_control);

   if ((reply = get_reply(p_control)) < 0)
   {
      return(INCORRECT);
   }

   if (check_reply(2, reply, 200) < 0)
   {
      return(reply);
   }

   return(SUCCESS);
}
#endif /* _BLOCK_MODE */


/*############################## ftp_quit() #############################*/
int
ftp_quit(void)
{
   int reply;

   (void)fprintf(p_control, "QUIT\r\n");
   if (fflush(p_control) == EOF)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__,
                "Failed to fflush() control connection : %s",
                strerror(errno));
   }
   if (p_data != NULL)
   {
      if (fflush(p_data) == EOF)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__,
                   "Failed to fflush() data connection : %s",
                   strerror(errno));
      }
#ifdef _WITH_SHUTDOWN
      if (shutdown(data_fd, 1) < 0)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                   "shutdown() error : %s", strerror(errno));
      }
#endif
      if (fclose(p_data) == EOF)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                   "fclose() error : %s", strerror(errno));
      }
   }

   /*
    * If timeout_flag is ON, lets NOT check the reply from
    * the QUIT command. Else we are again waiting 'ftp_timeout'
    * seconds for the reply!
    */
   if (timeout_flag != ON)
   {
      if ((reply = get_reply(p_control)) < 0)
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
      if (shutdown(fileno(p_control), 1) < 0)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                   "shutdown() error : %s", strerror(errno));
      }
   }
   if (fclose(p_control) == EOF)
   {
      trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                "fclose() error : %s", strerror(errno));
   }

   return(SUCCESS);
}


/*############################ ftp_get_reply() ##########################*/
int
ftp_get_reply(void)
{
   int tmp_timeout_flag = OFF,
       reply;

   if (timeout_flag == ON)
   {
      tmp_timeout_flag = ON;
   }
   reply = get_reply(p_control);
   if ((timeout_flag == ON) && (tmp_timeout_flag == OFF))
   {
      timeout_flag = ON;
   }

   return(reply);
}


/*++++++++++++++++++++++++++++++ get_reply() ++++++++++++++++++++++++++++*/
static int
get_reply(FILE *p_control)
{
   for (;;)
   {
      if (read_msg(p_control) == INCORRECT)
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


/*+++++++++++++++++++++++++++ read_data_line() ++++++++++++++++++++++++++*/
static int
read_data_line(FILE *p_data, char *line)
{
   static int    i;
   int           n,
                 read_fd = fileno(p_data),
                 status;
   unsigned char char_buf;
   fd_set        rset;

   i = 0;

   for (;;)
   {
      /* Initialise descriptor set */
      FD_ZERO(&rset);
      FD_SET(read_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = ftp_timeout;

      /* Wait for message x seconds and then continue. */
      status = select(read_fd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         /* timeout has arrived */
         timeout_flag = ON;
         return(INCORRECT);
      }
      else if (FD_ISSET(read_fd, &rset))
           {
              if ((n = read(read_fd, &char_buf, 1)) != 1)
              {
                 if (n == 0)
                 {
                    /*
                     * Some stupid dump systems like NT sometimes
                     * do not return any data here.
                     */
                    line[i] = '\0';
                    return(i);
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__,
                              "read() error (after reading %d Bytes) : %s",
                              i, strerror(errno));
                    return(INCORRECT);
                 }
              }
              if ((char_buf == '\n') || (char_buf == '\r'))
              {
                 line[i] = '\0';
                 return(i);
              }
              if (i > MAX_RET_MSG_LENGTH)
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__,
                           "Buffer for reading message to small.");
                 return(INCORRECT);
              }
              line[i] = char_buf;
              i++;
           }
           else if (status < 0)
                {
                   trans_log(ERROR_SIGN, __FILE__, __LINE__,
                             "select() error : %s", strerror(errno));
                   exit(INCORRECT);
                }
                else
                {
                   (void)rec(sys_log_fd, ERROR_SIGN,
                             "Unknown condition. (%s %d)\n",
                             __FILE__, __LINE__);
                   exit(INCORRECT);
                }
   } /* for (;;) */
}


/*----------------------------- read_msg() ------------------------------*/
static int
read_msg(FILE *p_control)
{
   int           i = 0,
                 read_fd = fileno(p_control),
                 status;
   unsigned char char_buf;
   fd_set        rset;

   for (;;)
   {
      /* Initialise descriptor set */
      FD_ZERO(&rset);
      FD_SET(read_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = ftp_timeout;

      /* Wait for message x seconds and then continue. */
      status = select(read_fd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         /* timeout has arrived */
         timeout_flag = ON;
         return(INCORRECT);
      }
      else if (FD_ISSET(read_fd, &rset))
           {
              if ((status = read(read_fd, &char_buf, 1)) != 1)
              {
                 if (status == 0)
                 {
                    trans_log(ERROR_SIGN,  __FILE__, __LINE__,
                              "Remote hang up.");
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__,
                              "read() error (after reading %d Bytes) : %s",
                              i, strerror(errno));
                 }
                 return(INCORRECT);
              }
              if ((char_buf == '\n') && (msg_str[i - 1] == '\r'))
              {
                 msg_str[i - 1] = '\0';
                 return(i);
              }
              if (i > MAX_RET_MSG_LENGTH)
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__,
                           "Buffer for reading message to small.");
                 return(INCORRECT);
              }
              msg_str[i] = char_buf;
              i++;
           }
      else if (status < 0)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "select() error : %s", strerror(errno));
              exit(INCORRECT);
           }
           else
           {
              (void)rec(sys_log_fd, ERROR_SIGN,
                        "Unknown condition. (%s %d)\n",
                        __FILE__, __LINE__);
              exit(INCORRECT);
           }
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


/*++++++++++++++++++++++++++++++ sig_handler() ++++++++++++++++++++++++++*/
static void
sig_handler(int signo)
{
   longjmp(env_alrm, 1);
}

/*
 *  ftpcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   int  ftp_list(int type, ...)
 **   int  ftp_data(char *filename, off_t seek, int mode, int type)
 **   int  ftp_close_data(int)
 **   int  ftp_write(char *block, char *buffer, int size)
 **   int  ftp_read(char *block, int blocksize)
 **   int  ftp_quit(void)
 **   int  ftp_get_reply(void)
 **   int  ftp_size(char *filename, off_t *remote_size)
 **   int  ftp_date(char *filenname, char *date)
 **
 ** DESCRIPTION
 **   ftpcmd provides a set of commands to communicate with an FTP
 **   server via BSD sockets.
 **   The procedure to send files to another FTP server is as
 **   follows:
 **          ftp_connect()
 **             |
 **             V
 **     +---------------+ YES
 **     | reply = 230 ? |-----+
 **     +---------------+     |
 **             |             |
 **             V             |
 **          ftp_user()       |
 **             |             |
 **             V             |
 **     +---------------+ YES V
 **     | reply = 230 ? |-----+
 **     +---------------+     |
 **             |             |
 **             V             |
 **          ftp_pass()       |
 **             |             |
 **             +<------------+
 **             |
 **             V
 **          ftp_type() -----> ftp_cd()
 **             |                 |
 **             +<----------------+
 **             |
 **             +------------> ftp_size()
 **             |                 |
 **             +<----------------+
 **             |
 **             V
 **          ftp_data()<----------------------+
 **             |                             |
 **             V                             |
 **          ftp_write()<----+                |
 **             |            |                |
 **             V            |                |
 **      +-------------+  NO |                |
 **      | File done ? |-----+                |
 **      +-------------+                      |
 **             |                             |
 **             V                             |
 **          ftp_close_data() ---> ftp_move() |
 **             |                     |       |
 **             +<--------------------+       |
 **             |                             |
 **             V                             |
 **      +-------------+           YES        |
 **      | Next file ? |----------------------+
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
 **   indicate that the 'transfer_timeout' time has been reached.
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
 **   29.01.2000 H.Kiehl Added function ftp_size().
 **   20.02.2000 H.Kiehl Added passive mode.
 **   02.03.2000 H.Kiehl Use setvbuf() once instead of always fflushing
 **                      each fprintf(). And double the timeout when
 **                      closing the data connection.
 **   01.05.2000 H.Kiehl Buffering read() in read_msg() to reduce number
 **                      of system calls.
 **   08.07.2000 H.Kiehl Cleaned up log output to reduce code size.
 **   06.08.2000 H.Kiehl Expanded ftp_list() to support the NLST command
 **                      and create a buffer for the output from that
 **                      command.
 **   20.08.2000 H.Kiehl Implemented ftp_read() function.
 **                      Implemented ftp_date() function.
 */
DESCR__E_M3


#include <stdio.h>            /* fprintf(), fdopen(), fclose()           */
#include <stdarg.h>           /* va_start(), va_arg(), va_end()          */
#include <string.h>           /* memset(), memcpy(), strcpy()            */
#include <stdlib.h>           /* strtoul()                               */
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
extern long               transfer_timeout;

/* Local global variables */
static int                control_fd,
                          data_fd;
static FILE               *p_control,
                          *p_data = NULL;
static jmp_buf            env_alrm;
static struct sockaddr_in ctrl,
                          data;
static struct timeval     timeout;

/* Local function prototypes */
static int                check_data_socket(int, int, int *),
                          check_reply(int, ...),
                          get_number(char **, char),
                          get_reply(FILE *),
                          read_data_line(int, char *),
                          read_data_to_buffer(int, char ***),
                          read_msg(void);
static void               sig_handler(int);


/*########################## ftp_connect() ##############################*/
int
ftp_connect(char *hostname, int port)
{
   int                     length,
                           reply;
#if defined (_WITH_TOS) && defined (IP_TOS)
   int                     tos;
#endif
   struct sockaddr_in      sin;
   register struct hostent *p_host = NULL;

   msg_str[0] = '\0';
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

   if ((control_fd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "socket() error : %s", strerror(errno));
      return(INCORRECT);
   }

   sin.sin_port = htons((u_short)port);

   if (connect(control_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to connect() to %s : %s",
                hostname, strerror(errno));
      (void)close(control_fd);
      return(INCORRECT);
   }

   length = sizeof(ctrl);
   if (getsockname(control_fd, (struct sockaddr *)&ctrl, &length) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "getsockname() error : %s", strerror(errno));
      (void)close(control_fd);
      return(INCORRECT);
   }

#if defined (_WITH_TOS) && defined (IP_TOS)
   tos = IPTOS_LOWDELAY;
   if (setsockopt(control_fd, IPPROTO_IP, IP_TOS, (char *)&tos,
                  sizeof(tos)) < 0)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__,
                "setsockopt() IP_TOS error : %s", strerror(errno));
   }
#endif

   if ((p_control = fdopen(control_fd, "r+")) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "fdopen() control error : %s", strerror(errno));
      (void)close(control_fd);
      return(INCORRECT);
   }

   if ((reply = get_reply(p_control)) < 0)
   {
      (void)fclose(p_control);
      return(INCORRECT);
   }
   if (check_reply(3, reply, 220, 120) < 0)
   {
      if (reply == 230)
      {
         if (setvbuf(p_control, (char *)NULL, _IOLBF, 0) != 0)
         {
            msg_str[0] = '\0';
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "setvbuf() error : %s", strerror(errno));
            (void)close(control_fd);
            return(INCORRECT);
         }
      }
      else
      {
         (void)fclose(p_control);
      }
      return(reply);
   }
   if (setvbuf(p_control, (char *)NULL, _IOLBF, 0) != 0)
   {
      msg_str[0] = '\0';
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "setvbuf() error : %s", strerror(errno));
      (void)close(control_fd);
      return(INCORRECT);
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
         msg_str[0] = '\0';
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

   if ((reply = get_reply(p_control)) < 0)
   {
      return(INCORRECT);
   }

   if (check_reply(4, reply, 230, 202, 332) < 0)
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

   if ((reply = get_reply(p_control)) < 0)
   {
      return(INCORRECT);
   }

   if (check_reply(3, reply, 350, 200) < 0)
   {
      return(reply);
   }

   (void)fprintf(p_control, "RNTO %s\r\n", to);

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


/*############################## ftp_size() #############################*/
int
ftp_size(char *filename, off_t *remote_size)
{
   int reply;

   (void)fprintf(p_control, "SIZE %s\r\n", filename);

   if ((reply = get_reply(p_control)) != INCORRECT)
   {
      if (check_reply(2, reply, 213) == SUCCESS)
      {
         char *ptr = &msg_str[3];

         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         if ((*remote_size = strtoul(ptr, NULL, 10)) != ULONG_MAX)
         {
            return(SUCCESS);
         }
         else
         {
            *remote_size = 0;
         }
      }
      else
      {
         return(reply);
      }
   }

   return(INCORRECT);
}


/*############################## ftp_date() #############################*/
int
ftp_date(char *filename, char *date)
{
   int reply;

   (void)fprintf(p_control, "MDTM %s\r\n", filename);

   if ((reply = get_reply(p_control)) != INCORRECT)
   {
      if (check_reply(2, reply, 213) == SUCCESS)
      {
         int  i = 0;
         char *ptr = &msg_str[3];

         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while ((*ptr != '\0') && (i < MAX_FTP_DATE_LENGTH))
         {
            date[i] = *ptr;
            ptr++; i++;
         }
         date[i] = '\0';
      }
      else
      {
         return(reply);
      }
   }
   else
   {
      return(INCORRECT);
   }

   return(SUCCESS);
}


/*############################## ftp_list() #############################*/
int
ftp_list(int type, ...)
{
   int                sock_fd,
                      new_sock_fd,
                      on = 1,
                      length,
                      reply;
   register char      *h,
                      *p;
   char               **buffer,
                      *filename,
                      *msg;
   va_list            ap;
   struct sockaddr_in from;

   va_start(ap, type);
   if (type & BUFFERED_LIST)
   {
      buffer = va_arg(ap, char **);
   }
   else
   {
      filename = va_arg(ap, char *);
      msg = va_arg(ap, char *);
   }
   va_end(ap);

   data = ctrl;
   data.sin_port = htons((u_short)0);
   msg_str[0] = '\0';

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

   if (type & NLIST_CMD)
   {
      (void)fprintf(p_control, "NLST\r\n");
   }
   else
   {
      (void)fprintf(p_control, "LIST %s\r\n", filename);
   }

   if ((reply = get_reply(p_control)) < 0)
   {
      return(INCORRECT);
   }

   if (check_reply(3, reply, 150, 125) < 0)
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
      msg_str[0] = '\0';
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to set signal handler : %s", strerror(errno));
      (void)close(sock_fd);
      return(INCORRECT);
   }
   if (setjmp(env_alrm) != 0)
   {
      msg_str[0] = '\0';
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "accept() timeout");
      timeout_flag = ON;
      (void)close(sock_fd);
      return(INCORRECT);
   }
   (void)alarm(2 * transfer_timeout);

   if ((new_sock_fd = accept(sock_fd, (struct sockaddr *) &from, &length)) < 0)
   {
      (void)alarm(0); /* Maybe it was a real accept() error */
      msg_str[0] = '\0';
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "accept() error : %s", strerror(errno));
      (void)close(sock_fd);
      return(INCORRECT);
   }
   (void)alarm(0);
   if (close(sock_fd) == -1)
   {
      msg_str[0] = '\0';
      trans_log(DEBUG_SIGN, __FILE__, __LINE__, "close() error : %s",
                 strerror(errno));
   }

   if ((p_data = fdopen(new_sock_fd, "r")) == NULL)
   {
      msg_str[0] = '\0';
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "fdopen() data error : %s", strerror(errno));
      (void)close(new_sock_fd);
      return(INCORRECT);
   }

   if (type & BUFFERED_LIST)
   {
      if (read_data_to_buffer(new_sock_fd, &buffer) < 0)
      {
         return(INCORRECT);
      }
   }
   else
   {
      if (read_data_line(new_sock_fd, msg) < 0)
      {
         return(INCORRECT);
      }
   }

#ifdef _WITH_SHUTDOWN
   if (shutdown(new_sock_fd, 1) < 0)
   {
      msg_str[0] = '\0';
      trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                "shutdown() error : %s", strerror(errno));
   }
#endif

   if (fclose(p_data) == EOF)
   {
      msg_str[0] = '\0';
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "fclose() error : %s", strerror(errno));
      return(INCORRECT);
   }
   p_data = NULL;

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
ftp_data(char *filename, off_t seek, int mode, int type)
{
   int           new_sock_fd,
                 on = 1,
                 length,
                 reply;
#if defined (_WITH_TOS) && defined (IP_TOS)
   int           tos;
#endif
   char          cmd[5];
#ifdef FTX
   struct linger l;
#endif

   if (type == DATA_WRITE)
   {
      if (seek == 0)
      {
         cmd[0] = 'S'; cmd[1] = 'T'; cmd[2] = 'O'; cmd[3] = 'R';
      }
      else
      {
         cmd[0] = 'A'; cmd[1] = 'P'; cmd[2] = 'P'; cmd[3] = 'E';
      }
   }
   else
   {
      cmd[0] = 'R'; cmd[1] = 'E';
      if (seek == 0)
      {
         cmd[2] = 'T'; cmd[3] = 'R';
      }
      else
      {
         cmd[2] = 'S'; cmd[3] = 'T';
      }
   }
   cmd[4] = '\0';

   if (mode == PASSIVE_MODE)
   {
      char *ptr;

      (void)fprintf(p_control, "PASV\r\n");

      if ((reply = get_reply(p_control)) < 0)
      {
         return(INCORRECT);
      }

      if (check_reply(2, reply, 227) < 0)
      {
         return(reply);
      }
      ptr = &msg_str[3];
      do
      {
         ptr++;
      } while ((*ptr != '(') && (*ptr != '\0'));
      if (*ptr == '(')
      {
         int number;

         data = ctrl;
         msg_str[0] = '\0';
         if ((number = get_number(&ptr, ',')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_addr) = number;
         if ((number = get_number(&ptr, ',')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_addr + 1) = number;
         if ((number = get_number(&ptr, ',')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_addr + 2) = number;
         if ((number = get_number(&ptr, ',')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_addr + 3) = number;
         if ((number = get_number(&ptr, ',')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_port) = number;
         if ((number = get_number(&ptr, ')')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_port + 1) = number;

         if ((new_sock_fd = socket(data.sin_family, SOCK_STREAM,
                                   IPPROTO_TCP)) < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "socket() error : %s", strerror(errno));
            return(INCORRECT);
         }

         if (setsockopt(new_sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                        sizeof(on)) < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "setsockopt() error : %s", strerror(errno));
            (void)close(new_sock_fd);
            return(INCORRECT);
         }
         if (connect(new_sock_fd, (struct sockaddr *) &data, sizeof(data)) < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to connect() to : %s", strerror(errno));
            (void)close(new_sock_fd);
            return(INCORRECT);
         }

         (void)fprintf(p_control, "%s %s\r\n", cmd, filename);

         if ((reply = get_reply(p_control)) < 0)
         {
            if (timeout_flag != ON)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__,
                         "Failed to get proper reply (%d).", reply);
            }
            (void)close(new_sock_fd);
            return(INCORRECT);
         }

         if (check_reply(3, reply, 150, 125) < 0)
         {
            (void)close(new_sock_fd);
            return(reply);
         }
      }
   }
   else /* ACTIVE_MODE */
   {
      int                retries = 0,
                         ret,
                         sock_fd;
      register char      *h, *p;
      struct sockaddr_in from;

      do
      {
         data = ctrl;
         data.sin_port = htons((u_short)0);
         msg_str[0] = '\0';

         if ((sock_fd = socket(data.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "socket() error : %s", strerror(errno));
            return(INCORRECT);
         }

         if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                        sizeof(on)) < 0)
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

         (void)fprintf(p_control, "%s %s\r\n", cmd, filename);

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
         msg_str[0] = '\0';
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to set signal handler : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }
      if (setjmp(env_alrm) != 0)
      {
         msg_str[0] = '\0';
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "accept() timeout");
         timeout_flag = ON;
         (void)close(sock_fd);
         return(INCORRECT);
      }
      (void)alarm(2 * transfer_timeout);

      if ((new_sock_fd = accept(sock_fd, (struct sockaddr *) &from,
                                &length)) < 0)
      {
         (void)alarm(0); /* Maybe it was a real accept() error */
         msg_str[0] = '\0';
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "accept() error : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }
      (void)alarm(0);
      if (close(sock_fd) == -1)
      {
         msg_str[0] = '\0';
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, "close() error : %s",
                   strerror(errno));
      }
   } /* ACTIVE_MODE */

#ifdef FTX
   l.l_onoff = 1; l.l_linger = 240;
   if (setsockopt(new_sock_fd, SOL_SOCKET, SO_LINGER,
                  (char *)&l, sizeof(struct linger)) < 0)
   {
      msg_str[0] = '\0';
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "setsockopt() error : %s",
                strerror(errno));
      (void)close(new_sock_fd);
      return(INCORRECT);
   }
#endif

#if defined (_WITH_TOS) && defined (IP_TOS)
   tos = IPTOS_THROUGHPUT;
   if (setsockopt(new_sock_fd, IPPROTO_IP, IP_TOS, (char *)&tos,
                  sizeof(tos)) < 0)
   {
      msg_str[0] = '\0';
      trans_log(WARN_SIGN, __FILE__, __LINE__,
                "setsockopt() IP_TOS error : %s", strerror(errno));
   }
#endif

   data_fd = new_sock_fd;
   if ((p_data = fdopen(new_sock_fd, "r+")) == NULL)
   {
      msg_str[0] = '\0';
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
   if (check_reply(6, reply, 150, 125, 120, 250, 200) < 0)
   {
      if (close(sock_fd) == -1)
      {
         msg_str[0] = '\0';
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, "close() error : %s",
                   strerror(errno));
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
ftp_close_data(int type)
{
   int reply;

   if (type == DATA_WRITE)
   {
      if (fflush(p_data) == EOF)
      {
         msg_str[0] = '\0';
         trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                   "Failed to fflush() data : %s", strerror(errno));
         (void)fclose(p_data);
         p_data = NULL;
         return(INCORRECT);
      }
   }

#ifdef _WITH_SHUTDOWN
   if (shutdown(data_fd, 1) < 0)
   {
      msg_str[0] = '\0';
      trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                "shutdown() error : %s", strerror(errno));
   }
#endif

   if (fclose(p_data) == EOF)
   {
      msg_str[0] = '\0';
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "fclose() error : %s", strerror(errno));
      return(INCORRECT);
   }
   p_data = NULL;

   if (timeout_flag != ON)
   {
      long tmp_ftp_timeout = transfer_timeout;

      /*
       * Since there are so many timeouts on slow lines when closing
       * the data connection lets double the timeout and see if this
       * helps.
       */
      transfer_timeout += transfer_timeout;
      if ((reply = get_reply(p_control)) < 0)
      {
         transfer_timeout = tmp_ftp_timeout;
         return(INCORRECT);
      }
      transfer_timeout = tmp_ftp_timeout;
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
   timeout.tv_sec = transfer_timeout;

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
           if ((status = send(data_fd, ptr, size, 0)) != size)
           {
              msg_str[0] = '\0';
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "send() error (%d) : %s", status, strerror(errno));
              return(errno);
           }
#else
           if ((status = write(data_fd, ptr, size)) != size)
           {
              msg_str[0] = '\0';
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "write() error (%d) : %s", status, strerror(errno));
              return(errno);
           }
#endif
#ifdef _FLUSH_FTP_WRITE
           if (fflush(p_data) == EOF)
           {
              msg_str[0] = '\0';
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "Failed to fflush() data buffer : %s",
                        strerror(errno));
              return(INCORRECT);
           }
#endif
        }
   else if (status < 0)
        {
           msg_str[0] = '\0';
           trans_log(ERROR_SIGN, __FILE__, __LINE__,
                     "select() error : %s", strerror(errno));
           return(INCORRECT);
        }
        else
        {
           msg_str[0] = '\0';
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "Unknown condition.");
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
   timeout.tv_sec = transfer_timeout;

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
              msg_str[0] = '\0';
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "Failed to write() descriptor of block header : %s",
                        strerror(errno));
              return(INCORRECT);
           }

           /* Write byte counter of block header */
           if (write(data_fd, &size, 2) != 2)
           {
              msg_str[0] = '\0';
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "Failed to write() byte counter of block header : %s",
                        strerror(errno));
              return(INCORRECT);
           }

           /* Now write the data */
           if (write(data_fd, block, size) != size)
           {
              msg_str[0] = '\0';
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "write() error : %s", strerror(errno));
              return(INCORRECT);
           }
        }
        else if (status < 0)
             {
                msg_str[0] = '\0';
                trans_log(ERROR_SIGN, __FILE__, __LINE__,
                          "select() error : %s", strerror(errno));
                exit(INCORRECT);
             }
             else
             {
                msg_str[0] = '\0';
                trans_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Unknown condition.");
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


/*############################## ftp_read() #############################*/
int
ftp_read(char *block, int blocksize)
{
   int    bytes_read,
          status;
   fd_set rset;

   /* Initialise descriptor set */
   FD_ZERO(&rset);
   FD_SET(data_fd, &rset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(data_fd + 1, &rset, NULL, NULL, &timeout);

   if (status == 0)
   {
      /* timeout has arrived */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (FD_ISSET(data_fd, &rset))
        {
           if ((bytes_read = read(data_fd, block, blocksize)) == -1)
           {
              msg_str[0] = '\0';
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "read() error : %s", strerror(errno));
              return(INCORRECT);
           }
        }
   else if (status < 0)
        {
           msg_str[0] = '\0';
           trans_log(ERROR_SIGN, __FILE__, __LINE__,
                     "select() error : %s", strerror(errno));
           exit(INCORRECT);
        }
        else
        {
           msg_str[0] = '\0';
           trans_log(ERROR_SIGN, __FILE__, __LINE__,
                     "Unknown condition.");
           exit(INCORRECT);
        }

   return(bytes_read);
}


/*############################## ftp_quit() #############################*/
int
ftp_quit(void)
{
   int reply;

   (void)fprintf(p_control, "QUIT\r\n");
   if (p_data != NULL)
   {
      if (fflush(p_data) == EOF)
      {
         msg_str[0] = '\0';
         trans_log(WARN_SIGN, __FILE__, __LINE__,
                   "Failed to fflush() data connection : %s",
                   strerror(errno));
      }
#ifdef _WITH_SHUTDOWN
      if (shutdown(data_fd, 1) < 0)
      {
         msg_str[0] = '\0';
         trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                   "shutdown() error : %s", strerror(errno));
      }
#endif /* _WITH_SHUTDOWN */
      if (fclose(p_data) == EOF)
      {
         msg_str[0] = '\0';
         trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                   "fclose() error : %s", strerror(errno));
      }
   }

   /*
    * If timeout_flag is ON, lets NOT check the reply from
    * the QUIT command. Else we are again waiting 'transfer_timeout'
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
#ifdef _WITH_SHUTDOWN
      if (shutdown(control_fd, 1) < 0)
      {
         msg_str[0] = '\0';
         trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                   "shutdown() error : %s", strerror(errno));
      }
#endif /* _WITH_SHUTDOWN */
   }
   if (fclose(p_control) == EOF)
   {
      msg_str[0] = '\0';
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


/*+++++++++++++++++++++++++++ read_data_line() ++++++++++++++++++++++++++*/
static int
read_data_line(int read_fd, char *line)
{
   int  bytes_buffered = 0,
        bytes_read = 0;
   char *read_ptr = line;

   for (;;)
   {
      if (bytes_read <= 0)
      {
         int    status;
         fd_set rset;

         /* Initialise descriptor set */
         FD_ZERO(&rset);
         FD_SET(read_fd, &rset);
         timeout.tv_usec = 0L;
         timeout.tv_sec = transfer_timeout;

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
                 if ((bytes_read = read(read_fd, &line[bytes_buffered],
                                        (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                 {
                    if (bytes_read == 0)
                    {
                       /*
                        * Due to security reasons some systems do not
                        * return any data here. So lets not count this
                        * as a remote hangup.
                        */
                       line[bytes_buffered] = '\0';
                       return(0);
                    }
                    else
                    {
                       msg_str[0] = '\0';
                       trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                 "read() error (after reading %d Bytes) : %s",
                                 bytes_buffered, strerror(errno));
                       return(INCORRECT);
                    }
                 }
                 read_ptr = &line[bytes_buffered];
                 bytes_buffered += bytes_read;
              }
         else if (status < 0)
              {
                 msg_str[0] = '\0';
                 trans_log(ERROR_SIGN, __FILE__, __LINE__,
                           "select() error : %s", strerror(errno));
                 exit(INCORRECT);
              }
              else
              {
                 msg_str[0] = '\0';
                 trans_log(ERROR_SIGN, __FILE__, __LINE__,
                           "Unknown condition.");
                 exit(INCORRECT);
              }
      }

      /* Evaluate what we have read. */
      do
      {
         if ((*read_ptr == '\n') && (*(read_ptr - 1) == '\r'))
         {
            *(read_ptr - 2) = '\0';
            return(bytes_buffered);
         }
         read_ptr++;
         bytes_read--;
      } while(bytes_read > 0);
   } /* for (;;) */
}


/*+++++++++++++++++++++++++ read_data_to_buffer() +++++++++++++++++++++++*/
static int
read_data_to_buffer(int read_fd, char ***buffer)
{
   int  bytes_buffered = 0,
        bytes_read = 0;
   char tmp_buffer[MAX_RET_MSG_LENGTH];

   for (;;)
   {
      int    status;
      fd_set rset;

      /* Initialise descriptor set */
      FD_ZERO(&rset);
      FD_SET(read_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

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
              if ((bytes_read = read(read_fd, tmp_buffer,
                                     MAX_RET_MSG_LENGTH)) < 1)
              {
                 if (bytes_read == -1)
                 {
                    msg_str[0] = '\0';
                    trans_log(ERROR_SIGN, __FILE__, __LINE__,
                              "read() error (after reading %d Bytes) : %s",
                              bytes_buffered, strerror(errno));
                    return(INCORRECT);
                 }
                 else
                 {
                    if (bytes_buffered > 0)
                    {
                       *(**buffer + bytes_buffered) = '\0';
                    }
                    return(bytes_buffered);
                 }
              }
              if (**buffer == NULL)
              {
                 if ((**buffer = malloc(bytes_read + 1)) == NULL)
                 {
                    msg_str[0] = '\0';
                    trans_log(ERROR_SIGN, __FILE__, __LINE__,
                              "malloc() error : %s", strerror(errno));
                    return(INCORRECT);
                 }
              }
              else
              {
                 if ((**buffer = realloc(**buffer,
                                         bytes_buffered + bytes_read + 1)) == NULL)
                 {
                    msg_str[0] = '\0';
                    trans_log(ERROR_SIGN, __FILE__, __LINE__,
                              "realloc() error (after reading %d Bytes) : %s",
                              bytes_buffered, strerror(errno));
                    return(INCORRECT);
                 }
              }
              (void)memcpy(**buffer + bytes_buffered, tmp_buffer, bytes_read);
              bytes_buffered += bytes_read;
           }
      else if (status < 0)
           {
              msg_str[0] = '\0';
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "select() error : %s", strerror(errno));
              exit(INCORRECT);
           }
           else
           {
              msg_str[0] = '\0';
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "Unknown condition.");
              exit(INCORRECT);
           }
   } /* for (;;) */
}


/*----------------------------- read_msg() ------------------------------*/
static int
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
      (void)memmove(msg_str, read_ptr + 1, bytes_read);
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
         FD_SET(control_fd, &rset);
         timeout.tv_usec = 0L;
         timeout.tv_sec = transfer_timeout;

         /* Wait for message x seconds and then continue. */
         status = select(control_fd + 1, &rset, NULL, NULL, &timeout);

         if (status == 0)
         {
            /* timeout has arrived */
            timeout_flag = ON;
            bytes_read = 0;
            return(INCORRECT);
         }
         else if (FD_ISSET(control_fd, &rset))
              {
                 if ((bytes_read = read(control_fd, &msg_str[bytes_buffered],
                                        (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                 {
                    msg_str[0] = '\0';
                    if (bytes_read == 0)
                    {
                       trans_log(ERROR_SIGN,  __FILE__, __LINE__,
                                 "Remote hang up.");
                    }
                    else
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                 "read() error (after reading %d Bytes) : %s",
                                 bytes_buffered, strerror(errno));
                       bytes_read = 0;
                    }
                    return(INCORRECT);
                 }
                 read_ptr = &msg_str[bytes_buffered];
                 bytes_buffered += bytes_read;
              }
         else if (status < 0)
              {
                 msg_str[0] = '\0';
                 trans_log(ERROR_SIGN, __FILE__, __LINE__,
                           "select() error : %s", strerror(errno));
                 exit(INCORRECT);
              }
              else
              {
                 msg_str[0] = '\0';
                 trans_log(ERROR_SIGN, __FILE__, __LINE__,
                           "Unknown condition.");
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
   int     i = 1,
           reply;
   va_list ap;

   va_start(ap, count);
   reply = va_arg(ap, int);
   do
   {
      if (reply == va_arg(ap, int))
      {
         va_end(ap);
         return(SUCCESS);
      }
      i++;
   } while (i < count);
   va_end(ap);

   return(INCORRECT);
}


/*++++++++++++++++++++++++++++ get_number() +++++++++++++++++++++++++++++*/
static int
get_number(char **ptr, char end_char)
{
   int length = 0,
       number = INCORRECT;

   (*ptr)++;
   do
   {
      if (isdigit(**ptr))
      {
         length++; (*ptr)++;
      }
      else
      {
         return(INCORRECT);
      }
   } while ((**ptr != end_char) && (length < 4));
   if ((length > 0) && (length < 4) && (**ptr == end_char))
   {
      number = *(*ptr - 1) - '0';
      if (length == 2)
      {
         number += (*(*ptr - 2) - '0') * 10;
      }
      else if (length == 3)
           {
              number += (*(*ptr - 3) - '0') * 100 + (*(*ptr - 2) - '0') * 10;
           }
   }

   return(number);
}


/*++++++++++++++++++++++++++++++ sig_handler() ++++++++++++++++++++++++++*/
static void
sig_handler(int signo)
{
   longjmp(env_alrm, 1);
}

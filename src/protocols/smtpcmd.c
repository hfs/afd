/*
 *  smtpcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   smtpcmd - commands to send data via SMTP
 **
 ** SYNOPSIS
 **   int smtp_connect(char *hostname, int port)
 **   int smtp_user(char *user)
 **   int smtp_rcpt(char *recipient)
 **   int smtp_open(void)
 **   int smtp_write(char *block, char *buffer, int size)
 **   int smtp_write_iso8859(char *block, char *buffer, int size)
 **   int smtp_close(void)
 **   int smtp_quit(void)
 **   void get_content_type(char *filename, char *content_type)
 **
 ** DESCRIPTION
 **   smtpcmd provides a set of commands to communicate with an SMTP
 **   server via BSD sockets.
 **   The procedure to send files to another SMTP server is as
 **   follows:
 **            smtp_connect()
 **               |
 **               V
 **            smtp_helo()
 **               |
 **               V
 **               +<----------------------------+
 **               |                             |
 **               V                             |
 **            smtp_user()                      |
 **               |                             |
 **               V                             |
 **            smtp_rcpt()<---------+           |
 **               |                 |           |
 **               V                 |           |
 **               +-----------------+           |
 **               |                             |
 **               V                             |
 **            smtp_open()                      |
 **               |                             |
 **               V                             |
 **   smtp_write()/smtp_write_iso8859()<----+   |
 **               |                         |   |
 **               V                         |   |
 **        +-------------+   NO             |   |
 **        | File done ? |------------------+   |
 **        +-------------+                      |
 **               |                             |
 **               V                             |
 **            smtp_close()                     |
 **               |                             |
 **               V                             |
 **        +--------------+      YES            |
 **        | More files ? |---------------------+
 **        +--------------+
 **               |
 **               V
 **            smtp_quit()
 **
 **
 ** RETURN VALUES
 **   Returns SUCCESS when successful. When an error has occurred
 **   it will either return INCORRECT or the three digit SMTP reply
 **   code when the reply of the server does not conform to the
 **   one expected. The complete reply string of the SMTP server
 **   is returned to 'msg_str'. 'timeout_flag' is just a flag to
 **   indicate that the 'transfer_timeout' time has been reached.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.12.1996 H.Kiehl Created
 **   23.07.1999 H.Kiehl Added smtp_write_iso8859()
 **   08.07.2000 H.Kiehl Cleaned up log output to reduce code size.
 **   05.08.2000 H.Kiehl Buffering read() in read_msg() to reduce number
 **                      of system calls.
 **   21.05.2001 H.Kiehl Failed to fclose() connection when an error
 **                      occured in smtp_quit().
 **   25.12.2003 H.Kiehl Added traceing.
 **   10.01.2004 H.Kiehl Remove function check_reply().
 **   27.09.2006 H.Kiehl Function smtp_write() now handles the case
 **                      correctly when a line starts with a leading
 **                      dot '.'.
 **
 */
DESCR__E_M3


#include <stdio.h>            /* fprintf(), fdopen(), fclose()           */
#include <string.h>           /* memset(), memcpy(), strcpy()            */
#include <stdlib.h>           /* atoi()                                  */
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
#include "fddefs.h"
#include "smtpdefs.h"


/* External global variables */
extern int                timeout_flag;
extern char               msg_str[],
                          tr_hostname[];
#ifdef LINUX
extern char               *h_errlist[];    /* for gethostbyname()        */
extern int                h_nerr;          /* for gethostbyname()        */
#endif
extern long               transfer_timeout;
extern struct job         db;

/* Local global variables */
static int                smtp_fd;
static FILE               *smtp_fp;
static struct sockaddr_in ctrl;
static struct timeval     timeout;

/* Local functions */
static int                get_reply(FILE *),
                          read_msg(void);


/*########################## smtp_connect() #############################*/
int
smtp_connect(char *hostname, int port)
{
   int                     reply;
   my_socklen_t            length;
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

   if ((smtp_fd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "socket() error : %s", strerror(errno));
      return(INCORRECT);
   }

   sin.sin_port = htons((u_short)port);

   if (connect(smtp_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "Failed to connect() to %s : %s", hostname, strerror(errno));
      (void)close(smtp_fd);
      return(INCORRECT);
   }

   length = sizeof(ctrl);
   if (getsockname(smtp_fd, (struct sockaddr *)&ctrl, &length) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "getsockname() error : %s", strerror(errno));
      (void)close(smtp_fd);
      return(INCORRECT);
   }

   if ((smtp_fp = fdopen(smtp_fd, "r+")) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "fdopen() control error : %s", strerror(errno));
      (void)close(smtp_fd);
      return(INCORRECT);
   }

   if ((reply = get_reply(smtp_fp)) < 0)
   {
      (void)fclose(smtp_fp);
      return(INCORRECT);
   }
   if (reply != 220)
   {
      (void)fclose(smtp_fp);
      return(reply);
   }

   return(SUCCESS);
}


/*############################# smtp_helo() #############################*/
int
smtp_helo(char *host_name)
{
   int reply;

#ifdef WITH_TRACE
   trace_log(NULL, 0, W_TRACE, NULL, 0, "HELO %s<0D><0A>", host_name);
#endif
   (void)fprintf(smtp_fp, "HELO %s\r\n", host_name);
   (void)fflush(smtp_fp);

   if ((reply = get_reply(smtp_fp)) != INCORRECT)
   {
      if (reply == 250)
      {
         reply = SUCCESS;
      }
   }

   return(reply);
}


/*############################# smtp_user() #############################*/
int
smtp_user(char *user)
{
   int reply;

#ifdef WITH_TRACE
   trace_log(NULL, 0, W_TRACE, NULL, 0, "MAIL FROM:%s<0D><0A>", user);
#endif
   (void)fprintf(smtp_fp, "MAIL FROM:%s\r\n", user);
   (void)fflush(smtp_fp);

   if ((reply = get_reply(smtp_fp)) != INCORRECT)
   {
      if (reply == 250)
      {
         reply = SUCCESS;
      }
   }

   return(reply);
}


/*############################# smtp_rcpt() #############################*/
int
smtp_rcpt(char *recipient)
{
   int reply;

#ifdef WITH_TRACE
   trace_log(NULL, 0, W_TRACE, NULL, 0, "RCPT TO:%s<0D><0A>", recipient);
#endif
   (void)fprintf(smtp_fp, "RCPT TO:%s\r\n", recipient);
   (void)fflush(smtp_fp);

   if ((reply = get_reply(smtp_fp)) != INCORRECT)
   {
      if ((reply == 250) || (reply == 251))
      {
         reply = SUCCESS;
      }
   }

   return(reply);
}


/*############################# smtp_open() #############################*/
int
smtp_open(void)
{
   int reply;

#ifdef WITH_TRACE
   trace_log(NULL, 0, W_TRACE, NULL, 0, "DATA<0D><0A>");
#endif
   (void)fprintf(smtp_fp, "DATA\r\n");
   (void)fflush(smtp_fp);

   if ((reply = get_reply(smtp_fp)) != INCORRECT)
   {
      if (reply == 354)
      {
         reply = SUCCESS;
      }
   }

   return(reply);
}


/*############################# smtp_write() ############################*/
/*                              ------------                             */
/* If buffer is not NULL, this function converts a '\n' only to a '\r'   */
/* '\n' and adds another dot '.' if a dot is at the beginning of the     */
/* line. Note that this works correctly the caller needs to allocate at  */
/* least twice the amount of memory as that specified in size. This      */
/* function does NOT check this. Also note that the first byte in buffer */
/* will always contain the last character of the previous block. So when */
/* function smtp_write() is used to send the mail body, buffer[0] must   */
/* be initialized by the caller to '\n' only for the first block send.   */
/*#######################################################################*/
int
smtp_write(char *block, char *buffer, int size)
{
   int    status;
   char   *ptr = block;
   fd_set wset;

   /* Initialise descriptor set */
   FD_ZERO(&wset);
   FD_SET(smtp_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(smtp_fd + 1, NULL, &wset, NULL, &timeout);

   if (status == 0)
   {
      /* timeout has arrived */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (FD_ISSET(smtp_fd, &wset))
        {
           if (buffer != NULL)
           {
              register int i,
                           count = 1;

              for (i = 0; i < size; i++)
              {
                 if (*ptr == '\n')
                 {
                    if (((i > 0) && (*(ptr - 1) == '\r')) ||
                        ((i == 0) && (buffer[0] == '\r')))
                    {
                       buffer[count++] = *ptr;
                    }
                    else
                    {
                       buffer[count++] = '\r';
                       buffer[count++] = '\n';
                    }
                 }
                 else if ((*ptr == '.') &&
                          (((i > 0) && (*(ptr - 1) == '\n')) ||
                           ((i == 0) && (buffer[0] == '\n'))))
                      {
                         buffer[count++] = '.';
                         buffer[count++] = '.';
                      }
                      else
                      {
                         buffer[count++] = *ptr;
                      }
                 ptr++;
              }
              if (i > 0)
              {
                 buffer[0] = *(ptr - 1);
                 size = count - 1;
              }
              else
              {
                 size = count;
              }
              ptr = buffer + 1;
           }

#ifdef _WITH_SEND
           if ((status = send(smtp_fd, ptr, size, 0)) != size)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "send() error after writting %d bytes : %s",
                        status, strerror(errno));
              return(INCORRECT);
           }
#else
           if ((status = write(smtp_fd, ptr, size)) != size)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "write() error after writting %d bytes : %s",
                        status, strerror(errno));
              return(INCORRECT);
           }
#endif
#ifdef WITH_TRACE
           trace_log(NULL, 0, BIN_W_TRACE, ptr, size, NULL);
#endif
        }
   else if (status < 0)
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "select() error : %s", strerror(errno));
           exit(INCORRECT);
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "Unknown condition.");
           exit(INCORRECT);
        }
   
   return(SUCCESS);
}


/*######################## smtp_write_iso8859() #########################*/
int
smtp_write_iso8859(char *block, char *buffer, int size)
{
   register int  i,
                 count = 1;
   int           status;
   unsigned char *ptr = (unsigned char *)block;
   fd_set        wset;

   /* Initialise descriptor set */
   FD_ZERO(&wset);
   FD_SET(smtp_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(smtp_fd + 1, NULL, &wset, NULL, &timeout);

   if (status == 0)
   {
      /* timeout has arrived */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (FD_ISSET(smtp_fd, &wset))
        {
           for (i = 0; i < size; i++)
           {
              if (*ptr == '\n')
              {
                 if (((i > 0) && (*(ptr - 1) == '\r')) ||
                     ((i == 0) && (buffer[0] == '\r')))
                 {
                    buffer[count++] = *ptr;
                 }
                 else
                 {
                    buffer[count++] = '\r';
                    buffer[count++] = '\n';
                 }
              }
              else if ((*ptr == '.') &&
                       (((i > 0) && (*(ptr - 1) == '\n')) ||
                        ((i == 0) && (buffer[0] == '\n'))))
                   {
                      buffer[count++] = '.';
                      buffer[count++] = '.';
                   }
                   else
                   {
                      if (*ptr == 129) /* ue */
                      {
                         buffer[count] = (char)(252);
                      }
                      else if (*ptr == 132) /* ae */
                           {
                              buffer[count] = (char)(228);
                           }
                      else if (*ptr == 142) /* AE */
                           {
                              buffer[count] = (char)(196);
                           }
                      else if (*ptr == 148) /* oe */
                           {
                              buffer[count] = (char)(246);
                           }
                      else if (*ptr == 153) /* OE */
                           {
                              buffer[count] = (char)(214);
                           }
                      else if (*ptr == 154) /* UE */
                           {
                              buffer[count] = (char)(220);
                           }
                      else if (*ptr == 225) /* ss */
                           {
                              buffer[count] = (char)(223);
                           }
                           else
                           {
                              buffer[count] = *ptr;
                           }
                      count++;
                   }
              ptr++;
           }
           if (i > 0)
           {
              buffer[0] = *(ptr - 1);
              size = count - 1;
           }
           else
           {
              size = count;
           }
           ptr = (unsigned char *)buffer + 1;

#ifdef _WITH_SEND
           if ((status = send(smtp_fd, ptr, size, 0)) != size)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "send() error after writting %d bytes : %s",
                        status, strerror(errno));
              return(INCORRECT);
           }
#else
           if ((status = write(smtp_fd, ptr, size)) != size)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "write() error after writting %d bytes : %s",
                        status, strerror(errno));
              return(INCORRECT);
           }
#endif
#ifdef WITH_TRACE
           trace_log(NULL, 0, BIN_W_TRACE, (char *)ptr, size, NULL);
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
                exit(INCORRECT);
             }
   
   return(SUCCESS);
}


/*############################ smtp_close() #############################*/
int
smtp_close(void)
{
   int reply;

#ifdef WITH_TRACE
   trace_log(NULL, 0, W_TRACE, NULL, 0, "<0D><0A>.<0D><0A>");
#endif
   (void)fprintf(smtp_fp, "\r\n.\r\n");
   if (fflush(smtp_fp) == EOF)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                "Failed to fflush() data when closing connection : %s",
                strerror(errno));
   }

   if (timeout_flag == OFF)
   {
      if ((reply = get_reply(smtp_fp)) < 0)
      {
         return(INCORRECT);
      }
      if (reply != 250)
      {
         return(reply);
      }
   }

   return(SUCCESS);
}


/*############################# smtp_quit() #############################*/
int
smtp_quit(void)
{
   int reply;

#ifdef WITH_TRACE
   trace_log(NULL, 0, W_TRACE, NULL, 0, "QUIT<0D><0A>");
#endif
   (void)fprintf(smtp_fp, "QUIT\r\n");
   if (fflush(smtp_fp) == EOF)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                "Failed to fflush() data when closing connection : %s",
                strerror(errno));
   }

   if ((timeout_flag != ON) && (timeout_flag != CON_RESET))
   {
      if ((reply = get_reply(smtp_fp)) < 0)
      {
         (void)fclose(smtp_fp);
         return(INCORRECT);
      }
      if (reply != 221)
      {
         (void)fclose(smtp_fp);
         return(reply);
      }

#ifdef _WITH_SHUTDOWN
      if (smtp_fp != NULL)
      {
         if (shutdown(smtp_fd, 1) < 0)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                      "shutdown() error : %s", strerror(errno));
         }
      }
#endif
   }
   if (fclose(smtp_fp) == EOF)
   {
      trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                "fclose() error : %s", strerror(errno));
   }

   return(SUCCESS);
}


/*######################### get_content_type() ##########################*/
void
get_content_type(char *filename, char *content_type)
{
   content_type[0] = '\0';
   if (filename != NULL)
   {
      char *ptr;

      ptr = filename + strlen(filename);
      while ((ptr > filename) && (*ptr != '.'))
      {
         ptr--;
      }
      if ((*ptr == '.') && (*(ptr + 1) != '\0'))
      {
         ptr++;
         if (((*ptr == 'p') || (*ptr == 'P')) &&
             ((*(ptr + 1) == 'n') || (*(ptr + 1) == 'N')) &&
             ((*(ptr + 2) == 'g') || (*(ptr + 2) == 'G')) &&
             (*(ptr + 3) == '\0'))
         {
            (void)strcpy(content_type, "IMAGE/png");
         }
         else if (((*ptr == 'j') || (*ptr == 'J')) &&
                  ((((*(ptr + 1) == 'p') || (*(ptr + 1) == 'P')) &&
                    (((*(ptr + 2) == 'g') || (*(ptr + 2) == 'G')) ||
                     ((*(ptr + 2) == 'e') || (*(ptr + 2) == 'E'))) &&
                    (*(ptr + 3) == '\0')) ||
                   (((*(ptr + 1) == 'e') || (*(ptr + 1) == 'E')) &&
                    ((*(ptr + 2) == 'p') || (*(ptr + 2) == 'P')) &&
                    ((*(ptr + 3) == 'g') || (*(ptr + 3) == 'G')) &&
                    (*(ptr + 4) == '\0'))))
              {
                 (void)strcpy(content_type, "IMAGE/jpeg");
              }
         else if (((*ptr == 't') || (*ptr == 'T')) &&
                  ((*(ptr + 1) == 'i') || (*(ptr + 1) == 'I')) &&
                  ((*(ptr + 2) == 'f') || (*(ptr + 2) == 'F')) &&
                  ((((*(ptr + 3) == 'f') || (*(ptr + 3) == 'F')) &&
                   (*(ptr + 4) == '\0')) ||
                   (*(ptr + 3) == '\0')))
              {
                 (void)strcpy(content_type, "IMAGE/tiff");
              }
         else if (((*ptr == 'g') || (*ptr == 'G')) &&
                  ((*(ptr + 1) == 'i') || (*(ptr + 1) == 'I')) &&
                  ((*(ptr + 2) == 'f') || (*(ptr + 2) == 'F')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "IMAGE/gif");
              }
         else if (((*ptr == 'p') || (*ptr == 'P')) &&
                  ((*(ptr + 1) == 'd') || (*(ptr + 1) == 'D')) &&
                  ((*(ptr + 2) == 'f') || (*(ptr + 2) == 'F')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/pdf");
              }
         else if (((*ptr == 'd') || (*ptr == 'D')) &&
                  ((*(ptr + 1) == 'o') || (*(ptr + 1) == 'O')) &&
                  ((*(ptr + 2) == 'c') || (*(ptr + 2) == 'C')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/msword");
              }
         else if (((*ptr == 'b') || (*ptr == 'B')) &&
                  ((*(ptr + 1) == 'z') || (*(ptr + 1) == 'Z')) &&
                  (*(ptr + 2) == '2') && (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/x-bzip2");
              }
         else if ((((*ptr == 'g') || (*ptr == 'G')) &&
                   ((*(ptr + 1) == 'z') || (*(ptr + 1) == 'Z')) &&
                   (*(ptr + 2) == '\0')) ||
                  (((*ptr == 't') || (*ptr == 'T')) &&
                   ((*(ptr + 1) == 'g') || (*(ptr + 1) == 'G')) &&
                   ((*(ptr + 2) == 'z') || (*(ptr + 2) == 'Z')) &&
                   (*(ptr + 3) == '\0')))
              {
                 (void)strcpy(content_type, "APPLICATION/x-gzip");
              }
         else if (((*ptr == 't') || (*ptr == 'T')) &&
                  ((*(ptr + 1) == 'a') || (*(ptr + 1) == 'A')) &&
                  ((*(ptr + 2) == 'r') || (*(ptr + 2) == 'R')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/x-tar");
              }
         else if (((*ptr == 'z') || (*ptr == 'Z')) &&
                  ((*(ptr + 1) == 'i') || (*(ptr + 1) == 'I')) &&
                  ((*(ptr + 2) == 'p') || (*(ptr + 2) == 'P')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/zip");
              }
         else if (((*ptr == 'm') || (*ptr == 'M')) &&
                  ((*(ptr + 1) == 'p') || (*(ptr + 1) == 'P')) &&
                  ((((*(ptr + 2) == 'g') || (*(ptr + 2) == 'G')) &&
                    ((*(ptr + 3) == 'a') || (*(ptr + 3) == 'A')) &&
                    (*(ptr + 4) == '\0')) ||
                   (((*(ptr + 2) == '2') || (*(ptr + 2) == '3')) &&
                    (*(ptr + 4) == '\0'))))
              {
                 (void)strcpy(content_type, "VIDEO/mpeg");
              }
         else if ((((*ptr == 'm') || (*ptr == 'M')) &&
                   ((*(ptr + 1) == 'o') || (*(ptr + 1) == 'O')) &&
                   ((*(ptr + 2) == 'v') || (*(ptr + 2) == 'V')) &&
                   (*(ptr + 3) == '\0')) ||
                  (((*ptr == 'q') || (*ptr == 'Q')) &&
                   ((*(ptr + 1) == 't') || (*(ptr + 1) == 'T')) &&
                   (*(ptr + 2) == '\0')))
              {
                 (void)strcpy(content_type, "VIDEO/quicktime");
              }
         else if (((((*ptr == 'a') || (*ptr == 'A')) &&
                    ((*(ptr + 1) == 's') || (*(ptr + 1) == 'S')) &&
                    ((*(ptr + 2) == 'c') || (*(ptr + 2) == 'C'))) ||
                   (((*ptr == 't') || (*ptr == 'T')) &&
                    ((*(ptr + 1) == 'x') || (*(ptr + 1) == 'X')) &&
                    ((*(ptr + 2) == 't') || (*(ptr + 2) == 'T')))) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "TEXT/plain");
              }
         else if (((*ptr == 'h') || (*ptr == 'H')) &&
                  ((*(ptr + 1) == 't') || (*(ptr + 1) == 'T')) &&
                  ((*(ptr + 2) == 'm') || (*(ptr + 2) == 'M')) &&
                  ((((*(ptr + 3) == 'l') || (*(ptr + 3) == 'L')) &&
                   (*(ptr + 4) == '\0')) ||
                   (*(ptr + 3) == '\0')))
              {
                 (void)strcpy(content_type, "TEXT/html");
              }
      }
   }
   if (content_type[0] == '\0')
   {
      (void)strcpy(content_type, "APPLICATION/octet-stream");
   }
   return;
}


/*++++++++++++++++++++++++++++++ get_reply() ++++++++++++++++++++++++++++*/
static int
get_reply(FILE *smtp_fp)
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
      if ((isdigit((int)msg_str[0]) != 0) && (isdigit((int)msg_str[1]) != 0) &&
          (isdigit((int)msg_str[2]) != 0) && (msg_str[3] != '-'))
      {
         break;
      }
   }

   return(((msg_str[0] - '0') * 100) + ((msg_str[1] - '0') * 10) +
          (msg_str[2] - '0'));
}


/*----------------------------- read_msg() ------------------------------*/
static int
read_msg(void)
{
   static int  bytes_buffered,
               bytes_read = 0;
   static char *read_ptr = NULL;
   int         status;
   fd_set      rset;

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

   FD_ZERO(&rset);
   for (;;)
   {
      if (bytes_read <= 0)
      {
         /* Initialise descriptor set */
         FD_SET(smtp_fd, &rset);
         timeout.tv_usec = 0L;
         timeout.tv_sec = transfer_timeout;

         /* Wait for message x seconds and then continue. */
         status = select(smtp_fd + 1, &rset, NULL, NULL, &timeout);

         if (status == 0)
         {
            /* timeout has arrived */
            timeout_flag = ON;
            bytes_read = 0;
            return(INCORRECT);
         }
         else if (FD_ISSET(smtp_fd, &rset))
              {
                 if ((bytes_read = read(smtp_fd, &msg_str[bytes_buffered],
                                        (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                 {
                    if (bytes_read == 0)
                    {
                       trans_log(ERROR_SIGN,  __FILE__, __LINE__, NULL,
                                 "Remote hang up.");
                       timeout_flag = NEITHER;
                    }
                    else
                    {
                       if (errno == ECONNRESET)
                       {
                          timeout_flag = CON_RESET;
                       }
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                 "read() error (after reading %d Bytes) : %s",
                                 bytes_buffered, strerror(errno));
                       bytes_read = 0;
                    }
                    return(INCORRECT);
                 }
#ifdef WITH_TRACE
                 trace_log(NULL, 0, R_TRACE,
                           &msg_str[bytes_buffered], bytes_read, NULL);
#endif
                 read_ptr = &msg_str[bytes_buffered];
                 bytes_buffered += bytes_read;
              }
         else if (status < 0)
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "select() error : %s", strerror(errno));
                 exit(INCORRECT);
              }
              else
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
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

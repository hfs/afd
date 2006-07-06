/*
 *  ftpcmd.c - Part of AFD, an automatic file distribution program.
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
 **   ftpcmd - commands to send files via FTP
 **
 ** SYNOPSIS
 **   int  ftp_connect(char *hostname, int port)
 **   int  ftp_ssl_auth(void)
 **   int  ftp_idle(int timeout)
 **   int  ftp_user(char *user)
 **   int  ftp_pass(char *password)
 **   int  ftp_ssl_init(char type)
 **   int  ftp_type(char type)
 **   int  ftp_cd(char *directory)
 **   int  ftp_pwd(void)
 **   int  ftp_chmod(char *filename, char *mode)
 **   int  ftp_move(char *from, char *to)
 **   int  ftp_dele(char *filename)
 **   int  ftp_list(int mode, int type, ...)
 **   int  ftp_exec(char *cmd, char *filename)
 **   int  ftp_data(char *filename, off_t seek, int mode, int type)
 **   int  ftp_close_data(void)
 **   int  ftp_write(char *block, char *buffer, int size)
 **   int  ftp_keepalive(void)
 **   int  ftp_read(char *block, int blocksize)
 **   int  ftp_quit(void)
 **   int  ftp_get_reply(void)
 **   int  ftp_size(char *filename, off_t *remote_size)
 **   int  ftp_date(char *filenname, time_t *file_mtime)
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
 **             +-----------> ftp_idle()
 **             |                 |
 **             +<----------------+
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
 **          ftp_write()<---------------+     |
 **             |                       |     |
 **             +----> ftp_keepalive()  |     |
 **             |             |         |     |
 **             +<------------+         |     |
 **             |                       |     |
 **             V                       |     |
 **      +-------------+       NO       |     |
 **      | File done ? |----------------+     |
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
 **   The second argument buffer to ftp_write() can be NULL, if you do
 **   not **   want it to add a carriage return (CR) to a line feed
 **   (LF) when transmitting in ASCII mode. However when used the first
 **   byte of this buffer has special meaning in that it always contains
 **   the last byte of the previous block written. Since ftp_write() does
 **   not know when a new file starts, ie. the first block of a file, the
 **   caller is responsible in setting this to zero for the first block
 **   of a file.
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
 **   03.11.2000 H.Kiehl Added ftp_chmod() function.
 **   12.11.2000 H.Kiehl Added ftp_exec() function.
 **   21.05.2001 H.Kiehl Failed to fclose() control connection when an error
 **                      occured in ftp_quit().
 **   22.07.2001 H.Kiehl Reduce transfer_timeout to 20 second in function
 **                      ftp_get_reply().
 **   26.07.2001 H.Kiehl Added ftp_account() function.
 **   05.08.2001 H.Kiehl Added ftp_idle() function.
 **   02.09.2001 H.Kiehl Added ftp_keepalive() function.
 **   29.10.2002 H.Kiehl Remove remote file if RNTO command files in
 **                      function ftp_move().
 **   06.11.2002 H.Kiehl Try to remove remote file if we fail to open
 **                      a remote file and the return message says this
 **                      is a overwrite error with ftp_data().
 **   24.12.2003 H.Kiehl Added tracing.
 **   09.01.2004 H.Kiehl Added fast_move parameter to function ftp_move().
 **                      Remove function check_reply().
 **   01.02.2004 H.Kiehl Added TLS/SSL support.
 **   11.03.2004 H.Kiehl Introduced command() to make code more readable
 **                      and removed all stdio buffering code.
 **   02.09.2004 H.Kiehl ftp_date() function returns unix time.
 **   07.10.2004 H.Kiehl Added ftp_pwd().
 **                      ftp_move() creates the missing directory on demand.
 */
DESCR__E_M3


#include <stdio.h>        /* fprintf(), fdopen(), fclose()               */
#include <stdarg.h>       /* va_start(), va_arg(), va_end()              */
#include <string.h>       /* memset(), memcpy(), strcpy()                */
#include <stdlib.h>       /* strtoul()                                   */
#include <ctype.h>        /* isdigit()                                   */
#include <time.h>         /* mktime()                                    */
#include <setjmp.h>       /* setjmp(), longjmp()                         */
#include <signal.h>       /* signal()                                    */
#include <sys/types.h>    /* fd_set                                      */
#include <sys/time.h>     /* struct timeval                              */
#ifdef WITH_SENDFILE
# include <sys/sendfile.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>  /* socket(), shutdown(), bind(), accept(),     */
                          /* setsockopt()                                */
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>  /* struct in_addr, sockaddr_in, htons()        */
#endif
#if defined (_WITH_TOS) && defined (IP_TOS)
# ifdef HAVE_NETINET_IN_SYSTM_H
#  include <netinet/in_systm.h>
# endif
# ifdef HAVE_NETINET_IP_H
#  include <netinet/ip.h> /* IPTOS_LOWDELAY, IPTOS_THROUGHPUT            */
# endif
#endif
#ifdef HAVE_ARPA_TELNET_H
# include <arpa/telnet.h> /* IAC, SYNCH, IP                              */
#endif
#ifdef HAVE_NETDB_H
# include <netdb.h>       /* struct hostent, gethostbyname()             */
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>   /* inet_addr()                                 */
#endif
#ifdef WITH_SSL
# include <openssl/crypto.h>
# include <openssl/x509.h>
# include <openssl/ssl.h>
#endif
#include <unistd.h>       /* select(), write(), read(), close(), alarm() */
#include <errno.h>
#include "fddefs.h"
#include "ftpdefs.h"
#include "commondefs.h"

#ifdef WITH_SSL
SSL                       *ssl_con = NULL;
#endif

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
                          data_fd = -1;
#ifdef WITH_SSL
static SSL                *ssl_data = NULL;
static SSL_CTX            *ssl_ctx;
#endif
static jmp_buf            env_alrm;
static struct sockaddr_in ctrl,
                          data;
static struct timeval     timeout;

/* Local function prototypes */
static int                check_data_socket(int, int, int *, char *),
                          get_number(char **, char),
                          get_reply(void),
#ifdef WITH_SSL
                          encrypt_data_connection(int),
                          read_data_line(int, SSL *, char *),
                          read_data_to_buffer(int, SSL *, char ***),
#else
                          read_data_line(int, char *),
                          read_data_to_buffer(int, char ***),
#endif
                          read_msg(void),
                          read_stat_data(void);
static void               sig_handler(int);


/*########################## ftp_connect() ##############################*/
int
ftp_connect(char *hostname, int port)
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
                         "ftp_connect(): Failed to gethostbyname() %s : %s",
                         hostname, h_errlist[h_errno]);
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "ftp_connect(): Failed to gethostbyname() %s (h_errno = %d) : %s",
                         hostname, h_errno, strerror(errno));
            }
#else
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_connect(): Failed to gethostbyname() %s (h_errno = %d) : %s",
                      hostname, h_errno, strerror(errno));
#endif
         }
         else
         {
#endif /* !_HPUX && !_SCO */
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_connect(): Failed to gethostbyname() %s : %s",
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

   if ((control_fd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "ftp_connect(): socket() error : %s", strerror(errno));
      return(INCORRECT);
   }

   sin.sin_port = htons((u_short)port);

   if (connect(control_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "ftp_connect(): Failed to connect() to %s : %s",
                hostname, strerror(errno));
      (void)close(control_fd);
      return(INCORRECT);
   }
#ifdef WITH_TRACE
   length = sprintf(msg_str, "Connected to %s", hostname);
   trace_log(NULL, 0, C_TRACE, msg_str, length, NULL);
#endif

   length = sizeof(ctrl);
   if (getsockname(control_fd, (struct sockaddr *)&ctrl, &length) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "ftp_connect(): getsockname() error : %s", strerror(errno));
      (void)close(control_fd);
      return(INCORRECT);
   }

#if defined (_WITH_TOS) && defined (IP_TOS) && defined (IPTOS_LOWDELAY)
   reply = IPTOS_LOWDELAY;
   if (setsockopt(control_fd, IPPROTO_IP, IP_TOS, (char *)&reply,
                  sizeof(reply)) < 0)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                "ftp_connect(): setsockopt() IP_TOS error : %s",
                strerror(errno));
   }
#endif

   if ((reply = get_reply()) < 0)
   {
      (void)close(control_fd);
      return(INCORRECT);
   }
   if ((reply != 220) && (reply != 120))
   {
      if (reply != 230)
      {
         (void)close(control_fd);
      }
      return(reply);
   }

   return(SUCCESS);
}


#ifdef WITH_SSL
/*########################## ftp_ssl_auth() #############################*/
int
ftp_ssl_auth(void)
{
   int reply = SUCCESS;

   if (ssl_con == NULL)
   {
      reply = command(control_fd, "AUTH TLS");
      if ((reply == SUCCESS) && ((reply = get_reply()) != INCORRECT))
      {
         if ((reply == 234) || (reply == 334))
         {
            char *p_env,
                 *p_env1;

            SSLeay_add_ssl_algorithms();
            ssl_ctx=(SSL_CTX *)SSL_CTX_new(SSLv23_client_method());
            SSL_CTX_set_options(ssl_ctx, SSL_OP_ALL);
            SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);
            if ((p_env = getenv("SSL_CIPHER")) != NULL)
            {
               SSL_CTX_set_cipher_list(ssl_ctx, p_env);
            }
            else
            {
               SSL_CTX_set_cipher_list(ssl_ctx, NULL);
            }
            if (((p_env = getenv(X509_get_default_cert_file_env())) != NULL) &&
                ((p_env1 = getenv(X509_get_default_cert_dir_env())) != NULL))
            {
               SSL_CTX_load_verify_locations(ssl_ctx, p_env, p_env1);
            }
#ifdef WHEN_WE_KNOW
            if (((p_env = getenv("SSL_CRL_FILE")) != NULL) && 
                ((p_env1 = getenv("SSL_CRL_DIR")) != NULL))
            {
            }
            else
            {
            }
#endif
            SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);

            ssl_con = (SSL *)SSL_new(ssl_ctx);
            SSL_set_connect_state(ssl_con);
            SSL_set_fd(ssl_con, control_fd);

            if (SSL_connect(ssl_con) <= 0)
            {
               char *ptr;

               ptr = ssl_error_msg("SSL_connect", ssl_con, reply, msg_str);
               reply = SSL_get_verify_result(ssl_con);
               if (reply == X509_V_ERR_CRL_SIGNATURE_FAILURE)
               {
                  (void)strcpy(ptr,
                               " | Verify result: The signature of the certificate is invalid!");
               }
               else if (reply == X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD)
                    {
                        (void)strcpy(ptr,
                                     " | Verify result: The CRL nextUpdate field contains an invalid time.");
                    }
               else if (reply == X509_V_ERR_CRL_HAS_EXPIRED)
                    {
                        (void)strcpy(ptr,
                                     " | Verify result: The CRL has expired.");
                    }
               else if (reply == X509_V_ERR_CERT_REVOKED)
                    {
                       (void)strcpy(ptr,
                                    " | Verify result: Certificate revoked.");
                    }
               else if (reply > X509_V_OK)
                    {
                       (void)sprintf(ptr, " | Verify result: %d", reply);
                    }
               reply = INCORRECT;
            }
            else
            {
               char       *ssl_version;
               int        length,
                          ssl_bits;
               SSL_CIPHER *ssl_cipher;

               ssl_version = SSL_get_cipher_version(ssl_con);
               ssl_cipher = SSL_get_current_cipher(ssl_con);
               SSL_CIPHER_get_bits(ssl_cipher, &ssl_bits);
               length = strlen(msg_str);
               (void)sprintf(&msg_str[length], "  <%s, cipher %s, %d bits>",
                             ssl_version, SSL_CIPHER_get_name(ssl_cipher),
                             ssl_bits);
               reply = SUCCESS;
            }
         }
         else
         {
            reply = INCORRECT;
         }
      }
   }

   return(reply);
}
#endif /* WITH_SSL */


/*############################## ftp_user() #############################*/
int
ftp_user(char *user)
{
   int reply,
       count = 0;

   do
   {
      reply = command(control_fd, "USER %s", user);
      if ((reply |= SUCCESS) || ((reply = get_reply()) < 0))
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
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                   "Hmmm. Still thinks I am logged on. Lets wait for a while.");
         (void)my_usleep(700000L);
      }
   } while ((reply == 430) && (count++ < 10));

   /*
    * NOTE: We delibaretly ignore 230 here, since this means that no
    *       password is required. Thus we do have to return the 230
    *       so the apllication knows what to do with it.
    */
   if ((reply == 331) || (reply == 332))
   {
      reply = SUCCESS;
   }

   return(reply);
}


/*############################ ftp_account() ############################*/
int
ftp_account(char *user)
{
   int reply;

   if ((reply = command(control_fd, "ACCT %s", user)) == SUCCESS)
   {
      if ((reply = get_reply()) != INCORRECT)
      {
         /*
          * NOTE: We delibaretly ignore 230 here, since this means that no
          *       password is required. Thus we do have to return the 230
          *       so the apllication knows what to do with it.
          */
         if (reply == 202)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*############################## ftp_pass() #############################*/
int
ftp_pass(char *password)
{
   int reply;

   if ((reply = command(control_fd, "PASS %s", password)) == SUCCESS)
   {
      if ((reply = get_reply()) != INCORRECT)
      {
          /*
           * If the remote server returns 332, it want's as the next step
           * the ACCT command. We do not handle this automatically since in
           * case of a proxy there can be more then one login name and we
           * don't know which one to take. Thus, the user must use the
           * proxy syntax of AFD, ie do it by hand :-(
           */
         if ((reply == 230) || (reply == 202) || (reply == 332))
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


#ifdef WITH_SSL
/*########################## ftp_ssl_init() #############################*/
int
ftp_ssl_init(char type)
{
   int reply = SUCCESS;

   if ((type == YES) || (type == BOTH))
   {
      reply = command(control_fd, "PBSZ 0");
      if ((reply == SUCCESS) && ((reply = get_reply()) != INCORRECT))
      {
         if (reply == 200)
         {
            if (type == BOTH)
            {
               reply = command(control_fd, "PROT P");
            }
            else
            {
               reply = command(control_fd, "PROT C");
            }
            if ((reply == SUCCESS) && ((reply = get_reply()) != INCORRECT))
            {
               if (reply == 200)
               {
                  reply = SUCCESS;
               }
            }
         }
      }
   }

   return(reply);
}
#endif /* WITH_SSL */


/*############################## ftp_idle() #############################*/
int
ftp_idle(int timeout)
{
   int reply;

   if ((reply = command(control_fd, "SITE IDLE %d", timeout)) == SUCCESS)
   {
      if ((reply = get_reply()) != INCORRECT)
      {
         if (reply == 200)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*############################### ftp_pwd() #############################*/
int
ftp_pwd(void)
{
   int reply;

   if ((reply = command(control_fd, "PWD")) == SUCCESS)
   {
      if ((reply = get_reply()) != INCORRECT)
      {
         if (reply == 257)
         {
            char *ptr;

            ptr = msg_str + 4;
            if (*ptr == '"')
            {
               char *end_ptr;

               ptr++;
               end_ptr = ptr;
               while ((*end_ptr != '"') && (*end_ptr != '\0'))
               {
                  end_ptr++;
               }
               if (*end_ptr == '"')
               {
                  (void)memmove(msg_str, ptr, end_ptr - ptr);
                  msg_str[end_ptr - ptr] = '\0';
                  reply = SUCCESS;
               }
            }
         }
      }
   }

   return(reply);
}


/*############################## ftp_type() #############################*/
int
ftp_type(char type)
{
   int reply;

   if ((reply = command(control_fd, "TYPE %c", type)) == SUCCESS)
   {
      if ((reply = get_reply()) != INCORRECT)
      {
         if (reply == 200)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*############################### ftp_cd() ##############################*/
int
ftp_cd(char *directory, int create_dir)
{
   int reply;

   if (directory[0] == '\0')
   {
      reply = command(control_fd, "CWD ~");
   }
   else
   {
      reply = command(control_fd, "CWD %s", directory);
   }

   if ((reply == SUCCESS) && ((reply = get_reply()) != INCORRECT))
   {
      if ((reply == 250) || (reply == 200))
      {
         reply = SUCCESS;
      }
      else if ((create_dir == YES) && (directory[0] != '\0') && (reply == 550))
           {
              int  offset = 0;
              char *p_start,
                   *ptr;

              ptr = directory;

              /*
               * We need to check if this is an absolute path and set
               * offset, otherwise we will create the remote path
               * as relative.
               */
              while (*ptr == '/')
              {
                 ptr++;
                 offset = 1;
              }

              do
              {
                 while (*ptr == '/')
                 {
                    ptr++;
                 }
                 if (offset)
                 {
                    p_start = ptr - 1;
                    offset = 0;
                 }
                 else
                 {
                    p_start = ptr;
                 }
                 while ((*ptr != '/') && (*ptr != '\0'))
                 {
                    ptr++;
                 }
                 if ((*ptr == '/') || ((*ptr == '\0') && (p_start != ptr)))
                 {
                    *ptr = '\0';
                    reply = command(control_fd, "CWD %s", p_start);
                    if ((reply == SUCCESS) &&
                        ((reply = get_reply()) != INCORRECT))
                    {
                       if ((reply != 250) && (reply != 200))
                       {
                          reply = command(control_fd, "MKD %s", p_start);
                          if ((reply == SUCCESS) &&
                              ((reply = get_reply()) != INCORRECT))
                          {
                             if (reply == 257)
                             {
                                reply = command(control_fd, "CWD %s", p_start);
                                if ((reply == SUCCESS) &&
                                    ((reply = get_reply()) != INCORRECT))
                                {
                                   if ((reply != 250) && (reply != 200))
                                   {
                                      p_start = NULL;
                                   }
                                   else
                                   {
                                      reply = SUCCESS;
                                   }
                                }
                             }
                             else
                             {
                                p_start = NULL;
                             }
                          }
                       }
                    }
                    else
                    {
                       p_start = NULL;
                    }
                    *ptr = '/';
                 }
              } while ((*ptr != '\0') && (p_start != NULL));
           }
   }

   return(reply);
}


/*############################# ftp_chmod() #############################*/
int
ftp_chmod(char *filename, char *mode)
{
   int reply;

   if ((reply = command(control_fd, "SITE CHMOD %s %s", mode, filename)) == SUCCESS)
   {
      if ((reply = get_reply()) != INCORRECT)
      {
         if ((reply == 250) || (reply == 200))
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*############################## ftp_move() #############################*/
int
ftp_move(char *from, char *to, int fast_move, int create_dir)
{
   int reply;
#ifdef WITH_MS_ERROR_WORKAROUND
   int retries = 0;
#endif

#ifdef WITH_MS_ERROR_WORKAROUND
retry_command:
#endif
   if (fast_move)
   {
      reply = command(control_fd, "RNFR %s\r\nRNTO %s", from, to);
   }
   else
   {
      reply = command(control_fd, "RNFR %s", from);
   }

   if ((reply != SUCCESS) || ((reply = get_reply()) < 0))
   {
      return(INCORRECT);
   }

#ifdef WITH_MS_ERROR_WORKAROUND
   if ((reply == 550) && (retries == 0))
   {
      if (fast_move)
      {
         (void)get_reply();
      }
      retries++;
      my_usleep(50000L);
      goto retry_command;
   }
#endif

   if ((reply != 350) && (reply != 200))
   {
      if (fast_move)
      {
         char tmp_msg_str[MAX_RET_MSG_LENGTH];

         /*
          * We already have send the second command and it is very likely
          * that there will be a reply for this one too. So lets read
          * that reply as well, but remember to store the original
          * messages otherwise we will show the user the reply
          * of the second command, which will presumably always be
          * wrong and thus the user will be unable to see what went
          * wrong.
          */
         (void)memcpy(tmp_msg_str, msg_str, MAX_RET_MSG_LENGTH);
         (void)get_reply();
         (void)memcpy(msg_str, tmp_msg_str, MAX_RET_MSG_LENGTH);
      }
      return(reply);
   }

   if (!fast_move)
   {
      if (command(control_fd, "RNTO %s", to) != SUCCESS)
      {
         return(INCORRECT);
      }
   }

   /* Get reply from RNTO part. */
   if ((reply = get_reply()) < 0)
   {
      return(INCORRECT);
   }

   if ((reply != 250) && (reply != 200))
   {
      /*
       * We would overwrite the original file in any case, so if
       * the RNTO part fails lets try to delete it and try again.
       */
      if (ftp_dele(to) == SUCCESS)
      {
         if (!fast_move)
         {
            /*
             * YUCK! Window system require that we send the RNFR again!
             * This is unnecessary, we only need to send the RNTO
             * again.
             */
            reply = command(control_fd, "RNFR %s", from);
            if ((reply != SUCCESS) || ((reply = get_reply()) < 0))
            {
               return(INCORRECT);
            }

            if ((reply != 350) && (reply != 200))
            {
               return(reply);
            }
         }

         reply = command(control_fd, "RNTO %s", to);
         if ((reply != SUCCESS) || ((reply = get_reply()) < 0))
         {
            return(INCORRECT);
         }

         if ((reply != 250) && (reply != 200))
         {
            return(reply);
         }
      }
      else if ((reply == 550) && (create_dir == YES))
           {
              char *ptr,
                   to_dir[MAX_PATH_LENGTH];

              (void)strcpy(to_dir, to);
              ptr = to_dir + strlen(to_dir) - 1;
              while ((*ptr == '/') && (ptr > to_dir))
              {
                 ptr--;
              }
              while ((*ptr != '/') && (ptr > to_dir))
              {
                 ptr--;
              }
              if ((*ptr == '/') && (to_dir != ptr))
              {
                 *ptr = '\0';
                 *(ptr + 1) = '\0';
                 if (ftp_pwd() == SUCCESS)
                 {
                    char current_dir[MAX_PATH_LENGTH];

                    (void)strcpy(current_dir, msg_str);

                    /* The following will actually create the directory. */
                    if (ftp_cd(to_dir, YES) == SUCCESS)
                    {
                       if (ftp_cd(current_dir, NO) == SUCCESS)
                       {
                          reply = command(control_fd, "RNFR %s", from);
                          if ((reply != SUCCESS) || ((reply = get_reply()) < 0))
                          {
                             return(INCORRECT);
                          }
                          reply = command(control_fd, "RNTO %s", to);
                          if ((reply != SUCCESS) || ((reply = get_reply()) < 0))
                          {
                             return(INCORRECT);
                          }
                          if ((reply != 250) && (reply != 200))
                          {
                             return(reply);
                          }
                       }
                       else
                       {
                          return(reply);
                       }
                    }
                    else
                    {
                       return(reply);
                    }
                 }
                 else
                 {
                    return(reply);
                 }
              }
              else
              {
                 return(reply);
              }
           }
           else
           {
              return(reply);
           }
   }

   return(SUCCESS);
}


/*############################## ftp_dele() #############################*/
int
ftp_dele(char *filename)
{
   int reply;

   if ((reply = command(control_fd, "DELE %s", filename)) == SUCCESS)
   {
      if ((reply = get_reply()) != INCORRECT)
      {
         if ((reply == 250) || (reply == 200))
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*########################### ftp_keepalive() ###########################*/
int
ftp_keepalive(void)
{
   int           reply;
   long          tmp_transfer_timeout;
   unsigned char telnet_cmd[2];
   fd_set        wset;

   tmp_transfer_timeout = transfer_timeout;
   transfer_timeout = 0L;
   while ((reply = read_msg()) > 0)
   {
      trans_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                "ftp_keepalive(): Hmmm, read %d Bytes.", reply);
   }
   timeout_flag = OFF;
   transfer_timeout = tmp_transfer_timeout;
   telnet_cmd[0] = IAC;
   telnet_cmd[1] = IP;

   /* Initialise descriptor set. */
   FD_ZERO(&wset);
   FD_SET(control_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   reply = select(control_fd + 1, NULL, &wset, NULL, &timeout);

   if (reply == 0)
   {
      /* timeout has arrived */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (FD_ISSET(control_fd, &wset))
        {
#ifdef WITH_SSL
           if (ssl_con == NULL)
           {
#endif
              if ((reply = write(control_fd, telnet_cmd, 2)) != 2)
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "ftp_keepalive(): write() error (%d) : %s", reply, strerror(errno));
                 return(reply);
              }
#ifdef WITH_SSL
           }
           else
           {
              if ((reply = ssl_write(ssl_con, (char *)telnet_cmd, 2)) != 2)
              {
                 return(reply);
              }
           }
#endif
        }
   else if (reply < 0)
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "ftp_keepalive(): select() error : %s", strerror(errno));
           return(INCORRECT);
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "ftp_keepalive(): Unknown condition.");
           return(INCORRECT);
        }

   /* Initialise descriptor set. */
   FD_SET(control_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   reply = select(control_fd + 1, NULL, &wset, NULL, &timeout);

   if (reply == 0)
   {
      /* timeout has arrived */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (FD_ISSET(control_fd, &wset))
        {
           if ((reply = send(control_fd, telnet_cmd, 1, MSG_OOB)) != 1)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "ftp_keepalive(): write() error (%d) : %s",
                        reply, strerror(errno));
              return(errno);
           }
        }
   else if (reply < 0)
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "ftp_keepalive(): select() error : %s", strerror(errno));
           return(INCORRECT);
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "ftp_keepalive(): Unknown condition.");
           return(INCORRECT);
        }
   if (command(control_fd, "%cSTAT", DM) != SUCCESS)
   {
      return(INCORRECT);
   }

   if ((reply = read_stat_data()) < 0)
   {
      return(INCORRECT);
   }
   if (reply == 426)
   {
      if ((reply = read_stat_data()) < 0)
      {
         return(INCORRECT);
      }
   }

   if ((reply != 211) && (reply != 212) && (reply != 213))
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

   if ((reply = command(control_fd, "SIZE %s", filename)) == SUCCESS)
   {
      if ((reply = get_reply()) != INCORRECT)
      {
         if (reply == 213)
         {
            char *ptr = &msg_str[3];

            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
            if ((*remote_size = (off_t)strtoul(ptr, NULL, 10)) != ULONG_MAX)
            {
               reply = SUCCESS;
            }
            else
            {
               *remote_size = 0;
               reply = INCORRECT;
            }
         }
      }
   }

   return(reply);
}


/*############################## ftp_date() #############################*/
int
ftp_date(char *filename, time_t *file_mtime)
{
   int reply;

   if ((reply = command(control_fd, "MDTM %s", filename)) == SUCCESS)
   {
      if ((reply = get_reply()) != INCORRECT)
      {
         if (reply == 213)
         {
            int  i = 0;
            char date[MAX_FTP_DATE_LENGTH],
                 *ptr = &msg_str[3];

            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }

            /* Store date YYYYMMDDhhmmss */
            while ((*ptr != '\0') && (i < MAX_FTP_DATE_LENGTH))
            {
               date[i] = *ptr;
               ptr++; i++;
            }
            date[i] = '\0';
            if (i == (MAX_FTP_DATE_LENGTH - 1))
            {
               struct tm bd_time;

               bd_time.tm_sec  = atoi(&date[i - 2]);
               date[i - 2] = '\0';
               bd_time.tm_min  = atoi(&date[i - 4]);
               date[i - 4] = '\0';
               bd_time.tm_hour = atoi(&date[i - 6]);
               date[i - 6] = '\0';
               bd_time.tm_mday = atoi(&date[i - 8]);
               date[i - 8] = '\0';
               bd_time.tm_mon = atoi(&date[i - 10]) - 1;
               date[i - 10] = '\0';
               bd_time.tm_year = atoi(date) - 1900;
               bd_time.tm_isdst = 0;
               *file_mtime = mktime(&bd_time);
            }
            else
            {
               *file_mtime = 0L;
            }
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*############################# ftp_exec() ##############################*/
int
ftp_exec(char *cmd, char *filename)
{
   int reply;

   if (filename == NULL)
   {
      reply = command(control_fd, "SITE %s", cmd);
   }
   else
   {
      reply = command(control_fd, "SITE %s %s", cmd, filename);
   }

   if ((reply == SUCCESS) && ((reply = get_reply()) != INCORRECT))
   {
      if ((reply == 250) || (reply == 200))
      {
         reply = SUCCESS;
      }
   }

   return(reply);
}


/*############################## ftp_list() #############################*/
int
ftp_list(int mode, int type, ...)
{
   int     new_sock_fd,
           on = 1,
           reply;
   char    **buffer,
           *filename,
           *msg;
   va_list ap;

   va_start(ap, type);
   if (type & BUFFERED_LIST)
   {
      buffer = va_arg(ap, char **);
      filename = NULL;
      msg = NULL;
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

   if (mode == PASSIVE_MODE)
   {
      char *ptr;

      reply = command(control_fd, "PASV");
      if ((reply != SUCCESS) || ((reply = get_reply()) < 0))
      {
         return(INCORRECT);
      }

      if (reply != 227)
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
         if ((number = get_number(&ptr, ',')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_list(): Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_addr) = number;
         if ((number = get_number(&ptr, ',')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_list(): Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_addr + 1) = number;
         if ((number = get_number(&ptr, ',')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_list(): Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_addr + 2) = number;
         if ((number = get_number(&ptr, ',')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_list(): Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_addr + 3) = number;
         if ((number = get_number(&ptr, ',')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_list(): Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_port) = number;
         if ((number = get_number(&ptr, ')')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_list(): Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_port + 1) = number;

         if ((new_sock_fd = socket(data.sin_family, SOCK_STREAM,
                                   IPPROTO_TCP)) < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_list(): socket() error : %s", strerror(errno));
            return(INCORRECT);
         }

         if (setsockopt(new_sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                        sizeof(on)) < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_list(): setsockopt() error : %s", strerror(errno));
            (void)close(new_sock_fd);
            return(INCORRECT);
         }
         if (connect(new_sock_fd, (struct sockaddr *) &data, sizeof(data)) < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_list(): Failed to connect() to : %s", strerror(errno));
            (void)close(new_sock_fd);
            return(INCORRECT);
         }

         if (type & NLIST_CMD)
         {
            reply = command(control_fd, "NLST");
         }
         else
         {
            reply = command(control_fd, "LIST %s", filename);
         }

         if ((reply != SUCCESS) || ((reply = get_reply()) < 0))
         {
            (void)close(new_sock_fd);
            return(INCORRECT);
         }

         if ((reply != 150) && (reply != 125))
         {
            (void)close(new_sock_fd);
            return(reply);
         }
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                   "ftp_list(): Failed to locate an open bracket <(> in reply from PASV command.");
         return(INCORRECT);
      }
   }
   else /* mode == ACTIVE_MODE */
   {
      int                sock_fd;
      my_socklen_t       length;
      register char      *h,
                         *p;
      struct sockaddr_in from;

      if ((sock_fd = socket(data.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_list(): socket() error : %s", strerror(errno));
         return(INCORRECT);
      }

      if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                     sizeof(on)) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_list(): setsockopt() error : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }

      length = sizeof(data);
      if (bind(sock_fd, (struct sockaddr *)&data, length) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_list(): bind() error : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }

      if (getsockname(sock_fd, (struct sockaddr *)&data, &length) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_list(): getsockname() error : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }

      if (listen(sock_fd, 1) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_list(): listen() error : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }

      h = (char *)&data.sin_addr;
      p = (char *)&data.sin_port;
      reply = command(control_fd, "PORT %d,%d,%d,%d,%d,%d",
                      (((int)h[0]) & 0xff), (((int)h[1]) & 0xff),
                      (((int)h[2]) & 0xff), (((int)h[3]) & 0xff),
                      (((int)p[0]) & 0xff), (((int)p[1]) & 0xff));
      if ((reply != SUCCESS) || ((reply = get_reply()) < 0))
      {
         (void)close(sock_fd);
         return(INCORRECT);
      }

      if (reply != 200)
      {
         (void)close(sock_fd);
         return(reply);
      }

      if (type & NLIST_CMD)
      {
         reply = command(control_fd, "NLST");
      }
      else
      {
         reply = command(control_fd, "LIST %s", filename);
      }

      if ((reply != SUCCESS) || ((reply = get_reply()) < 0))
      {
         return(INCORRECT);
      }

      if ((reply != 150) && (reply != 125))
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
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_list(): Failed to set signal handler : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }
      if (setjmp(env_alrm) != 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_list(): accept() timeout");
         timeout_flag = ON;
         (void)close(sock_fd);
         return(INCORRECT);
      }
      (void)alarm(2 * transfer_timeout);

      if ((new_sock_fd = accept(sock_fd, (struct sockaddr *) &from, &length)) < 0)
      {
         (void)alarm(0); /* Maybe it was a real accept() error */
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_list(): accept() error : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }
      (void)alarm(0);
      if (close(sock_fd) == -1)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_list(): close() error : %s", strerror(errno));
      }
   } /* mode == ACTIVE_MODE */

#ifdef WITH_SSL
   if (type & ENCRYPT_DATA)
   {
      encrypt_data_connection(new_sock_fd);
   }
#endif

   if (type & BUFFERED_LIST)
   {
#ifdef WITH_SSL
      if (read_data_to_buffer(new_sock_fd, ssl_data, &buffer) < 0)
#else
      if (read_data_to_buffer(new_sock_fd, &buffer) < 0)
#endif
      {
         return(INCORRECT);
      }
   }
   else
   {
#ifdef WITH_SSL
      if (read_data_line(new_sock_fd, ssl_data, msg) < 0)
#else
      if (read_data_line(new_sock_fd, msg) < 0)
#endif
      {
         return(INCORRECT);
      }
   }

#ifdef WITH_SSL
   if ((type & ENCRYPT_DATA) && (ssl_data != NULL))
   {
      SSL_free(ssl_data);
      ssl_data = NULL;
   }
#endif

#ifdef _WITH_SHUTDOWN
   if (shutdown(new_sock_fd, 1) < 0)
   {
      trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                "ftp_list(): shutdown() error : %s", strerror(errno));
   }
#endif

   if (close(new_sock_fd) == -1)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "ftp_list(): close() error : %s", strerror(errno));
      return(INCORRECT);
   }

   /* Read last message: 'Binary Transfer complete' */
   if ((reply = get_reply()) != INCORRECT)
   {
      if ((reply == 226) || (reply == 250))
      {
         reply = SUCCESS;
      }
   }

   return(reply);
}


/*############################## ftp_data() #############################*/
int
ftp_data(char *filename, off_t seek, int mode, int type, int sockbuf_size)
{
   int           new_sock_fd,
                 on = 1,
                 reply;
#if defined (_WITH_TOS) && defined (IP_TOS) && defined (IPTOS_THROUGHPUT)
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
      cmd[0] = 'R'; cmd[1] = 'E'; cmd[2] = 'T'; cmd[3] = 'R';
   }
   cmd[4] = '\0';

   if (mode == PASSIVE_MODE)
   {
      char *ptr;

      if (command(control_fd, "PASV") != SUCCESS)
      {
         return(INCORRECT);
      }
      if ((reply = get_reply()) < 0)
      {
         if (timeout_flag == OFF)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_data(): Failed to get reply after sending PASV command (%d).",
                      reply);
         }
         return(INCORRECT);
      }

      if (reply != 227)
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
         if ((number = get_number(&ptr, ',')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_data(): Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_addr) = number;
         if ((number = get_number(&ptr, ',')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_data(): Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_addr + 1) = number;
         if ((number = get_number(&ptr, ',')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_data(): Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_addr + 2) = number;
         if ((number = get_number(&ptr, ',')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_data(): Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_addr + 3) = number;
         if ((number = get_number(&ptr, ',')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_data(): Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_port) = number;
         if ((number = get_number(&ptr, ')')) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_data(): Failed to retrieve remote address %s", msg_str);
            return(INCORRECT);
         }
         *((char *)&data.sin_port + 1) = number;

         if ((new_sock_fd = socket(data.sin_family, SOCK_STREAM,
                                   IPPROTO_TCP)) < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_data(): socket() error : %s", strerror(errno));
            return(INCORRECT);
         }

         if (setsockopt(new_sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                        sizeof(on)) < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_data(): setsockopt() error : %s", strerror(errno));
            (void)close(new_sock_fd);
            return(INCORRECT);
         }
         if (sockbuf_size > 0)
         {
            int optname;

            if (type == DATA_WRITE)
            {
               optname = SO_SNDBUF;
            }
            else
            {
               optname = SO_RCVBUF;
            }
            if (setsockopt(new_sock_fd, SOL_SOCKET, optname,
                           (char *)&sockbuf_size, sizeof(sockbuf_size)) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                         "ftp_data(): setsockopt() error : %s", strerror(errno));
            }
         }
         if (connect(new_sock_fd, (struct sockaddr *) &data, sizeof(data)) < 0)
         {
#ifdef ETIMEDOUT
            if (errno == ETIMEDOUT)
            {
               timeout_flag = ON;
            }
#endif
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_data(): connect() error : %s", strerror(errno));
            (void)close(new_sock_fd);
            return(INCORRECT);
         }

         /*
          * When we retrieve a file and part of it has already been
          * retrieved, tell the remote ftp server where to put the server
          * marker. No need to resend the data we already have.
          */
         if ((seek > 0) && (type == DATA_READ))
         {
#if SIZEOF_OFF_T == 4
            if (command(control_fd, "REST %ld", seek) != SUCCESS)
#else
            if (command(control_fd, "REST %lld", seek) != SUCCESS)
#endif
            {
               (void)close(new_sock_fd);
               return(INCORRECT);
            }
            if ((reply = get_reply()) < 0)
            {
               if (timeout_flag == OFF)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, msg_str,
                            "ftp_data(): Failed to get proper reply for REST command (%d).",
                            reply);
               }
               else
               {
                  (void)close(new_sock_fd);
                  return(INCORRECT);
               }
            }
            else
            {
               if (reply != 350)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, msg_str,
                            "ftp_data(): Failed to get proper reply for REST command (%d).",
                            reply);
               }
            }
         }

         if (command(control_fd, "%s %s", cmd, filename) != SUCCESS)
         {
            (void)close(new_sock_fd);
            return(INCORRECT);
         }
         if ((reply = get_reply()) < 0)
         {
            if (timeout_flag == OFF)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, msg_str,
                         "ftp_data(): Failed to get proper reply (%d).", reply);
            }
            (void)close(new_sock_fd);
            return(INCORRECT);
         }

         if ((reply != 150) && (reply != 125))
         {
            /*
             * Assume that we may not overwrite the remote file so let's
             * just delete it and try again.
             * This is not 100% correct since there is no shure way to
             * tell that this is an overwrite error, since there are
             * just to many different return codes possible:
             *    553 (Upload)
             *    550 (Filename (deny))
             *    553 (Overwrite)
             * So lets just do it for those cases we know and where
             * a delete makes sence.
             */
            if ((((reply == 553) && (posi(&msg_str[3], "(Overwrite)") != NULL)) ||
                 ((reply == 550) && (posi(&msg_str[3], "Overwrite permission denied") != NULL))) &&
                 (ftp_dele(filename) == SUCCESS))
            {  
               if (command(control_fd, "%s %s", cmd, filename) != SUCCESS)
               {
                  (void)close(new_sock_fd);
                  return(INCORRECT);
               }
               if ((reply = get_reply()) < 0)
               {
                  if (timeout_flag == OFF)
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__, msg_str,
                               "ftp_data(): Failed to get proper reply (%d).",
                               reply);         
                  }
                  (void)close(new_sock_fd);
                  return(INCORRECT);
               }
               if ((reply != 150) && (reply != 125))
               {
                  (void)close(new_sock_fd);
                  return(INCORRECT);
               }
            }   
            else
            {  
               (void)close(new_sock_fd);
               return(reply);
            }
         }
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                   "ftp_data(): Failed to locate an open bracket <(> in reply from PASV command.");
         return(INCORRECT);
      }
   }
   else /* ACTIVE_MODE */
   {
      int                retries = 0,
                         ret,
                         sock_fd;
      my_socklen_t       length;
#ifdef FTP_REUSE_DATA_PORT
      unsigned int       loop_counter = 0;
      static u_short     data_port = 0;
#endif
      register char      *h, *p;
      struct sockaddr_in from;

      do
      {
         data = ctrl;
#ifdef FTP_REUSE_DATA_PORT
try_again:
         if (type != DATA_READ)
         {
            data.sin_port = htons(data_port);
         }
         else
         {
            data.sin_port = htons((u_short)0);
         }
#else
         data.sin_port = htons((u_short)0);
#endif

         if ((sock_fd = socket(data.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_data(): socket() error : %s", strerror(errno));
            return(INCORRECT);
         }

         if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                        sizeof(on)) < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_data(): setsockopt() error : %s", strerror(errno));
            (void)close(sock_fd);
            return(INCORRECT);
         }

         length = sizeof(data);
         if (bind(sock_fd, (struct sockaddr *)&data, length) < 0)
         {
#ifdef FTP_REUSE_DATA_PORT
            if ((type != DATA_READ) &&
                ((errno == EADDRINUSE) || (errno == EACCES)))
            {
               data_port = 0;
               loop_counter++;
               if (loop_counter < 100)
               {
                  goto try_again;
               }
            }
#endif
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_data(): bind() error : %s", strerror(errno));
            (void)close(sock_fd);
            return(INCORRECT);
         }

#ifdef FTP_REUSE_DATA_PORT
         if ((type == DATA_READ) || (data_port == (u_short)0))
         {
#endif
            if (getsockname(sock_fd, (struct sockaddr *)&data, &length) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "ftp_data(): getsockname() error : %s",
                         strerror(errno));
               (void)close(sock_fd);
               return(INCORRECT);
            }
#ifdef FTP_REUSE_DATA_PORT
         }
#endif
         if (sockbuf_size > 0)
         {
            int optname;

            if (type == DATA_WRITE)
            {
               optname = SO_SNDBUF;
            }
            else
            {
               optname = SO_RCVBUF;
            }
            if (setsockopt(sock_fd, SOL_SOCKET, optname,
                           (char *)&sockbuf_size, sizeof(sockbuf_size)) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                         "ftp_data(): setsockopt() error : %s", strerror(errno));
            }
         }

         if (listen(sock_fd, 1) < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "ftp_data(): listen() error : %s", strerror(errno));
            (void)close(sock_fd);
            return(INCORRECT);
         }

         h = (char *)&data.sin_addr;
         p = (char *)&data.sin_port;
#ifdef FTP_REUSE_DATA_PORT
         data_port = data.sin_port;
#endif

         if (command(control_fd, "PORT %d,%d,%d,%d,%d,%d",
                     (((int)h[0]) & 0xff), (((int)h[1]) & 0xff),
                     (((int)h[2]) & 0xff), (((int)h[3]) & 0xff),
                     (((int)p[0]) & 0xff), (((int)p[1]) & 0xff)) != SUCCESS)
         {
            (void)close(sock_fd);
            return(INCORRECT);
         }
         if ((reply = get_reply()) < 0)
         {
            if (timeout_flag == OFF)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, msg_str,
                         "ftp_data(): Failed to get proper reply (%d)", reply);
            }
            (void)close(sock_fd);
            return(INCORRECT);
         }

         if (reply != 200)
         {
            (void)close(sock_fd);
            return(reply);
         }

         /*
          * When we retrieve a file and part of it has already been
          * retrieved, tell the remote ftp server where to put the server
          * marker. No need to resend the data we already have.
          */
         if ((seek > 0) && (type == DATA_READ))
         {
#if SIZEOF_OFF_T == 4
            if (command(control_fd, "REST %ld", seek) != SUCCESS)
#else
            if (command(control_fd, "REST %lld", seek) != SUCCESS)
#endif
            {
               (void)close(sock_fd);
               return(INCORRECT);
            }
            if ((reply = get_reply()) < 0)
            {
               if (timeout_flag == OFF)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, msg_str,
                            "ftp_data(): Failed to get proper reply for REST command (%d).",
                            reply);
               }
               else
               {
                  (void)close(sock_fd);
                  return(INCORRECT);
               }
            }
            else
            {
               if (reply != 350)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, msg_str,
                            "ftp_data(): Failed to get proper reply for REST command (%d).",
                            reply);
               }
            }
         }

         if (command(control_fd, "%s %s", cmd, filename) != SUCCESS)
         {
            (void)close(sock_fd);
            return(INCORRECT);
         }
         if ((reply = get_reply()) < 0)
         {
            if (timeout_flag == OFF)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, msg_str,
                         "ftp_data(): Failed to get proper reply (%d).", reply);
            }
            (void)close(sock_fd);
            return(INCORRECT);
         }
      } while ((ret = check_data_socket(reply, sock_fd, &retries, filename)) == 1);

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
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_data(): Failed to set signal handler : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }
      if (setjmp(env_alrm) != 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_data(): accept() timeout");
         timeout_flag = ON;
         (void)close(sock_fd);
         return(INCORRECT);
      }
      (void)alarm(2 * transfer_timeout);

      if ((new_sock_fd = accept(sock_fd, (struct sockaddr *) &from,
                                &length)) < 0)
      {
         (void)alarm(0); /* Maybe it was a real accept() error */
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_data(): accept() error : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }
      (void)alarm(0);
      if (close(sock_fd) == -1)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_data(): close() error : %s", strerror(errno));
      }
   } /* ACTIVE_MODE */

#ifdef FTX
   l.l_onoff = 1; l.l_linger = 240;
   if (setsockopt(new_sock_fd, SOL_SOCKET, SO_LINGER,
                  (char *)&l, sizeof(struct linger)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "ftp_data(): setsockopt() error : %s", strerror(errno));
      (void)close(new_sock_fd);
      return(INCORRECT);
   }
#endif

#if defined (_WITH_TOS) && defined (IP_TOS) && defined (IPTOS_THROUGHPUT)
   tos = IPTOS_THROUGHPUT;
   if (setsockopt(new_sock_fd, IPPROTO_IP, IP_TOS, (char *)&tos,
                  sizeof(tos)) < 0)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                "ftp_data(): setsockopt() IP_TOS error : %s", strerror(errno));
   }
#endif

   data_fd = new_sock_fd;

   return(SUCCESS);
}


#ifdef WITH_SSL
/*########################### ftp_auth_data() ###########################*/
int
ftp_auth_data(void)
{
   return(encrypt_data_connection(data_fd));
}
#endif


/*+++++++++++++++++++++++++ check_data_socket() +++++++++++++++++++++++++*/
static int
check_data_socket(int reply, int sock_fd, int *retries, char *filename)
{
   /*
    * The replies for STOR and APPE should be the same.
    */
   if ((reply != 150) && (reply != 125) && (reply != 120) &&
       (reply != 250) && (reply != 200))
   {
      if (close(sock_fd) == -1)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                   "check_data_socket(): close() error : %s", strerror(errno));
      }

      if (((reply == 553) && (posi(&msg_str[3], "(Overwrite)") != NULL)) ||
          ((reply == 550) && (posi(&msg_str[3], "Overwrite permission denied") != NULL)))
      {
         if (*retries < MAX_DATA_CONNECT_RETRIES)
         {
            if (ftp_dele(filename) == SUCCESS)
            {  
               (*retries)++;
               return(1);
            }
         }
         return(-2);
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


#ifdef WITH_SSL
/*++++++++++++++++++++++ encrypt_data_connection() ++++++++++++++++++++++*/
static int
encrypt_data_connection(int fd)
{
   int reply;

   ssl_data = (SSL *)SSL_new(ssl_ctx);
   SSL_set_connect_state(ssl_data);
   SSL_set_fd(ssl_data, fd);
   SSL_copy_session_id(ssl_data, ssl_con);
   if ((reply = SSL_connect(ssl_data)) <= 0)
   {
      char *ptr;

      ptr = ssl_error_msg("SSL_connect", ssl_con, reply, msg_str);
      reply = SSL_get_verify_result(ssl_con);
      if (reply == X509_V_ERR_CRL_SIGNATURE_FAILURE)
      {
         (void)strcpy(ptr,
                      " | Verify result: The signature of the certificate is invalid!");
      }
      else if (reply == X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD)
           {
               (void)strcpy(ptr,
                            " | Verify result: The CRL nextUpdate field contains an invalid time.");
           }
      else if (reply == X509_V_ERR_CRL_HAS_EXPIRED)
           {
               (void)strcpy(ptr,
                            " | Verify result: The CRL has expired.");
           }
      else if (reply == X509_V_ERR_CERT_REVOKED)
           {
              (void)strcpy(ptr,
                           " | Verify result: Certificate revoked.");
           }
      else if (reply > X509_V_OK)
           {
              (void)sprintf(ptr, " | Verify result: %d", reply);
           }
      reply = INCORRECT;
   }
   else
   {
      char       *ssl_version;
      int        length,
                 ssl_bits;
      SSL_CIPHER *ssl_cipher;
      X509       *x509_ssl_con,
                 *x509_ssl_data;

      ssl_version = SSL_get_cipher_version(ssl_data);
      ssl_cipher = SSL_get_current_cipher(ssl_data);
      SSL_CIPHER_get_bits(ssl_cipher, &ssl_bits);
      length = strlen(msg_str);
      (void)sprintf(&msg_str[length], "  <%s, cipher %s, %d bits>",
                    ssl_version, SSL_CIPHER_get_name(ssl_cipher),
                    ssl_bits);

      /* Get server certifcates for ctrl and data connection. */
      x509_ssl_con = SSL_get_peer_certificate(ssl_con);
      x509_ssl_data = SSL_get_peer_certificate(ssl_data);

      /* Now check if we did get a server certificate and */
      /* if it matches with that from ctrl connection.    */
      if ((x509_ssl_con != NULL) && (x509_ssl_data == NULL))
      {
         (void)strcpy(msg_str,
                      "Server did not present a certificate for data connection.");
         SSL_free(ssl_data);
         ssl_data = NULL;
         (void)close(fd);
         reply = INCORRECT;
      }
      else if ((x509_ssl_con == NULL) &&
               ((x509_ssl_data == NULL) || (x509_ssl_data != NULL)))
           {
              (void)sprintf(msg_str,
                            "Failed to compare server certificates for control and data connection (%d).",
                            reply);
              SSL_free(ssl_data);
              ssl_data = NULL;
              (void)close(fd);
              reply = INCORRECT;
           }
           else
           {
              if (X509_cmp(x509_ssl_con, x509_ssl_data))
              {
                 SSL_free(ssl_data);
                 ssl_data = NULL;
                 (void)close(fd);
                 reply = INCORRECT;
              }
              else
              {
                 reply = SUCCESS;
              }
           }
      X509_free(x509_ssl_con);
      X509_free(x509_ssl_data);
   }

   /* The error or encryption info is in msg_str. */
   return(reply);
}
#endif /* WITH_SSL */


#ifdef _BLOCK_MODE
/*############################## ftp_open() #############################*/
int
ftp_open(char *filename, int seek)
{
   int reply;

   if (seek != 0)
   {
      reply = command(control_fd, "REST %d", seek);
      if ((reply != SUCCESS) || ((reply = get_reply()) < 0))
      {
         return(INCORRECT);
      }

      if (reply != 350)
      {
         return(reply);
      }
   }

   reply = command(control_fd, "STOR %s", filename);
   if ((reply != SUCCESS) || ((reply = get_reply()) < 0))
   {
      return(INCORRECT);
   }

   if ((reply != 125) && (reply != 150) && (reply != 120) &&
       (reply != 250) && (reply != 200))
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

#ifdef WITH_SSL
   if (ssl_data != NULL)
   {
      SSL_free(ssl_data);
      ssl_data = NULL;
   }
#endif
#ifdef _WITH_SHUTDOWN
   if (shutdown(data_fd, 1) < 0)
   {
      trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                "ftp_close_data(): shutdown() error : %s", strerror(errno));
   }
#endif

   if (close(data_fd) == -1)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "ftp_close_data(): close() error : %s", strerror(errno));
      return(INCORRECT);
   }
   data_fd = -1;

   if (timeout_flag == OFF)
   {
      long tmp_ftp_timeout = transfer_timeout;

      /*
       * Since there are so many timeouts on slow lines when closing
       * the data connection lets double the timeout and see if this
       * helps.
       */
      transfer_timeout += transfer_timeout;
      if ((reply = get_reply()) < 0)
      {
         transfer_timeout = tmp_ftp_timeout;
         return(INCORRECT);
      }
      transfer_timeout = tmp_ftp_timeout;
      if ((reply != 226) && (reply != 250))
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

#ifdef WITH_SSL
           if (ssl_data == NULL)
           {
#endif
              if ((status = write(data_fd, ptr, size)) != size)
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "ftp_write(): write() error (%d) : %s",
                           status, strerror(errno));
                 return(errno);
              }
#ifdef WITH_SSL
           }
           else
           {
              if (ssl_write(ssl_data, ptr, size) != size)
              {
                 return(INCORRECT);
              }
           }
#endif
#ifdef WITH_TRACE
           trace_log(NULL, 0, BIN_W_TRACE, ptr, size, NULL);
#endif
        }
   else if (status < 0)
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "ftp_write(): select() error : %s", strerror(errno));
           return(INCORRECT);
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "ftp_write(): Unknown condition.");
           return(INCORRECT);
        }
   
   return(SUCCESS);
}


#ifdef WITH_SENDFILE
/*############################ ftp_sendfile() ###########################*/
int
ftp_sendfile(int source_fd, off_t *offset, int size)
{
   int    nleft,
          nwritten;
   fd_set wset;

   nleft = size;
   size = 0;
   FD_ZERO(&wset);
   while (nleft > 0)
   {
      FD_SET(data_fd, &wset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      nwritten = select(data_fd + 1, NULL, &wset, NULL, &timeout);

      if (FD_ISSET(data_fd, &wset))
      {
         if ((nwritten = sendfile(data_fd, source_fd, offset, nleft)) > 0)
         {
            nleft -= nwritten;
            size += nwritten;
         }
         else if (nwritten == 0)
              {
                 nleft = 0;
              }
              else
              {
                 if (errno == ECONNRESET)
                 {
                    timeout_flag = CON_RESET;
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "ftp_sendfile(): sendfile() error (%d) : %s",
                           nwritten, strerror(errno));
                 return(-errno);
              }
      }
      else if (nwritten == 0)
           {
              /* timeout has arrived */
              timeout_flag = ON;
              return(INCORRECT);
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "ftp_sendfile(): select() error : %s", strerror(errno));
              return(INCORRECT);
           }
   }
   
   return(size);
}
#endif /* WITH_SENDFILE */


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
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "ftp_block_write(): Failed to write() descriptor of block header : %s",
                        strerror(errno));
              return(INCORRECT);
           }

           /* Write byte counter of block header */
           if (write(data_fd, &size, 2) != 2)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "ftp_block_write(): Failed to write() byte counter of block header : %s",
                        strerror(errno));
              return(INCORRECT);
           }

           /* Now write the data */
           if (write(data_fd, block, size) != size)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "ftp_block_write(): write() error : %s", strerror(errno));
              return(INCORRECT);
           }
        }
        else if (status < 0)
             {
                trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                          "ftp_block_write(): select() error : %s", strerror(errno));
                return(INCORRECT);
             }
             else
             {
                trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                          "ftp_block_write(): Unknown condition.");
                return(INCORRECT);
             }
   
   return(SUCCESS);
}


/*############################## ftp_mode() #############################*/
int
ftp_mode(char mode)
{
   int reply;

   if ((reply = command(control_fd, "MODE %c", mode)) == SUCCESS)
   {
      if ((reply = get_reply()) != INCORRECT)
      {
         if (reply == 200)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
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
#ifdef WITH_SSL
           if (ssl_data == NULL)
           {
#endif
              if ((bytes_read = read(data_fd, block, blocksize)) == -1)
              {
                 if (errno == ECONNRESET)
                 {
                    timeout_flag = CON_RESET;
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "ftp_read(): read() error : %s", strerror(errno));
                 return(INCORRECT);
              }
#ifdef WITH_SSL
           }
           else
           {
              if ((bytes_read = SSL_read(ssl_data, block,
                                         blocksize)) == INCORRECT)
              {
                 if ((status = SSL_get_error(ssl_data,
                                             bytes_read)) == SSL_ERROR_SYSCALL)
                 {
                    if (errno == ECONNRESET)
                    {
                       timeout_flag = CON_RESET;
                    }
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                              "ftp_read(): SSL_read() error : %s",
                              strerror(errno));
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                              "ftp_read(): SSL_read() error %d", status);
                 }
                 return(INCORRECT);
              }
           }
#endif
#ifdef WITH_TRACE
           trace_log(NULL, 0, BIN_R_TRACE, block, bytes_read, NULL);
#endif
        }
   else if (status < 0)
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "ftp_read(): select() error : %s", strerror(errno));
           return(INCORRECT);
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "ftp_read(): Unknown condition.");
           return(INCORRECT);
        }

   return(bytes_read);
}


/*############################## ftp_quit() #############################*/
int
ftp_quit(void)
{
   int reply;

   (void)command(control_fd, "QUIT");
   if (data_fd != -1)
   {
#ifdef WITH_SSL
      if (ssl_data != NULL)
      {
         SSL_free(ssl_data);
         ssl_data = NULL;
      }
#endif
#ifdef _WITH_SHUTDOWN
      if (shutdown(data_fd, 1) < 0)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_quit(): shutdown() error : %s", strerror(errno));
      }
#endif /* _WITH_SHUTDOWN */
      if (close(data_fd) == -1)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_quit(): close() error : %s", strerror(errno));
      }
   }

   /*
    * If timeout_flag is ON, lets NOT check the reply from
    * the QUIT command. Else we are again waiting 'transfer_timeout'
    * seconds for the reply!
    */
   if (timeout_flag == OFF)
   {
      if ((reply = get_reply()) < 0)
      {
         (void)close(control_fd);
         return(INCORRECT);
      }

      /*
       * NOTE: Lets not count the 421 warning as an error.
       */
      if ((reply != 221) && (reply != 421))
      {
         (void)close(control_fd);
         return(reply);
      }
#ifdef _WITH_SHUTDOWN
      if (shutdown(control_fd, 1) < 0)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                   "ftp_quit(): shutdown() error : %s", strerror(errno));
      }
#endif /* _WITH_SHUTDOWN */
   }
#ifdef WITH_SSL
   if (ssl_con != NULL)
   {
      SSL_free(ssl_con);
      ssl_con = NULL;
   }
#endif
   if (close(control_fd) == -1)
   {
      trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                "ftp_quit(): close() error : %s", strerror(errno));
   }

   return(SUCCESS);
}


/*############################ ftp_get_reply() ##########################*/
int
ftp_get_reply(void)
{
   int  tmp_timeout_flag = OFF,
        reply;
   long tmp_transfer_timeout = 0L;

   if (transfer_timeout > 20L)
   {
      tmp_transfer_timeout = transfer_timeout;
      transfer_timeout = 20L;
   }
   if (timeout_flag == ON)
   {
      tmp_timeout_flag = ON;
   }
   reply = get_reply();
   if ((timeout_flag == ON) && (tmp_timeout_flag == OFF))
   {
      timeout_flag = ON;
   }
   if (tmp_transfer_timeout > 0L)
   {
      transfer_timeout = tmp_transfer_timeout;
   }

   return(reply);
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
      if ((isdigit((int)msg_str[0]) != 0) && (isdigit((int)msg_str[1]) != 0) &&
          (isdigit((int)msg_str[2]) != 0) && (msg_str[3] != '-'))
      {
         break;
      }
   }

   return(((msg_str[0] - '0') * 100) + ((msg_str[1] - '0') * 10) +
          (msg_str[2] - '0'));
}


/*+++++++++++++++++++++++++++ read_data_line() ++++++++++++++++++++++++++*/
static int
#ifdef WITH_SSL
read_data_line(int read_fd, SSL *ssl, char *line)
#else
read_data_line(int read_fd, char *line)
#endif
{
   int    bytes_buffered = 0,
          bytes_read = 0,
          status;
   char   *read_ptr = line;
   fd_set rset;

   FD_ZERO(&rset);
   for (;;)
   {
      if (bytes_read <= 0)
      {
         /* Initialise descriptor set */
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
#ifdef WITH_SSL
                 if (ssl == NULL)
                 {
#endif
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
                       }
                       else
                       {
                          if (errno == ECONNRESET)
                          {
                             timeout_flag = CON_RESET;
                          }
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                    "read_data_line(): read() error (after reading %d Bytes) : %s",
                                    bytes_buffered, strerror(errno));
                       }
                       return(bytes_read);
                    }
#ifdef WITH_SSL
                 }
                 else
                 {
                    if ((bytes_read = SSL_read(ssl, &line[bytes_buffered],
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
                       }
                       else
                       {
                          if ((status = SSL_get_error(ssl,
                                                      bytes_read)) == SSL_ERROR_SYSCALL)
                          {
                             if (errno == ECONNRESET)
                             {
                                timeout_flag = CON_RESET;
                             }
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                       "read_data_line(): SSL_read() error (after reading %d Bytes) : %s",
                                       bytes_buffered, strerror(errno));
                          }
                          else
                          {
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                       "read_data_line(): SSL_read() error (after reading %d Bytes) (%d)",
                                       bytes_buffered, status);
                          }
                       }
                       return(bytes_read);
                    }
                 }
#endif
#ifdef WITH_TRACE
                 trace_log(NULL, 0, BIN_R_TRACE,
                           &line[bytes_buffered], bytes_read, NULL);
#endif
                 read_ptr = &line[bytes_buffered];
                 bytes_buffered += bytes_read;
              }
         else if (status < 0)
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "read_data_line(): select() error : %s", strerror(errno));
                 return(INCORRECT);
              }
              else
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "read_data_line(): Unknown condition.");
                 return(INCORRECT);
              }
      }

      /* Evaluate what we have read. */
      do
      {
         if ((*read_ptr == '\n') && (*(read_ptr - 1) == '\r'))
         {
            *(read_ptr - 1) = '\0';
            return(bytes_buffered);
         }
         read_ptr++;
         bytes_read--;
      } while(bytes_read > 0);
   } /* for (;;) */
}


/*+++++++++++++++++++++++++ read_data_to_buffer() +++++++++++++++++++++++*/
static int
#ifdef WITH_SSL
read_data_to_buffer(int read_fd, SSL *ssl, char ***buffer)
#else
read_data_to_buffer(int read_fd, char ***buffer)
#endif
{
   int    bytes_buffered = 0,
          bytes_read = 0,
          status;
   fd_set rset;
   char   tmp_buffer[MAX_RET_MSG_LENGTH];

   FD_ZERO(&rset);
   for (;;)
   {
      /* Initialise descriptor set */
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
#ifdef WITH_SSL
              if (ssl == NULL)
              {
#endif
                 if ((bytes_read = read(read_fd, tmp_buffer,
                                        MAX_RET_MSG_LENGTH)) < 1)
                 {
                    if (bytes_read == -1)
                    {
                       if (errno == ECONNRESET)
                       {
                          timeout_flag = CON_RESET;
                       }
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                 "read_data_to_buffer(): read() error (after reading %d Bytes) : %s",
                                 bytes_buffered, strerror(errno));
                    }
                    else
                    {
                       if (bytes_buffered > 0)
                       {
                          *(**buffer + bytes_buffered) = '\0';
                       }
                       bytes_read = bytes_buffered;
                    }
                    return(bytes_read);
                 }
#ifdef WITH_SSL
              }
              else
              {
                 if ((bytes_read = SSL_read(ssl, tmp_buffer,
                                            MAX_RET_MSG_LENGTH)) < 1)
                 {
                    if (bytes_read == -1)
                    {
                       if ((status = SSL_get_error(ssl,
                                                   bytes_read)) == SSL_ERROR_SYSCALL)
                       {
                          if (errno == ECONNRESET)
                          {
                             timeout_flag = CON_RESET;
                          }
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                    "read_data_to_buffer(): SSL_read() error (after reading %d Bytes) : %s",
                                    bytes_buffered, strerror(errno));
                       }
                       else
                       {
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                    "read_data_to_buffer(): SSL_read() error (after reading %d Bytes) (%d)",
                                    bytes_buffered, status);
                       }
                       bytes_buffered = INCORRECT;
                    }
                    else
                    {
                       if (bytes_buffered > 0)
                       {
                          *(**buffer + bytes_buffered) = '\0';
                       }
                    }
                    return(bytes_buffered);
                 }
              }
#endif
#ifdef WITH_TRACE
              trace_log(NULL, 0, BIN_R_TRACE, tmp_buffer, bytes_read, NULL);
#endif
              if (**buffer == NULL)
              {
                 if ((**buffer = malloc(bytes_read + 1)) == NULL)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                              "read_data_to_buffer(): malloc() error : %s", strerror(errno));
                    return(INCORRECT);
                 }
              }
              else
              {
                 if ((**buffer = realloc(**buffer,
                                         bytes_buffered + bytes_read + 1)) == NULL)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                              "read_data_to_buffer(): realloc() error (after reading %d Bytes) : %s",
                              bytes_buffered, strerror(errno));
                    return(INCORRECT);
                 }
              }
              (void)memcpy(**buffer + bytes_buffered, tmp_buffer, bytes_read);
              bytes_buffered += bytes_read;
           }
      else if (status < 0)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "read_data_to_buffer(): select() error : %s", strerror(errno));
              return(INCORRECT);
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "Unknown condition.");
              return(INCORRECT);
           }
   } /* for (;;) */
}


/*+++++++++++++++++++++++++++ read_stat_data() ++++++++++++++++++++++++++*/
static int
read_stat_data(void)
{
   int    bytes_buffered = 0,
          bytes_read = 0,
          status;
   char   *read_ptr = msg_str;
   fd_set rset;

   FD_ZERO(&rset);
   for (;;)
   {
      /* Initialise descriptor set */
      FD_SET(control_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      /* Wait for message x seconds and then continue. */
      status = select(control_fd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         /* timeout has arrived */
         timeout_flag = ON;
         return(INCORRECT);
      }
      else if (FD_ISSET(control_fd, &rset))
           {
#ifdef WITH_SSL
              if (ssl_con == NULL)
              {
#endif
                 if ((bytes_read = read(control_fd, &msg_str[bytes_buffered],
                                        (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                 {
                    if (bytes_read == 0)
                    {
                       /*
                        * Due to security reasons some systems do not
                        * return any data here. So lets not count this
                        * as a remote hangup.
                        */
                       msg_str[bytes_buffered] = '\0';
                    }
                    else
                    {
                       if (errno == ECONNRESET)
                       {
                          timeout_flag = CON_RESET;
                       }
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                 "read_stat_data(): read() error (after reading %d Bytes) : %s",
                                 bytes_buffered, strerror(errno));
                    }
                    return(bytes_read);
                 }
#ifdef WITH_SSL
              }
              else
              {
                 if ((bytes_read = SSL_read(ssl_con, &msg_str[bytes_buffered],
                                            (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                 {
                    if (bytes_read == 0)
                    {
                       /*
                        * Due to security reasons some systems do not
                        * return any data here. So lets not count this
                        * as a remote hangup.
                        */
                       msg_str[bytes_buffered] = '\0';
                    }
                    else
                    {
                       if ((status = SSL_get_error(ssl_con,
                                                   bytes_read)) == SSL_ERROR_SYSCALL)
                       {
                          if (errno == ECONNRESET)
                          {
                             timeout_flag = CON_RESET;
                          }
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                    "read_stat_data(): SSL_read() error (after reading %d Bytes) : %s",
                                    bytes_buffered, strerror(errno));
                       }
                       else
                       {
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                    "read_stat_data(): SSL_read() error (after reading %d Bytes) (%d)",
                                    bytes_buffered, status);
                       }
                    }
                    return(bytes_read);
                 }
              }
#endif
#ifdef WITH_TRACE
              trace_log(NULL, 0, BIN_R_TRACE,
                        &msg_str[bytes_buffered], bytes_read, NULL);
#endif
              bytes_buffered += bytes_read;
              read_ptr = &msg_str[bytes_buffered];
           }
      else if (status < 0)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "read_stat_data(): select() error : %s",
                        strerror(errno));
              return(INCORRECT);
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "read_stat_data(): Unknown condition.");
              return(INCORRECT);
           }

      /* Evaluate what we have read. */
      if ((bytes_read > 1) &&
          (*(read_ptr - 1) == '\n') && (*(read_ptr - 2) == '\r') &&
          (isdigit((int)msg_str[0]) != 0) && (isdigit((int)msg_str[1]) != 0) &&
          (isdigit((int)msg_str[2]) != 0) && (msg_str[3] != '-'))
      {
         int gotcha = YES;

         if ((msg_str[0] == '2') && (msg_str[1] == '1') &&
             (msg_str[2] == '1'))
         {
            if (posi(&msg_str[3], "\r\n211 ") == NULL)
            {
               gotcha = NO;
            }
         }
         if (gotcha == YES)
         {
            *(read_ptr - 2) = '\0';
            return(((msg_str[0] - '0') * 100) + ((msg_str[1] - '0') * 10) +
                   (msg_str[2] - '0'));
         }
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
#ifdef WITH_SSL
                 if (ssl_con == NULL)
                 {
#endif
                    if ((bytes_read = read(control_fd, &msg_str[bytes_buffered],
                                           (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                    {
                       if (bytes_read == 0)
                       {
                          trans_log(ERROR_SIGN,  __FILE__, __LINE__, NULL,
                                    "read_msg(): Remote hang up.");
                          timeout_flag = NEITHER;
                       }
                       else
                       {
                          if (errno == ECONNRESET)
                          {
                             timeout_flag = CON_RESET;
                          }
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                    "read_msg(): read() error (after reading %d Bytes) : %s",
                                    bytes_buffered, strerror(errno));
                          bytes_read = 0;
                       }
                       return(INCORRECT);
                    }
#ifdef WITH_SSL
                 }
                 else
                 {
                    if ((bytes_read = SSL_read(ssl_con,
                                               &msg_str[bytes_buffered],
                                               (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                    {
                       if (bytes_read == 0)
                       {
                          trans_log(ERROR_SIGN,  __FILE__, __LINE__, NULL,
                                    "read_msg(): Remote hang up.");
                          timeout_flag = NEITHER;
                       }
                       else
                       {
                          if ((status = SSL_get_error(ssl_con,
                                                      bytes_read)) == SSL_ERROR_SYSCALL)
                          {
                             if (errno == ECONNRESET)
                             {
                                timeout_flag = CON_RESET;
                             }
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                       "read_msg(): SSL_read() error (after reading %d Bytes) : %s",
                                       bytes_buffered, strerror(errno));
                          }
                          else
                          {
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                       "read_msg(): SSL_read() error (after reading %d Bytes) (%d)",
                                       bytes_buffered, status);
                          }
                          bytes_read = 0;
                       }
                       return(INCORRECT);
                    }
                 }
#endif
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
                           "read_msg(): select() error : %s", strerror(errno));
                 return(INCORRECT);
              }
              else
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "read_msg(): Unknown condition.");
                 return(INCORRECT);
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


/*++++++++++++++++++++++++++++ get_number() +++++++++++++++++++++++++++++*/
static int
get_number(char **ptr, char end_char)
{
   int length = 0,
       number = INCORRECT;

   (*ptr)++;
   do
   {
      if (isdigit((int)(**ptr)))
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

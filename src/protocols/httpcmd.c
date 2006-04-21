/*
 *  httpcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   httpcmd - commands to send and retrieve files via HTTP
 **
 ** SYNOPSIS
 **   int  http_connect(char *hostname, int port, int sndbuf_size, int rcvbuf_size)
 **   int  http_ssl_auth(void)
 **   int  http_check_connection(void)
 **   int  http_del(char *host, char *path, char *filename)
 **   int  http_get(char *host, char *path, char *filename)
 **   int  http_put(char *host, char *path, char *filename, off_t length)
 **   int  http_read(char *block, int blocksize)
 **   int  http_chunk_read(char **chunk, int *chunksize)
 **   int  http_write(char *block, char *buffer, int size)
 **   void http_quit(void)
 **
 ** DESCRIPTION
 **   ftpcmd provides a set of commands to communicate with an HTTP
 **   server via BSD sockets.
 **
 **
 ** RETURN VALUES
 **   Returns SUCCESS when successful. When an error has occurred
 **   it will either return INCORRECT or the three digit HTTP reply
 **   code when the reply of the server does not conform to the
 **   one expected. The complete reply string of the HTTP server
 **   is returned to 'msg_str'. 'timeout_flag' is just a flag to
 **   indicate that the 'transfer_timeout' time has been reached.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.04.2003 H.Kiehl Created
 **   25.12.2003 H.Kiehl Added traceing.
 **   28.04.2004 H.Kiehl Handle chunked reading.
 */
DESCR__E_M3


#include <stdio.h>        /* fprintf(), fdopen(), fclose()               */
#include <stdarg.h>       /* va_start(), va_arg(), va_end()              */
#include <string.h>       /* memset(), memcpy(), strcpy()                */
#include <stdlib.h>       /* strtoul()                                   */
#include <ctype.h>        /* isdigit()                                   */
#include <sys/types.h>    /* fd_set                                      */
#include <sys/time.h>     /* struct timeval                              */
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
#include "httpdefs.h"
#include "commondefs.h"

#ifdef WITH_SSL
SSL                              *ssl_con = NULL;
#endif

/* External global variables */
extern int                       timeout_flag;
extern char                      msg_str[];
#ifdef LINUX
extern char                      *h_errlist[];  /* for gethostbyname()   */
extern int                       h_nerr;        /* for gethostbyname()   */
#endif
extern long                      transfer_timeout;

/* Local global variables */
static int                       http_fd;
#ifdef WITH_SSL
static SSL_CTX                   *ssl_ctx;
#endif
static struct timeval            timeout;
static struct http_message_reply hmr;

/* Local function prototypes */
static int                       basic_authentication(void),
                                 get_http_reply(int *),
                                 read_msg(int *);


/*########################## http_connect() #############################*/
int
#ifdef WITH_SSL
http_connect(char *hostname, int port, char *user, char *passwd, int ssl, int sndbuf_size, int rcvbuf_size)
#else
http_connect(char *hostname, int port, char *user, char *passwd, int sndbuf_size, int rcvbuf_size)
#endif
{
   struct sockaddr_in sin;

   (void)memset((struct sockaddr *) &sin, 0, sizeof(sin));
   if ((sin.sin_addr.s_addr = inet_addr(hostname)) != -1)
   {
      sin.sin_family = AF_INET;
   }
   else
   {
      register struct hostent *p_host = NULL;

      if ((p_host = gethostbyname(hostname)) == NULL)
      {
#if !defined (_HPUX) && !defined (_SCO)
         if (h_errno != 0)
         {
#ifdef LINUX
            if ((h_errno > 0) && (h_errno < h_nerr))
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "http_connect(): Failed to gethostbyname() %s : %s",
                         hostname, h_errlist[h_errno]);
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "http_connect(): Failed to gethostbyname() %s (h_errno = %d) : %s",
                         hostname, h_errno, strerror(errno));
            }
#else
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "http_connect(): Failed to gethostbyname() %s (h_errno = %d) : %s",
                      hostname, h_errno, strerror(errno));
#endif
         }
         else
         {
#endif /* !_HPUX && !_SCO */
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "http_connect(): Failed to gethostbyname() %s : %s",
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

   if ((http_fd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "http_connect(): socket() error : %s", strerror(errno));
      return(INCORRECT);
   }

   sin.sin_port = htons((u_short)port);

   if (sndbuf_size > 0)
   {
      if (setsockopt(http_fd, SOL_SOCKET, SO_SNDBUF,
                     (char *)&sndbuf_size, sizeof(sndbuf_size)) < 0)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                   "http_connect(): setsockopt() error : %s", strerror(errno));
      }
      hmr.sndbuf_size = sndbuf_size;
   }
   else
   {
      hmr.sndbuf_size = 0;
   }
   if (rcvbuf_size > 0)
   {
      if (setsockopt(http_fd, SOL_SOCKET, SO_RCVBUF,
                     (char *)&rcvbuf_size, sizeof(rcvbuf_size)) < 0)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                   "http_connect(): setsockopt() error : %s", strerror(errno));
      }
      hmr.rcvbuf_size = rcvbuf_size;
   }
   else
   {
      hmr.rcvbuf_size = 0;
   }

   if (connect(http_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "http_connect(): Failed to connect() to %s : %s",
                hostname, strerror(errno));
      (void)close(http_fd);
      return(INCORRECT);
   }

   (void)strcpy(hmr.hostname, hostname);
   if (user != NULL)
   {
      (void)strcpy(hmr.user, user);
   }
   if (passwd != NULL)
   {
      (void)strcpy(hmr.passwd, passwd);
   }
   if ((user[0] != '\0') || (passwd[0] != '\0'))
   {
      if (basic_authentication() != SUCCESS)
      {
         return(INCORRECT);
      }
   }
   hmr.port = port;
   hmr.free = YES;

#ifdef WITH_SSL
   if ((ssl == YES) || (ssl == BOTH))
   {
      int  reply;
      char *p_env,
           *p_env1;

      hmr.ssl = YES;
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
      SSL_set_fd(ssl_con, http_fd);

      if ((reply = SSL_connect(ssl_con)) <= 0)
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
      return(reply);
   }
   else
   {
      hmr.ssl = NO;
   }
#endif /* WITH_SSL */

   return(SUCCESS);
}


/*############################## http_get() #############################*/
int
http_get(char *host, char *path, char *filename, off_t *content_length)
{
   int reply;

   hmr.bytes_read = 0;
   hmr.chunked = NO;

retry_get:
   if ((reply = command(http_fd,
                        "GET %s%s%s HTTP/1.1\r\nUser-Agent: AFD/%s\r\n%sHost: %s\r\nAccept: *\r\n",
                        (*path != '/') ? "/" : "", path, filename,
                        PACKAGE_VERSION,
                        (hmr.authorization == NULL) ? "" : hmr.authorization,
                        host)) == SUCCESS)
   {
      if ((reply = get_http_reply(&hmr.bytes_buffered)) == 200)
      {
         reply = SUCCESS;
         *content_length = hmr.content_length;
      }
      else if (reply == 401)
           {
              if (hmr.www_authenticate == WWW_AUTHENTICATE_BASIC)
              {
                 if (basic_authentication() == SUCCESS)
                 {
                    if (http_check_connection() > INCORRECT)
                    {
                       goto retry_get;
                    }
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "Failed to create basic authentication.");
              }
              else if (hmr.www_authenticate == WWW_AUTHENTICATE_DIGEST)
                   {
                      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                "Digest authentication not yet implemented.");
                   }
           }
   }
   return(reply);
}


/*############################## http_put() #############################*/
int
http_put(char *host, char *path, char *filename, off_t length)
{
   int reply;

retry_put:
   if ((reply = command(http_fd,
#if SIZEOF_OFF_T == 4
                        "PUT %s%s%s HTTP/1.1\r\nUser-Agent: AFD/%s\r\nContent-length: %ld\r\n%sHost: %s\r\nControl: overwrite=1\r\n",
#else
                        "PUT %s%s%s HTTP/1.1\r\nUser-Agent: AFD/%s\r\nContent-length: %lld\r\n%sHost: %s\r\nControl: overwrite=1\r\n",
#endif
                        (*path != '/') ? "/" : "", path, filename,
                        PACKAGE_VERSION,
                        length,
                        (hmr.authorization == NULL) ? "" : hmr.authorization,
                        host)) == SUCCESS)
   {
      if ((reply = get_http_reply(NULL)) == 200)
      {
         reply = SUCCESS;
      }
      else if (reply == 401)
           {
              if (hmr.www_authenticate == WWW_AUTHENTICATE_BASIC)
              {
                 if (basic_authentication() == SUCCESS)
                 {
                    if (http_check_connection() > INCORRECT)
                    {
                       goto retry_put;
                    }
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "Failed to create basic authentication.");
              }
              else if (hmr.www_authenticate == WWW_AUTHENTICATE_DIGEST)
                   {
                      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                "Digest authentication not yet implemented.");
                   }
           }
   }
   return(reply);
}


/*############################## http_del() #############################*/
int
http_del(char *host, char *path, char *filename)
{
   int reply;

retry_del:
   if ((reply = command(http_fd,
                        "DELETE %s%s%s HTTP/1.1\r\nUser-Agent: AFD/%s\r\n%sHost: %s\r\n",
                        (*path != '/') ? "/" : "", path, filename,
                        PACKAGE_VERSION,
                        (hmr.authorization == NULL) ? "" : hmr.authorization,
                        host)) == SUCCESS)
   {
      if ((reply = get_http_reply(NULL)) == 200)
      {
         reply = SUCCESS;
      }
      else if (reply == 401)
           {
              if (hmr.www_authenticate == WWW_AUTHENTICATE_BASIC)
              {
                 if (basic_authentication() == SUCCESS)
                 {
                    if (http_check_connection() > INCORRECT)
                    {
                       goto retry_del;
                    }
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "Failed to create basic authentication.");
              }
              else if (hmr.www_authenticate == WWW_AUTHENTICATE_DIGEST)
                   {
                      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                "Digest authentication not yet implemented.");
                   }
           }
   }
   return(reply);
}


/*++++++++++++++++++++++++ basic_authentication() +++++++++++++++++++++++*/
static int
basic_authentication(void)
{
   size_t length;
   char   *dst_ptr,
          *src_ptr,
          base_64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
          userpasswd[MAX_USER_NAME_LENGTH + MAX_USER_NAME_LENGTH];

   /*
    * Let us first construct the authorization string from user name
    * and passwd:  <user>:<passwd>
    * And then encode this to using base-64 encoding.
    */
   length = sprintf(userpasswd, "%s:%s", hmr.user, hmr.passwd);
   if (hmr.authorization != NULL)
   {
      free(hmr.authorization);
   }
   if ((hmr.authorization = malloc(21 + length + (length / 3) + 2 + 1)) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "http_basic_authentication(): malloc() error : %s",
                strerror(errno));
      return(INCORRECT);
   }
   dst_ptr = hmr.authorization;
   *dst_ptr = 'A'; *(dst_ptr + 1) = 'u'; *(dst_ptr + 2) = 't';
   *(dst_ptr + 3) = 'h'; *(dst_ptr + 4) = 'o'; *(dst_ptr + 5) = 'r';
   *(dst_ptr + 6) = 'i'; *(dst_ptr + 7) = 'z'; *(dst_ptr + 8) = 'a';
   *(dst_ptr + 9) = 't'; *(dst_ptr + 10) = 'i'; *(dst_ptr + 11) = 'o';
   *(dst_ptr + 12) = 'n'; *(dst_ptr + 13) = ':'; *(dst_ptr + 14) = ' ';
   *(dst_ptr + 15) = 'B'; *(dst_ptr + 16) = 'a'; *(dst_ptr + 17) = 's';
   *(dst_ptr + 18) = 'i'; *(dst_ptr + 19) = 'c'; *(dst_ptr + 20) = ' ';
   dst_ptr += 21;
   src_ptr = userpasswd;
   while (length > 2)
   {
      *dst_ptr = base_64[(int)(*src_ptr) >> 2];
      *(dst_ptr + 1) = base_64[((((int)(*src_ptr) & 0x3)) << 4) | (((int)(*(src_ptr + 1)) & 0xF0) >> 4)];
      *(dst_ptr + 2) = base_64[((((int)(*(src_ptr + 1))) & 0xF) << 2) | ((((int)(*(src_ptr + 2))) & 0xC0) >> 6)];
      *(dst_ptr + 3) = base_64[((int)(*(src_ptr + 2))) & 0x3F];
      src_ptr += 3;
      length -= 3;
      dst_ptr += 4;
   }
   if (length == 2)
   {
      *dst_ptr = base_64[(int)(*src_ptr) >> 2];
      *(dst_ptr + 1) = base_64[((((int)(*src_ptr) & 0x3)) << 4) | (((int)(*(src_ptr + 1)) & 0xF0) >> 4)];
      *(dst_ptr + 2) = base_64[((((int)(*(src_ptr + 1))) & 0xF) << 2) | ((((int)(*(src_ptr + 2))) & 0xC0) >> 6)];
      *(dst_ptr + 3) = '=';
      dst_ptr += 4;
   }
   else if (length == 1)
        {
           *dst_ptr = base_64[(int)(*src_ptr) >> 2];
           *(dst_ptr + 1) = base_64[((((int)(*src_ptr) & 0x3)) << 4) | (((int)(*(src_ptr + 1)) & 0xF0) >> 4)];
           *(dst_ptr + 2) = '=';
           *(dst_ptr + 3) = '=';
           dst_ptr += 4;
        }
   *dst_ptr = '\r';
   *(dst_ptr + 1) = '\n';
   *(dst_ptr + 2) = '\0';

   return(SUCCESS);
}


/*############################# http_write() ############################*/
int
http_write(char *block, char *buffer, int size)
{
   char   *ptr = block;
   int    status;
   fd_set wset;

   /* Initialise descriptor set */
   FD_ZERO(&wset);
   FD_SET(http_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(http_fd + 1, NULL, &wset, NULL, &timeout);

   if (status == 0)
   {
      /* timeout has arrived */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (status < 0)
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "http_write(): select() error : %s", strerror(errno));
           return(INCORRECT);
        }
   else if (FD_ISSET(http_fd, &wset))
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

#ifdef WITH_SSL
           if (ssl_con == NULL)
           {
#endif
              if ((status = write(http_fd, ptr, size)) != size)
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "http_write(): write() error (%d) : %s",
                           status, strerror(errno));
                 return(errno);
              }
#ifdef WITH_SSL
           }
           else
           {
              if (ssl_write(ssl_con, ptr, size) != size)
              {
                 return(INCORRECT);
              }
           }
#endif
#ifdef WITH_TRACE
           trace_log(NULL, 0, BIN_W_TRACE, ptr, size, NULL);
#endif
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "http_write(): Unknown condition.");
           return(INCORRECT);
        }
   
   return(SUCCESS);
}


/*############################# http_read() #############################*/
int
http_read(char *block, int blocksize)
{
   int    bytes_read,
          offset,
          status;
   fd_set rset;

   if (hmr.bytes_buffered > 0)
   {
      if (hmr.bytes_buffered >= blocksize)
      {
         memcpy(block, msg_str, blocksize);
         if (hmr.bytes_buffered > blocksize)
         {
            hmr.bytes_buffered -= blocksize;
            (void)memmove(msg_str, msg_str + blocksize, hmr.bytes_buffered);
         }
         else
         {
            hmr.bytes_buffered = 0;
         }
         hmr.bytes_read = 0;
         return(blocksize);
      }
      else
      {
         memcpy(block, msg_str, hmr.bytes_buffered);
         offset = hmr.bytes_buffered;
         hmr.bytes_buffered = 0;
         hmr.bytes_read = 0;
      }
   }
   else
   {
      offset = 0;
   }

   /* Initialise descriptor set */
   FD_ZERO(&rset);
   FD_SET(http_fd, &rset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(http_fd + 1, &rset, NULL, NULL, &timeout);

   if (status == 0)
   {
      /* timeout has arrived */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (status < 0)
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "http_read(): select() error : %s", strerror(errno));
           return(INCORRECT);
        }
   else if (FD_ISSET(http_fd, &rset))
        {
#ifdef WITH_SSL
           if (ssl_con == NULL)
           {
#endif
              if ((bytes_read = read(http_fd, &block[offset],
                                     blocksize - offset)) == -1)
              {
                 if (errno == ECONNRESET)
                 {
                    timeout_flag = CON_RESET;
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "http_read(): read() error : %s", strerror(errno));
                 return(INCORRECT);
              }
#ifdef WITH_SSL
           }
           else
           {
              if ((bytes_read = SSL_read(ssl_con, &block[offset],
                                         blocksize - offset)) == INCORRECT)
              {
                 if ((status = SSL_get_error(ssl_con,
                                             bytes_read)) == SSL_ERROR_SYSCALL)
                 {
                    if (errno == ECONNRESET)
                    {
                       timeout_flag = CON_RESET;
                    }
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                              "http_read(): SSL_read() error : %s",
                              strerror(errno));
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                              "http_read(): SSL_read() error %d", status);
                 }
                 return(INCORRECT);
              }
           }
#endif
#ifdef WITH_TRACE
           trace_log(NULL, 0, BIN_R_TRACE, &block[offset], bytes_read, NULL);
#endif
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "http_read(): Unknown condition.");
           return(INCORRECT);
        }

   return(bytes_read + offset);
}


/*########################## http_chunk_read() ##########################*/
int
http_chunk_read(char **chunk, int *chunksize)
{
   int bytes_buffered,
       read_length;

   /* First, try read the chunk size. */
   if ((bytes_buffered = read_msg(&read_length)) != INCORRECT)
   {
      int    bytes_read,
             status,
             tmp_chunksize;
      fd_set rset;

      tmp_chunksize = (int)strtol(msg_str, NULL, 16);
      if (errno == ERANGE)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                   "http_chunk_read(): Failed to determine the chunk size with strtol() : %s",
                   strerror(errno));
         return(INCORRECT);
      }
      else
      {
         if (tmp_chunksize == 0)
         {
            return(HTTP_LAST_CHUNK);
         }
         tmp_chunksize += 2;
         if (tmp_chunksize > *chunksize)
         {
            if ((*chunk = realloc(*chunk, tmp_chunksize)) == NULL)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "http_chunk_read(): Failed to realloc() %d bytes : %s",
                         tmp_chunksize, strerror(errno));
               return(INCORRECT);
            }
            *chunksize = tmp_chunksize;
         }
         bytes_buffered -= (read_length + 1);
         if (tmp_chunksize > bytes_buffered)
         {
            (void)memcpy(*chunk, msg_str, bytes_buffered);
         }
         else
         {
            (void)memcpy(*chunk, msg_str, tmp_chunksize);
            hmr.bytes_read = tmp_chunksize;
            return(tmp_chunksize);
         }
      }

      FD_ZERO(&rset);
      do
      {
         FD_SET(http_fd, &rset);
         timeout.tv_usec = 0L;
         timeout.tv_sec = transfer_timeout;
   
         /* Wait for message x seconds and then continue. */
         status = select(http_fd + 1, &rset, NULL, NULL, &timeout);

         if (status == 0)
         {
            /* timeout has arrived */
            timeout_flag = ON;
            return(INCORRECT);
         }
         else if (status < 0)
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "http_chunk_read(): select() error : %s",
                           strerror(errno));
                 return(INCORRECT);
              }
         else if (FD_ISSET(http_fd, &rset))
              {
#ifdef WITH_SSL
                 if (ssl_con == NULL)
                 {
#endif
                    if ((bytes_read = read(http_fd, (*chunk + bytes_buffered),
                                           tmp_chunksize - bytes_buffered)) == -1)
                    {
                       if (errno == ECONNRESET)
                       {
                          timeout_flag = CON_RESET;
                       }
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                 "http_chunk_read(): read() error : %s",
                                 strerror(errno));
                       return(INCORRECT);
                    }
#ifdef WITH_SSL
                 }
                 else
                 {
                    if ((bytes_read = SSL_read(ssl_con, (*chunk + bytes_buffered),
                                               tmp_chunksize - bytes_buffered)) == INCORRECT)
                    {
                       if ((status = SSL_get_error(ssl_con,
                                                   bytes_read)) == SSL_ERROR_SYSCALL)
                       {
                          if (errno == ECONNRESET)
                          {
                             timeout_flag = CON_RESET;
                          }
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                    "http_chunk_read(): SSL_read() error : %s",
                                    strerror(errno));
                       }
                       else
                       {
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                    "http_chunk_read(): SSL_read() error %d",
                                    status);
                       }
                       return(INCORRECT);
                    }
                 }
#endif
                 if (bytes_read == 0)
                 {
                    /* Premature end, remote side has closed connection. */
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                              "http_chunk_read(): Remote side closed connection (expected: %d read: %d)",
                              tmp_chunksize, bytes_buffered);
                    return(INCORRECT);
                 }
#ifdef WITH_TRACE
                 trace_log(NULL, 0, BIN_R_TRACE, (*chunk + bytes_buffered),
                           bytes_read, NULL);
#endif
                 bytes_buffered += bytes_read;
              }
              else
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "http_chunk_read(): Unknown condition.");
                 return(INCORRECT);
              }
      } while (bytes_buffered < tmp_chunksize);

      if (bytes_buffered == tmp_chunksize)
      {
         bytes_buffered -= 2;
      }
   }
   return(bytes_buffered);
}


/*############################ http_quit() ##############################*/
void
http_quit(void)
{
   if ((hmr.authorization != NULL) && (hmr.free != NO))
   {
      free(hmr.authorization);
      hmr.authorization = NULL;
   }
   if (http_fd != -1)
   {
      if ((timeout_flag != ON) && (timeout_flag != CON_RESET))
      {
         if (shutdown(http_fd, 1) < 0)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                      "http_close(): shutdown() error : %s", strerror(errno));
         }
      }
#ifdef WITH_SSL
      if (ssl_con != NULL)
      {
         SSL_free(ssl_con);
         ssl_con = NULL;
      }
#endif
      if (close(http_fd) == -1)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                   "http_close(): close() error : %s", strerror(errno));
      }
      http_fd = -1;
   }
   return;
}


/*####################### http_check_connection() #######################*/
int
http_check_connection(void)
{
   int connection_closed;

   if (hmr.close == YES)
   {
      connection_closed = YES;
      hmr.free = NO;
      http_quit();
      hmr.free = YES;
   }
   else
   {
      /*
       * TODO: We still need the code to check if connection is still up!
       */
      connection_closed = NO;
   }
   if (connection_closed == YES)
   {
      int status;

      if ((status = http_connect(hmr.hostname, hmr.port,
#ifdef WITH_SSL
                                 NULL, NULL, hmr.ssl, hmr.sndbuf_size, hmr.rcvbuf_size)) != SUCCESS)
#else
                                 NULL, NULL, hmr.sndbuf_size, hmr.rcvbuf_size)) != SUCCESS)
#endif
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                   "HTTP connection to %s at port %d failed (%d).",
                   hmr.hostname, hmr.port, status);
         return(INCORRECT);
      }
      else
      {
         return(CONNECTION_REOPENED);
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++ get_http_reply() ++++++++++++++++++++++++++*/
static int
get_http_reply(int *ret_bytes_buffered)
{
   int bytes_buffered,
       read_length,
       status_code = INCORRECT;

   /* First read start line. */
   hmr.close = NO;
   hmr.bytes_buffered = 0;
   if (ret_bytes_buffered != NULL)
   {
      *ret_bytes_buffered = 0;
   }
   if ((bytes_buffered = read_msg(&read_length)) != INCORRECT)
   {
      if ((read_length > 12) &&
          (((msg_str[0] == 'H') || (msg_str[0] == 'h')) &&
           ((msg_str[1] == 'T') || (msg_str[1] == 't')) &&
           ((msg_str[2] == 'T') || (msg_str[2] == 't')) &&
           ((msg_str[3] == 'P') || (msg_str[3] == 'p')) &&
           (msg_str[4] == '/') &&
           (isdigit(msg_str[5])) && (msg_str[6] == '.') &&
           (isdigit(msg_str[7])) && (msg_str[8] == ' ') &&
           (isdigit(msg_str[9])) && (isdigit(msg_str[10])) &&
           (isdigit(msg_str[11]))))
      {
         hmr.http_version = ((msg_str[5] - '0') * 10) + (msg_str[7] - '0');
         status_code = ((msg_str[9] - '0') * 100) +
                       ((msg_str[10] - '0') * 10) +
                       (msg_str[11] - '0');
         if (read_length <= MAX_HTTP_HEADER_BUFFER)
         {
            (void)memcpy(hmr.msg_header, msg_str, read_length);
            hmr.header_length = read_length;
         }
         else
         {
            (void)memcpy(hmr.msg_header, msg_str, MAX_HTTP_HEADER_BUFFER - 1);
            hmr.msg_header[MAX_HTTP_HEADER_BUFFER - 1] = '\0';
            hmr.header_length = MAX_HTTP_HEADER_BUFFER;
         }

         /*
          * Now lets read the headers and lets store that what we
          * think might be usefull later.
          */
         for (;;)
         {
            if ((bytes_buffered = read_msg(&read_length)) == INCORRECT)
            {
               return(INCORRECT);
            }

            /* Check if we have reached header end. */
            if  ((read_length == 1) && (msg_str[0] == '\0'))
            {
               break;
            }

            /* Check for Content-Length header. */
            if ((read_length > 15) && (msg_str[14] == ':') &&
                ((msg_str[8] == 'L') || (msg_str[8] == 'l')) &&
                ((msg_str[9] == 'E') || (msg_str[9] == 'e')) &&
                ((msg_str[10] == 'N') || (msg_str[10] == 'n')) &&
                ((msg_str[11] == 'G') || (msg_str[11] == 'g')) &&
                ((msg_str[12] == 'T') || (msg_str[12] == 't')) &&
                ((msg_str[13] == 'H') || (msg_str[13] == 'h')) &&
                ((msg_str[0] == 'C') || (msg_str[0] == 'c')) &&
                ((msg_str[1] == 'O') || (msg_str[1] == 'o')) &&
                ((msg_str[2] == 'N') || (msg_str[2] == 'n')) &&
                ((msg_str[3] == 'T') || (msg_str[3] == 't')) &&
                ((msg_str[4] == 'E') || (msg_str[4] == 'e')) &&
                ((msg_str[5] == 'N') || (msg_str[5] == 'n')) &&
                ((msg_str[6] == 'T') || (msg_str[6] == 't')) &&
                (msg_str[7] == '-'))
            {
               int i = 15;

               while (((msg_str[i] == ' ') || (msg_str[i] == '\t')) &&
                      (i < read_length))
               {
                  i++;
               }
               if (i < read_length)
               {
                  int k = i;

                  while ((isdigit(msg_str[k])) && (k < read_length))
                  {
                     k++;
                  }
                  if (k > 0)
                  {
                     if (msg_str[k] != '\0')
                     {
                        msg_str[k] = '\0';
                     }
                     if ((hmr.content_length = strtoul(&msg_str[i], NULL,
                                                      10)) == ULONG_MAX)
                     {
                        hmr.content_length = 0;
                     }
                  }
               }
            }
            else if ((read_length > 11) && (msg_str[10] == ':') &&
                     ((msg_str[0] == 'C') || (msg_str[0] == 'c')) &&
                     ((msg_str[1] == 'O') || (msg_str[1] == 'o')) &&
                     ((msg_str[2] == 'N') || (msg_str[2] == 'n')) &&
                     ((msg_str[3] == 'N') || (msg_str[3] == 'n')) &&
                     ((msg_str[4] == 'E') || (msg_str[4] == 'e')) &&
                     ((msg_str[5] == 'C') || (msg_str[5] == 'c')) &&
                     ((msg_str[6] == 'T') || (msg_str[6] == 't')) &&
                     ((msg_str[7] == 'I') || (msg_str[7] == 'i')) &&
                     ((msg_str[8] == 'O') || (msg_str[8] == 'o')) &&
                     ((msg_str[9] == 'N') || (msg_str[9] == 'n')))
                 {
                    int i = 11;

                    while (((msg_str[i] == ' ') || (msg_str[i] == '\t')) &&
                           (i < read_length))
                    {
                       i++;
                    }
                    if (((i + 4) < read_length) &&
                        ((msg_str[i] == 'C') || (msg_str[i] == 'c')) &&
                        ((msg_str[i + 1] == 'L') || (msg_str[i + 1] == 'l')) &&
                        ((msg_str[i + 2] == 'O') || (msg_str[i + 2] == 'o')) &&
                        ((msg_str[i + 3] == 'S') || (msg_str[i + 3] == 's')) &&
                        ((msg_str[i + 4] == 'E') || (msg_str[i + 4] == 'e')))
                    {
                       hmr.close = YES;
                    }
                 }
            else if ((read_length > 17) && (msg_str[16] == ':') &&
                     (msg_str[3] == '-') &&
                     ((msg_str[0] == 'W') || (msg_str[0] == 'w')) &&
                     ((msg_str[1] == 'W') || (msg_str[1] == 'w')) &&
                     ((msg_str[2] == 'W') || (msg_str[2] == 'w')) &&
                     ((msg_str[4] == 'A') || (msg_str[4] == 'a')) &&
                     ((msg_str[5] == 'U') || (msg_str[5] == 'u')) &&
                     ((msg_str[6] == 'T') || (msg_str[6] == 't')) &&
                     ((msg_str[7] == 'H') || (msg_str[7] == 'h')) &&
                     ((msg_str[8] == 'E') || (msg_str[8] == 'e')) &&
                     ((msg_str[9] == 'N') || (msg_str[9] == 'n')) &&
                     ((msg_str[10] == 'T') || (msg_str[10] == 't')) &&
                     ((msg_str[11] == 'I') || (msg_str[11] == 'i')) &&
                     ((msg_str[12] == 'C') || (msg_str[12] == 'c')) &&
                     ((msg_str[13] == 'A') || (msg_str[13] == 'a')) &&
                     ((msg_str[14] == 'T') || (msg_str[14] == 't')) &&
                     ((msg_str[15] == 'E') || (msg_str[15] == 'e')))
                 {
                    int i = 17;

                    while (((msg_str[i] == ' ') || (msg_str[i] == '\t')) &&
                           (i < read_length))
                    {
                       i++;
                    }
                    if ((i + 4) < read_length)
                    {
                       if (((msg_str[i] == 'B') || (msg_str[i] == 'b')) &&
                           ((msg_str[i + 1] == 'A') || (msg_str[i + 1] == 'a')) &&
                           ((msg_str[i + 2] == 'S') || (msg_str[i + 2] == 's')) &&
                           ((msg_str[i + 3] == 'I') || (msg_str[i + 3] == 'i')) &&
                           ((msg_str[i + 4] == 'C') || (msg_str[i + 4] == 'c')))
                       {
                          hmr.www_authenticate = WWW_AUTHENTICATE_BASIC;
                       }
                       else if (((msg_str[i] == 'D') || (msg_str[i] == 'd')) &&
                                ((msg_str[i + 1] == 'I') || (msg_str[i + 1] == 'i')) &&
                                ((msg_str[i + 2] == 'G') || (msg_str[i + 2] == 'g')) &&
                                ((msg_str[i + 3] == 'E') || (msg_str[i + 3] == 'e')) &&
                                ((msg_str[i + 4] == 'S') || (msg_str[i + 4] == 's')) &&
                                ((msg_str[i + 5] == 'T') || (msg_str[i + 5] == 't')))
                       {
                          hmr.www_authenticate = WWW_AUTHENTICATE_DIGEST;
                       }
                    }
                 }
            else if ((read_length > 18) && (msg_str[17] == ':') &&
                     (msg_str[8] == '-') &&
                     ((msg_str[0] == 'T') || (msg_str[0] == 't')) &&
                     ((msg_str[1] == 'R') || (msg_str[1] == 'r')) &&
                     ((msg_str[2] == 'A') || (msg_str[2] == 'a')) &&
                     ((msg_str[3] == 'N') || (msg_str[3] == 'n')) &&
                     ((msg_str[4] == 'S') || (msg_str[4] == 's')) &&
                     ((msg_str[5] == 'F') || (msg_str[5] == 'f')) &&
                     ((msg_str[6] == 'E') || (msg_str[6] == 'e')) &&
                     ((msg_str[7] == 'R') || (msg_str[7] == 'r')) &&
                     ((msg_str[9] == 'E') || (msg_str[9] == 'e')) &&
                     ((msg_str[10] == 'N') || (msg_str[10] == 'n')) &&
                     ((msg_str[11] == 'C') || (msg_str[11] == 'c')) &&
                     ((msg_str[12] == 'O') || (msg_str[12] == 'o')) &&
                     ((msg_str[13] == 'D') || (msg_str[13] == 'd')) &&
                     ((msg_str[14] == 'I') || (msg_str[14] == 'i')) &&
                     ((msg_str[15] == 'N') || (msg_str[15] == 'n')) &&
                     ((msg_str[16] == 'G') || (msg_str[16] == 'g')))
                 {
                    int i = 18;

                    while (((msg_str[i] == ' ') || (msg_str[i] == '\t')) &&
                           (i < read_length))
                    {
                       i++;
                    }
                    if ((i + 7) < read_length)
                    {
                       if (((msg_str[i] == 'C') || (msg_str[i] == 'c')) &&
                           ((msg_str[i + 1] == 'H') || (msg_str[i + 1] == 'h')) &&
                           ((msg_str[i + 2] == 'U') || (msg_str[i + 2] == 'u')) &&
                           ((msg_str[i + 3] == 'N') || (msg_str[i + 3] == 'n')) &&
                           ((msg_str[i + 4] == 'K') || (msg_str[i + 4] == 'k')) &&
                           ((msg_str[i + 5] == 'E') || (msg_str[i + 5] == 'e')) &&
                           ((msg_str[i + 6] == 'D') || (msg_str[i + 6] == 'd')))
                       {
                          hmr.chunked = YES;
                       }
                    }
                 }
         }
         if ((ret_bytes_buffered != NULL) && (bytes_buffered > read_length))
         {
            *ret_bytes_buffered = bytes_buffered - read_length - 1;
            (void)memmove(msg_str, msg_str + read_length + 1,
                          *ret_bytes_buffered);
         }
      }
   }
   return(status_code);
}


/*----------------------------- read_msg() ------------------------------*/
/*                             ------------                              */
/* Reads blockwise from http_fd until it has read one complete line.     */
/* From this line it removes the \r\n and inserts a \0 and returns the   */
/* the number of bytes it buffered. The number of bytes in the line are  */
/* returned by read_length.                                              */
/*-----------------------------------------------------------------------*/
static int
read_msg(int *read_length)
{
   static int  bytes_buffered;
   static char *read_ptr = NULL;
   int         status;
   fd_set      rset;

   *read_length = 0;
   if (hmr.bytes_read == 0)
   {
      bytes_buffered = 0;
   }
   else
   {
      (void)memmove(msg_str, read_ptr + 1, hmr.bytes_read);
      bytes_buffered = hmr.bytes_read;
      read_ptr = msg_str;
   }

   FD_ZERO(&rset);
   for (;;)
   {
      if (hmr.bytes_read <= 0)
      {
         /* Initialise descriptor set */
         FD_SET(http_fd, &rset);
         timeout.tv_usec = 0L;
         timeout.tv_sec = transfer_timeout;

         /* Wait for message x seconds and then continue. */
         status = select(http_fd + 1, &rset, NULL, NULL, &timeout);

         if (status == 0)
         {
            /* timeout has arrived */
            timeout_flag = ON;
            hmr.bytes_read = 0;
            return(INCORRECT);
         }
         else if (FD_ISSET(http_fd, &rset))
              {
#ifdef WITH_SSL
                 if (ssl_con == NULL)
                 {
#endif
                    if ((hmr.bytes_read = read(http_fd, &msg_str[bytes_buffered],
                                               (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                    {
                       if (hmr.bytes_read == 0)
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
                          hmr.bytes_read = 0;
                       }
                       return(INCORRECT);
                    }
#ifdef WITH_SSL
                 }
                 else
                 {
                    if ((hmr.bytes_read = SSL_read(ssl_con,
                                                   &msg_str[bytes_buffered],
                                                   (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                    {
                       if (hmr.bytes_read == 0)
                       {
                          trans_log(ERROR_SIGN,  __FILE__, __LINE__, NULL,
                                    "Remote hang up.");
                          timeout_flag = NEITHER;
                       }
                       else
                       {
                          if ((status = SSL_get_error(ssl_con,
                                                      hmr.bytes_read)) == SSL_ERROR_SYSCALL)
                          {
                             if (errno == ECONNRESET)
                             {
                                timeout_flag = CON_RESET;
                             }
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                       "SSL_read() error (after reading %d Bytes) : %s",
                                       bytes_buffered, strerror(errno));
                          }
                          else
                          {
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                       "SSL_read() error (after reading %d Bytes) (%d)",
                                       bytes_buffered, status);
                          }
                          hmr.bytes_read = 0;
                       }
                       return(INCORRECT);
                    }
                 }
#endif
#ifdef WITH_TRACE
                 trace_log(NULL, 0, BIN_R_TRACE,
                           &msg_str[bytes_buffered], hmr.bytes_read, NULL);
#endif
                 read_ptr = &msg_str[bytes_buffered];
                 bytes_buffered += hmr.bytes_read;
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
      }

      /* Evaluate what we have read. */
      do
      {
         if (*read_ptr == '\n')
         {
            if (*(read_ptr - 1) == '\r')
            {
               *(read_ptr - 1) = '\0';
               hmr.bytes_read--;
            }
            else
            {
               *read_ptr = '\0';
            }
            *read_length = read_ptr - msg_str;
            return(bytes_buffered);
         }
         read_ptr++;
         hmr.bytes_read--;
      } while (hmr.bytes_read > 0);
   } /* for (;;) */
}

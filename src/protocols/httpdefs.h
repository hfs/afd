/*
 *  httpdefs.h - Part of AFD, an automatic file distribution program.
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

#ifndef __httpdefs_h
#define __httpdefs_h

#define DEFAULT_HTTP_PORT       80
#define DEFAULT_HTTPS_PORT      443
#define MAX_HTTP_HEADER_BUFFER  256

#define CONNECTION_REOPENED     100
#define WWW_AUTHENTICATE_BASIC  10
#define WWW_AUTHENTICATE_DIGEST 11

#define HTTP_LAST_CHUNK         0

struct http_message_reply
       {
          char  msg_header[MAX_HTTP_HEADER_BUFFER];
          char  hostname[MAX_REAL_HOSTNAME_LENGTH];
          char  user[MAX_USER_NAME_LENGTH];
          char  passwd[MAX_USER_NAME_LENGTH];
          char  *authorization;
          int   port;
          int   header_length;
          int   http_version;
          int   www_authenticate;
          int   bytes_buffered;
          int   bytes_read;
          int   sndbuf_size;
          int   rcvbuf_size;
          off_t content_length;
          char  chunked;
          char  close;
          char  free;
#ifdef WITH_SSL
          char  ssl;
#endif
       };

/* Function prototypes */
extern int  http_check_connection(void),
#ifdef WITH_SSL
            http_connect(char *, int, char *, char *, int, int, int),
#else
            http_connect(char *, int, char *, char *, int, int),
#endif
            http_del(char *, char *, char *),
            http_get(char *, char *, char *, off_t *),
            http_put(char *, char *, char *, off_t),
            http_read(char *, int),
            http_chunk_read(char **, int *),
            http_write(char *, char *, int);
extern void http_quit(void);

#endif /* __httpdefs_h */

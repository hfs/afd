/*
 *  ftpdefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __ftpdefs_h
#define __ftpdefs_h

#ifndef MAX_FILENAME_LENGTH
# define MAX_FILENAME_LENGTH      256
#endif
#ifndef SUCCESS
# define SUCCESS                  0
#endif
#ifndef INCORRECT
# define INCORRECT                -1
#endif

#ifndef MAX_RET_MSG_LENGTH
# define MAX_RET_MSG_LENGTH       1024
#endif
#define MAX_DATA_CONNECT_RETRIES  3
#define DEFAULT_FTP_PORT          21
#define MAX_FTP_DATE_LENGTH       15

#ifdef _BLOCK_MODE
# define DATA_BLOCK               128
# define END_BLOCK                64
#endif

/* Definitions for the different FTP modes (PORT, PASV, EPRT, EPSV). */
#define PASSIVE_MODE              1
#define ACTIVE_MODE               2
#define EXTENDED_MODE             4
#define ALLOW_DATA_REDIRECT       8

/* Definitions for the ftp_data() function. */
#define DATA_WRITE                1
#define DATA_READ                 2

/* Definitions for the ftp_list() function. */
#define LIST_CMD                  1
#define NLIST_CMD                 2
#define ONE_FILENAME              4
#define BUFFERED_LIST             8
#ifdef WITH_SSL
#define ENCRYPT_DATA              16
#endif

extern int  ftp_auth_data(void),
            ftp_connect(char *, int),
#ifdef WITH_SSL
            ftp_ssl_auth(void),
            ftp_ssl_init(char),
#endif
            ftp_data(char *, off_t, int, int, int),
            ftp_close_data(void),
#ifdef WITH_SENDFILE
            ftp_sendfile(int, off_t *, int),
#endif
            ftp_write(char *, char *, int),
            ftp_read(char *, int),
            ftp_user(char *),
            ftp_account(char *),
#ifdef _BLOCK_MODE
            ftp_mode(char),
            ftp_open(char *, int),
            ftp_block_write(char *, unsigned short, char),
#endif
            ftp_pass(char *),
            ftp_idle(int),
            ftp_type(char),
            ftp_cd(char *, int),
            ftp_pwd(void),
            ftp_chmod(char *, char *),
            ftp_move(char *, char *, int, int),
            ftp_noop(void),
            ftp_keepalive(void),
            ftp_list(int, int, ...),
            ftp_dele(char *),
            ftp_exec(char *, char *),
            ftp_quit(void),
            ftp_get_reply(void),
            ftp_size(char *, off_t *),
            ftp_date(char *, time_t *);
#endif /* __ftpdefs_h */

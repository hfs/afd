/*
 *  ftpdefs.h - Part of AFD, an automatic file distribution program.
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

#ifndef __ftpdefs_h
#define __ftpdefs_h

/* Define to flush every FTP write command. */
#define _FLUSH_FTP_WRITE

#ifndef MAX_FILENAME_LENGTH
#define MAX_FILENAME_LENGTH       256
#endif
#ifndef SUCCESS
#define SUCCESS                   0
#endif
#ifndef INCORRECT
#define INCORRECT                 -1
#endif

#ifndef MAX_RET_MSG_LENGTH
#define MAX_RET_MSG_LENGTH        1024
#endif
#define MAX_DATA_CONNECT_RETRIES  3
#define DEFAULT_FTP_PORT          21

#ifdef _BLOCK_MODE
#define DATA_BLOCK                128
#define END_BLOCK                 64
#endif

extern int  ftp_connect(char *, int),
            ftp_data(char *, int),
            ftp_close_data(void),
            ftp_write(char *, char *, int),
            ftp_user(char *),
#ifdef _BLOCK_MODE
            ftp_mode(char),
            ftp_open(char *, int),
            ftp_block_write(char *, unsigned short, char),
#endif
            ftp_pass(char *),
            ftp_type(char),
            ftp_cd(char *),
            ftp_move(char *, char *),
            ftp_list(char *, char *),
            ftp_dele(char *),
            ftp_quit(void),
            ftp_get_reply(void);
#endif /* __ftpdefs_h */

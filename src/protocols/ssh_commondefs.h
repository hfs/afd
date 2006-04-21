/*
 *  ssh_commondefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __ssh_commondefs_h
#define __ssh_commondefs_h

#define DEFAULT_SSH_PORT     22
#define SSH_COMMAND          "ssh"

#define MIN_SSH_PROMPT_DELAY 1L
#define MAX_SSH_PROMPT_DELAY 5L

/* Function prototypes */
extern int    get_ssh_reply(int, int),
              ssh_exec(char *, int, char *, char *, char *, char *,
                       int *, pid_t *),
              ssh_login(int, char *);
extern size_t pipe_write(int, char *, size_t);

#endif /* __ssh_commondefs_h */

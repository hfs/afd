/*
 *  awdefs.h - Part of AFD, an automatic file distribution program.
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

#ifndef __awdefs_h
#define __awdefs_h

#ifdef _NEW_ARCHIVE_WATCH
/*
 * The archive directory is build up as follows:
 *   bsh-jade/bm13dat/0/0_916089609_3939_164
 *      |        |    |            |
 *      |        |    |            +---------> priority_delete-time_unique-number_job-id
 *      |        |    +----------------------> directory counter
 *      |        +---------------------------> user name
 *      +------------------------------------> hostname
 *
 * From this we create two different structures, archive_directories
 * and archive_entry. The reason for spliting them up is to save
 * memory, since the first three directory entries are often the
 * same (hostname, user name and directory counter). Structure
 */

struct archive_directories
       {
          char archive_dir[MAX_HOSTNAME_LENGTH +
                           MAX_USER_NAME_LENGTH + MAX_INT_LENGTH];
          int  no_of_entries;
       };
struct archive_entry
       {
          time_t         remove_time;
          unsigned int   ade;   /* archive directory entry (archive_dir) */
          unsigned int   job_id;
          unsigned short unique_number;
          unsigned char  priority;
       };
#endif /* _NEW_ARCHIVE_WATCH */

/* Function prototypes */
extern void inspect_archive(char *);

#endif /* __awdefs_h */

/*
 *  create_eumetsat_name.c - Part of AFD, an automatic file distribution
 *                           program.
 *  Copyright (c) 1999 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   create_eumetsat_name - generates the file names for EUMETSAT
 **
 ** SYNOPSIS
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.03.1999 H.Kiehl  Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

/* Global variables */
int         counter_fd,                  /* NOTE: This is not used. */
            sys_log_fd = STDERR_FILENO,
            no_of_rule_headers;
struct rule *rule;

#define HUNK_MAX 20480
#ifdef _WITH_FIXED_NNN
#define WMO_HEADER_OFFSET 10
#else
#define WMO_HEADER_OFFSET 4
#endif


/*$$$$$$$$$$$$$$$$$$$$$$$ create_eumetsat_name() $$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   size_t      length;
   time_t      time_val;
   char        new_name[47],
               str_time[4],
               tmp_name[15];
   struct stat stat_buf;
   struct tm   *p_tm;

   if ((argc != 2) && (argc != 3))
   {
      (void)fprintf(stderr, "Usage: %s <file name> [<rename rule>]\n", argv[0]);
      exit(1);
   }
   if (strlen(argv[1]) < 9)
   {
      (void)fprintf(stderr, "Filename to short.\n");
      exit(1);
   }

   if (stat(argv[1], &stat_buf) == -1)
   {
      (void)fprintf(stderr, "Failed to stat() %s : %s\n",
                    argv[1], strerror(errno));
      exit(1);
   }

   str_time[0] = argv[1][5];
   str_time[1] = argv[1][6];
   str_time[2] = argv[1][7];
   str_time[3] = '\0';

   p_tm = gmtime(&stat_buf.st_mtime);
   (void)strftime(tmp_name, 15, "%Y%m%d%H%M%S", p_tm);

   if (argv[1][8] == '0')
   {
      p_tm->tm_hour = 0;
   }
   else
   {
      p_tm->tm_hour = 12;
   }
   p_tm->tm_sec = 0;
   p_tm->tm_min = 0;
   p_tm->tm_mday = 0;
   p_tm->tm_mon = 0;
   p_tm->tm_wday = 0;
   time_val = mktime(p_tm) - timezone;
   time_val += (atoi(str_time) * 86400);
   p_tm = gmtime(&time_val);
   length = strftime(new_name, 47, "%Y%m%d%H%M%SZ_", p_tm);

   str_time[0] = argv[1][3];
   str_time[1] = argv[1][4];
   str_time[2] = '\0';
   time_val += (((atoi(str_time) - 4) * 6) * 3600);
   p_tm = gmtime(&time_val);
   length += strftime(&new_name[length], 47, "%Y%m%d%H%M%SZ_", p_tm);

   (void)sprintf(&new_name[length], "%s", tmp_name);

   /*
    * Check if it is neccessary to insert a WMO-header.
    */
   if (argc == 3)
   {
      char rule_file[MAX_PATH_LENGTH];

      if (get_afd_path(&argc, argv, rule_file) < 0)
      {
         exit(INCORRECT);
      }
      (void)strcat(rule_file, ETC_DIR);
      (void)strcat(rule_file, RENAME_RULE_FILE);
      get_rename_rules(rule_file);

      if (no_of_rule_headers == 0)
      {
         (void)fprintf(stderr,
                       "The rename.rule file does not contain any valid data. (%s %d)\n",
                       __FILE__, __LINE__);
         exit(INCORRECT);
      }
      else
      {
         int    from_fd,
                gotcha = NO,
                i,
                length,
                rule_pos,
                to_fd;
         size_t hunk,
                left;
         char   *buffer,
                *ptr,
                wmo_header[MAX_FILENAME_LENGTH];

         if ((rule_pos = get_rule(argv[2], no_of_rule_headers)) < 0)
         {
            (void)fprintf(stderr,
                          "Could NOT find rule %s in rename.rule file. (%s %d)\n",
                          argv[2], __FILE__, __LINE__);
            exit(INCORRECT);
         }
         for (i = 0; i < rule[rule_pos].no_of_rules; i++)
         {
            if (pmatch(rule[rule_pos].filter[i], argv[1]) == 0)
            {
               gotcha = YES;
               wmo_header[0] = 1;
               wmo_header[1] = '\015'; /* CR */
               wmo_header[2] = '\015'; /* CR */
               wmo_header[3] = '\012'; /* LF */
#ifdef _WITH_FIXED_NNN
               wmo_header[4] = '5';
               wmo_header[5] = '5';
               wmo_header[6] = '5';
               wmo_header[7] = '\015'; /* CR */
               wmo_header[8] = '\015'; /* CR */
               wmo_header[9] = '\012'; /* LF */
#endif /* _WITH_FIXED_NNN */
               change_name(argv[1],
                           rule[rule_pos].filter[i],
                           rule[rule_pos].rename_to[i],
                           &wmo_header[WMO_HEADER_OFFSET]);
               break;
            }
         } /* for (i = 0; i < rule[rule_pos].no_of_rules; i++) */

         if (gotcha == NO)
         {
            (void)fprintf(stderr,
                          "Filename %s does not match any of the filters. (%s %d)\n",
                          argv[1], __FILE__, __LINE__);
            exit(INCORRECT);
         }

         ptr = &wmo_header[WMO_HEADER_OFFSET];
         do
         {
            if (*ptr == '_')
            {
               *ptr = ' ';
            }
            ptr++;
         } while (*ptr != '\0');
         *ptr = '\015'; /* CR */
         *(ptr + 1) = '\015'; /* CR */
         *(ptr + 2) = '\012'; /* LF */
         length = ptr + 3 - wmo_header;

         /* Open source file */
         if ((from_fd = open(argv[1], O_RDONLY)) == -1)
         {
            (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                          argv[1], strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }

         if (fstat(from_fd, &stat_buf) == -1)
         {
            (void)fprintf(stderr, "Failed to fstat() %s : %s (%s %d)\n",
                          argv[1], strerror(errno), __FILE__, __LINE__);
            (void)close(from_fd);
            exit(INCORRECT);
         }

         left = hunk = stat_buf.st_size;

         if (hunk > HUNK_MAX)
         {
            hunk = HUNK_MAX;
         }

         if ((buffer = (char *)malloc(hunk)) == NULL)
         {
            (void)fprintf(stderr, "Failed to allocate memory : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            (void)close(from_fd);
            exit(INCORRECT);
         }

         /* Open destination file */
         if ((to_fd = open(new_name,
                           O_WRONLY | O_CREAT | O_TRUNC,
                           S_IRUSR|S_IWUSR)) == -1)
         {
            (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                          new_name, strerror(errno), __FILE__, __LINE__);
            free(buffer);
            (void)close(from_fd);
            exit(INCORRECT);
         }
         if (write(to_fd, wmo_header, length) != length)
         {
            (void)fprintf(stderr, "Failed to write() %s : %s (%s %d)\n",
                          new_name, strerror(errno), __FILE__, __LINE__);
            free(buffer);
            (void)close(from_fd);
            (void)close(to_fd);
            exit(INCORRECT);
         }

         while (left > 0)
         {
            /* Try read file in one hunk */
            if (read(from_fd, buffer, hunk) != hunk)
            {
               (void)fprintf(stderr, "Failed to read() %s : %s (%s %d)\n",
                             argv[1], strerror(errno), __FILE__, __LINE__);
               free(buffer);
               (void)close(from_fd);
               (void)close(to_fd);
               exit(INCORRECT);
            }

            /* Try write file in one hunk */
            if (write(to_fd, buffer, hunk) != hunk)
            {
               (void)fprintf(stderr, "Failed to write() %s : %s (%s %d)\n",
                             new_name, strerror(errno), __FILE__, __LINE__);
               free(buffer);
               (void)close(from_fd);
               (void)close(to_fd);
               exit(INCORRECT);
            }
            left -= hunk;
            if (left < hunk)
            {
               hunk = left;
            }
         }
         free(buffer);
         if (close(from_fd) == -1)
         {
            (void)fprintf(stderr, "Failed to close() %s : %s (%s %d)\n",
                          argv[1], strerror(errno), __FILE__, __LINE__);
         }
         wmo_header[0] = '\015'; /* CR */
         wmo_header[1] = '\015'; /* CR */
         wmo_header[2] = '\012'; /* LF */
         wmo_header[3] = 3;
         if (write(to_fd, wmo_header, 4) != 4)
         {
            (void)fprintf(stderr, "Failed to write() %s : %s (%s %d)\n",
                          new_name, strerror(errno), __FILE__, __LINE__);
            free(buffer);
            (void)close(from_fd);
            (void)close(to_fd);
            exit(INCORRECT);
         }
         if (close(to_fd) == -1)
         {
            (void)fprintf(stderr, "Failed to close() %s : %s (%s %d)\n",
                          new_name, strerror(errno), __FILE__, __LINE__);
         }
         if (remove(argv[1]) == -1)
         {
            (void)fprintf(stderr, "Failed to remove() %s : %s (%s %d)\n",
                          argv[1], strerror(errno), __FILE__, __LINE__);
         }
      }
   }
   else
   {
      if (rename(argv[1], new_name) == -1)
      {
         (void)fprintf(stderr, "Failed to rename %s to %s : %s\n",
                       argv[1], new_name, strerror(errno));
         exit(1);
      }
   }

   exit(SUCCESS);
}

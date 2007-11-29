/*
 *  view_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   view_data - calls a configured program to view data
 **
 ** SYNOPSIS
 **   void view_data(char *fullname, char *file_name)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.04.2007 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>            /* strerror()                             */
#include <stdlib.h>            /* realloc()                              */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "ui_common_defs.h"

/* External global variables. */
extern char                     font_name[],
                                *p_work_dir;

/* Local global variables. */
static int                      no_of_vdp;
static struct view_process_list *vdpl;


/*############################# view_data() #############################*/
void
view_data(char *fullname, char *file_name)
{
   int  i,
        j;
   char afd_config_file[MAX_PATH_LENGTH];

   (void)sprintf(afd_config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if (eaccess(afd_config_file, F_OK) == 0)
   {
      int           fd;
      static time_t afd_config_mtime = 0;
      struct stat   stat_buf;

      if ((fd = open(afd_config_file, O_RDONLY)) == -1)
      {
         (void)fprintf(stderr,
                       "Failed to open() `%s' for reading : %s (%s %d)\n",
                       afd_config_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (fstat(fd, &stat_buf) == -1)
      {
         (void)fprintf(stderr,
                       "Failed to fstat() `%s' : %s (%s %d)\n",
                       afd_config_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (stat_buf.st_mtime > afd_config_mtime)
      {
         int  argcounter;
         char *buffer,
              *ptr,
              *p_arg,
              *p_start,
              view_data_prog_def[VIEW_DATA_PROG_DEF_LENGTH + 2];

         if ((buffer = malloc(1 + stat_buf.st_size + 1)) == NULL)
         {
            (void)fprintf(stderr,
#if SIZEOF_OFF_T == 4
                          "Failed to malloc() %ld bytes : %s (%s %d)\n",
#else
                          "Failed to malloc() %lld bytes : %s (%s %d)\n",
#endif
                          (pri_off_t)(1 + stat_buf.st_size + 1),
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if (read(fd, &buffer[1], stat_buf.st_size) == -1)
         {
            (void)fprintf(stderr,
#if SIZEOF_OFF_T == 4
                          "Failed to read() %ld bytes from `%s' : %s (%s %d)\n",
#else
                          "Failed to read() %lld bytes from `%s' : %s (%s %d)\n",
#endif
                          (pri_off_t)stat_buf.st_size, afd_config_file,
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         buffer[0] = '\n';
         buffer[stat_buf.st_size + 1] = '\0';

         (void)strcpy(&view_data_prog_def[1], VIEW_DATA_PROG_DEF);
         view_data_prog_def[0] = '\n';
         if (no_of_vdp > 0)
         {
            for (i = 0; i < no_of_vdp; i++)
            {
               free(vdpl[i].progname);
               FREE_RT_ARRAY(vdpl[i].filter);
               free(vdpl[i].args);
            }
            free(vdpl);
         }
         no_of_vdp = 0;
         vdpl = NULL;
         ptr = buffer;
         do
         {
            if ((ptr = posi(ptr, view_data_prog_def)) != NULL)
            {
               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
               p_start = ptr;
               argcounter = 0;
               if (*ptr == '"')
               {
                  ptr++;
                  p_start = ptr;
                  while ((*ptr != '"') && (*ptr != '\n') && (*ptr != '\0'))
                  {
                     if ((*ptr == ' ') || (*ptr == '\t'))
                     {
                        argcounter++;
                     }
                     ptr++;
                  }
                  if (*ptr == '"')
                  {
                     *ptr = ' ';
                  }
               }
               else
               {
                  while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
                         (*ptr != '\0'))
                  {
                     ptr++;
                  }
               }
               if (ptr != p_start)
               {
                  int length,
                      max_length;

                  if ((vdpl = realloc(vdpl, ((no_of_vdp + 1) * sizeof(struct view_process_list)))) == NULL)
                  {
                     (void)fprintf(stderr, "realloc() error : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  length = ptr - p_start;
                  if ((vdpl[no_of_vdp].progname = malloc(length + 1)) == NULL)
                  {
                     (void)fprintf(stderr,
                                   "Failed to malloc() %d bytes : %s (%s %d)\n",
                                   length + 1, strerror(errno),
                                   __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  (void)strncpy(vdpl[no_of_vdp].progname, p_start, length);
                  vdpl[no_of_vdp].progname[length] = '\0';

                  /* Cut out the args. */
                  if (argcounter > 0)
                  {
                     int  filename_set = NO;
                     char *p_arg;

                     if ((vdpl[no_of_vdp].args = malloc((argcounter + 2) * sizeof(char *))) == NULL)
                     {
                        (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
                        exit(INCORRECT);
                     }
                     vdpl[no_of_vdp].args[0] = vdpl[no_of_vdp].progname;
                     p_arg = vdpl[no_of_vdp].progname;
                     for (i = 1; i < (argcounter + 1); i++)
                     {
                        while ((*p_arg != ' ') && (*p_arg != '\t'))
                        {
                           p_arg++;
                        }
                        *p_arg = '\0';
                        p_arg++;
                        vdpl[no_of_vdp].args[i] = p_arg;
                        if ((*p_arg == '%') && (*(p_arg + 1) == 's'))
                        {
                           filename_set = YES;
                           vdpl[no_of_vdp].args[i] = fullname;
                        }
                     }
                     while ((*p_arg != ' ') && (*p_arg != '\t') &&
                            (*p_arg != '\n') && (*p_arg != '\0'))
                     {
                        p_arg++;
                     }
                     if (*p_arg != '\0')
                     {
                        *p_arg = '\0';
                     }
                     if (filename_set == NO)
                     {
                        vdpl[no_of_vdp].args[i + 1] = fullname;
                        i += 2;
                     }
                     vdpl[no_of_vdp].args[i] = NULL;
                  }
                  else
                  {
                     if ((vdpl[no_of_vdp].args = malloc(3 * sizeof(char *))) == NULL)
                     {
                        (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
                        exit(INCORRECT);
                     }
                     vdpl[no_of_vdp].args[0] = vdpl[no_of_vdp].progname;
                     vdpl[no_of_vdp].args[1] = fullname;
                     vdpl[no_of_vdp].args[2] = NULL;
                  }

                  while ((*ptr == ' ') || (*ptr == '\t'))
                  {
                     ptr++;
                  }
                  p_start = ptr;
                  length = 0;
                  max_length = 0;
                  vdpl[no_of_vdp].no_of_filters = 0;
                  while ((*ptr != '\n') && (*ptr != '\0'))
                  {
                     if (*ptr == '|')
                     {
                        vdpl[no_of_vdp].no_of_filters++;
                        if (length > max_length)
                        {
                           max_length = length;
                        }
                        length = 0;
                        ptr++;
                     }
                     else
                     {
                        ptr++; length++;
                     }
                  }
                  if (ptr != p_start)
                  {
                     vdpl[no_of_vdp].no_of_filters++;
                     if (length > max_length)
                     {
                        max_length = length;
                     }
                     RT_ARRAY(vdpl[no_of_vdp].filter,
                              vdpl[no_of_vdp].no_of_filters,
                              max_length + 1, char);

                     ptr = p_start;
                     length = 0;
                     max_length = 0;
                     while ((*ptr != '\n') && (*ptr != '\0'))
                     {
                        if (*ptr == '|')
                        {
                           vdpl[no_of_vdp].filter[max_length][length] = '\0';
                           length = 0;
                           max_length++;
                           ptr++;
                        }
                        else
                        {
                           vdpl[no_of_vdp].filter[max_length][length] = *ptr;
                           ptr++; length++;
                        }
                     }
                  }
                  no_of_vdp++;
               }
            }
         } while (ptr != NULL);
         free(buffer);
         afd_config_mtime = stat_buf.st_mtime;

         /* Add as default program afd_hex_print. */
         if ((vdpl = realloc(vdpl, ((no_of_vdp + 1) * sizeof(struct view_process_list)))) == NULL)
         {
            (void)fprintf(stderr, "realloc() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         vdpl[no_of_vdp].no_of_filters = 1;
         RT_ARRAY(vdpl[no_of_vdp].filter, 1, 2, char);
         vdpl[no_of_vdp].filter[0][0] = '*';
         vdpl[no_of_vdp].filter[0][1] = '\0';
         if ((vdpl[no_of_vdp].progname = malloc(MAX_PATH_LENGTH)) == NULL)
         {
            (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         (void)sprintf(vdpl[no_of_vdp].progname, "%s %s %s -b -f %s \"%s %s %s\"",
                       SHOW_CMD, WORK_DIR_ID, p_work_dir, font_name,
                       HEX_PRINT, fullname, file_name);

         if ((vdpl[no_of_vdp].args = malloc(8 * sizeof(char *))) == NULL)
         {
            (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         vdpl[no_of_vdp].args[0] = vdpl[no_of_vdp].progname;
         p_arg = vdpl[no_of_vdp].progname;
         p_arg += SHOW_CMD_LENGTH;
         *p_arg = '\0';
         p_arg++;
         vdpl[no_of_vdp].args[1] = p_arg;
         p_arg += WORK_DIR_ID_LENGTH;
         *p_arg = '\0';
         p_arg++;
         vdpl[no_of_vdp].args[2] = p_arg;
         while (*p_arg != ' ')
         {
            p_arg++;
         }
         *p_arg = '\0';
         p_arg++;
         vdpl[no_of_vdp].args[3] = p_arg;
         p_arg += 2;
         *p_arg = '\0';
         p_arg++;
         vdpl[no_of_vdp].args[4] = p_arg;
         p_arg += 2;
         *p_arg = '\0';
         p_arg++;
         vdpl[no_of_vdp].args[5] = p_arg;
         while (*p_arg != ' ')
         {
            p_arg++;
         }
         *p_arg = '\0';
         p_arg++;
         vdpl[no_of_vdp].args[6] = p_arg;
         vdpl[no_of_vdp].args[7] = NULL;
         no_of_vdp++;
      }
   }

   if (no_of_vdp == 0)
   {
      char *p_arg;

      /* Add as default program afd_hex_print. */
      no_of_vdp = 1;
      if ((vdpl = realloc(vdpl, sizeof(struct view_process_list))) == NULL)
      {
         (void)fprintf(stderr, "realloc() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      vdpl[0].no_of_filters = 1;
      RT_ARRAY(vdpl[0].filter, 1, 2, char);
      vdpl[0].filter[0][0] = '*';
      vdpl[0].filter[0][1] = '\0';
      if ((vdpl[0].progname = malloc(MAX_PATH_LENGTH)) == NULL)
      {
         (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      (void)sprintf(vdpl[0].progname, "%s %s %s -b -f %s \"%s %s %s\"",
                    SHOW_CMD, WORK_DIR_ID, p_work_dir, font_name,
                    HEX_PRINT, fullname, file_name);

      if ((vdpl[0].args = malloc(8 * sizeof(char *))) == NULL)
      {
         (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      vdpl[0].args[0] = vdpl[no_of_vdp].progname;
      p_arg = vdpl[0].progname;
      p_arg += SHOW_CMD_LENGTH;
      *p_arg = '\0';
      p_arg++;
      vdpl[0].args[1] = p_arg;
      p_arg += WORK_DIR_ID_LENGTH;
      *p_arg = '\0';
      p_arg++;
      vdpl[0].args[2] = p_arg;
      while (*p_arg != ' ')
      {
         p_arg++;
      }
      *p_arg = '\0';
      p_arg++;
      vdpl[0].args[3] = p_arg;
      p_arg += 2;
      *p_arg = '\0';
      p_arg++;
      vdpl[0].args[4] = p_arg;
      p_arg += 2;
      *p_arg = '\0';
      p_arg++;
      vdpl[0].args[5] = p_arg;
      while (*p_arg != ' ')
      {
         p_arg++;
      }
      *p_arg = '\0';
      p_arg++;
      vdpl[0].args[6] = p_arg;
      vdpl[0].args[7] = NULL;
   }

   for (i = 0; i < no_of_vdp; i++)
   {
      for (j = 0; j < vdpl[i].no_of_filters; j++)
      {
         if (pmatch(vdpl[i].filter[j], file_name, NULL) == 0)
         {
            if (i == (no_of_vdp - 1))
            {
               (void)sprintf(vdpl[i].args[6], "\"%s %s %s\"",
                             HEX_PRINT, fullname, file_name);
            }
            make_xprocess(vdpl[i].progname, vdpl[i].progname, vdpl[i].args, -1);
            return;
         }
      }
   }

   return;
}

/*
 *  eval_bul_rep_config.c - Part of AFD, an automatic file distribution
 *                          program.
 *  Copyright (c) 2011 Deutscher Wetterdienst (DWD),
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

DESCR__S_M1
/*
 ** NAME
 **   eval_bul_rep_config - 
 **
 ** SYNOPSIS
 **   void eval_bul_rep_config(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.03.2011 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>             /* fprintf(), sprintf()                   */
#include <string.h>            /* atoi(), strerror()                     */
#include <stdlib.h>            /* malloc(), calloc(), free()             */
#include <ctype.h>             /* isdigit()                              */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>            /* read()                                 */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                no_of_brc_entries;
extern struct wmo_bul_rep *brcdb; /* Bulletin Report Configuration Database */

/* Local function prototypes. */
static void               get_report_data(struct wmo_bul_rep *, char *);


/*######################### eval_bul_rep_config() #######################*/
void
eval_bul_rep_config(char *bul_file, char *rep_file, int verbose)
{
   static time_t last_read_bul = 0,
                 last_read_rep = 0;
   static int    first_time = YES;
   struct stat   stat_buf;

   if (stat(bul_file, &stat_buf) == -1)
   {
      if (errno == ENOENT)
      {
         /*
          * Only tell user once that the message specification file is
          * missing. Otherwise it is anoying to constantly receive this
          * message.
          */
         if (first_time == YES)
         {
            system_log(INFO_SIGN, __FILE__, __LINE__,
                       _("There is no message specification file `%s'"),
                       bul_file);
            first_time = NO;
         }
      }
      else
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to stat() `%s' : %s"),
                    bul_file, strerror(errno));
      }
   }
   else
   {
      char        *rep_buffer = NULL,
                  *rep_header_end;
      struct stat stat_buf2;

      if (stat(rep_file, &stat_buf2) == -1)
      {
         if (errno != ENOENT)
         {
            system_log(INFO_SIGN, __FILE__, __LINE__,
                       _("There is no report specification file `%s'"),
                       rep_file);
         }
      }
      else
      {
         if (stat_buf2.st_mtime != last_read_rep)
         {
            int fd;

            /* Allocate memory to store file. */
            if ((rep_buffer = malloc(stat_buf2.st_size + 1)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("malloc() error : %s"), strerror(errno));
               exit(INCORRECT);
            }

            /* Open file. */
            if ((fd = open(rep_file, O_RDONLY)) == -1)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' : %s"),
                          rep_file, strerror(errno));
               free(rep_buffer);
               exit(INCORRECT);
            }

            /* Read file into buffer. */
            if (read(fd, &rep_buffer[0], stat_buf2.st_size) != stat_buf2.st_size)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to read() `%s' : %s"),
                          rep_file, strerror(errno));
               free(rep_buffer);
               (void)close(fd);
               exit(INCORRECT);
            }
            if (close(fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("close() error : %s"), strerror(errno));
            }
            rep_buffer[stat_buf2.st_size] = '\0';
            rep_header_end = rep_buffer;
            while ((*rep_header_end != '\0') &&
                   (*rep_header_end != '\n') &&
                   (*rep_header_end != '\r'))
            {
               rep_header_end++;
            }
            while ((*rep_header_end == '\n') ||
                   (*rep_header_end == '\r'))
            {
               rep_header_end++;
            }
         } /* if (stat_buf2.st_mtime != last_read_rep) */

         last_read_bul = 0;
      }

      if (stat_buf.st_mtime != last_read_bul)
      {
         register int i;
         int          fd;
         char         *buffer,
                      *ptr;

         if (first_time == YES)
         {      
            first_time = NO;
         }      
         else   
         {      
            if (verbose == YES)
            {   
               system_log(INFO_SIGN, NULL, 0,
                          _("Rereading message specification file."));
            }
         }

         if (last_read_bul != 0)
         {
            /*
             * Since we are rereading the whole message specification file
             * again lets release the memory we stored for the previous
             * structure of wmo_bul_rep.
             */
            free(brcdb);
            no_of_brc_entries = 0;
         }

         /* Allocate memory to store file. */
         if ((buffer = malloc(stat_buf.st_size + 1)) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("malloc() error : %s"), strerror(errno));
            free(rep_buffer);
            exit(INCORRECT);
         }

         /* Open file. */
         if ((fd = open(bul_file, O_RDONLY)) == -1)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' : %s"),
                       bul_file, strerror(errno));
            free(buffer);
            free(rep_buffer);
            exit(INCORRECT);
         }

         /* Read file into buffer. */
         if (read(fd, &buffer[0], stat_buf.st_size) != stat_buf.st_size)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("Failed to read() `%s' : %s"),
                       bul_file, strerror(errno));
            free(buffer);
            free(rep_buffer);
            (void)close(fd);
            exit(INCORRECT);
         }
         if (close(fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
         buffer[stat_buf.st_size] = '\0';
         last_read_bul = stat_buf.st_mtime;

         /*
          * Now that we have the contents in the buffer lets first see
          * how many entries there are in the buffer so we can allocate
          * memory for the entries.
          */
         ptr = buffer;
         while (*ptr != '\0')
         {
            while ((*ptr != '\n') && (*ptr != '\r'))
            {
               ptr++;
            }
            if ((*ptr == '\n') || (*ptr == '\r'))
            {
               no_of_brc_entries++;
               ptr++;
            }
            while ((*ptr == '\n') || (*ptr == '\r'))
            {
               ptr++;
            }
            if (no_of_brc_entries > 1)
            {
               /* Ignore first description line. */
               no_of_brc_entries--;
            }
         }

         if (no_of_brc_entries > 0)
         {
            register int k;

            if ((brcdb = calloc(no_of_brc_entries,
                                sizeof(struct wmo_bul_rep))) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("calloc() error : %s (%s %d)\n"), strerror(errno));
               free(rep_buffer);
               exit(INCORRECT);
            }
            ptr = buffer;

            for (i = 0; i < no_of_brc_entries; i++)
            {
               k = 0;
               while ((k < 6) && (*ptr != ';') && (*ptr != '\n') &&
                      (*ptr != '\0') && (*ptr != '\r'))
               {
                  brcdb[i].TTAAii[k] = *ptr;
                  k++; ptr++;
               }
               brcdb[i].TTAAii[k] = '\0';
               if (*ptr != ';')
               {
                  while ((*ptr != ';') && (*ptr != '\n') &&
                         (*ptr != '\0') && (*ptr != '\r'))
                  {
                     ptr++;
                  }
               }
               if (*ptr == ';')
               {
                  ptr++;
                  k = 0;
                  while ((k < 4) && (*ptr != ';') && (*ptr != '\n') &&
                         (*ptr != '\0') && (*ptr != '\r'))
                  {
                     brcdb[i].CCCC[k] = *ptr;
                     k++; ptr++;
                  }
                  brcdb[i].CCCC[k] = '\0';
                  if (*ptr != ';')
                  {
                     ptr++;
                     if ((*ptr == 'i') && (*(ptr + 1) == 'n') &&
                         (*(ptr + 2) == 'p') && (*(ptr + 3) == ';'))
                     {
                        brcdb[i].type = BUL_TYPE_INP;
                        ptr += 3;
                     }
                     else if ((*ptr == 'i') && (*(ptr + 1) == 'g') &&
                              (*(ptr + 2) == 'n') && (*(ptr + 3) == ';'))
                          {
                             brcdb[i].type = BUL_TYPE_IGN;
                             ptr += 3;
                          }
                     else if ((*ptr == 'c') && (*(ptr + 1) == 'm') &&
                              (*(ptr + 2) == 'p') && (*(ptr + 3) == ';'))
                          {
                             brcdb[i].type = BUL_TYPE_CMP;
                             ptr += 3;
                          }
                          else
                          {
                             while ((*ptr != ';') && (*ptr != '\n') &&
                                    (*ptr != '\0') && (*ptr != '\r'))
                             {
                                ptr++;
                             }
                             brcdb[i].type = 0;
                          }

                     if (*ptr == ';')
                     {
                        ptr++;
                        if (*ptr == 'D')
                        {
                           brcdb[i].spec = BUL_SPEC_DUP;
                           ptr++;
                        }
                        else
                        {
                           brcdb[i].spec = 0;
                        }
                        while ((*ptr != ';') && (*ptr != '\n') &&
                               (*ptr != '\0') && (*ptr != '\r'))
                        {
                           ptr++;
                        }

                        if (*ptr == ';')
                        {
                           ptr++;

                           /* Ignore BTIME. */
                           while ((*ptr != ';') && (*ptr != '\n') &&
                                  (*ptr != '\0') && (*ptr != '\r'))
                           {
                              ptr++;
                           }

                           if (*ptr == ';')
                           {
                              ptr++;

                              /* Ignore ITIME */
                              while ((*ptr != ';') && (*ptr != '\n') &&
                                     (*ptr != '\0') && (*ptr != '\r'))
                              {
                                 ptr++;
                              }

                              if (*ptr == ';')
                              {
                                 ptr++;

                                 if ((*ptr == 'Y') && (*(ptr + 1) == ';') &&
                                     (isdigit((int)(*(ptr + 2)))))
                                 {
                                    char str_number[MAX_SHORT_LENGTH];

                                    str_number[0] = *(ptr + 2);
                                    ptr += 3;
                                    k = 1;
                                    while ((isdigit((int)(*ptr))) &&
                                           (k < (MAX_SHORT_LENGTH - 1)))
                                    {
                                       str_number[k] = *ptr;
                                       ptr++; k++;
                                    }
                                    str_number[k] = '\0';
                                    brcdb[i].rss = (unsigned short)atoi(str_number);
                                    if (rep_buffer != NULL)
                                    {
                                       get_report_data(&brcdb[i],
                                                       rep_header_end);
                                    }
                                    else
                                    {
                                       brcdb[i].rt = 0;
                                       brcdb[i].mimj[0] = '\0';
                                       brcdb[i].stid = 0;
                                    }
                                 }
                                 else
                                 {
                                    brcdb[i].rss = -1;
                                    brcdb[i].rt = 0;
                                    brcdb[i].mimj[0] = '\0';
                                    brcdb[i].stid = 0;
                                 }
                              }
                           }
                        }
                     }
                     else
                     {
                        brcdb[i].spec = 0;
                        brcdb[i].rss = -1;
                        brcdb[i].rt = 0;
                        brcdb[i].mimj[0] = '\0';
                        brcdb[i].stid = 0;
                     }
                  }
                  else
                  {
                     brcdb[i].type = 0;
                     brcdb[i].spec = 0;
                     brcdb[i].rss = -1;
                     brcdb[i].rt = 0;
                     brcdb[i].mimj[0] = '\0';
                     brcdb[i].stid = 0;
                  }
               }
               else
               {
                  brcdb[i].CCCC[0] = '\0';
                  brcdb[i].type = 0;
                  brcdb[i].spec = 0;
                  brcdb[i].rss = -1;
                  brcdb[i].rt = 0;
                  brcdb[i].mimj[0] = '\0';
                  brcdb[i].stid = 0;
               }
            } /* for (i = 0; i < no_of_brc_entries; i++) */

            if (verbose == YES)
            {
               system_log(INFO_SIGN, NULL, 0,
                          _("Found %d bulletin rules in `%s'."),
                          no_of_brc_entries, bul_file);
            }
         } /* if (no_of_brc_entries > 0) */
         else
         {
            if (verbose == YES)
            {
               system_log(INFO_SIGN, NULL, 0,
                          _("No bulletin rules found in `%s'"), bul_file);
            }
         }

         /* The buffer holding the contents of the bulletin */
         /* file is no longer needed.                       */
         free(buffer);

#ifdef _DEBUG_BUL_REP
         for (i = 0; i < no_of_brc_entries; i++)
         {
            system_log(DEBUG_SIGN, NULL, 0, "%s;%s;%d;%d;%d;%d;%c%c;%d",
                       brcdb[i].TTAAii, brcdb[i].CCCC, (int)brcdb[i].type,
                       (int)brcdb[i].spec, (int)brcdb[i].rss,
                       (int)brcdb[i].rt, brcdb[i].mimj[0],
                       brcdb[i].mimj[1], (int)brcdb[i].stid);
         }
#endif
      } /* if (stat_buf.st_mtime != last_read_bul) */

      free(rep_buffer);
   }

   return;
}


/*++++++++++++++++++++++++++ get_report_data() ++++++++++++++++++++++++++*/
static void
get_report_data(struct wmo_bul_rep *brcdb, char *rep_header_end)
{
   int  i;
   char *p_start_r,
        str_number[MAX_SHORT_LENGTH];

   p_start_r = rep_header_end;
   while (*p_start_r != '\0')
   {
      if (((brcdb->TTAAii[0] == '/') || (*p_start_r == brcdb->TTAAii[0])) &&
          ((brcdb->TTAAii[1] == '/') || (*(p_start_r + 1) == brcdb->TTAAii[1])) &&
          (*(p_start_r + 1) == ';'))
      {
         p_start_r += 3;
         i = 0;
         while ((i < (MAX_SHORT_LENGTH - 1)) && (isdigit((int)(*p_start_r))))
         {
            str_number[i] = *p_start_r;
            i++; p_start_r++;
         }
         str_number[i] = '\0';
         if (atoi(str_number) == brcdb->rss)
         {
            while ((*p_start_r != ';') && (*p_start_r != '\n') &&
                   (*p_start_r != '\0') && (*p_start_r != '\r'))
            {
               p_start_r++;
            }
            if (*p_start_r == ';')
            {
               p_start_r++;
               if ((*p_start_r == 'T') && (*(p_start_r + 1) == 'E') &&
                   (*(p_start_r + 2) == 'X') && (*(p_start_r + 3) == 'T') &&
                   (*(p_start_r + 4) == ';'))
               {
                  brcdb->rt = RT_TEXT;
                  p_start_r += 4;
               }
               else if ((*p_start_r == 'C') && (*(p_start_r + 1) == 'L') &&
                        (*(p_start_r + 2) == 'I') &&
                        (*(p_start_r + 3) == 'M') &&
                        (*(p_start_r + 4) == 'A') &&
                        (*(p_start_r + 5) == 'T') &&
                        (*(p_start_r + 6) == ';'))
                    {
                       brcdb->rt = RT_CLIMAT;
                       p_start_r += 6;
                    }
               else if ((*p_start_r == 'T') && (*(p_start_r + 1) == 'A') &&
                        (*(p_start_r + 2) == 'F') && (*(p_start_r + 3) == ';'))
                    {
                       brcdb->rt = RT_TAF;
                       p_start_r += 3;
                    }
               else if ((*p_start_r == 'M') && (*(p_start_r + 1) == 'E') &&
                        (*(p_start_r + 2) == 'T') &&
                        (*(p_start_r + 3) == 'A') &&
                        (*(p_start_r + 4) == 'R') &&
                        (*(p_start_r + 5) == ';'))
                    {
                       brcdb->rt = RT_METAR;
                       p_start_r += 5;
                    }
               else if ((*p_start_r == 'S') && (*(p_start_r + 1) == 'P') &&
                        (*(p_start_r + 2) == 'E') &&
                        (*(p_start_r + 3) == 'C') &&
                        (*(p_start_r + 4) == 'I') &&
                        (*(p_start_r + 5) == 'A') &&
                        (*(p_start_r + 6) == 'L') &&
                        (*(p_start_r + 7) == '-') &&
                        (*(p_start_r + 8) == '0') &&
                        (*(p_start_r + 9) == '1') &&
                        (*(p_start_r + 10) == ';'))
                    {
                       brcdb->rt = RT_SPECIAL_01;
                       p_start_r += 10;
                    }
               else if ((*p_start_r == 'S') && (*(p_start_r + 1) == 'P') &&
                        (*(p_start_r + 2) == 'E') &&
                        (*(p_start_r + 3) == 'C') &&
                        (*(p_start_r + 4) == 'I') &&
                        (*(p_start_r + 5) == 'A') &&
                        (*(p_start_r + 6) == 'L') &&
                        (*(p_start_r + 7) == '-') &&
                        (*(p_start_r + 8) == '0') &&
                        (*(p_start_r + 9) == '2') &&
                        (*(p_start_r + 10) == ';'))
                    {
                       brcdb->rt = RT_SPECIAL_02;
                       p_start_r += 10;
                    }
               else if ((*p_start_r == 'S') && (*(p_start_r + 1) == 'P') &&
                        (*(p_start_r + 2) == 'E') &&
                        (*(p_start_r + 3) == 'C') &&
                        (*(p_start_r + 4) == 'I') &&
                        (*(p_start_r + 5) == 'A') &&
                        (*(p_start_r + 6) == 'L') &&
                        (*(p_start_r + 7) == '-') &&
                        (*(p_start_r + 8) == '0') &&
                        (*(p_start_r + 9) == '3') &&
                        (*(p_start_r + 10) == ';'))
                    {
                       brcdb->rt = RT_SPECIAL_03;
                       p_start_r += 10;
                    }
               else if ((*p_start_r == 'S') && (*(p_start_r + 1) == 'P') &&
                        (*(p_start_r + 2) == 'E') &&
                        (*(p_start_r + 3) == 'C') &&
                        (*(p_start_r + 4) == 'I') &&
                        (*(p_start_r + 5) == 'A') &&
                        (*(p_start_r + 6) == 'L') &&
                        (*(p_start_r + 7) == '-') &&
                        (*(p_start_r + 8) == '6') &&
                        (*(p_start_r + 9) == '6') &&
                        (*(p_start_r + 10) == ';'))
                    {
                       brcdb->rt = RT_SPECIAL_66;
                       p_start_r += 10;
                    }
               else if ((*p_start_r == 'S') && (*(p_start_r + 1) == 'Y') &&
                        (*(p_start_r + 2) == 'N') &&
                        (*(p_start_r + 3) == 'O') &&
                        (*(p_start_r + 4) == 'P') &&
                        (*(p_start_r + 5) == ';'))
                    {
                       brcdb->rt = RT_SYNOP;
                       p_start_r += 5;
                    }
               else if ((*p_start_r == 'S') && (*(p_start_r + 1) == 'Y') &&
                        (*(p_start_r + 2) == 'N') &&
                        (*(p_start_r + 3) == 'O') &&
                        (*(p_start_r + 4) == 'P') &&
                        (*(p_start_r + 5) == '-') &&
                        (*(p_start_r + 6) == 'S') &&
                        (*(p_start_r + 7) == 'H') &&
                        (*(p_start_r + 8) == 'I') &&
                        (*(p_start_r + 9) == 'P') &&
                        (*(p_start_r + 10) == ';'))
                    {
                       brcdb->rt = RT_SYNOP_SHIP;
                       p_start_r += 10;
                    }
               else if ((*p_start_r == 'U') && (*(p_start_r + 1) == 'P') &&
                        (*(p_start_r + 2) == 'P') &&
                        (*(p_start_r + 3) == 'E') &&
                        (*(p_start_r + 4) == 'R') &&
                        (*(p_start_r + 5) == '-') &&
                        (*(p_start_r + 6) == 'A') &&
                        (*(p_start_r + 7) == 'I') &&
                        (*(p_start_r + 8) == 'R') &&
                        (*(p_start_r + 9) == ';'))
                    {
                       brcdb->rt = RT_UPPER_AIR;
                       p_start_r += 9;
                    }
                    else
                    {
                       brcdb->rt = 0;
                       while ((*p_start_r != ';') && (*p_start_r != '\n') &&
                              (*p_start_r != '\0') && (*p_start_r != '\r'))
                       {
                          p_start_r++;
                       }
                    }

               if (*p_start_r == ';')
               {
                  p_start_r++;
                  brcdb->mimj[0] = *p_start_r;
                  if (*(p_start_r + 1) != ';')
                  {
                     brcdb->mimj[1] = *(p_start_r + 1);
                     p_start_r += 2;

                     if (*p_start_r == ';')
                     {
                        p_start_r++;
                        if (*p_start_r == 'D')
                        {
                           brcdb->stid = STID_IIiii;
                        }
                        else if (*p_start_r == 'L')
                             {
                                brcdb->stid = STID_CCCC;
                             }
                             else
                             {
                                brcdb->stid = 0;
                                return;
                             }
                     }
                     else
                     {
                        brcdb->stid = 0;
                        return;
                     }
                  }
                  else
                  {
                     brcdb->mimj[0] = '\0';
                     brcdb->stid = 0;
                     return;
                  }
               }
               else
               {
                  brcdb->mimj[0] = '\0';
                  brcdb->stid = 0;
                  return;
               }
            }
            else
            {
               brcdb->rt = 0;
               brcdb->mimj[0] = '\0';
               brcdb->stid = 0;
               return;
            }
         }
      }
      while ((*p_start_r != '\n') && (*p_start_r != '\r'))
      {
         p_start_r++;
      }
      if ((*p_start_r == '\n') || (*p_start_r == '\r'))
      {
         p_start_r++;
      }
   }

   /* Not found, so initialize with default values. */
   brcdb->rt = 0;
   brcdb->mimj[0] = '\0';
   brcdb->stid = 0;

   return;
}

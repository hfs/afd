/*
 *  init_text.c - Part of AFD, an automatic file distribution program.
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

#include "afddefs.h"

DESCR__S_M1
/*
 ** NAME
 **   init_text - Initializes text to be displayed
 **
 ** SYNOPSIS
 **   void init_text(void)
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
 **   16.03.1996 H.Kiehl Created
 **   31.05.1997 H.Kiehl Added debug toggle.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>     /* calloc(), free()                              */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _NO_MMAP
#include <sys/mman.h>   /* mmap(), munmap()                              */
#endif
#include <unistd.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <errno.h>
#include "afd_ctrl.h"
#include "show_log.h"
#include "logdefs.h"

extern Display        *display;
extern XtAppContext   app;
extern Cursor         cursor1;
extern Widget         toplevel,
                      log_output,
                      counterbox;
extern XmTextPosition wpr_position;
extern int            log_type_flag,
                      toggles_set,
                      line_counter,
                      no_of_hosts;
extern char           **hosts;
extern unsigned int   total_length,
                      toggles_set_parallel_jobs;
extern FILE           *p_log_file;


/*############################## init_text() ############################*/
void
init_text(void)
{
   int                  i,
                        fd = fileno(p_log_file),
                        length,
                        block_length,
                        last = MISS;
   char                 *src = NULL,
                        *dst = NULL,
                        *ptr,
                        *ptr_dst,
                        *ptr_start,
                        *ptr_line;
   struct stat          stat_buf;
   XSetWindowAttributes attrs;
   XEvent               event;

   if (fstat(fd, &stat_buf) < 0)
   {
      (void)xrec(toplevel, FATAL_DIALOG, "fstat() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   if (stat_buf.st_size > 0)
   {
      /* Change cursor to indicate we are doing something */
      attrs.cursor = cursor1;
      XChangeWindowAttributes(display, XtWindow(toplevel), CWCursor, &attrs);
      XFlush(display);

      /* Copy file to memory */
#ifdef _NO_MMAP
      if ((src = calloc(stat_buf.st_size, sizeof(char))) == NULL)
      {
         (void)xrec(toplevel, FATAL_DIALOG, "calloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      if (read(fd, src, stat_buf.st_size) != stat_buf.st_size)
      {
         (void)xrec(toplevel, FATAL_DIALOG, "read() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
#else
      if (lseek(fd, stat_buf.st_size, SEEK_SET) == -1)
      {
         (void)xrec(toplevel, FATAL_DIALOG, "lseek() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }

      if ((src = mmap(0, stat_buf.st_size, PROT_READ, MAP_SHARED, fd, 0)) == (caddr_t) -1)
      {
         (void)xrec(toplevel, FATAL_DIALOG, "mmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
#endif
      if ((dst = calloc(stat_buf.st_size + 1, sizeof(char))) == NULL)
      {
         (void)xrec(toplevel, FATAL_DIALOG, "calloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }

      ptr_start = ptr = src;
      ptr_dst = dst;
      block_length = 0;

      if (no_of_hosts > 0)
      {
         while (total_length < stat_buf.st_size)
         {
            length = 0;
            ptr_line = ptr;
            while ((*ptr != '\n') && (*ptr != '\0'))
            {
               length++; ptr++;
            }
            length++; ptr++;
            total_length += length;
            if ((log_type_flag == TRANSFER_LOG_TYPE) ||
                (log_type_flag == TRANS_DB_LOG_TYPE))
            {
               if ((length > (LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4)) &&
                   ((((toggles_set & SHOW_INFO) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'I')) ||
                    (((toggles_set & SHOW_WARN) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'W')) ||
                    (((toggles_set & SHOW_ERROR) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'E')) ||
                    (((toggles_set & SHOW_FATAL) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'F')) ||
                    (((toggles_set & SHOW_DEBUG) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'D')) ||
                    (((toggles_set & SHOW_DEBUG) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'D')) ||
#ifdef _TOGGLED_PROC_SELECTION
                    (((toggles_set_parallel_jobs & 1) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '0')) ||
                    (((toggles_set_parallel_jobs & 2) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '1')) ||
                    (((toggles_set_parallel_jobs & 4) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '2')) ||
                    (((toggles_set_parallel_jobs & 8) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '3')) ||
                    (((toggles_set_parallel_jobs & 16) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '4'))))
#else
                    (((toggles_set_parallel_jobs - 1) != (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) - 48)) &&
                    (toggles_set_parallel_jobs != 0))))
#endif
               {
                  if (last == HIT)
                  {
                     (void)memcpy(ptr_dst, ptr_start, block_length);
                     *(ptr_dst + block_length) = '\0';
                     ptr_dst += block_length;
                     block_length = 0;
                  }
                  last = MISS;
               }
               else
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     if (log_filter(hosts[i], ptr_line + 16) == 0)
                     {
                        i = no_of_hosts + 1;
                     }
                  }
                  if (i == (no_of_hosts + 2))
                  {
                     if (last == MISS)
                     {
                        ptr_start = ptr - length;
                     }
                     block_length += length;
                     line_counter++;
                     last = HIT;
                  }
                  else
                  {
                     if (last == HIT)
                     {
                        memcpy(ptr_dst, ptr_start, block_length);
                        *(ptr_dst + block_length) = '\0';
                        ptr_dst += block_length;
                        block_length = 0;
                     }
                     last = MISS;
                  }
               }
            }
            else
            {
               if ((length > LOG_SIGN_POSITION) &&
                   ((((toggles_set & SHOW_INFO) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'I')) ||
                    (((toggles_set & SHOW_CONFIG) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'C')) ||
                    (((toggles_set & SHOW_WARN) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'W')) ||
                    (((toggles_set & SHOW_ERROR) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'E')) ||
                    (((toggles_set & SHOW_FATAL) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'F')) ||
                    (((toggles_set & SHOW_DEBUG) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'D')) ||
                    (((toggles_set & SHOW_DEBUG) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'D'))))
               {
                  if (last == HIT)
                  {
                     (void)memcpy(ptr_dst, ptr_start, block_length);
                     *(ptr_dst + block_length) = '\0';
                     ptr_dst += block_length;
                     block_length = 0;
                  }
                  last = MISS;
               }
               else
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     if (log_filter(hosts[i], ptr_line + 16) == 0)
                     {
                        i = no_of_hosts + 1;
                     }
                  }
                  if (i == (no_of_hosts + 2))
                  {
                     if (last == MISS)
                     {
                        ptr_start = ptr - length;
                     }
                     block_length += length;
                     line_counter++;
                     last = HIT;
                  }
                  else
                  {
                     if (last == HIT)
                     {
                        memcpy(ptr_dst, ptr_start, block_length);
                        *(ptr_dst + block_length) = '\0';
                        ptr_dst += block_length;
                        block_length = 0;
                     }
                     last = MISS;
                  }
               }
            }
         }
      }
      else
      {
         while (total_length < stat_buf.st_size)
         {
            length = 0;
            ptr_line = ptr;
            while ((*ptr != '\n') && (*ptr != '\0'))
            {
               length++; ptr++;
            }
            length++; ptr++;
            total_length += length;
            if ((log_type_flag == TRANSFER_LOG_TYPE) ||
                (log_type_flag == TRANS_DB_LOG_TYPE))
            {
               if ((length > (LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4)) &&
                   ((((toggles_set & SHOW_INFO) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'I')) ||
                    (((toggles_set & SHOW_WARN) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'W')) ||
                    (((toggles_set & SHOW_ERROR) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'E')) ||
                    (((toggles_set & SHOW_FATAL) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'F')) ||
                    (((toggles_set & SHOW_DEBUG) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'D')) ||
#ifdef _TOGGLED_PROC_SELECTION
                    (((toggles_set_parallel_jobs & 1) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '0')) ||
                    (((toggles_set_parallel_jobs & 2) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '1')) ||
                    (((toggles_set_parallel_jobs & 4) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '2')) ||
                    (((toggles_set_parallel_jobs & 8) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '3')) ||
                    (((toggles_set_parallel_jobs & 16) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '4'))))
#else
                    (((toggles_set_parallel_jobs - 1) != (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) - 48)) &&
                    (toggles_set_parallel_jobs != 0))))
#endif
               {
                  if (last == HIT)
                  {
                     memcpy(ptr_dst, ptr_start, block_length);
                     *(ptr_dst + block_length) = '\0';
                     ptr_dst += block_length;
                     block_length = 0;
                  }
                  last = MISS;
               }
               else
               {
                  if (last == MISS)
                  {
                     ptr_start = ptr - length;
                  }
                  block_length += length;
                  line_counter++;
                  last = HIT;
               }
            }
            else
            {
               if ((length > LOG_SIGN_POSITION) &&
                   ((((toggles_set & SHOW_INFO) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'I')) ||
                    (((toggles_set & SHOW_CONFIG) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'C')) ||
                    (((toggles_set & SHOW_WARN) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'W')) ||
                    (((toggles_set & SHOW_ERROR) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'E')) ||
                    (((toggles_set & SHOW_FATAL) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'F')) ||
                    (((toggles_set & SHOW_DEBUG) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'D'))))
               {
                  if (last == HIT)
                  {
                     memcpy(ptr_dst, ptr_start, block_length);
                     *(ptr_dst + block_length) = '\0';
                     ptr_dst += block_length;
                     block_length = 0;
                  }
                  last = MISS;
               }
               else
               {
                  if (last == MISS)
                  {
                     ptr_start = ptr - length;
                  }
                  block_length += length;
                  line_counter++;
                  last = HIT;
               }
            }
         }
      }
      if (block_length > 0)
      {
         memcpy(ptr_dst, ptr_start, block_length);
         *(ptr_dst + block_length) = '\0';
      }

      XmTextInsert(log_output, 0, dst);
      wpr_position = total_length;
      XmTextShowPosition(log_output, wpr_position);

#ifdef _NO_MMAP
      free(src);
#else
      if (munmap(src, stat_buf.st_size) < 0)
      {
         (void)xrec(toplevel, WARN_DIALOG, "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
#endif
     if (dst != NULL)
     {
        free((void *)dst);
     }

      attrs.cursor = None;
      XChangeWindowAttributes(display, XtWindow(toplevel), CWCursor, &attrs);
      XFlush(display);

      /* Get rid of all events that have occurred */
      XSync(XtDisplay(toplevel), False);
      while (XCheckMaskEvent(XtDisplay(toplevel),
                             ButtonPressMask | ButtonReleaseMask |
                             ButtonMotionMask | PointerMotionMask |
                             KeyPressMask, &event) == TRUE)
      {
         /* do nothing */;
      }
   }

   return;
}

/*
 *  check_status.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_status - checks the status of each connection
 **
 ** SYNOPSIS
 **   void check_status(Widget w)
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
 **   17.02.1996 H.Kiehl Created
 **   03.09.1997 H.Kiehl Added LED for AFDD.
 **   12.01.2000 H.Kiehl Added receive log.
 **
 */
DESCR__E_M3

#include <stdio.h>               /* sprintf()                            */
#include <string.h>              /* strcmp(), strcpy(), memmove()        */
#include <math.h>                /* log10()                              */
#include <sys/times.h>           /* times()                              */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _NO_MMAP
#include <sys/mman.h>            /* mmap(), munmap()                     */
#endif
#include <signal.h>              /* kill()                               */
#include <fcntl.h>
#include <unistd.h>              /* close()                              */
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <Xm/Xm.h>
#include "afd_ctrl.h"

extern Display                    *display;
extern XtAppContext               app;
extern XtIntervalId               interval_id_status;
extern Widget                     appshell;
extern int                        sys_log_fd,
                                  glyph_width,
                                  redraw_time_status;
#ifndef _NO_MMAP
extern off_t                      afd_active_size;
#endif
extern time_t                     afd_active_time;
extern char                       blink_flag,
                                  *pid_list,
                                  afd_active_file[];
extern struct afd_status          *p_afd_status,
                                  prev_afd_status;
extern struct filetransfer_status *fsa;


/*########################### check_status() ############################*/
void
check_status(w)
Widget   w;
{
   static int  loop_timer = 0;
   static char blink = TR_BAR;
   signed char flush = NO;

   /*
    * Check if all process are still running.
    */
   if (prev_afd_status.amg != p_afd_status->amg)
   {
      if (p_afd_status->amg == OFF)
      {
         blink_flag = ON;
      }
      else if ((p_afd_status->amg == ON) &&
               (prev_afd_status.amg == OFF) &&
               (p_afd_status->fd != OFF) &&
               (p_afd_status->archive_watch != OFF) &&
               (p_afd_status->afdd != OFF))
              {
                 blink_flag = OFF;
              }
      prev_afd_status.amg = p_afd_status->amg;
      draw_proc_led(AMG_LED, prev_afd_status.amg);
      flush = YES;
   }
   if (prev_afd_status.fd != p_afd_status->fd)
   {
      if (p_afd_status->fd == OFF)
      {
         blink_flag = ON;
      }
      else if ((p_afd_status->fd == ON) &&
               (prev_afd_status.fd == OFF) &&
               (p_afd_status->amg != OFF) &&
               (p_afd_status->archive_watch != OFF) &&
               (p_afd_status->afdd != OFF))
              {
                 blink_flag = OFF;
              }
      prev_afd_status.fd = p_afd_status->fd;
      draw_proc_led(FD_LED, prev_afd_status.fd);
      flush = YES;
   }
   if (prev_afd_status.archive_watch != p_afd_status->archive_watch)
   {
      if (p_afd_status->archive_watch == OFF)
      {
         blink_flag = ON;
      }
      else if ((p_afd_status->archive_watch == ON) &&
               (prev_afd_status.archive_watch == OFF) &&
               (p_afd_status->amg != OFF) &&
               (p_afd_status->fd != OFF) &&
               (p_afd_status->afdd != OFF))
              {
                 blink_flag = OFF;
              }
      prev_afd_status.archive_watch = p_afd_status->archive_watch;
      draw_proc_led(AW_LED, prev_afd_status.archive_watch);
      flush = YES;
   }
   if (prev_afd_status.afdd != p_afd_status->afdd)
   {
      if (p_afd_status->afdd == OFF)
      {
         blink_flag = ON;
      }
      else if ((p_afd_status->afdd == ON) &&
               (prev_afd_status.afdd == OFF) &&
               (p_afd_status->amg != OFF) &&
               (p_afd_status->fd != OFF) &&
               (p_afd_status->archive_watch != OFF))
              {
                 blink_flag = OFF;
              }
      prev_afd_status.afdd = p_afd_status->afdd;
      draw_proc_led(AFDD_LED, prev_afd_status.afdd);
      flush = YES;
   }

   /*
    * If the AFD_ACTIVE file says a process is still running, it
    * is only true if init_afd still is alive. However, if this process
    * killed in such a way that it does not have a chance to
    * update the AFD_ACTIVE file, then this assumption is wrong.
    * So do check if the key process are still running.
    */
   loop_timer += redraw_time_status;
   if (loop_timer > 20000)
   {
      struct stat stat_buf;

      loop_timer = 0;

      if (stat(afd_active_file, &stat_buf) == 0)
      {
         if (stat_buf.st_mtime != afd_active_time)
         {
            int fd;

            if (pid_list != NULL)
            {
#ifdef _NO_MMAP
               (void)munmap_emu((void *)pid_list);
#else
               (void)munmap((void *)pid_list, afd_active_size);
#endif
            }
            afd_active_time = stat_buf.st_mtime;

            if ((fd = open(afd_active_file, O_RDWR)) < 0)
            {
               pid_list = NULL;
            }
            else
            {
#ifdef _NO_MMAP
               if ((pid_list = mmap_emu(0, stat_buf.st_size,
                                        (PROT_READ | PROT_WRITE), MAP_SHARED,
                                        afd_active_file, 0)) == (caddr_t) -1)
#else
               if ((pid_list = mmap(0, stat_buf.st_size,
                                    (PROT_READ | PROT_WRITE), MAP_SHARED,
                                    fd, 0)) == (caddr_t) -1)
#endif
               {
                  (void)xrec(appshell, ERROR_DIALOG,
                             "mmap() error : %s (%s %d)",
                             strerror(errno), __FILE__, __LINE__);
                  pid_list = NULL;
               }

               afd_active_size = stat_buf.st_size;

               if (close(fd) == -1)
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "close() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
               }
            }
         }

         if (pid_list != NULL)
         {
            if (kill(*(pid_t *)(pid_list + ((AMG_NO + 1) * sizeof(pid_t))), 0) == -1)
            {
               /*
                * Process is not alive, but it is in the AFD_ACTIVE
                * file?!
                */
               blink_flag = ON;
               p_afd_status->amg = OFF;
               prev_afd_status.amg = p_afd_status->amg;
               draw_proc_led(AMG_LED, prev_afd_status.amg);
               flush = YES;
            }
            if (kill(*(pid_t *)(pid_list + ((FD_NO + 1)  * sizeof(pid_t))), 0) == -1)
            {
               /*
                * Process is not alive, but it is in the AFD_ACTIVE
                * file?!
                */
               blink_flag = ON;
               p_afd_status->fd = OFF;
               prev_afd_status.fd = p_afd_status->fd;
               draw_proc_led(FD_LED, prev_afd_status.fd);
               flush = YES;
            }
            if (kill(*(pid_t *)(pid_list + ((AW_NO + 1) * sizeof(pid_t))), 0) == -1)
            {
               /*
                * Process is not alive, but it is in the AFD_ACTIVE
                * file?!
                */
               blink_flag = ON;
               p_afd_status->archive_watch = OFF;
               prev_afd_status.archive_watch = p_afd_status->archive_watch;
               draw_proc_led(AW_LED, prev_afd_status.archive_watch);
               flush = YES;
            }
            if (kill(*(pid_t *)(pid_list + ((AFDD_NO + 1) * sizeof(pid_t))), 0) == -1)
            {
               /*
                * Process is not alive, but it is in the AFD_ACTIVE
                * file?!
                */
               blink_flag = ON;
               p_afd_status->afdd = OFF;
               prev_afd_status.afdd = p_afd_status->afdd;
               draw_proc_led(AFDD_LED, prev_afd_status.afdd);
               flush = YES;
            }
         }
      }
   }

   if (blink_flag == ON)
   {
      if (prev_afd_status.amg == OFF)
      {
         draw_proc_led(AMG_LED, blink);
         flush = YES;
      }
      if (prev_afd_status.fd == OFF)
      {
         draw_proc_led(FD_LED, blink);
         flush = YES;
      }
      if (blink == TR_BAR)
      {
         blink = OFF;
      }
      else
      {
         blink = TR_BAR;
      }
   }

   /*
    * See if there is any activity in the log files.
    */
   if (prev_afd_status.receive_log_ec != p_afd_status->receive_log_ec)
   {
      prev_afd_status.receive_log_ec = p_afd_status->receive_log_ec;
      (void)memcpy(prev_afd_status.receive_log_fifo,
                   p_afd_status->receive_log_fifo,
                   LOG_FIFO_SIZE + 1);
      draw_log_status(RECEIVE_LOG_INDICATOR,
                      prev_afd_status.receive_log_ec % LOG_FIFO_SIZE);
      flush = YES;
   }
   if (prev_afd_status.sys_log_ec != p_afd_status->sys_log_ec)
   {
      prev_afd_status.sys_log_ec = p_afd_status->sys_log_ec;
      (void)memcpy(prev_afd_status.sys_log_fifo,
                   p_afd_status->sys_log_fifo,
                   LOG_FIFO_SIZE + 1);
      draw_log_status(SYS_LOG_INDICATOR,
                      prev_afd_status.sys_log_ec % LOG_FIFO_SIZE);
      flush = YES;
   }
   if (prev_afd_status.trans_log_ec != p_afd_status->trans_log_ec)
   {
      prev_afd_status.trans_log_ec = p_afd_status->trans_log_ec;
      (void)memcpy(prev_afd_status.trans_log_fifo,
                   p_afd_status->trans_log_fifo,
                   LOG_FIFO_SIZE + 1);
      draw_log_status(TRANS_LOG_INDICATOR,
                      prev_afd_status.trans_log_ec % LOG_FIFO_SIZE);
      flush = YES;
   }

   if (p_afd_status->jobs_in_queue != prev_afd_status.jobs_in_queue)
   {
      prev_afd_status.jobs_in_queue = p_afd_status->jobs_in_queue;
      draw_queue_counter(prev_afd_status.jobs_in_queue);
      flush = YES;
   }

   if (flush == YES)
   {
      XFlush(display);
      redraw_time_status = MIN_REDRAW_TIME;
   }
   else
   {
      if (redraw_time_status < 3500)
      {
         redraw_time_status += REDRAW_STEP_TIME;
      }
#ifdef _DEBUG
      (void)fprintf(stderr, "count_channels: Redraw time = %d\n",
                    redraw_time_status);
#endif
   }

   /* Redraw every redraw_time ms */
   interval_id_status = XtAppAddTimeOut(app, redraw_time_status,
                                        (XtTimerCallbackProc)check_status, w);
 
   return;
}

/*
 *  check_host_status.c - Part of AFD, an automatic file distribution
 *                        program.
 *  Copyright (c) 1996 - 1999 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_host_status - checks the status of each connection
 **
 ** SYNOPSIS
 **   void check_host_status(Widget w)
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
 **   18.01.1996 H.Kiehl Created
 **   30.08.1997 H.Kiehl Remove sprintf() from critical areas.
 **
 */
DESCR__E_M3

#include <stdio.h>               /* sprintf()                            */
#include <string.h>              /* strcmp(), strcpy(), memmove()        */
#include <stdlib.h>              /* calloc(), realloc(), free()          */
#include <math.h>                /* log10()                              */
#include <sys/times.h>           /* times()                              */
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <Xm/Xm.h>
#include "afd_ctrl.h"

extern Display                    *display;
extern XtAppContext               app;
extern XtIntervalId               interval_id_host,
                                  interval_id_tv;
extern GC                         color_gc;
extern Widget                     transviewshell;
extern Window                     detailed_window,
                                  line_window;
extern char                       line_style,
                                  *p_work_dir,
                                  tv_window;
extern float                      max_bar_length;
extern size_t                     current_jd_size;
extern int                        no_of_hosts,
                                  no_of_jobs_selected,
                                  glyph_height,
                                  glyph_width,
                                  led_width,
                                  redraw_time_host,
                                  bar_thickness_2,
                                  no_selected,
                                  no_of_columns,
                                  fsa_id,
                                  button_width,
                                  tv_no_of_rows,
                                  x_offset_proc;
#ifndef _NO_MMAP
extern off_t                      fsa_size;
#endif
extern unsigned int               text_offset;
extern unsigned short             step_size;
extern unsigned long              color_pool[];
extern clock_t                    clktck;
extern struct job_data            *jd;
extern struct line                *connect_data;
extern struct filetransfer_status *fsa;

/* local global variables */
struct tms                        tmsdummy;

/* Local function prototypes */
static void                       calc_transfer_rate(int);
static int                        check_fsa_data(char *),
                                  check_disp_data(char *, int);


/*######################### check_host_status() #########################*/
void
check_host_status(w)
Widget   w;
{
   unsigned char new_color;
   signed char   flush;
   int           i,
                 j,
                 x,
                 y,
                 pos,
                 prev_no_of_hosts = no_of_hosts,
                 led_changed = 0,
                 location_where_changed,
                 new_bar_length,
                 old_bar_length;
   double        tmp_transfer_rate;

   /* Initialise variables */
   location_where_changed = no_of_hosts + 10;
   flush = NO;

   /*
    * See if a host has been added or removed from the FSA.
    * If it changed resize the window.
    */
   if (check_fsa() == YES)
   {
      size_t      new_size = no_of_hosts * sizeof(struct line);
      struct line *new_connect_data;

      if ((new_connect_data = calloc(no_of_hosts, sizeof(struct line))) == NULL)
      {
         (void)xrec(w, FATAL_DIALOG, "calloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }

      /*
       * First try to copy the connect data from the old structure
       * so long as the hostnames are the same.
       */
      for (i = 0, location_where_changed = 0; i < prev_no_of_hosts; i++, location_where_changed++)
      {
         if (strcmp(connect_data[i].hostname, fsa[i].host_alias) == 0)
         {
            memmove(&new_connect_data[i], &connect_data[i],
                    sizeof(struct line));
         }
         else
         {
            break;
         }
      }

      for (i = location_where_changed; i < no_of_hosts; i++)
      {
         if ((pos = check_disp_data(fsa[i].host_alias, prev_no_of_hosts)) != INCORRECT)
         {
            (void)memmove(&new_connect_data[i], &connect_data[pos],
                          sizeof(struct line));
         }
         else /* A new host has been added */
         {
            /* Initialise values for new host */
            (void)strcpy(new_connect_data[i].hostname, fsa[i].host_alias);
            (void)sprintf(new_connect_data[i].host_display_str, "%-*s",
                          MAX_HOSTNAME_LENGTH, fsa[i].host_dsp_name);
            if (fsa[i].host_toggle_str[0] != '\0')
            {
               new_connect_data[i].host_toggle_display = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
            }
            else
            {
               new_connect_data[i].host_toggle_display = fsa[i].host_dsp_name[(int)fsa[i].toggle_pos];
            }
            new_connect_data[i].stat_color_no = NORMAL_STATUS;
            new_connect_data[i].debug = fsa[i].debug;
            new_connect_data[i].start_time = times(&tmsdummy);
            new_connect_data[i].total_file_counter = fsa[i].total_file_counter;
            CREATE_FC_STRING(new_connect_data[i].str_tfc, new_connect_data[i].total_file_counter);
            new_connect_data[i].total_file_size = fsa[i].total_file_size;
            CREATE_FS_STRING(new_connect_data[i].str_tfs, new_connect_data[i].total_file_size);
            new_connect_data[i].bytes_per_sec = 0;
            (void)strcpy(new_connect_data[i].str_tr, "  0B");
            new_connect_data[i].average_tr = 0.0;
            new_connect_data[i].max_average_tr = 0.0;
            new_connect_data[i].max_errors = fsa[i].max_errors;
            new_connect_data[i].error_counter = fsa[i].error_counter;
            if (fsa[i].host_status > 1) /* PAUSE_QUEUE_STAT AUTO_PAUSE_QUEUE_LOCK_STAT   */
                                        /* AUTO_PAUSE_QUEUE_STAT DANGER_PAUSE_QUEUE_STAT */
            {
               if (fsa[i].host_status & PAUSE_QUEUE_STAT)
               {
                  new_connect_data[i].status_led[0] = PAUSE_QUEUE;
               }
               else
               {
                  new_connect_data[i].status_led[0] = AUTO_PAUSE_QUEUE;
               }
            }
            else
            {
               new_connect_data[i].status_led[0] = NORMAL_STATUS;
            }
            if (fsa[i].host_status & STOP_TRANSFER_STAT)
            {
               new_connect_data[i].status_led[1] = STOP_TRANSFER;
            }
            else
            {
               new_connect_data[i].status_led[1] = NORMAL_STATUS;
            }
            CREATE_EC_STRING(new_connect_data[i].str_ec, new_connect_data[i].error_counter);
            if (fsa[i].max_errors < 2)
            {
               new_connect_data[i].scale = (double)max_bar_length;
            }
            else
            {
               new_connect_data[i].scale = max_bar_length / new_connect_data[i].max_errors;
            }
            new_connect_data[i].bar_length[TR_BAR_NO] = 0;
            new_connect_data[i].bar_length[ERROR_BAR_NO] = 0;
            new_connect_data[i].inverse = OFF;
            new_connect_data[i].expose_flag = NO;
            new_connect_data[i].special_flag = fsa[i].special_flag | 191;
            new_connect_data[i].allowed_transfers = fsa[i].allowed_transfers;
            for (j = 0; j < new_connect_data[i].allowed_transfers; j++)
            {
               new_connect_data[i].no_of_files[j] = fsa[i].job_status[j].no_of_files - fsa[i].job_status[j].no_of_files_done;
               new_connect_data[i].bytes_send[j] = fsa[i].job_status[j].bytes_send;
               new_connect_data[i].detailed_selection[j] = NO;
               if (fsa[i].job_status[j].connect_status != 0)
               {
                  new_connect_data[i].connect_status[j] = fsa[i].job_status[j].connect_status;
               }
               else
               {
                  new_connect_data[i].connect_status[j] = WHITE;
               }
            }

            /*
             * If this line has been selected in the old
             * connect_data structure, we have to make sure
             * that this host has not been deleted. If it
             * is deleted reduce the select counter!
             */
            if (connect_data[i].inverse == ON)
            {
               if ((pos = check_fsa_data(connect_data[i].hostname)) == INCORRECT)
               {
                  /* Host has been deleted */
                  no_selected--;
               }
            }
         }
      }

      /*
       * Ensure that we really have checked all hosts in old
       * structure.
       */
      if (prev_no_of_hosts > no_of_hosts)
      {
         while (i < prev_no_of_hosts)
         {
            if (connect_data[i].inverse == ON)
            {
               if ((pos = check_fsa_data(connect_data[i].hostname)) == INCORRECT)
               {
                  /* Host has been deleted */
                  no_selected--;
               }
            }
            i++;
         }
      }

      if ((connect_data = realloc(connect_data, new_size)) == NULL)
      {
         (void)xrec(w, FATAL_DIALOG, "realloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }

      /* Activate the new connect_data structure */
      memmove(&connect_data[0], &new_connect_data[0],
              no_of_hosts * sizeof(struct line));

      free(new_connect_data);

      /* Resize window if necessary */
      if (resize_window() == YES)
      {
         if (no_of_columns != 0)
         {
            location_where_changed = 0;
         }
      }

      /* When no. of channels have been reduced, then delete */
      /* removed channels from end of list.                  */
      for (i = prev_no_of_hosts; i > no_of_hosts; i--)
      {
         draw_blank_line(i - 1);
      }

      /*
       * Change the detailed transfer window if it is active.
       */
      if (no_of_jobs_selected > 0)
      {
         int             new_no_of_jobs_selected = 0;
         size_t          new_current_jd_size = 0;
         struct job_data *new_jd = NULL;

         for (i = 0; i < no_of_hosts; i++)
         {
            for (j = 0; j < connect_data[i].allowed_transfers; j++)
            {
               if (connect_data[i].detailed_selection[j] == YES)
               {
                  new_no_of_jobs_selected++;
                  if (new_no_of_jobs_selected == 1)
                  {
                     size_t new_size = 5 * sizeof(struct job_data);

                     new_current_jd_size = new_size;
                     if ((new_jd = malloc(new_size)) == NULL)
                     {
                        (void)xrec(w, FATAL_DIALOG,
                                   "malloc() error [%d] : %s [%d] (%s %d)",
                                   new_size, strerror(errno), errno,
                                   __FILE__, __LINE__);
                        no_of_jobs_selected = 0;
                        return;
                     }
                  }
                  else if ((new_no_of_jobs_selected % 5) == 0)
                       {
                          size_t new_size = ((new_no_of_jobs_selected / 5) + 1) *
                                            5 * sizeof(struct job_data);

                          if (new_current_jd_size < new_size)
                          {
                             new_current_jd_size = new_size;
                             if ((new_jd = realloc(new_jd, new_size)) == NULL)
                             {
                                (void)xrec(w, FATAL_DIALOG,
                                           "realloc() error [%d] : %s [%d] (%s %d)",
                                           new_size, strerror(errno), errno,
                                           __FILE__, __LINE__);
                                no_of_jobs_selected = 0;
                                return;
                             }
                          }
                       }
                  init_jd_structure(&new_jd[new_no_of_jobs_selected - 1], i, j);
               }
            }
         } /* for (i = 0; i < no_of_hosts; i++) */

         if (new_no_of_jobs_selected > 0)
         {
            size_t new_size = new_no_of_jobs_selected * sizeof(struct job_data);

            if (new_no_of_jobs_selected != no_of_jobs_selected)
            {
               no_of_jobs_selected = new_no_of_jobs_selected;

               if (new_current_jd_size > current_jd_size)
               {
                  current_jd_size = new_current_jd_size;
                  if ((jd = realloc(jd, new_size)) == NULL)
                  {
                     (void)xrec(w, FATAL_DIALOG,
                                "realloc() error : %s (%s %d)",
                                strerror(errno), __FILE__, __LINE__);
                     no_of_jobs_selected = 0;
                     return;
                  }
               }

               (void)resize_tv_window();
            }
            if (new_jd != NULL)
            {
               (void)memcpy(jd, new_jd, new_size);
               free(new_jd);
            }

            for (i = 0; i < no_of_jobs_selected; i++)
            {
               draw_detailed_line(i);
            }
         }
         else
         {
            no_of_jobs_selected = 0;
            XtRemoveTimeOut(interval_id_tv);
            free(jd);
            jd = NULL;
            XtPopdown(transviewshell);
         }
      } /* if (no_of_jobs_selected > 0) */

      /* Make sure changes are drawn !!! */
      flush = YES;
   } /* if (check_fsa() == YES) */

   /* Change information for each remote host if necessary */
   for (i = 0; i < no_of_hosts; i++)
   {
      x = y = -1;

      if (connect_data[i].allowed_transfers != fsa[i].allowed_transfers)
      {
         locate_xy(i, &x, &y);
         if (connect_data[i].allowed_transfers < fsa[i].allowed_transfers)
         {
            for (j = connect_data[i].allowed_transfers; j < fsa[i].allowed_transfers; j++)
            {
               draw_proc_stat(i, j, x, y);
            }
         }
         else
         {
            int       offset;
            XGCValues gc_values;

            for (j = fsa[i].allowed_transfers; j < connect_data[i].allowed_transfers; j++)
            {
               offset = j * (button_width + BUTTON_SPACING);

               if (connect_data[i].inverse == OFF)
               {
                  gc_values.foreground = color_pool[DEFAULT_BG];
               }
               else if (connect_data[i].inverse == ON)
                    {
                       gc_values.foreground = color_pool[BLACK];
                    }
                    else
                    {
                       gc_values.foreground = color_pool[LOCKED_INVERSE];
                    }
               XChangeGC(display, color_gc, GCForeground, &gc_values);
               XFillRectangle(display, line_window, color_gc,
                              DEFAULT_FRAME_SPACE + x + x_offset_proc + offset - 1,
                              y + SPACE_ABOVE_LINE - 1,
                              button_width + 2,
                              glyph_height + 2);

               /* Update detailed selection. */
               if ((no_of_jobs_selected > 0) &&
                   (connect_data[i].detailed_selection[j] == YES))
               {
                  no_of_jobs_selected--;
                  connect_data[i].detailed_selection[j] = NO;
                  if (no_of_jobs_selected == 0)
                  {
                     XtRemoveTimeOut(interval_id_tv);
                     free(jd);
                     jd = NULL;
                     XtPopdown(transviewshell);
                     tv_window = OFF;
                  }
                  else
                  {
                     int k, m, tmp_tv_no_of_rows;

                     /* Remove detailed selection. */
                     for (k = 0; k < (no_of_jobs_selected + 1); k++)
                     {
                        if ((jd[k].job_no == j) &&
                            (strcmp(jd[k].hostname, connect_data[i].hostname) == 0))
                        {
                           if (k != no_of_jobs_selected)
                           {
                              size_t move_size = (no_of_jobs_selected - k) * sizeof(struct job_data);

                              (void)memmove(&jd[k], &jd[k + 1], move_size);
                           }
                           break;
                        }
                     }

                     for (m = k; m < no_of_jobs_selected; m++)
                     {
                        draw_detailed_line(m);
                     }

                     tmp_tv_no_of_rows = tv_no_of_rows;
                     if (resize_tv_window() == YES)
                     {
                        for (k = (tmp_tv_no_of_rows - 1); k < no_of_jobs_selected; k++)
                        {
                           draw_detailed_line(k);
                        }
                     }

                     draw_tv_blank_line(m);
                     XFlush(display);
                  }
               }
            }
         }
         connect_data[i].allowed_transfers = fsa[i].allowed_transfers;
         flush = YES;
      }

      if (connect_data[i].max_errors != fsa[i].max_errors)
      {
         connect_data[i].max_errors = fsa[i].max_errors;

         /*
          * Hmmm. What now? We cannot do anything here since
          * we cannot assume that the afd_ctrl is always
          * running.
          */
      }

      /* For each transfer, see if number of files changed */
      for (j = 0; j < fsa[i].allowed_transfers; j++)
      {
         if (connect_data[i].connect_status[j] != fsa[i].job_status[j].connect_status)
         {
            connect_data[i].connect_status[j] = fsa[i].job_status[j].connect_status;

            if (connect_data[i].no_of_files[j] != (fsa[i].job_status[j].no_of_files - fsa[i].job_status[j].no_of_files_done))
            {
               connect_data[i].no_of_files[j] = fsa[i].job_status[j].no_of_files - fsa[i].job_status[j].no_of_files_done;
            }

            locate_xy(i, &x, &y);

            /* Draw changed process file counter button */
            draw_proc_stat(i, j, x, y);

            flush = YES;
         }
         else if (connect_data[i].no_of_files[j] != (fsa[i].job_status[j].no_of_files - fsa[i].job_status[j].no_of_files_done))
              {
                 connect_data[i].no_of_files[j] = fsa[i].job_status[j].no_of_files - fsa[i].job_status[j].no_of_files_done;

                 locate_xy(i, &x, &y);

                 /* Draw changed process file counter button */
                 draw_proc_stat(i, j, x, y);

                 flush = YES;
              }
      }

      if (connect_data[i].special_flag != (fsa[i].special_flag | 191))
      {
         connect_data[i].special_flag = fsa[i].special_flag | 191;

         if (i < location_where_changed)
         {
            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }
            draw_dest_identifier(i, x, y);
            flush = YES;
         }
      }

      if (connect_data[i].special_flag & HOST_IN_DIR_CONFIG)
      {
         /* Did any significant change occur in the status */
         /* for this host?                                 */
         if (fsa[i].special_flag & HOST_DISABLED)
         {
            new_color = WHITE;
         }
         else if (fsa[i].error_counter >= fsa[i].max_errors)
              {
                 new_color = NOT_WORKING2;
              }
              else if (fsa[i].active_transfers > 0)
                   {
                      new_color = TRANSFER_ACTIVE; /* Transferring files */
                   }
                   else if (fsa[i].host_status == FAULTY_TRANSFERS)
                        {
                           new_color = FAULTY_TRANSFERS;
                        }
                        else
                        {
                           new_color = NORMAL_STATUS; /* Nothing to do but connection active */
                        }

         if (connect_data[i].stat_color_no != new_color)
         {
            connect_data[i].stat_color_no = new_color;

            if (i < location_where_changed)
            {
               if (x == -1)
               {
                  locate_xy(i, &x, &y);
               }
               draw_dest_identifier(i, x, y);
               flush = YES;
            }
         }
      }

      /* Host switched ? */
      if (connect_data[i].host_toggle != fsa[i].host_toggle)
      {
         connect_data[i].host_toggle = fsa[i].host_toggle;

         if (fsa[i].host_toggle_str[0] != '\0')
         {
            fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
            connect_data[i].host_display_str[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
            connect_data[i].host_toggle_display = connect_data[i].host_display_str[(int)fsa[i].toggle_pos];

            if (i < location_where_changed)
            {
               if (x == -1)
               {
                  locate_xy(i, &x, &y);
               }
               draw_dest_identifier(i, x, y);
               flush = YES;
            }

            /* Don't forget to redraw display name of tv window. */
            if (no_of_jobs_selected > 0)
            {
               int ii;

               for (ii = 0; ii < no_of_jobs_selected; ii++)
               {
                  if (jd[ii].fsa_no == i)
                  {
                     int x, y;

                     while ((ii < no_of_jobs_selected) && (jd[ii].fsa_no == i))
                     {
                        jd[ii].host_display_str[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
                        tv_locate_xy(ii, &x, &y);
                        draw_tv_dest_identifier(ii, x, y);
                        ii++;
                     }
                     break;
                  }
               }
            }
         }
      }

      /* Did the toggle information change? */
      if (connect_data[i].host_toggle_display != fsa[i].host_dsp_name[(int)fsa[i].toggle_pos])
      {
         connect_data[i].host_toggle_display = fsa[i].host_dsp_name[(int)fsa[i].toggle_pos];

         if (fsa[i].host_toggle_str[0] != '\0')
         {
            connect_data[i].host_display_str[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
         }
         else
         {
            connect_data[i].host_display_str[(int)fsa[i].toggle_pos] = ' ';
         }

         if (i < location_where_changed)
         {
            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }
            draw_dest_identifier(i, x, y);
            flush = YES;
         }

         /* Don't forget to redraw display name of tv window. */
         if (no_of_jobs_selected > 0)
         {
            int ii;

            for (ii = 0; ii < no_of_jobs_selected; ii++)
            {
               if (jd[ii].fsa_no == i)
               {
                  int x, y;

                  while ((ii < no_of_jobs_selected) && (jd[ii].fsa_no == i))
                  {
                     jd[ii].host_display_str[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
                     tv_locate_xy(ii, &x, &y);
                     draw_tv_dest_identifier(ii, x, y);
                     ii++;
                  }
                  break;
               }
            }
         }
      }

      /*
       * DEBUG LED
       * =========
       */
      if (connect_data[i].debug != fsa[i].debug)
      {
         connect_data[i].debug = fsa[i].debug;

         if (i < location_where_changed)
         {
            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }
            draw_debug_led(i, x, y);
            flush = YES;
         }
      }

      /*
       * STATUS LED
       * ==========
       */
      if (fsa[i].host_status > 1) /* PAUSE_QUEUE_STAT AUTO_PAUSE_QUEUE_LOCK_STAT   */
                                  /* AUTO_PAUSE_QUEUE_STAT DANGER_PAUSE_QUEUE_STAT */
      {
         if (fsa[i].host_status & PAUSE_QUEUE_STAT)
         {
            if (connect_data[i].status_led[0] != PAUSE_QUEUE)
            {
               connect_data[i].status_led[0] = PAUSE_QUEUE;
               led_changed |= LED_ONE;
            }
         }
         else
         {
            if (connect_data[i].status_led[0] != AUTO_PAUSE_QUEUE)
            {
               connect_data[i].status_led[0] = AUTO_PAUSE_QUEUE;
               led_changed |= LED_ONE;
            }
         }
      }
      else
      {
         if (connect_data[i].status_led[0] != NORMAL_STATUS)
         {
            connect_data[i].status_led[0] = NORMAL_STATUS;
            led_changed |= LED_ONE;
         }
      }
      if (fsa[i].host_status & STOP_TRANSFER_STAT)
      {
         if (connect_data[i].status_led[1] != STOP_TRANSFER)
         {
            connect_data[i].status_led[1] = STOP_TRANSFER;
            led_changed |= LED_TWO;
         }
      }
      else
      {
         if (connect_data[i].status_led[1] != NORMAL_STATUS)
         {
            connect_data[i].status_led[1] = NORMAL_STATUS;
            led_changed |= LED_TWO;
         }
      }
      if (led_changed > 0)
      {
         if (i < location_where_changed)
         {
            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }
            if (led_changed & LED_ONE)
            {
               draw_led(i, 0, x, y);
            }
            if (led_changed & LED_TWO)
            {
               draw_led(i, 1, x + led_width + LED_SPACING, y);
            }
            led_changed = 0;
            flush = YES;
         }
      }

      /*
       * CHARACTER INFORMATION
       * =====================
       *
       * If in character mode see if any change took place,
       * if so, redraw only those characters.
       */
      if (line_style != BARS_ONLY)
      {
         /*
          * Number of files to be send (NF)
          */
         if (connect_data[i].total_file_counter != fsa[i].total_file_counter)
         {
            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }

            connect_data[i].total_file_counter = fsa[i].total_file_counter;
            CREATE_FC_STRING(connect_data[i].str_tfc, connect_data[i].total_file_counter);

            if (i < location_where_changed)
            {
               draw_chars(i, NO_OF_FILES, x, y);
               flush = YES;
            }
         }

         /*
          * Total File Size (TFS)
          */
         if (connect_data[i].total_file_size != fsa[i].total_file_size)
         {
            char tmp_string[5];

            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }

            connect_data[i].total_file_size = fsa[i].total_file_size;
            CREATE_FS_STRING(tmp_string, connect_data[i].total_file_size);

            if ((tmp_string[2] != connect_data[i].str_tfs[2]) ||
                (tmp_string[1] != connect_data[i].str_tfs[1]) ||
                (tmp_string[0] != connect_data[i].str_tfs[0]) ||
                (tmp_string[3] != connect_data[i].str_tfs[3]))
            {
               connect_data[i].str_tfs[0] = tmp_string[0];
               connect_data[i].str_tfs[1] = tmp_string[1];
               connect_data[i].str_tfs[2] = tmp_string[2];
               connect_data[i].str_tfs[3] = tmp_string[3];

               if (i < location_where_changed)
               {
                  draw_chars(i, TOTAL_FILE_SIZE, x + (5 * glyph_width), y);
                  flush = YES;
               }
            }
         }

         /*
          * Transfer Rate (TR)
          */
         tmp_transfer_rate = connect_data[i].bytes_per_sec;
         calc_transfer_rate(i);

         /* NOTE: We show the actual active transfer rate at this */
         /*       moment. When drawing the bar we show the        */
         /*       average transfer rate.                          */
         /*       ^^^^^^^                                         */
         if (connect_data[i].bytes_per_sec != tmp_transfer_rate)
         {
            char tmp_string[5];

            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }

            CREATE_FS_STRING(tmp_string, connect_data[i].bytes_per_sec);

            if ((tmp_string[2] != connect_data[i].str_tr[2]) ||
                (tmp_string[1] != connect_data[i].str_tr[1]) ||
                (tmp_string[0] != connect_data[i].str_tr[0]) ||
                (tmp_string[3] != connect_data[i].str_tr[3]))
            {
               connect_data[i].str_tr[0] = tmp_string[0];
               connect_data[i].str_tr[1] = tmp_string[1];
               connect_data[i].str_tr[2] = tmp_string[2];
               connect_data[i].str_tr[3] = tmp_string[3];

               if (i < location_where_changed)
               {
                  draw_chars(i, TRANSFER_RATE, x + (10 * glyph_width), y);
                  flush = YES;
               }
            }
         }

         /*
          * Error Counter (EC)
          */
         if (connect_data[i].error_counter != fsa[i].error_counter)
         {
            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }

            /*
             * If line_style is CHARACTERS_AND_BARS don't update
             * the connect_data structure. Otherwise when we draw the bar
             * we will not notice any change. There we will then update
             * the structure.
             */
            if (line_style == CHARACTERS_ONLY)
            {
               connect_data[i].error_counter = fsa[i].error_counter;
            }
            CREATE_EC_STRING(connect_data[i].str_ec, fsa[i].error_counter);

            if (i < location_where_changed)
            {
               draw_chars(i, ERROR_COUNTER, x + (15 * glyph_width), y);
               flush = YES;
            }
         }
      }

      /*
       * BAR INFORMATION
       * ===============
       */
      if (line_style != CHARACTERS_ONLY)
      {
         /*
          * Transfer Rate Bar
          */
         /* Did we already calculate the transfer rate? */
         if (line_style == BARS_ONLY)
         {
            calc_transfer_rate(i);
         }

         if (connect_data[i].average_tr > 1.0)
         {
            if (connect_data[i].max_average_tr < 2.0)
            {
               new_bar_length = (log10(connect_data[i].average_tr) *
                                 max_bar_length /
                                 log10((double) 2.0));
            }
            else
            {
               new_bar_length = (log10(connect_data[i].average_tr) *
                                 max_bar_length /
                                 log10(connect_data[i].max_average_tr));
            }
         }
         else
         {
            new_bar_length = 0;
         }

         if ((connect_data[i].bar_length[TR_BAR_NO] != new_bar_length) &&
             (new_bar_length < max_bar_length))
         {
            old_bar_length = connect_data[i].bar_length[TR_BAR_NO];
            connect_data[i].bar_length[TR_BAR_NO] = new_bar_length;

            if (i < location_where_changed)
            {
               if (x == -1)
               {
                  locate_xy(i, &x, &y);
               }

               if (old_bar_length < new_bar_length)
               {
                  draw_bar(i, 1, TR_BAR_NO, x, y);
               }
               else
               {
                  draw_bar(i, -1, TR_BAR_NO, x, y);
               }

               if (flush != YES)
               {
                  flush = YUP;
               }
            }
         }
         else if ((new_bar_length >= max_bar_length) &&
                  (connect_data[i].bar_length[TR_BAR_NO] < max_bar_length))
              {
                 connect_data[i].bar_length[TR_BAR_NO] = max_bar_length;
                 if (i < location_where_changed)
                 {
                    if (x == -1)
                    {
                       locate_xy(i, &x, &y);
                    }

                    draw_bar(i, 1, TR_BAR_NO, x, y);

                    if (flush != YES)
                    {
                       flush = YUP;
                    }
                 }
              }

         /*
          * Error Bar
          */
         if (connect_data[i].error_counter != fsa[i].error_counter)
         {
            connect_data[i].error_counter = fsa[i].error_counter;
            if (connect_data[i].error_counter >= connect_data[i].max_errors)
            {
               new_bar_length = max_bar_length;
            }
            else
            {
               new_bar_length = connect_data[i].error_counter * connect_data[i].scale;
            }
            if (connect_data[i].bar_length[ERROR_BAR_NO] != new_bar_length)
            {
               connect_data[i].red_color_offset = new_bar_length * step_size;
               connect_data[i].green_color_offset = MAX_INTENSITY - connect_data[i].red_color_offset;

               if (i < location_where_changed)
               {
                  if (x == -1)
                  {
                     locate_xy(i, &x, &y);
                  }

                  if (connect_data[i].bar_length[ERROR_BAR_NO] < new_bar_length)
                  {
                     connect_data[i].bar_length[ERROR_BAR_NO] = new_bar_length;
                     draw_bar(i, 1, ERROR_BAR_NO, x, y + bar_thickness_2);
                  }
                  else
                  {
                     connect_data[i].bar_length[ERROR_BAR_NO] = new_bar_length;
                     draw_bar(i, -1, ERROR_BAR_NO, x, y + bar_thickness_2);
                  }
                  flush = YES;
               }
            }
         }
      }

      /* Redraw the line */
      if (i >= location_where_changed)
      {
#ifdef _DEBUG
         (void)fprintf(stderr, "count_channels: i = %d\n", i);
#endif
         flush = YES;
         draw_line_status(i, 1);
      }
   }

   /* Make sure all changes are shown */
   if ((flush == YES) || (flush == YUP))
   {
      XFlush(display);

      if (flush != YUP)
      {
         redraw_time_host = MIN_REDRAW_TIME;
      }
   }
   else
   {
      if (redraw_time_host < MAX_REDRAW_TIME)
      {
         redraw_time_host += REDRAW_STEP_TIME;
      }
#ifdef _DEBUG
      (void)fprintf(stderr, "count_channels: Redraw time = %d\n", redraw_time_host);
#endif
   }

   /* Redraw every redraw_time_host ms */
   interval_id_host = XtAppAddTimeOut(app, redraw_time_host,
                                      (XtTimerCallbackProc)check_host_status, w);
 
   return;
}


/*+++++++++++++++++++++++++ calc_transfer_rate() ++++++++++++++++++++++++*/
static void
calc_transfer_rate(int i)
{
   int          j;
   unsigned int bytes_send = 0;
   time_t       end_time,
                delta_time;

   end_time = times(&tmsdummy);
   if ((delta_time = (end_time - connect_data[i].start_time)) == 0)
   {
      delta_time = 1;
   }
   connect_data[i].start_time = end_time;

   for (j = 0; j < fsa[i].allowed_transfers; j++)
   {
      if (fsa[i].job_status[j].bytes_send != connect_data[i].bytes_send[j])
      {
         /* Check if an overrun has occurred */
         if (fsa[i].job_status[j].bytes_send < connect_data[i].bytes_send[j])
         {
            connect_data[i].bytes_send[j] = 0;
         }

         bytes_send += (fsa[i].job_status[j].bytes_send - connect_data[i].bytes_send[j]);

         connect_data[i].bytes_send[j] = fsa[i].job_status[j].bytes_send;
      }
   }

   if (bytes_send > 0)
   {
      connect_data[i].bytes_per_sec = (double)(bytes_send) / delta_time * clktck;

      /* arithmetischer Mittelwert */
      connect_data[i].average_tr = (connect_data[i].average_tr +
                                   connect_data[i].bytes_per_sec) / 2.0;

      if (connect_data[i].average_tr > connect_data[i].max_average_tr)
      {
         connect_data[i].max_average_tr = connect_data[i].average_tr;
      }
   }
   else
   {
      connect_data[i].bytes_per_sec = 0;

      if (connect_data[i].average_tr > 0.0)
      {
         /* arithmetischer Mittelwert */
         connect_data[i].average_tr = (connect_data[i].average_tr +
                                      connect_data[i].bytes_per_sec) / 2.0;

         if (connect_data[i].average_tr > connect_data[i].max_average_tr)
         {
            connect_data[i].max_average_tr = connect_data[i].average_tr;
         }
      }
   }

   return;
}


/*+++++++++++++++++++++++++++ check_fsa_data() ++++++++++++++++++++++++++*/
static int
check_fsa_data(char *hostname)
{
   static int i;

   for (i = 0; i < no_of_hosts; i++)
   {
      if (strcmp(fsa[i].host_alias, hostname) == 0)
      {
         return(i);
      }
   }

   return(INCORRECT);
}


/*++++++++++++++++++++++++++ check_disp_data() ++++++++++++++++++++++++++*/
static int
check_disp_data(char *hostname, int prev_no_of_hosts)
{
   static int i;

   for (i = 0; i < prev_no_of_hosts; i++)
   {
      if (strcmp(connect_data[i].hostname, hostname) == 0)
      {
         return(i);
      }
   }

   return(INCORRECT);
}

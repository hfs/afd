/*
 *  mouse_handler.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mouse_handler - handles all mouse- AND key events
 **
 ** SYNOPSIS
 **   void focus(Widget w, XtPointer client_data, XEvent *event)
 **   void input(Widget w, XtPointer client_data, XtPointer call_data)
 **   void popup_menu_cb(Widget w, XtPointer client_data, XEvent *event)
 **   void save_setup_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void popup_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void control_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_font_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_row_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_style_cb(Widget w, XtPointer client_data, XtPointer call_data)
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
 **   27.01.1996 H.Kiehl Created
 **   30.07.2001 H.Kiehl Support for show_queue dialog.
 **   09.11.2002 H.Kiehl Short line support.
 **   27.02.2003 H.Kiehl Set error_counter to zero when AUTO_PAUSE_QUEUE_STAT
 **                      is set.
 **
 */
DESCR__E_M3

#include <stdio.h>             /* fprintf(), stderr                      */
#include <string.h>            /* strcpy(), strlen()                     */
#include <stdlib.h>            /* atoi(), exit()                         */
#include <sys/types.h>
#include <sys/wait.h>          /* waitpid()                              */
#include <fcntl.h>
#include <signal.h>            /* kill()                                 */
#include <unistd.h>            /* fork()                                 */
#ifndef _NO_MMAP
#include <sys/mman.h>          /* munmap()                               */
#endif
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/DrawingA.h>
#include "show_log.h"
#include "afd_ctrl.h"
#include "permission.h"

/* External global variables */
extern Display                    *display;
extern XtAppContext               app;
extern XtIntervalId               interval_id_tv;
extern Widget                     fw[],
                                  rw[],
                                  lsw[],
                                  appshell,
                                  transviewshell;
extern Window                     detailed_window,
                                  line_window,
                                  short_line_window;
extern XFontStruct                *font_struct;
extern GC                         letter_gc,
                                  normal_letter_gc,
                                  locked_letter_gc,
                                  color_letter_gc,
                                  default_bg_gc,
                                  normal_bg_gc,
                                  locked_bg_gc,
                                  label_bg_gc,
                                  button_bg_gc,
                                  tr_bar_gc,
                                  color_gc,
                                  black_line_gc,
                                  white_line_gc,
                                  led_gc;
extern int                        button_width,
                                  current_font,
                                  current_row,
                                  filename_display_length,
                                  line_height,
                                  *line_length,
                                  no_of_active_process,
                                  no_of_hosts,
                                  no_of_jobs_selected,
                                  no_selected,
                                  no_selected_static,
                                  no_of_long_lines,
                                  no_of_short_lines,
				  no_of_short_columns,
				  no_of_short_rows,
                                  no_of_rows,
                                  no_of_rows_set,
                                  short_line_length,
                                  sys_log_fd,
                                  tv_no_of_columns,
                                  tv_no_of_rows,
                                  window_width,
                                  x_offset_proc;
#ifndef _NO_MMAP
extern off_t                      afd_active_size;
#endif
extern float                      max_bar_length;
extern unsigned long              color_pool[];
extern char                       *p_work_dir,
                                  *pid_list,
                                  line_style,
                                  font_name[],
                                  *ping_cmd,
                                  *ptr_ping_cmd,
                                  *traceroute_cmd,
                                  *ptr_traceroute_cmd,
                                  tv_window,
                                  user[];
extern struct job_data            *jd;
extern struct line                *connect_data;
extern struct afd_status          *p_afd_status;
extern struct filetransfer_status *fsa;
extern struct afd_control_perm    acp;
extern struct apps_list           *apps_list;

/* Local global variables */
static int                        in_window = NO;

/* Global variables */
size_t                            current_jd_size = 0;

/* Local function prototypes */
static int                        to_long(int, int, int),
                                  to_short(int, int, int);
static void                       redraw_long(int),
                                  redraw_short(void);


/*############################## focus() ################################*/
void
focus(Widget w, XtPointer client_data, XEvent *event)
{
   if (event->xany.type == EnterNotify)
   {
      in_window = YES;
   }
   if (event->xany.type == LeaveNotify)
   {
      in_window = NO;
   }

   return;
}


/*############################## input() ################################*/
void
input(Widget w, XtPointer client_data, XEvent *event)
{
   int        select_no;
   static int last_motion_pos = -1;

   /* Handle any motion event */
   if ((event->xany.type == MotionNotify) && (in_window == YES))
   {
      int column = 0,
          dummy_length = event->xbutton.x;

      do
      {
         dummy_length -= line_length[column];
         column++;
      } while (dummy_length > 0);
      column--;
      select_no = (event->xbutton.y / line_height) + (column * no_of_rows);

      if ((select_no < no_of_long_lines) && (last_motion_pos != select_no))
      {
         int pos;

         if ((pos = get_long_pos(select_no, YES)) == -1)
         {
            return;
         }
         if (event->xkey.state & ControlMask)
         {
            if (connect_data[pos].inverse == STATIC)
            {
               connect_data[pos].inverse = OFF;
               no_selected_static--;
            }
            else
            {
               connect_data[pos].inverse = STATIC;
               no_selected_static++;
            }

            draw_line_status(pos, 1);
            XFlush(display);
         }
         else if (event->xkey.state & ShiftMask)
              {
                 if (connect_data[pos].inverse == ON)
                 {
                    connect_data[pos].inverse = OFF;
                    no_selected--;
                 }
                 else if (connect_data[pos].inverse == STATIC)
                      {
                         connect_data[pos].inverse = OFF;
                         no_selected_static--;
                      }
                      else
                      {
                         connect_data[pos].inverse = ON;
                         no_selected++;
                      }

                 draw_line_status(pos, 1);
                 XFlush(display);
              }
      }
      last_motion_pos = select_no;

      return;
   }

   /* Handle any button press event. */
   if (event->xbutton.button == 1)
   {
      int column = 0,
          dummy_length = event->xbutton.x;

      do
      {
         dummy_length -= line_length[column];
         column++;
      } while (dummy_length > 0);
      column--;
      select_no = (event->xbutton.y / line_height) + (column * no_of_rows);

      /* Make sure that this field does contain a channel */
      if (select_no < no_of_long_lines)
      {
         int pos;

         if ((pos = get_long_pos(select_no, YES)) == -1)
         {
            return;
         }
         if (((event->xkey.state & Mod1Mask) ||
             (event->xkey.state & Mod4Mask)) &&
             (event->xany.type == ButtonPress))
         {
            int gotcha = NO,
                i;

            for (i = 0; i < no_of_active_process; i++)
            {
               if ((apps_list[i].position == select_no) &&
                   (CHECK_STRCMP(apps_list[i].progname, AFD_INFO) == 0))
               {
                  gotcha = YES;
                  break;
               }
            }
            if (gotcha == NO)
            {
               char *args[6],
                    progname[MAX_PATH_LENGTH];

               args[0] = progname;
               args[1] = "-f";
               args[2] = font_name;
               args[3] = "-h";
               args[4] = fsa[pos].host_alias;
               args[5] = NULL;
               (void)strcpy(progname, AFD_INFO);

               make_xprocess(progname, progname, args, select_no);
            }
            else
            {
               (void)xrec(appshell, INFO_DIALOG,
                          "Information dialog for %s is already open on your display.",
                          fsa[pos].host_alias);
            }
         }
         else if (event->xany.type == ButtonPress)
              {
                 if (event->xkey.state & ControlMask)
                 {
                    if (connect_data[pos].inverse == STATIC)
                    {
                       connect_data[pos].inverse = OFF;
                       no_selected_static--;
                    }
                    else
                    {
                       connect_data[pos].inverse = STATIC;
                       no_selected_static++;
                    }

                    draw_line_status(pos, 1);
                    XFlush(display);
                 }
                 else if (event->xkey.state & ShiftMask)
                      {
                         if (connect_data[pos].inverse == ON)
                         {
                            connect_data[pos].inverse = OFF;
                            no_selected--;
                         }
                         else if (connect_data[pos].inverse == STATIC)
                              {
                                 connect_data[pos].inverse = OFF;
                                 no_selected_static--;
                              }
                              else
                              {
                                 connect_data[pos].inverse = ON;
                                 no_selected++;
                              }

                         draw_line_status(pos, 1);
                         XFlush(display);
                      }

                 last_motion_pos = select_no;
              }
#ifdef _DEBUG
         (void)fprintf(stderr, "input(): no_selected = %d    select_no = %d\n",
                       no_selected, select_no);
         (void)fprintf(stderr, "input(): xbutton.x     = %d\n",
                       event->xbutton.x);
         (void)fprintf(stderr, "input(): xbutton.y     = %d\n",
                       event->xbutton.y);
#endif
      }
   }

   if ((acp.view_jobs != NO_PERMISSION) &&
       ((event->xbutton.button == 2) || (event->xbutton.button == 3)) &&
       (event->xkey.state & ControlMask))
   {
      int column = 0,
          dummy_length = event->xbutton.x;

      do
      {
         dummy_length -= line_length[column];
         column++;
      } while (dummy_length > 0);
      column--;
      select_no = (event->xbutton.y / line_height) + (column * no_of_rows);

      /* Make sure that this field does contain a channel */
      if (select_no < no_of_long_lines)
      {
         int pos,
             x_pos,
             min_length = x_offset_proc;

         if ((pos = get_long_pos(select_no, YES)) == -1)
         {
            return;
         }
         if (dummy_length < 0)
         {
            x_pos = dummy_length + line_length[column];
         }
         else
         {
            x_pos = 0;
         }

         /* See if this is a proc_stat area. */
         if ((x_pos > min_length) &&
             (x_pos < (min_length + (fsa[pos].allowed_transfers * (button_width + BUTTON_SPACING)) - BUTTON_SPACING)))
         {
            int job_no;

            x_pos -= min_length;
            for (job_no = 0; job_no < fsa[pos].allowed_transfers; job_no++)
            {
               x_pos -= button_width;
               if (x_pos <= 0)
               {
                  if (connect_data[pos].detailed_selection[job_no] == YES)
                  {
                     connect_data[pos].detailed_selection[job_no] = NO;
                     no_of_jobs_selected--;
                     if (no_of_jobs_selected == 0)
                     {
                        XtRemoveTimeOut(interval_id_tv);
                        if (jd != NULL)
                        {
                           free(jd);
                           jd = NULL;
                        }
                        if (transviewshell != (Widget)NULL)
                        {
                           XtPopdown(transviewshell);
                        }
                        tv_window = OFF;
                     }
                     else
                     {
                        int i, j, tmp_tv_no_of_rows;

                        /* Remove detailed selection. */
                        for (i = 0; i < (no_of_jobs_selected + 1); i++)
                        {
                           if ((jd[i].job_no == job_no) &&
                               (CHECK_STRCMP(jd[i].hostname, connect_data[pos].hostname) == 0))
                           {
                              if (i != no_of_jobs_selected)
                              {
                                 size_t move_size = (no_of_jobs_selected - i) * sizeof(struct job_data);

                                 (void)memmove(&jd[i], &jd[i + 1], move_size);
                              }
                              break;
                           }
                        }

                        for (j = i; j < no_of_jobs_selected; j++)
                        {
                           draw_detailed_line(j);
                        }

                        tmp_tv_no_of_rows = tv_no_of_rows;
                        if (resize_tv_window() == YES)
                        {
                           for (i = (tmp_tv_no_of_rows - 1); i < no_of_jobs_selected; i++)
                           {
                              draw_detailed_line(i);
                           }
                        }

                        draw_tv_blank_line(j);
                        XFlush(display);
                     }
                  }
                  else
                  {
                     no_of_jobs_selected++;
                     connect_data[pos].detailed_selection[job_no] = YES;
                     if (no_of_jobs_selected == 1)
                     {
                        size_t new_size = 5 * sizeof(struct job_data);

                        current_jd_size = new_size;
                        if ((jd = malloc(new_size)) == NULL)
                        {
                           (void)xrec(appshell, FATAL_DIALOG,
                                      "malloc() error [%d] : %s [%d] (%s %d)",
                                      new_size, strerror(errno),
                                      errno, __FILE__, __LINE__);
                           return;
                        }

                        /* Fill job_data structure. */
                        init_jd_structure(&jd[0], select_no, job_no);

                        if ((transviewshell == (Widget)NULL) ||
                            (XtIsRealized(transviewshell) == False) ||
                            (XtIsSensitive(transviewshell) != True))
                        {
                           create_tv_window();
                        }
                        else
                        {
                           draw_detailed_line(0);
                           interval_id_tv = XtAppAddTimeOut(app,
                                                            STARTING_REDRAW_TIME,
                                                            (XtTimerCallbackProc)check_tv_status,
                                                            w);
                        }
                        XtPopup(transviewshell, XtGrabNone);
                        tv_window = ON;
                     }
                     else
                     {
                        int i,
                            fsa_pos = -1;

                        if ((no_of_jobs_selected % 5) == 0)
                        {
                           size_t new_size = ((no_of_jobs_selected / 5) + 1) *
                                             5 * sizeof(struct job_data);

                           if (current_jd_size < new_size)
                           {
                              current_jd_size = new_size;
                              if ((jd = realloc(jd, new_size)) == NULL)
                              {
                                 (void)xrec(appshell, FATAL_DIALOG,
                                            "realloc() error [%d] : %s [%d] (%s %d)",
                                            new_size, strerror(errno),
                                            errno, __FILE__, __LINE__);
                                 return;
                              }
                           }
                        }

                        /*
                         * Add new detailed selection to list. First
                         * determine where this one is to be inserted.
                         */
                        for (i = 0; i < (no_of_jobs_selected - 1); i++)
                        {
                           if (CHECK_STRCMP(jd[i].hostname, connect_data[pos].hostname) == 0)
                           {
                              if (jd[i].job_no > job_no)
                              {
                                 fsa_pos = i;
                                 break;
                              }
                              else
                              {
                                 fsa_pos = i + 1;
                              }
                           }
                           else if (fsa_pos != -1)
                                {
                                   break;
                                }
                           else if (pos < jd[i].fsa_no)
                                {
                                   fsa_pos = i;
                                }
                        }
                        if (fsa_pos == -1)
                        {
                           fsa_pos = no_of_jobs_selected - 1;
                        }
                        else if (fsa_pos != (no_of_jobs_selected - 1))
                             {
                                size_t move_size = (no_of_jobs_selected - fsa_pos) * sizeof(struct job_data);

                                (void)memmove(&jd[fsa_pos + 1], &jd[fsa_pos], move_size);
                             }

                        /* Fill job_data structure. */
                        init_jd_structure(&jd[fsa_pos], pos, job_no);

                        if ((resize_tv_window() == YES) &&
                            (tv_no_of_columns > 1))
                        {
                           fsa_pos = tv_no_of_rows - 1;
                        }
                        for (i = fsa_pos; i < no_of_jobs_selected; i++)
                        {
                           draw_detailed_line(i);
                        }

                        XFlush(display);
                     }
                  }
                  draw_detailed_selection(pos, job_no);
                  break;
               }
               x_pos -= BUTTON_SPACING;
               if (x_pos < 0)
               {
                  break;
               }
            }
         }
      }
      return;
   }

   /* Convert long line to short line. */
   if ((event->xbutton.button == 2) &&
       (((event->xkey.state & Mod1Mask) || (event->xkey.state & Mod4Mask)) &&
        (event->xany.type == ButtonPress)))
   {
      int column = 0,
          dummy_length = event->xbutton.x;

      do
      {
         dummy_length -= line_length[column];
         column++;
      } while (dummy_length > 0);
      column--;
      select_no = (event->xbutton.y / line_height) + (column * no_of_rows);

      /* Make sure that this field does contain a channel. */
      if (select_no < no_of_long_lines)
      {
         (void)to_short(-1, select_no, YES);
      }
   }

   return;
}


/*########################### short_input() #############################*/
void
short_input(Widget w, XtPointer client_data, XEvent *event)
{
   if ((event->xbutton.x < (no_of_short_columns * short_line_length)) &&
       (event->xbutton.y < (no_of_short_rows * line_height)))
   {
      int        select_no;
      static int last_motion_pos = -1;

      /* Handle any motion event */
      if ((event->xany.type == MotionNotify) && (in_window == YES))
      {
         select_no = ((event->xbutton.y / line_height) * no_of_short_columns) +
                      (event->xbutton.x / short_line_length);
         if ((select_no < no_of_short_lines) && (last_motion_pos != select_no))
         {
            int pos;

            if ((pos = get_short_pos(select_no, YES)) == -1)
            {
               return;
            }
            if (event->xkey.state & ControlMask)
            {
               if (connect_data[pos].inverse == STATIC)
               {
                  connect_data[pos].inverse = OFF;
                  no_selected_static--;
               }
               else
               {
                  connect_data[pos].inverse = STATIC;
                  no_selected_static++;
               }

               draw_line_status(pos, 1);
               XFlush(display);
            }
            else if (event->xkey.state & ShiftMask)
                 {
                    if (connect_data[pos].inverse == ON)
                    {
                       connect_data[pos].inverse = OFF;
                       no_selected--;
                    }
                    else if (connect_data[pos].inverse == STATIC)
                         {
                            connect_data[pos].inverse = OFF;
                            no_selected_static--;
                         }
                         else
                         {
                            connect_data[pos].inverse = ON;
                            no_selected++;
                         }

                    draw_line_status(pos, 1);
                    XFlush(display);
                 }
         }
         last_motion_pos = select_no;

         return;
      }

      /* Handle button one press event. */
      if (event->xbutton.button == 1)
      {
         select_no = ((event->xbutton.y / line_height) * no_of_short_columns) +
                      (event->xbutton.x / short_line_length);

         /* Make sure that this field does contain a channel */
         if (select_no < no_of_short_lines)
         {
            int pos;

            if ((pos = get_short_pos(select_no, YES)) == -1)
            {
               return;
            }
            if (((event->xkey.state & Mod1Mask) ||
                (event->xkey.state & Mod4Mask)) &&
                (event->xany.type == ButtonPress))
            {
               int gotcha = NO,
                   i;

               for (i = 0; i < no_of_active_process; i++)
               {
                  if ((apps_list[i].position == select_no) &&
                      (CHECK_STRCMP(apps_list[i].progname, AFD_INFO) == 0))
                  {
                     gotcha = YES;
                     break;
                  }
               }
               if (gotcha == NO)
               {
                  char *args[6],
                       progname[MAX_PATH_LENGTH];

                  args[0] = progname;
                  args[1] = "-f";
                  args[2] = font_name;
                  args[3] = "-h";
                  args[4] = fsa[pos].host_alias;
                  args[5] = NULL;
                  (void)strcpy(progname, AFD_INFO);

                  make_xprocess(progname, progname, args, select_no);
               }
               else
               {
                  (void)xrec(appshell, INFO_DIALOG,
                             "Information dialog for %s is already open on your display.",
                             fsa[pos].host_alias);
               }
            }
            else if (event->xany.type == ButtonPress)
                 {
                    if (event->xkey.state & ControlMask)
                    {
                       if (connect_data[pos].inverse == STATIC)
                       {
                          connect_data[pos].inverse = OFF;
                          no_selected_static--;
                       }
                       else
                       {
                          connect_data[pos].inverse = STATIC;
                          no_selected_static++;
                       }

                       draw_line_status(pos, 1);
                       XFlush(display);
                    }
                    else if (event->xkey.state & ShiftMask)
                         {
                            if (connect_data[pos].inverse == ON)
                            {
                               connect_data[pos].inverse = OFF;
                               no_selected--;
                            }
                            else if (connect_data[pos].inverse == STATIC)
                                 {
                                    connect_data[pos].inverse = OFF;
                                    no_selected_static--;
                                 }
                                 else
                                 {
                                    connect_data[pos].inverse = ON;
                                    no_selected++;
                                 }

                            draw_line_status(pos, 1);
                            XFlush(display);
                         }

                    last_motion_pos = select_no;
                 }
#ifdef _DEBUG
            (void)fprintf(stderr, "short_input(): no_selected = %d    select_no = %d\n",
                          no_selected, select_no);
            (void)fprintf(stderr, "short_input(): xbutton.x     = %d\n",
                          event->xbutton.x);
            (void)fprintf(stderr, "short_input(): xbutton.y     = %d\n",
                          event->xbutton.y);
#endif
         }
      }

      /* Convert short line to long. */
      if ((event->xbutton.button == 2) &&
          (((event->xkey.state & Mod1Mask) || (event->xkey.state & Mod4Mask)) &&
           (event->xany.type == ButtonPress)))
      {
         select_no = ((event->xbutton.y / line_height) * no_of_short_columns) +
                      (event->xbutton.x / short_line_length);

         /* Make sure that this field does contain a channel. */
         if (select_no < no_of_short_lines)
         {
            (void)to_long(-1, select_no, YES);
         }
      }
   }

   return;
}


/*############################ popup_menu_cb() ##########################*/
void
popup_menu_cb(Widget w, XtPointer client_data, XEvent *event)
{
   Widget popup = (Widget)client_data;

   if ((event->xany.type != ButtonPress) ||
       (event->xbutton.button != 3) ||
       (event->xkey.state & ControlMask))
   {
      return;
   }

   /* Position the menu where the event occurred */
   XmMenuPosition(popup, (XButtonPressedEvent *) (event));
   XtManageChild(popup);

   return;
}


/*########################## save_setup_cb() ############################*/
void
save_setup_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
   char **hosts = NULL;

   if (no_of_short_lines > 0)
   {
      int i, j = 0;

      RT_ARRAY(hosts, no_of_short_lines, (MAX_HOSTNAME_LENGTH + 1), char);
      for (i = 0; i < no_of_hosts; i++)
      {
         if (connect_data[i].short_pos > -1)
         {
            (void)strcpy(hosts[j], connect_data[i].hostname);
            j++;
         }
      }
   }
   write_setup(filename_display_length, -1, hosts,
               no_of_short_lines, MAX_HOSTNAME_LENGTH);
   if (no_of_short_lines > 0)
   {
      FREE_RT_ARRAY(hosts);
   }

   return;
}


/*############################# popup_cb() ##############################*/
void
popup_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
   int              ehc = YES,
                    sel_typ = (int)client_data,
                    to_long_counter = 0,
                    to_short_counter = 0,
                    hosts_found,
                    i,
#ifdef _DEBUG
                    j,
#endif
                    k;
   char             host_config_file[MAX_PATH_LENGTH],
                    host_err_no[1025],
                    progname[MAX_PROCNAME_LENGTH + 1],
                    *p_user,
                    **hosts = NULL,
                    **args = NULL,
                    log_typ[30],
                    display_error,
#ifdef _FIFO_DEBUG
                    cmd[2],
#endif
       	            err_msg[1025 + 100];
   size_t           new_size = (no_of_hosts + 8) * sizeof(char *);
   struct host_list *hl = NULL;

   if ((no_selected == 0) && (no_selected_static == 0) &&
       ((sel_typ == QUEUE_SEL) || (sel_typ == TRANS_SEL) ||
        (sel_typ == DISABLE_SEL) || (sel_typ == SWITCH_SEL) ||
        (sel_typ == RETRY_SEL) || (sel_typ == DEBUG_SEL) ||
        (sel_typ == INFO_SEL) || (sel_typ == VIEW_DC_SEL) ||
        (sel_typ == PING_SEL) || (sel_typ == TRACEROUTE_SEL) ||
        (sel_typ == LONG_SHORT_SEL)))
   {
      (void)xrec(appshell, INFO_DIALOG,
                 "You must first select a host!\nUse mouse button 1 together with the SHIFT or CTRL key.");
      return;
   }
   RT_ARRAY(hosts, no_of_hosts, (MAX_HOSTNAME_LENGTH + 1), char);
   if ((args = malloc(new_size)) == NULL)
   {
      (void)xrec(appshell, FATAL_DIALOG, "malloc() error : %s [%d] (%s %d)",
                 strerror(errno), errno, __FILE__, __LINE__);
      return;
   }

   switch(sel_typ)
   {
      case QUEUE_SEL:
      case TRANS_SEL:
      case DISABLE_SEL:
         (void)sprintf(host_config_file, "%s%s%s",
                       p_work_dir, ETC_DIR, DEFAULT_HOST_CONFIG_FILE);
         ehc = eval_host_config(&hosts_found, host_config_file, &hl, NO);
         if ((ehc == NO) && (no_of_hosts != hosts_found))
         {
            (void)xrec(appshell, WARN_DIALOG,
                       "Hosts found in HOST_CONFIG (%d) and those currently storred (%d) are not the same. Unable to do any changes. (%s %d)",
                       no_of_hosts, hosts_found, __FILE__, __LINE__);
            ehc = YES;
         }
         else if (ehc == YES)
              {
                 (void)xrec(appshell, WARN_DIALOG,
                            "Unable to retrieve data from HOST_CONFIG, therefore no values changed in it! (%s %d)",
                            __FILE__, __LINE__);
              }
         break;

      case SWITCH_SEL:
      case RETRY_SEL:
      case DEBUG_SEL:
      case LONG_SHORT_SEL:
         break;

      case PING_SEL : /* Ping test */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = ping_cmd;
         args[6] = NULL;
         (void)strcpy(progname, SHOW_CMD);
         break;

      case TRACEROUTE_SEL : /* Traceroute test */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = traceroute_cmd;
         args[6] = NULL;
         (void)strcpy(progname, SHOW_CMD);
         break;

      case INFO_SEL : /* Information */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = "-h";
         args[7] = NULL;
         (void)strcpy(progname, AFD_INFO);
         break;

      case S_LOG_SEL : /* System Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = "-l";
         args[6] = log_typ;
         args[7] = NULL;
         (void)strcpy(progname, SHOW_LOG);
         (void)strcpy(log_typ, SYSTEM_STR);
         make_xprocess(progname, progname, args, -1);
         return;

      case R_LOG_SEL : /* Receive Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = "-l";
         args[6] = log_typ;
         args[7] = NULL;
         (void)strcpy(progname, SHOW_LOG);
         (void)strcpy(log_typ, RECEIVE_STR);
         make_xprocess(progname, progname, args, -1);
         return;

      case T_LOG_SEL : /* Transfer Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = "-l";
         args[6] = log_typ;
         (void)strcpy(progname, SHOW_LOG);
         break;

      case D_LOG_SEL : /* Transfer Debug Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = "-l";
         args[6] = log_typ;
         (void)strcpy(progname, SHOW_LOG);
         break;
  
      case I_LOG_SEL : /* Input Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         (void)strcpy(progname, SHOW_ILOG);
         break;

      case O_LOG_SEL : /* Output Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         (void)strcpy(progname, SHOW_OLOG);
         break;

      case E_LOG_SEL : /* Delete Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         (void)strcpy(progname, SHOW_RLOG);
         break;

      case SHOW_QUEUE_SEL : /* View AFD Queue */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         (void)strcpy(progname, SHOW_QUEUE);
         break;

      case VIEW_FILE_LOAD_SEL : /* File Load */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = log_typ;
         args[4] = "-f";
         args[5] = font_name;
         args[6] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_FILE_LOAD);
         make_xprocess(progname, progname, args, -1);
         return;

      case VIEW_KBYTE_LOAD_SEL : /* KByte Load */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = log_typ;
         args[4] = "-f";
         args[5] = font_name;
         args[6] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_KBYTE_LOAD);
         make_xprocess(progname, progname, args, -1);
         return;

      case VIEW_CONNECTION_LOAD_SEL : /* Connection Load */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = log_typ;
         args[4] = "-f";
         args[5] = font_name;
         args[6] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_CONNECTION_LOAD);
         make_xprocess(progname, progname, args, -1);
         return;

      case VIEW_TRANSFER_LOAD_SEL : /* Active Transfers Load */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = log_typ;
         args[4] = "-f";
         args[5] = font_name;
         args[6] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_TRANSFER_LOAD);
         make_xprocess(progname, progname, args, -1);
         return;

#ifdef _WITH_VIEW_QUEUE
      case VIEW_QUEUE_SEL : /* View Queue */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = NULL;
         (void)strcpy(progname, SHOW_QUEUE);
         make_xprocess(progname, progname, args, -1);
         return;
#endif

      case VIEW_DC_SEL : /* View DIR_CONFIG entries */
         args[0] = progname;
         args[1] = "-f";
         args[2] = font_name;
         args[3] = WORK_DIR_ID;
         args[4] = p_work_dir;
         args[5] = "-h";
         args[7] = NULL;
         (void)strcpy(progname, VIEW_DC);
         break;

      case VIEW_JOB_SEL : /* View job details */
         if (tv_window == ON)
         {
            XtPopdown(transviewshell);
            tv_window = OFF;
         }
         else if ((tv_window == OFF) && (no_of_jobs_selected > 0))
              {
                 if (transviewshell == (Widget)NULL)
                 {
                    create_tv_window();
                    interval_id_tv = XtAppAddTimeOut(app,
                                                     STARTING_REDRAW_TIME,
                                                     (XtTimerCallbackProc)check_tv_status,
                                                     w);
                 }
                 XtPopup(transviewshell, XtGrabNone);
                 tv_window = ON;
              }
              else
              {
                 (void)xrec(appshell, INFO_DIALOG,
                            "No job marked. Mark with CTRL + Mouse button 2 or 3.",
                            sel_typ);
              }
         return;

      case EDIT_HC_SEL : /* Edit host configuration data. */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = NULL;
         (void)strcpy(progname, EDIT_HC);
         if ((p_user = lock_proc(EDIT_HC_LOCK_ID, YES)) != NULL)
         {
            (void)xrec(appshell, INFO_DIALOG,
                       "Only one user may use this dialog. Currently %s is using it.",
                       p_user);
         }
         else
         {
            make_xprocess(progname, progname, args, -1);
         }
         return;

      case DIR_CTRL_SEL : /* Directory Control. */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = NULL;
         (void)strcpy(progname, DIR_CTRL);
         make_xprocess(progname, progname, args, -1);
         return;

      case EXIT_SEL  : /* Exit */
         XFreeFont(display, font_struct);
         XFreeGC(display, letter_gc);
         XFreeGC(display, normal_letter_gc);
         XFreeGC(display, locked_letter_gc);
         XFreeGC(display, color_letter_gc);
         XFreeGC(display, default_bg_gc);
         XFreeGC(display, normal_bg_gc);
         XFreeGC(display, locked_bg_gc);
         XFreeGC(display, label_bg_gc);
         XFreeGC(display, button_bg_gc);
         XFreeGC(display, tr_bar_gc);
         XFreeGC(display, color_gc);
         XFreeGC(display, black_line_gc);
         XFreeGC(display, white_line_gc);
         XFreeGC(display, led_gc);

         if (pid_list != NULL)
         {
#ifdef _NO_MMAP
            (void)munmap_emu((void *)pid_list);
#else
            (void)munmap((void *)pid_list, afd_active_size);
#endif
         }

         /* Free all the memory from the permission stuff. */
         if (acp.afd_ctrl_list != NULL)
         {
            FREE_RT_ARRAY(acp.afd_ctrl_list);
         }
         if (acp.ctrl_transfer_list != NULL)
         {
            FREE_RT_ARRAY(acp.ctrl_transfer_list);
         }
         if (acp.ctrl_queue_list != NULL)
         {
            FREE_RT_ARRAY(acp.ctrl_queue_list);
         }
         if (acp.switch_host_list != NULL)
         {
            FREE_RT_ARRAY(acp.switch_host_list);
         }
         if (acp.disable_list != NULL)
         {
            FREE_RT_ARRAY(acp.disable_list);
         }
         if (acp.info_list != NULL)
         {
            FREE_RT_ARRAY(acp.info_list);
         }
         if (acp.debug_list != NULL)
         {
            FREE_RT_ARRAY(acp.debug_list);
         }
         if (acp.retry_list != NULL)
         {
            FREE_RT_ARRAY(acp.retry_list);
         }
         if (acp.show_slog_list != NULL)
         {
            FREE_RT_ARRAY(acp.show_slog_list);
         }
         if (acp.show_rlog_list != NULL)
         {
            FREE_RT_ARRAY(acp.show_rlog_list);
         }
         if (acp.show_tlog_list != NULL)
         {
            FREE_RT_ARRAY(acp.show_tlog_list);
         }
         if (acp.show_dlog_list != NULL)
         {
            FREE_RT_ARRAY(acp.show_dlog_list);
         }
         if (acp.show_ilog_list != NULL)
         {
            FREE_RT_ARRAY(acp.show_ilog_list);
         }
         if (acp.show_olog_list != NULL)
         {
            FREE_RT_ARRAY(acp.show_olog_list);
         }
         if (acp.show_elog_list != NULL)
         {
            FREE_RT_ARRAY(acp.show_elog_list);
         }
         if (acp.show_queue_list != NULL)
         {
            FREE_RT_ARRAY(acp.show_queue_list);
         }
         if (acp.afd_load_list != NULL)
         {
            FREE_RT_ARRAY(acp.afd_load_list);
         }
         if (acp.view_jobs_list != NULL)
         {
            FREE_RT_ARRAY(acp.view_jobs_list);
         }
         if (acp.edit_hc_list != NULL)
         {
            FREE_RT_ARRAY(acp.edit_hc_list);
         }
         if (acp.view_dc_list != NULL)
         {
            FREE_RT_ARRAY(acp.view_dc_list);
         }
         free(connect_data);
         free(args);
         FREE_RT_ARRAY(hosts);
         exit(SUCCESS);

      default  :
         (void)xrec(appshell, WARN_DIALOG,
                    "Impossible item selection (%d).", sel_typ);
         free(args);
         FREE_RT_ARRAY(hosts)
         return;
    }
#ifdef _DEBUG
    (void)fprintf(stderr, "Selected %d hosts (", no_selected);
    for (i = j = 0; i < no_of_hosts; i++)
    {
       if (connect_data[i].inverse > OFF)
       {
	  if (j++ < (no_selected - 1))
	  {
             (void)fprintf(stderr, "%d, ", i);
          }
          else
          {
             j = i;
          }
       }
    }
    if (no_selected > 0)
    {
       (void)fprintf(stderr, "%d)\n", j);
    }
    else
    {
       (void)fprintf(stderr, "none)\n");
    }
#endif

   /* Set each host. */
   k = display_error = 0;
   for (i = 0; i < no_of_hosts; i++)
   {
      if (connect_data[i].inverse > OFF)
      {
         switch(sel_typ)
         {
            case QUEUE_SEL :
               if (ehc == NO)
               {
                  if ((fsa[i].host_status & PAUSE_QUEUE_STAT) ||
                      (fsa[i].host_status & AUTO_PAUSE_QUEUE_STAT))
                  {
                     if (fsa[i].host_status & AUTO_PAUSE_QUEUE_STAT)
                     {
                        (void)rec(sys_log_fd, CONFIG_SIGN,
                                  "%s: STARTED input queue that stopped automatically (%s).\n",
                                  connect_data[i].host_display_str, user);
                        if (fsa[i].error_counter > 0)
                        {
                           fsa[i].error_counter = 0;
                        }
                        fsa[i].host_status ^= AUTO_PAUSE_QUEUE_STAT;
                     }
                     else
                     {
                        (void)rec(sys_log_fd, CONFIG_SIGN,
                                  "%s: STARTED input queue (%s).\n",
                                  connect_data[i].host_display_str, user);
                        fsa[i].host_status ^= PAUSE_QUEUE_STAT;
                        hl[i].host_status &= ~PAUSE_QUEUE_STAT;
                     }
                  }
                  else
                  {
                     (void)rec(sys_log_fd, CONFIG_SIGN,
                               "%s: STOPPED input queue (%s).\n",
                               connect_data[i].host_display_str, user);
                     fsa[i].host_status ^= PAUSE_QUEUE_STAT;
                     hl[i].host_status |= PAUSE_QUEUE_STAT;
                  }
               }
               break;
                             
            case TRANS_SEL :
               if (ehc == NO)
               {
                  if (fsa[i].host_status & STOP_TRANSFER_STAT)
                  {
                     int  fd;
                     char wake_up_fifo[MAX_PATH_LENGTH];

                     (void)sprintf(wake_up_fifo, "%s%s%s",
                                   p_work_dir, FIFO_DIR, FD_WAKE_UP_FIFO);
                     if ((fd = open(wake_up_fifo, O_RDWR)) == -1)
                     {
                        (void)xrec(appshell, ERROR_DIALOG,
                                   "Failed to open() %s : %s (%s %d)",
                                   FD_WAKE_UP_FIFO, strerror(errno),
                                   __FILE__, __LINE__);
                     }
                     else
                     {
                        char dummy;

                        if (write(fd, &dummy, 1) != 1)
                        {
                           (void)xrec(appshell, ERROR_DIALOG,
                                      "Failed to write() to %s : %s (%s %d)",
                                      FD_WAKE_UP_FIFO, strerror(errno),
                                                    __FILE__, __LINE__);
                        }
                        if (close(fd) == -1)
                        {
                           (void)rec(sys_log_fd, DEBUG_SIGN,
                                  "Failed to close() FIFO %s : %s (%s %d)\n",
                                  FD_WAKE_UP_FIFO, strerror(errno),
                                  __FILE__, __LINE__);
                        }
                     }
                     (void)rec(sys_log_fd, CONFIG_SIGN,
                               "%s: STARTED transfer (%s).\n",
                               connect_data[i].host_display_str, user);
                     hl[i].host_status &= ~STOP_TRANSFER_STAT;
                     fsa[i].host_status ^= STOP_TRANSFER_STAT;
                  }
                  else
                  {
                     fsa[i].host_status ^= STOP_TRANSFER_STAT;
                     if (fsa[i].active_transfers > 0)
                     {
                        int m;

                        for (m = 0; m < fsa[i].allowed_transfers; m++)
                        {
                           if (fsa[i].job_status[m].proc_id > 0)
                           {
                              if ((kill(fsa[i].job_status[m].proc_id, SIGINT) == -1) &&
                                  (errno != ESRCH))
                              {
                                 (void)rec(sys_log_fd, DEBUG_SIGN,
                                           "Failed to kill process %d : %s (%s %d)\n",
                                           fsa[i].job_status[m].proc_id,
                                           strerror(errno), __FILE__, __LINE__);
                              }
                           }
                        }
                     }
                     (void)rec(sys_log_fd, CONFIG_SIGN,
                               "%s: STOPPED transfer (%s).\n",
                               connect_data[i].host_display_str,
                               user);
                     hl[i].host_status |= STOP_TRANSFER_STAT;
                  }
               }
               break;

            case DISABLE_SEL:
               if (ehc == NO)
               {
                  if (fsa[i].special_flag & HOST_DISABLED)
                  {
                     fsa[i].special_flag ^= HOST_DISABLED;
                     hl[i].host_status &= ~HOST_CONFIG_HOST_DISABLED;
                     (void)rec(sys_log_fd, CONFIG_SIGN,
                               "%s: ENABLED (%s).\n",
                               connect_data[i].host_display_str, user);
                  }
                  else
                  {
                     if (xrec(appshell, QUESTION_DIALOG,
                              "Are you shure that you want to disable %s?\nAll jobs for this host will be lost.",
                              fsa[i].host_dsp_name) == YES)
                     {
                        int    fd;
                        size_t length;
                        char   delete_jobs_host_fifo[MAX_PATH_LENGTH];

                        length = strlen(fsa[i].host_alias) + 1;
                        fsa[i].special_flag ^= HOST_DISABLED;
                        hl[i].host_status |= HOST_CONFIG_HOST_DISABLED;
                        (void)rec(sys_log_fd, CONFIG_SIGN,
                                  "%s: DISABLED (%s).\n",
                                  connect_data[i].host_display_str, user);

                        (void)sprintf(delete_jobs_host_fifo, "%s%s%s",
                                      p_work_dir, FIFO_DIR, DELETE_JOBS_HOST_FIFO);
                        if ((fd = open(delete_jobs_host_fifo, O_RDWR)) == -1)
                        {
                           (void)xrec(appshell, ERROR_DIALOG,
                                      "Failed to open() %s : %s (%s %d)",
                                      DELETE_JOBS_HOST_FIFO, strerror(errno),
                                      __FILE__, __LINE__);
                        }
                        else
                        {
                           if (write(fd, fsa[i].host_alias, length) != length)
                           {
                              (void)xrec(appshell, ERROR_DIALOG,
                                         "Failed to write() to %s : %s (%s %d)",
                                         DELETE_JOBS_HOST_FIFO, strerror(errno),
                                         __FILE__, __LINE__);
                           }
                           if (close(fd) == -1)
                           {
                              (void)rec(sys_log_fd, DEBUG_SIGN,
                                        "Failed to close() FIFO %s : %s (%s %d)\n",
                                        DELETE_JOBS_HOST_FIFO, strerror(errno),
                                        __FILE__, __LINE__);
                           }
                        }
                        (void)sprintf(delete_jobs_host_fifo, "%s%s%s",
                                      p_work_dir, FIFO_DIR, DEL_TIME_JOB_FIFO);
                        if ((fd = open(delete_jobs_host_fifo, O_RDWR)) == -1)
                        {
                           (void)xrec(appshell, ERROR_DIALOG,
                                      "Failed to open() %s : %s (%s %d)",
                                      DEL_TIME_JOB_FIFO, strerror(errno),
                                      __FILE__, __LINE__);
                        }
                        else
                        {
                           if (write(fd, fsa[i].host_alias, length) != length)
                           {
                              (void)xrec(appshell, ERROR_DIALOG,
                                         "Failed to write() to %s : %s (%s %d)",
                                         DEL_TIME_JOB_FIFO, strerror(errno),
                                         __FILE__, __LINE__);
                           }
                           if (close(fd) == -1)
                           {
                              (void)rec(sys_log_fd, DEBUG_SIGN,
                                        "Failed to close() FIFO %s : %s (%s %d)\n",
                                        DEL_TIME_JOB_FIFO, strerror(errno),
                                        __FILE__, __LINE__);
                           }
                        }
                     }
                  }
               }
               break;

            case SWITCH_SEL :
               if ((fsa[i].toggle_pos > 0) &&
                   (fsa[i].host_toggle_str[0] != '\0'))
               {
                  (void)rec(sys_log_fd, CONFIG_SIGN,
                            "Host Switch initiated for host %s (%s)\n",
                            fsa[i].host_dsp_name, user);
                  if (fsa[i].host_toggle == HOST_ONE)
                  {
                     connect_data[i].host_toggle = fsa[i].host_toggle = HOST_TWO;
                  }
                  else
                  {
                     connect_data[i].host_toggle = fsa[i].host_toggle = HOST_ONE;
                  }
                  fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
                  connect_data[i].host_display_str[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];

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
               else
               {
                  (void)xrec(appshell, ERROR_DIALOG,
                             "Host %s cannot be switched!",
                             fsa[i].host_dsp_name);
               }

               if (connect_data[i].inverse == ON)
               {
                  connect_data[i].inverse = OFF;
               }
               draw_line_status(i, 1);
               break;

            case RETRY_SEL :
               /* It is not very helpful if we just check     */
               /* whether the error_counter is larger than    */
               /* zero, since we might have restarted the AFD */
               /* and then the error_counter is zero. Also do */
               /* NOT check if the total_file_counter is      */
               /* larger than zero, there might be a retrieve */
               /* job in the queue.                           */
               if ((fsa[i].special_flag & HOST_DISABLED) == 0)
               {
                  int  fd;
                  char retry_fifo[MAX_PATH_LENGTH];

                  (void)sprintf(retry_fifo, "%s%s%s",
                                p_work_dir, FIFO_DIR, RETRY_FD_FIFO);
                  if ((fd = open(retry_fifo, O_RDWR)) == -1)
                  {
                     (void)xrec(appshell, ERROR_DIALOG,
                                "Failed to open() %s : %s (%s %d)",
                                RETRY_FD_FIFO, strerror(errno),
                                __FILE__, __LINE__);
                  }
                  else
                  {
                     if (write(fd, &i, sizeof(int)) != sizeof(int))
                     {
                        (void)xrec(appshell, ERROR_DIALOG,
                                   "Failed to write() to %s : %s (%s %d)",
                                   RETRY_FD_FIFO, strerror(errno),
                                   __FILE__, __LINE__);
                     }
                     if (close(fd) == -1)
                     {
                        (void)rec(sys_log_fd, DEBUG_SIGN,
                                  "Failed to close() FIFO %s : %s (%s %d)\n",
                                  RETRY_FD_FIFO, strerror(errno),
                                  __FILE__, __LINE__);
                     }
                  }
               }
               break;

            case DEBUG_SEL :
               if (fsa[i].debug == NO)
               {
                  (void)rec(sys_log_fd, CONFIG_SIGN,
                            "%s: ENABLED debug mode by user %s.\n",
                            connect_data[i].host_display_str,
                            user);
                  fsa[i].debug = YES;
               }
               else
               {
                  (void)rec(sys_log_fd, CONFIG_SIGN,
                            "%s: DISABLED debug mode by user %s.\n",
                            connect_data[i].host_display_str,
                            user);
                  fsa[i].debug = NO;
               }
               break;

            case LONG_SHORT_SEL :
               if (connect_data[i].short_pos == -1)
               {
                  if (to_short(i, -1, NO) == SUCCESS)
                  {
                     to_short_counter++;
                  }
               }
               else
               {
                  if (to_long(i, -1, NO) == SUCCESS)
                  {
                     to_long_counter++;
                  }
               }
               break;

            case I_LOG_SEL :
            case O_LOG_SEL :
            case E_LOG_SEL :
            case SHOW_QUEUE_SEL :
               (void)strcpy(hosts[k], fsa[i].host_alias);
               args[k + 5] = hosts[k];
               k++;
               break;

            case D_LOG_SEL :
            case T_LOG_SEL : /* Start Debug/Transfer Log */
               (void)strcpy(hosts[k], fsa[i].host_alias);
               if (fsa[i].host_toggle_str[0] != '\0')
               {
                  (void)strcat(hosts[k], "?");
               }
               args[k + 7] = hosts[k];
               k++;
               break;

            case VIEW_DC_SEL : /* View DIR_CONFIG entries */
               {
                  int gotcha = NO,
                      ii;

                  for (ii = 0; ii < no_of_active_process; ii++)
                  {
                     if ((apps_list[ii].position == i) &&
                         (CHECK_STRCMP(apps_list[ii].progname, VIEW_DC) == 0))
                     {
                        gotcha = YES;
                        break;
                     }
                  }
                  if (gotcha == NO)
                  {
                     args[6] = fsa[i].host_alias;
                     make_xprocess(progname, progname, args, i);
                  }
                  else
                  {
                     (void)xrec(appshell, INFO_DIALOG,
                                "DIR_CONFIG dialog for %s is already open on your display.",
                                fsa[i].host_alias);
                  }
               }
               break;

            case PING_SEL  :   /* Show ping test */
               {
                  (void)sprintf(ptr_ping_cmd, "%s %s\"",
                                fsa[i].real_hostname[fsa[i].host_toggle - 1],
                                fsa[i].host_dsp_name);
                  make_xprocess(progname, progname, args, i);
               }
               break;

            case TRACEROUTE_SEL  :   /* Show traceroute test */
               {
                  (void)sprintf(ptr_traceroute_cmd, "%s %s\"",
                                fsa[i].real_hostname[fsa[i].host_toggle - 1],
                                fsa[i].host_dsp_name);
                  make_xprocess(progname, progname, args, i);
               }
               break;

            case INFO_SEL  :   /* Show information for this host */
               {
                  int gotcha = NO,
                      ii;

                  for (ii = 0; ii < no_of_active_process; ii++)
                  {
                     if ((apps_list[ii].position == i) &&
                         (CHECK_STRCMP(apps_list[ii].progname, AFD_INFO) == 0))
                     {
                        gotcha = YES;
                        break;
                     }
                  }
                  if (gotcha == NO)
                  {
                     args[6] = fsa[i].host_alias;
                     make_xprocess(progname, progname, args, i);
                  }
                  else
                  {
                     (void)xrec(appshell, INFO_DIALOG,
                                "Information dialog for %s is already open on your display.",
                                fsa[i].host_alias);
                  }
               }
               break;

            default        :
               (void)xrec(appshell, WARN_DIALOG,
                          "Impossible selection! NOOO this can't be true! (%s %d)",
                          __FILE__, __LINE__);
               free(args);
               FREE_RT_ARRAY(hosts)
               return;
         }
      }
   } /* for (i = 0; i < no_of_hosts; i++) */

   if (sel_typ == T_LOG_SEL)
   {
      (void)strcpy(log_typ, TRANSFER_STR);
      args[k + 7] = NULL;
      make_xprocess(progname, progname, args, -1);
   }
   else if (sel_typ == D_LOG_SEL)
        {
           (void)strcpy(log_typ, TRANS_DB_STR);
           args[k + 7] = NULL;
           make_xprocess(progname, progname, args, -1);
        }
   else if ((sel_typ == O_LOG_SEL) || (sel_typ == E_LOG_SEL) ||
            (sel_typ == I_LOG_SEL) || (sel_typ == SHOW_QUEUE_SEL))
        {
           args[k + 5] = NULL;
           make_xprocess(progname, progname, args, -1);
        }
   else if (((sel_typ == QUEUE_SEL) || (sel_typ == TRANS_SEL) ||
             (sel_typ == DISABLE_SEL)) && (ehc == NO))
        {
           (void)write_host_config(no_of_hosts, host_config_file, hl);
           if (hl != NULL)
           {
              free(hl);
           }
        }
   else if (sel_typ == LONG_SHORT_SEL)
        {
           if ((to_long_counter != 0) && (to_short_counter != 0))
           {
              (void)resize_window();
              XClearWindow(display, line_window);
              XClearWindow(display, short_line_window);
              draw_label_line();
              for (i = 0; i < no_of_hosts; i++)
              {
                 draw_line_status(i, 1);
              }
              draw_button_line();
              XFlush(display);
           }
           else
           {
              if (to_long_counter != 0)
              {
                 redraw_long(-1);
              }
              else if (to_short_counter != 0)
                   {
                      redraw_short();
                   }
           }
        }

   /* Memory for arg list stuff no longer needed. */
   free(args);
   FREE_RT_ARRAY(hosts)

   if (display_error > 0)
   {
      if (display_error > 1)
      {
         (void)sprintf(err_msg, "Operation for hosts %s not done.",
                       host_err_no);
      }
      else
      {
         (void)sprintf(err_msg, "Operation for host %s not done.",
                       host_err_no);
      }
   }

   for (i = 0; i < no_of_hosts; i++)
   {
      if (connect_data[i].inverse == ON)
      {
         connect_data[i].inverse = OFF;
         draw_line_status(i, -1);
      }
   }

   /* Make sure that all changes are shown */
   XFlush(display);

   no_selected = 0;

   return;
}


/*############################ control_cb() #############################*/
void
control_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
   int  item_no = (int)client_data;

   switch(item_no)
   {
      case CONTROL_AMG_SEL : /* Start/Stop AMG */
         if (p_afd_status->amg == ON)
         {
            if (xrec(appshell, QUESTION_DIALOG,
                     "Are you shure that you want to stop %s?", AMG) == YES)
            {
               int  afd_cmd_fd;
               char afd_cmd_fifo[MAX_PATH_LENGTH];

               (void)sprintf(afd_cmd_fifo, "%s%s%s",
                             p_work_dir, FIFO_DIR, AFD_CMD_FIFO);
               if ((afd_cmd_fd = open(afd_cmd_fifo, O_RDWR)) < 0)
               {
                  (void)xrec(appshell, ERROR_DIALOG,
                             "Could not open fifo %s : %s (%s %d)",
                             afd_cmd_fifo, strerror(errno),
                             __FILE__, __LINE__);
                  return;
               }
               (void)rec(sys_log_fd, CONFIG_SIGN,
                         "Sending STOP to %s by %s\n", AMG, user);
               if (send_cmd(STOP_AMG, afd_cmd_fd) < 0)
               {
                  (void)xrec(appshell, ERROR_DIALOG,
                             "Was not able to stop %s. (%s %d)",
                             AMG, __FILE__, __LINE__);
               }
               if (close(afd_cmd_fd) == -1)
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "close() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
               }
            }
         }
         else
         {
            int  afd_cmd_fd;
            char afd_cmd_fifo[MAX_PATH_LENGTH];

            (void)sprintf(afd_cmd_fifo, "%s%s%s",
                          p_work_dir, FIFO_DIR, AFD_CMD_FIFO);
            if ((afd_cmd_fd = open(afd_cmd_fifo, O_RDWR)) < 0)
            {
               (void)xrec(appshell, ERROR_DIALOG,
                          "Could not open fifo %s : %s (%s %d)",
                          afd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
               return;
            }

            (void)rec(sys_log_fd, CONFIG_SIGN, "Sending START to %s by %s\n",
                      AMG, user);
            if (send_cmd(START_AMG, afd_cmd_fd) < 0)
            {
               (void)xrec(appshell, ERROR_DIALOG,
                          "Was not able to start %s. (%s %d)",
                          AMG, __FILE__, __LINE__);
            }
            if (close(afd_cmd_fd) == -1)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "close() error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
            }
         }
         break;

      case CONTROL_FD_SEL : /* Start/Stop FD */
         if (p_afd_status->fd == ON)
         {
            if (xrec(appshell, QUESTION_DIALOG,
                     "Are you shure that you want to stop %s?\nNOTE: No more files will be distributed!!!", FD) == YES)
            {
               int  afd_cmd_fd;
               char afd_cmd_fifo[MAX_PATH_LENGTH];

               (void)sprintf(afd_cmd_fifo, "%s%s%s",
                             p_work_dir, FIFO_DIR, AFD_CMD_FIFO);
               if ((afd_cmd_fd = open(afd_cmd_fifo, O_RDWR)) < 0)
               {
                  (void)xrec(appshell, ERROR_DIALOG,
                             "Could not open fifo %s : %s (%s %d)",
                             afd_cmd_fifo, strerror(errno),
                             __FILE__, __LINE__);
                  return;
               }
               (void)rec(sys_log_fd, CONFIG_SIGN, 
                         "Sending STOP to %s by %s\n", FD, user);
               if (send_cmd(STOP_FD, afd_cmd_fd) < 0)
               {
                  (void)xrec(appshell, ERROR_DIALOG,
                             "Was not able to stop %s. (%s %d)",
                             FD, __FILE__, __LINE__);
               }
               if (close(afd_cmd_fd) == -1)
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "close() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
               }
            }
         }
         else /* FD is NOT running. */
         {
            int  afd_cmd_fd;
            char afd_cmd_fifo[MAX_PATH_LENGTH];

            (void)sprintf(afd_cmd_fifo, "%s%s%s",
                          p_work_dir, FIFO_DIR, AFD_CMD_FIFO);
            if ((afd_cmd_fd = open(afd_cmd_fifo, O_RDWR)) < 0)
            {
               (void)xrec(appshell, ERROR_DIALOG,
                          "Could not open fifo %s : %s (%s %d)",
                          afd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
               return;
            }

            (void)rec(sys_log_fd, CONFIG_SIGN,
                      "Sending START to %s by %s\n", FD, user);
            if (send_cmd(START_FD, afd_cmd_fd) < 0)
            {
               (void)xrec(appshell, ERROR_DIALOG,
                           "Was not able to start %s. (%s %d)",
                           FD, __FILE__, __LINE__);
            }
            if (close(afd_cmd_fd) == -1)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "close() error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
            }
         }
         break;

      case REREAD_DIR_CONFIG_SEL :
      case REREAD_HOST_CONFIG_SEL : /* Reread DIR_CONFIG or HOST_CONFIG */
         {
            int  db_update_fd;
            char db_update_fifo[MAX_PATH_LENGTH];

            (void)sprintf(db_update_fifo, "%s%s%s",
                          p_work_dir, FIFO_DIR, DB_UPDATE_FIFO);
            if ((db_update_fd = open(db_update_fifo, O_RDWR)) < 0)
            {
               (void)xrec(appshell, ERROR_DIALOG,
                          "Could not open fifo %s : %s (%s %d)",
                          db_update_fifo, strerror(errno), __FILE__, __LINE__);
               return;
            }

            if (item_no == REREAD_DIR_CONFIG_SEL)
            {
               (void)rec(sys_log_fd, CONFIG_SIGN, 
                         "Rereading DIR_CONFIG initiated by %s\n", user);
               if (send_cmd(REREAD_DIR_CONFIG, db_update_fd) < 0)
               {
                  (void)xrec(appshell, ERROR_DIALOG,
                             "Was not able to send reread command to %s. (%s %d)",
                             AMG, __FILE__, __LINE__);
               }
            }
            else
            {
               (void)rec(sys_log_fd, CONFIG_SIGN, 
                         "Rereading HOST_CONFIG initiated by %s\n", user);
               if (send_cmd(REREAD_HOST_CONFIG, db_update_fd) < 0)
               {
                  (void)xrec(appshell, ERROR_DIALOG,
                             "Was not able to send reread command to %s. (%s %d)",
                             AMG, __FILE__, __LINE__);
               }
            }
            if (close(db_update_fd) == -1)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "close() error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
            }
         }
         break;

      case STARTUP_AFD_SEL : /* Startup AFD */
         {
            pid_t pid;
            char  *args[5],
                  progname[4],
                  parameter[3];

            args[0] = progname;
            args[1] = WORK_DIR_ID;
            args[2] = p_work_dir;
            args[3] = parameter;
            args[4] = NULL;
            (void)strcpy(progname, "afd");
            (void)strcpy(parameter, "-a");
            switch (pid = fork())
            {
               case -1:
                  (void)xrec(appshell, ERROR_DIALOG,
                             "Failed to fork() : %s (%s %d)",
                             strerror(errno), __FILE__, __LINE__);
                  break;

               case 0 : /* Child process */
                  (void)execvp(progname, args); /* NOTE: NO return from execvp() */
                  _exit(INCORRECT);

               default: /* Parent process */
                  if (waitpid(pid, NULL, 0) != pid)
                  {
                     (void)xrec(appshell, ERROR_DIALOG,
                                "Failed to waitpid() : %s (%s %d)",
                                strerror(errno), __FILE__, __LINE__);
                  }
                  break;
            }
	 }
	 return;

      case SHUTDOWN_AFD_SEL : /* Shutdown AFD */

         if (xrec(appshell, QUESTION_DIALOG,
                  "Are you shure that you want to do a shutdown?") == YES)
         {
            char *args[5],
                 progname[4],
                 parameter[3];

            args[0] = progname;
            args[1] = WORK_DIR_ID;
            args[2] = p_work_dir;
            args[3] = parameter;
            args[4] = NULL;
            (void)strcpy(progname, "afd");
            (void)strcpy(parameter, "-S");
	    make_xprocess(progname, progname, args, -1);
	 }
	 return;

      default  :
         (void)xrec(appshell, INFO_DIALOG,
                    "This function [%d] has not yet been implemented.",
                    item_no);
         break;
   }

   return;
}


/*########################## change_font_cb() ###########################*/
void
change_font_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
   int       item_no = (int)client_data,
             i,
             redraw = NO;
   XGCValues gc_values;

   if (current_font != item_no)
   {
      XtVaSetValues(fw[current_font], XmNset, False, NULL);
      current_font = item_no;
   }

   switch(item_no)
   {
      case 0   :
         (void)strcpy(font_name, FONT_0);
         break;

      case 1   :
         (void)strcpy(font_name, FONT_1);
         break;

      case 2   :
         (void)strcpy(font_name, FONT_2);
         break;

      case 3   :
         (void)strcpy(font_name, FONT_3);
         break;

      case 4   :
         (void)strcpy(font_name, FONT_4);
         break;

      case 5   :
         (void)strcpy(font_name, FONT_5);
         break;

      case 6   :
         (void)strcpy(font_name, FONT_6);
         break;

      case 7   :
         (void)strcpy(font_name, FONT_7);
         break;

      case 8   :
         (void)strcpy(font_name, FONT_8);
         break;

      case 9   :
         (void)strcpy(font_name, FONT_9);
         break;

      case 10  :
         (void)strcpy(font_name, FONT_10);
         break;

      case 11  :
         (void)strcpy(font_name, FONT_11);
         break;

      case 12  :
         (void)strcpy(font_name, FONT_12);
         break;

      default  :
         (void)xrec(appshell, WARN_DIALOG, "Impossible font selection (%d).",
                    item_no);
         return;
    }

#ifdef _DEBUG
   (void)fprintf(stderr, "You have chosen: %s\n", font_name);
#endif

   /* remove old font */
   XFreeFont(display, font_struct);

   /* calculate the new values for global variables */
   setup_window(font_name, YES);

   /* Load the font into the old GC */
   gc_values.font = font_struct->fid;
   XChangeGC(display, letter_gc, GCFont, &gc_values);
   XChangeGC(display, normal_letter_gc, GCFont, &gc_values);
   XChangeGC(display, locked_letter_gc, GCFont, &gc_values);
   XChangeGC(display, color_letter_gc, GCFont, &gc_values);
   XFlush(display);

   /* Redraw detailed transfer view window. */
   if (no_of_jobs_selected > 0)
   {
      setup_tv_window();

      if (resize_tv_window() == YES)
      {
         XClearWindow(display, detailed_window);
         draw_tv_label_line();
         for (i = 0; i < no_of_jobs_selected; i++)
         {
            draw_detailed_line(i);
         }
         redraw = YES;
      }
   }

   /* resize and redraw window if necessary */
   if (resize_window() == YES)
   {
      if (no_of_long_lines > 0)
      {
         XClearWindow(display, line_window);
      }
      if (no_of_short_lines > 0)
      {
         XClearWindow(display, short_line_window);
      }
      XFlush(display);

      /* redraw label */
      draw_label_line();

      /* redraw all status lines */
      for (i = 0; i < no_of_hosts; i++)
      {
         draw_line_status(i, 1);
      }
      XFlush(display);

      /* redraw button line */
      draw_button_line();

      redraw = YES;
   }

   if (redraw == YES)
   {
      XFlush(display);
   }

   return;
}


/*########################## change_rows_cb() ###########################*/
void
change_rows_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
   int item_no = (int)client_data,
       i,
       redraw = NO;

   if (current_row != item_no)
   {
      XtVaSetValues(rw[current_row], XmNset, False, NULL);
      current_row = item_no;
   }

   switch(item_no)
   {
      case 0   :
         no_of_rows_set = atoi(ROW_0);
         break;

      case 1   :
         no_of_rows_set = atoi(ROW_1);
         break;

      case 2   :
         no_of_rows_set = atoi(ROW_2);
         break;

      case 3   :
         no_of_rows_set = atoi(ROW_3);
         break;

      case 4   :
         no_of_rows_set = atoi(ROW_4);
         break;

      case 5   :
         no_of_rows_set = atoi(ROW_5);
         break;

      case 6   :
         no_of_rows_set = atoi(ROW_6);
         break;

      case 7   :
         no_of_rows_set = atoi(ROW_7);
         break;

      case 8   :
         no_of_rows_set = atoi(ROW_8);
         break;

      case 9   :
         no_of_rows_set = atoi(ROW_9);
         break;

      case 10  :
         no_of_rows_set = atoi(ROW_10);
         break;

      case 11  :
         no_of_rows_set = atoi(ROW_11);
         break;

      case 12  :
         no_of_rows_set = atoi(ROW_12);
         break;

      case 13  :
         no_of_rows_set = atoi(ROW_13);
         break;

      default  :
         (void)xrec(appshell, WARN_DIALOG, "Impossible row selection (%d).",
                    item_no);
         return;
   }

   if (no_of_rows_set == 0)
   {
      no_of_rows_set = 2;
   }

#ifdef _DEBUG
   (void)fprintf(stderr, "%s: You have chosen: %d rows/column\n",
                 __FILE__, no_of_rows_set);
#endif

   /* Redraw detailed transfer view window. */
   if ((no_of_jobs_selected > 0) && (resize_tv_window() == YES))
   {
      XClearWindow(display, detailed_window);

      draw_tv_label_line();
      for (i = 0; i < no_of_jobs_selected; i++)
      {
         draw_detailed_line(i);
      }

      redraw = YES;
   }

   if (resize_window() == YES)
   {
      if (no_of_long_lines > 0)
      {
         XClearWindow(display, line_window);
      }
      if (no_of_short_lines > 0)
      {
         XClearWindow(display, short_line_window);
      }

      /* redraw label */
      draw_label_line();

      /* redraw all status lines */
      for (i = 0; i < no_of_hosts; i++)
      {
         draw_line_status(i, 1);
      }

      /* redraw button line */
      draw_button_line();

      redraw = YES;
   }

   if (redraw == YES)
   {
      XFlush(display);
   }

   return;
}


/*########################## change_style_cb() ##########################*/
void
change_style_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
   int item_no = (int)client_data,
       i,
       redraw = NO;

   switch(item_no)
   {
      case LEDS_STYLE_W :
         if (line_style & SHOW_LEDS)
         {
            line_style &= ~SHOW_LEDS;
            XtVaSetValues(lsw[LEDS_STYLE_W], XmNset, False, NULL);
         }
         else
         {
            line_style |= SHOW_LEDS;
            XtVaSetValues(lsw[LEDS_STYLE_W], XmNset, True, NULL);
         }
         break;

      case JOBS_STYLE_W :
         if (line_style & SHOW_JOBS)
         {
            line_style &= ~SHOW_JOBS;
            XtVaSetValues(lsw[JOBS_STYLE_W], XmNset, False, NULL);
         }
         else
         {
            line_style |= SHOW_JOBS;
            XtVaSetValues(lsw[JOBS_STYLE_W], XmNset, True, NULL);
         }
         break;

      case CHARACTERS_STYLE_W :
         if (line_style & SHOW_CHARACTERS)
         {
            line_style &= ~SHOW_CHARACTERS;
            XtVaSetValues(lsw[CHARACTERS_STYLE_W], XmNset, False, NULL);
         }
         else
         {
            line_style |= SHOW_CHARACTERS;
            XtVaSetValues(lsw[CHARACTERS_STYLE_W], XmNset, True, NULL);
         }
         break;

      case BARS_STYLE_W :
         if (line_style & SHOW_BARS)
         {
            line_style &= ~SHOW_BARS;
            XtVaSetValues(lsw[BARS_STYLE_W], XmNset, False, NULL);
         }
         else
         {
            line_style |= SHOW_BARS;
            XtVaSetValues(lsw[BARS_STYLE_W], XmNset, True, NULL);
         }
         break;

      default  :
         (void)xrec(appshell, WARN_DIALOG, "Impossible row selection (%d).",
                    item_no);
         return;
   }

#ifdef _DEBUG
   switch(item_no)
   {
      case LEDS_STYLE_W :
         if (line_style & SHOW_LEDS)
         {
            (void)fprintf(stderr, "Adding LED's.\n");
         }
         else
         {
            (void)fprintf(stderr, "Removing LED's.\n");
         }
         break;

      case JOBS_STYLE_W :
         if (line_style & SHOW_JOBS)
         {
            (void)fprintf(stderr, "Adding Job's.\n");
         }
         else
         {
            (void)fprintf(stderr, "Removing Job's.\n");
         }
         break;

      case CHARACTERS_STYLE_W :
         if (line_style & SHOW_CHARACTERS)
         {
            (void)fprintf(stderr, "Adding Character's.\n");
         }
         else
         {
            (void)fprintf(stderr, "Removing Character's.\n");
         }
         break;

      case BARS_STYLE_W :
         if (line_style & SHOW_BARS)
         {
            (void)fprintf(stderr, "Adding Bar's.\n");
         }
         else
         {
            (void)fprintf(stderr, "Removing Bar's.\n");
         }
         break;

      default : /* IMPOSSIBLE !!! */
         break;
   }
#endif

   setup_window(font_name, NO);

   /* Redraw detailed transfer view window. */
   if (no_of_jobs_selected > 0)
   {
      setup_tv_window();
      if (resize_tv_window() == YES)
      {
         XClearWindow(display, detailed_window);
         draw_tv_label_line();
         for (i = 0; i < no_of_jobs_selected; i++)
         {
            draw_detailed_line(i);
         }

         redraw = YES;
      }
   }

   if (resize_window() == YES)
   {
      calc_but_coord(window_width);
      if (no_of_long_lines > 0)
      {
         XClearWindow(display, line_window);
      }
      if (no_of_short_lines > 0)
      {
         XClearWindow(display, short_line_window);
      }

      /* redraw label */
      draw_label_line();

      /* redraw all status lines */
      for (i = 0; i < no_of_hosts; i++)
      {
         draw_line_status(i, 1);
      }

      /* redraw buttons */
      draw_button_line();

      redraw = YES;
   }

   if (redraw == YES)
   {
      XFlush(display);
   }

   return;
}


/*+++++++++++++++++++++++++++++ to_long() +++++++++++++++++++++++++++++++*/
static int
to_long(int pos, int select_no, int apply)
{
   if ((pos > -1) ||
       ((pos = get_short_pos(select_no, YES)) != -1))
   {
      int i,
          long_pos = -1;

      connect_data[pos].short_pos = -1;
      for (i = 0; i < pos; i++)
      {
         if (connect_data[i].long_pos > long_pos)
         {
            long_pos = connect_data[i].long_pos;
         }
      }
      connect_data[pos].long_pos = long_pos + 1;
      for (i = (pos + 1); i < no_of_hosts; i++)
      {
         if (connect_data[i].short_pos > -1)
         {
            connect_data[i].short_pos--;
         }
         if (connect_data[i].long_pos > -1)
         {
            connect_data[i].long_pos++;
         }
      }
      no_of_short_lines--;
      no_of_long_lines++;

      if (apply == YES)
      {
         redraw_long(pos);
      }
      return(SUCCESS);
   }
   else
   {
      return(INCORRECT);
   }
}


/*+++++++++++++++++++++++++++++ to_short() ++++++++++++++++++++++++++++++*/
static int
to_short(int pos, int select_no, int apply)
{
   if ((pos > -1) ||
       ((pos = get_long_pos(select_no, YES)) != -1))
   {
      int i,
          short_pos = -1;

      for (i = 0; i < pos; i++)
      {
         if (connect_data[i].short_pos > short_pos)
         {
            short_pos = connect_data[i].short_pos;
         }
      }
      connect_data[pos].short_pos = short_pos + 1;
      for (i = (pos + 1); i < no_of_hosts; i++)
      {
         if (connect_data[i].short_pos > -1)
         {
            connect_data[i].short_pos++;
         }
         if (connect_data[i].long_pos > -1)
         {
            connect_data[i].long_pos--;
         }
      }
      connect_data[pos].long_pos = -1;
      no_of_short_lines++;
      no_of_long_lines--;
      if (apply == YES)
      {
         redraw_short();
      }
      return(SUCCESS);
   }
   else
   {
      return(INCORRECT);
   }
}


/*---------------------------- redraw_long() ----------------------------*/
static void
redraw_long(int pos)
{
   int i;

   if ((no_of_short_lines == 0) &&
       ((pos == -1) || (connect_data[pos].inverse > OFF)))
   {
      XClearWindow(display, short_line_window);
   }
   if (resize_window() == YES)
   {
      if (no_of_long_lines > 0)
      {
         XClearWindow(display, line_window);
      }
      if (no_of_short_lines > 0)
      {
         XClearWindow(display, short_line_window);
      }

      /* redraw label */
      draw_label_line();

      /* redraw all status lines */
      for (i = 0; i < no_of_hosts; i++)
      {
         draw_line_status(i, 1);
      }

      /* redraw buttons */
      draw_button_line();
   }
   else
   {
      if (no_of_short_lines == 0)
      {
         draw_label_line();
      }
      else
      {
         XClearWindow(display, short_line_window);
      }

      /* redraw all status lines */
      for (i = 0; i < no_of_hosts; i++)
      {
         draw_line_status(i, 1);
      }
   }
   XFlush(display);

   return;
}


/*--------------------------- redraw_short() ----------------------------*/
static void
redraw_short(void)
{
   int i;

   if (resize_window() == YES)
   {
      if (no_of_long_lines > 0)
      {
         XClearWindow(display, line_window);
      }
      if (no_of_short_lines > 0)
      {
         XClearWindow(display, short_line_window);
      }

      /* redraw label */
      draw_label_line();

      /* redraw all status lines */
      for (i = 0; i < no_of_hosts; i++)
      {
         draw_line_status(i, 1);
      }

      /* redraw buttons */
      draw_button_line();
   }
   else
   {
      if (no_of_long_lines == 0)
      {
         draw_label_line();
      }
      else
      {
         XClearWindow(display, line_window);
      }

      /* redraw all status lines */
      for (i = 0; i < no_of_hosts; i++)
      {
         draw_line_status(i, 1);
      }
   }
   XFlush(display);

   return;
}

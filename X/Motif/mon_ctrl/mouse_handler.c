/*
 *  mouse_handler.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mouse_handler - handles all mouse- AND key events of the mon_ctrl
 **                   dialog
 **
 ** SYNOPSIS
 **   void mon_focus(Widget w, XtPointer client_data, XEvent *event)
 **   void mon_input(Widget w, XtPointer client_data, XtPointer call_data)
 **   void popup_mon_menu_cb(Widget w, XtPointer client_data, XEvent *event)
 **   void save_mon_setup_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void mon_popup_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void mon_control_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_mon_font_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_mon_row_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_mon_style_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_mon_history_cb(Widget w, XtPointer client_data, XtPointer call_data)
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
 **   13.09.1998 H.Kiehl Created
 **   10.09.2000 H.Kiehl Addition of history logs.
 **
 */
DESCR__E_M3

#include <stdio.h>             /* fprintf(), stderr                      */
#include <string.h>            /* strcpy(), strlen()                     */
#include <stdlib.h>            /* atoi(), exit()                         */
#include <sys/types.h>
#include <sys/wait.h>          /* waitpid()                              */
#include <fcntl.h>
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
#include "mon_ctrl.h"
#include "permission.h"

/* External global variables */
extern Display                 *display;
extern Widget                  fw[],
                               rw[],
                               hlw[],
                               tw[],
                               lsw[];
extern Widget                  appshell;
extern Window                  line_window;
extern XFontStruct             *font_struct;
extern GC                      letter_gc,
                               normal_letter_gc,
                               locked_letter_gc,
                               color_letter_gc,
                               red_color_letter_gc,
                               red_error_letter_gc,
                               default_bg_gc,
                               normal_bg_gc,
                               locked_bg_gc,
                               label_bg_gc,
                               tr_bar_gc,
                               color_gc,
                               black_line_gc,
                               white_line_gc,
                               led_gc;
extern int                     no_of_active_process,
                               no_of_afds,
                               no_of_jobs_selected,
                               line_length,
                               line_height,
                               x_offset_proc,
                               mon_log_fd,
                               no_selected,
                               no_selected_static,
                               no_of_rows,
                               no_of_rows_set,
                               current_font,
                               current_row,
                               current_style,
                               current_his_log,
                               his_log_set,
                               sys_log_fd;
extern float                   max_bar_length;
extern unsigned long           color_pool[];
extern char                    *p_work_dir,
                               *ping_cmd,
                               *ptr_ping_cmd,
                               *traceroute_cmd,
                               *ptr_traceroute_cmd,
                               line_style,
                               font_name[],
                               user[],
                               username[];
extern struct mon_line         *connect_data;
extern struct mon_status_area  *msa;
extern struct mon_control_perm mcp;
extern struct apps_list        *apps_list;

/* Local global variables */
static int                     in_window = NO;


/*############################ mon_focus() ##############################*/
void
mon_focus(Widget      w,
          XtPointer   client_data,
          XEvent      *event)
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


/*############################ mon_input() ##############################*/
void
mon_input(Widget      w,
          XtPointer   client_data,
          XEvent      *event)
{
   int        select_no;
   static int last_motion_pos = -1;

   /* Handle any motion event */
   if ((event->xany.type == MotionNotify) && (in_window == YES))
   {
      select_no = (event->xbutton.y / line_height) +
                  ((event->xbutton.x / line_length) * no_of_rows);

      if ((select_no < no_of_afds) && (last_motion_pos != select_no))
      {
         if (event->xkey.state & ControlMask)
         {
            if (connect_data[select_no].inverse == STATIC)
            {
               connect_data[select_no].inverse = OFF;
               no_selected_static--;
            }
            else
            {
               connect_data[select_no].inverse = STATIC;
               no_selected_static++;
            }

            draw_line_status(select_no, select_no);
            XFlush(display);
         }
         else if (event->xkey.state & ShiftMask)
              {
                 if (connect_data[select_no].inverse == ON)
                 {
                    connect_data[select_no].inverse = OFF;
                    no_selected--;
                 }
                 else if (connect_data[select_no].inverse == STATIC)
                      {
                         connect_data[select_no].inverse = OFF;
                         no_selected_static--;
                      }
                      else
                      {
                         connect_data[select_no].inverse = ON;
                         no_selected++;
                      }

                 draw_line_status(select_no, 1);
                 XFlush(display);
              }
      }
      last_motion_pos = select_no;

      return;
   }

   /* Handle any button press event. */
   if (event->xbutton.button == 1)
   {
      select_no = (event->xbutton.y / line_height) +
                  ((event->xbutton.x / line_length) * no_of_rows);

      /* Make sure that this field does contain a channel */
      if (select_no < no_of_afds)
      {
         if (((event->xkey.state & Mod1Mask) ||
             (event->xkey.state & Mod4Mask)) &&
             (event->xany.type == ButtonPress))
         {
            int gotcha = NO,
                i;

            for (i = 0; i < no_of_active_process; i++)
            {
               if ((apps_list[i].position == select_no) &&
                   (strcmp(apps_list[i].progname, MON_INFO) == 0))
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
               args[3] = "-a";
               args[4] = msa[select_no].afd_alias;
               args[5] = NULL;
               (void)strcpy(progname, MON_INFO);

               make_xprocess(progname, progname, args, select_no);
            }
            else
            {
               (void)xrec(appshell, INFO_DIALOG,
                          "Information dialog for %s is already open on your display.",
                          msa[select_no].afd_alias);
            }
         }
         else if (event->xany.type == ButtonPress)
              {
                 if (event->xkey.state & ControlMask)
                 {
                    if (connect_data[select_no].inverse == STATIC)
                    {
                       connect_data[select_no].inverse = OFF;
                       no_selected_static--;
                    }
                    else
                    {
                       connect_data[select_no].inverse = STATIC;
                       no_selected_static++;
                    }

                    draw_line_status(select_no, 1);
                    XFlush(display);
                 }
                 else if (event->xkey.state & ShiftMask)
                      {
                         if (connect_data[select_no].inverse == ON)
                         {
                            connect_data[select_no].inverse = OFF;
                            no_selected--;
                         }
                         else if (connect_data[select_no].inverse == STATIC)
                              {
                                 connect_data[select_no].inverse = OFF;
                                 no_selected_static--;
                              }
                              else
                              {
                                 connect_data[select_no].inverse = ON;
                                 no_selected++;
                              }

                         draw_line_status(select_no, 1);
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

   return;
}


/*########################## popup_mon_menu_cb() ########################*/
void
popup_mon_menu_cb(Widget      w,
                  XtPointer   client_data,
                  XEvent      *event)
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


/*######################## save_mon_setup_cb() ##########################*/
void
save_mon_setup_cb(Widget    w,
                  XtPointer client_data,
                  XtPointer call_data)
{
   write_setup(-1, his_log_set);

   return;
}


/*########################### mon_popup_cb() ############################*/
void
mon_popup_cb(Widget    w,
             XtPointer client_data,
             XtPointer call_data)
{
   int         sel_typ = (int)client_data,
               i,
#ifdef _DEBUG
               j,
#endif
               k;
   char        host_err_no[1025],
               progname[MAX_PROCNAME_LENGTH + 1],
               **hosts = NULL,
               **args = NULL,
               log_typ[30],
               display_error,
       	       err_msg[1025 + 100];
   size_t      new_size = (no_of_afds + 8) * sizeof(char *);

   if ((no_selected == 0) && (no_selected_static == 0) &&
       ((sel_typ == MON_RETRY_SEL) || (sel_typ == MON_INFO_SEL) ||
        (sel_typ == PING_SEL) || (sel_typ == TRACEROUTE_SEL)))
   {
      (void)xrec(appshell, INFO_DIALOG,
                 "You must first select an AFD!\nUse mouse button 1 together with the SHIFT or CTRL key.");
      return;
   }
   RT_ARRAY(hosts, no_of_afds, (MAX_AFDNAME_LENGTH + 1), char);
   if ((args = malloc(new_size)) == NULL)
   {
      (void)xrec(appshell, FATAL_DIALOG, "malloc() error : %s [%d] (%s %d)",
                 strerror(errno), errno, __FILE__, __LINE__);
      return;
   }

   switch(sel_typ)
   {
      case MON_RETRY_SEL:
      case MON_DISABLE_SEL:
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

      case MON_INFO_SEL : /* Information */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = "-a";
         args[7] = NULL;
         (void)strcpy(progname, MON_INFO);
         break;

      case MON_SYS_LOG_SEL : /* System Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = "-l";
         args[6] = log_typ;
         args[7] = NULL;
         (void)strcpy(progname, SHOW_LOG);
         (void)strcpy(log_typ, MON_SYSTEM_STR);
	 make_xprocess(progname, progname, args, -1);
	 return;

      case MON_LOG_SEL : /* Monitor Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = "-l";
         args[6] = log_typ;
         (void)strcpy(progname, SHOW_LOG);
         break;

      case VIEW_FILE_LOAD_SEL : /* File Load */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-l";
         args[4] = log_typ;
         args[5] = "-f";
         args[6] = font_name;
         args[7] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_FILE_LOAD);
	 make_xprocess(progname, progname, args, -1);
	 return;

      case VIEW_KBYTE_LOAD_SEL : /* KByte Load */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-l";
         args[4] = log_typ;
         args[5] = "-f";
         args[6] = font_name;
         args[7] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_KBYTE_LOAD);
	 make_xprocess(progname, progname, args, -1);
	 return;

      case VIEW_CONNECTION_LOAD_SEL : /* Connection Load */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-l";
         args[4] = log_typ;
         args[5] = "-f";
         args[6] = font_name;
         args[7] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_CONNECTION_LOAD);
         make_xprocess(progname, progname, args, -1);
         return;

      case VIEW_TRANSFER_LOAD_SEL : /* Active Transfers Load */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-l";
         args[4] = log_typ;
         args[5] = "-f";
         args[6] = font_name;
         args[7] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_TRANSFER_LOAD);
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
         XFreeGC(display, tr_bar_gc);
         XFreeGC(display, color_gc);
         XFreeGC(display, black_line_gc);
         XFreeGC(display, white_line_gc);
         XFreeGC(display, led_gc);

         /* Free all the memory from the permission stuff. */
         if (mcp.mon_ctrl_list != NULL)
         {
            FREE_RT_ARRAY(mcp.mon_ctrl_list);
         }
         if (mcp.info_list != NULL)
         {
            FREE_RT_ARRAY(mcp.info_list);
         }
         if (mcp.retry_list != NULL)
         {
            FREE_RT_ARRAY(mcp.retry_list);
         }
         if (mcp.disable_list != NULL)
         {
            FREE_RT_ARRAY(mcp.disable_list);
         }
         if (mcp.show_slog_list != NULL)
         {
            FREE_RT_ARRAY(mcp.show_slog_list);
         }
         if (mcp.show_tlog_list != NULL)
         {
            FREE_RT_ARRAY(mcp.show_tlog_list);
         }
         if (mcp.show_ilog_list != NULL)
         {
            FREE_RT_ARRAY(mcp.show_ilog_list);
         }
         if (mcp.show_olog_list != NULL)
         {
            FREE_RT_ARRAY(mcp.show_olog_list);
         }
         if (mcp.afd_load_list != NULL)
         {
            FREE_RT_ARRAY(mcp.afd_load_list);
         }
         if (mcp.edit_hc_list != NULL)
         {
            FREE_RT_ARRAY(mcp.edit_hc_list);
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
    (void)fprintf(stderr, "Selected %d AFD's (", no_selected);
    for (i = j = 0; i < no_of_afds; i++)
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

   /* Set each host */
   k = display_error = 0;
   for (i = 0; i < no_of_afds; i++)
   {
      if (connect_data[i].inverse > OFF)
      {
         switch(sel_typ)
         {
            case MON_RETRY_SEL : /* Retry to connect to remote AFD only if */
                                 /* we are currently disconnected from it. */
               if ((msa[i].connect_status == DISCONNECTED) ||
                   (msa[i].connect_status == ERROR_ID))
               {
                  int  fd;
                  char retry_fifo[MAX_PATH_LENGTH];

                  (void)sprintf(retry_fifo, "%s%s%s%d",
                                p_work_dir, FIFO_DIR,
                                RETRY_MON_FIFO, i);
                  if ((fd = open(retry_fifo, O_RDWR)) == -1)
                  {
                     (void)xrec(appshell, ERROR_DIALOG,
                                "Failed to open() %s : %s (%s %d)",
                                retry_fifo, strerror(errno),
                                __FILE__, __LINE__);
                  }
                  else
                  {
                     if (write(fd, &i, sizeof(int)) != sizeof(int))
                     {
                        (void)xrec(appshell, ERROR_DIALOG,
                                   "Failed to write() to %s : %s (%s %d)",
                                   retry_fifo, strerror(errno),
                                   __FILE__, __LINE__);
                     }
                     if (close(fd) == -1)
                     {
                        (void)rec(sys_log_fd, DEBUG_SIGN,
                                  "Failed to close() FIFO %s : %s (%s %d)\n",
                                  retry_fifo, strerror(errno),
                                  __FILE__, __LINE__);
                     }
                  }
               }
               break;

            case MON_DISABLE_SEL : /* Enable/Disable AFD */
               if (msa[i].connect_status == DISABLED)
               {
                  int  fd;
                  char mon_cmd_fifo[MAX_PATH_LENGTH];

                  (void)sprintf(mon_cmd_fifo, "%s%s%s",
                                p_work_dir, FIFO_DIR, MON_CMD_FIFO);
                  if ((fd = open(mon_cmd_fifo, O_RDWR)) == -1)
                  {
                     (void)xrec(appshell, ERROR_DIALOG,
                                "Failed to open() %s : %s (%s %d)",
                                mon_cmd_fifo, strerror(errno),
                                __FILE__, __LINE__);
                  }
                  else
                  {
                     int length;
                     char cmd[2 + MAX_INT_LENGTH];

                     length = sprintf(cmd, "%c %d", ENABLE_MON, i);
                     if (write(fd, cmd, length) != length)
                     {
                        (void)xrec(appshell, ERROR_DIALOG,
                                   "Failed to write() to %s : %s (%s %d)",
                                   mon_cmd_fifo, strerror(errno),
                                   __FILE__, __LINE__);
                     }
                     (void)rec(sys_log_fd, CONFIG_SIGN,
                               "ENABLED monitoring for AFD %s (%s)\n",
                               msa[i].afd_alias, user);
                     if (close(fd) == -1)
                     {
                        (void)rec(sys_log_fd, DEBUG_SIGN,
                                  "Failed to close() FIFO %s : %s (%s %d)\n",
                                  mon_cmd_fifo, strerror(errno),
                                  __FILE__, __LINE__);
                     }
                  }
               }
               else
               {
                  if (xrec(appshell, QUESTION_DIALOG,
                      "Are you shure that you want to disable %s\nThis AFD will then not be monitored.",
                      msa[i].afd_alias) == YES)
                  {
                     int  fd;
                     char mon_cmd_fifo[MAX_PATH_LENGTH];

                     (void)sprintf(mon_cmd_fifo, "%s%s%s",
                                   p_work_dir, FIFO_DIR, MON_CMD_FIFO);
                     if ((fd = open(mon_cmd_fifo, O_RDWR)) == -1)
                     {
                        (void)xrec(appshell, ERROR_DIALOG,
                                   "Failed to open() %s : %s (%s %d)",
                                   mon_cmd_fifo, strerror(errno),
                                   __FILE__, __LINE__);
                     }
                     else
                     {
                        int length;
                        char cmd[2 + MAX_INT_LENGTH];

                        length = sprintf(cmd, "%c %d", DISABLE_MON, i);
                        if (write(fd, cmd, length) != length)
                        {
                           (void)xrec(appshell, ERROR_DIALOG,
                                      "Failed to write() to %s : %s (%s %d)",
                                      mon_cmd_fifo, strerror(errno),
                                      __FILE__, __LINE__);
                        }
                        (void)rec(sys_log_fd, CONFIG_SIGN,
                                  "DISABLED monitoring for AFD %s (%s)\n",
                                  msa[i].afd_alias, user);
                        if (close(fd) == -1)
                        {
                           (void)rec(sys_log_fd, DEBUG_SIGN,
                                     "Failed to close() FIFO %s : %s (%s %d)\n",
                                     mon_cmd_fifo, strerror(errno),
                                     __FILE__, __LINE__);
                        }
                     }
                  }
               }
               break;

            case MON_LOG_SEL : /* Monitor Log */
               (void)strcpy(hosts[k], msa[i].afd_alias);
               args[k + 7] = hosts[k];
               k++;
               break;

            case PING_SEL  :   /* Show ping test */
               {
                  (void)sprintf(ptr_ping_cmd, "%s %s\"",
                                msa[i].hostname, msa[i].afd_alias);
                  make_xprocess(progname, progname, args, i);
               }
               break;

            case TRACEROUTE_SEL  :   /* Show traceroute test */
               {
                  (void)sprintf(ptr_traceroute_cmd, "%s %s\"",
                                msa[i].hostname, msa[i].afd_alias);
                  make_xprocess(progname, progname, args, i);
               }
               break;

            case MON_INFO_SEL : /* Show information for this AFD. */
               {
                  int gotcha = NO,
                      ii;

                  for (ii = 0; ii < no_of_active_process; ii++)
                  {
                     if ((apps_list[ii].position == i) &&
                         (strcmp(apps_list[ii].progname, MON_INFO) == 0))
                     {
                        gotcha = YES;
                        break;
                     }
                  }
                  if (gotcha == NO)
                  {
                     args[6] = msa[i].afd_alias;
                     make_xprocess(progname, progname, args, i);
                  }
                  else
                  {
                     (void)xrec(appshell, INFO_DIALOG,
                                "Information dialog for %s is already open on your display.",
                                msa[i].afd_alias);
                  }
               }
               break;

            default :
               (void)xrec(appshell, WARN_DIALOG,
                          "Impossible selection! NOOO this can't be true! (%s %d)",
                          __FILE__, __LINE__);
               free(args);
               FREE_RT_ARRAY(hosts)
               return;
         }
      }
   } /* for (i = 0; i < no_of_afds; i++) */

   if (sel_typ == MON_LOG_SEL)
   {
      (void)strcpy(log_typ, MONITOR_STR);
      args[k + 7] = NULL;
      make_xprocess(progname, progname, args, -1);
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

   for (i = 0; i < no_of_afds; i++)
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

#define _WITH_MINUS_N_OPTION_
#ifdef _WITH_MINUS_N_OPTION_
/*######################## start_remote_prog() ##########################*/
void
start_remote_prog(Widget    w,
                  XtPointer client_data,
                  XtPointer call_data)
{
   int  i,
        item_no = (int)client_data;
   char **args,
        local_display[80],
        parameter[30],
        progname[MAX_PATH_LENGTH],
        prog_to_call[MAX_PROCNAME_LENGTH + 1],
        remote_user_parameter[3],
        rsh_str[4],
        no_block[3];

   if ((no_selected == 0) && (no_selected_static == 0))
   {
      (void)xrec(appshell, INFO_DIALOG,
                 "You must first select an AFD!\nUse mouse button 1 together with the SHIFT or CTRL key.");
      return;
   }
   if ((args = malloc(14 * sizeof(char *))) == NULL)
   {
      (void)xrec(appshell, FATAL_DIALOG, "malloc() error : %s [%d] (%s %d)",
                 strerror(errno), errno, __FILE__, __LINE__);
      return;
   }

   if ((no_selected == 0) && (no_selected_static == 0))
   {
      (void)xrec(appshell, INFO_DIALOG,
                 "You must first select an AFD!\nUse mouse button 1 together with the SHIFT or CTRL key.");
      return;
   }

   /*
    * Arglist is build up as follows:
    *  rsh -n  -l <username> host rafdd_cmd <display> <AFD workdir> cmd+args
    *  [0] [1] [2]   [3]     [4]     [5]      [6]          [7]
    */
   (void)strcpy(rsh_str, "rsh");
   (void)strcpy(remote_user_parameter, "-l");
   (void)strcpy(local_display, XDisplayName(NULL));
   if (local_display[0] == ':')
   {
      char hostname[90];

      if (gethostname(hostname, 80) == 0)
      {
         (void)strcat(hostname, local_display);
         (void)strcpy(local_display, hostname);
      }
   }
   (void)strcpy(no_block, "-n");
   args[0] = rsh_str;
   args[1] = no_block;
   args[2] = remote_user_parameter;
   args[5] = progname;
   args[6] = local_display;
   args[8] = prog_to_call;

   switch(item_no)
   {
      case AFD_CTRL_SEL : /* Remote afd_ctrl */
         (void)strcpy(prog_to_call, AFD_CTRL);
         args[9] = "-f";
         args[10] = font_name;
         args[11] = NULL;
         break;

      case S_LOG_SEL : /* Remote System Log */
         (void)strcpy(prog_to_call, SHOW_LOG);
         (void)strcpy(parameter, SYSTEM_STR);
         args[9] = "-f";
         args[10] = font_name;
         args[11] = "-l";
         args[12] = parameter;
         args[13] = NULL;
         break;

      case R_LOG_SEL : /* Remote Receive Log */
         (void)strcpy(prog_to_call, SHOW_LOG);
         (void)strcpy(parameter, RECEIVE_STR);
         args[9] = "-f";
         args[10] = font_name;
         args[11] = "-l";
         args[12] = parameter;
         args[13] = NULL;
         break;

      case T_LOG_SEL : /* Remote Transfer Log */
         (void)strcpy(prog_to_call, SHOW_LOG);
         (void)strcpy(parameter, TRANSFER_STR);
         args[9] = "-f";
         args[10] = font_name;
         args[11] = "-l";
         args[12] = parameter;
         args[13] = NULL;
         break;

      case I_LOG_SEL : /* Remote Input Log */
         (void)strcpy(prog_to_call, SHOW_ILOG);
         args[9] = "-f";
         args[10] = font_name;
         args[11] = NULL;
         break;

      case O_LOG_SEL : /* Remote Output Log */
         (void)strcpy(prog_to_call, SHOW_OLOG);
         args[9] = "-f";
         args[10] = font_name;
         args[11] = NULL;
         break;

      case E_LOG_SEL : /* Remote Delete Log */
         (void)strcpy(prog_to_call, SHOW_RLOG);
         args[9] = "-f";
         args[10] = font_name;
         args[11] = NULL;
         break;

      case VIEW_FILE_LOAD_SEL :
         (void)strcpy(prog_to_call, AFD_LOAD);
         (void)strcpy(parameter, SHOW_FILE_LOAD);
         args[9] = parameter;
         args[10] = "-f";
         args[11] = font_name;
         args[12] = NULL;
         break;

      case VIEW_KBYTE_LOAD_SEL :
         (void)strcpy(prog_to_call, AFD_LOAD);
         (void)strcpy(parameter, SHOW_KBYTE_LOAD);
         args[9] = parameter;
         args[10] = "-f";
         args[11] = font_name;
         args[12] = NULL;
         break;

      case VIEW_CONNECTION_LOAD_SEL :
         (void)strcpy(prog_to_call, AFD_LOAD);
         (void)strcpy(parameter, SHOW_CONNECTION_LOAD);
         args[9] = parameter;
         args[10] = "-f";
         args[11] = font_name;
         args[12] = NULL;
         break;

      case VIEW_TRANSFER_LOAD_SEL :
         (void)strcpy(prog_to_call, AFD_LOAD);
         (void)strcpy(parameter, SHOW_TRANSFER_LOAD);
         args[9] = parameter;
         args[10] = "-f";
         args[11] = font_name;
         args[12] = NULL;
         break;

      case CONTROL_AMG_SEL : /* Start/Stop AMG */
         (void)strcpy(prog_to_call, AFD_CMD);
         args[9] = "-Y";
         args[10] = NULL;
         break;

      case CONTROL_FD_SEL : /* Start/Stop FD */
         (void)strcpy(prog_to_call, AFD_CMD);
         args[9] = "-Z";
         args[10] = NULL;
         break;

      case REREAD_DIR_CONFIG_SEL : /* Reread DIR_CONFIG file */
         (void)strcpy(prog_to_call, "udc");
         args[9] = NULL;
         break;

      case REREAD_HOST_CONFIG_SEL : /* Reread HOST_CONFIG file */
         (void)strcpy(prog_to_call, "uhc");
         args[9] = NULL;
         break;

      case EDIT_HC_SEL : /* Edit HOST_CONFIG file */
         (void)strcpy(prog_to_call, EDIT_HC);
         args[9] = "-f";
         args[10] = font_name;
         args[11] = NULL;
         break;

      case DIR_CTRL_SEL : /* dir_control dialog */
         (void)strcpy(prog_to_call, DIR_CTRL);
         args[9] = "-f";
         args[10] = font_name;
         args[11] = NULL;
         break;

      case STARTUP_AFD_SEL : /* Startup AFD */
         (void)strcpy(prog_to_call, "afd");
         args[9] = "-a";
         args[10] = NULL;
	 break;

      case SHUTDOWN_AFD_SEL : /* Shutdown AFD */
         (void)strcpy(prog_to_call, "afd");
         args[9] = "-S";
         args[10] = NULL;
	 break;

      default :
         (void)xrec(appshell, INFO_DIALOG,
                    "This function [%d] has not yet been implemented.",
                    item_no);
         return;
   }

   for (i = 0; i < no_of_afds; i++)
   {
      if (connect_data[i].inverse > OFF)
      {
         if (msa[i].r_work_dir[0] == '\0')
         {
            (void)xrec(appshell, WARN_DIALOG,
                       "Did not yet receive remote working directory from %s.\nTry again latter.",
                       msa[i].afd_alias);
         }
         else
         {
            int gotcha = NO;

            if (item_no == AFD_CTRL_SEL)
            {
               int j;

               for (j = 0; j < no_of_active_process; j++)
               {
                  if ((apps_list[j].position == i) &&
                      (strcmp(apps_list[j].progname, AFD_CTRL) == 0))
                  {
                     gotcha = YES;
                     break;
                  }
               }
            }
            if (gotcha == NO)
            {
               int j;

               args[3] = username;
               for (j = 0; j < MAX_CONVERT_USERNAME; j++)
               {
                  if (msa[i].convert_username[j][0] != '\0')
                  {
                     if (strcmp(msa[i].convert_username[j][0], username) == 0)
                     {
                        args[3] = msa[i].convert_username[j][1];
                        break;
                     }
                  }
               }
               args[4] = msa[i].hostname;
               args[7] = msa[i].r_work_dir;
               (void)sprintf(progname, "%s/sbin/rafdd_cmd", msa[i].r_work_dir);
               make_xprocess(rsh_str, prog_to_call, args, i);
               if (connect_data[i].inverse == ON)
               {
                  connect_data[i].inverse = OFF;
                  draw_line_status(i, -1);
               }
            }
            else
            {
               (void)xrec(appshell, INFO_DIALOG,
                          "AFD_CTRL dialog for %s is already open on your display.",
                          msa[i].afd_alias);
            }
         }

         if (item_no == STARTUP_AFD_SEL)
         {
            (void)rec(mon_log_fd, INFO_SIGN,
                      "%-*s: AFD startup initiated by %s\n",
                      MAX_AFDNAME_LENGTH, msa[i].afd_alias,
                      user);
         }
         if (item_no == SHUTDOWN_AFD_SEL)
         {
            (void)rec(mon_log_fd, WARN_SIGN,
                      "%-*s: AFD shutdown initiated by %s\n",
                      MAX_AFDNAME_LENGTH, msa[i].afd_alias,
                      user);
         }
      }
   }

   return;
}
#else
/*######################## start_remote_prog() ##########################*/
void
start_remote_prog(Widget    w,
                  XtPointer client_data,
                  XtPointer call_data)
{
   int  i,
        item_no = (int)client_data;
   char **args,
        local_display[80],
        parameter[30],
        progname[MAX_PATH_LENGTH],
        prog_to_call[MAX_PROCNAME_LENGTH + 1],
        remote_user_parameter[3],
        rsh_str[4];

   if ((no_selected == 0) && (no_selected_static == 0))
   {
      (void)xrec(appshell, INFO_DIALOG,
                 "You must first select an AFD!\nUse mouse button 1 together with the SHIFT or CTRL key.");
      return;
   }
   if ((args = malloc(14 * sizeof(char *))) == NULL)
   {
      (void)xrec(appshell, FATAL_DIALOG, "malloc() error : %s [%d] (%s %d)",
                 strerror(errno), errno, __FILE__, __LINE__);
      return;
   }

   if ((no_selected == 0) && (no_selected_static == 0))
   {
      (void)xrec(appshell, INFO_DIALOG,
                 "You must first select an AFD!\nUse mouse button 1 together with the SHIFT or CTRL key.");
      return;
   }

   /*
    * Arglist is build up as follows:
    *  rsh -l <username> host rafdd_cmd <display> <AFD workdir> cmd+args
    *  [0] [1]   [2]     [3]     [4]      [5]          [6]
    */
   (void)strcpy(rsh_str, "rsh");
   (void)strcpy(remote_user_parameter, "-l");
   (void)strcpy(local_display, XDisplayName(NULL));
   args[0] = rsh_str;
   args[1] = remote_user_parameter;
   args[4] = progname;
   args[5] = local_display;
   args[7] = prog_to_call;

   switch(item_no)
   {
      case AFD_CTRL_SEL : /* Remote afd_ctrl */
         (void)strcpy(prog_to_call, AFD_CTRL);
         args[8] = "-f";
         args[9] = font_name;
         args[10] = NULL;
         break;

      case S_LOG_SEL : /* Remote System Log */
         (void)strcpy(prog_to_call, SHOW_LOG);
         (void)strcpy(parameter, SYSTEM_STR);
         args[8] = "-f";
         args[9] = font_name;
         args[10] = "-l";
         args[11] = parameter;
         args[12] = NULL;
         break;

      case R_LOG_SEL : /* Remote Receive Log */
         (void)strcpy(prog_to_call, SHOW_LOG);
         (void)strcpy(parameter, RECEIVE_STR);
         args[8] = "-f";
         args[9] = font_name;
         args[10] = "-l";
         args[11] = parameter;
         args[12] = NULL;
         break;

      case T_LOG_SEL : /* Remote Transfer Log */
         (void)strcpy(prog_to_call, SHOW_LOG);
         (void)strcpy(parameter, TRANSFER_STR);
         args[8] = "-f";
         args[9] = font_name;
         args[10] = "-l";
         args[11] = parameter;
         args[12] = NULL;
         break;

      case I_LOG_SEL : /* Remote Input Log */
         (void)strcpy(prog_to_call, SHOW_ILOG);
         args[8] = "-f";
         args[9] = font_name;
         args[10] = NULL;
         break;

      case O_LOG_SEL : /* Remote Output Log */
         (void)strcpy(prog_to_call, SHOW_OLOG);
         args[8] = "-f";
         args[9] = font_name;
         args[10] = NULL;
         break;

      case E_LOG_SEL : /* Remote Delete Log */
         (void)strcpy(prog_to_call, SHOW_RLOG);
         args[8] = "-f";
         args[9] = font_name;
         args[10] = NULL;
         break;

      case VIEW_FILE_LOAD_SEL :
         (void)strcpy(prog_to_call, AFD_LOAD);
         (void)strcpy(parameter, SHOW_FILE_LOAD);
         args[8] = parameter;
         args[9] = "-f";
         args[10] = font_name;
         args[11] = NULL;
         break;

      case VIEW_KBYTE_LOAD_SEL :
         (void)strcpy(prog_to_call, AFD_LOAD);
         (void)strcpy(parameter, SHOW_KBYTE_LOAD);
         args[8] = parameter;
         args[9] = "-f";
         args[10] = font_name;
         args[11] = NULL;
         break;

      case VIEW_CONNECTION_LOAD_SEL :
         (void)strcpy(prog_to_call, AFD_LOAD);
         (void)strcpy(parameter, SHOW_CONNECTION_LOAD);
         args[8] = parameter;
         args[9] = "-f";
         args[10] = font_name;
         args[11] = NULL;
         break;

      case VIEW_TRANSFER_LOAD_SEL :
         (void)strcpy(prog_to_call, AFD_LOAD);
         (void)strcpy(parameter, SHOW_TRANSFER_LOAD);
         args[8] = parameter;
         args[9] = "-f";
         args[10] = font_name;
         args[11] = NULL;
         break;

      case CONTROL_AMG_SEL : /* Start/Stop AMG */
         (void)strcpy(prog_to_call, AFD_CMD);
         args[8] = "-Y";
         args[9] = NULL;
         break;

      case CONTROL_FD_SEL : /* Start/Stop FD */
         (void)strcpy(prog_to_call, AFD_CMD);
         args[8] = "-Z";
         args[9] = NULL;
         break;

      case REREAD_DIR_CONFIG_SEL : /* Reread DIR_CONFIG file */
         (void)strcpy(prog_to_call, "udc");
         args[8] = NULL;
         break;

      case REREAD_HOST_CONFIG_SEL : /* Reread HOST_CONFIG file */
         (void)strcpy(prog_to_call, "uhc");
         args[8] = NULL;
         break;

      case EDIT_HC_SEL : /* Edit HOST_CONFIG file */
         (void)strcpy(prog_to_call, EDIT_HC);
         args[8] = "-f";
         args[9] = font_name;
         args[10] = NULL;
         break;

      case DIR_CTRL_SEL : /* dir_ctrl dialog */
         (void)strcpy(prog_to_call, DIR_CTRL);
         args[8] = "-f";
         args[9] = font_name;
         args[10] = NULL;
         break;

      case STARTUP_AFD_SEL : /* Startup AFD */
         (void)strcpy(prog_to_call, "afd");
         args[9] = "-a";
         args[10] = NULL;
	 break;

      case SHUTDOWN_AFD_SEL : /* Shutdown AFD */
         (void)strcpy(prog_to_call, "afd");
         args[8] = "-S";
         args[9] = NULL;
	 break;

      default :
         (void)xrec(appshell, INFO_DIALOG,
                    "This function [%d] has not yet been implemented.",
                    item_no);
         return;
   }

   for (i = 0; i < no_of_afds; i++)
   {
      if (connect_data[i].inverse > OFF)
      {
         if (msa[i].r_work_dir[0] == '\0')
         {
            (void)xrec(appshell, WARN_DIALOG,
                       "Did not yet receive remote working directory from %s.\nTry again latter.",
                       msa[i].afd_alias);
         }
         else
         {
            int gotcha = NO;

            if (item_no == AFD_CTRL_SEL)
            {
               int j;

               for (j = 0; j < no_of_active_process; j++)
               {
                  if ((apps_list[j].position == i) &&
                      (strcmp(apps_list[j].progname, AFD_CTRL) == 0))
                  {
                     gotcha = YES;
                     break;
                  }
               }
            }
            if (gotcha == NO)
            {
               int j;

               args[2] = username;
               for (j = 0; j < MAX_CONVERT_USERNAME; j++)
               {
                  if (msa[i].convert_username[j][0] != '\0')
                  {
                     if (strcmp(msa[i].convert_username[j][0], username) == 0)
                     {
                        args[2] = msa[i].convert_username[j][1];
                        break;
                     }
                  }
               }
               args[3] = msa[i].hostname;
               args[6] = msa[i].r_work_dir;
               (void)sprintf(progname, "%s/sbin/rafdd_cmd", msa[i].r_work_dir);
               make_xprocess(rsh_str, prog_to_call, args, i);
               if (connect_data[i].inverse == ON)
               {
                  connect_data[i].inverse = OFF;
                  draw_line_status(i, -1);
               }
            }
            else
            {
               (void)xrec(appshell, INFO_DIALOG,
                          "AFD_CTRL dialog for %s is already open on your display.",
                          msa[i].afd_alias);
            }
         }
      }
   }

   return;
}
#endif


/*######################## change_mon_font_cb() #########################*/
void
change_mon_font_cb(Widget    w,
                   XtPointer client_data,
                   XtPointer call_data)
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
   setup_mon_window(font_name);

   /* Load the font into the old GC */
   gc_values.font = font_struct->fid;
   XChangeGC(display, letter_gc, GCFont, &gc_values);
   XChangeGC(display, normal_letter_gc, GCFont, &gc_values);
   XChangeGC(display, locked_letter_gc, GCFont, &gc_values);
   XChangeGC(display, color_letter_gc, GCFont, &gc_values);
   XChangeGC(display, red_color_letter_gc, GCFont, &gc_values);
   XChangeGC(display, red_error_letter_gc, GCFont, &gc_values);
   XFlush(display);

   /* resize and redraw window if necessary */
   if (resize_mon_window() == YES)
   {
      XClearWindow(display, line_window);

      /* redraw label */
      draw_label_line();

      /* redraw all status lines */
      for (i = 0; i < no_of_afds; i++)
      {
         draw_line_status(i, 1);
      }

      redraw = YES;
   }

   if (redraw == YES)
   {
      XFlush(display);
   }

   return;
}


/*######################## change_mon_rows_cb() #########################*/
void
change_mon_rows_cb(Widget    w,
                   XtPointer client_data,
                   XtPointer call_data)
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

   if (resize_mon_window() == YES)
   {
      XClearWindow(display, line_window);

      /* redraw label */
      draw_label_line();

      /* redraw all status lines */
      for (i = 0; i < no_of_afds; i++)
      {
         draw_line_status(i, 1);
      }

      redraw = YES;
   }

   if (redraw == YES)
   {
      XFlush(display);
   }

  return;
}


/*######################## change_mon_style_cb() ########################*/
void
change_mon_style_cb(Widget    w,
                    XtPointer client_data,
                    XtPointer call_data)
{
   int item_no = (int)client_data,
       i,
       redraw = NO;

   if (current_style != item_no)
   {
      XtVaSetValues(lsw[current_style], XmNset, False, NULL);
      current_style = item_no;
   }

   switch(item_no)
   {
      case 0   :
         line_style = BARS_ONLY;
         break;

      case 1   :
         line_style = CHARACTERS_ONLY;
         break;

      case 2   :
         line_style = CHARACTERS_AND_BARS;
         break;

      default  :
         (void)xrec(appshell, WARN_DIALOG, "Impossible row selection (%d).",
                    item_no);
         return;
   }

#ifdef _DEBUG
   switch(line_style)
   {
      case BARS_ONLY             :
         (void)fprintf(stderr, "Changing line style to bars only.\n");
         break;

      case CHARACTERS_ONLY       :
         (void)fprintf(stderr, "Changing line style to characters only.\n");
         break;

      case CHARACTERS_AND_BARS   :
         (void)fprintf(stderr, "Changing line style to bars and characters.\n");
         break;

      default                    : /* IMPOSSIBLE !!! */
         break;
   }
#endif

   setup_mon_window(font_name);

   if (resize_mon_window() == YES)
   {
      XClearWindow(display, line_window);

      /* redraw label */
      draw_label_line();

      /* redraw all status lines */
      for (i = 0; i < no_of_afds; i++)
      {
         draw_line_status(i, 1);
      }

      redraw = YES;
   }

   if (redraw == YES)
   {
      XFlush(display);
   }

   return;
}


/*###################### change_mon_history_cb() ########################*/
void
change_mon_history_cb(Widget    w,
                      XtPointer client_data,
                      XtPointer call_data)
{
   int item_no = (int)client_data,
       i,
       redraw = NO;

   if (current_his_log != item_no)
   {
      XtVaSetValues(hlw[current_his_log], XmNset, False, NULL);
      current_his_log = item_no;
   }

   switch(item_no)
   {
      case 0   :
         his_log_set = atoi(HIS_0);
         break;

      case 1   :
         his_log_set = atoi(HIS_1);
         break;

      case 2   :
         his_log_set = atoi(HIS_2);
         break;

      case 3   :
         his_log_set = atoi(HIS_3);
         break;

      case 4   :
         his_log_set = atoi(HIS_4);
         break;

      default  :
         (void)xrec(appshell, WARN_DIALOG, "Impossible history selection (%d).",
                    item_no);
         return;
   }

#ifdef _DEBUG
   (void)fprintf(stderr, "%s: You have chosen: %d history logs\n",
                 __FILE__, his_log_set);
#endif

   setup_mon_window(font_name);

   if (resize_mon_window() == YES)
   {
      XClearWindow(display, line_window);

      /* redraw label */
      draw_label_line();

      /* redraw all status lines */
      for (i = 0; i < no_of_afds; i++)
      {
         if (his_log_set > 0)
         {
            (void)memcpy(connect_data[i].log_history, msa[i].log_history,
                         (NO_OF_LOG_HISTORY * MAX_LOG_HISTORY));
         }
         draw_line_status(i, 1);
      }

      redraw = YES;
   }

   if (redraw == YES)
   {
      XFlush(display);
   }

  return;
}

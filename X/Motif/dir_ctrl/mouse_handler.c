/*
 *  mouse_handler.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mouse_handler - handles all mouse- AND key events of the dir_ctrl
 **                   dialog
 **
 ** SYNOPSIS
 **   void dir_focus(Widget w, XtPointer client_data, XEvent *event)
 **   void dir_input(Widget w, XtPointer client_data, XtPointer call_data)
 **   void popup_dir_menu_cb(Widget w, XtPointer client_data, XEvent *event)
 **   void save_dir_setup_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void dir_popup_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_dir_font_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_dir_row_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_dir_style_cb(Widget w, XtPointer client_data, XtPointer call_data)
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
 **   31.08.2000 H.Kiehl Created
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
#include "dir_ctrl.h"
#include "permission.h"

/* External global variables */
extern Display                    *display;
extern Widget                     fw[],
                                  rw[],
                                  tw[],
                                  lsw[];
extern Widget                     appshell;
extern Window                     line_window;
extern XFontStruct                *font_struct;
extern GC                         letter_gc,
                                  normal_letter_gc,
                                  locked_letter_gc,
                                  color_letter_gc,
                                  red_color_letter_gc,
                                  default_bg_gc,
                                  normal_bg_gc,
                                  locked_bg_gc,
                                  label_bg_gc,
                                  tr_bar_gc,
                                  fr_bar_gc,
                                  color_gc,
                                  black_line_gc,
                                  white_line_gc;
extern int                        no_of_active_process,
                                  no_of_dirs,
                                  no_of_jobs_selected,
                                  line_length,
                                  line_height,
                                  no_selected,
                                  no_selected_static,
                                  no_of_rows,
                                  no_of_rows_set,
                                  current_font,
                                  current_row,
                                  current_style,
                                  sys_log_fd;
extern float                      max_bar_length;
extern unsigned long              color_pool[];
extern char                       *p_work_dir,
                                  line_style,
                                  font_name[],
                                  user[],
                                  username[];
extern struct dir_line            *connect_data;
extern struct fileretrieve_status *fra;
extern struct dir_control_perm    dcp;
extern struct apps_list           *apps_list;

/* Global variables. */
int                               fsa_fd = -1,
                                  fsa_id,
                                  no_of_hosts;
#ifndef _NO_MMAP
off_t                             fsa_size;
#endif
struct filetransfer_status        *fsa;

/* Local global variables */
static int                        in_window = NO;


/*############################ dir_focus() ##############################*/
void
dir_focus(Widget      w,
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


/*############################ dir_input() ##############################*/
void
dir_input(Widget      w,
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

      if ((select_no < no_of_dirs) && (last_motion_pos != select_no))
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

            draw_dir_line_status(select_no, select_no);
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

                 draw_dir_line_status(select_no, 1);
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

      /* Make sure that this field does contain a channel. */
      if (select_no < no_of_dirs)
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
                   (strcmp(apps_list[i].progname, DIR_INFO) == 0))
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
               args[3] = "-d";
               args[4] = fra[select_no].dir_alias;
               args[5] = NULL;
               (void)strcpy(progname, DIR_INFO);

               make_xprocess(progname, progname, args, select_no);
            }
            else
            {
               (void)xrec(appshell, INFO_DIALOG,
                          "Information dialog for %s is already open on your display.",
                          fra[select_no].dir_alias);
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

                    draw_dir_line_status(select_no, 1);
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

                         draw_dir_line_status(select_no, 1);
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


/*########################## popup_dir_menu_cb() ########################*/
void
popup_dir_menu_cb(Widget      w,
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


/*######################## save_dir_setup_cb() ##########################*/
void
save_dir_setup_cb(Widget    w,
                  XtPointer client_data,
                  XtPointer call_data)
{
   write_setup(-1, -1);

   return;
}


/*########################### dir_popup_cb() ############################*/
void
dir_popup_cb(Widget    w,
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
               **args = NULL,
               **dirs = NULL,
               **hosts = NULL,
               log_typ[30],
               display_error,
       	       err_msg[1025 + 100];
   size_t      new_size = (no_of_dirs + 8) * sizeof(char *);

   if ((no_selected == 0) && (no_selected_static == 0) &&
       ((sel_typ == DIR_DISABLE_SEL) || (sel_typ == DIR_INFO_SEL)))
   {
      (void)xrec(appshell, INFO_DIALOG,
                 "You must first select a directory!\nUse mouse button 1 together with the SHIFT or CTRL key.");
      return;
   }
   RT_ARRAY(dirs, no_of_dirs, (MAX_DIR_ALIAS_LENGTH + 1), char);
   RT_ARRAY(hosts, no_of_dirs, (MAX_HOSTNAME_LENGTH + 1), char);
   if ((args = malloc(new_size)) == NULL)
   {
      (void)xrec(appshell, FATAL_DIALOG, "malloc() error : %s [%d] (%s %d)",
                 strerror(errno), errno, __FILE__, __LINE__);
      return;
   }

   switch(sel_typ)
   {
      case DIR_DISABLE_SEL:
         break;

      case DIR_INFO_SEL : /* Information */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = "-d";
         args[7] = NULL;
         (void)strcpy(progname, DIR_INFO);
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
         (void)strcpy(progname, SHOW_LOG);
         (void)strcpy(log_typ, RECEIVE_STR);
         break;

      case T_LOG_SEL : /* Transfer Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = "-l";
         args[6] = log_typ;
         (void)strcpy(progname, SHOW_LOG);
         (void)strcpy(log_typ, TRANSFER_STR);
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

      case SHOW_QUEUE_SEL : /* Queue */
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
         XFreeGC(display, fr_bar_gc);
         XFreeGC(display, color_gc);
         XFreeGC(display, black_line_gc);
         XFreeGC(display, white_line_gc);

         /* Free all the memory from the permission stuff. */
         if (dcp.info_list != NULL)
         {
            FREE_RT_ARRAY(dcp.info_list);
         }
         if (dcp.disable_list != NULL)
         {
            FREE_RT_ARRAY(dcp.disable_list);
         }
         if (dcp.show_slog_list != NULL)
         {
            FREE_RT_ARRAY(dcp.show_slog_list);
         }
         if (dcp.show_tlog_list != NULL)
         {
            FREE_RT_ARRAY(dcp.show_tlog_list);
         }
         if (dcp.show_ilog_list != NULL)
         {
            FREE_RT_ARRAY(dcp.show_ilog_list);
         }
         if (dcp.show_olog_list != NULL)
         {
            FREE_RT_ARRAY(dcp.show_olog_list);
         }
         if (dcp.afd_load_list != NULL)
         {
            FREE_RT_ARRAY(dcp.afd_load_list);
         }
         free(connect_data);
         free(args);
         FREE_RT_ARRAY(dirs);
         FREE_RT_ARRAY(hosts);
         exit(SUCCESS);

      default  :
         (void)xrec(appshell, WARN_DIALOG,
                    "Impossible item selection (%d).", sel_typ);
         free(args);
         FREE_RT_ARRAY(dirs)
         FREE_RT_ARRAY(hosts);
         return;
    }
#ifdef _DEBUG
    (void)fprintf(stderr, "Selected %d directories (", no_selected);
    for (i = j = 0; i < no_of_dirs; i++)
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

   if (sel_typ == T_LOG_SEL)
   {
      if (fsa_attach() == INCORRECT)
      {
         (void)xrec(appshell, FATAL_DIALOG, "Failed to attach to FSA! (%s %d)",
                    __FILE__, __LINE__);
         return;
      }
   }

   /* Set each directory. */
   k = display_error = 0;
   for (i = 0; i < no_of_dirs; i++)
   {
      if (connect_data[i].inverse > OFF)
      {
         switch(sel_typ)
         {
            case DIR_DISABLE_SEL : /* Enable/Disable Directory. */
               if (fra[i].dir_status == DISABLED)
               {
                  fra[i].dir_status = NORMAL_STATUS;
                  (void)rec(sys_log_fd, CONFIG_SIGN,
                            "ENABLED directory %s (%s)\n",
                            fra[i].dir_alias, user);
               }
               else
               {
                  if (xrec(appshell, QUESTION_DIALOG,
                      "Are you shure that you want to disable %s\nThis directory will then not be monitored.",
                      fra[i].dir_alias) == YES)
                  {
                     fra[i].dir_status = DISABLED;
                     (void)rec(sys_log_fd, CONFIG_SIGN,
                               "DISABLED directory %s (%s)\n",
                               fra[i].dir_alias, user);
                  }
               }
               break;

            case DIR_INFO_SEL : /* Show information for this directory. */
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
                     args[6] = fra[i].dir_alias;
                     make_xprocess(progname, progname, args, i);
                  }
                  else
                  {
                     (void)xrec(appshell, INFO_DIALOG,
                                "Information dialog for %s is already open on your display.",
                                fra[i].dir_alias);
                  }
               }
               break;

            case I_LOG_SEL : /* View Input Log. */
            case O_LOG_SEL : /* View Output Log. */
            case E_LOG_SEL : /* View Delete Log. */
               (void)strcpy(dirs[k], fra[i].dir_alias);
               args[k + 5] = dirs[k];
               k++;
               break;

            case R_LOG_SEL : /* View Retrieve Log. */
               (void)strcpy(dirs[k], fra[i].dir_alias);
               args[k + 7] = dirs[k];
               k++;
               break;

            case T_LOG_SEL : /* View Transfer Log. */
               if (fra[i].host_alias[0] != '\0')
               {
                  (void)strcpy(hosts[k], fra[i].host_alias);
                  if (fsa[fra[i].fsa_pos].host_toggle_str[0] != '\0')
                  {
                     (void)strcat(hosts[k], "?");
                  }
                  args[k + 7] = hosts[k];
                  k++;
               }
               break;

            case SHOW_QUEUE_SEL :
               (void)xrec(appshell, WARN_DIALOG,
                          "Not implemented. Tell this lazy programmer to do some work! (%s %d)",
                          __FILE__, __LINE__);
               free(args);
               FREE_RT_ARRAY(dirs)
               FREE_RT_ARRAY(hosts)
               return;

            default :
               (void)xrec(appshell, WARN_DIALOG,
                          "Impossible selection! NOOO this can't be true! (%s %d)",
                          __FILE__, __LINE__);
               free(args);
               FREE_RT_ARRAY(dirs)
               FREE_RT_ARRAY(hosts)
               return;
         }
      }
   } /* for (i = 0; i < no_of_dirs; i++) */

   if (sel_typ == T_LOG_SEL)
   {
      (void)fsa_detach(NO);
   }

   if ((sel_typ == R_LOG_SEL) || (sel_typ == T_LOG_SEL))
   {
      args[k + 7] = NULL;
      make_xprocess(progname, progname, args, -1);
   }
   else if ((sel_typ == O_LOG_SEL) || (sel_typ == E_LOG_SEL) ||
            (sel_typ == I_LOG_SEL) || (sel_typ == SHOW_QUEUE_SEL))
        {
           args[k + 5] = NULL;
           make_xprocess(progname, progname, args, -1);
        }

   /* Memory for arg list stuff no longer needed. */
   free(args);
   FREE_RT_ARRAY(dirs)
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

   for (i = 0; i < no_of_dirs; i++)
   {
      if (connect_data[i].inverse == ON)
      {
         connect_data[i].inverse = OFF;
         draw_dir_line_status(i, -1);
      }
   }

   /* Make sure that all changes are shown */
   XFlush(display);

   no_selected = 0;

   return;
}


/*######################## change_dir_font_cb() #########################*/
void
change_dir_font_cb(Widget    w,
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
   setup_dir_window(font_name);

   /* Load the font into the old GC */
   gc_values.font = font_struct->fid;
   XChangeGC(display, letter_gc, GCFont, &gc_values);
   XChangeGC(display, normal_letter_gc, GCFont, &gc_values);
   XChangeGC(display, locked_letter_gc, GCFont, &gc_values);
   XChangeGC(display, color_letter_gc, GCFont, &gc_values);
   XChangeGC(display, red_color_letter_gc, GCFont, &gc_values);
   XFlush(display);

   /* resize and redraw window if necessary */
   if (resize_dir_window() == YES)
   {
      XClearWindow(display, line_window);

      /* redraw label */
      draw_dir_label_line();

      /* redraw all status lines */
      for (i = 0; i < no_of_dirs; i++)
      {
         draw_dir_line_status(i, 1);
      }

      redraw = YES;
   }

   if (redraw == YES)
   {
      XFlush(display);
   }

   return;
}


/*######################## change_dir_rows_cb() #########################*/
void
change_dir_rows_cb(Widget    w,
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

   if (resize_dir_window() == YES)
   {
      XClearWindow(display, line_window);

      /* redraw label */
      draw_dir_label_line();

      /* redraw all status lines */
      for (i = 0; i < no_of_dirs; i++)
      {
         draw_dir_line_status(i, 1);
      }

      redraw = YES;
   }

   if (redraw == YES)
   {
      XFlush(display);
   }

  return;
}


/*######################## change_dir_style_cb() ########################*/
void
change_dir_style_cb(Widget    w,
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

   setup_dir_window(font_name);

   if (resize_dir_window() == YES)
   {
      XClearWindow(display, line_window);

      /* redraw label */
      draw_dir_label_line();

      /* redraw all status lines */
      for (i = 0; i < no_of_dirs; i++)
      {
         draw_dir_line_status(i, 1);
      }

      redraw = YES;
   }

   if (redraw == YES)
   {
      XFlush(display);
   }

   return;
}

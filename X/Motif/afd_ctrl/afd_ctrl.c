/*
 *  afd_ctrl.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M1
/*
 ** NAME
 **   afd_ctrl - controls and monitors the AFD
 **
 ** SYNOPSIS
 **   afd_ctrl [--version][-w <AFD working directory>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.01.1996 H.Kiehl Created
 **   14.08.1997 H.Kiehl Support for multi-selection of lines.
 **
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf(), stderr                       */
#include <string.h>           /* strcpy(), strcmp()                      */
#include <ctype.h>            /* toupper()                               */
#include <unistd.h>           /* gethostname(), getcwd(), STDERR_FILENO  */
#include <stdlib.h>           /* getenv(), calloc()                      */
#include <math.h>             /* log10()                                 */
#include <sys/times.h>        /* times(), struct tms                     */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _NO_MMAP
#include <sys/mman.h>         /* mmap()                                  */
#endif
#include <signal.h>           /* kill(), signal(), SIGINT                */
#include <fcntl.h>            /* O_RDWR                                  */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#ifdef _EDITRES
#include <X11/Xmu/Editres.h>
#endif

#include "afd_ctrl.h"
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/Form.h>
#include <Xm/PushBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/CascadeBG.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/DrawingA.h>
#include <Xm/CascadeB.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <errno.h>
#include "version.h"
#include "permission.h"

/* Global variables */
Display                    *display;
XtAppContext               app;
XtIntervalId               interval_id_host,
                           interval_id_status,
                           interval_id_tv;
GC                         letter_gc,
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
Colormap                   default_cmap;
XFontStruct                *font_struct;
XmFontList                 fontlist = NULL;
Widget                     mw[5],        /* Main menu */
                           ow[10],       /* Host menu */
                           vw[8],        /* View menu */
                           cw[7],        /* Control menu */
                           sw[4],        /* Setup menu */
                           hw[3],        /* Help menu */
                           fw[13],       /* Select font */
                           rw[13],       /* Select rows */
                           tw[2],        /* Test (ping, traceroute) */
                           lw[4],        /* AFD load */
                           lsw[3];
Widget                     appshell,
                           button_window_w,
                           detailed_window_w,
                           label_window_w,
                           line_window_w,
                           transviewshell = NULL,
                           tv_label_window_w;
Window                     button_window,
                           detailed_window,
                           label_window,
                           line_window,
                           tv_label_window;
float                      max_bar_length;
int                        amg_flag = NO,
                           bar_thickness_2,
                           bar_thickness_3,
                           button_line_length,
                           button_width,
                           current_font = -1,
                           current_row = -1,
                           current_style = -1,
                           filename_display_length,
                           fsa_fd = -1,
                           fsa_id,
                           ft_exposure_tv_line = 0,
                           led_width,
                           line_length,
                           line_height = 0,
                           log_angle,
                           no_selected,
                           no_selected_static,
                           no_of_active_process = 0,
                           no_of_columns,
                           no_of_rows,
                           no_of_rows_set,
                           no_of_hosts,
                           no_of_jobs_selected,
                           redraw_time_host,
                           redraw_time_status,
                           sys_log_fd = STDERR_FILENO,
                           tv_line_length,
                           tv_no_of_columns,
                           tv_no_of_rows,
                           window_width,
                           window_height,
                           x_center_sys_log,
                           x_center_trans_log,
                           x_offset_led,
                           x_offset_debug_led,
                           x_offset_proc,
                           x_offset_bars,
                           x_offset_characters,
                           x_offset_stat_leds,
                           x_offset_sys_log,
                           x_offset_trans_log,
                           x_offset_rotating_dash,
                           x_offset_tv_characters,
                           x_offset_tv_bars,
                           x_offset_tv_file_name,
                           y_center_log,
                           y_offset_led;
Dimension                  tv_window_height,
                           tv_window_width;
#ifndef _NO_MMAP
off_t                      fsa_size,
                           afd_active_size;
#endif
time_t                     afd_active_time;
unsigned short             step_size;
unsigned long              color_pool[COLOR_POOL_SIZE];
unsigned int               glyph_height,
                           glyph_width,
                           text_offset;
char                       work_dir[MAX_PATH_LENGTH],
                           *p_work_dir,
                           *pid_list,
                           afd_active_file[MAX_PATH_LENGTH],
                           line_style,
                           font_name[20],
                           tv_window = OFF,
                           blink_flag,
                           *ping_cmd = NULL,
                           *ptr_ping_cmd,
                           *traceroute_cmd = NULL,
                           *ptr_traceroute_cmd,
                           user[MAX_FILENAME_LENGTH];
clock_t                    clktck;
struct apps_list           *apps_list = NULL;
struct coord               coord[2][LOG_FIFO_SIZE];
struct line                *connect_data;
struct job_data            *jd = NULL;
struct afd_status          *p_afd_status,
                           prev_afd_status;
struct filetransfer_status *fsa;
struct afd_control_perm    acp;

/* Local function prototypes */
static void                afd_ctrl_exit(void),
                           create_pullright_font(Widget),
                           create_pullright_load(Widget),
                           create_pullright_row(Widget),
                           create_pullright_style(Widget),
                           create_pullright_test(Widget),
                           eval_permissions(char *),
                           init_menu_bar(Widget, Widget *),
                           init_popup_menu(Widget),
                           init_afd_ctrl(int *, char **, char *),
                           sig_bus(int),
                           sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char          window_title[100];
   static String fallback_res[] =
                 {
                    "*mwmDecorations : 42",
                    "*mwmFunctions : 12",
                    ".afd_ctrl*background : NavajoWhite2",
                    NULL
                 };
   Widget        mainform_w,
                 mainwindow,
                 menu_w;
   Arg           args[MAXARGS];
   Cardinal      argcount;

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values */
   init_afd_ctrl(&argc, argv, window_title);

#ifdef _X_DEBUG
   XSynchronize(display, 1);
#endif

   /* Create the top-level shell widget and initialise the toolkit */
   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   appshell = XtAppInitialize(&app, "AFD", NULL, 0, &argc, argv,
                              fallback_res, args, argcount);

   mainwindow = XtVaCreateManagedWidget("Main_window",
                                        xmMainWindowWidgetClass, appshell,
                                        NULL);
   display = XtDisplay(mainwindow);

   /* setup and determine window parameters */
   setup_window(font_name);

   /* Get window size */
   (void)window_size(&window_width, &window_height);

   /* Create managing widget for label, line and button widget */
   mainform_w = XmCreateForm(mainwindow, "mainform_w", NULL, 0);
   XtManageChild(mainform_w);

   init_menu_bar(mainform_w, &menu_w);

   /* Setup colors */
   default_cmap = DefaultColormap(display, DefaultScreen(display));
   init_color(XtDisplay(appshell));

   /* Create the label_window_w */
   argcount = 0;
   XtSetArg(args[argcount], XmNheight, (Dimension) line_height);
   argcount++;
   XtSetArg(args[argcount], XmNwidth, (Dimension) window_width);
   argcount++;
   XtSetArg(args[argcount], XmNbackground, color_pool[LABEL_BG]);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,     menu_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   label_window_w = XmCreateDrawingArea(mainform_w, "label_window_w", args,
                                        argcount);
   XtManageChild(label_window_w);

   /* Get background color from the widget's resources */
   argcount = 0;
   XtSetArg(args[argcount], XmNbackground, &color_pool[LABEL_BG]);
   argcount++;
   XtGetValues(label_window_w, args, argcount);

   /* Create the line_window_w */
   argcount = 0;
   XtSetArg(args[argcount], XmNheight, (Dimension) window_height);
   argcount++;
   XtSetArg(args[argcount], XmNwidth, (Dimension) window_width);
   argcount++;
   XtSetArg(args[argcount], XmNbackground, color_pool[DEFAULT_BG]);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget, label_window_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   line_window_w = XmCreateDrawingArea(mainform_w, "line_window_w", args,
                                       argcount);
   XtManageChild(line_window_w);

   /* Initialise the GC's */
   init_gcs();

   /* Get foreground color from the widget's resources */
   argcount = 0;
   XtSetArg(args[argcount], XmNforeground, &color_pool[FG]); argcount++;
   XtGetValues(line_window_w, args, argcount);

   /* Create the button_window_w */
   argcount = 0;
   XtSetArg(args[argcount], XmNheight, (Dimension) line_height);
   argcount++;
   XtSetArg(args[argcount], XmNwidth, (Dimension) window_width);
   argcount++;
   XtSetArg(args[argcount], XmNbackground, color_pool[LABEL_BG]);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget, line_window_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   button_window_w = XmCreateDrawingArea(mainform_w, "button_window_w", args,
                                         argcount);
   XtManageChild(button_window_w);

   /* Get background color from the widget's resources */
   argcount = 0;
   XtSetArg(args[argcount], XmNbackground, &color_pool[LABEL_BG]);
   argcount++;
   XtGetValues(button_window_w, args, argcount);

   /* Add call back to handle expose events for the label window */
   XtAddCallback(label_window_w, XmNexposeCallback,
                 (XtCallbackProc)expose_handler_label, (XtPointer)0);

   /* Add call back to handle expose events for the line window */
   XtAddCallback(line_window_w, XmNexposeCallback,
                 (XtCallbackProc)expose_handler_line, NULL);

   /* Add call back to handle expose events for the button window */
   XtAddCallback(button_window_w, XmNexposeCallback,
                 (XtCallbackProc)expose_handler_button, NULL);

   XtAddEventHandler(line_window_w,
                     ButtonPressMask | Button1MotionMask,
                     False, (XtEventHandler)input, NULL);

   /* Set toggle button for font|row */
   XtVaSetValues(fw[current_font], XmNset, True, NULL);
   XtVaSetValues(rw[current_row], XmNset, True, NULL);
   XtVaSetValues(lsw[current_style], XmNset, True, NULL);

   /* Setup popup menu */
   init_popup_menu(line_window_w);

   XtAddEventHandler(line_window_w,
                     EnterWindowMask | LeaveWindowMask,
                     False, (XtEventHandler)focus, NULL);

#ifdef _EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True, _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets */
   XtRealizeWidget(appshell);

   /* Set some signal handlers. */
   if ((signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(appshell, WARN_DIALOG,
                 "Failed to set signal handlers for afd_ctrl : %s",
                 strerror(errno));
   }

   /* Exit handler so we can close applications that the user started. */
   if (atexit(afd_ctrl_exit) != 0)
   {
      (void)xrec(appshell, WARN_DIALOG,
                 "Failed to set exit handler for afd_ctrl : %s\n\nWill not be able to close applications when terminating.",
                 strerror(errno));
   }

   /* Get window ID of three main windows */
   label_window = XtWindow(label_window_w);
   line_window = XtWindow(line_window_w);
   button_window = XtWindow(button_window_w);

   /* Start the main event-handling loop */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ init_afd_ctrl() ++++++++++++++++++++++++++*/
static void
init_afd_ctrl(int *argc, char *argv[], char *window_title)
{
   int          i,
                j,
                fd;
   unsigned int new_bar_length;
   char         *buffer,
                config_file[MAX_PATH_LENGTH],
                *perm_buffer,
                hostname[MAX_AFD_NAME_LENGTH],
                sys_log_fifo[MAX_PATH_LENGTH];
   struct tms   tmsdummy;
   struct stat  stat_buf;

   /*
    * Determine the working directory. If it is not specified
    * in the command line try read it from the environment else
    * just take the default.
    */
   if (get_afd_path(argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   /* Now lets see if user may use this program */
   switch(get_permissions(&perm_buffer))
   {
      case NONE     : /* User is not allowed to use this program */
                      {
                         char *user;

                         if ((user = getenv("LOGNAME")) != NULL)
                         {
                            (void)fprintf(stderr,
                                          "User %s is not permitted to use this program.\n",
                                          user);
                         }
                         else
                         {
                            (void)fprintf(stderr, "%s\n",
                                          PERMISSION_DENIED_STR);
                         }
                      }
                      exit(INCORRECT);

      case SUCCESS  : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
                      eval_permissions(perm_buffer);
                      free(perm_buffer);
                      break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
                      acp.afd_ctrl_list      = NULL;
                      acp.amg_ctrl           = YES; /* Start/Stop the AMG    */
                      acp.fd_ctrl            = YES; /* Start/Stop the FD     */
                      acp.rr_dc              = YES; /* Reread DIR_CONFIG     */
                      acp.rr_hc              = YES; /* Reread HOST_CONFIG    */
                      acp.startup_afd        = YES; /* Startup the AFD       */
                      acp.shutdown_afd       = YES; /* Shutdown the AFD      */
                      acp.ctrl_transfer      = YES; /* Start/Stop a transfer */
                      acp.ctrl_transfer_list = NULL;
                      acp.ctrl_queue         = YES; /* Start/Stop the queue  */
                      acp.ctrl_queue_list    = NULL;
                      acp.switch_host        = YES;
                      acp.switch_host_list   = NULL;
                      acp.disable            = YES;
                      acp.disable_list       = NULL;
                      acp.info               = YES;
                      acp.info_list          = NULL;
                      acp.debug              = YES;
                      acp.debug_list         = NULL;
                      acp.retry              = YES;
                      acp.retry_list         = NULL;
                      acp.show_slog          = YES; /* View the system log   */
                      acp.show_slog_list     = NULL;
                      acp.show_tlog          = YES; /* View the transfer log */
                      acp.show_tlog_list     = NULL;
                      acp.show_dlog          = YES; /* View the debug log    */
                      acp.show_dlog_list     = NULL;
                      acp.show_ilog          = YES; /* View the input log    */
                      acp.show_ilog_list     = NULL;
                      acp.show_olog          = YES; /* View the output log   */
                      acp.show_olog_list     = NULL;
                      acp.show_rlog          = YES; /* View the delete log   */
                      acp.show_rlog_list     = NULL;
                      acp.afd_load           = YES;
                      acp.afd_load_list      = NULL;
                      acp.view_jobs          = YES; /* View jobs             */
                      acp.view_jobs_list     = NULL;
                      acp.edit_hc            = YES; /* Edit Host Config      */
                      acp.edit_hc_list       = NULL;
                      acp.view_dc            = YES; /* View DIR_CONFIG entry */
                      acp.view_dc_list       = NULL;
                      break;

      default       : (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
                      exit(INCORRECT);
   }

   (void)strcpy(sys_log_fifo, p_work_dir);
   (void)strcat(sys_log_fifo, FIFO_DIR);
   (void)strcpy(afd_active_file, sys_log_fifo);
   (void)strcat(afd_active_file, AFD_ACTIVE_FILE);
   (void)strcat(sys_log_fifo, SYSTEM_LOG_FIFO);

   /* Create and open sys_log fifo. */
   if ((stat(sys_log_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(sys_log_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN, "Failed to create fifo %s. (%s %d)\n",
                   sys_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   if ((sys_log_fd = coe_open(sys_log_fifo, O_RDWR)) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Could not open fifo %s : %s (%s %d)\n",
                sys_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Prepare title for afd_ctrl window */
#ifdef PRE_RELEASE
   (void)sprintf(window_title, "AFD PRE %d.%d.%d-%d ",
                 MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
   (void)sprintf(window_title, "AFD %d.%d.%d ", MAJOR, MINOR, BUG_FIX);
#endif
   if (get_afd_name(hostname) == INCORRECT)
   {
      if (gethostname(hostname, MAX_AFD_NAME_LENGTH) == 0)
      {
         hostname[0] = toupper((int)hostname[0]);
         (void)strcat(window_title, hostname);
      }
   }
   else
   {
      (void)strcat(window_title, hostname);
   }

   get_user(user);

   /*
    * Attach to the FSA and get the number of host
    * and the fsa_id of the FSA.
    */
   if (fsa_attach() < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to FSA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Attach to the AFD Status Area
    */
   if (attach_afd_status() < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to attach to AFD status area. (%s %d)\n",
                    __FILE__, __LINE__);
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to attach to AFD status area. (%s %d)\n",
                __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Get display pointer */
   if ((display = XOpenDisplay(NULL)) == NULL)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open Display : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
   {
      (void)fprintf(stderr, "Could not get clock ticks per second.\n");
      exit(INCORRECT);
   }

   /*
    * Map to AFD_ACTIVE file, to check if all process are really
    * still alive.
    */
   if ((fd = open(afd_active_file, O_RDWR)) < 0)
   {
      pid_list = NULL;
   }
   else
   {
      if (fstat(fd, &stat_buf) < 0)
      {
         (void)fprintf(stderr, "WARNING : fstat() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         pid_list = NULL;
      }
      else
      {
#ifdef _NO_MMAP
         if ((pid_list = mmap_emu(0, stat_buf.st_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                                  afd_active_file, 0)) == (caddr_t) -1)
#else
         if ((pid_list = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                              fd, 0)) == (caddr_t) -1)
#endif
         {
            (void)fprintf(stderr, "WARNING : mmap() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            pid_list = NULL;
         }

         afd_active_size = stat_buf.st_size;
         afd_active_time = stat_buf.st_mtime;

         if (close(fd) == -1)
         {
            (void)fprintf(stderr, "WARNING : close() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
         }
      }
   }

   /* Allocate memory for local 'FSA' */
   if ((connect_data = calloc(no_of_hosts, sizeof(struct line))) == NULL)
   {
      (void)fprintf(stderr, "calloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Read setup file of this user.
    */
   line_style = CHARACTERS_AND_BARS;
   no_of_rows_set = DEFAULT_NO_OF_ROWS;
   filename_display_length = DEFAULT_FILENAME_DISPLAY_LENGTH;
   if ((*argc > 1) && (isdigit(argv[1][0])))
   {
      (void)strcpy(font_name, argv[1]);
   }
   else
   {
      (void)strcpy(font_name, DEFAULT_FONT);
   }
   read_setup("afd_ctrl");

   /* Determine the default bar length */
   max_bar_length  = 6 * BAR_LENGTH_MODIFIER;
   step_size = MAX_INTENSITY / max_bar_length;

   /* Initialise all display data for each host */
   for (i = 0; i < no_of_hosts; i++)
   {
      (void)strcpy(connect_data[i].hostname, fsa[i].host_alias);
      (void)sprintf(connect_data[i].host_display_str, "%-*s",
                    MAX_HOSTNAME_LENGTH, fsa[i].host_dsp_name);
      if (fsa[i].host_toggle_str[0] != '\0')
      {
         connect_data[i].host_toggle_display = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
      }
      else
      {
         connect_data[i].host_toggle_display = fsa[i].host_dsp_name[(int)fsa[i].toggle_pos];
      }
      connect_data[i].start_time = times(&tmsdummy);
      connect_data[i].total_file_counter = fsa[i].total_file_counter;
      CREATE_FC_STRING(connect_data[i].str_tfc, connect_data[i].total_file_counter);
      connect_data[i].debug = fsa[i].debug;
      if (fsa[i].special_flag & HOST_DISABLED)
      {
         connect_data[i].stat_color_no = WHITE;
      }
      else
      {
         connect_data[i].stat_color_no = NORMAL_STATUS;
      }
      if (fsa[i].host_status > 1) /* PAUSE_QUEUE_STAT AUTO_PAUSE_QUEUE_LOCK_STAT   */
                                  /* AUTO_PAUSE_QUEUE_STAT DANGER_PAUSE_QUEUE_STAT */
      {
         if (fsa[i].host_status & PAUSE_QUEUE_STAT)
         {
            connect_data[i].status_led[0] = PAUSE_QUEUE;
         }
         else
         {
            connect_data[i].status_led[0] = AUTO_PAUSE_QUEUE;
         }
      }
      else
      {
         connect_data[i].status_led[0] = NORMAL_STATUS;
      }
      if (fsa[i].host_status & STOP_TRANSFER_STAT)
      {
         connect_data[i].status_led[1] = STOP_TRANSFER;
      }
      else
      {
         connect_data[i].status_led[1] = NORMAL_STATUS;
      }
      connect_data[i].total_file_size = fsa[i].total_file_size;
      CREATE_FS_STRING(connect_data[i].str_tfs, connect_data[i].total_file_size);
      connect_data[i].bytes_per_sec = 0;
      (void)strcpy(connect_data[i].str_tr, "  0B");
      connect_data[i].average_tr = 0.0;
      connect_data[i].max_average_tr = 0.0;
      connect_data[i].max_errors = fsa[i].max_errors;
      connect_data[i].error_counter = fsa[i].error_counter;
      CREATE_EC_STRING(connect_data[i].str_ec, connect_data[i].error_counter);
      if (connect_data[i].max_errors < 1)
      {
         connect_data[i].scale = (double)max_bar_length;
      }
      else
      {
         connect_data[i].scale = max_bar_length / connect_data[i].max_errors;
      }
      if ((new_bar_length = (connect_data[i].error_counter * connect_data[i].scale)) > 0)
      {
         if (new_bar_length >= max_bar_length)
         {
            connect_data[i].bar_length[ERROR_BAR_NO] = max_bar_length;
            connect_data[i].red_color_offset = MAX_INTENSITY;
            connect_data[i].green_color_offset = 0;
         }
         else
         {
            connect_data[i].bar_length[ERROR_BAR_NO] = new_bar_length;
            connect_data[i].red_color_offset = new_bar_length * step_size;
            connect_data[i].green_color_offset = MAX_INTENSITY - connect_data[i].red_color_offset;
         }
      }
      else
      {
         connect_data[i].bar_length[ERROR_BAR_NO] = 0;
         connect_data[i].red_color_offset = 0;
         connect_data[i].green_color_offset = MAX_INTENSITY;
      }
      connect_data[i].bar_length[TR_BAR_NO] = 0;
      connect_data[i].inverse = OFF;
      connect_data[i].expose_flag = NO;
      connect_data[i].special_flag = fsa[i].special_flag | 191;
      connect_data[i].allowed_transfers = fsa[i].allowed_transfers;
      for (j = 0; j < connect_data[i].allowed_transfers; j++)
      {
         connect_data[i].no_of_files[j] = fsa[i].job_status[j].no_of_files - fsa[i].job_status[j].no_of_files_done;
         connect_data[i].bytes_send[j] = fsa[i].job_status[j].bytes_send;
         connect_data[i].connect_status[j] = fsa[i].job_status[j].connect_status;
         connect_data[i].detailed_selection[j] = NO;
      }
   }

   /*
    * Initialise all data for AFD status area.
    */
   prev_afd_status.amg = p_afd_status->amg;
   prev_afd_status.fd = p_afd_status->fd;
   prev_afd_status.archive_watch = p_afd_status->archive_watch;
   if ((prev_afd_status.fd == OFF) ||
       (prev_afd_status.amg == OFF) ||
       (prev_afd_status.archive_watch == OFF))
   {
      blink_flag = ON;
   }
   else
   {
      blink_flag = OFF;
   }
   prev_afd_status.sys_log = p_afd_status->sys_log;
   prev_afd_status.trans_log = p_afd_status->trans_log;
   prev_afd_status.trans_db_log = p_afd_status->trans_db_log;
   prev_afd_status.sys_log_ec = p_afd_status->sys_log_ec;
   memcpy(prev_afd_status.sys_log_fifo, p_afd_status->sys_log_fifo, LOG_FIFO_SIZE + 1);
   prev_afd_status.trans_log_ec = p_afd_status->trans_log_ec;
   memcpy(prev_afd_status.trans_log_fifo, p_afd_status->trans_log_fifo, LOG_FIFO_SIZE + 1);
   prev_afd_status.jobs_in_queue = p_afd_status->jobs_in_queue;

   log_angle = 360 / LOG_FIFO_SIZE;
   no_selected = no_selected_static = 0;
   redraw_time_host = redraw_time_status = STARTING_REDRAW_TIME;

   (void)sprintf(config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((access(config_file, F_OK) == 0) &&
       (read_file(config_file, &buffer) != INCORRECT))
   {
      int  str_length;
      char value[MAX_PATH_LENGTH];

      if (get_definition(buffer, PING_CMD_DEF,
                         value, MAX_PATH_LENGTH) != NULL)
      {
         str_length = strlen(value);
         if (str_length > 0)
         {
            if ((ping_cmd = malloc(str_length + 4 + MAX_REAL_HOSTNAME_LENGTH)) == NULL)
            {
               (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            ping_cmd[0] = '"';
            (void)strcpy(&ping_cmd[1], value);
            ping_cmd[str_length + 1] = ' ';
            ptr_ping_cmd = ping_cmd + str_length + 2;
         }
      }
      if (get_definition(buffer, TRACEROUTE_CMD_DEF,
                         value, MAX_PATH_LENGTH) != NULL)
      {
         str_length = strlen(value);
         if (str_length > 0)
         {
            if ((traceroute_cmd = malloc(str_length + 4 + MAX_REAL_HOSTNAME_LENGTH)) == NULL)
            {
               (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            traceroute_cmd[0] = '"';
            (void)strcpy(&traceroute_cmd[1], value);
            traceroute_cmd[str_length + 1] = ' ';
            ptr_traceroute_cmd = traceroute_cmd + str_length + 2;
         }
      }
      free(buffer);
   }

   return;
}


/*+++++++++++++++++++++++++++ init_menu_bar() +++++++++++++++++++++++++++*/
static void
init_menu_bar(Widget mainform_w, Widget *menu_w)
{
   Arg      args[MAXARGS];
   Cardinal argcount;
   Widget   host_pull_down_w,
            view_pull_down_w,
            control_pull_down_w,
            setup_pull_down_w,
#ifdef _WITH_HELP_PULLDOWN
            help_pull_down_w,
#endif
            pullright_font,
            pullright_load,
            pullright_row,
            pullright_test,
            pullright_line_style;

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,         XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNmarginHeight,    0);
   argcount++;
   XtSetArg(args[argcount], XmNmarginWidth,     0);
   argcount++;
   *menu_w = XmCreateSimpleMenuBar(mainform_w, "Menu Bar", args, argcount);

   /**********************************************************************/
   /*                           Host Menu                                */
   /**********************************************************************/
   host_pull_down_w = XmCreatePulldownMenu(*menu_w, "Host Pulldown", NULL, 0);
   XtVaSetValues(host_pull_down_w, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);
   mw[HOST_W] = XtVaCreateManagedWidget("Host",
                           xmCascadeButtonWidgetClass, *menu_w,
                           XmNfontList,                fontlist,
                           XmNmnemonic,                'o',
                           XmNsubMenuId,               host_pull_down_w,
                           NULL);

   if ((acp.ctrl_queue != NO_PERMISSION) ||
       (acp.ctrl_transfer != NO_PERMISSION) ||
       (acp.disable != NO_PERMISSION) ||
       (acp.switch_host != NO_PERMISSION) ||
       (acp.retry != NO_PERMISSION) ||
       (acp.debug != NO_PERMISSION) ||
       (acp.info != NO_PERMISSION) ||
       (acp.view_dc != NO_PERMISSION) ||
       (traceroute_cmd != NULL) ||
       (ping_cmd != NULL))
   {
      if (acp.ctrl_queue != NO_PERMISSION)
      {
         ow[QUEUE_W] = XtVaCreateManagedWidget("Start/Stop queue",
                                 xmPushButtonWidgetClass, host_pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(ow[QUEUE_W], XmNactivateCallback, popup_cb,
                       (XtPointer)QUEUE_SEL);
      }
      if (acp.ctrl_transfer != NO_PERMISSION)
      {
         ow[TRANSFER_W] = XtVaCreateManagedWidget("Start/Stop transfer",
                                 xmPushButtonWidgetClass, host_pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(ow[TRANSFER_W], XmNactivateCallback, popup_cb,
                       (XtPointer)TRANS_SEL);
      }
      if (acp.disable != NO_PERMISSION)
      {
         ow[DISABLE_W] = XtVaCreateManagedWidget("Enable/Disable host",
                                 xmPushButtonWidgetClass, host_pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(ow[DISABLE_W], XmNactivateCallback, popup_cb,
                       (XtPointer)DISABLE_SEL);
      }
      if (acp.switch_host != NO_PERMISSION)
      {
         ow[SWITCH_W] = XtVaCreateManagedWidget("Switch host",
                                 xmPushButtonWidgetClass, host_pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(ow[SWITCH_W], XmNactivateCallback, popup_cb,
                       (XtPointer)SWITCH_SEL);
      }
      if (acp.retry != NO_PERMISSION)
      {
         ow[RETRY_W] = XtVaCreateManagedWidget("Retry",
                                 xmPushButtonWidgetClass, host_pull_down_w,
                                 XmNfontList,             fontlist,
                                 XmNmnemonic,             'R',
                                 XmNaccelerator,          "Alt<Key>R",
                                 NULL);
         XtAddCallback(ow[RETRY_W], XmNactivateCallback, popup_cb,
                       (XtPointer)RETRY_SEL);
      }
      if (acp.debug != NO_PERMISSION)
      {
         ow[DEBUG_W] = XtVaCreateManagedWidget("Debug",
                                 xmPushButtonWidgetClass, host_pull_down_w,
                                 XmNfontList,             fontlist,
                                 XmNmnemonic,             'D',
                                 XmNaccelerator,          "Alt<Key>D",
                                 NULL);
         XtAddCallback(ow[DEBUG_W], XmNactivateCallback, popup_cb,
                       (XtPointer)DEBUG_SEL);
      }
      if ((traceroute_cmd != NULL) || (ping_cmd != NULL))
      {
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, host_pull_down_w,
                                 NULL);
         pullright_test = XmCreateSimplePulldownMenu(host_pull_down_w,
                                                     "pullright_test", NULL, 0);
         ow[TEST_W] = XtVaCreateManagedWidget("Test",
                                 xmCascadeButtonWidgetClass, host_pull_down_w,
                                 XmNfontList,                fontlist,
                                 XmNsubMenuId,               pullright_test,
                                 NULL);
         create_pullright_test(pullright_test);
      }
      if (((acp.ctrl_queue != NO_PERMISSION) ||
           (acp.ctrl_transfer != NO_PERMISSION) ||
           (acp.disable != NO_PERMISSION) ||
           (acp.switch_host != NO_PERMISSION) ||
           (acp.retry != NO_PERMISSION) ||
           (acp.debug != NO_PERMISSION) ||
           (traceroute_cmd != NULL) ||
           (ping_cmd != NULL)) &&
          ((acp.info != NO_PERMISSION) ||
           (acp.view_dc != NO_PERMISSION)))
      {
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, host_pull_down_w,
                                 NULL);
      }
      if (acp.info != NO_PERMISSION)
      {
         ow[INFO_W] = XtVaCreateManagedWidget("Info",
                                 xmPushButtonWidgetClass, host_pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(ow[INFO_W], XmNactivateCallback, popup_cb,
                       (XtPointer)INFO_SEL);
      }
      if (acp.view_dc != NO_PERMISSION)
      {
         ow[VIEW_DC_W] = XtVaCreateManagedWidget("Configuration",
                                 xmPushButtonWidgetClass, host_pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(ow[VIEW_DC_W], XmNactivateCallback, popup_cb,
                       (XtPointer)VIEW_DC_SEL);
      }
      XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, host_pull_down_w,
                              XmNseparatorType,       XmDOUBLE_LINE,
                              NULL);
   }
   ow[EXIT_W] = XtVaCreateManagedWidget("Exit",
                           xmPushButtonWidgetClass, host_pull_down_w,
                           XmNfontList,             fontlist,
                           XmNmnemonic,             'x',
                           XmNaccelerator,          "Alt<Key>x",
                           NULL);
   XtAddCallback(ow[EXIT_W], XmNactivateCallback, popup_cb,
                 (XtPointer)EXIT_SEL);

   /**********************************************************************/
   /*                           View Menu                                */
   /**********************************************************************/
   if ((acp.show_slog != NO_PERMISSION) || (acp.show_tlog != NO_PERMISSION) ||
       (acp.show_dlog != NO_PERMISSION) || (acp.show_ilog != NO_PERMISSION) ||
       (acp.show_olog != NO_PERMISSION) || (acp.show_rlog != NO_PERMISSION) ||
       (acp.view_jobs != NO_PERMISSION))
   {
      view_pull_down_w = XmCreatePulldownMenu(*menu_w,
                                              "View Pulldown", NULL, 0);
      XtVaSetValues(view_pull_down_w,
                    XmNtearOffModel, XmTEAR_OFF_ENABLED,
                    NULL);
      mw[LOG_W] = XtVaCreateManagedWidget("View",
                              xmCascadeButtonWidgetClass, *menu_w,
                              XmNfontList,                fontlist,
                              XmNmnemonic,                'V',
                              XmNsubMenuId,               view_pull_down_w,
                              NULL);
      if (acp.show_slog != NO_PERMISSION)
      {
         vw[SYSTEM_W] = XtVaCreateManagedWidget("System Log",
                                 xmPushButtonWidgetClass, view_pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(vw[SYSTEM_W], XmNactivateCallback, popup_cb,
                       (XtPointer)S_LOG_SEL);
      }
      if (acp.show_tlog != NO_PERMISSION)
      {
         vw[TRANS_W] = XtVaCreateManagedWidget("Transfer Log",
                                 xmPushButtonWidgetClass, view_pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(vw[TRANS_W], XmNactivateCallback, popup_cb,
                       (XtPointer)T_LOG_SEL);
      }
      if (acp.show_dlog != NO_PERMISSION)
      {
         vw[TRANS_DEBUG_W] = XtVaCreateManagedWidget("Transfer Debug Log",
                                 xmPushButtonWidgetClass, view_pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(vw[TRANS_DEBUG_W], XmNactivateCallback, popup_cb,
                       (XtPointer)D_LOG_SEL);
      }
      if ((acp.show_ilog != NO_PERMISSION) ||
          (acp.show_olog != NO_PERMISSION) ||
          (acp.show_rlog != NO_PERMISSION))
      {
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, view_pull_down_w,
                                 NULL);
         if (acp.show_ilog != NO_PERMISSION)
         {
            vw[INPUT_W] = XtVaCreateManagedWidget("Input Log",
                                    xmPushButtonWidgetClass, view_pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(vw[INPUT_W], XmNactivateCallback, popup_cb,
                          (XtPointer)I_LOG_SEL);
         }
         if (acp.show_olog != NO_PERMISSION)
         {
            vw[OUTPUT_W] = XtVaCreateManagedWidget("Output Log",
                                    xmPushButtonWidgetClass, view_pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(vw[OUTPUT_W], XmNactivateCallback, popup_cb,
                          (XtPointer)O_LOG_SEL);
         }
         if (acp.show_rlog != NO_PERMISSION)
         {
            vw[DELETE_W] = XtVaCreateManagedWidget("Delete Log",
                                    xmPushButtonWidgetClass, view_pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(vw[DELETE_W], XmNactivateCallback, popup_cb,
                          (XtPointer)R_LOG_SEL);
         }
      }
      if (acp.afd_load != NO_PERMISSION)
      {
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, view_pull_down_w,
                                 NULL);
         pullright_load = XmCreateSimplePulldownMenu(view_pull_down_w,
                                                     "pullright_load", NULL, 0);
         vw[VIEW_LOAD_W] = XtVaCreateManagedWidget("Load",
                                 xmCascadeButtonWidgetClass, view_pull_down_w,
                                 XmNfontList,                fontlist,
                                 XmNsubMenuId,               pullright_load,
                                 NULL);
         create_pullright_load(pullright_load);
      }
      if (acp.view_jobs != NO_PERMISSION)
      {
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, view_pull_down_w,
                                 NULL);
         vw[VIEW_JOB_W] = XtVaCreateManagedWidget("Job details",
                                 xmPushButtonWidgetClass, view_pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(vw[VIEW_JOB_W], XmNactivateCallback, popup_cb,
                       (XtPointer)VIEW_JOB_SEL);
      }
   }

   /**********************************************************************/
   /*                          Control Menu                              */
   /**********************************************************************/
   if ((acp.amg_ctrl != NO_PERMISSION) ||
       (acp.fd_ctrl != NO_PERMISSION) ||
       (acp.rr_dc != NO_PERMISSION) ||
       (acp.rr_hc != NO_PERMISSION) ||
       (acp.edit_hc != NO_PERMISSION) ||
       (acp.startup_afd != NO_PERMISSION) ||
       (acp.shutdown_afd != NO_PERMISSION))
   {
      control_pull_down_w = XmCreatePulldownMenu(*menu_w,
                                                 "Control Pulldown", NULL, 0);
      XtVaSetValues(control_pull_down_w,
                    XmNtearOffModel, XmTEAR_OFF_ENABLED,
                    NULL);
      mw[CONTROL_W] = XtVaCreateManagedWidget("Control",
                              xmCascadeButtonWidgetClass, *menu_w,
                              XmNfontList,                fontlist,
                              XmNmnemonic,                'C',
                              XmNsubMenuId,               control_pull_down_w,
                              NULL);
      if (acp.amg_ctrl != NO_PERMISSION)
      {
         cw[AMG_CTRL_W] = XtVaCreateManagedWidget("Start/Stop AMG",
                                 xmPushButtonWidgetClass, control_pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(cw[AMG_CTRL_W], XmNactivateCallback,
                       control_cb, (XtPointer)CONTROL_AMG_SEL);
      }
      if (acp.fd_ctrl != NO_PERMISSION)
      {
         cw[FD_CTRL_W] = XtVaCreateManagedWidget("Start/Stop FD",
                                 xmPushButtonWidgetClass, control_pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(cw[FD_CTRL_W], XmNactivateCallback,
                       control_cb, (XtPointer)CONTROL_FD_SEL);
      }
      if ((acp.rr_dc != NO_PERMISSION) || (acp.rr_hc != NO_PERMISSION))
      {
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, control_pull_down_w,
                                 NULL);
         if (acp.rr_dc != NO_PERMISSION)
         {
            cw[RR_DC_W] = XtVaCreateManagedWidget("Reread DIR_CONFIG",
                                    xmPushButtonWidgetClass, control_pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(cw[RR_DC_W], XmNactivateCallback,
                          control_cb, (XtPointer)REREAD_DIR_CONFIG_SEL);
         }
         if (acp.rr_hc != NO_PERMISSION)
         {
            cw[RR_HC_W] = XtVaCreateManagedWidget("Reread HOST_CONFIG",
                                    xmPushButtonWidgetClass, control_pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(cw[RR_HC_W], XmNactivateCallback,
                          control_cb, (XtPointer)REREAD_HOST_CONFIG_SEL);
         }
      }
      if (acp.edit_hc != NO_PERMISSION)
      {
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, control_pull_down_w,
                                 NULL);
         cw[EDIT_HC_W] = XtVaCreateManagedWidget("Edit HOST_CONFIG",
                                 xmPushButtonWidgetClass, control_pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(cw[EDIT_HC_W], XmNactivateCallback,
                       popup_cb, (XtPointer)EDIT_HC_SEL);
      }

      /* Startup/Shutdown of AFD */
      if ((acp.startup_afd != NO_PERMISSION) ||
          (acp.shutdown_afd != NO_PERMISSION))
      {
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, control_pull_down_w,
                                 NULL);
         if (acp.startup_afd != NO_PERMISSION)
         {
            cw[STARTUP_AFD_W] = XtVaCreateManagedWidget("Startup AFD",
                                    xmPushButtonWidgetClass, control_pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(cw[STARTUP_AFD_W], XmNactivateCallback,
                          control_cb, (XtPointer)STARTUP_AFD_SEL);
         }
         if (acp.shutdown_afd != NO_PERMISSION)
         {
            cw[SHUTDOWN_AFD_W] = XtVaCreateManagedWidget("Shutdown AFD",
                                    xmPushButtonWidgetClass, control_pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(cw[SHUTDOWN_AFD_W], XmNactivateCallback,
                          control_cb, (XtPointer)SHUTDOWN_AFD_SEL);
         }
      }
   }

   /**********************************************************************/
   /*                           Setup Menu                               */
   /**********************************************************************/
   setup_pull_down_w = XmCreatePulldownMenu(*menu_w, "Setup Pulldown", NULL, 0);
   XtVaSetValues(setup_pull_down_w, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);
   pullright_font = XmCreateSimplePulldownMenu(setup_pull_down_w,
                                               "pullright_font", NULL, 0);
   pullright_row = XmCreateSimplePulldownMenu(setup_pull_down_w,
                                              "pullright_row", NULL, 0);
   pullright_line_style = XmCreateSimplePulldownMenu(setup_pull_down_w,
                                              "pullright_line_style", NULL, 0);
   mw[CONFIG_W] = XtVaCreateManagedWidget("Setup",
                           xmCascadeButtonWidgetClass, *menu_w,
                           XmNfontList,                fontlist,
                           XmNmnemonic,                'S',
                           XmNsubMenuId,               setup_pull_down_w,
                           NULL);
   sw[FONT_W] = XtVaCreateManagedWidget("Font size",
                           xmCascadeButtonWidgetClass, setup_pull_down_w,
                           XmNfontList,                fontlist,
                           XmNsubMenuId,               pullright_font,
                           NULL);
   create_pullright_font(pullright_font);
   sw[ROWS_W] = XtVaCreateManagedWidget("Number of rows",
                           xmCascadeButtonWidgetClass, setup_pull_down_w,
                           XmNfontList,                fontlist,
                           XmNsubMenuId,               pullright_row,
                           NULL);
   create_pullright_row(pullright_row);
   sw[STYLE_W] = XtVaCreateManagedWidget("Line Style",
                           xmCascadeButtonWidgetClass, setup_pull_down_w,
                           XmNfontList,                fontlist,
                           XmNsubMenuId,               pullright_line_style,
                           NULL);
   create_pullright_style(pullright_line_style);
   XtVaCreateManagedWidget("Separator",
                           xmSeparatorWidgetClass, setup_pull_down_w,
                           NULL);
   sw[SAVE_W] = XtVaCreateManagedWidget("Save Setup",
                           xmPushButtonWidgetClass, setup_pull_down_w,
                           XmNfontList,             fontlist,
                           XmNmnemonic,             'a',
                           XmNaccelerator,          "Alt<Key>a",
                           NULL);
   XtAddCallback(sw[SAVE_W], XmNactivateCallback, save_setup_cb, (XtPointer)0);

#ifdef _WITH_HELP_PULLDOWN
   /**********************************************************************/
   /*                            Help Menu                               */
   /**********************************************************************/
   help_pull_down_w = XmCreatePulldownMenu(*menu_w, "Help Pulldown", NULL, 0);
   XtVaSetValues(help_pull_down_w, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);
   mw[HELP_W] = XtVaCreateManagedWidget("Help",
                           xmCascadeButtonWidgetClass, *menu_w,
                           XmNfontList,                fontlist,
                           XmNmnemonic,                'H',
                           XmNsubMenuId,               help_pull_down_w,
                           NULL);
   hw[ABOUT_W] = XtVaCreateManagedWidget("About AFD",
                           xmPushButtonWidgetClass, help_pull_down_w,
                           XmNfontList,             fontlist,
                           NULL);
   hw[HYPER_W] = XtVaCreateManagedWidget("Hyper Help",
                           xmPushButtonWidgetClass, help_pull_down_w,
                           XmNfontList,             fontlist,
                           NULL);
   hw[VERSION_W] = XtVaCreateManagedWidget("Version",
                           xmPushButtonWidgetClass, help_pull_down_w,
                           XmNfontList,             fontlist,
                           NULL);
#endif /* _WITH_HELP_PULLDOWN */

   XtManageChild(*menu_w);
   XtVaSetValues(*menu_w, XmNmenuHelpWidget, mw[HELP_W], NULL);

   return;
}


/*+++++++++++++++++++++++++ init_popup_menu() +++++++++++++++++++++++++++*/
static void
init_popup_menu(Widget line_window_w)
{
   XmString x_string;
   Widget   popupmenu,
            pushbutton;
   Arg      args[MAXARGS];
   Cardinal argcount;

   argcount = 0;
   XtSetArg(args[argcount], XmNtearOffModel, XmTEAR_OFF_ENABLED); argcount++;
   popupmenu = XmCreateSimplePopupMenu(line_window_w, "popup", args, argcount);

   if ((acp.ctrl_queue != NO_PERMISSION) ||
       (acp.ctrl_transfer != NO_PERMISSION) ||
       (acp.disable != NO_PERMISSION) ||
       (acp.switch_host != NO_PERMISSION) ||
       (acp.retry != NO_PERMISSION) ||
       (acp.debug != NO_PERMISSION) ||
       (acp.info != NO_PERMISSION) ||
       (acp.view_dc != NO_PERMISSION) ||
       (ping_cmd != NULL) ||
       (traceroute_cmd != NULL))
   {
      if (acp.ctrl_queue != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Start/Stop queue");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         pushbutton = XmCreatePushButton(popupmenu, "Queue", args, argcount);
         XtAddCallback(pushbutton, XmNactivateCallback,
                       popup_cb, (XtPointer)QUEUE_SEL);
         XtManageChild(pushbutton);
         XmStringFree(x_string);
      }
      if (acp.ctrl_transfer != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Start/Stop transfer");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         pushbutton = XmCreatePushButton(popupmenu, "Transfer", args, argcount);
         XtAddCallback(pushbutton, XmNactivateCallback,
                       popup_cb, (XtPointer)TRANS_SEL);
         XtManageChild(pushbutton);
         XmStringFree(x_string);
      }
      if (acp.disable != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Enable/Disable host");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         pushbutton = XmCreatePushButton(popupmenu, "Disable", args, argcount);
         XtAddCallback(pushbutton, XmNactivateCallback,
                       popup_cb, (XtPointer)DISABLE_SEL);
         XtManageChild(pushbutton);
         XmStringFree(x_string);
      }
      if (acp.switch_host != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Switch host");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         pushbutton = XmCreatePushButton(popupmenu, "Switch", args, argcount);
         XtAddCallback(pushbutton, XmNactivateCallback,
                       popup_cb, (XtPointer)SWITCH_SEL);
         XtManageChild(pushbutton);
         XmStringFree(x_string);
      }
      if (acp.retry != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Retry");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNaccelerator, "Alt<Key>R"); argcount++;
         XtSetArg(args[argcount], XmNmnemonic, 'R'); argcount++;
         pushbutton = XmCreatePushButton(popupmenu, "Retry", args, argcount);
         XtAddCallback(pushbutton, XmNactivateCallback,
                       popup_cb, (XtPointer)RETRY_SEL);
         XtManageChild(pushbutton);
         XmStringFree(x_string);
      }
      if (acp.debug != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Debug");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNaccelerator, "Alt<Key>D"); argcount++;
         XtSetArg(args[argcount], XmNmnemonic, 'D'); argcount++;
         pushbutton = XmCreatePushButton(popupmenu, "Debug", args, argcount);
         XtAddCallback(pushbutton, XmNactivateCallback,
                       popup_cb, (XtPointer)DEBUG_SEL);
         XtManageChild(pushbutton);
         XmStringFree(x_string);
      }
      if (acp.info != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Info");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNaccelerator, "Ctrl<Key>I"); argcount++;
         XtSetArg(args[argcount], XmNmnemonic, 'I'); argcount++;
         pushbutton = XmCreatePushButton(popupmenu, "Info", args, argcount);
         XtAddCallback(pushbutton, XmNactivateCallback,
                       popup_cb, (XtPointer)INFO_SEL);
         XtManageChild(pushbutton);
         XmStringFree(x_string);
      }
      if (acp.view_dc != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Configuration");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         pushbutton = XmCreatePushButton(popupmenu, "Configuration",
                                         args, argcount);
         XtAddCallback(pushbutton, XmNactivateCallback,
                       popup_cb, (XtPointer)VIEW_DC_SEL);
         XtManageChild(pushbutton);
         XmStringFree(x_string);
      }
   }

   XtAddEventHandler(line_window_w,
                     ButtonPressMask | ButtonReleaseMask |
                     Button1MotionMask,
                     False, (XtEventHandler)popup_menu_cb, popupmenu);

   return;
}


/*------------------------ create_pullright_test() ----------------------*/
static void
create_pullright_test(Widget pullright_test)
{
   XmString x_string;
   Arg      args[MAXARGS];
   Cardinal argcount;

   if (ping_cmd != NULL)
   {
      /* Create pullright for "Ping" */
      argcount = 0;
      x_string = XmStringCreateLocalized(SHOW_PING_TEST);
      XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
      tw[PING_W] = XmCreatePushButton(pullright_test, "Ping",
                                      args, argcount);
      XtAddCallback(tw[PING_W], XmNactivateCallback, popup_cb,
		    (XtPointer)PING_SEL);
      XtManageChild(tw[PING_W]);
      XmStringFree(x_string);
   }

   if (traceroute_cmd != NULL)
   {
      /* Create pullright for "Traceroute" */
      argcount = 0;
      x_string = XmStringCreateLocalized(SHOW_TRACEROUTE_TEST);
      XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
      tw[TRACEROUTE_W] = XmCreatePushButton(pullright_test, "Traceroute",
                                            args, argcount);
      XtAddCallback(tw[TRACEROUTE_W], XmNactivateCallback, popup_cb,
		    (XtPointer)TRACEROUTE_SEL);
      XtManageChild(tw[TRACEROUTE_W]);
      XmStringFree(x_string);
   }

   return;
}


/*------------------------ create_pullright_load() ----------------------*/
static void
create_pullright_load(Widget pullright_line_load)
{
   XmString x_string;
   Arg      args[MAXARGS];
   Cardinal argcount;

   /* Create pullright for "Files" */
   argcount = 0;
   x_string = XmStringCreateLocalized(SHOW_FILE_LOAD);
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   lw[FILE_LOAD_W] = XmCreatePushButton(pullright_line_load, "file",
				        args, argcount);
   XtAddCallback(lw[FILE_LOAD_W], XmNactivateCallback, popup_cb,
		 (XtPointer)VIEW_FILE_LOAD_SEL);
   XtManageChild(lw[FILE_LOAD_W]);
   XmStringFree(x_string);

   /* Create pullright for "KBytes" */
   argcount = 0;
   x_string = XmStringCreateLocalized(SHOW_KBYTE_LOAD);
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   lw[KBYTE_LOAD_W] = XmCreatePushButton(pullright_line_load, "kbytes",
				        args, argcount);
   XtAddCallback(lw[KBYTE_LOAD_W], XmNactivateCallback, popup_cb,
		 (XtPointer)VIEW_KBYTE_LOAD_SEL);
   XtManageChild(lw[KBYTE_LOAD_W]);
   XmStringFree(x_string);

   /* Create pullright for "Connections" */
   argcount = 0;
   x_string = XmStringCreateLocalized(SHOW_CONNECTION_LOAD);
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   lw[CONNECTION_LOAD_W] = XmCreatePushButton(pullright_line_load,
                                              "connection",
				              args, argcount);
   XtAddCallback(lw[CONNECTION_LOAD_W], XmNactivateCallback, popup_cb,
		 (XtPointer)VIEW_CONNECTION_LOAD_SEL);
   XtManageChild(lw[CONNECTION_LOAD_W]);
   XmStringFree(x_string);

   /* Create pullright for "Active-Transfers" */
   argcount = 0;
   x_string = XmStringCreateLocalized(SHOW_TRANSFER_LOAD);
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   lw[TRANSFER_LOAD_W] = XmCreatePushButton(pullright_line_load,
                                              "active-transfers",
				              args, argcount);
   XtAddCallback(lw[TRANSFER_LOAD_W], XmNactivateCallback, popup_cb,
		 (XtPointer)VIEW_TRANSFER_LOAD_SEL);
   XtManageChild(lw[TRANSFER_LOAD_W]);
   XmStringFree(x_string);

   return;
}


/*------------------------ create_pullright_font() ----------------------*/
static void
create_pullright_font(Widget pullright_font)
{
   int             i;
   char            *font[NO_OF_FONTS] =
                   {
                      FONT_0, FONT_1, FONT_2, FONT_3, FONT_4, FONT_5, FONT_6,
                      FONT_7, FONT_8, FONT_9, FONT_10, FONT_11, FONT_12
                   };
   XmString        x_string;
   XmFontListEntry entry;
   XmFontList      tmp_fontlist;
   Arg             args[MAXARGS];
   Cardinal        argcount;
   XFontStruct     *p_font_struct;

   for (i = 0; i < NO_OF_FONTS; i++)
   {
      if ((current_font == -1) && (strcmp(font_name, font[i]) == 0))
      {
         current_font = i;
      }
      if ((p_font_struct = XLoadQueryFont(display, font[i])) != NULL)
      {
         if ((entry = XmFontListEntryLoad(display, font[i], XmFONT_IS_FONT, "TAG1")) == NULL)
         {
            (void)fprintf(stderr, "Failed to load font with XmFontListEntryLoad() : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         tmp_fontlist = XmFontListAppendEntry(NULL, entry);
         XmFontListEntryFree(&entry);

         argcount = 0;
         x_string = XmStringCreateLocalized(font[i]);
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
         XtSetArg(args[argcount], XmNfontList, tmp_fontlist); argcount++;
         fw[i] = XmCreateToggleButton(pullright_font, "font_x", args, argcount);
         XtAddCallback(fw[i], XmNvalueChangedCallback, change_font_cb, (XtPointer)i);
         XtManageChild(fw[i]);
         XmFontListFree(tmp_fontlist);
         XmStringFree(x_string);
         XFreeFont(display, p_font_struct);
      }
   }

   return;
}


/*------------------------ create_pullright_row() -----------------------*/
static void
create_pullright_row(Widget pullright_row)
{
   int      i;
   char     *row[NO_OF_ROWS] =
            {
               ROW_0, ROW_1, ROW_2, ROW_3, ROW_4, ROW_5, ROW_6,
               ROW_7, ROW_8, ROW_9, ROW_10, ROW_11, ROW_12
            };
   XmString x_string;
   Arg      args[MAXARGS];
   Cardinal argcount;

   for (i = 0; i < NO_OF_FONTS; i++)
   {
      if ((current_row == -1) && (no_of_rows_set == atoi(row[i])))
      {
         current_row = i;
      }
      argcount = 0;
      x_string = XmStringCreateLocalized(row[i]);
      XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
      XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
      rw[i] = XmCreateToggleButton(pullright_row, "row_x", args, argcount);
      XtAddCallback(rw[i], XmNvalueChangedCallback, change_rows_cb, (XtPointer)i);
      XtManageChild(rw[i]);
      XmStringFree(x_string);
   }

   return;
}


/*------------------------ create_pullright_style() ---------------------*/
static void
create_pullright_style(Widget pullright_line_style)
{
   XmString x_string;
   Arg      args[MAXARGS];
   Cardinal argcount;

   /* Create pullright for "Line style" */
   argcount = 0;
   x_string = XmStringCreateLocalized("Bars only");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
   lsw[STYLE_0_W] = XmCreateToggleButton(pullright_line_style, "style_0",
				   args, argcount);
   XtAddCallback(lsw[STYLE_0_W], XmNvalueChangedCallback, change_style_cb,
		 (XtPointer)0);
   XtManageChild(lsw[STYLE_0_W]);
   current_style = line_style;
   XmStringFree(x_string);

   argcount = 0;
   x_string = XmStringCreateLocalized("Characters only");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
   lsw[STYLE_1_W] = XmCreateToggleButton(pullright_line_style, "style_1",
				   args, argcount);
   XtAddCallback(lsw[STYLE_1_W], XmNvalueChangedCallback, change_style_cb,
		 (XtPointer)1);
   XtManageChild(lsw[STYLE_1_W]);
   XmStringFree(x_string);

   argcount = 0;
   x_string = XmStringCreateLocalized("Characters and bars");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
   lsw[STYLE_2_W] = XmCreateToggleButton(pullright_line_style, "style_2",
				   args, argcount);
   XtAddCallback(lsw[STYLE_2_W], XmNvalueChangedCallback, change_style_cb,
		 (XtPointer)2);
   XtManageChild(lsw[STYLE_2_W]);
   XmStringFree(x_string);

   return;
}


/*-------------------------- eval_permissions() -------------------------*/
/*                           ------------------                          */
/* Description: Checks the permissions on what the user may do.          */
/* Returns    : Fills the global structure acp with data.                */
/*-----------------------------------------------------------------------*/
static void
eval_permissions(char *perm_buffer)
{
   char *ptr;

   /*
    * If we find 'all' right at the beginning, no further evaluation
    * is needed, since the user has all permissions.
    */
   if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
       (perm_buffer[2] == 'l') && ((perm_buffer[3] == '\0') ||
       (perm_buffer[3] == ' ') || (perm_buffer[3] == '\t')))
   {
      acp.afd_ctrl_list      = NULL;
      acp.amg_ctrl           = YES;   /* Start/Stop the AMG    */
      acp.fd_ctrl            = YES;   /* Start/Stop the FD     */
      acp.rr_dc              = YES;   /* Reread DIR_CONFIG     */
      acp.rr_hc              = YES;   /* Reread HOST_CONFIG    */
      acp.startup_afd        = YES;   /* Startup AFD           */
      acp.shutdown_afd       = YES;   /* Shutdown AFD          */
      acp.ctrl_transfer      = YES;   /* Start/Stop a transfer */
      acp.ctrl_transfer_list = NULL;
      acp.ctrl_queue         = YES;   /* Start/Stop the queue  */
      acp.ctrl_queue_list    = NULL;
      acp.switch_host        = YES;
      acp.switch_host_list   = NULL;
      acp.disable            = YES;
      acp.disable_list       = NULL;
      acp.info               = YES;
      acp.info_list          = NULL;
      acp.debug              = YES;
      acp.debug_list         = NULL;
      acp.retry              = YES;
      acp.retry_list         = NULL;
      acp.show_slog          = YES;   /* View the system log   */
      acp.show_slog_list     = NULL;
      acp.show_tlog          = YES;   /* View the transfer log */
      acp.show_tlog_list     = NULL;
      acp.show_dlog          = YES;   /* View the debug log    */
      acp.show_dlog_list     = NULL;
      acp.show_ilog          = YES;   /* View the input log    */
      acp.show_ilog_list     = NULL;
      acp.show_olog          = YES;   /* View the output log   */
      acp.show_olog_list     = NULL;
      acp.show_rlog          = YES;   /* View the delete log   */
      acp.show_rlog_list     = NULL;
      acp.view_jobs          = YES;   /* View jobs             */
      acp.view_jobs_list     = NULL;
      acp.edit_hc            = YES;   /* Edit Host Configuration */
      acp.edit_hc_list       = NULL;
      acp.view_dc            = YES;   /* View DIR_CONFIG entries */
      acp.view_dc_list       = NULL;
   }
   else
   {
      /*
       * First of all check if the user may use this program
       * at all.
       */
      if ((ptr = posi(perm_buffer, AFD_CTRL_PERM)) == NULL)
      {
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         free(perm_buffer);
         exit(INCORRECT);
      }
      else
      {
         /*
          * For future use. Allow to limit for host names
          * as well.
          */
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            store_host_names(acp.afd_ctrl_list, ptr + 1);
         }
      }

      /* May the user start/stop the AMG? */
      if ((ptr = posi(perm_buffer, AMG_CTRL_PERM)) == NULL)
      {
         /* The user may NOT do any resending. */
         acp.amg_ctrl = NO_PERMISSION;
      }
      else
      {
         acp.amg_ctrl = NO_LIMIT;
      }

      /* May the user start/stop the FD? */
      if ((ptr = posi(perm_buffer, FD_CTRL_PERM)) == NULL)
      {
         /* The user may NOT do any resending. */
         acp.fd_ctrl = NO_PERMISSION;
      }
      else
      {
         acp.fd_ctrl = NO_LIMIT;
      }

      /* May the user reread the DIR_CONFIG? */
      if ((ptr = posi(perm_buffer, RR_DC_PERM)) == NULL)
      {
         /* The user may NOT do reread the DIR_CONFIG. */
         acp.rr_dc = NO_PERMISSION;
      }
      else
      {
         acp.rr_dc = NO_LIMIT;
      }

      /* May the user reread the HOST_CONFIG? */
      if ((ptr = posi(perm_buffer, RR_HC_PERM)) == NULL)
      {
         /* The user may NOT do reread the HOST_CONFIG. */
         acp.rr_hc = NO_PERMISSION;
      }
      else
      {
         acp.rr_hc = NO_LIMIT;
      }

      /* May the user startup the AFD? */
      if ((ptr = posi(perm_buffer, STARTUP_PERM)) == NULL)
      {
         /* The user may NOT do a startup the AFD. */
         acp.startup_afd = NO_PERMISSION;
      }
      else
      {
         acp.startup_afd = NO_LIMIT;
      }

      /* May the user shutdown the AFD? */
      if ((ptr = posi(perm_buffer, SHUTDOWN_PERM)) == NULL)
      {
         /* The user may NOT do a shutdown the AFD. */
         acp.shutdown_afd = NO_PERMISSION;
      }
      else
      {
         acp.shutdown_afd = NO_LIMIT;
      }

      /* May the user start/stop the queue? */
      if ((ptr = posi(perm_buffer, CTRL_QUEUE_PERM)) == NULL)
      {
         /* The user may NOT start/stop the queue. */
         acp.ctrl_queue = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.ctrl_queue = store_host_names(acp.ctrl_queue_list, ptr + 1);
         }
         else
         {
            acp.ctrl_queue = NO_LIMIT;
         }
      }

      /* May the user start/stop the transfer? */
      if ((ptr = posi(perm_buffer, CTRL_TRANSFER_PERM)) == NULL)
      {
         /* The user may NOT start/stop the transfer. */
         acp.ctrl_transfer = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.ctrl_transfer = store_host_names(acp.ctrl_transfer_list, ptr + 1);
         }
         else
         {
            acp.ctrl_transfer = NO_LIMIT;
         }
      }

      /* May the user do a host switch? */
      if ((ptr = posi(perm_buffer, SWITCH_HOST_PERM)) == NULL)
      {
         /* The user may NOT do a host switch. */
         acp.switch_host = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.switch_host = store_host_names(acp.switch_host_list, ptr + 1);
         }
         else
         {
            acp.switch_host = NO_LIMIT;
         }
      }

      /* May the user disable a host? */
      if ((ptr = posi(perm_buffer, DISABLE_HOST_PERM)) == NULL)
      {
         /* The user may NOT disable a host. */
         acp.disable = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.disable = store_host_names(acp.disable_list, ptr + 1);
         }
         else
         {
            acp.disable = NO_LIMIT;
         }
      }

      /* May the user view the information of a host? */
      if ((ptr = posi(perm_buffer, INFO_PERM)) == NULL)
      {
         /* The user may NOT view any information. */
         acp.info = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.info = store_host_names(acp.info_list, ptr + 1);
         }
         else
         {
            acp.info = NO_LIMIT;
         }
      }

      /* May the user toggle the debug flag? */
      if ((ptr = posi(perm_buffer, DEBUG_PERM)) == NULL)
      {
         /* The user may NOT toggle the debug flag. */
         acp.debug = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.debug = store_host_names(acp.debug_list, ptr + 1);
         }
         else
         {
            acp.debug = NO_LIMIT;
         }
      }

      /* May the user use the retry button for a particular host? */
      if ((ptr = posi(perm_buffer, RETRY_PERM)) == NULL)
      {
         /* The user may NOT use the retry button. */
         acp.retry = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.retry = store_host_names(acp.retry_list, ptr + 1);
         }
         else
         {
            acp.retry = NO_LIMIT;
         }
      }

      /* May the user view the system log? */
      if ((ptr = posi(perm_buffer, SHOW_SLOG_PERM)) == NULL)
      {
         /* The user may NOT view the system log. */
         acp.show_slog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_slog = store_host_names(acp.show_slog_list, ptr + 1);
         }
         else
         {
            acp.show_slog = NO_LIMIT;
         }
      }

      /* May the user view the transfer log? */
      if ((ptr = posi(perm_buffer, SHOW_TLOG_PERM)) == NULL)
      {
         /* The user may NOT view the transfer log. */
         acp.show_tlog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_tlog = store_host_names(acp.show_tlog_list, ptr + 1);
         }
         else
         {
            acp.show_tlog = NO_LIMIT;
         }
      }

      /* May the user view the debug log? */
      if ((ptr = posi(perm_buffer, SHOW_DLOG_PERM)) == NULL)
      {
         /* The user may NOT view the debug log. */
         acp.show_dlog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_dlog = store_host_names(acp.show_dlog_list, ptr + 1);
         }
         else
         {
            acp.show_dlog = NO_LIMIT;
         }
      }

      /* May the user view the input log? */
      if ((ptr = posi(perm_buffer, SHOW_ILOG_PERM)) == NULL)
      {
         /* The user may NOT view the input log. */
         acp.show_ilog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_ilog = store_host_names(acp.show_ilog_list, ptr + 1);
         }
         else
         {
            acp.show_ilog = NO_LIMIT;
         }
      }

      /* May the user view the output log? */
      if ((ptr = posi(perm_buffer, SHOW_OLOG_PERM)) == NULL)
      {
         /* The user may NOT view the output log. */
         acp.show_olog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_olog = store_host_names(acp.show_olog_list, ptr + 1);
         }
         else
         {
            acp.show_olog = NO_LIMIT;
         }
      }

      /* May the user view the delete log? */
      if ((ptr = posi(perm_buffer, SHOW_RLOG_PERM)) == NULL)
      {
         /* The user may NOT view the delete log. */
         acp.show_rlog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_rlog = store_host_names(acp.show_rlog_list, ptr + 1);
         }
         else
         {
            acp.show_rlog = NO_LIMIT;
         }
      }

      /* May the user view the job details? */
      if ((ptr = posi(perm_buffer, VIEW_JOBS_PERM)) == NULL)
      {
         /* The user may NOT view the job details. */
         acp.view_jobs = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.view_jobs = store_host_names(acp.view_jobs_list, ptr + 1);
         }
         else
         {
            acp.view_jobs = NO_LIMIT;
         }
      }

      /* May the user edit the host configuration file? */
      if ((ptr = posi(perm_buffer, EDIT_HC_PERM)) == NULL)
      {
         /* The user may NOT edit the host configuration file. */
         acp.edit_hc = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.edit_hc = store_host_names(acp.edit_hc_list, ptr + 1);
         }
         else
         {
            acp.edit_hc = NO_LIMIT;
         }
      }

      /* May the user view the DIR_CONFIG file? */
      if ((ptr = posi(perm_buffer, VIEW_DIR_CONFIG_PERM)) == NULL)
      {
         /* The user may NOT view the DIR_CONFIG file. */
         acp.view_dc = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.view_dc = store_host_names(acp.view_dc_list, ptr + 1);
         }
         else
         {
            acp.view_dc = NO_LIMIT;
         }
      }
   }

   return;
}


/*+++++++++++++++++++++++++++ afd_ctrl_exit() +++++++++++++++++++++++++++*/
static void
afd_ctrl_exit(void)
{
   int i;

   for (i = 0; i < no_of_active_process; i++)
   {
      if (kill(apps_list[i].pid, SIGINT) < 0)
      {
         (void)xrec(appshell, WARN_DIALOG,
                    "Failed to kill() process %s (%d) : %s",
                    apps_list[i].progname,
                    apps_list[i].pid, strerror(errno));
      }
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void                                                                
sig_segv(int signo)
{
   afd_ctrl_exit();
   (void)fprintf(stderr, "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
                 __FILE__, __LINE__);
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void                                                                
sig_bus(int signo)
{
   afd_ctrl_exit();
   (void)fprintf(stderr, "Uuurrrggh! Received SIGBUS. (%s %d)\n",
                 __FILE__, __LINE__);
   abort();
}

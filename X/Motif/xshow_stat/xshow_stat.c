/*
 *  xshow_stat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   xshow_stat - shows output statistics of the AFD
 **
 ** SYNOPSIS
 **   xshow_stat [--version]
 **                  OR
 **              [-w <AFD working directory>] [-f font name] [host name 1..n]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.02.1999 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "xshow_stat.h"
#include "statdefs.h"
#include "permission.h"
#include "version.h"

#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/Form.h>
#include <Xm/Separator.h>
#include <Xm/DrawingA.h>
#include <Xm/CascadeB.h>
#include <Xm/PushB.h>


/* Global variables. */
Display        *display;
XtAppContext   app;
Colormap       default_cmap;
XFontStruct    *font_struct;
XmFontList     fontlist = NULL;
Widget         toplevel_w,
               stat_window_w;
Window         stat_window;
GC             letter_gc,
               normal_letter_gc,
               color_letter_gc,
               default_bg_gc,
               normal_bg_gc,
               button_bg_gc,
               color_gc,
               black_line_gc,
               white_line_gc;
char           font_name[20],
               **hosts,
               *p_work_dir,
               **x_data_point = NULL;
int            data_height,
               data_length,
               first_data_pos,
               host_counter,
               no_of_chars,
               no_of_x_data_points,
               no_of_y_data_points,
               no_of_hosts,
               *stat_pos = NULL,
               stat_type,
               sys_log_fd,
               time_type,
               window_height = 0,
               window_width = 0,
               x_data_spacing,
               x_offset_left_xaxis,
               x_offset_right_xaxis,
               y_data_spacing,
               y_offset_top_yaxis,
               y_offset_bottom_yaxis,
               y_offset_xaxis;
unsigned int   glyph_height,
               glyph_width;
unsigned long  color_pool[COLOR_POOL_SIZE];
struct afdstat *stat_db;

/* Local function prototypes */
static void    init_show_stat(int *, char **, char *, char *),
               sig_bus(int),
               sig_exit(int),
               sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char          window_title[100],
                 work_dir[MAX_PATH_LENGTH];
   static String fallback_res[] =
                 {
                    ".xshow_stat*background : NavajoWhite2",
                    ".xshow_stat.mainform.buttonbox*background : PaleVioletRed2",
                    ".xshow_stat.mainform.buttonbox*foreground : Black",
                    ".xshow_stat.mainform.buttonbox*highlightColor : Black",
                    NULL
                 };
   Widget        button_w,
                 buttonbox_w,
                 mainform_w,
                 separator_w;
   Arg           args[MAXARGS];
   Cardinal      argcount;

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values */
   p_work_dir = work_dir;
   init_show_stat(&argc, argv, font_name, window_title);

   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   toplevel_w = XtAppInitialize(&app, "AFD", NULL, 0, &argc, argv,
                                fallback_res, args, argcount);

   /* Get display pointer */
   if ((display = XtDisplay(toplevel_w)) == NULL)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open Display : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

#ifdef _X_DEBUG
   XSynchronize(display, 1);
#endif

   /* Setup and determine window parameters. */
   setup_window(font_name);

   /* Get window size. */
   (void)window_size(&window_width, &window_height, 0, 0);

   /* Create managing widget. */
   mainform_w = XmCreateForm(toplevel_w, "mainform", NULL, 0);

   /* Setup colors */
   default_cmap = DefaultColormap(display, DefaultScreen(display));
   init_color(XtDisplay(toplevel_w));

   /*--------------------------------------------------------------------*/
   /*                            Button Box                              */
   /*--------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   buttonbox_w = XmCreateForm(mainform_w, "buttonbox", args, argcount);
   button_w = XtVaCreateManagedWidget("Close",
                                     xmPushButtonWidgetClass, buttonbox_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_FORM,
                                     XmNleftAttachment,   XmATTACH_FORM,
                                     XmNrightAttachment,  XmATTACH_FORM,
                                     XmNbottomAttachment, XmATTACH_FORM,
                                     NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)close_button, 0);
   XtManageChild(buttonbox_w);

   /*--------------------------------------------------------------------*/
   /*                        Horizontal Separator                        */
   /*--------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     buttonbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(mainform_w, "separator", args, argcount);
   XtManageChild(separator_w);

   /*--------------------------------------------------------------------*/
   /*                            Drawing Area                            */
   /*--------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNheight, (Dimension) window_height);
   argcount++;
   XtSetArg(args[argcount], XmNwidth, (Dimension) window_width);
   argcount++;
   XtSetArg(args[argcount], XmNbackground, color_pool[DEFAULT_BG]);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget, separator_w);
   argcount++;
   stat_window_w = XmCreateDrawingArea(mainform_w, "stat_window_w", args,
                                       argcount);
   XtManageChild(stat_window_w);
   XtAddCallback(stat_window_w, XmNexposeCallback,
                 (XtCallbackProc)expose_handler_stat, NULL);
   XtManageChild(mainform_w);

   /* Initialise the GC's */
   init_gcs();

#ifdef _EDITRES
   XtAddEventHandler(toplevel_w, (EventMask)0, True, _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets */
   XtRealizeWidget(toplevel_w);

   /* Set some signal handlers. */
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(toplevel_w, WARN_DIALOG,
                 "Failed to set signal handlers for xshow_stat : %s",
                 strerror(errno));
   }

   /* Get window ID of the drawing widget. */
   stat_window = XtWindow(stat_window_w);

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++ init_show_stat() +++++++++++++++++++++++++++*/
static void
init_show_stat(int *argc, char *argv[], char *font_name, char *window_title)
{
   int         stat_fd;
   time_t      now;
   char        *perm_buffer,
               hostname[MAX_AFD_NAME_LENGTH],
               statistic_file[MAX_PATH_LENGTH];
   struct tm   *p_ts;
   struct stat stat_buf;

   /* See if user wants some help. */
   if ((get_arg(argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      (void)fprintf(stdout,
                    "Usage: %s [-w <work_dir>] [-f <numeric font name>] [-[CDFHKY]]\n",
                    argv[0]);
      (void)fprintf(stdout, "       -C  View number of network connections.\n");
      (void)fprintf(stdout, "       -D  Day statistics.\n");
      (void)fprintf(stdout, "       -F  View number of files transmitted.\n");
      (void)fprintf(stdout, "       -H  Hour statistics.\n");
      (void)fprintf(stdout, "       -K  View number of bytes transmitted.\n");
      (void)fprintf(stdout, "       -Y  Your statistics.\n");
      exit(SUCCESS);
   }

   if (get_afd_path(argc, argv, p_work_dir) < 0)
   {
      (void)fprintf(stderr,
                    "Failed to get working directory of AFD. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * If not set, set some default values.
    */
   stat_type = SHOW_FILE_STAT;
   time_type = DAY_STAT;
   if (get_arg(argc, argv, "-f", font_name, 20) == INCORRECT)
   {
      (void)strcpy(font_name, DEFAULT_FONT);
   }
   if (get_arg(argc, argv, "-K", NULL, 0) == SUCCESS)
   {
      stat_type = SHOW_KBYTE_STAT;
   }
   if (get_arg(argc, argv, "-F", NULL, 0) == SUCCESS)
   {
      stat_type = SHOW_FILE_STAT;
   }
   if (get_arg(argc, argv, "-C", NULL, 0) == SUCCESS)
   {
      stat_type = SHOW_CONNECT_STAT;
   }
   if (get_arg(argc, argv, "-H", NULL, 0) == SUCCESS)
   {
      time_type = HOUR_STAT;
   }
   if (get_arg(argc, argv, "-D", NULL, 0) == SUCCESS)
   {
      time_type = DAY_STAT;
   }
   if (get_arg(argc, argv, "-Y", NULL, 0) == SUCCESS)
   {
      time_type = YEAR_STAT;
   }

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
               (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
            }
         }
         exit(INCORRECT);

      case SUCCESS  : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
         if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
             (perm_buffer[2] == 'l') &&
             ((perm_buffer[3] == '\0') || (perm_buffer[3] == ' ') ||
              (perm_buffer[3] == '\t')))
         {
            free(perm_buffer);
            break;
         }
         else if (posi(perm_buffer, XSHOW_STAT_PERM) == NULL)
              {
                 (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
                 exit(INCORRECT);
              }
         free(perm_buffer);
         break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
         break;

      default       :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   /* Map to statistic file. */
   now = time(NULL);
   p_ts = gmtime(&now);
   (void)sprintf(statistic_file, "%s%s%s.%d", p_work_dir,
                 LOG_DIR, STATISTIC_FILE, p_ts->tm_year + 1900);
   if ((stat_fd = open(statistic_file, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, "ERROR   : Failed to open() %s : %s (%s %d)\n",
                    statistic_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (fstat(stat_fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr, "ERROR   : Failed to fstat() %s : %s (%s %d)\n",
                    statistic_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((stat_db = (struct afdstat *)mmap(0, stat_buf.st_size, PROT_READ,
                                         (MAP_FILE | MAP_SHARED), stat_fd,
                                         0)) == (struct afdstat *) -1)
   {
      (void)fprintf(stderr, "ERROR   : Failed to mmap() %s : %s (%s %d)\n",
                    statistic_file, strerror(errno), __FILE__, __LINE__);
      (void)close(stat_fd);
      exit(INCORRECT);
   }
   no_of_hosts = stat_buf.st_size / sizeof(struct afdstat);
   if (close(stat_fd) == -1)
   {
      (void)fprintf(stderr, "WARN    : Failed to close() %s : %s (%s %d)\n",
                    statistic_file, strerror(errno), __FILE__, __LINE__);
   }

   /* Collect all hostnames */
   host_counter = *argc - 1;
   if (host_counter > 0)
   {
      register int i = 0,
                   j;

      RT_ARRAY(hosts, host_counter, (MAX_RECIPIENT_LENGTH + 1), char);
      while (*argc > 1)
      {
         (void)strcpy(hosts[i], argv[1]);
         (*argc)--; argv++;
         i++;
      }
      RT_ARRAY(stat_pos, host_counter, sizeof(int), int);
      for (i = 0; i < host_counter; i++)
      {
         for (j = 0; j < no_of_hosts; j++)
         {
            if (strcmp(host[i], stat_db[j].hostname) == 0)
            {
               stat_pos[i] = j;
               break;
            }
         }
      }
   }
   else
   {
      register int i;

      RT_ARRAY(stat_pos, no_of_hosts, sizeof(int), int);
      for (i = 0; i < no_of_hosts; i++)
      {
         stat_pos[i] = i;
      }
   }

   /* Prepare title of this window. */
   (void)strcpy(window_title, "Statistics ");
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

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   (void)fprintf(stderr, "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
                 __FILE__, __LINE__);
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   (void)fprintf(stderr, "Uuurrrggh! Received SIGBUS. (%s %d)\n",
                 __FILE__, __LINE__);
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}

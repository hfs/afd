/*
 *  xshow_stat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "statdefs.h"
#include "version.h"


/* Global variables. */
Display        *display;
XtAppContext   app;
Colormap       default_cmap;
XFontStruct    *font_struct;
XmFontList     fontlist = NULL;
Widget         mw[1],                    /* Main menu */
               ow[1],                    /* Host menu */
               appshell,
               stat_window_w;
char           font_name[20],
               **hosts,
               *p_work_dir,
               stat_type;
unsigned long  color_pool[COLOR_POOL_SIZE];
struct afdstat *stat_db;

/* Local function prototypes */
static void    init_afd_stat(int, char **, char *, char *),
               init_menu_bar(Widget, Widget *),
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
                    ".xshow_stat*background : NavajoWhite2".
                    NULL
                 };
   Widget        mainform_w,
                 menu_w;
   Arg           args[MAXARGS];
   Cardinal      argcount;

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values */
   p_work_dir = work_dir;
   init_afd_stat(&argc, argv, font_name, window_title);

#ifdef _X_DEBUG
   XSynchronize(display, 1);
#endif

   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   appshell = XtAppInitialize(&app, "AFD", NULL, 0, &argc, argv,
                              fallback_res, args, argcount);

   mainwindow = XtVaCreateManagedWidget("Main_window",
                                        xmMainWindowWidgetClass, appshell,
                                        NULL);

   /* Setup and determine window parameters. */
   setup_window(font_name);

   /* Get window size. */
   (void)window_size(&window_width, &window_height);

   /* Create managing widget. */
   mainform_w = XmCreateForm(appshell, "mainform_w", NULL, 0);
   XtManageChild(mainform_w);

   /* Initialize the menu bar. */
   init_menu_bar(mainform_w, &menu_w);

   /* Setup colors */
   default_cmap = DefaultColormap(display, DefaultScreen(display));
   init_color(XtDisplay(appshell));

   /* Create drawing area for statistics. */
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
   stat_window_w = XmCreateDrawingArea(mainform_w, "stat_window_w", args,
                                       argcount);
   XtManageChild(stat_window_w);

   /* Initialise the GC's */
   init_gcs();

   /* Add call back to handle expose events for the stat window */
   XtAddCallback(stat_window_w, XmNexposeCallback,
                 (XtCallbackProc)expose_handler_stat, NULL);

#ifdef _EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True, _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets */
   XtRealizeWidget(appshell);

   /* Set some signal handlers. */
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(appshell, WARN_DIALOG,
                 "Failed to set signal handlers for xshow_stat : %s",
                 strerror(errno));
   }

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++ init_show_stat() +++++++++++++++++++++++++++*/
static void
init_show_stat(int argc, char *argv[], char *font_name, char *window_title)
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
                    "Usage: %s [-w <work_dir>] [-f <numeric font name>]\n",
                    argv[0]);
      exit(SUCCESS);
   }

   if (get_afd_path(&argc, argv, p_work_dir) < 0)
   {
      (void)fprintf(stderr,
                    "Failed to get working directory of AFD. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (get_arg(argc, argv, "-f", font_name, 20) == INCORRECT)
   {
      (void)strcpy(font_name, DEFAULT_FONT);
   }
   if (get_arg(argc, argv, "-K", NULL, 0) == INCORRECT)
   {
      stat_type = SHOW_FILE_STAT;
   }
   else
   {
      stat_type = SHOW_KBYTE_STAT;
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

   /* Collect all hostnames */
   no_of_hosts = argc - 1;
   if (no_of_hosts > 0)
   {
      register int i = 0;

      RT_ARRAY(hosts, no_of_hosts, (MAX_RECIPIENT_LENGTH + 1), char);
      while (argc > 1)
      {
         (void)strcpy(hosts[i], argv[1]);
         argc--; argv++;
         i++;
      }
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


/*+++++++++++++++++++++++++++ init_menu_bar() +++++++++++++++++++++++++++*/
static void                                                                
init_menu_bar(Widget mainform_w, Widget *menu_w)
{
   Arg      args[MAXARGS];
   Cardinal argcount;
   Widget   host_pull_down_w;

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
   ow[0] = XtVaCreateManagedWidget("Exit",
                           xmPushButtonWidgetClass, host_pull_down_w,
                           XmNfontList,             fontlist,
                           XmNmnemonic,             'x',
                           XmNaccelerator,          "Alt<Key>x",
                           NULL);
   XtAddCallback(ow[0], XmNactivateCallback, popup_cb,
                 (XtPointer)EXIT_SEL);
   XtManageChild(*menu_w);

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

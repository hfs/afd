/*
 *  show_cmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_cmd - displays log result of any command
 **
 ** SYNOPSIS
 **   show_cmd [--version]
 **                OR
 **   show_cmd [-w <AFD working directory>] [font name] <command>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.12.1999 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>               /* fopen(), NULL                        */
#include <string.h>              /* strcpy(), strcat(), strcmp()         */
#include <ctype.h>               /* toupper()                            */
#include <signal.h>              /* signal()                             */
#include <sys/types.h>
#include <sys/wait.h>            /* wait()                               */
#include <unistd.h>              /* gethostname(), STDERR_FILENO         */
#include <stdlib.h>              /* getenv()                             */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#ifdef _EDITRES
#include <X11/Xmu/Editres.h>
#endif

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/LabelG.h>
#include <Xm/Separator.h>
#include <Xm/Form.h>
#include <errno.h>
#include "afd_ctrl.h"
#include "show_cmd.h"
#include "version.h"

/* Global variables */
Display        *display;
XtAppContext   app;
XmTextPosition wpr_position;
XtInputId      cmd_input_id;
Widget         toplevel,
               cmd_output;
int            cmd_fd,
               sys_log_fd = STDERR_FILENO;
pid_t          cmd_pid;
char           cmd[MAX_PATH_LENGTH],
               work_dir[MAX_PATH_LENGTH],
               font_name[40],
               *p_work_dir;

/* Local functions */
static void    init_cmd(int *, char **, char *),
               sig_bus(int),
               sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef _WITH_SCROLLBAR
   int             slider_size;
#endif
   char            window_title[MAX_TITLE_CMD_LENGTH + 1 + MAX_HOSTNAME_LENGTH + 1];
   static String   fallback_res[] =
                   {
                      ".show_cmd*mwmDecorations : 110",
                      ".show_cmd*mwmFunctions : 30",
                      ".show_cmd.form.cmd_outputSW*XmText.fontList : fixed",
                      ".show_cmd*background : NavajoWhite2",
                      ".show_cmd.form.cmd_outputSW.cmd_output.background : NavajoWhite1",
                      ".show_cmd.form.buttonbox*background : PaleVioletRed2",
                      ".show_cmd.form.buttonbox*foreground : Black",
                      ".show_cmd.form.buttonbox*highlightColor : Black",
                      NULL
                   };
   Widget          form,
                   button,
                   buttonbox,
                   h_separator;
   Arg             args[MAXARGS];
   Cardinal        argcount;
   XmFontListEntry entry;
   XmFontList      fontlist;

   CHECK_FOR_VERSION(argc, argv);

   p_work_dir = work_dir;
   init_cmd(&argc, argv, window_title);

   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   toplevel = XtAppInitialize(&app, "AFD", NULL, 0,
                              &argc, argv, fallback_res, args, argcount);

   /* Create managing widget */
   form = XmCreateForm(toplevel, "form", NULL, 0);

   /* Prepare the font */
   entry = XmFontListEntryLoad(XtDisplay(toplevel), font_name,
                               XmFONT_IS_FONT, "TAG1");
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase, 21);
   argcount++;
   buttonbox = XmCreateForm(form, "buttonbox", args, argcount);
   button = XtVaCreateManagedWidget("Repeat",
                        xmPushButtonWidgetClass, buttonbox,
                        XmNfontList,         fontlist,
                        XmNtopAttachment,    XmATTACH_POSITION,
                        XmNtopPosition,      1,
                        XmNleftAttachment,   XmATTACH_POSITION,
                        XmNleftPosition,     1,
                        XmNrightAttachment,  XmATTACH_POSITION,
                        XmNrightPosition,    10,
                        XmNbottomAttachment, XmATTACH_POSITION,
                        XmNbottomPosition,   20,
                        NULL);
   XtAddCallback(button, XmNactivateCallback,
                 (XtCallbackProc)repeat_button, 0);
   button = XtVaCreateManagedWidget("Close",
                        xmPushButtonWidgetClass, buttonbox,
                        XmNfontList,         fontlist,
                        XmNtopAttachment,    XmATTACH_POSITION,
                        XmNtopPosition,      1,
                        XmNleftAttachment,   XmATTACH_POSITION,
                        XmNleftPosition,     11,
                        XmNrightAttachment,  XmATTACH_POSITION,
                        XmNrightPosition,    20,
                        XmNbottomAttachment, XmATTACH_POSITION,
                        XmNbottomPosition,   20,
                        NULL);
   XtAddCallback(button, XmNactivateCallback,
                 (XtCallbackProc)close_button, 0);
   XtManageChild(buttonbox);

   /* Create the second horizontal separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,      XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,          buttonbox);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator = XmCreateSeparator(form, "h_separator", args, argcount);
   XtManageChild(h_separator);

   /* Create cmd_output as a ScrolledText window */
   argcount = 0;
   XtSetArg(args[argcount], XmNrows,                   18);
   argcount++;
   XtSetArg(args[argcount], XmNcolumns,                80);
   argcount++;
   XtSetArg(args[argcount], XmNeditable,               False);
   argcount++;
   XtSetArg(args[argcount], XmNeditMode,               XmMULTI_LINE_EDIT);
   argcount++;
   XtSetArg(args[argcount], XmNwordWrap,               False);
   argcount++;
   XtSetArg(args[argcount], XmNscrollHorizontal,       True);
   argcount++;
   XtSetArg(args[argcount], XmNcursorPositionVisible,  True);
   argcount++;
   XtSetArg(args[argcount], XmNautoShowCursorPosition, False);
   argcount++;
   XtSetArg(args[argcount], XmNfontList,               fontlist);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,           h_separator);
   argcount++;
   cmd_output = XmCreateScrolledText(form, "cmd_output", args, argcount);
   XtManageChild(cmd_output);
   XtManageChild(form);

#ifdef _EDITRES
   XtAddEventHandler(toplevel, (EventMask)0, True, _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets */
   XtRealizeWidget(toplevel);


   /* Set some signal handlers. */
   if ((signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(toplevel, WARN_DIALOG,
                 "Failed to set signal handler's for afd_ctrl : %s",
                 strerror(errno));
   }

   exec_cmd(cmd);

   /* We want the keyboard focus on the cmd output */
   XmProcessTraversal(cmd_output, XmTRAVERSE_CURRENT);

   /* Start the main event-handling loop */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++ init_cmd() ++++++++++++++++++++++++++++++*/
static void
init_cmd(int *argc, char *argv[], char *title_cmd)
{
   int  length = 0;
   char progname[MAX_PATH_LENGTH],
        *ptr;

   /* Get display pointer */
   if ((display = XOpenDisplay(NULL)) == NULL)
   {
      (void)fprintf(stderr, "ERROR   : Could not open Display.\n");
      exit(INCORRECT);
   }

   /* First get working directory for the AFD */
   if (get_afd_path(argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   (void)strcpy(progname, argv[0]);
   if (*argc < 2)
   {
      (void)fprintf(stderr,
                    "Usage: %s [nummeric font name] <command>\n",
                    progname);
      exit(INCORRECT);
   }
   if ((*argc > 1) && (isdigit(argv[1][0]) != 0))
   {
      (void)strcpy(font_name, argv[1]);
      (*argc)--; argv++;
   }
   else
   {
      (void)strcpy(font_name, "fixed");
   }
   if (*argc < 2)
   {
      (void)fprintf(stderr,
                    "Usage: %s [nummeric font name] <command>\n",
                    progname);
      exit(INCORRECT);
   }
   if (argv[1][0] == '"')
   {
      (void)strcpy(cmd, &argv[1][1]);
      length = strlen(cmd);
      if (cmd[length - 1] == '"')
      {
         cmd[length - 1] = '\0';
      }
   }
   else
   {
      (void)strcpy(cmd, argv[1]);
   }
   (*argc)--; argv++;

   /* Cut out command for title of window */
   ptr = cmd;
   if ((cmd[0] == '/') || (cmd[0] == '.'))
   {
      char *p_end;

      while ((*ptr != ' ') && (*ptr != '\0'))
      {
         ptr++;
      }
      p_end = ptr;
      while ((*ptr != '/') && (*ptr != '.'))
      {
         ptr--;
      }
      ptr++;
      length = p_end - ptr;
      if (length > MAX_TITLE_CMD_LENGTH)
      {
         length = MAX_TITLE_CMD_LENGTH;
      }
      (void)strncpy(title_cmd, ptr, length);
   }
   else
   {
      while ((*ptr != ' ') && (*ptr != '\0') &&
             (length < MAX_TITLE_CMD_LENGTH))
      {
         title_cmd[length] = *ptr;
         ptr++; length++;
      }
   }
   title_cmd[length] = ' ';
   title_cmd[length + 1] = '\0';

   /* Cut out target hostname */
   while (*ptr != '\0')
   {
      ptr++;
   }
   while ((*ptr != ' ') && (ptr != cmd))
   {
      ptr--;
   }
   if (*ptr == ' ')
   {
      *ptr = '\0';
      ptr++;
   }
   (void)strcat(title_cmd, ptr);

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

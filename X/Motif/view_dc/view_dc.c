/*
 *  view_dc.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2004 Deutscher Wetterdienst (DWD),
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
 **   view_dc - displays information on a single AFD
 **
 ** SYNOPSIS
 **   view_dc [--version] [-w <AFD working directory>] host-name [font name]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.02.1999 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>              /* strcpy(), strcat(), strcmp()         */
#include <ctype.h>               /* toupper()                            */
#include <sys/types.h>
#include <stdlib.h>              /* getenv()                             */
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#ifdef _EDITRES
#include <X11/Xmu/Editres.h>
#endif

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/ToggleBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/LabelG.h>
#include <Xm/Separator.h>
#include <Xm/ScrollBar.h>
#include <Xm/Form.h>
#include <errno.h>
#include "view_dc.h"
#include "version.h"
#include "permission.h"

/* Global variables */
Display      *display;
XtAppContext app;
Widget       text_w,
             toplevel_w;
Dimension    button_height;
int          glyph_height,
             glyph_width,
             sys_log_fd = STDERR_FILENO,
             view_passwd = NO;
char         host_name[MAX_HOSTNAME_LENGTH + 1],
             font_name[40],
             *p_work_dir;

/* Local function prototypes */
static void  init_view_dc(int *, char **),
             usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char            window_title[100],
                   work_dir[MAX_PATH_LENGTH];
   static String   fallback_res[] =
                   {
                      "*mwmDecorations : 42",
                      "*mwmFunctions : 12",
                      ".view_dc.form*background : NavajoWhite2",
                      ".view_dc.form.dc_textSW.dc_text.background : NavajoWhite1",
                      ".view_dc.form.buttonbox*background : PaleVioletRed2",
                      ".view_dc.form.buttonbox*foreground : Black",
                      ".view_dc.form.buttonbox*highlightColor : Black",
                      NULL
                   };
   Widget          form_w,
                   button_w,
                   buttonbox_w,
                   h_separator_w;
   XmFontListEntry entry;
   XFontStruct     *font_struct;
   XmFontList      fontlist;
   XmFontType      dummy;
   Arg             args[MAXARGS];
   Cardinal        argcount;
   uid_t           euid, /* Effective user ID. */
                   ruid; /* Real user ID. */

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values */
   p_work_dir = work_dir;
   init_view_dc(&argc, argv);

   /*
    * SSH uses wants to look at .Xauthority and with setuid flag
    * set we cannot do that. So when we initialize X lets temporaly
    * disable it. After XtAppInitialize() we set it back.
    */
   euid = geteuid();
   ruid = getuid();
   if (euid != ruid)
   {
      if (euid != ruid)
      {
         if (seteuid(ruid) == -1)
         {
            (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                          ruid, strerror(errno));
         }
      }
   }
   (void)strcpy(window_title, "DIR_CONFIG ");
   (void)strcat(window_title, host_name);
   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   toplevel_w = XtAppInitialize(&app, "AFD", NULL, 0,
                                &argc, argv, fallback_res, args, argcount);
   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       euid, strerror(errno));
      }
   }

   /* Get display pointer */
   if ((display = XtDisplay(toplevel_w)) == NULL)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open Display : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Create managing widget */
   form_w = XmCreateForm(toplevel_w, "form", NULL, 0);

   entry = XmFontListEntryLoad(XtDisplay(form_w), font_name,
                               XmFONT_IS_FONT, "TAG1");
   font_struct = (XFontStruct *)XmFontListEntryGetFont(entry, &dummy);
   glyph_height = font_struct->ascent + font_struct->descent;
   glyph_width  = font_struct->per_char->width;
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,     21);
   argcount++;
   buttonbox_w = XmCreateForm(form_w, "buttonbox", args, argcount);
   XtVaGetValues(buttonbox_w, XmNheight, &button_height, NULL);

   /* Create a horizontal separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,      XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,          buttonbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator_w = XmCreateSeparator(form_w, "h_separator", args, argcount);
   XtManageChild(h_separator_w);

   button_w = XtVaCreateManagedWidget("Close",
                                      xmPushButtonWidgetClass, buttonbox_w,
                                      XmNfontList,         fontlist,
                                      XmNtopAttachment,    XmATTACH_POSITION,
                                      XmNtopPosition,      2,
                                      XmNbottomAttachment, XmATTACH_POSITION,
                                      XmNbottomPosition,   19,
                                      XmNleftAttachment,   XmATTACH_POSITION,
                                      XmNleftPosition,     1,
                                      XmNrightAttachment,  XmATTACH_POSITION,
                                      XmNrightPosition,    20,
                                      NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)close_button, 0);
   XtManageChild(buttonbox_w);

   /* Create DIR_CONFIG data as a ScrolledText window */
   argcount = 0;
   XtSetArg(args[argcount], XmNfontList,               fontlist);
   argcount++;
   XtSetArg(args[argcount], XmNeditable,               False);
   argcount++;
   XtSetArg(args[argcount], XmNeditMode,               XmMULTI_LINE_EDIT);
   argcount++;
   XtSetArg(args[argcount], XmNwordWrap,               False);
   argcount++;
   XtSetArg(args[argcount], XmNscrollHorizontal,       False);
   argcount++;
   XtSetArg(args[argcount], XmNcursorPositionVisible,  False);
   argcount++;
   XtSetArg(args[argcount], XmNautoShowCursorPosition, False);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,              3);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftOffset,             3);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,            3);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,           h_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomOffset,           3);
   argcount++;
   text_w = XmCreateScrolledText(form_w, "dc_text", args, argcount);
   XtManageChild(text_w);
   XtManageChild(form_w);

   /* Free font list */
   XmFontListFree(fontlist);

#ifdef _EDITRES
   XtAddEventHandler(toplevel_w, (EventMask)0, True, _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets */
   XtRealizeWidget(toplevel_w);

   /* Get DIR_CONFIG data information. */
   get_dc_data();

   /* We want the keyboard focus on the Done button */
   XmProcessTraversal(button_w, XmTRAVERSE_CURRENT);

   /* Start the main event-handling loop */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ init_view_dc() +++++++++++++++++++++++++++*/
static void
init_view_dc(int *argc, char *argv[])
{
   char fake_user[MAX_FULL_USER_ID_LENGTH],
        *perm_buffer;

   if ((get_arg(argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(SUCCESS);
   }
   if (get_afd_path(argc, argv, p_work_dir) < 0)
   {
      (void)fprintf(stderr,
                    "Failed to get working directory of AFD. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (get_arg(argc, argv, "-f", font_name, 40) == INCORRECT)
   {
      (void)strcpy(font_name, "fixed");
   }
   if (get_arg(argc, argv, "-h", host_name, MAX_HOSTNAME_LENGTH + 1) == INCORRECT)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }

   /* Now lets see if user may use this program */
   check_fake_user(argc, argv, AFD_CONFIG_FILE, fake_user);
   switch(get_permissions(&perm_buffer, fake_user))
   {
      case NONE :
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         exit(INCORRECT);

      case SUCCESS :
         /* Lets evaluate the permissions and see what */
         /* the user may do.                           */
         if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
             (perm_buffer[2] == 'l') &&
             ((perm_buffer[3] == '\0') || (perm_buffer[3] == ' ') ||
              (perm_buffer[3] == '\t')))
         {
            view_passwd = YES;
            free(perm_buffer);
            break;
         }
         else if (posi(perm_buffer, VIEW_DIR_CONFIG_PERM) == NULL)
              {
                 (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
                 exit(INCORRECT);
              }
              else
              {
                 if (posi(perm_buffer, VIEW_PASSWD_PERM) != NULL)
                 {
                    view_passwd = YES;
                 }
              }
         free(perm_buffer);
         break;

      case INCORRECT:
         /* Hmm. Something did go wrong. Since we want to */
         /* be able to disable permission checking let    */
         /* the user have all permissions.                */
         break;

      default :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   return;
}


/*------------------------------- usage() -------------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "Usage : %s [options] -h hostname\n", progname);
   (void)fprintf(stderr, "             --version\n");
   (void)fprintf(stderr, "             -f <font name>\n");
   (void)fprintf(stderr, "             -u[ <user>]\n");
   (void)fprintf(stderr, "             -w <working directory>\n");
   return;
}

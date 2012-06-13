/*
 *  view_dc.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2011 Deutscher Wetterdienst (DWD),
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
 **   view_dc - displays DIR_CONFIG data for the given dir or host alias
 **
 ** SYNOPSIS
 **   view_dc [options] -d <dir alias> | -h <host alias>
 **              --version
 **              -d <dir alias>
 **              -f <font name>
 **              -h <host alias>
 **              -u[ <user>]
 **              -w <working directory>
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
 **   06.08.2004 H.Kiehl Write window ID to a common file.
 **   19.05.2006 H.Kiehl Use program get_dc_data from the tools section
 **                      to get the DIR_CONFIG data.
 **   05.01.2011 H.Kiehl Added search button.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>              /* strcpy(), strcat(), strcmp()         */
#include <ctype.h>               /* toupper()                            */
#include <sys/types.h>
#include <stdlib.h>              /* getenv(), atexit()                   */
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

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
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif
#include <errno.h>
#include "view_dc.h"
#include "version.h"
#include "permission.h"

/* Global variables. */
Display      *display;
XtAppContext app;
Widget       appshell,
             searchbox_w,
             text_w;
int          sys_log_fd = STDERR_FILENO;
char         dir_alias[MAX_DIR_ALIAS_LENGTH + 1],
             host_alias[MAX_HOSTNAME_LENGTH + 1],
             font_name[40],
             *p_title,
             *p_work_dir;
const char   *sys_log_name = SYSTEM_LOG_FIFO;

/* Local variables. */
static char  *view_buffer = NULL;
static int   max_x,
             max_y;

/* Local function prototypes. */
static void  init_view_dc(int *, char **),
             usage(char *),
             view_dc_exit(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int             glyph_height,
                   max_vertical_lines;
   char            window_title[100],
                   work_dir[MAX_PATH_LENGTH];
   static String   fallback_res[] =
                   {
                      "*mwmDecorations : 42",
                      "*mwmFunctions : 12",
                      ".view_dc.form*background : NavajoWhite2",
                      ".view_dc.form.buttonbox2.searchbox*background : NavajoWhite1",
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

   /* Initialise global values. */
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
      if (seteuid(ruid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       ruid, strerror(errno));
      }
   }

   (void)strcpy(window_title, "DIR_CONFIG ");
   (void)strcat(window_title, p_title);
   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   appshell = XtAppInitialize(&app, "AFD", NULL, 0,
                              &argc, argv, fallback_res, args, argcount);
   disable_drag_drop(appshell);

   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       euid, strerror(errno));
      }
   }

   /* Get display pointer. */
   if ((display = XtDisplay(appshell)) == NULL)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open Display : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

#ifdef HAVE_XPM
   /* Setup AFD logo as icon. */
   setup_icon(display, appshell);
#endif

   /* Create managing widget. */
   form_w = XmCreateForm(appshell, "form", NULL, 0);

   if ((entry = XmFontListEntryLoad(XtDisplay(form_w), font_name,
                                    XmFONT_IS_FONT, "TAG1")) == NULL)
   {
      if ((entry = XmFontListEntryLoad(XtDisplay(form_w), "fixed",
                                       XmFONT_IS_FONT, "TAG1")) == NULL)
      {
         (void)fprintf(stderr,
                       "Failed to load font with XmFontListEntryLoad() : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   font_struct = (XFontStruct *)XmFontListEntryGetFont(entry, &dummy);
   glyph_height = font_struct->ascent + font_struct->descent;
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   /* Calculate the maximum lines to show. */
   max_vertical_lines = (8 * (DisplayHeight(display, DefaultScreen(display)) /
                        glyph_height)) / 10;
   if (max_y > max_vertical_lines)
   {
      max_y = max_vertical_lines;
   }

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

   /* Create a horizontal separator. */
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
                 (XtCallbackProc)close_button, (XtPointer)0);
   XtManageChild(buttonbox_w);

   /* Create DIR_CONFIG data as a ScrolledText window. */
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
   XtSetArg(args[argcount], XmNrows,                   max_y);
   argcount++;
   XtSetArg(args[argcount], XmNcolumns,                max_x);
   argcount++;
   XtSetArg(args[argcount], XmNvalue,                  view_buffer);
   argcount++;
   text_w = XmCreateScrolledText(form_w, "dc_text", args, argcount);
   XtManageChild(text_w);

   /* Create a horizontal separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,      XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,          text_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator_w = XmCreateSeparator(form_w, "h_separator", args, argcount);
   XtManageChild(h_separator_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,              1);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,           h_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomOffset,           1);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,           31);
   argcount++;
   buttonbox_w = XmCreateForm(form_w, "buttonbox2", args, argcount);

   searchbox_w = XtVaCreateWidget("searchbox",
                                  xmTextWidgetClass,        buttonbox_w,
                                  XmNtopAttachment,         XmATTACH_POSITION,
                                  XmNtopPosition,           5,
                                  XmNbottomAttachment,      XmATTACH_POSITION,
                                  XmNbottomPosition,        26,
                                  XmNleftAttachment,        XmATTACH_POSITION,
                                  XmNleftPosition,          1,
                                  XmNrightAttachment,       XmATTACH_POSITION,
                                  XmNrightPosition,         20,
                                  XmNfontList,              fontlist,
                                  XmNrows,                  1,
                                  XmNeditable,              True,
                                  XmNcursorPositionVisible, True,
                                  XmNmarginHeight,          1,
                                  XmNmarginWidth,           1,
                                  XmNshadowThickness,       1,
                                  XmNhighlightThickness,    0,
                                  NULL);
   XtManageChild(searchbox_w);
   button_w = XtVaCreateManagedWidget("Search",
                                      xmPushButtonWidgetClass, buttonbox_w,
                                      XmNleftAttachment,       XmATTACH_POSITION,
                                      XmNleftPosition,         22,
                                      XmNrightAttachment,      XmATTACH_POSITION,
                                      XmNrightPosition,        28,
                                      XmNtopAttachment,        XmATTACH_FORM,
                                      XmNfontList,             fontlist,
                                      NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)search_button, (XtPointer)0);
   XtManageChild(buttonbox_w);
   XtManageChild(form_w);

   /* Free font list. */
   XmFontListFree(fontlist);

#ifdef WITH_EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True,
                     _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets. */
   XtRealizeWidget(appshell);

   /* We want the keyboard focus on the Close button. */
   XmProcessTraversal(button_w, XmTRAVERSE_CURRENT);

   /* Write window ID, so afd_ctrl can set focus if it is called again. */
   write_window_id(XtWindow(appshell), getpid(), VIEW_DC);

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ init_view_dc() +++++++++++++++++++++++++++*/
static void
init_view_dc(int *argc, char *argv[])
{
   int  empty_lines,
        length,
        line_length;
   char cmd[MAX_PATH_LENGTH],
        *data_buffer,
        fake_user[MAX_FULL_USER_ID_LENGTH],
        *perm_buffer,
        *p_retr_send_sep;

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
   if (get_arg(argc, argv, "-h", host_alias, MAX_HOSTNAME_LENGTH + 1) == INCORRECT)
   {
      if (get_arg(argc, argv, "-d", dir_alias, MAX_DIR_ALIAS_LENGTH + 1) == INCORRECT)
      {
         usage(argv[0]);
         exit(INCORRECT);
      }
      host_alias[0] = '\0';
   }
   else
   {
      dir_alias[0] = '\0';
   }

   /* Now lets see if user may use this program. */
   check_fake_user(argc, argv, AFD_CONFIG_FILE, fake_user);
   switch (get_permissions(&perm_buffer, fake_user))
   {
      case NO_ACCESS : /* Cannot access afd.users file. */
         {
            char afd_user_file[MAX_PATH_LENGTH];

            (void)strcpy(afd_user_file, p_work_dir);
            (void)strcat(afd_user_file, ETC_DIR);
            (void)strcat(afd_user_file, AFD_USER_FILE);

            (void)fprintf(stderr,
                          "Failed to access `%s', unable to determine users permissions.\n",
                          afd_user_file);
         }
         exit(INCORRECT);

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
            free(perm_buffer);
            break;
         }
         else if (posi(perm_buffer, VIEW_DIR_CONFIG_PERM) == NULL)
              {
                 (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
                 exit(INCORRECT);
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

   /*
    * Run get_dc_data program to get the data.
    */
   length = sprintf(cmd, "%s %s %s", GET_DC_DATA, WORK_DIR_ID, p_work_dir);
   if (fake_user[0] != '\0')
   {
      length += sprintf(&cmd[length], " -u %s", fake_user);
   }
   if (host_alias[0] != '\0')
   {
      length += sprintf(&cmd[length], " -h \"%s\"", host_alias);
   }
   else if (dir_alias[0] != '\0')
        {
           length += sprintf(&cmd[length], " -d \"%s\"", dir_alias);
        }
   data_buffer = NULL;
   if ((exec_cmd(cmd, &data_buffer, -1, NULL, 0,
#ifdef HAVE_SETPRIORITY
                 NO_PRIORITY,
#endif
                 "", 0L, NO, NO) != 0) ||
       (data_buffer == NULL))
   {
      (void)fprintf(stderr, "Failed to execute command: %s\n", cmd);
      (void)fprintf(stderr, "See SYSTEM_LOG for more information.\n");
      exit(INCORRECT);
   }

   length = 0;
   empty_lines = 0;
   max_x = 0;
   max_y = 0;
   while (data_buffer[length] != '\0')
   {
      line_length = 0;
      while ((data_buffer[length] != '\n') && (data_buffer[length] != '\0'))
      {
         length++; line_length++;
      }
      if (data_buffer[length] == '\n')
      {
         max_y++;
         length++;
         if (line_length > max_x)
         {
            max_x = line_length;
         }
         line_length = 0;
         if (data_buffer[length] == '\n')
         {
            length++;
            empty_lines++;
            max_y++;
         }
      }
   }
   if (length > 0)
   {
      int new_lines_removed = 0;

      while (data_buffer[length - 1] == '\n')
      {
         new_lines_removed++;
         length--;
      }
      empty_lines = empty_lines - (new_lines_removed / 2);
      data_buffer[length] = '\0';
      length++;
   }

   if ((length > 0) && (host_alias[0] != '\0'))
   {
      if (posi(data_buffer, DIR_IDENTIFIER) != NULL)
      {
         if ((p_retr_send_sep = posi(data_buffer,
                                     VIEW_DC_DIR_IDENTIFIER)) == NULL)
         {
            (void)strcpy(dir_alias, host_alias);
            host_alias[0] = '\0';
         }
         else
         {
            p_retr_send_sep -= (VIEW_DC_DIR_IDENTIFIER_LENGTH + 1);
         }
      }
      else
      {
         p_retr_send_sep = NULL;
      }
   }

   /*
    * For host alias, lets insert separator lines for empty
    * lines. This makes it more readable.
    */
   if (host_alias[0] != '\0')
   {
      if (length == 0)
      {
         view_buffer = data_buffer;
         max_x = sprintf(view_buffer,
                         "\n  No data found for host %s!\n\n",
                         host_alias);
         max_y = 3;
      }
      else
      {
         size_t new_size;
         char   *wptr;

         new_size = length + 1 + (empty_lines * (max_x + 1));

         if ((view_buffer = malloc(new_size)) == NULL)
         {
#if SIZEOF_SIZE_T == 4
            (void)fprintf(stderr, "Failed to malloc() %d bytes : %s",
#else
            (void)fprintf(stderr, "Failed to malloc() %lld bytes : %s",
#endif
                          (pri_size_t)new_size, strerror(errno));
            exit(INCORRECT);
         }

         if (p_retr_send_sep == NULL)
         {
            length = 0;
            wptr = view_buffer;
         }
         else
         {
            length = p_retr_send_sep - data_buffer;
            (void)memcpy(view_buffer, data_buffer, length);
            wptr = &view_buffer[length];
            (void)memset(wptr, '=', max_x);
            wptr += max_x;
            *wptr = '\n';
            wptr++;
         }
         while (data_buffer[length] != '\0')
         {
            while ((data_buffer[length] != '\n') && (data_buffer[length] != '\0'))
            {
               *wptr = data_buffer[length];
               length++; wptr++;
            }
            if (data_buffer[length] == '\n')
            {
               *wptr = '\n';
               length++; wptr++;
               if (data_buffer[length] == '\n')
               {
                  (void)memset(wptr, '-', max_x);
                  wptr += max_x;
                  *wptr = '\n';
                  length++; wptr++;
               }
            }
         }
         *wptr = '\0';
         free(data_buffer);
         max_y--;
      }
      p_title = host_alias;
   }
   else
   {
      view_buffer = data_buffer;
      if (length == 0)
      {
         max_x = sprintf(view_buffer,
                         "\n  No data found for directory %s!\n\n",
                         dir_alias);
         max_y = 3;
      }
      p_title = dir_alias;
   }

   if (atexit(view_dc_exit) != 0)
   {
      (void)xrec(WARN_DIALOG, "Failed to set exit handler for %s : %s",
                 VIEW_DC, strerror(errno));
   }
   check_window_ids(VIEW_DC);

   return;
}


/*------------------------------- usage() -------------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 "Usage : %s [options] -d <dir alias> | -h <host alias>\n",
                 progname);
   (void)fprintf(stderr, "             --version\n");
   (void)fprintf(stderr, "             -d <dir alias>\n");
   (void)fprintf(stderr, "             -f <font name>\n");
   (void)fprintf(stderr, "             -h <host alias>\n");
   (void)fprintf(stderr, "             -u[ <user>]\n");
   (void)fprintf(stderr, "             -w <working directory>\n");
   return;
}


/*--------------------------- view_dc_exit() ----------------------------*/
static void
view_dc_exit(void)
{
   remove_window_id(getpid(), VIEW_DC);
   return;
}

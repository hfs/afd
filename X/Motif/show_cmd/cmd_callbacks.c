/*
 *  cmd_callbacks.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M3
/*
 ** NAME
 **   cmd_callbacks - all callback functions for module show_cmd
 **
 ** SYNOPSIS
 **   cmd_callbacks
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
DESCR__E_M3

#include <stdio.h>                  /* popen(), pclose()                 */
#include <string.h>
#include <stdlib.h>                 /* exit()                            */
#include <sys/types.h>
#include <sys/wait.h>               /* waitpid()                         */
#include <unistd.h>                 /* read(), close()                   */
#include <signal.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <errno.h>
#include "afd_ctrl.h"
#include "show_cmd.h"

/* External global variables */
extern char           cmd[];
extern pid_t          cmd_pid;
extern int            cmd_fd;
extern Display        *display;
extern Widget         cmd_output;
extern XtInputId      cmd_input_id;
extern XmTextPosition wpr_position;
extern XtAppContext   app;

/* Local function prototypes */
static void           kill_child(Widget),
                      repeat_cmd(Widget);


/*############################ close_button() ###########################*/
void
close_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (cmd_pid != 0)
   {
      if (kill(cmd_pid, SIGINT) == -1)
      {
         (void)fprintf(stderr,
                       "Failed to kill() process %d : %s (%s %d)\n",
                       cmd_pid, strerror(errno), __FILE__, __LINE__);
      }
   }

   exit(SUCCESS);
}


/*########################## repeat_button() ############################*/
void
repeat_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (cmd_pid != 0)
   {
      XtAppAddTimeOut(app, 0, (XtTimerCallbackProc)kill_child, cmd_output);
   }

   XtAppAddTimeOut(app, 0, (XtTimerCallbackProc)repeat_cmd, cmd_output);

   return;
}


/*++++++++++++++++++++++++++++ kill_child() +++++++++++++++++++++++++++++*/
static void
kill_child(Widget w)
{
   pid_t pid = cmd_pid;

   XtRemoveInput(cmd_input_id);
   cmd_input_id = 0L;
   if (kill(pid, SIGINT) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to kill() process %d : %s (%s %d)\n",
                    cmd_pid, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      catch_child(cmd_output);
   }

   return;
}


/*--------------------------- catch_child() -----------------------------*/
void
catch_child(Widget w)
{
   int   n;
   char  buffer[MAX_PATH_LENGTH + 3];

   if ((n = read(cmd_fd, buffer, MAX_PATH_LENGTH)) > 0)
   {
      buffer[n] = '\0';
      XmTextInsert(cmd_output, wpr_position, buffer);
      wpr_position += n;
      XmTextShowPosition(cmd_output, wpr_position);
      XFlush(display);
   }
   else if (n == 0)
        {
           buffer[0] = '>';
           buffer[1] = '\0';
           XmTextInsert(cmd_output, wpr_position, buffer);
           XmTextShowPosition(cmd_output, wpr_position);
           XFlush(display);
           if (cmd_pid > 0)
           {
              if (wait(NULL) == -1)
              {
                 (void)fprintf(stderr, "wait() error : %s (%s %d)\n",
                               strerror(errno), __FILE__, __LINE__);
                 exit(INCORRECT);
              }
              cmd_pid = 0;
           }
           wpr_position = 0;
           if (cmd_input_id != 0L)
           {
              XtRemoveInput(cmd_input_id);
              cmd_input_id = 0L;
              if (close(cmd_fd) == -1)
              {
                 (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                               strerror(errno), __FILE__, __LINE__);
              }
           }
        }

   return;
}


/*++++++++++++++++++++++++++++ repeat_cmd() +++++++++++++++++++++++++++++*/
static void
repeat_cmd(Widget w)
{
   XmTextSetInsertionPosition(cmd_output, 0);
   XmTextSetString(cmd_output, NULL);
   XFlush(display);
   wpr_position = 0;

   exec_cmd(cmd);

   return;
}

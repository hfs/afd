/*
 *  xshow_stat.c - Part of AFD, an automatic file distribution program.
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
 **   xshow_stat - shows output statistics of the AFD
 **
 ** SYNOPSIS
 **   xshow_stat [--version]
 **                  OR
 **              [-w <AFD working directory>] [font name] [host name 1..n]
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

/* Global variables */
char        *p_work_dir,
            **hosts;

/* Local function prototypes */
static void init_afd_stat(int, char **, char *, char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char     window_title[100],
            work_dir[MAX_PATH_LENGTH];
   Widget   form;
   Arg      args[MAXARGS];
   Cardinal argcount;

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
                             NULL, args, argcount);

   mainwindow = appshell;

   /* Create managing widget */
   form = XmCreateForm(appshell, "form", NULL, 0);
   XtManageChild(form);

   /* Setup colors */
   default_cmap = DefaultColormap(display, DefaultScreen(display));
   init_color(XtDisplay(appshell));
}


/*++++++++++++++++++++++++++ init_show_stat() +++++++++++++++++++++++++++*/
static void
init_show_stat(int argc, char *argv[], char *font_name, char *window_title)
{
   char *perm_buffer,
        hostname[MAX_AFD_NAME_LENGTH];

   if (get_afd_path(&argc, argv, p_work_dir) < 0)
   {
      (void)fprintf(stderr,
                    "Failed to get working directory of AFD. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Now lets see if user may use this program */
   switch(get_permissions(&perm_buffer))
   {
      case NONE     : (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
                      exit(INCORRECT);

      case SUCCESS  : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
                      if ((perm_buffer[0] == 'a') &&
                          (perm_buffer[1] == 'l') &&
                          (perm_buffer[2] == 'l') &&
                          ((perm_buffer[3] == '\0') ||
                           (perm_buffer[3] == ' ') ||
                           (perm_buffer[3] == '\t')))
                      {
                         free(perm_buffer);
                         break;
                      }
                      else if (posi(perm_buffer, XSHOW_STAT_PERM) == NULL)
                           {
                              (void)fprintf(stderr,
                                            "%s\n", PERMISSION_DENIED_STR);
                              exit(INCORRECT);
                           }
                      free(perm_buffer);
                      break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
                      break;

      default       : (void)fprintf(stderr,
                                    "Impossible!! Remove the programmer!\n");
                      exit(INCORRECT);
   }
   if (argc < 1)
   {
      (void)fprintf(stderr,
                    "Usage : %s [-w <working directory>] [font name] [host name 1..n]\n",
                    argv[0]);
      exit(INCORRECT);
   }
   if ((argc > 1) && (isdigit(argv[1][0]) != 0))
   {
      (void)strcpy(font_name, argv[1]);
      argc--; argv++;
   }
   else
   {
      (void)strcpy(font_name, "fixed");
   }

   /* Collect all hostnames */
   no_of_hosts = argc - 1;
   if (no_of_hosts > 0)
   {
      int i = 0;

      RT_ARRAY(search_recipient, no_of_hosts,
               (MAX_RECIPIENT_LENGTH + 1), char);
      while (argc > 1)
      {
         (void)strcpy(hosts[i], argv[1]);
         argc--; argv++;
         i++;
      }
   }

   /* Prepare title of this window. */
   (void)strcpy(window_title, "Output Log ");
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

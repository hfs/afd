/*
 *  print_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   print_data - prints data from the input log
 **
 ** SYNOPSIS
 **   void print_data(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.05.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>               /* sprintf(), popen(), pclose()         */
#include <string.h>              /* strcpy(), strcat(), strerror()       */
#include <unistd.h>              /* write(), close()                     */
#include <time.h>                /* ctime(), strftime()                  */
#include <signal.h>              /* signal()                             */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/LabelG.h>
#include <Xm/Frame.h>
#include <Xm/Text.h>
#include <Xm/List.h>
#include <Xm/Separator.h>
#include <Xm/PushB.h>
#include <Xm/ToggleBG.h>
#include <X11/keysym.h>
#ifdef _EDITRES
#include <X11/Xmu/Editres.h>
#endif
#include <errno.h>
#include "afd_ctrl.h"
#include "show_ilog.h"

/* External global variables */
extern Display *display;
extern Widget  listbox_w,
               toplevel_w,
               summarybox_w;
extern char    font_name[],
               search_file_name[],
               search_directory_name[],
               **search_recipient,
               search_file_size_str[],
               summary_str[],
               total_summary_str[];
extern int     no_of_search_hosts,
               toggles_set,
               file_name_length;
extern time_t  start_time_val,
               end_time_val;

/* Local global variables */
static Widget  printshell = (Widget)NULL,
               printer_text_w,
               printer_radio_w,
               file_text_w,
               file_radio_w;
static char    printer_cmd[PRINTER_INFO_LENGTH + 1],
               printer_name[PRINTER_INFO_LENGTH + 1],
               file_name[MAX_PATH_LENGTH];
static int     range_type,
               device_type;

/* Local function prototypes. */
static int     prepare_printer(int *),
               prepare_file(int *);
static FILE    *fp;
static void    print_data_button(Widget, XtPointer, XtPointer),
               cancel_print_button(Widget, XtPointer, XtPointer),
               device_select(Widget, XtPointer, XtPointer),
               range_select(Widget, XtPointer, XtPointer),
               save_file_name(Widget, XtPointer, XtPointer),
               save_printer_name(Widget, XtPointer, XtPointer),
               write_header(int),
               write_summary(int);


/*############################# print_data() ############################*/
void
print_data(void)
{
   if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
   {
      (void)fprintf(stderr, "signal() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }

   /*
    * First, see if the window has already been created. If
    * no create a new window.
    */
   if ((printshell == (Widget)NULL) || (XtIsRealized(printshell) == False) ||
       (XtIsSensitive(printshell) != True))
   {
      Widget          main_form_w,
                      form_w,
                      frame_w,
                      radio_w,
                      criteriabox_w,
                      radiobox_w,
                      separator_w,
                      buttonbox_w,
                      button_w,
                      inputline_w;
      Arg             args[MAXARGS];
      Cardinal        argcount;
      XmFontList      p_fontlist;
      XmFontListEntry entry;

      /* Get default values from AFD_CONFIG file. */
      get_printer_cmd(printer_cmd, printer_name);

      printshell = XtVaCreatePopupShell("Print Data", topLevelShellWidgetClass,
                                       toplevel_w, NULL);

      /* Create managing widget */
      main_form_w = XmCreateForm(printshell, "main_form", NULL, 0);

      /* Prepare font */
      if ((entry = XmFontListEntryLoad(XtDisplay(main_form_w), font_name, XmFONT_IS_FONT, "TAG1")) == NULL)
      {
         if ((entry = XmFontListEntryLoad(XtDisplay(main_form_w), "fixed", XmFONT_IS_FONT, "TAG1")) == NULL)
         {
            (void)fprintf(stderr, "Failed to load font with XmFontListEntryLoad() : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      p_fontlist = XmFontListAppendEntry(NULL, entry);
      XmFontListEntryFree(&entry);

      /*---------------------------------------------------------------*/
      /*                         Button Box                            */
      /*---------------------------------------------------------------*/
      argcount = 0;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNfractionBase,     21);
      argcount++;
      buttonbox_w = XmCreateForm(main_form_w, "buttonbox", args, argcount);

      /* Create Print Button. */
      button_w = XtVaCreateManagedWidget("Print", xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,         p_fontlist,
                        XmNtopAttachment,    XmATTACH_POSITION,
                        XmNtopPosition,      1,
                        XmNleftAttachment,   XmATTACH_POSITION,
                        XmNleftPosition,     1,
                        XmNrightAttachment,  XmATTACH_POSITION,
                        XmNrightPosition,    10,
                        XmNbottomAttachment, XmATTACH_POSITION,
                        XmNbottomPosition,   20,
                        NULL);
      XtAddCallback(button_w, XmNactivateCallback,
                    (XtCallbackProc)print_data_button, 0);

      /* Create Cancel Button. */
      button_w = XtVaCreateManagedWidget("Close", xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,         p_fontlist,
                        XmNtopAttachment,    XmATTACH_POSITION,
                        XmNtopPosition,      1,
                        XmNleftAttachment,   XmATTACH_POSITION,
                        XmNleftPosition,     11,
                        XmNrightAttachment,  XmATTACH_POSITION,
                        XmNrightPosition,    20,
                        XmNbottomAttachment, XmATTACH_POSITION,
                        XmNbottomPosition,   20,
                        NULL);
      XtAddCallback(button_w, XmNactivateCallback,
                    (XtCallbackProc)cancel_print_button, 0);
      XtManageChild(buttonbox_w);

      /*---------------------------------------------------------------*/
      /*                      Horizontal Separator                     */
      /*---------------------------------------------------------------*/
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
      separator_w = XmCreateSeparator(main_form_w, "separator", args, argcount);
      XtManageChild(separator_w);

      /*---------------------------------------------------------------*/
      /*                        Criteria Box                           */
      /*---------------------------------------------------------------*/
      criteriabox_w = XtVaCreateWidget("criteriabox",
                                       xmFormWidgetClass,   main_form_w,
                                       XmNtopAttachment,    XmATTACH_FORM,
                                       XmNleftAttachment,   XmATTACH_FORM,
                                       XmNrightAttachment,  XmATTACH_FORM,
                                       XmNbottomAttachment, XmATTACH_WIDGET,
                                       XmNbottomWidget,     separator_w,
                                       NULL);

      /*---------------------------------------------------------------*/
      /*                         Range Box                             */
      /*---------------------------------------------------------------*/
      /* Frame for Range Box */
      frame_w = XtVaCreateManagedWidget("range_frame",
                              xmFrameWidgetClass,  criteriabox_w,
                              XmNshadowType,       XmSHADOW_ETCHED_IN,
                              XmNtopAttachment,    XmATTACH_FORM,
                              XmNtopOffset,        5,
                              XmNleftAttachment,   XmATTACH_FORM,
                              XmNleftOffset,       5,
                              XmNbottomAttachment, XmATTACH_FORM,
                              XmNbottomOffset,     5,
                              NULL);

      /* Label for the frame */
      XtVaCreateManagedWidget("Range",
                              xmLabelGadgetClass,          frame_w,
                              XmNchildType,                XmFRAME_TITLE_CHILD,
                              XmNchildVerticalAlignment,   XmALIGNMENT_CENTER,
                              NULL);

      /* Manager widget for the actual range stuff */
      radiobox_w = XtVaCreateWidget("radiobox",
                                     xmRowColumnWidgetClass, frame_w,
                                     XmNradioBehavior,       True,
                                     XmNorientation,         XmVERTICAL,
                                     XmNpacking,             XmPACK_COLUMN,
                                     XmNnumColumns,          1,
                                     XmNresizable,           False,
                                     NULL);

      radio_w = XtVaCreateManagedWidget("Selection",
                                     xmToggleButtonGadgetClass, radiobox_w,
                                     XmNfontList,               p_fontlist,
                                     XmNset,                    True,
                                     NULL);
      XtAddCallback(radio_w, XmNarmCallback,
                    (XtCallbackProc)range_select, (XtPointer)SELECTION_TOGGLE);
      radio_w = XtVaCreateManagedWidget("All",
                                     xmToggleButtonGadgetClass, radiobox_w,
                                     XmNfontList,               p_fontlist,
                                     XmNset,                    False,
                                     NULL);
      XtAddCallback(radio_w, XmNarmCallback,
                    (XtCallbackProc)range_select, (XtPointer)ALL_TOGGLE);
      XtManageChild(radiobox_w);
      range_type = SELECTION_TOGGLE;

      /*---------------------------------------------------------------*/
      /*                        Device Box                             */
      /*---------------------------------------------------------------*/
      /* Frame for Device Box */
      frame_w = XtVaCreateManagedWidget("device_frame",
                              xmFrameWidgetClass,   criteriabox_w,
                              XmNshadowType,        XmSHADOW_ETCHED_IN,
                              XmNtopAttachment,     XmATTACH_FORM,
                              XmNtopOffset,         5,
                              XmNleftAttachment,    XmATTACH_WIDGET,
                              XmNleftWidget,        frame_w,
                              XmNleftOffset,        5,
                              XmNrightAttachment,   XmATTACH_FORM,
                              XmNrightOffset,       5,
                              XmNbottomAttachment,  XmATTACH_FORM,
                              XmNbottomOffset,      5,
                              NULL);

      /* Label for the frame */
      XtVaCreateManagedWidget("Device",
                              xmLabelGadgetClass,          frame_w,
                              XmNchildType,                XmFRAME_TITLE_CHILD,
                              XmNchildVerticalAlignment,   XmALIGNMENT_CENTER,
                              NULL);

      form_w = XtVaCreateWidget("device_form",
                                xmFormWidgetClass, frame_w,
                                NULL);

      /* Create selection line to select a printer */
      inputline_w = XtVaCreateWidget("input_line",
                                     xmFormWidgetClass,  form_w,
                                     XmNtopAttachment,   XmATTACH_FORM,
                                     XmNrightAttachment, XmATTACH_FORM,
                                     XmNleftAttachment,  XmATTACH_FORM,
                                     NULL);

      printer_radio_w = XtVaCreateManagedWidget("Printer",
                                     xmToggleButtonGadgetClass, inputline_w,
                                     XmNindicatorType,          XmONE_OF_MANY,
                                     XmNfontList,               p_fontlist,
                                     XmNset,                    True,
                                     XmNtopAttachment,          XmATTACH_FORM,
                                     XmNleftAttachment,         XmATTACH_FORM,
                                     XmNbottomAttachment,       XmATTACH_FORM,
                                     NULL);
      XtAddCallback(printer_radio_w, XmNvalueChangedCallback,
                    (XtCallbackProc)device_select, (XtPointer)PRINTER_TOGGLE);

      /* A text box to enter the printers name. */
      printer_text_w = XtVaCreateManagedWidget("printer_name",
                              xmTextWidgetClass,   inputline_w,
                              XmNfontList,         p_fontlist,
                              XmNmarginHeight,     1,
                              XmNmarginWidth,      1,
                              XmNshadowThickness,  1,
                              XmNcolumns,          20,
                              XmNvalue,            printer_name,
                              XmNtopAttachment,    XmATTACH_FORM,
                              XmNleftAttachment,   XmATTACH_WIDGET,
                              XmNleftWidget,       printer_radio_w,
                              XmNrightAttachment,  XmATTACH_WIDGET,
                              XmNrightOffset,      5,
                              XmNbottomAttachment, XmATTACH_FORM,
                              NULL);
      XtAddCallback(printer_text_w, XmNlosingFocusCallback, save_printer_name, 0);
      XtManageChild(inputline_w);

      /* Create selection line to select a file to store the data. */
      inputline_w = XtVaCreateWidget("input_line",
                                     xmFormWidgetClass,  form_w,
                                     XmNtopAttachment,   XmATTACH_WIDGET,
                                     XmNtopWidget,       inputline_w,
                                     XmNtopOffset,       5,
                                     XmNrightAttachment, XmATTACH_FORM,
                                     XmNleftAttachment,  XmATTACH_FORM,
                                     NULL);

      file_radio_w = XtVaCreateManagedWidget("File   ",
                                     xmToggleButtonGadgetClass, inputline_w,
                                     XmNindicatorType,          XmONE_OF_MANY,
                                     XmNfontList,               p_fontlist,
                                     XmNset,                    False,
                                     XmNtopAttachment,          XmATTACH_FORM,
                                     XmNleftAttachment,         XmATTACH_FORM,
                                     XmNbottomAttachment,       XmATTACH_FORM,
                                     NULL);
      XtAddCallback(file_radio_w, XmNvalueChangedCallback,
                    (XtCallbackProc)device_select, (XtPointer)FILE_TOGGLE);
      device_type = PRINTER_TOGGLE;

      /* A text box to enter the files name. */
      file_text_w = XtVaCreateManagedWidget("file_name",
                              xmTextWidgetClass,   inputline_w,
                              XmNfontList,         p_fontlist,
                              XmNmarginHeight,     1,
                              XmNmarginWidth,      1,
                              XmNshadowThickness,  1,
                              XmNcolumns,          20,
                              XmNvalue,            file_name,
                              XmNtopAttachment,    XmATTACH_FORM,
                              XmNleftAttachment,   XmATTACH_WIDGET,
                              XmNleftWidget,       file_radio_w,
                              XmNrightAttachment,  XmATTACH_WIDGET,
                              XmNrightOffset,      5,
                              XmNbottomAttachment, XmATTACH_FORM,
                              NULL);
      XtAddCallback(file_text_w, XmNlosingFocusCallback, save_file_name, 0);
      XtSetSensitive(file_text_w, False);
      XtManageChild(inputline_w);
      XtManageChild(form_w);
      XtManageChild(criteriabox_w);
      XtManageChild(main_form_w);

#ifdef _EDITRES
      XtAddEventHandler(printshell, (EventMask)0, True, _XEditResCheckMessages, NULL);
#endif
   }
   XtPopup(printshell, XtGrabNone);

   return;
}


/*+++++++++++++++++++++++++ print_data_button() +++++++++++++++++++++++++*/
static void
print_data_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   char message[MAX_MESSAGE_LENGTH];

   if (range_type == SELECTION_TOGGLE)
   {
      int no_selected,
          *select_list;

      if (XmListGetSelectedPos(listbox_w, &select_list, &no_selected) == False)
      {
         show_message("No data selected for printing!");
         XtPopdown(printshell);

         return;
      }
      else
      {
         register int  i,
                       length;
         int           fd,
                       prepare_status;
         char          *line,
                       line_buffer[256];
         XmStringTable all_items;

         if (device_type == PRINTER_TOGGLE)
         {
            prepare_status = prepare_printer(&fd);
         }
         else
         {
            prepare_status = prepare_file(&fd);
         }
         if (prepare_status == SUCCESS)
         {
            write_header(fd);

            XtVaGetValues(listbox_w, XmNitems, &all_items, NULL);
            for (i = 0; i < no_selected; i++)
            {
               XmStringGetLtoR(all_items[select_list[i] - 1],
                               XmFONTLIST_DEFAULT_TAG, &line);
               length = sprintf(line_buffer, "%s\n", line);
               if (write(fd, line_buffer, length) != length)
               {
                  (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
                  XtFree(line);
                  exit(INCORRECT);
               }
               XtFree(line);
               XmListDeselectPos(listbox_w, select_list[i]);
            }

            /*
             * Remember to insert the correct summary, since all files
             * have now been deselected.
             */
            write_summary(fd);
            (void)strcpy(summary_str, total_summary_str);
            XmTextSetString(summarybox_w, summary_str);

            if (device_type == PRINTER_TOGGLE)
            {
               int status;

               /* Send Control-D to printer queue */
               (void)strcpy(line, CONTROL_D);
               length = strlen(line);
               if (write(fd, line, length) != length)
               {
                  (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
                  XtFree(line);
                  exit(INCORRECT);
               }

               if ((status = pclose(fp)) < 0)
               {
                  (void)sprintf(message,
                                "Failed to send printer command (%d) : %s",
                                status, strerror(errno));
               }
               else
               {
                  (void)sprintf(message, "Send job to printer (%d)", status);
               }
            }
            else
            {
               if (close(fd) < 0)
               {
                  (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
               }
               (void)sprintf(message, "Send job to file %s.", file_name);
            }
         }
         XtFree((char *)select_list);
      }
   }
   else /* Print everything! */
   {
      register int  i,
                    length;
      int           fd,
                    no_of_items,
                    prepare_status;
      char          *line,
                    line_buffer[256];
      XmStringTable all_items;

      if (device_type == PRINTER_TOGGLE)
      {
         prepare_status = prepare_printer(&fd);
      }
      else
      {
         prepare_status = prepare_file(&fd);
      }
      if (prepare_status == SUCCESS)
      {
         write_header(fd);

         XtVaGetValues(listbox_w,
                       XmNitemCount, &no_of_items,
                       XmNitems,     &all_items,
                       NULL);
         for (i = 0; i < no_of_items; i++)
         {
            XmStringGetLtoR(all_items[i], XmFONTLIST_DEFAULT_TAG, &line);
            length = sprintf(line_buffer, "%s\n", line);
            if (write(fd, line_buffer, length) != length)
            {
               (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               XtFree(line);
               exit(INCORRECT);
            }
            XtFree(line);
         }
         write_summary(fd);

         if (device_type == PRINTER_TOGGLE)
         {
            int status;

            /* Send Control-D to printer queue */
            (void)strcpy(line, CONTROL_D);
            length = strlen(line);
            if (write(fd, line, length) != length)
            {
               (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               XtFree(line);
               exit(INCORRECT);
            }

            if ((status = pclose(fp)) < 0)
            {
               (void)sprintf(message,
                             "Failed to send printer command (%d) : %s",
                             status, strerror(errno));
            }
            else
            {
               (void)sprintf(message, "Send job to printer (%d)", status);
            }
         }
         else
         {
            if (close(fd) < 0)
            {
               (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
            }
            (void)sprintf(message, "Send job to file %s.", file_name);
         }
      }
   }

   show_message(message);
   XtPopdown(printshell);

   return;
}


/*-------------------------- prepare_printer() --------------------------*/
static int
prepare_printer(int *fd)
{
   char cmd[PRINTER_INFO_LENGTH + PRINTER_INFO_LENGTH + 1];

   (void)strcpy(cmd, printer_cmd);
   (void)strcat(cmd, printer_name);
   (void)strcat(cmd, " > /dev/null");

   if ((fp = popen(cmd, "w")) == NULL)
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Failed to send printer command %s : %s (%s %d)",
                 cmd, strerror(errno), __FILE__, __LINE__);
      XtPopdown(printshell);

      return(INCORRECT);
   }

   *fd = fileno(fp);

   return(SUCCESS);
}


/*-------------------------- prepare_file() -----------------------------*/
static int
prepare_file(int *fd)
{
   if ((*fd = open(file_name, (O_RDWR | O_CREAT | O_TRUNC), FILE_MODE)) < 0)
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Failed to open() %s : %s (%s %d)",
                 file_name, strerror(errno), __FILE__, __LINE__);
      XtPopdown(printshell);

      return(INCORRECT);
   }

   return(SUCCESS);
}


/*--------------------------- write_header() ----------------------------*/
static void
write_header(int fd)
{
   int  length;
   char buffer[1024];

   if ((start_time_val < 0) && (end_time_val < 0))
   {
      length = sprintf(buffer, "                                AFD INPUT LOG\n\n\
                Time Interval : earliest entry - latest entry\n\
                File name     : %s\n\
                File size     : %s\n\
                Directory     : %s\n",
                       search_file_name, search_file_size_str,
                       search_directory_name);
   }
   else if ((start_time_val > 0) && (end_time_val < 0))
        {
           length = strftime(buffer, 1024, "                                AFD INPUT LOG\n\n\tTime Interval : %m.%d. %H:%M",
                             gmtime(&start_time_val));
           length += sprintf(&buffer[length], " - latest entry\n\
                File name     : %s\n\
                File size     : %s\n\
                Directory     : %s\n",
                             search_file_name, search_file_size_str,
                             search_directory_name);
        }
        else if ((start_time_val < 0) && (end_time_val > 0))
             {
                length = strftime(buffer, 1024, "                                AFD INPUT LOG\n\n\tTime Interval : earliest entry - %m.%d. %H:%M",
                                  gmtime(&end_time_val));
                length += sprintf(&buffer[length], "\n\
                File name     : %s\n\
                File size     : %s\n\
                Directory     : %s\n",
                                  search_file_name, search_file_size_str,
                                  search_directory_name);
             }
             else
             {
                length = strftime(buffer, 1024, "                                AFD INPUT LOG\n\n\tTime Interval : %m.%d. %H:%M",
                                  gmtime(&start_time_val));
                length += strftime(&buffer[length], 1024 - length,
                                   " - %m.%d. %H:%M",
                                   gmtime(&end_time_val));
                length += sprintf(&buffer[length], "\n\
                File name     : %s\n\
                File size     : %s\n\
                Directory     : %s\n",
                                 search_file_name, search_file_size_str,
                                 search_directory_name);
             }

   if (no_of_search_hosts > 0)
   {
      int i;

      length += sprintf(&buffer[length], "                Host name     : %s",
                        search_recipient[0]);
      for (i = 1; i < no_of_search_hosts; i++)
      {
         length += sprintf(&buffer[length], ", %s", search_recipient[i]);
      }
      length += sprintf(&buffer[length], "\n\n");
   }
   else
   {
      length += sprintf(&buffer[length], "                Host name     : \n\n");
   }

   /* Don't forget the heading for the data. */
   if (file_name_length == SHOW_SHORT_FORMAT)
   {
      length += sprintf(&buffer[length], "%s\n%s\n",
                        HEADING_LINE_SHORT, SUM_SEP_LINE_SHORT);
   }
   else
   {
      length += sprintf(&buffer[length], "%s\n%s\n",
                        HEADING_LINE_LONG, SUM_SEP_LINE_LONG);
   }

   /* Write heading to file/printer. */
   if (write(fd, buffer, length) != length)
   {
      (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   return;
}


/*-------------------------- write_summary() ----------------------------*/
static void
write_summary(int fd)
{
   int  length;
   char buffer[1024];

   if (file_name_length == SHOW_SHORT_FORMAT)
   {
      length = sprintf(buffer, "%s\n", SUM_SEP_LINE_SHORT);
   }
   else
   {
      length = sprintf(buffer, "%s\n", SUM_SEP_LINE_LONG);
   }
   length += sprintf(&buffer[length], "%s\n", summary_str);

   /* Write summary to file/printer. */
   if (write(fd, buffer, length) != length)
   {
      (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   return;
}


/*+++++++++++++++++++++++ cancel_print_button() +++++++++++++++++++++++++*/
static void
cancel_print_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   XtPopdown(printshell);

   return;
}


/*+++++++++++++++++++++++++++ range_select() ++++++++++++++++++++++++++++*/
static void
range_select(Widget w, XtPointer client_data, XtPointer call_data)
{
   range_type = (int)client_data;

   return;
}


/*+++++++++++++++++++++++++++ device_select() +++++++++++++++++++++++++++*/
static void
device_select(Widget w, XtPointer client_data, XtPointer call_data)
{
   device_type = (int)client_data;

   if (device_type == PRINTER_TOGGLE)
   {
      XtVaSetValues(file_radio_w, XmNset, False, NULL);
      XtSetSensitive(file_text_w, False);
      XtVaSetValues(printer_radio_w, XmNset, True, NULL);
      XtSetSensitive(printer_text_w, True);
   }
   else
   {
      XtVaSetValues(printer_radio_w, XmNset, False, NULL);
      XtSetSensitive(printer_text_w, False);
      XtVaSetValues(file_radio_w, XmNset, True, NULL);
      XtSetSensitive(file_text_w, True);
   }

   return;
}


/*+++++++++++++++++++++++++ save_printer_name() +++++++++++++++++++++++++*/
static void
save_printer_name(Widget w, XtPointer client_data, XtPointer call_data)
{
   char *value = XmTextGetString(w);

   (void)strcpy(printer_name, value);

   return;
}


/*++++++++++++++++++++++++++ save_file_name() +++++++++++++++++++++++++++*/
static void
save_file_name(Widget w, XtPointer client_data, XtPointer call_data)
{
   char *value = XmTextGetString(w);

   (void)strcpy(file_name, value);

   return;
}

/*
 *  print_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   18.03.2000 H.Kiehl Modified to make it more generic.
 **
 */
DESCR__E_M3

#include <stdio.h>               /* sprintf(), popen(), pclose()         */
#include <string.h>              /* strcpy(), strcat(), strerror()       */
#include <stdlib.h>              /* exit()                               */
#include <unistd.h>              /* write(), close()                     */
#include <time.h>                /* strftime()                           */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/List.h>
#include <errno.h>
#include "afd_ctrl.h"
#include "show_ilog.h"

/* External global variables */
extern Widget  listbox_w,
               printshell,
               statusbox_w,
               summarybox_w;
extern char    file_name[],
               search_file_name[],
               search_directory_name[],
               **search_recipient,
               search_file_size_str[],
               summary_str[],
               total_summary_str[];
extern int     device_type,
               file_name_length,
               no_of_search_hosts,
               range_type,
               toggles_set;
extern time_t  start_time_val,
               end_time_val;
extern FILE    *fp;

/* Local function prototypes. */
static void    write_header(int),
               write_summary(int);


/*######################### print_data_button() #########################*/
void
print_data_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   char message[MAX_MESSAGE_LENGTH];

   if (range_type == SELECTION_TOGGLE)
   {
      int no_selected,
          *select_list;

      if (XmListGetSelectedPos(listbox_w, &select_list, &no_selected) == False)
      {
         show_message(statusbox_w, "No data selected for printing!");
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
               int  status;
               char buf;

               /* Send Control-D to printer queue */
               buf = CONTROL_D;
               if (write(fd, &buf, 1) != 1)
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
               if (device_type == MAIL_TOGGLE)
               {
                  send_mail_cmd(message);
               }
               else
               {
                  (void)sprintf(message, "Send job to file %s.", file_name);
               }
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
            int  status;
            char buf;

            /* Send Control-D to printer queue */
            buf = CONTROL_D;
            if (write(fd, &buf, 1) != 1)
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
            if (device_type == MAIL_TOGGLE)
            {
               send_mail_cmd(message);
            }
            else
            {
               (void)sprintf(message, "Send job to file %s.", file_name);
            }
         }
      }
   }

   show_message(statusbox_w, message);
   XtPopdown(printshell);

   return;
}


/*+++++++++++++++++++++++++++ write_header() ++++++++++++++++++++++++++++*/
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
                             localtime(&start_time_val));
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
                                  localtime(&end_time_val));
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
                                  localtime(&start_time_val));
                length += strftime(&buffer[length], 1024 - length,
                                   " - %m.%d. %H:%M",
                                   localtime(&end_time_val));
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


/*+++++++++++++++++++++++++++ write_summary() +++++++++++++++++++++++++++*/
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

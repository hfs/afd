/*
 *  callbacks.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   callbacks - all callback functions for module show_olog
 **
 ** SYNOPSIS
 **   void radio_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void item_selection(Widget w, XtPointer client_data, XtPointer call_data)
 **   void info_click(Widget w, XtPointer client_data, XEvent *event)
 **   void search_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void resend_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void close_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void save_input(Widget w, XtPointer client_data, XtPointer call_data)
 **   void scrollbar_moved(Widget w, XtPointer client_data, XtPointer call_data)
 **
 ** DESCRIPTION
 **   The famous 'AFD Info Click' is done by info_click(). When clicking on
 **   an item with the middle or right mouse button in the list widget,
 **   it list the following information: file name, directory, filter,
 **   recipient, AMG-options, FD-options, priority, and job ID.
 **
 **   Function item_selection() calculates a new summary string of
 **   the items that are currently selected and displays them.
 **
 **   search_button() activates the search in the output log. When
 **   pressed the label of the button changes to 'Stop'. Now the user
 **   has the chance to stop the search. During the search only the
 **   list widget and the Stop button can be used.
 **
 **   close_button() will terminate the program.
 **
 **   save_input() evaluates the input for start and end time.
 **
 **   scrollbar_moved() sets a flag that the scrollbar has been moved so
 **   we do NOT position to the last item in the list.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.05.1997 H.Kiehl Created
 **   15.02.1999 H.Kiehl Multiple recipients.
 **   23.11.2003 H.Kiehl Disallow user to change window width even if
 **                      window manager allows this, but allow to change
 **                      height.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <ctype.h>          /* isdigit()                                 */
#include <stdlib.h>         /* atoi(), atol(), free()                    */
#include <ctype.h>          /* isdigit()                                 */
#include <time.h>           /* time(), localtime(), mktime(), strftime() */
#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#endif
#include <unistd.h>         /* close()                                   */
#include <Xm/Xm.h>
#include <Xm/List.h>
#include <Xm/Text.h>
#include <Xm/LabelP.h>
#include <errno.h>
#include "afd_ctrl.h"
#include "show_ilog.h"

/* External global variables */
extern Display          *display;
extern Widget           cont_togglebox_w,
                        directory_w,
                        end_time_w,
                        file_length_w,
                        file_name_w,
                        headingbox_w,
                        listbox_w,
                        print_button_w,
                        radiobox_w,
                        recipient_w,
                        resend_button_w,
                        send_button_w,
                        start_time_w,
                        statusbox_w,
                        summarybox_w,
                        appshell;
extern Window           main_window;
extern int              continues_toggle_set,
                        file_name_length,
                        no_of_search_dirs,
                        no_of_search_dirids,
                        no_of_search_hosts,
                        special_button_flag,
                        sum_line_length,
                        no_of_log_files,
                        char_width;
extern time_t           start_time_val,
                        end_time_val;
extern size_t           search_file_size;
extern char             header_line[],
                        search_file_name[],
                        **search_dir,
                        **search_dirid,
                        **search_recipient,
                        **search_user;
extern struct item_list *il;

/* Global variables. */
int                     gt_lt_sign,
                        max_x,
                        max_y;
char                    search_file_size_str[20],
                        summary_str[MAX_OUTPUT_LINE_LENGTH + SHOW_LONG_FORMAT + 5],
                        total_summary_str[MAX_OUTPUT_LINE_LENGTH + SHOW_LONG_FORMAT + 5];
struct info_data        id;

/* Local global variables */
static int              scrollbar_moved_flag;

/* Local function prototypes */
static int              eval_time(char *, Widget, time_t *);


/*########################## continues_toggle() #########################*/
void
continues_toggle(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (continues_toggle_set == NO)
   {
      continues_toggle_set = YES;
   }
   else
   {
      continues_toggle_set = NO;
   }
   return;
}


/*########################### radio_button() ############################*/
void
radio_button(Widget w, XtPointer client_data, XtPointer call_data)
{
#if SIZEOF_LONG == 4
   int new_file_name_length = (int)client_data;
#else
   union intlong
         {
            int  ival[2];
            long lval;
         } il;
   int   byte_order = 1,
         new_file_name_length;

   il.lval = (long)client_data;
   if (*(char *)&byte_order == 1)
   {
      new_file_name_length = il.ival[0];
   }
   else
   {
      new_file_name_length = il.ival[1];
   }
#endif

   if (new_file_name_length != file_name_length)
   {
      Window       root_return;
      int          x,
                   y,
                   no_of_items;
      Dimension    window_width;
      unsigned int width,
                   window_height,
                   border,
                   depth;

      file_name_length = new_file_name_length;

      XGetGeometry(display, main_window, &root_return, &x, &y, &width, &window_height, &border, &depth);
      sum_line_length = sprintf(header_line, "%s%-*s %s",
                                DATE_TIME_HEADER, file_name_length,
                                FILE_NAME_HEADER, REST_HEADER);
      XmTextSetString(headingbox_w, header_line);

      window_width = char_width *
                     (MAX_OUTPUT_LINE_LENGTH + file_name_length + 6);
      XtVaSetValues(appshell,
                    XmNminWidth, window_width,
                    XmNmaxWidth, window_width,
                    NULL);
      XResizeWindow(display, main_window, window_width, window_height);

      XtVaGetValues(listbox_w, XmNitemCount, &no_of_items, NULL);
      if (no_of_items > 0)
      {
         scrollbar_moved_flag = NO;
         XmListDeleteAllItems(listbox_w);
         get_data();

         /* Only position to last item when scrollbar was NOT moved! */
         if (scrollbar_moved_flag == NO)
         {
            XmListSetBottomPos(listbox_w, 0);
         }
      }
   }

   return;
}


/*########################## item_selection() ###########################*/
void
item_selection(Widget w, XtPointer client_data, XtPointer call_data)
{
   static time_t        first_date_found,
                        last_date_found;
   static unsigned int  total_no_files;
   static double        file_size;
   XmListCallbackStruct *cbs = (XmListCallbackStruct *)call_data;

   if (cbs->reason == XmCR_EXTENDED_SELECT)
   {
      int    i;
      time_t date;
      double current_file_size;

      total_no_files = cbs->selected_item_count;
      file_size = 0.0;
      first_date_found = -1;
      for (i = 0; i < cbs->selected_item_count; i++)
      {
         if (get_sum_data((cbs->selected_item_positions[i] - 1),
                          &date, &current_file_size) == INCORRECT)
         {
            return;
         }
         if (first_date_found == -1)
         {
            first_date_found = date;
         }
         file_size += current_file_size;
      }
      last_date_found = date;

      /* Show summary */
      if (cbs->selected_item_count > 0)
      {
         calculate_summary(summary_str, first_date_found, last_date_found,
                           total_no_files, file_size);
      }
      else
      {
         (void)strcpy(summary_str, total_summary_str);
      }
      SHOW_SUMMARY_DATA();
   }

   return;
}


/*############################ info_click() #############################*/
void
info_click(Widget w, XtPointer client_data, XEvent *event)
{
   if ((event->xbutton.button == Button2) ||
       (event->xbutton.button == Button3))
   {
      int  pos = XmListYToPos(w, event->xbutton.y),
           max_pos;

      /* Check if pos is valid */
      XtVaGetValues(w, XmNitemCount, &max_pos, NULL);
      if ((max_pos > 0) && (pos <= max_pos))
      {
         int  i;
         char *text;

         /* Initialize text an data area. */
         id.count  = 0;
         id.dir[0] = '\0';
         id.dbe    = NULL;

         /* Get the information */
         get_info(pos);

         /* Format information in a human readable text. */
         format_info(&text);

         /* Show the information */
         show_info(text);

         free(text);

         /* Free all memory that was allocated in get_info(). */
         for (i = 0; i < id.count; i++)
         {
            free(id.dbe[i].files);
            if (id.dbe[i].soptions != NULL)
            {
               free((void *)id.dbe[i].soptions);
               id.dbe[i].soptions = NULL;
            }
         }
         if (id.count > 0)
         {
            free((void *)id.dbe);
            id.dbe = NULL;
            id.count = 0;
         }
      }
   }

   return;
}


/*######################### scrollbar_moved() ###########################*/
void
scrollbar_moved(Widget w, XtPointer client_data, XtPointer call_data)
{
   scrollbar_moved_flag = YES;

   return;
}


/*########################## search_button() ############################*/
void
search_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (special_button_flag == SEARCH_BUTTON)
   {
      XtSetSensitive(cont_togglebox_w, False);
      XtSetSensitive(start_time_w, False);
      XtSetSensitive(end_time_w, False);
      XtSetSensitive(file_name_w, False);
      XtSetSensitive(directory_w, False);
      XtSetSensitive(file_length_w, False);
      XtSetSensitive(recipient_w, False);
      XtSetSensitive(radiobox_w, False);
      XtSetSensitive(print_button_w, False);

      scrollbar_moved_flag = NO;
      XmListDeleteAllItems(listbox_w);
      get_data();

      /* Only position to last item when scrollbar was NOT moved! */
      if (scrollbar_moved_flag == NO)
      {
         XmListSetBottomPos(listbox_w, 0);
      }
   }
   else
   {
      set_sensitive();
      special_button_flag = STOP_BUTTON_PRESSED;
   }

   return;
}


/*########################### set_sensitive() ###########################*/
void
set_sensitive(void)
{
   XtSetSensitive(cont_togglebox_w, True);
   XtSetSensitive(start_time_w, True);
   XtSetSensitive(end_time_w, True);
   XtSetSensitive(file_name_w, True);
   XtSetSensitive(directory_w, True);
   XtSetSensitive(file_length_w, True);
   XtSetSensitive(recipient_w, True);
   XtSetSensitive(radiobox_w, True);
   XtSetSensitive(print_button_w, True);

   return;
}


/*############################ print_button() ###########################*/
void
print_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   reset_message(statusbox_w);
   print_data();

   return;
}


/*########################### close_button() ############################*/
void
close_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   exit(0);
}


/*############################# save_input() ############################*/
void
save_input(Widget w, XtPointer client_data, XtPointer call_data)
{
   int         extra_sign = 0;
   XT_PTR_TYPE type = (XT_PTR_TYPE)client_data;
   char        *ptr,
               *value = XmTextGetString(w);

   switch (type)
   {
      case START_TIME_NO_ENTER : 
         if (value[0] == '\0')
         {
            start_time_val = -1;
         }
         else if (eval_time(value, w, &start_time_val) < 0)
              {
                 show_message(statusbox_w, TIME_FORMAT);
                 XtFree(value);
                 return;
              }
         reset_message(statusbox_w);
         break;

      case START_TIME :
         if (eval_time(value, w, &start_time_val) < 0)
         {
            show_message(statusbox_w, TIME_FORMAT);
         }
         else
         {
            reset_message(statusbox_w);
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case END_TIME_NO_ENTER : 
         if (value[0] == '\0')
         {
            end_time_val = -1;
         }
         else if (eval_time(value, w, &end_time_val) < 0)
              {
                 show_message(statusbox_w, TIME_FORMAT);
                 XtFree(value);
                 return;
              }
         reset_message(statusbox_w);
         break;

      case END_TIME :
         if (eval_time(value, w, &end_time_val) < 0)
         {
            show_message(statusbox_w, TIME_FORMAT);
         }
         else
         {
            reset_message(statusbox_w);
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case FILE_NAME_NO_ENTER :
         (void)strcpy(search_file_name, value);
         break;

      case FILE_NAME :
         (void)strcpy(search_file_name, value);
         reset_message(statusbox_w);
         XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         break;

      case DIRECTORY_NAME_NO_ENTER :
      case DIRECTORY_NAME :
         {
            int is_dir_id,
                length,
                max_dir_length = 0,
                max_dirid_length = 0;

            if (no_of_search_dirs != 0)
            {
               FREE_RT_ARRAY(search_dir);
               no_of_search_dirs = 0;
            }
            if (no_of_search_dirids != 0)
            {
               FREE_RT_ARRAY(search_dirid);
               no_of_search_dirids = 0;
            }
            ptr = value;
            for (;;)
            {
               while (*ptr == ' ')
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  ptr++;
               }
               if (*ptr == '\0')
               {
                  if (ptr == value)
                  {
                     no_of_search_dirs = 0;
                     no_of_search_dirids = 0;
                  }
                  break;
               }
               if (*ptr == '#')
               {
                  is_dir_id = YES;
                  ptr++;
               }
               else
               {
                  is_dir_id = NO;
               }
               length = 0;
               while ((*ptr != '\0') && (*ptr != ','))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  ptr++; length++;
               }
               if (is_dir_id == YES)
               {
                  no_of_search_dirids++;
                  if (length > max_dirid_length)
                  {
                     max_dirid_length = length;
                  }
               }
               else
               {
                  no_of_search_dirs++;
                  if (length > max_dir_length)
                  {
                     max_dir_length = length;
                  }
               }
               if (*ptr == '\0')
               {
                  if (ptr == value)
                  {
                     no_of_search_dirs = 0;
                     no_of_search_dirids = 0;
                  }
                  break;
               }
               ptr++;
            }
            if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0))
            {
               int  ii_dirs = 0,
                    ii_dirids = 0,
                    *p_ii;
               char *p_dir;

               if (no_of_search_dirs > 0)
               {
                  RT_ARRAY(search_dir, no_of_search_dirs,
                           (max_dir_length + 1), char);
               }
               if (no_of_search_dirids > 0)
               {
                  RT_ARRAY(search_dirid, no_of_search_dirids,
                           (max_dirid_length + 1), char);
               }

               ptr = value;
               for (;;)
               {
                  while ((*ptr == ' ') || (*ptr == '\t'))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     ptr++;
                  }
                  if (*ptr == '#')
                  {
                     p_ii =  &ii_dirids;
                     p_dir = search_dirid[ii_dirids];
                     ptr++;
                  }
                  else
                  {
                     p_ii =  &ii_dirs;
                     p_dir = search_dir[ii_dirs];
                  }
                  while ((*ptr != '\0') && (*ptr != ','))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     *p_dir = *ptr;
                     ptr++; p_dir++;
                  }
                  *p_dir = '\0';
                  if (*ptr == ',')
                  {
                     ptr++;
                     (*p_ii)++;
                  }
                  else
                  {
                     break;
                  }
               } /* for (;;) */
            }
         }
         reset_message(statusbox_w);
         if (type == DIRECTORY_NAME)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case FILE_LENGTH_NO_ENTER :
      case FILE_LENGTH :
         if (value[0] == '\0')
         {
            search_file_size = -1;
         }
         else
         {
            if (isdigit((int)value[0]))
            {
               gt_lt_sign = EQUAL_SIGN;
            }
            else if (value[0] == '=')
                 {
                    extra_sign = 1;
                    gt_lt_sign = EQUAL_SIGN;
                 }
            else if (value[0] == '<')
                 {
                    extra_sign = 1;
                    gt_lt_sign = LESS_THEN_SIGN;
                 }
            else if (value[0] == '>')
                 {
                    extra_sign = 1;
                    gt_lt_sign = GREATER_THEN_SIGN;
                 }
                 else
                 {
                    show_message(statusbox_w, FILE_SIZE_FORMAT);
                    XtFree(value);
                    return;
                 }
            search_file_size = (size_t)atol(value + extra_sign);
            (void)strcpy(search_file_size_str, value);
         }
         reset_message(statusbox_w);
         if (type == FILE_LENGTH)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case RECIPIENT_NAME_NO_ENTER : /* Read the recipient */
      case RECIPIENT_NAME : /* Read the recipient */
         {
            int  i = 0,
                 ii = 0;
            char *ptr_start;

            if (no_of_search_hosts != 0)
            {
               FREE_RT_ARRAY(search_recipient);
               FREE_RT_ARRAY(search_user);
               no_of_search_hosts = 0;
            }
            ptr = value;
            for (;;)
            {
               while ((*ptr != '\0') &&
                      (*ptr != ','))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  ptr++;
               }
               no_of_search_hosts++;
               if (*ptr == '\0')
               {
                  if (ptr == value)
                  {
                     no_of_search_hosts = 0;
                  }
                  break;
               }
               ptr++;
            }
            if (no_of_search_hosts > 0)
            {
               RT_ARRAY(search_recipient, no_of_search_hosts,
                        (MAX_RECIPIENT_LENGTH + 1), char);
               RT_ARRAY(search_user, no_of_search_hosts,
                        (MAX_RECIPIENT_LENGTH + 1), char);

               ptr = value;
               for (;;)
               {
                  ptr_start = ptr;
                  i = 0;
                  while ((*ptr != '\0') &&
                         (*ptr != '@') &&
                         (*ptr != ','))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     search_user[ii][i] = *ptr;
                     ptr++; i++;
                  }
                  if (*ptr == '@')
                  {
                     ptr++;
                     search_user[ii][i] = '\0';
                     ptr_start = ptr;;
                     while ((*ptr != '\0') &&
                            (*ptr != ','))
                     {
                        if (*ptr == '\\')
                        {
                           ptr++;
                        }
                        ptr++;
                     }
                  }
                  else
                  {
                     search_user[ii][0] = '\0';
                  }
                  if (*ptr == ',')
                  {
                     *ptr = '\0';
                     (void)strcpy(search_recipient[ii], ptr_start);
                     ii++; ptr++;
                     while ((*ptr == ' ') || (*ptr == '\t'))
                     {
                        ptr++;
                     }
                  }
                  else
                  {
                     (void)strcpy(search_recipient[ii], ptr_start);
                     break;
                  }
               } /* for (;;) */
            } /* if (no_of_search_hosts > 0) */
         }
         reset_message(statusbox_w);
         if (type == RECIPIENT_NAME)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      default :
         (void)fprintf(stderr, "ERROR   : Impossible! (%s %d)\n",
                       __FILE__, __LINE__);
         exit(INCORRECT);
   }
   XtFree(value);

   return;
}


/*++++++++++++++++++++++++++++ eval_time() ++++++++++++++++++++++++++++++*/
static int
eval_time(char *numeric_str, Widget w, time_t *value)
{
   int    length = strlen(numeric_str),
          min,
          hour;
   time_t time_val;
   char   str[3];

   time_val = time(NULL);

   switch (length)
   {
      case 0 : /* Assume user means current time */
               {
                  char time_str[9];

                  (void)strftime(time_str, 9, "%m%d%H%M", localtime(&time_val));
                  XmTextSetString(w, time_str);
               }
               return(time_val);
      case 3 :
      case 4 :
      case 5 :
      case 6 :
      case 7 :
      case 8 : break;
      default: return(INCORRECT);
   }

   if (numeric_str[0] == '-')
   {
      if ((!isdigit((int)numeric_str[1])) || (!isdigit((int)numeric_str[2])))
      {
         return(INCORRECT);
      }

      if (length == 3)
      {
         str[0] = numeric_str[1];
         str[1] = numeric_str[2];
         str[2] = '\0';
         min = atoi(str);
         if ((min < 0) || (min > 59))
         {
            return(INCORRECT);
         }

         *value = time_val - (min * 60);
      }
      else if (length == 5) 
           {
              if ((!isdigit((int)numeric_str[3])) ||
                  (!isdigit((int)numeric_str[4])))
              {
                 return(INCORRECT);
              }

              str[0] = numeric_str[1];
              str[1] = numeric_str[2];
              str[2] = '\0';
              hour = atoi(str);
              if ((hour < 0) || (hour > 23))
              {
                 return(INCORRECT);
              }
              str[0] = numeric_str[3];
              str[1] = numeric_str[4];
              min = atoi(str);
              if ((min < 0) || (min > 59))
              {
                 return(INCORRECT);
              }

              *value = time_val - (min * 60) - (hour * 3600);
           }
      else if (length == 7)
           {
              int days;

              if ((!isdigit((int)numeric_str[3])) ||
                  (!isdigit((int)numeric_str[4])) ||
                  (!isdigit((int)numeric_str[5])) ||
                  (!isdigit((int)numeric_str[6])))
              {
                 return(INCORRECT);
              }

              str[0] = numeric_str[1];
              str[1] = numeric_str[2];
              str[2] = '\0';
              days = atoi(str);
              str[0] = numeric_str[3];
              str[1] = numeric_str[4];
              str[2] = '\0';
              hour = atoi(str);
              if ((hour < 0) || (hour > 23))
              {
                 return(INCORRECT);
              }
              str[0] = numeric_str[5];
              str[1] = numeric_str[6];
              min = atoi(str);
              if ((min < 0) || (min > 59))
              {
                 return(INCORRECT);
              }

              *value = time_val - (min * 60) - (hour * 3600) - (days * 86400);
           }
           else
           {
              return(INCORRECT);
           }

      return(SUCCESS);
   }

   if ((!isdigit((int)numeric_str[0])) || (!isdigit((int)numeric_str[1])) ||
       (!isdigit((int)numeric_str[2])) || (!isdigit((int)numeric_str[3])))
   {
      return(INCORRECT);
   }

   str[0] = numeric_str[0];
   str[1] = numeric_str[1];
   str[2] = '\0';

   if (length == 4) /* hhmm */
   {
      struct tm *bd_time;     /* Broken-down time */

      hour = atoi(str);
      if ((hour < 0) || (hour > 23))
      {
         return(INCORRECT);
      }
      str[0] = numeric_str[2];
      str[1] = numeric_str[3];
      min = atoi(str);
      if ((min < 0) || (min > 59))
      {
         return(INCORRECT);
      }
      bd_time = localtime(&time_val);
      bd_time->tm_sec  = 0;
      bd_time->tm_min  = min;
      bd_time->tm_hour = hour;

      *value = mktime(bd_time);
   }
   else if (length == 6) /* DDhhmm */
        {
           int       day = atoi(str);
           struct tm *bd_time;     /* Broken-down time */

           if ((day < 0) || (day > 31))
           {
              return(INCORRECT);
           }
           str[0] = numeric_str[2];
           str[1] = numeric_str[3];
           hour = atoi(str);
           if ((hour < 0) || (hour > 23))
           {
              return(INCORRECT);
           }
           str[0] = numeric_str[4];
           str[1] = numeric_str[5];
           min = atoi(str);
           if ((min < 0) || (min > 59))
           {
              return(INCORRECT);
           }
           bd_time = localtime(&time_val);
           bd_time->tm_sec  = 0;
           bd_time->tm_min  = min;
           bd_time->tm_hour = hour;
           bd_time->tm_mday = day;

           *value = mktime(bd_time);
        }
        else /* MMDDhhmm */
        {
           int       month = atoi(str),
                     day;
           struct tm *bd_time;     /* Broken-down time */

           if ((month < 0) || (month > 12))
           {
              return(INCORRECT);
           }
           str[0] = numeric_str[2];
           str[1] = numeric_str[3];
           day = atoi(str);
           if ((day < 0) || (day > 31))
           {
              return(INCORRECT);
           }
           str[0] = numeric_str[4];
           str[1] = numeric_str[5];
           hour = atoi(str);
           if ((hour < 0) || (hour > 23))
           {
              return(INCORRECT);
           }
           str[0] = numeric_str[6];
           str[1] = numeric_str[7];
           min = atoi(str);
           if ((min < 0) || (min > 59))
           {
              return(INCORRECT);
           }
           bd_time = localtime(&time_val);
           bd_time->tm_sec  = 0;
           bd_time->tm_min  = min;
           bd_time->tm_hour = hour;
           bd_time->tm_mday = day;
           if ((bd_time->tm_mon == 0) && (month == 12))
           {
              bd_time->tm_year -= 1;
           }
           bd_time->tm_mon  = month - 1;

           *value = mktime(bd_time);
        }

   return(SUCCESS);
}

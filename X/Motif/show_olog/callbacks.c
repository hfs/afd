/*
 *  callbacks.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2000 Deutscher Wetterdienst (DWD),
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

DESCR__S_M3
/*
 ** NAME
 **   callbacks - all callback functions for module show_olog
 **
 ** SYNOPSIS
 **   void toggled(Widget w, XtPointer client_data, XtPointer call_data)
 **   void file_name_toggle(Widget w, XtPointer client_data, XtPointer call_data)
 **   void radio_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void item_selection(Widget w, XtPointer client_data, XtPointer call_data)
 **   void info_click(Widget w, XtPointer client_data, XEvent *event)
 **   void search_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void resend_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void send_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void close_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void save_input(Widget w, XtPointer client_data, XtPointer call_data)
 **   void scrollbar_moved(Widget w, XtPointer client_data, XtPointer call_data)
 **
 ** DESCRIPTION
 **   The function toggled() is used to set the bits in the global
 **   variable toggles_set. The following bits can be set: SHOW_FTP,
 **   SHOW_SMTP and SHOW_FILE.
 **
 **   file_name_toggle() sets the variable file_name_toggle_set either
 **   to local or remote and sets the label of the toggle.
 **
 **   Function item_selection() calculates a new summary string of
 **   the items that are currently selected and displays them.
 **
 **   The famous 'AFD Info Click' is done by info_click(). When clicking on
 **   an item with the middle or right mouse button in the list widget,
 **   it list the following information: file name, directory, filter,
 **   recipient, AMG-options, FD-options, priority, job ID and archive
 **   directory.
 **
 **   search_button() activates the search in the output log. When
 **   pressed the label of the button changes to 'Stop'. Now the user
 **   has the chance to stop the search. During the search only the
 **   list widget and the Stop button can be used.
 **
 **   resend_button() will resend all selected files. As in search_button()
 **   during the resending the search button turns into a stop button,
 **   which can be used to terminate the process. By pressing the resend
 **   button again, it continues the resending at that point where it
 **   was stopped.
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
 **   31.03.1997 H.Kiehl Created
 **   13.02.1999 H.Kiehl Multiple recipients.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <ctype.h>             /* isdigit()                              */
#include <stdlib.h>            /* atoi(), atol(), free()                 */
#include <ctype.h>             /* isdigit()                              */
#include <time.h>              /* time(), gmtime(), mktime(), strftime() */
#include <unistd.h>            /* close()                                */
#include <Xm/Xm.h>
#include <Xm/List.h>
#include <Xm/Text.h>
#include <errno.h>
#include "afd_ctrl.h"
#include "show_olog.h"

/* External global variables */
extern Display          *display;
extern Widget           listbox_w,
                        headingbox_w,
                        statusbox_w,
                        summarybox_w;
extern Window           main_window;
extern int              file_name_toggle_set,
                        items_selected,
                        no_of_search_hosts,
                        toggles_set,
                        file_name_length,
                        special_button_flag,
                        sys_log_fd,
                        no_of_log_files,
                        char_width;
extern time_t           start_time_val,
                        end_time_val;
extern size_t           search_file_size;
extern char             search_file_name[],
                        search_directory_name[],
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


/*############################## toggled() ##############################*/
void
toggled(Widget w, XtPointer client_data, XtPointer call_data)
{
   toggles_set ^= (int)client_data;

   return;
}


/*########################## file_name_toggle() #########################*/
void
file_name_toggle(Widget w, XtPointer client_data, XtPointer call_data)
{
   XmString text;

   if (file_name_toggle_set == LOCAL_FILENAME)
   {
      file_name_toggle_set = REMOTE_FILENAME;
      text = XmStringCreateLocalized("Remote");
   }
   else
   {
      file_name_toggle_set = LOCAL_FILENAME;
      text = XmStringCreateLocalized("Local ");
   }
   XtVaSetValues(w, XmNlabelString, text, NULL);
   XmStringFree(text);

   return;
}


/*########################## item_selection() ###########################*/
void
item_selection(Widget w, XtPointer client_data, XtPointer call_data)
{
#ifdef _SMART_SELECTION
   static time_t        first_date_found,
                        last_date_found,
                        prev_first_date_found,
                        prev_last_date_found;
   static unsigned int  total_no_files;
   static double        file_size,
                        trans_time;
   XmListCallbackStruct *cbs = (XmListCallbackStruct *)call_data;

   if (cbs->reason == XmCR_EXTENDED_SELECT)
   {
      char selection_summary_str[MAX_OUTPUT_LINE_LENGTH + SHOW_LONG_FORMAT + 5];

      if (cbs->selection_type == XmINITIAL)
      {
         /*
          * Initial selection
          */
         total_no_files = cbs->selected_item_count;
         if (get_sum_data((cbs->item_position - 1),
                          &first_date_found, &file_size,
                          &trans_time) == INCORRECT)
         {
            return;
         }
         last_date_found = first_date_found;
      }
      else if (cbs->selection_type == XmMODIFICATION)
           {
              int    i;
              time_t date;
              double current_file_size,
                     current_trans_time;

              /*
               * Modification of selection. Have to recalculate
               * everything.
               */
              total_no_files = cbs->selected_item_count;
              file_size = trans_time = 0.0;
              first_date_found = -1;
              for (i = 0; i < cbs->selected_item_count; i++)
              {
                 if (get_sum_data((cbs->selected_item_positions[i] - 1),
                                  &date, &current_file_size,
                                  &current_trans_time) == INCORRECT)
                 {
                    return;
                 }
                 if (first_date_found == -1)
                 {
                    first_date_found = date;
                 }
                 file_size += current_file_size;
                 trans_time += current_trans_time;
              }
              last_date_found = date;
           }
           else
           {
              time_t date;
              double current_file_size,
                     current_trans_time;

              /*
               * Additional selection. Add or subtract this selection to
               * current selection list.
               */
              if (get_sum_data((cbs->item_position - 1),
                               &date, &current_file_size,
                               &current_trans_time) == INCORRECT)
              {
                 return;
              }
              if (XmListPosSelected(listbox_w, cbs->item_position) == True)
              {
                 file_size += current_file_size;
                 trans_time += current_trans_time;
                 total_no_files++;
                 if (last_date_found < date)
                 {
                    prev_last_date_found = last_date_found;
                    last_date_found = date;
                 }
                 if (date < first_date_found)
                 {
                    prev_first_date_found = first_date_found;
                    first_date_found = date;
                 }
              }
              else
              {
                 file_size -= current_file_size;
                 trans_time -= current_trans_time;
                 total_no_files--;
                 if (date == first_date_found)
                 {
                    first_date_found = prev_first_date_found;
                    if (total_no_files == 1)
                    {
                       last_date_found = first_date_found;
                    }
                 }
                 else if (date == last_date_found)
                      {
                         last_date_found = prev_last_date_found;
                         if (total_no_files == 1)
                         {
                            first_date_found = last_date_found;
                         }
                      }
              }
           }
#else
   static time_t        first_date_found,
                        last_date_found;
   static unsigned int  total_no_files;
   static double        file_size,
                        trans_time;
   XmListCallbackStruct *cbs = (XmListCallbackStruct *)call_data;

   if (cbs->reason == XmCR_EXTENDED_SELECT)
   {
      int    i;
      time_t date;
      double current_file_size,
             current_trans_time;

      total_no_files = cbs->selected_item_count;
      file_size = trans_time = 0.0;
      first_date_found = -1;
      for (i = 0; i < cbs->selected_item_count; i++)
      {
         if (get_sum_data((cbs->selected_item_positions[i] - 1),
                          &date, &current_file_size,
                          &current_trans_time) == INCORRECT)
         {
            return;
         }
         if (first_date_found == -1)
         {
            first_date_found = date;
         }
         file_size += current_file_size;
         trans_time += current_trans_time;
      }
      last_date_found = date;
#endif

      /* Show summary */
      if (cbs->selected_item_count > 0)
      {
         calculate_summary(summary_str, first_date_found, last_date_found,
                           total_no_files, file_size, trans_time);
      }
      else
      {
         (void)strcpy(summary_str, total_summary_str);
      }
      XmTextSetString(summarybox_w, summary_str);
      
      items_selected = YES;
   }

   return;
}


/*########################### radio_button() ############################*/
void
radio_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   int new_file_name_length = (int)client_data;

   if (new_file_name_length != file_name_length)
   {
      Window       root_return;
      int          x,
                   y,
                   no_of_items;
      unsigned int width,
                   window_height,
                   border,
                   depth;

      file_name_length = new_file_name_length;

      XGetGeometry(display, main_window, &root_return, &x, &y, &width, &window_height, &border, &depth);
      if (file_name_length == SHOW_SHORT_FORMAT)
      {
         XmTextSetString(headingbox_w, HEADING_LINE_SHORT);
      }
      else if (file_name_length == SHOW_MEDIUM_FORMAT)
           {
              XmTextSetString(headingbox_w, HEADING_LINE_MEDIUM);
           }
           else
           {
              XmTextSetString(headingbox_w, HEADING_LINE_LONG);
           }

      XResizeWindow(display, main_window, char_width * (MAX_OUTPUT_LINE_LENGTH + file_name_length + 6), window_height);

      XtVaGetValues(listbox_w, XmNitemCount, &no_of_items, NULL);
      if (no_of_items > 0)
      {
         int i;

         for (i = 0; i < no_of_log_files; i++)
         {
            if (il[i].fp != NULL)
            {
               if (fclose(il[i].fp) == EOF)
               {
                  (void)fprintf(stderr, "fclose() error : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
               }
            }
            if (il[i].line_offset != NULL)
            {
               free(il[i].line_offset);
               il[i].line_offset = NULL;
            }
            if (il[i].offset != NULL)
            {
               free(il[i].offset);
               il[i].offset = NULL;
            }
         }
         if (no_of_log_files > 0)
         {
            free(il);
         }

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


/*############################ info_click() #############################*/
void
info_click(Widget w, XtPointer client_data, XEvent *event)
{
   if ((event->xbutton.button == Button2) ||
       (event->xbutton.button == Button3))
   {
      int  pos = XmListYToPos(w, event->xbutton.y),
           max_pos;

      /* Check if pos is valid. */
      XtVaGetValues(w, XmNitemCount, &max_pos, NULL);
      if ((max_pos > 0) && (pos <= max_pos))
      {
         char text[8192];

         /* Initialize text an data area. */
         text[0] = '\0';
         id.no_of_files = 0;
         id.local_file_name[0] = '\0';
         id.files = NULL;
#ifdef _WITH_DYNAMIC_MEMORY
         id.loptions = NULL;
#endif
         id.soptions = NULL;
         id.archive_dir[0] = '\0';

         /* Get the information. */
         get_info(pos);

         /* Format information in a human readable text. */
         format_info(text);

         /* Show the information. */
         show_info(text);

         /* Free all memory that was allocated in get_info(). */
         free(id.files);
#ifdef _WITH_DYNAMIC_MEMORY
         if (id.loptions != NULL)
         {
            FREE_RT_ARRAY(id.loptions);
         }
#else
         if (id.soptions != NULL)
         {
            free((void *)id.soptions);
         }
#endif
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
      int i;

      for (i = 0; i < no_of_log_files; i++)
      {
         if (il[i].fp != NULL)
         {
            if (fclose(il[i].fp) == EOF)
            {
               (void)fprintf(stderr, "fclose() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
            }
         }
         if (il[i].line_offset != NULL)
         {
            free(il[i].line_offset);
            il[i].line_offset = NULL;
         }
         if (il[i].offset != NULL)
         {
            free(il[i].offset);
            il[i].offset = NULL;
         }
      }
      if (no_of_log_files > 0)
      {
         free(il);
      }

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
      special_button_flag = STOP_BUTTON_PRESSED;
   }

   return;
}


/*########################### resend_button() ###########################*/
void
resend_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   int no_selected,
       *select_list;

   reset_message(statusbox_w);
   if (XmListGetSelectedPos(listbox_w, &select_list, &no_selected) == True)
   {
      resend_files(no_selected, select_list);
      XtFree((char *)select_list);

      /*
       * After resending, see if any items habe been left selected.
       * If so, create a new summary string or else insert the total
       * summary string if no items are left selected.
       */
      if (XmListGetSelectedPos(listbox_w, &select_list, &no_selected) == True)
      {
         int    i;
         time_t date,
                first_date_found,
                last_date_found;
         double current_file_size,
                current_trans_time,
                file_size = 0.0,
                trans_time = 0.0;

         first_date_found = -1;
         for (i = 0; i < no_selected; i++)
         {
            if (get_sum_data((select_list[i] - 1),
                             &date, &current_file_size,
                             &current_trans_time) == INCORRECT)
            {
               return;
            }
            if (first_date_found == -1)
            {
               first_date_found = date;
            }
            file_size += current_file_size;
            trans_time += current_trans_time;
         }
         last_date_found = date;
         XtFree((char *)select_list);
         calculate_summary(summary_str, first_date_found, last_date_found,
                           no_selected, file_size, trans_time);
      }
      else
      {
         (void)strcpy(summary_str, total_summary_str);
      }
      XmTextSetString(summarybox_w, summary_str);
   }
   else
   {
      show_message(statusbox_w, "No file selected!");
   }

   return;
}


/*############################# send_button() ###########################*/
void
send_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   reset_message(statusbox_w);
   if (items_selected == YES)
   {
      get_send_data();
   }
   else
   {
      show_message(statusbox_w, "No file selected!");
   }

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
   int  type = (int)client_data,
        extra_sign = 0;
   char *ptr,
        *value = XmTextGetString(w);

   switch(type)
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
         (void)strcpy(search_directory_name, value);
         reset_message(statusbox_w);
         break;

      case DIRECTORY_NAME :
         (void)strcpy(search_directory_name, value);
         reset_message(statusbox_w);
         XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         break;

      case FILE_LENGTH_NO_ENTER :
      case FILE_LENGTH :
         if (value[0] == '\0')
         {
            search_file_size = -1;
         }
         else
         {
            if (isdigit(value[0]))
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
            (void)strcpy(search_file_size_str, value + extra_sign);
         }
         reset_message(statusbox_w);
         if (type == FILE_LENGTH)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case RECIPIENT_NAME_NO_ENTER : /* Read the recipient */
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
         break;

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
                         (*ptr != ',') &&
                         (*ptr != '@'))
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
         XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
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

   (void)time(&time_val);

   switch (length)
   {
      case 0 : /* Assume user means current time */
               {
                  char time_str[9];

                  (void)strftime(time_str, 9, "%m%d%H%M", gmtime(&time_val));
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
      if ((!isdigit(numeric_str[1])) || (!isdigit(numeric_str[2])))
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
              if ((!isdigit(numeric_str[3])) || (!isdigit(numeric_str[4])))
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

              if ((!isdigit(numeric_str[3])) || (!isdigit(numeric_str[4])) ||
                  (!isdigit(numeric_str[5])) || (!isdigit(numeric_str[6])))
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

   if ((!isdigit(numeric_str[0])) || (!isdigit(numeric_str[1])) ||
       (!isdigit(numeric_str[2])) || (!isdigit(numeric_str[3])))
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
      bd_time = gmtime(&time_val);
      bd_time->tm_sec  = 0;
      bd_time->tm_min  = min;
      bd_time->tm_hour = hour;

      time_val = mktime(bd_time);
      bd_time = gmtime(&time_val);

      *value = mktime(bd_time) - timezone;
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
           bd_time = gmtime(&time_val);
           bd_time->tm_sec  = 0;
           bd_time->tm_min  = min;
           bd_time->tm_hour = hour;
           bd_time->tm_mday = day;

           time_val = mktime(bd_time);
           bd_time = gmtime(&time_val);

           *value = mktime(bd_time) - timezone;
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
           bd_time = gmtime(&time_val);
           bd_time->tm_sec  = 0;
           bd_time->tm_min  = min;
           bd_time->tm_hour = hour;
           bd_time->tm_mday = day;
           bd_time->tm_mon  = month - 1;
           bd_time->tm_isdst = 0;

           *value = mktime(bd_time) - timezone;
        }

   return(SUCCESS);
}

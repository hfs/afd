/*
 *  motif_common_defs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __motif_common_defs_h
#define __motif_common_defs_h

#include <Xm/Xm.h>
#include "ui_common_defs.h"

/* The following definitions are for show_ilog and show_olog only. */
#define CHECK_TIME_INTERVAL             10

/* Function Prototypes. */
extern int    check_info_file(char *, char *, int),
              prepare_printer(int *),
              prepare_file(int *, int);
extern void   check_nummeric(Widget, XtPointer, XtPointer),
              disable_drag_drop(Widget),
              make_xprocess(char *, char *, char **, int),
              prepare_tmp_name(void),
              print_data(Widget, XtPointer, XtPointer),
              print_data_button(Widget, XtPointer, XtPointer),
              reset_message(Widget),                /* show_?log */
              send_mail_cmd(char *),
              send_print_cmd(char *),
#ifdef HAVE_XPM
              setup_icon(Display *, Widget),
#endif
              show_info(char *),                    /* show_?log */
              show_message(Widget, char *),         /* show_?log */
              update_time(XtPointer, XtIntervalId), /* show_?log */
              wait_visible(Widget);

#endif /* __motif_common_defs_h */

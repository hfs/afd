/*
 *  handle_event.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __handle_event_h
#define __handle_event_h

#include "x_common_defs.h"

#define MAXARGS                  20

#define ACKNOWLEDGE_SELECT       1
#define OFFLINE_SELECT           2
#define UNSET_SELECT             3

/* Function prototypes */
extern void close_button(Widget, XtPointer, XtPointer),
            radio_button(Widget, XtPointer, XtPointer),
            set_button(Widget, XtPointer, XtPointer);

#endif /* __handle_event_h */

/*
 *  get_error_str.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_error_str - returns the error string for the given error number
 **
 ** SYNOPSIS
 **   char *get_error_str(int error_code)
 **
 ** DESCRIPTION
 **   The function get_error_str() looks up the error string for the
 **   given error_code and returns this to the caller.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.06.2004 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "fddefs.h"


/*######################### get_error_str() #############################*/
char *
get_error_str(int error_code)
{
   switch (error_code)
   {
      case TRANSFER_SUCCESS : return(TRANSFER_SUCCESS_STR);
      case CONNECT_ERROR : return(CONNECT_ERROR_STR);
      case USER_ERROR : return(USER_ERROR_STR);
      case PASSWORD_ERROR : return(PASSWORD_ERROR_STR);
      case TYPE_ERROR : return(TYPE_ERROR_STR);
      case LIST_ERROR : return(LIST_ERROR_STR);
      case MAIL_ERROR : return(MAIL_ERROR_STR);
      case JID_NUMBER_ERROR : return(JID_NUMBER_ERROR_STR);
      case GOT_KILLED : return(GOT_KILLED_STR);
#ifdef WITH_SSL
      case AUTH_ERROR : return(AUTH_ERROR_STR);
#endif
      case OPEN_REMOTE_ERROR : return(OPEN_REMOTE_ERROR_STR);
      case WRITE_REMOTE_ERROR : return(WRITE_REMOTE_ERROR_STR);
      case CLOSE_REMOTE_ERROR : return(CLOSE_REMOTE_ERROR_STR);
      case MOVE_REMOTE_ERROR : return(MOVE_REMOTE_ERROR_STR);
      case CHDIR_ERROR : return(CHDIR_ERROR_STR);
      case WRITE_LOCK_ERROR : return(WRITE_LOCK_ERROR_STR);
      case REMOVE_LOCKFILE_ERROR : return(REMOVE_LOCKFILE_ERROR_STR);
      case STAT_ERROR : return(STAT_ERROR_STR);
      case MOVE_ERROR : return(MOVE_ERROR_STR);
      case RENAME_ERROR : return(RENAME_ERROR_STR);
      case TIMEOUT_ERROR : return(TIMEOUT_ERROR_STR);
#ifdef _WITH_WMO_SUPPORT
      case CHECK_REPLY_ERROR : return(CHECK_REPLY_ERROR_STR);
#endif
      case READ_REMOTE_ERROR : return(READ_REMOTE_ERROR_STR);
      case SIZE_ERROR : return(SIZE_ERROR_STR);
      case DATE_ERROR : return(DATE_ERROR_STR);
      case QUIT_ERROR : return(QUIT_ERROR_STR);
      case MKDIR_ERROR : return(MKDIR_ERROR_STR);
      case CHOWN_ERROR : return(CHOWN_ERROR_STR);
      case OPEN_LOCAL_ERROR : return(OPEN_LOCAL_ERROR_STR);
      case READ_LOCAL_ERROR : return(READ_LOCAL_ERROR_STR);
      case LOCK_REGION_ERROR : return(LOCK_REGION_ERROR_STR);
      case UNLOCK_REGION_ERROR : return(UNLOCK_REGION_ERROR_STR);
      case ALLOC_ERROR : return(ALLOC_ERROR_STR);
      case SELECT_ERROR : return(SELECT_ERROR_STR);
      case WRITE_LOCAL_ERROR : return(WRITE_LOCAL_ERROR_STR);
      case OPEN_FILE_DIR_ERROR : return(OPEN_FILE_DIR_ERROR_STR);
      case NO_MESSAGE_FILE : return(NO_MESSAGE_FILE_STR);
      case REMOTE_USER_ERROR : return(REMOTE_USER_ERROR_STR);
      case DATA_ERROR : return(DATA_ERROR_STR);
#ifdef _WITH_WMO_SUPPORT
      case SIG_PIPE_ERROR : return(SIG_PIPE_ERROR_STR);
#endif
#ifdef _WITH_MAP_SUPPORT
      case MAP_FUNCTION_ERROR : return(MAP_FUNCTION_ERROR_STR);
#endif
      case SYNTAX_ERROR : return(SYNTAX_ERROR_STR);
      case NO_FILES_TO_SEND : return(NO_FILES_TO_SEND_STR);
      case STILL_FILES_TO_SEND : return(STILL_FILES_TO_SEND_STR);
   }
   return("Unknown error");
} 

/*
 *  append.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   trans_exec - execute a command for a file that has just been send.
 **
 ** SYNOPSIS
 **   void trans_exec(char *file_path, char *fullname, char *p_file_name_buffer)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None. If we fail to log the fail name, all that happens the next
 **   time is that the complete file gets send again.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.05.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                /* sprintf()                           */
#include <string.h>               /* strerror()                          */
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables */
extern char                       msg_str[];
extern struct filetransfer_status *fsa;
extern struct job                 db;


/*############################ trans_exec() #############################*/
void
trans_exec(char *file_path, char *fullname, char *p_file_name_buffer)
{
   char *p_command,
        tmp_connect_status;

   msg_str[0] = '\0';
   tmp_connect_status = fsa[db.fsa_pos].job_status[(int)db.job_no].connect_status;
   fsa[db.fsa_pos].job_status[(int)db.job_no].connect_status = POST_EXEC;
   p_command = db.trans_exec_cmd;
   while (((*p_command == ' ') || (*p_command == '\t')) &&
          ((*p_command != '\n') && (*p_command != '\0')))
   {
      p_command++;
   }
   if ((*p_command == '\n') || (*p_command == '\0'))
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__,
                "No command specified for executing. Ignoring this option.");
   }
   else
   {
      register int ii = 0,
                   k = 0;
      char         *p_end = p_command,
                   *p_tmp_dir,
                   *insert_list[10],
                   tmp_char,
                   tmp_option[1024],
                   command_str[1024],
                   return_str[MAX_PATH_LENGTH + MAX_PATH_LENGTH + 1];

      while ((*p_end != '\n') && (*p_end != '\0'))
      {
         if ((*p_end == '%') && (*(p_end + 1) == 's'))
         {
            tmp_option[k] = *p_end;
            tmp_option[k + 1] = *(p_end + 1);
            insert_list[ii] = &tmp_option[k];
            ii++;
            p_end += 2;
            k += 2;
         }
         else
         {
            tmp_option[k] = *p_end;
            p_end++; k++;
         }
      }
      tmp_option[k] = '\0';
      p_command = tmp_option;

      /*
       * Create a temporary directory in which the user can execute
       * the commands. We do not want the manipulated files in the
       * archive. After the user commands are executed the files
       * in the temporary directory and the directory will be
       * removed. This is not efficient, but is the simplest way
       * to implement this.
       */
      p_tmp_dir = file_path + strlen(file_path);
      (void)strcpy(p_tmp_dir, "/.tmp");
      if (mkdir(file_path, DIR_MODE) == -1)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__,
                   "Failed to mkdir() %s : %s",
                   file_path, strerror(errno));
      }
      else
      {
         *(p_tmp_dir + 5) = '/';
         (void)strcpy(p_tmp_dir + 6, p_file_name_buffer);
         if (copy_file(fullname, file_path) < 0)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__,
                      "Failed to copy_file().");
            *(p_tmp_dir + 5) = '\0';
         }
         else
         {
            *(p_tmp_dir + 5) = '\0';
            if (ii > 0)
            {
               int  length,
                    length_start,
                    mask_file_name,
                    ret;
               char *fnp;

               insert_list[ii] = &tmp_option[k];
               tmp_char = *insert_list[0];
               *insert_list[0] = '\0';
               length_start = sprintf(command_str, "cd %s && %s",
                                      file_path, p_command);
               *insert_list[0] = tmp_char;

               fnp = p_file_name_buffer;
               mask_file_name = NO;
               do
               {
                  if ((*fnp == ';') || (*fnp == ' '))
                  {
                     mask_file_name = YES;
                     break;
                  }
                  fnp++;
               } while (*fnp != '\0');

               /* Generate command string with file name(s) */
               length = 0;
               for (k = 1; k < (ii + 1); k++)
               {
                  tmp_char = *insert_list[k];
                  *insert_list[k] = '\0';
                  if (mask_file_name == YES)
                  {
                     length += sprintf(command_str + length_start + length,
                                       "%s\"%s\"", p_file_name_buffer,
                                       insert_list[k - 1] + 2);
                  }
                  else
                  {
                     length += sprintf(command_str + length_start + length,
                                       "%s%s", p_file_name_buffer,
                                       insert_list[k - 1] + 2);
                  }
                  *insert_list[k] = tmp_char;
               }

               if ((ret = exec_cmd(command_str, return_str)) != 0) /* ie != SUCCESS */
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to execute command %s [Return code = %d]",
                            command_str, ret);
                  if (return_str[0] != '\0')
                  {
                     char *end_ptr = return_str,
                          *start_ptr;

                     do
                     {
                        start_ptr = end_ptr;
                        while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                        {
                           end_ptr++;
                        }
                        if (*end_ptr == '\n')
                        {
                           *end_ptr = '\0';
                           end_ptr++;
                        }
                        trans_log(WARN_SIGN, __FILE__, __LINE__,
                                  "%s", start_ptr);
                     } while (*end_ptr != '\0');
                  }
               }
            }
            else
            {
               int ret;

               (void)sprintf(command_str, "cd %s && %s",
                             file_path, p_command);
               if ((ret = exec_cmd(command_str, return_str)) != 0)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to execute command %s [Return code = %d]",
                            command_str, ret);
                  trans_log(WARN_SIGN, __FILE__, __LINE__,
                            "%s", return_str);
               }
            }
         }
         if (rec_rmdir(file_path) < 0)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__,
                      "Failed to remove directory %s.", file_path);
         }
      }
      *p_tmp_dir = '\0';
   }
   fsa[db.fsa_pos].job_status[(int)db.job_no].connect_status = tmp_connect_status;

   return;
}

/*
 *  sftpcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005, 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   sftpcmd - commands to send files via the SFTP protocol
 **
 ** SYNOPSIS
 **   int          sftp_close_file(void)
 **   int          sftp_connect(char *hostname, int port, char *user,
 **                             char *passwd, char *dir, char debug)
 **   int          sftp_dele(char *filename)
 **   int          sftp_flush(void)
 **   int          sftp_mkdir(char *directory)
 **   int          sftp_move(char *from, char *to, int create_dir)
 **   int          sftp_open_file(char *filename, off_t size, mode_t *mode,
 **                               char debug)
 **   void         sftp_quit(void)
 **   int          sftp_read(char *block, int size)
 **   int          sftp_write(char *block, int size)
 **   unsigned int sftp_version(void)
 **
 ** DESCRIPTION
 **   scpcmd provides a set of commands to communicate SFTP with a
 **   SSH (Secure Shell) server via pipes. The following functions are
 **   required to send a file:
 **      sftp_close_file() - close a file
 **      sftp_connect()    - build a connection to the SSH server
 **      sftp_dele()       - deletes a file/link
 **      sftp_flush()      - flush all pending writes
 **      sftp_mkdir()      - creates a directory
 **      sftp_move()       - move/rename a file
 **      sftp_open_file()  - open a file
 **      sftp_quit()       - disconnect from the SSH server
 **      sftp_read()       - read data from a file
 **      sftp_write()      - write data to a file
 **      sftp_version()    - returns SSH version agreed on
 **
 ** RETURN VALUES
 **   Returns SUCCESS when successful. When an error has occurred
 **   it will return INCORRECT. 'timeout_flag' is just a flag to
 **   indicate that the 'transfer_timeout' time has been reached.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.12.2005 H.Kiehl Created
 **
 */
DESCR__E_M3


#include <stdio.h>            /* fdopen(), fclose()                      */
#include <string.h>           /* memset(), memcpy()                      */
#include <stdlib.h>           /* malloc(), free()                        */
#ifdef WITH_OWNER_GROUP_EVAL
# include <grp.h>             /* getgrnam()                              */
# include <pwd.h>             /* getpwnam()                              */
#endif
#include <sys/types.h>        /* fd_set                                  */
#include <sys/time.h>         /* struct timeval                          */
#include <sys/wait.h>         /* waitpid()                               */
#include <sys/stat.h>         /* S_ISUID, S_ISGID, etc                   */
#include <signal.h>           /* signal(), kill()                        */
#include <unistd.h>           /* select(), exit(), write(), read(),      */
                              /* close()                                 */
#include <fcntl.h>            /* O_NONBLOCK                              */
#include <errno.h>
#include "sftpdefs.h"
#include "fddefs.h"           /* struct job                              */


/* External global variables */
extern int                      timeout_flag;
extern char                     msg_str[];
extern long                     transfer_timeout;
extern char                     tr_hostname[];
extern struct job               db;

/* Local global variables */
static int                      byte_order = 1,
                                data_fd = -1;
static pid_t                    data_pid = -1;
static char                     *msg = NULL;
static struct timeval           timeout;
static struct sftp_connect_data scd;

/* Local function prototypes. */
static unsigned int             get_xfer_uint(char *);
static u_long_64                get_xfer_uint64(char *);
static int                      check_msg_pending(void),
                                get_reply(unsigned int),
                                get_write_reply(unsigned int),
                                get_xfer_str(char *, char **),
                                is_with_path(char *),
                                read_msg(char *, int),
                                write_msg(char *, int);
static char                     *error_2_str(char *),
                                *response_2_str(char);
static void                     get_msg_str(char *),
                                set_xfer_str(char *, char *, int),
                                set_xfer_uint(char *, unsigned int),
                                set_xfer_uint64(char *, u_long_64),
                                sig_ignore_handler(int);


/*########################### sftp_connect() ############################*/
int
sftp_connect(char *hostname, int port, char *user, char *passwd, char debug)
{
   int status;

   if ((status = ssh_exec(hostname, port, user, passwd, NULL, "sftp",
                          &data_fd, &data_pid)) == SUCCESS)
   {
      unsigned int ui_var = 5;

      if (msg == NULL)
      {
         if ((msg = malloc(MAX_SFTP_MSG_LENGTH)) == NULL)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "sftp_connect(): malloc() error : %s", strerror(errno));
            return(INCORRECT);
         }
      }

      if (*(char *)&byte_order == 1)
      {
         /* little-endian */
         msg[0] = ((char *)&ui_var)[3];
         msg[1] = ((char *)&ui_var)[2];
         msg[2] = ((char *)&ui_var)[1];
         msg[3] = ((char *)&ui_var)[0];
         ui_var = SSH_FILEXFER_VERSION;
         msg[5] = ((char *)&ui_var)[3];
         msg[6] = ((char *)&ui_var)[2];
         msg[7] = ((char *)&ui_var)[1];
         msg[8] = ((char *)&ui_var)[0];
      }
      else
      {
         /* big-endian */
         msg[0] = ((char *)&ui_var)[0];
         msg[1] = ((char *)&ui_var)[1];
         msg[2] = ((char *)&ui_var)[2];
         msg[3] = ((char *)&ui_var)[3];
         ui_var = SSH_FILEXFER_VERSION;
         msg[5] = ((char *)&ui_var)[0];
         msg[6] = ((char *)&ui_var)[1];
         msg[7] = ((char *)&ui_var)[2];
         msg[8] = ((char *)&ui_var)[3];
      }
      msg[4] = SSH_FXP_INIT;
      scd.debug = debug;

      if ((status = write_msg(msg, 9)) == SUCCESS)
      {
         if ((status = ssh_login(data_fd, passwd)) == SUCCESS)
         {
            if ((status = read_msg(msg, 4)) == SUCCESS)
            {
               ui_var = get_xfer_uint(msg);
               if (ui_var <= MAX_SFTP_MSG_LENGTH)
               {
                  if ((status = read_msg(msg, (int)ui_var)) == SUCCESS)
                  {
                     if (msg[0] == SSH_FXP_VERSION)
                     {
                        int  str_len;
                        char *ptr,
                             *p_xfer_str = NULL;

                        scd.version = get_xfer_uint(&msg[1]);
                        if (scd.version > SSH_FILEXFER_VERSION)
                        {
                           trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                                     "Server version (%u) is higher, downgrading to version we can handle (%d).",
                                     scd.version, SSH_FILEXFER_VERSION);
                           scd.version = SSH_FILEXFER_VERSION;
                        }
                        ui_var -= 5;
                        if (ui_var > 0)
                        {
                           ptr = &msg[5];
                           while (ui_var > 0)
                           {
                              if (((str_len = get_xfer_str(ptr, &p_xfer_str)) == 0) ||
                                  (str_len > ui_var))
                              {
                                 break;
                              }
                              else
                              {
                                 trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                                           "-> %s", p_xfer_str);
                                 ui_var -= (str_len + 4);
                                 free(p_xfer_str);
                                 p_xfer_str = NULL;
                              }
                           }
                        }
                        scd.request_id     = 0;
                        scd.stored_replies = 0;
                        scd.cwd            = NULL;
                        scd.handle         = NULL;
                     }
                     else
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                  "sftp_connect(): Received invalid reply (%d) from SSH_FXP_INIT.",
                                  (int)msg[0]);
                        status = INCORRECT;
                     }
                  }
               }
               else
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            "sftp_connect(): Received message is %u bytes, can only handle %d bytes.",
                            ui_var, MAX_SFTP_MSG_LENGTH);
                  status = INCORRECT;
                  sftp_quit();
               }
            }
         }
      }
   }
   return(status);
}


/*########################### sftp_version() ############################*/
unsigned int
sftp_version(void)
{
   return(scd.version);
}


/*############################# sftp_pwd() ##############################*/
int
sftp_pwd(void)
{
   int status;

   /*
    * byte   SSH_FXP_REALPATH
    * uint32 request-id
    * string original-path [UTF-8]
    * byte   control-byte [optional]
    * string compose-path[0..n] [optional]
    */
   msg[4] = SSH_FXP_REALPATH;
   scd.request_id++;
   set_xfer_uint(&msg[4 + 1], scd.request_id);
   set_xfer_str(&msg[4 + 1 + 4], ".", 1);
   set_xfer_uint(msg, (1 + 4 + 4 + 1));
   if ((status = write_msg(msg, 14)) == SUCCESS)
   {
      if ((status = get_reply(scd.request_id)) == SUCCESS)
      {
         if (msg[0] == SSH_FXP_NAME)
         {
            unsigned int ui_var;

            ui_var = get_xfer_uint(&msg[5]);

            /* We can only handle one name. */
            if (ui_var == 1)
            {
               int  str_len;
               char *p_xfer_str = NULL;

               if ((str_len = get_xfer_str(&msg[9], &p_xfer_str)) == 0)
               {
                  status = INCORRECT;
               }
               else
               {
                  (void)memcpy(msg_str, p_xfer_str, str_len);
                  free(p_xfer_str);
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "sftp_pwd(): Expecting a one here, but received %d. We are only able to handle one name.",
                         ui_var);
               status = INCORRECT;
            }
         }
         else
         {
            if (msg[0] == SSH_FXP_STATUS)
            {
               /* Some error has occured. */
               get_msg_str(&msg[9]);
               trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                         "sftp_pwd(): %s", error_2_str(&msg[5]));
               status = INCORRECT;
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "sftp_pwd(): Expecting %d (SSH_FXP_NAME) but got %d (%s) as reply.",
                         SSH_FXP_NAME, (int)msg[0], response_2_str(msg[0]));
               msg_str[0] = '\0';
               status = INCORRECT;
            }
         }
      }
   }

   return(status);
}


/*############################# sftp_cd() ###############################*/
int
sftp_cd(char *directory, int create_dir)
{
   int retries = 0,
       status;

retry_cd:
   if ((directory[0] == '\0') || (scd.cwd != NULL))
   {
      /* Go back to users home directory. */
      free(scd.cwd);
      scd.cwd = NULL;
      if (directory[0] == '\0')
      {
         return(SUCCESS);
      }
   }

   /*
    * byte   SSH_FXP_REALPATH
    * uint32 request-id
    * string original-path [UTF-8]
    * byte   control-byte [optional]
    * string compose-path[0..n] [optional]
    */
   msg[4] = SSH_FXP_REALPATH;
   scd.request_id++;
   set_xfer_uint(&msg[4 + 1], scd.request_id);
   status = strlen(directory);
   set_xfer_str(&msg[4 + 1 + 4], directory, status);
   set_xfer_uint(msg, (1 + 4 + 4 + status));
   if ((status = write_msg(msg, (4 + 1 + 4 + 4 + status))) == SUCCESS)
   {
      if ((status = get_reply(scd.request_id)) == SUCCESS)
      {
         if (msg[0] == SSH_FXP_NAME)
         {
            unsigned int ui_var;

            ui_var = get_xfer_uint(&msg[5]);

            /* We can only handle one name. */
            if (ui_var == 1)
            {
               int str_len;

               if (scd.cwd != NULL)
               {
                  free(scd.cwd);
                  scd.cwd = NULL;
               }
               if ((str_len = get_xfer_str(&msg[9], &scd.cwd)) == 0)
               {
                  msg_str[0] = '\0';
                  status = INCORRECT;
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "sftp_cd(): Expecting a one here, but received %d. We are only able to handle one name.",
                         ui_var);
               msg_str[0] = '\0';
               status = INCORRECT;
            }
         }
         else
         {
            if (msg[0] == SSH_FXP_STATUS)
            {
               if ((create_dir == YES) && (retries == 0) &&
                   (get_xfer_uint(&msg[5]) == SSH_FX_NO_SUCH_FILE))
               {
                  char *ptr,
                       tmp_char;

                  ptr = directory;
                  do
                  {
                     while (*ptr == '/')
                     {
                        ptr++;
                     }
                     while ((*ptr != '/') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                     if ((*ptr == '/') || (*ptr == '\0'))
                     {
                        tmp_char = *ptr;
                        *ptr = '\0';
                        if ((status = sftp_stat(directory, NULL)) != SUCCESS)
                        {
                           status = sftp_mkdir(directory);
                        }
                        else if (scd.version >= 6)
                             {
                                if (scd.stat_buf.st_mode != S_IFDIR)
                                {
                                   status = INCORRECT;
                                }
                             }
                        *ptr = tmp_char;
                     }
                  } while ((*ptr != '\0') && (status == SUCCESS));

                  if ((*ptr == '\0') && (status == SUCCESS))
                  {
                     retries++;
                     goto retry_cd;
                  }
               }
               else
               {
                  /* Some error has occured. */
                  get_msg_str(&msg[9]);
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                            "sftp_cd(): %s", error_2_str(&msg[5]));
                  status = INCORRECT;
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "sftp_cd(): Expecting %d (SSH_FXP_NAME) but got %d (%s) as reply.",
                         SSH_FXP_NAME, (int)msg[0], response_2_str(msg[0]));
               msg_str[0] = '\0';
               status = INCORRECT;
            }
         }
      }
   }

   return(status);
}


/*############################ sftp_stat() ##############################*/
int
sftp_stat(char *filename, struct stat *p_stat_buf)
{
   int pos,
       status;

   if ((filename == NULL) && (scd.handle == NULL))
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "sftp_stat(): Wrong usage of function. filename and scd.handle are both NULL! Remove the programmer.");
      msg_str[0] = '\0';
      return(INCORRECT);
   }

   /*
    * byte   SSH_FXP_STAT | SSH_FXP_FSTAT
    * uint32 request_id
    * string path [UTF-8] | handle
    * [uint32 flags]  Version 6+
    */
   scd.request_id++;
   set_xfer_uint(&msg[4 + 1], scd.request_id);
   if (filename == NULL)
   {
      msg[4] = SSH_FXP_FSTAT;
      status = (int)scd.handle_length;
      set_xfer_str(&msg[4 + 1 + 4], scd.handle, status);
   }
   else
   {
      msg[4] = SSH_FXP_STAT;
      if (scd.cwd == NULL)
      {
         status = strlen(filename);
         set_xfer_str(&msg[4 + 1 + 4], filename, status);
      }
      else
      {
         char fullname[MAX_PATH_LENGTH];

         status = sprintf(fullname, "%s/%s", scd.cwd, filename);
         set_xfer_str(&msg[4 + 1 + 4], fullname, status);
      }
   }
   pos = 4 + 1 + 4 + 4 + status;
   if (scd.version >= 6)
   {
      set_xfer_uint(&msg[pos],
                    (SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_MODIFYTIME));
      pos += 4;
   }
   set_xfer_uint(msg, (pos - 4));
   if ((status = write_msg(msg, pos)) == SUCCESS)
   {
      if ((status = get_reply(scd.request_id)) == SUCCESS)
      {
         if (msg[0] == SSH_FXP_ATTRS)
         {
            (void)memset(&scd.stat_buf, 0, sizeof(struct stat));
            scd.stat_flags = get_xfer_uint(&msg[5]);
            if (scd.version >= 6)
            {
               switch (msg[9])
               {
                  case SSH_FILEXFER_TYPE_REGULAR      :
                     scd.stat_buf.st_mode = S_IFREG;
                     break;
                  case SSH_FILEXFER_TYPE_DIRECTORY    :
                     scd.stat_buf.st_mode = S_IFDIR;
                     break;
                  case SSH_FILEXFER_TYPE_SYMLINK      :
                     scd.stat_buf.st_mode = S_IFLNK;
                     break;
                  case SSH_FILEXFER_TYPE_SPECIAL      :
                     break;
                  case SSH_FILEXFER_TYPE_UNKNOWN      :
                     break;
                  case SSH_FILEXFER_TYPE_SOCKET       :
#ifdef S_IFSOCK
                     scd.stat_buf.st_mode = S_IFSOCK;
#endif
                     break;
                  case SSH_FILEXFER_TYPE_CHAR_DEVICE  :
                     scd.stat_buf.st_mode = S_IFCHR;
                     break;
                  case SSH_FILEXFER_TYPE_BLOCK_DEVICE :
                     scd.stat_buf.st_mode = S_IFBLK;
                     break;
                  case SSH_FILEXFER_TYPE_FIFO         :
                     scd.stat_buf.st_mode = S_IFIFO;
                     break;
                  default                             :
                     trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                               "Unknown type field %d in protocol.",
                               (int)msg[4]);
                     break;
               }
               pos = 10;
            }
            else
            {
               pos = 9;
            }
            if (scd.stat_flags & SSH_FILEXFER_ATTR_SIZE)
            {
               scd.stat_buf.st_size = (off_t)get_xfer_uint64(&msg[pos]);
               pos += 8;
            }
            if (scd.stat_flags & SSH_FILEXFER_ATTR_ALLOCATION_SIZE)
            {
               pos += 8;
            }
            if (scd.stat_flags & SSH_FILEXFER_ATTR_OWNERGROUP)
            {
               unsigned int  length;
#ifdef WITH_OWNER_GROUP_EVAL
               char          *p_owner_group = NULL,
                             *ptr;
               struct passwd *p_pw;
               struct group  *p_gr;

               /* Get the user ID. */
               if ((length = get_xfer_str(&msg[pos], &p_owner_group)) == 0)
               {
                  return(INCORRECT);
               }
               pos += (length + 4);
               ptr = p_owner_group;
               while ((*ptr != '@') && (*ptr != '\0'))
               {
                  ptr++;
               }
               if (*ptr == '@')
               {
                  *ptr = '\0';
               }
               if ((p_pw = getpwnam(p_owner_group)) != NULL)
               {
                  scd.stat_buf.st_uid = p_pw->pw_uid;
               }
               free(p_owner_group);
               p_owner_group = NULL;

               /* Get the group ID. */
               if ((length = get_xfer_str(&msg[pos], &p_owner_group)) == 0)
               {
                  return(INCORRECT);
               }
               pos += (length + 4);
               ptr = p_owner_group;
               while ((*ptr != '@') && (*ptr != '\0'))
               {
                  ptr++;
               }
               if (*ptr == '@')
               {
                  *ptr = '\0';
               }
               if ((p_gr = getgrnam(p_owner_group)) != NULL)
               {
                  scd.stat_buf.st_gid = p_gr->gr_gid;
               }
               free(p_owner_group);
#else
               if ((length = get_xfer_str(&msg[pos], NULL)) == 0)
               {
                  return(INCORRECT);
               }
               pos += (length + 4);
               if ((length = get_xfer_str(&msg[pos], NULL)) == 0)
               {
                  return(INCORRECT);
               }
               pos += (length + 4);
#endif
            }
            if (scd.stat_flags & SSH_FILEXFER_ATTR_PERMISSIONS)
            {
               unsigned int ui_var;

               ui_var = get_xfer_uint(&msg[pos]);
               scd.stat_buf.st_mode |= ui_var;
               pos += 4;
            }
            if (scd.stat_flags & SSH_FILEXFER_ATTR_ACCESSTIME)
            {
               scd.stat_buf.st_atime = (time_t)get_xfer_uint64(&msg[pos]);
               pos += 8;
            }
            if (scd.stat_flags & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
            {
               pos += 4;
            }
            if (scd.stat_flags & SSH_FILEXFER_ATTR_CREATETIME)
            {
               pos += 8;
            }
            if (scd.stat_flags & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
            {
               pos += 4;
            }
            if (scd.stat_flags & SSH_FILEXFER_ATTR_MODIFYTIME)
            {
               scd.stat_buf.st_mtime = (time_t)get_xfer_uint64(&msg[pos]);
               pos += 8;
            }
            if (scd.stat_flags & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
            {
               pos += 4;
            }
            if (scd.stat_flags & SSH_FILEXFER_ATTR_CTIME)
            {
               scd.stat_buf.st_ctime = (time_t)get_xfer_uint64(&msg[pos]);
               pos += 8;
            }
            if (scd.stat_flags & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
            {
               pos += 4;
            }

            if (p_stat_buf != NULL)
            {
               (void)memcpy(p_stat_buf, &scd.stat_buf, sizeof(struct stat));
            }
         }
         else if (msg[0] == SSH_FXP_STATUS)
              {
                 /* Some error has occured. */
                 get_msg_str(&msg[9]);
                 trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                           "sftp_stat(): %s", error_2_str(&msg[5]));
                 status = INCORRECT;
              }
              else
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "sftp_stat(): Expecting %d (SSH_FXP_HANDLE) but got %d (%s) as reply.",
                           SSH_FXP_HANDLE, (int)msg[0], response_2_str(msg[0]));
                 msg_str[0] = '\0';
                 status = INCORRECT;
              }
      }
   }
   return(status);
}


/*########################## sftp_open_file() ###########################*/
int
sftp_open_file(int    openmode,
               char   *filename,
               off_t  offset,
               mode_t *mode,
               int    blocksize,
               int    *buffer_offset,
               char   debug)
{
   int pos,
       status;

   if (scd.handle != NULL)
   {
      free(scd.handle);
      scd.handle = NULL;
   }

   /*
    * byte   SSH_FXP_OPEN
    * uint32 request_id
    * string filename [UTF-8]
    * [uint32 desired-access] Version 6+
    * uint32 flags
    * ATTRS  attrs
    */
   msg[4] = SSH_FXP_OPEN;
   scd.request_id++;
   set_xfer_uint(&msg[4 + 1], scd.request_id);
   if (scd.cwd == NULL)
   {
      status = strlen(filename);
      set_xfer_str(&msg[4 + 1 + 4], filename, status);
   }
   else
   {
      char fullname[MAX_PATH_LENGTH];

      status = sprintf(fullname, "%s/%s", scd.cwd, filename);
      set_xfer_str(&msg[4 + 1 + 4], fullname, status);
   }
   if (openmode == SFTP_WRITE_FILE)
   {
      if (scd.version >= 6)
      {
         set_xfer_uint(&msg[4 + 1 + 4 + 4 + status],
                       (offset == 0) ? ACE4_WRITE_DATA : ACE4_APPEND_DATA);
         set_xfer_uint(&msg[4 + 1 + 4 + 4 + status + 4],
                       (offset == 0) ? SSH_FXF_CREATE_TRUNCATE : SSH_FXF_OPEN_EXISTING);
         pos = 4 + 1 + 4 + 4 + status + 4 + 4;
      }
      else
      {
         set_xfer_uint(&msg[4 + 1 + 4 + 4 + status],
                       (SSH_FXF_WRITE | SSH_FXF_CREAT |
                       ((offset == 0) ? SSH_FXF_TRUNC : 0)));
         pos = 4 + 1 + 4 + 4 + status + 4;
      }
      if (mode == NULL)
      {
         set_xfer_uint(&msg[pos], 0);
         pos += 4;
         if (scd.version >= 6)
         {
            msg[pos] = SSH_FILEXFER_TYPE_REGULAR;
            pos++;
         }
      }
      else
      {
         set_xfer_uint(&msg[pos], SSH_FILEXFER_ATTR_PERMISSIONS);
         pos += 4;
         if (scd.version >= 6)
         {
            msg[pos] = SSH_FILEXFER_TYPE_REGULAR;
            pos++;
         }
         set_xfer_uint(&msg[pos], (unsigned int)(*mode));
         pos += 4;
      }
   }
   else if (openmode == SFTP_READ_FILE)
        {
           if (scd.version >= 6)
           {
              set_xfer_uint(&msg[4 + 1 + 4 + 4 + status], ACE4_READ_DATA);
              set_xfer_uint(&msg[4 + 1 + 4 + 4 + status + 4],
                            SSH_FXF_OPEN_EXISTING);
              pos = 4 + 1 + 4 + 4 + status + 4 + 4;
           }
           else
           {
              set_xfer_uint(&msg[4 + 1 + 4 + 4 + status], SSH_FXF_READ);
              pos = 4 + 1 + 4 + 4 + status + 4;
           }
           set_xfer_uint(&msg[pos], 0);
           pos += 4;
           if (scd.version >= 6)
           {
              msg[pos] = SSH_FILEXFER_TYPE_REGULAR;
              pos++;
           }
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "sftp_open_file(): Unknown open mode %d.", openmode);
           msg_str[0] = '\0';
           return(INCORRECT);
        }
   set_xfer_uint(msg, (unsigned int)(pos - 4));
   scd.debug = debug;
   if ((status = write_msg(msg, pos)) == SUCCESS)
   {
      if ((status = get_reply(scd.request_id)) == SUCCESS)
      {
         if (msg[0] == SSH_FXP_HANDLE)
         {
            if ((scd.handle_length = get_xfer_str(&msg[5], &scd.handle)) == 0)
            {
               status = INCORRECT;
            }
            else
            {
               scd.file_offset = offset;
               if (openmode == O_WRONLY)
               {
                  scd.pending_write_counter = -1;
                  scd.max_pending_writes = MAX_PENDING_WRITE_BUFFER / blocksize;
                  if (scd.max_pending_writes > MAX_PENDING_WRITES)
                  {
                     scd.max_pending_writes = MAX_PENDING_WRITES;
                  }
               }
               else
               {
                  scd.max_pending_writes = 0;
               }
               *buffer_offset = 4 + 1 + 4 + 4 + scd.handle_length + 8 + 4;
            }
         }
         else if (msg[0] == SSH_FXP_STATUS)
              {
                 /* Some error has occured. */
                 get_msg_str(&msg[9]);
                 trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                           "sftp_open_file(): %s", error_2_str(&msg[5]));
                 status = INCORRECT;
              }
              else
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "sftp_open_file(): Expecting %d (SSH_FXP_HANDLE) but got %d (%s) as reply.",
                           SSH_FXP_HANDLE, (int)msg[0], response_2_str(msg[0]));
                 msg_str[0] = '\0';
                 status = INCORRECT;
              }
      }
   }

   return(status);
}


/*########################## sftp_close_file() ##########################*/
int
sftp_close_file(void)
{
   int status = SUCCESS;

   /*
    * Before doing a close, catch all pending writes.
    */
   if (scd.pending_write_counter > 0)
   {
      status = sftp_flush();
   }

   if (status == SUCCESS)
   {
      /*
       * byte   SSH_FXP_CLOSE
       * uint32 request_id
       * string handle
       */
      msg[4] = SSH_FXP_CLOSE;
      scd.request_id++;
      set_xfer_uint(&msg[4 + 1], scd.request_id);
      set_xfer_str(&msg[4 + 1 + 4], scd.handle, scd.handle_length);
      set_xfer_uint(msg, (1 + 4 + 4 + scd.handle_length));
      if ((status = write_msg(msg, (4 + 1 + 4 + 4 + scd.handle_length))) == SUCCESS)
      {
         if ((status = get_reply(scd.request_id)) == SUCCESS)
         {
            if (msg[0] == SSH_FXP_STATUS)
            {
               if (get_xfer_uint(&msg[5]) != SSH_FX_OK)
               {
                  /* Some error has occured. */
                  get_msg_str(&msg[9]);
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                            "sftp_close_file(): %s", error_2_str(&msg[5]));
                  status = INCORRECT;
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "sftp_close_file(): Expecting %d (SSH_FXP_STATUS) but got %d (%s) as reply.",
                         SSH_FXP_STATUS, (int)msg[0], response_2_str(msg[0]));
               msg_str[0] = '\0';
               status = INCORRECT;
            }
         }
      }
   }

   /*
    * Regardless if an error has occurred, we may not try to reuse
    * the handle.
    */
   free(scd.handle);
   scd.handle = NULL;

   return(status);
}


/*############################ sftp_mkdir() #############################*/
int
sftp_mkdir(char *directory)
{
   int status;

   /*
    * byte   SSH_FXP_MKDIR
    * uint32 request_id
    * string path [UTF-8]
    * ATTRS  attrs
    */
   msg[4] = SSH_FXP_MKDIR;
   scd.request_id++;
   set_xfer_uint(&msg[4 + 1], scd.request_id);
   if (scd.cwd == NULL)
   {
      status = strlen(directory);
      set_xfer_str(&msg[4 + 1 + 4], directory, status);
   }
   else
   {
      char fullname[MAX_PATH_LENGTH];

      status = sprintf(fullname, "%s/%s", scd.cwd, directory);
      set_xfer_str(&msg[4 + 1 + 4], fullname, status);
   }
   set_xfer_uint(&msg[4 + 1 + 4 + 4 + status], 0);
   set_xfer_uint(msg, (1 + 4 + 4 + status + 4));
   if ((status = write_msg(msg, (4 + 1 + 4 + 4 + status + 4))) == SUCCESS)
   {
      if ((status = get_reply(scd.request_id)) == SUCCESS)
      {
         if (msg[0] == SSH_FXP_STATUS)
         {
            if (get_xfer_uint(&msg[5]) != SSH_FX_OK)
            {
               /* Some error has occured. */
               get_msg_str(&msg[9]);
               trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                         "sftp_mkdir(): %s", error_2_str(&msg[5]));
               status = INCORRECT;
            }
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "sftp_mkdir(): Expecting %d (SSH_FXP_STATUS) but got %d (%s) as reply.",
                      SSH_FXP_STATUS, (int)msg[0], response_2_str(msg[0]));
            msg_str[0] = '\0';
            status = INCORRECT;
         }
      }
   }

   return(status);
}


/*############################ sftp_move() ##############################*/
int
sftp_move(char *from, char *to, int create_dir)
{
   int from_length,
       pos,
       retries = 0,
       status,
       to_length;

   /*
    * byte   SSH_FXP_RENAME
    * uint32 request_id
    * string oldpath [UTF-8]
    * string newpath [UTF-8]
    * [uint32 flags]  Version 6+
    */
retry_move:
   msg[4] = SSH_FXP_RENAME;
   scd.request_id++;
   set_xfer_uint(&msg[4 + 1], scd.request_id);
   if (scd.cwd == NULL)
   {
      from_length = strlen(from);
      set_xfer_str(&msg[4 + 1 + 4], from, from_length);
      to_length = strlen(to);
      set_xfer_str(&msg[4 + 1 + 4 + 4 + from_length], to, to_length);
   }
   else
   {
      char fullname[MAX_PATH_LENGTH];

      from_length = sprintf(fullname, "%s/%s", scd.cwd, from);
      set_xfer_str(&msg[4 + 1 + 4], fullname, from_length);
      to_length = sprintf(fullname, "%s/%s", scd.cwd, to);
      set_xfer_str(&msg[4 + 1 + 4+ 4 + from_length], fullname, to_length);
   }
   pos = 4 + 1 + 4 + 4 + from_length + 4 + to_length;
   if (scd.version >= 6)
   {
      set_xfer_uint(&msg[pos], SSH_FXF_RENAME_OVERWRITE);
      pos += 4;
   }
   set_xfer_uint(msg, (pos - 4));
   if ((status = write_msg(msg, pos)) == SUCCESS)
   {
      if ((status = get_reply(scd.request_id)) == SUCCESS)
      {
         if (msg[0] == SSH_FXP_STATUS)
         {
            unsigned int ret_status;

            ret_status = get_xfer_uint(&msg[5]);
            if (ret_status != SSH_FX_OK)
            {
               /*
                * In version 3 the default behaviour is to fail
                * when we try to overwrite an existing file.
                * So we must delete it and then retry.
                */
               if ((((ret_status == SSH_FX_FAILURE) && (scd.version < 6)) ||
                    ((ret_status == SSH_FX_NO_SUCH_FILE) && (create_dir == YES) &&
                     (is_with_path(to) == YES))) &&
                   (retries == 0))
               {
                  if (ret_status == SSH_FX_NO_SUCH_FILE)
                  {
                     char full_to[MAX_PATH_LENGTH],
                          *ptr;

                     ptr = to + strlen(to) - 1;
                     while ((*ptr != '/') && (ptr != to))
                     {
                        ptr--;
                     }
                     if (*ptr == '/')
                     {
                        char *p_to,
                             *tmp_cwd = scd.cwd;

                        *ptr = '\0';
                        if (tmp_cwd == NULL)
                        {
                           p_to = to;
                        }
                        else
                        {
                           scd.cwd = NULL;
                           (void)sprintf(full_to, "%s/%s", tmp_cwd, to);
                           p_to = full_to;
                        }

                        /*
                         * NOTE: We do NOT want to go into this directory.
                         *       We just misuse sftp_cd() to create the
                         *       directory for us, nothing more.
                         */
                        if ((status = sftp_cd(p_to, YES)) == SUCCESS)
                        {
                           retries++;
                           *ptr = '/';
                           free(scd.cwd);
                           scd.cwd = tmp_cwd;
                           goto retry_move;
                        }
                     }
                     else
                     {
                        trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                                  "Hmm, something wrong here bailing out.");
                        msg_str[0] = '\0';
                        status = INCORRECT;
                     }
                  }
                  else
                  {
                     /* Assuming file already exists, so delete it and retry. */
                     if ((status = sftp_dele(to)) == SUCCESS)
                     {
                        retries++;
                        goto retry_move;
                     }
                  }
               }
               else
               {
                  /* Some error has occured. */
                  get_msg_str(&msg[9]);
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                            "sftp_move(): %s", error_2_str(&msg[5]));
                  status = INCORRECT;
               }
            }
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "sftp_move(): Expecting %d (SSH_FXP_STATUS) but got %d (%s) as reply.",
                      SSH_FXP_STATUS, (int)msg[0], response_2_str(msg[0]));
            msg_str[0] = '\0';
            status = INCORRECT;
         }
      }
   }
   return(status);
}


/*############################# sftp_write() ############################*/
int                                                                        
sftp_write(char *block, int size)
{
   int status;

   /*
    * byte   SSH_FXP_WRITE
    * uint32 request_id
    * string handle
    * uint64 offset
    * string data
    */
   msg[4] = SSH_FXP_WRITE;
   scd.request_id++;
   set_xfer_uint(&msg[4 + 1], scd.request_id);
   set_xfer_str(&msg[4 + 1 + 4], scd.handle, scd.handle_length);
   set_xfer_uint64(&msg[4 + 1 + 4 + 4 + scd.handle_length], scd.file_offset);
   set_xfer_str(&msg[4 + 1 + 4 + 4 + scd.handle_length + 8], block, size);
   set_xfer_uint(msg, (1 + 4 + 4 + scd.handle_length + 8 + 4 + size));
   if ((status = write_msg(msg, (4 + 1 + 4 + 4 + scd.handle_length + 8 + 4 + size))) == SUCCESS)
   {
      if ((scd.pending_write_counter != -1) &&
          (scd.pending_write_counter < scd.max_pending_writes))
      {
         scd.pending_write_id[scd.pending_write_counter] = scd.request_id;
         scd.pending_write_counter++;
         scd.file_offset += size;
      }
      else
      {
         if ((status = get_write_reply(scd.request_id)) == SUCCESS)
         {
            if (msg[0] == SSH_FXP_STATUS)
            {
               if (get_xfer_uint(&msg[5]) != SSH_FX_OK)
               {
                  /* Some error has occured. */
                  get_msg_str(&msg[9]);
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                            "sftp_write(): %s", error_2_str(&msg[5]));
                  status = INCORRECT;
               }
               else
               {
                  scd.file_offset += size;
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "sftp_write(): Expecting %d (SSH_FXP_STATUS) but got %d (%s) as reply.",
                         SSH_FXP_STATUS, (int)msg[0], response_2_str(msg[0]));
               msg_str[0] = '\0';
               status = INCORRECT;
            }
         }
      }
   }
   return(status);
}


/*############################## sftp_read() ############################*/
int                                                                        
sftp_read(char *block, int size)
{
   int status;

   /*
    * byte   SSH_FXP_READ
    * uint32 request_id
    * string handle
    * uint64 offset
    * uint32 length
    */
   msg[4] = SSH_FXP_READ;
   scd.request_id++;
   set_xfer_uint(&msg[4 + 1], scd.request_id);
   set_xfer_str(&msg[4 + 1 + 4], scd.handle, scd.handle_length);
   set_xfer_uint64(&msg[4 + 1 + 4 + 4 + scd.handle_length], scd.file_offset);
   set_xfer_uint(&msg[4 + 1 + 4 + 4 + scd.handle_length + 8], (unsigned int)size);
   set_xfer_uint(msg, (1 + 4 + 4 + scd.handle_length + 8));
   if ((status = write_msg(msg, (5 + 4 + scd.handle_length + 8 + 4))) == SUCCESS)
   {
      if ((status = get_reply(scd.request_id)) == SUCCESS)
      {
         if (msg[0] == SSH_FXP_DATA)
         {
            if ((status = get_xfer_str(&msg[5], &block)) == 0)
            {
               status = INCORRECT;
            }
            else
            {
               scd.file_offset += status;
            }
         }
         else if (msg[0] == SSH_FXP_STATUS)
              {
                 /* Some error has occured. */
                 get_msg_str(&msg[9]);
                 trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                           "sftp_read(): %s", error_2_str(&msg[5]));
                 status = INCORRECT;
              }
              else
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "sftp_read(): Expecting %d (SSH_FXP_DATA) but got %d (%s) as reply.",
                           SSH_FXP_DATA, (int)msg[0], response_2_str(msg[0]));
                 msg_str[0] = '\0';
                 status = INCORRECT;
              }
      }
   }
   return(status);
}


/*############################ sftp_flush() #############################*/
int
sftp_flush(void)
{
   if (scd.pending_write_counter > 0)
   {
      int i;

      for (i = 0; i < scd.pending_write_counter; i++)
      {
         if (get_reply(scd.pending_write_id[i]) == SUCCESS)
         {
            if (msg[0] == SSH_FXP_STATUS)
            {
               if (get_xfer_uint(&msg[5]) != SSH_FX_OK)
               {
                  /* Some error has occured. */
                  get_msg_str(&msg[9]);
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                            "sftp_flush(): %s", error_2_str(&msg[5]));
                  return(INCORRECT);
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "sftp_flush(): Expecting %d (SSH_FXP_STATUS) but got %d (%s) as reply.",
                         SSH_FXP_STATUS, (int)msg[0], response_2_str(msg[0]));
               msg_str[0] = '\0';
               return(INCORRECT);
            }
         }
         else
         {
            return(INCORRECT);
         }
      }
      scd.pending_write_counter = 0;
   }

   return(SUCCESS);
}


/*############################# sftp_dele() #############################*/
int
sftp_dele(char *filename)
{
   int status;

   /*
    * byte   SSH_FXP_REMOVE
    * uint32 request_id
    * string filename [UTF-8]
    */
   msg[4] = SSH_FXP_REMOVE;
   scd.request_id++;
   set_xfer_uint(&msg[4 + 1], scd.request_id);
   if (scd.cwd == NULL)
   {
      status = strlen(filename);
      set_xfer_str(&msg[4 + 1 + 4], filename, status);
   }
   else
   {
      char fullname[MAX_PATH_LENGTH];

      status = sprintf(fullname, "%s/%s", scd.cwd, filename);
      set_xfer_str(&msg[4 + 1 + 4], fullname, status);
   }
   set_xfer_uint(msg, (1 + 4 + 4 + status));
   if ((status = write_msg(msg, (4 + 1 + 4 + 4 + status))) == SUCCESS)
   {
      if ((status = get_reply(scd.request_id)) == SUCCESS)
      {
         if (msg[0] == SSH_FXP_STATUS)
         {
            if (get_xfer_uint(&msg[5]) != SSH_FX_OK)
            {
               /* Some error has occured. */
               get_msg_str(&msg[9]);
               trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                         "sftp_dele(): %s", error_2_str(&msg[5]));
               status = INCORRECT;
            }
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "sftp_dele(): Expecting %d (SSH_FXP_STATUS) but got %d (%s) as reply.",
                      SSH_FXP_STATUS, (int)msg[0], response_2_str(msg[0]));
            msg_str[0] = '\0';
            status = INCORRECT;
         }
      }
   }
   return(status);
}


/*############################# sftp_quit() #############################*/
void
sftp_quit(void)
{
   /* Free all allocated memory. */
   if (scd.cwd != NULL)
   {
      free(scd.cwd);
      scd.cwd = NULL;
   }
   if (scd.handle != NULL)
   {
      free(scd.handle);
      scd.handle = NULL;
   }
   if (scd.stored_replies > 0)
   {
      int i;

      for (i = 0; i < scd.stored_replies; i++)
      {
         free(scd.sm[i].sm_buffer);
      }
   }
   if (msg != NULL)
   {
      free(msg);
      msg = NULL;
   }

   /* Remove ssh process for writing data. */
   if (data_pid != -1)
   {
      int loop_counter,
          max_waitpid_loops,
          status;

      /* Close pipe for read/write data connection. */
      if (data_fd != -1)
      {
         if (close(data_fd) == -1)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                      "sftp_quit(): Failed to close() write pipe to ssh process : %s",
                      strerror(errno));
         }
         data_fd = -1;
      }

      errno = 0;
      loop_counter = 0;
      max_waitpid_loops = (transfer_timeout / 2) * 10;
      while ((waitpid(data_pid, &status, WNOHANG) != data_pid) &&
             (loop_counter < max_waitpid_loops))
      {
         my_usleep(100000L);
         loop_counter++;
      }
      if ((errno != 0) || (loop_counter >= max_waitpid_loops))
      {
         msg_str[0] = '\0';
         if (errno != 0)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                      "sftp_quit(): Failed to catch zombie of data ssh process : %s",
                      strerror(errno));
         }
         if (data_pid > 0)
         {
            if (kill(data_pid, SIGKILL) == -1)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "sftp_quit(): Failed to kill() data ssh process %d : %s",
                         data_pid, strerror(errno));
            }
            else
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                         "sftp_quit(): Killing hanging data ssh process.");
            }
         }
         else
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
# if SIZEOF_PID_T == 4
                      "sftp_quit(): Hmm, pid is %d!!!", data_pid);
# else
                      "sftp_quit(): Hmm, pid is %lld!!!", data_pid);
# endif
         }
      }
      data_pid = -1;
   }

   return;
}


/*++++++++++++++++++++++++++++ get_reply() ++++++++++++++++++++++++++++++*/
static int
get_reply(unsigned int id)
{
   int reply;

   if (scd.stored_replies > 0)
   {
      int i;

      for (i = 0; i < scd.stored_replies; i++)
      {
         if (scd.sm[i].request_id == id)
         {
            /* Save content of stored message to buffer msg. */
            (void)memcpy(msg, scd.sm[i].sm_buffer, scd.sm[i].message_length);

            /* Remove reply from buffer and free its memory. */
            free(scd.sm[i].sm_buffer);
            if ((scd.stored_replies > 1) && (i != (scd.stored_replies - 1)))
            {
               size_t move_size = (scd.stored_replies - 1 - i) *
                                  sizeof(struct stored_messages);

               (void)memmove(&scd.sm[i], &scd.sm[i + 1], move_size);
            }
            scd.stored_replies--;

            return(SUCCESS);
         }
      }
   }

retry:
   if ((reply = read_msg(msg, 4)) == SUCCESS)
   {
      unsigned int msg_length;

      msg_length = get_xfer_uint(msg);
      if (msg_length <= MAX_SFTP_MSG_LENGTH)
      {
         if ((reply = read_msg(msg, (int)msg_length)) == SUCCESS)
         {
            unsigned int reply_id;

            reply_id = get_xfer_uint(&msg[1]);
            if (reply_id != id)
            {
               if (scd.stored_replies == MAX_SFTP_REPLY_BUFFER)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            "get_reply(): Only able to queue %d replies, try increase MAX_SFTP_REPLY_BUFFER and recompile.",
                            MAX_SFTP_REPLY_BUFFER);
                  reply = INCORRECT;
               }
               else
               {
                  if ((scd.sm[scd.stored_replies].sm_buffer = malloc(msg_length)) == NULL)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "get_reply(): Unable to malloc() %u bytes : %s",
                               msg_length, strerror(errno));
                     reply = INCORRECT;
                  }
                  else
                  {
                     (void)memcpy(scd.sm[scd.stored_replies].sm_buffer,
                                  msg, msg_length);
                     scd.stored_replies++;

                     goto retry;
                  }
               }
            }
         }
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "get_reply(): Received message is %u bytes, can only handle %d bytes.",
                   msg_length, MAX_SFTP_MSG_LENGTH);
         reply = INCORRECT;
      }
   }

   return(reply);
}


/*++++++++++++++++++++++++++ get_write_reply() ++++++++++++++++++++++++++*/
static int
get_write_reply(unsigned int id)
{
   int reply;

   if (scd.pending_write_counter == -1)
   {
      if ((reply = get_reply(id)) == SUCCESS)
      {
         scd.pending_write_counter = 0;
      }
   }
   else
   {
      register int i;
      int          got_current_id = NO,
                   gotcha;
      unsigned int msg_length,
                   reply_id;

      do
      {
         if ((reply = read_msg(msg, 4)) == SUCCESS)
         {
            msg_length = get_xfer_uint(msg);
            if (msg_length <= MAX_SFTP_MSG_LENGTH)
            {
               if ((reply = read_msg(msg, (int)msg_length)) == SUCCESS)
               {
                  gotcha = NO;
                  reply_id = get_xfer_uint(&msg[1]);

                  for (i = 0; i < scd.pending_write_counter; i++)
                  {
                     if (reply_id == scd.pending_write_id[i])
                     {
                        if ((scd.pending_write_counter > 1) &&
                            (i != (scd.pending_write_counter - 1)))
                        {
                           size_t move_size = (scd.pending_write_counter - 1 - i) *
                                              sizeof(unsigned int);

                           (void)memmove(&scd.pending_write_id[i],
                                         &scd.pending_write_id[i + 1],
                                         move_size);
                        }
                        scd.pending_write_counter--;
                        gotcha = YES;
                        break;
                     }
                  }
                  if (gotcha == NO)
                  {
                     if (got_current_id == NO)
                     {
                        if (reply_id == id)
                        {
                           got_current_id = YES;
                           gotcha = YES;
                        }
                     }
                     if (gotcha == NO)
                     {
                        if (scd.stored_replies == MAX_SFTP_REPLY_BUFFER)
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                     "get_write_reply(): Only able to queue %d replies, try increase MAX_SFTP_REPLY_BUFFER and recompile.",
                                     MAX_SFTP_REPLY_BUFFER);
                           reply = INCORRECT;
                        }
                        else
                        {
                           if ((scd.sm[scd.stored_replies].sm_buffer = malloc(msg_length)) == NULL)
                           {
                              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                        "get_write_reply(): Unable to malloc() %u bytes : %s",
                                        msg_length, strerror(errno));
                              reply = INCORRECT;
                           }
                           else
                           {
                              (void)memcpy(scd.sm[scd.stored_replies].sm_buffer,
                                           msg, msg_length);
                              scd.stored_replies++;
                           }
                        }
                     }
                  }
                  if (gotcha == YES)
                  {
                     if ((msg[0] == SSH_FXP_STATUS) &&
                         (get_xfer_uint(&msg[5]) == SSH_FX_OK))
                     {
                        reply = SUCCESS;
                     }
                     else
                     {
                        reply = INCORRECT;
                     }
                  }
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "get_write_reply(): Received message is %u bytes, can only handle %d bytes.",
                         msg_length, MAX_SFTP_MSG_LENGTH);
               reply = INCORRECT;
            }
         }
      } while ((reply == SUCCESS) &&
               ((scd.pending_write_counter > 0) || (got_current_id == NO)) &&
               ((scd.pending_write_counter == scd.max_pending_writes) ||
                (check_msg_pending() == YES)));

      if ((got_current_id == NO) && (reply == SUCCESS) &&
          (scd.pending_write_counter < scd.max_pending_writes))
      {
         scd.pending_write_id[scd.pending_write_counter] = id;
         scd.pending_write_counter++;
      }
   }

   return(reply);
}


/*-------------------------- check_msg_pending() ------------------------*/
static int
check_msg_pending(void)
{
   int            status;
   fd_set         rset;
   struct timeval timeout;

   /* Initialise descriptor set */
   FD_ZERO(&rset);
   FD_SET(data_fd, &rset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = 0L;

   /* Wait for message x seconds and then continue. */
   status = select(data_fd + 1, &rset, NULL, NULL, &timeout);

   if (status == 0)
   {
      /* Nothing in the pipe. */
      status = NO;
   }
   else if (FD_ISSET(data_fd, &rset))
        {
           status = YES;
        }
        else
        {
           status = NO;
        }

   return(status);
}


/*+++++++++++++++++++++++++++++ write_msg() +++++++++++++++++++++++++++++*/
static int
write_msg(char *block, int size)
{
#ifdef WITH_TRACE
   int            continue_show = NO,
                  type,
                  what_to_show;
#endif
   int            nleft,
                  status;
   char           *ptr;
   fd_set         wset;
   struct timeval timeout;

   /* Initialise descriptor set */
   FD_ZERO(&wset);

   ptr = block;
   nleft = size;
   while (nleft > 0)
   {
      FD_SET(data_fd, &wset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      /* Wait for message x seconds and then continue. */
      status = select(data_fd + 1, NULL, &wset, NULL, &timeout);

      if (status == 0)
      {
         /* timeout has arrived */
         timeout_flag = ON;
         return(INCORRECT);
      }
      else if (FD_ISSET(data_fd, &wset))
           {
              /* In some cases, the write system call hangs. */
              signal(SIGALRM, sig_ignore_handler); /* Ignore the default which */
                                                   /* is to abort.             */
              siginterrupt(SIGALRM, 1); /* Allow SIGALRM to interrupt write. */

              alarm(transfer_timeout); /* Set up an alarm to interrupt write. */
              if ((status = write(data_fd, ptr, nleft)) <= 0)
              {
                 int tmp_errno = errno;

                 alarm(0);
                 signal(SIGALRM, SIG_DFL); /* restore the default which is abort. */
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "write_msg(): write() error (%d) : %s",
                           status, strerror(tmp_errno));
                 return(tmp_errno);
              }
              alarm(0);
              signal(SIGALRM, SIG_DFL); /* Restore the default which is abort. */
#ifdef WITH_TRACE
              if (scd.debug == TRACE_MODE)
              {
                 if ((nleft == size) && (status > 4))
                 {
                    if (*(ptr + 4) == SSH_FXP_WRITE)
                    {
                       if (status < (4 + 1 + 4 + 4 + scd.handle_length + 8 + 4))
                       {
                          what_to_show = status;
                       }
                       else
                       {
                          what_to_show = 4 + 1 + 4 + 4 + scd.handle_length + 8 + 4;
                       }
                    }
                    else
                    {
                       what_to_show = status;
                       continue_show = YES;
                    }
                 }
                 else
                 {
                    if ((continue_show == YES) ||
                        ((nleft == size) && (status < 5)))
                    {
                       what_to_show = status;
                    }
                    else
                    {
                       what_to_show = 0;
                    }
                 }
                 type = BIN_CMD_W_TRACE;
              }
              else if (scd.debug == FULL_TRACE_MODE)
                   {
                      what_to_show = status;
                      type = BIN_W_TRACE;
                   }
                   else
                   {
                      what_to_show = 0;
                   }
              if (what_to_show > 0)
              {
                 trace_log(NULL, 0, type, block, what_to_show, NULL);
              }
#endif
              nleft -= status;
              ptr   += status;
           }
      else if (status < 0)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "write_msg(): select() error : %s", strerror(errno));
              return(INCORRECT);
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "write_msg(): Unknown condition.");
              return(INCORRECT);
           }
   }
   
   return(SUCCESS);
}


/*++++++++++++++++++++++++++++++ read_msg() +++++++++++++++++++++++++++++*/
int
read_msg(char *block, int blocksize)
{
#ifdef WITH_TRACE
   int            type;
#endif
   int            bytes_read,
                  status;
   fd_set         rset;
   struct timeval timeout;

   /* Initialise descriptor set */
   FD_ZERO(&rset);
   FD_SET(data_fd, &rset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(data_fd + 1, &rset, NULL, NULL, &timeout);

   if (status == 0)
   {
      /* timeout has arrived */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (FD_ISSET(data_fd, &rset))
        {
           signal(SIGALRM, sig_ignore_handler); /* Ignore the default which */
                                                /* is to abort.             */
           siginterrupt(SIGALRM, 1); /* Allow SIGALRM to interrupt read. */

           alarm(transfer_timeout); /* Set up an alarm to interrupt read. */
           if ((bytes_read = read(data_fd, block, blocksize)) == -1)
           {
              int tmp_errno = errno;

              alarm(0);
              signal(SIGALRM, SIG_DFL); /* restore the default which is abort. */
              if (tmp_errno == ECONNRESET)
              {
                 timeout_flag = CON_RESET;
              }
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "read_msg(): read() error : %s", strerror(tmp_errno));
              return(INCORRECT);
           }
           alarm(0);
           signal(SIGALRM, SIG_DFL); /* Restore the default which is abort. */
#ifdef WITH_TRACE
           if (scd.debug == TRACE_MODE)
           {
              if ((bytes_read > 4) && (*(block + 4) == SSH_FXP_DATA))
              {
                 /*
                  * From a SSH_FXP_READ request we just want to see
                  * the beginning, not the data.
                  * NOTE: Since we can show 16 bytes on one line, lets
                  *       always show the first 3 bytes of data as well.
                  */
                 if (bytes_read < (4 + 1 + 4 + 4 + 3))
                 {
                    status = bytes_read;
                 }
                 else
                 {
                    status = 4 + 1 + 4 + 4 + 3;
                 }
              }
              else
              {
                 status = bytes_read;
              }
              type = BIN_CMD_R_TRACE;
           }
           else if (scd.debug == FULL_TRACE_MODE)
                {
                   status = bytes_read;
                   type = BIN_R_TRACE;
                }
                else
                {
                   status = 0;
                }
           if (status > 0)
           {
              trace_log(NULL, 0, type, block, status, NULL);
           }
#endif
        }
   else if (status < 0)
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "read_msg(): select() error : %s", strerror(errno));
           return(INCORRECT);
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "read_msg(): Unknown condition.");
           return(INCORRECT);
        }

   return(SUCCESS);
}


/*++++++++++++++++++++++++++ get_xfer_uint() ++++++++++++++++++++++++++++*/
static unsigned int
get_xfer_uint(char *msg)
{
   unsigned int ui_var;

   if (*(char *)&byte_order == 1)
   {
      /* little-endian */
      ((char *)&ui_var)[3] = msg[0];
      ((char *)&ui_var)[2] = msg[1];
      ((char *)&ui_var)[1] = msg[2];
      ((char *)&ui_var)[0] = msg[3];
   }
   else
   {
      /* big-endian */
      ((char *)&ui_var)[0] = msg[0];
      ((char *)&ui_var)[1] = msg[1];
      ((char *)&ui_var)[2] = msg[2];
      ((char *)&ui_var)[3] = msg[3];
   }

   return(ui_var);
}


/*+++++++++++++++++++++++++ get_xfer_uint64() +++++++++++++++++++++++++++*/
static u_long_64
get_xfer_uint64(char *msg)
{
   u_long_64 ui_var;

   if (*(char *)&byte_order == 1)
   {
      /* little-endian */
#ifdef HAVE_LONG_LONG
      ((char *)&ui_var)[7] = msg[0];
      ((char *)&ui_var)[6] = msg[1];
      ((char *)&ui_var)[5] = msg[2];
      ((char *)&ui_var)[4] = msg[3];
#endif
      ((char *)&ui_var)[3] = msg[4];
      ((char *)&ui_var)[2] = msg[5];
      ((char *)&ui_var)[1] = msg[6];
      ((char *)&ui_var)[0] = msg[7];
   }
   else
   {
      /* big-endian */
      ((char *)&ui_var)[0] = msg[0];
      ((char *)&ui_var)[1] = msg[1];
      ((char *)&ui_var)[2] = msg[2];
      ((char *)&ui_var)[3] = msg[3];
#ifdef HAVE_LONG_LONG
      ((char *)&ui_var)[4] = msg[4];
      ((char *)&ui_var)[5] = msg[5];
      ((char *)&ui_var)[6] = msg[6];
      ((char *)&ui_var)[7] = msg[7];
#endif
   }

   return(ui_var);
}


/*++++++++++++++++++++++++++ get_xfer_str() +++++++++++++++++++++++++++++*/
static int
get_xfer_str(char *msg, char **p_xfer_str)
{
   unsigned int ui_var;

   if (*(char *)&byte_order == 1)
   {
      /* little-endian */
      ((char *)&ui_var)[3] = msg[0];
      ((char *)&ui_var)[2] = msg[1];
      ((char *)&ui_var)[1] = msg[2];
      ((char *)&ui_var)[0] = msg[3];
   }
   else
   {
      /* big-endian */
      ((char *)&ui_var)[0] = msg[0];
      ((char *)&ui_var)[1] = msg[1];
      ((char *)&ui_var)[2] = msg[2];
      ((char *)&ui_var)[3] = msg[3];
   }
   if (ui_var <= MAX_SFTP_MSG_LENGTH)
   {
      if (p_xfer_str != NULL)
      {
         if ((*p_xfer_str = malloc((ui_var + 1))) == NULL)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "get_xfer_str(): Failed to malloc() %u bytes : %s",
                      ui_var, strerror(errno));
            ui_var = 0;
         }
         else
         {
            (void)memcpy(*p_xfer_str, &msg[4], ui_var);
            *(*p_xfer_str + ui_var) = '\0';
         }
      }
   }
   else
   {
      ui_var = 0;
   }

   return(ui_var);
}


/*+++++++++++++++++++++++++++ get_msg_str() +++++++++++++++++++++++++++++*/
static void
get_msg_str(char *msg)
{
   unsigned int ui_var;

   if (*(char *)&byte_order == 1)
   {
      /* little-endian */
      ((char *)&ui_var)[3] = msg[0];
      ((char *)&ui_var)[2] = msg[1];
      ((char *)&ui_var)[1] = msg[2];
      ((char *)&ui_var)[0] = msg[3];
   }
   else
   {
      /* big-endian */
      ((char *)&ui_var)[0] = msg[0];
      ((char *)&ui_var)[1] = msg[1];
      ((char *)&ui_var)[2] = msg[2];
      ((char *)&ui_var)[3] = msg[3];
   }
   if (ui_var > (MAX_RET_MSG_LENGTH - 1))
   {
      ui_var = MAX_RET_MSG_LENGTH - 1;
   }
   (void)memcpy(msg_str, &msg[4], ui_var);
   *(msg_str + ui_var) = '\0';

   return;
}


/*++++++++++++++++++++++++++ set_xfer_uint() ++++++++++++++++++++++++++++*/
static void
set_xfer_uint(char *msg, unsigned int ui_var)
{
   if (*(char *)&byte_order == 1)
   {
      /* little-endian */
      msg[0] = ((char *)&ui_var)[3];
      msg[1] = ((char *)&ui_var)[2];
      msg[2] = ((char *)&ui_var)[1];
      msg[3] = ((char *)&ui_var)[0];
   }
   else
   {
      /* big-endian */
      msg[0] = ((char *)&ui_var)[0];
      msg[1] = ((char *)&ui_var)[1];
      msg[2] = ((char *)&ui_var)[2];
      msg[3] = ((char *)&ui_var)[3];
   }

   return;
}


/*+++++++++++++++++++++++++ set_xfer_uint64() +++++++++++++++++++++++++++*/
static void
set_xfer_uint64(char *msg, u_long_64 ui_var)
{
   if (*(char *)&byte_order == 1)
   {
      /* little-endian */
#ifdef HAVE_LONG_LONG
      msg[0] = ((char *)&ui_var)[7];
      msg[1] = ((char *)&ui_var)[6];
      msg[2] = ((char *)&ui_var)[5];
      msg[3] = ((char *)&ui_var)[4];
#else
      msg[0] = 0;
      msg[1] = 0;
      msg[2] = 0;
      msg[3] = 0;
#endif
      msg[4] = ((char *)&ui_var)[3];
      msg[5] = ((char *)&ui_var)[2];
      msg[6] = ((char *)&ui_var)[1];
      msg[7] = ((char *)&ui_var)[0];
   }
   else
   {
      /* big-endian */
      msg[0] = ((char *)&ui_var)[0];
      msg[1] = ((char *)&ui_var)[1];
      msg[2] = ((char *)&ui_var)[2];
      msg[3] = ((char *)&ui_var)[3];
#ifdef HAVE_LONG_LONG
      msg[4] = ((char *)&ui_var)[4];
      msg[5] = ((char *)&ui_var)[5];
      msg[6] = ((char *)&ui_var)[6];
      msg[7] = ((char *)&ui_var)[7];
#else
      msg[4] = 0;
      msg[5] = 0;
      msg[6] = 0;
      msg[7] = 0;
#endif
   }

   return;
}


/*++++++++++++++++++++++++++ set_xfer_str() +++++++++++++++++++++++++++++*/
static void
set_xfer_str(char *msg, char *p_xfer_str, int length)
{
   if (*(char *)&byte_order == 1)
   {
      /* little-endian */
      msg[0] = ((char *)&length)[3];
      msg[1] = ((char *)&length)[2];
      msg[2] = ((char *)&length)[1];
      msg[3] = ((char *)&length)[0];
   }
   else
   {
      /* big-endian */
      msg[0] = ((char *)&length)[0];
      msg[1] = ((char *)&length)[1];
      msg[2] = ((char *)&length)[2];
      msg[3] = ((char *)&length)[3];
   }
   (void)memcpy(&msg[4], p_xfer_str, length);

   return;
}


/*++++++++++++++++++++++++++ error_2_str() ++++++++++++++++++++++++++++++*/
static char *
error_2_str(char *msg)
{
   unsigned int error_code;

   error_code = get_xfer_uint(msg);
   switch (error_code)
   {
      case SSH_FX_OK                       : /*  0 */
         return("No error. (0)");
      case SSH_FX_EOF                      : /*  1 */
         return("Attempted to read past the end-of-file or there are no more directory entries. (1)");
      case SSH_FX_NO_SUCH_FILE             : /*  2 */
         return("A reference was made to a file which does not exist. (2)");
      case SSH_FX_PERMISSION_DENIED        : /*  3 */
         return("Permission denied. (3)");
      case SSH_FX_FAILURE                  : /*  4 */
         return("An error occurred, but no specific error code exists to describe the failure. (4)");
      case SSH_FX_BAD_MESSAGE              : /*  5 */
         return("A badly formatted packet or other SFTP protocol incompatibility was detected. (5)");
      case SSH_FX_NO_CONNECTION            : /*  6 */
         return("There is no connection to the server. (6)");
      case SSH_FX_CONNECTION_LOST          : /*  7 */
         return("The connection to the server was lost. (7)");
      case SSH_FX_OP_UNSUPPORTED           : /*  8 */
         return("Operation unsupported. (8)");
      case SSH_FX_INVALID_HANDLE           : /*  9 */
         return("The handle value was invalid. (9)");
      case SSH_FX_NO_SUCH_PATH             : /* 10 */
         return("File path does not exist or is invalid. (10)");
      case SSH_FX_FILE_ALREADY_EXISTS      : /* 11 */
         return("File already exists. (11)");
      case SSH_FX_WRITE_PROTECT            : /* 12 */
         return("File is on read-only media, or the media is write protected. (12)");
      case SSH_FX_NO_MEDIA                 : /* 13 */
         return("The requested operation cannot be completed because there is no media available in the drive. (13)");
      case SSH_FX_NO_SPACE_ON_FILESYSTEM   : /* 14 */
         return("No space on filesystem. (14)");
      case SSH_FX_QUOTA_EXCEEDED           : /* 15 */
         return("Quota exceeded. (15)");
      case SSH_FX_UNKNOWN_PRINCIPAL        : /* 16 */
         return("Unknown principal. (16)");
      case SSH_FX_LOCK_CONFLICT            : /* 17 */
         return("File could not be opened because it is locked by another process. (17)");
      case SSH_FX_DIR_NOT_EMPTY            : /* 18 */
         return("Directory is not empty. (18)");
      case SSH_FX_NOT_A_DIRECTORY          : /* 19 */
         return("The specified file is not a directory. (19)");
      case SSH_FX_INVALID_FILENAME         : /* 20 */
         return("Invalid filename. (20)");
      case SSH_FX_LINK_LOOP                : /* 21 */
         return("Too many symbolic links encountered. (21)");
      case SSH_FX_CANNOT_DELETE            : /* 22 */
         return("File cannot be deleted. (22)");
      case SSH_FX_INVALID_PARAMETER        : /* 23 */
         return("Invalid parameter. (23)");
      case SSH_FX_FILE_IS_A_DIRECTORY      : /* 24 */
         return("File is a directory. (24)");
      case SSH_FX_BYTE_RANGE_LOCK_CONFLICT : /* 25 */
         return("Byte range lock conflict. (25)");
      case SSH_FX_BYTE_RANGE_LOCK_REFUSED  : /* 26 */
         return("Byte range lock refused. (26)");
      case SSH_FX_DELETE_PENDING           : /* 27 */
         return("Delete is pending. (27)");
      case SSH_FX_FILE_CORRUPT             : /* 28 */
         return("File is corrupt. (28)");
      case SSH_FX_OWNER_INVALID            : /* 29 */
         return("Invalid owner. (29)");
      case SSH_FX_GROUP_INVALID            : /* 30 */
         return("Invalid group. (30)");
      default                              : /* ?? */
         (void)sprintf(msg_str, "Unknown error code. (%u)", error_code);
         return(msg_str);
   }
}


/*+++++++++++++++++++++++++ response_2_str() ++++++++++++++++++++++++++++*/
static char *
response_2_str(char response_type)
{
   switch (response_type)
   {
      case SSH_FXP_STATUS  : return("SSH_FXP_STATUS");
      case SSH_FXP_HANDLE  : return("SSH_FXP_HANDLE");
      case SSH_FXP_DATA    : return("SSH_FXP_DATA");
      case SSH_FXP_NAME    : return("SSH_FXP_NAME");
      case SSH_FXP_ATTRS   : return("SSH_FXP_ATTRS");
      case SSH_FXP_VERSION : return("SSH_FXP_VERSION");
      default              : return("Unknown response");
   }
}


/*+++++++++++++++++++++++++++ is_with_path() ++++++++++++++++++++++++++++*/
static int
is_with_path(char *name)
{
   while (*name != '\0')
   {
      if (*name == '/')
      {
         return(YES);
      }
      name++;
   }
   return(NO);
}


/*+++++++++++++++++++++++++ sig_ignore_handler() ++++++++++++++++++++++++*/
static void
sig_ignore_handler(int signo)
{
   /* Do nothing. This just allows us to receive a SIGALRM, thus */
   /* interrupting a system call without being forced to abort.  */
}

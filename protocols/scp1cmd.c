/*
 *  scp1cmd.c - Part of AFD, an automatic file distribution program.
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
 **   scp1cmd - commands to send files via the SCP1 protocol (SSH1)
 **
 ** SYNOPSIS
 **   int  scp1_connect(char *hostname, int port, char *user,
 **                     char *passwd, char *dir)
 **   int  scp1_open_file(char *filename, off_t size, mode_t mode)
 **   int  scp1_close_file(void)
 **   int  scp1_write(char *block, int size)
 **   int  scp1_chmod(char *filename, char *mode)
 **   int  scp1_move(char *from, char *to)
 **   void scp1_quit(void)
 **
 ** DESCRIPTION
 **   scp1cmd provides a set of commands to communicate with a SSH
 **   (Secure Shell) server via pipes. The following functions are
 **   required to send a file:
 **      scp1_connect()    - build a Tconnection to the SSH server
 **      scp1_open_file()  - open a file
 **      scp1_close_file() - close a file
 **      scp1_write()      - write data to the pipe
 **      scp1_chmod()      - change mode of a file
 **      scp1_move()       - rename a file
 **      scp1_quit()       - disconnect from the SSH server
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
 **   22.04.2001 H.Kiehl Created
 **   30.09.2001 H.Kiehl Added functions scp1_cmd_connect(), scp1_chmod()
 **                      and scp1_move().
 **
 */
DESCR__E_M3


#include <stdio.h>            /* fdopen(), fclose()                      */
#include <string.h>           /* memset(), memcpy()                      */
#include <stdlib.h>           /* malloc(), free()                        */
#include <sys/types.h>        /* fd_set                                  */
#include <sys/time.h>         /* struct timeval                          */
#include <sys/wait.h>         /* waitpid()                               */
#include <sys/stat.h>         /* S_ISUID, S_ISGID, etc                   */
#include <signal.h>           /* signal()                                */
#include <termios.h>
#include <unistd.h>           /* select(), exit(), write(), read(),      */
                              /* close()                                 */
#include <fcntl.h>            /* O_NONBLOCK                              */
#include <errno.h>
#include "scp1defs.h"
#include "fddefs.h"           /* struct job                              */


/* External global variables */
extern int            timeout_flag;
extern char           msg_str[];
extern long           transfer_timeout;
extern char           tr_hostname[];
extern struct job     db;

#ifdef _WITH_SCP1_SUPPORT
/* Local global variables */
static int            ctrl_fd = -1,
                      data_fd = -1;
static pid_t          ctrl_pid = -1,
                      data_pid = -1;
static struct timeval timeout;

/* Local function prototypes. */
static int            get_reply(int, int),
                      get_passwd_reply(int),
                      ptym_open(char *),
                      ptys_open(int, char *),
                      ssh_cmd(char *, char *, char *, char *, int *, pid_t *),
                      tty_raw(int);
#endif /* _WITH_SCP1_SUPPORT */


/*########################## scp1_connect() #############################*/
int
scp1_connect(char *hostname, int port, char *user, char *passwd, char *dir)
{
#ifdef _WITH_SCP1_SUPPORT
   char cmd[MAX_PATH_LENGTH];

   (void)sprintf(cmd, "scp -t %s", (dir[0] == '\0') ? "." : dir);

   return(ssh_cmd(hostname, user, passwd, cmd, &data_fd, &data_pid));
#else
   return(0);
#endif /* _WITH_SCP1_SUPPORT */
}


/*######################## scp1_cmd_connect() ###########################*/
int
scp1_cmd_connect(char *hostname, int port, char *user, char *passwd, char *dir)
{
#ifdef _WITH_SCP1_SUPPORT
   int status;

   if ((status = ssh_cmd(hostname, user, passwd, NULL, &ctrl_fd,
                          &ctrl_pid)) == INCORRECT)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to build SSH command connection.");
   }
   else
   {
      if (dir != NULL)
      {
         size_t length;
         char   cmd[MAX_PATH_LENGTH];

         length = sprintf(cmd, "cd %s\r\n", dir);
         if ((status = write(ctrl_fd, cmd, length)) != length)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to write() command <%s> to pipe [%d] : %s",
                      cmd, status, strerror(errno));
            status = INCORRECT;
         }
         else
         {
            if ((status = get_reply(ctrl_fd, YES)) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to send command <%s> [%d]", cmd, status);
            }
         }
      }
   }
   return(status);
#else
   return(0);
#endif /* _WITH_SCP1_SUPPORT */
}


/*########################## scp1_open_file() ###########################*/
int
scp1_open_file(char *filename, off_t size, mode_t mode)
{
#ifdef _WITH_SCP1_SUPPORT
   int    status;
   size_t length;
   char   cmd[MAX_PATH_LENGTH];

   length = sprintf(cmd, "C%04o %lu %s\n",
                    (mode & (S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO)),
                    size, filename);
   if ((status = write(data_fd, cmd, length)) != length)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to write() to pipe [%d] : %s", status, strerror(errno));
      status = INCORRECT;
   }
   else
   {
      if ((status = get_reply(data_fd, YES)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to open remote file <%s> [%d]", filename, status);
      }
   }
   return(status);
#else
   return(0);
#endif /* _WITH_SCP1_SUPPORT */
}


/*########################## scp1_close_file() ##########################*/
int
scp1_close_file(void)
{
#ifdef _WITH_SCP1_SUPPORT
   int status;

   if (write(data_fd, "\0", 1) != 1)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to write() [close file] to pipe : %s", strerror(errno));
      status = INCORRECT;
   }
   else
   {
      if ((status = get_reply(data_fd, YES)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to close remote file [%d]", status);
      }
   }
   return(status);
#else
   return(0);
#endif /* _WITH_SCP1_SUPPORT */
}


/*############################# scp1_write() ############################*/
int
scp1_write(char *block, int size)
{
#ifdef _WITH_SCP1_SUPPORT
   int    status;
   fd_set wset;

   /* Initialise descriptor set */
   FD_ZERO(&wset);
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
           if ((status = write(data_fd, block, size)) != size)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "write() error (%d) : %s", status, strerror(errno));
              return(errno);
           }
        }
   else if (status < 0)
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__,
                     "select() error : %s", strerror(errno));
           return(INCORRECT);
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__,
                     "Unknown condition.");
           return(INCORRECT);
        }
#endif /* _WITH_SCP1_SUPPORT */
   
   return(SUCCESS);
}


/*############################ scp1_chmod() #############################*/
int
scp1_chmod(char *filename, char *mode)
{
#ifdef _WITH_SCP1_SUPPORT
   int    status;
   size_t length;
   char   cmd[MAX_PATH_LENGTH];

   length = sprintf(cmd, "chmod %s %s\n", mode, filename);
   if ((status = write(ctrl_fd, cmd, length)) != length)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to write() command <%s> to pipe [%d] : %s",
                cmd, status, strerror(errno));
      status = INCORRECT;
   }
   else
   {
      if ((status = get_reply(ctrl_fd, YES)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to send command <%s> [%d]", cmd, status);
      }
   }
   return(status);
#else
   return(SUCCESS);
#endif /* _WITH_SCP1_SUPPORT */
}


/*############################ scp1_move() ##############################*/
int
scp1_move(char *from, char *to)
{
#ifdef _WITH_SCP1_SUPPORT
   int    status;
   size_t length;
   char   cmd[MAX_PATH_LENGTH];

   length = sprintf(cmd, "mv %s %s\n", from, to);
   if ((status = write(ctrl_fd, cmd, length)) != length)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to write() command <%s> to pipe [%d] : %s",
                cmd, status, strerror(errno));
      status = INCORRECT;
   }
   else
   {
      if ((status = get_reply(ctrl_fd, YES)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to send command <%s> [%d]", cmd, status);
      }
   }
   return(status);
#else
   return(SUCCESS);
#endif /* _WITH_SCP1_SUPPORT */
}


/*############################# scp1_quit() #############################*/
void
scp1_quit(void)
{
#ifdef _WITH_SCP1_SUPPORT
   /* Remove ssh process for controlling. */
   if (ctrl_pid != -1)
   {
      int status;

      /* Close pipe for read/write data connection. */
      if (ctrl_fd != -1)
      {
         char cmd[MAX_PATH_LENGTH];

         cmd[0] = 'e'; cmd[1] = 'x'; cmd[2] = 'i';
         cmd[3] = 't'; cmd[4] = '\n';
         if ((status = write(ctrl_fd, cmd, 5)) != 5)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to write() command <%s> to pipe [%d] : %s",
                      cmd, status, strerror(errno));
            status = INCORRECT;
         }
         else
         {
            if ((status = get_reply(ctrl_fd, YES)) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to send command <%s> [%d]", cmd, status);
            }
         }
         if (close(ctrl_fd) == -1)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__,
                      "Failed to close() write pipe to ssh process : %s",
                      strerror(errno));
         }
         ctrl_fd = -1;
      }

      if (waitpid(ctrl_pid, &status, 0) != ctrl_pid)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__,
                   "Failed to catch zombie of controlling process : %s",
                   strerror(errno));
      }
      ctrl_pid = -1;
   }

   /* Remove ssh process for writting data. */
   if (data_pid != -1)
   {
      int status;

      /* Close pipe for read/write data connection. */
      if (data_fd != -1)
      {
         if (close(data_fd) == -1)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__,
                      "Failed to close() write pipe to ssh process : %s",
                      strerror(errno));
         }
         data_fd = -1;
      }

      if (waitpid(data_pid, &status, 0) != data_pid)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__,
                   "Failed to catch zombie of data process : %s",
                   strerror(errno));
      }
      data_pid = -1;
   }
#endif /* _WITH_SCP1_SUPPORT */

   return;
}


#ifdef _WITH_SCP1_SUPPORT
/*+++++++++++++++++++++++++++++++ ssh_cmd() +++++++++++++++++++++++++++++*/
static int
ssh_cmd(char  *host,
        char  *user,
        char  *passwd,
        char  *cmd,
        int   *fd,
        pid_t *child_pid)
{
   int  fdm,
        status;
   char pts_name[11];

   msg_str[0] = '\0';
   if ((fdm = ptym_open(pts_name)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptym_open() error");
      status = INCORRECT;
   }
   else
   {
      int pipe_fd[2];

      /* Prepare pipes for parent/child synchronization. */
      if (pipe(pipe_fd) == -1)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "pipe() error : %s", strerror(errno));
         status = INCORRECT;
      }
      else
      {
         if ((*child_pid = fork()) == 0)  /* Child process */
         {
            char *args[10],
                 dummy;
            int  argcount = 0,
                 fds;

            setsid();
            if ((fds = ptys_open(fdm, pts_name)) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptys_open() error");
               return(INCORRECT);
            }
            (void)close(fdm);

            /* Synchronize with parent. */
            (void)close(pipe_fd[1]);
            if (read(pipe_fd[0], &dummy, 1) != 1)
            {
               (void)close(pipe_fd[0]);
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "read() error : %s", strerror(errno));
               return(INCORRECT);
            }
            (void)close(pipe_fd[0]);

            dup2(fds, STDIN_FILENO);
            dup2(fds, STDOUT_FILENO);
            dup2(fds, STDERR_FILENO);
            if (fds > 2)
            {
               close(fds);
            }

            args[argcount] = SSH_COMMAND;
            argcount++;
            args[argcount] = "-x";
            argcount++;
            args[argcount] = "-oFallBackToRsh no";
            argcount++;
            if (user != NULL)
            {
               args[argcount] = "-l";
               argcount++;
               args[argcount] = user;
               argcount++;
            }
            args[argcount] = host;
            argcount++;
            if (cmd != NULL)
            {
               args[argcount] = cmd;
               argcount++;
            }
            args[argcount] = NULL;
            argcount++;

            (void)execvp(SSH_COMMAND, args);
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "execvp() error : %s", strerror(errno));
            _exit(INCORRECT);
         }
         else if (*child_pid > 0) /* Parent process. */
              {
                 tty_raw(fdm);
                 *fd = fdm;

                 /* Synchronize with parent. */
                 (void)close(pipe_fd[0]);
                 if (write(pipe_fd[1], "", 1) != 1)
                 {
                    (void)close(pipe_fd[1]);
                    trans_log(ERROR_SIGN, __FILE__, __LINE__,
                              "write() error : %s", strerror(errno));
                    return(INCORRECT);
                 }
                 (void)close(pipe_fd[1]);

                 if ((status = get_reply(fdm, NO)) > 0)
                 {
                    if (status == 1)
                    {
                       if (msg_str[0] == '\0')
                       {
                          /* No password is required. */
                          status = SUCCESS;
                       }
                       else
                       {
                          trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                    "Error while logging in [%d].",
                                    (int)msg_str[0]);
                          status = INCORRECT;
                       }
                    }
                    else
                    {
                       char *ptr = msg_str;

                       if (posi(ptr, "assword:") != NULL)
                       {
                          size_t length = strlen(passwd);
                          char   *passwd_wn;

                          if ((passwd_wn = malloc(length + 1)) == NULL)
                          {
                             trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                       "malloc() error : %s",
                                       strerror(errno));
                             status = INCORRECT;
                          }
                          else
                          {
                             (void)memcpy(passwd_wn, passwd, length);
                             passwd_wn[length] = '\n';

                             /* Enter passwd. */
                             if ((status = write(fdm, passwd_wn, (length + 1))) != (length + 1))
                             {
                                trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                          "write() error [%d] : %s",
                                          status, strerror(errno));
                             }
                             else
                             {
                                /* Check if correct password was entered. */
                                msg_str[0] = '\0';
                                if ((status = get_passwd_reply(fdm)) > 0)
                                {
                                   if ((status == 1) && (msg_str[0] == '\n'))
                                   {
                                      status = SUCCESS;
                                   }
                                   else
                                   {
                                      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                                "Failed to enter passwd.");
                                      status = INCORRECT;
                                   }
                                }
                             }
                             (void)free(passwd_wn);
                          }
                       }
                       else /* It's not asking for passwd, it must be an error. */
                       {
                          status = INCORRECT;
                       }
                    }
                 }
              }
              else /* Failed to fork(). */
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__,
                           "fork() error : %s", strerror(errno));
                 status = INCORRECT;
              }
      }
   }

   return(status);
}


/*-------------------------- get_passwd_reply() -------------------------*/
static int
get_passwd_reply(int fd)
{
   int    status;
   fd_set rset;

   msg_str[0] = '\0';
   FD_ZERO(&rset);
   FD_SET(fd, &rset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   status = select(fd + 1, &rset, NULL, NULL, &timeout);

   if (status == 0)
   {
      timeout_flag = ON;
      status = INCORRECT;
   }
   else if (FD_ISSET(fd, &rset))
        {
           if ((status = read(fd, msg_str, MAX_RET_MSG_LENGTH)) < 0)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "read() error : %s", strerror(errno));
           }
           else if (status == 1)
                {
                   if ((status = read(fd, msg_str, MAX_RET_MSG_LENGTH)) < 0)
                   {
                      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                "read() error : %s", strerror(errno));
                   }
                   else if ((status == 1) || (status == 0))
                        {
                           status = 1;
                           msg_str[0] = '\n';
                        }
                        else
                        {
                           int  length = 0;
                           char *ptr = msg_str;

                           do
                           {
                              if ((*ptr == '\r') || (*ptr == '\n'))
                              {
                                 *ptr = '\0';
                                 break;
                              }
                              ptr++; length++;
                           } while (length < status);

                           if (posi(msg_str, "Permission denied") == NULL)
                           {
                              status = 1;
                              msg_str[0] = '\n';
                           }
                        }
                }
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__,
                     "select() error : %s", strerror(errno));
        }

   return(status);
}


/*----------------------------- get_reply() -----------------------------*/
static int
get_reply(int fd, int check_reply)
{
   int    status;
   fd_set rset;

   msg_str[0] = '\0';
   FD_ZERO(&rset);
   FD_SET(fd, &rset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   status = select(fd + 1, &rset, NULL, NULL, &timeout);

   if (status == 0)
   {
      timeout_flag = ON;
      status = INCORRECT;
   }
   else if (FD_ISSET(fd, &rset))
        {
           if ((status = read(fd, msg_str, MAX_RET_MSG_LENGTH)) < 0)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "read() error : %s", strerror(errno));
           }
           else
           {
              if (check_reply == YES)
              {
                 if (msg_str[status - 1] == '\n')
                 {
                    msg_str[status - 1] = '\0';
                 }
                 status = SUCCESS;
              }
           }
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__,
                     "select() error : %s", strerror(errno));
        }

   return(status);
}


/*----------------------------- ptym_open() -----------------------------*/
static int
ptym_open(char *pts_name)
{
   int  fd;
#if defined(SYSV)
   char *tempstr;

   strcpy(pts_name, "/dev/ptmx");
   if ((fd = open(pts_name, O_RDWR)) == -1)
   {
      return(-1);
   }

   if (grantpt(fd) < 0)
   {
      close(fd);
      return(-1);
   }

   if (unlockpt(fd) < 0)
   {
      close(fd);
      return(-1);
   }

   if ((tempstr = ptsname(fd)) == NULL)
   {
      close(fd);
      return(-1);
   }

   (void)strcpy(pts_name, tempstr);
   return(fd);
#else
   char *pos1, *pos2;

   (void)strcpy(pts_name, "/dev/ptyXY");
   for (pos1 = "pqrstuvwxyzPQRST"; *pos1 != '\0'; pos1++)
   {
      pts_name[8] = *pos1;
      for (pos2 = "0123456789abcdef"; *pos2 != '\0'; pos2++)
      {
         pts_name[9] = *pos2;
         if ((fd = open(pts_name, O_RDWR)) == -1)
         {
            if (errno == ENOENT)
            {
               return (-1);
            }
            else
            {
               continue;
            }
         }
         pts_name[5] = 't';
         return(fd);
      }
   }
   return(-1);
#endif
}


/*----------------------------- ptys_open() -----------------------------*/
static int
ptys_open(int fdm, char *pts_name)
{
   int fds;

#if !defined(SYSV)
#ifdef _SCHNULLI_
   (void)chmod(pts_name, S_IRUSR | S_IWUSR);
   (void)chown(pts_name, getuid(), -1);
#endif
#endif

   if ((fds = open(pts_name, O_RDWR)) < 0)
   {
      close(fdm);
      return(-1);
   }

#if defined(SYSV)
   if (ioctl(fds, I_PUSH, "ptem") < 0)
   {
      close(fdm);
      close(fds);
      return(-1);
   }

   if (ioctl(fds, I_PUSH, "ldterm") < 0)
   {
      close(fdm);
      close(fds);
      return(-1);
   }

   if (ioctl(fds, I_PUSH, "ttcompat") < 0)
   {
      close(fdm);
      close(fds);
      return(-1);
   }
#endif

#if !defined(SYSV) && defined(TIOCSCTTY) && !defined(CIBAUD)
   if (ioctl(fds, TIOCSCTTY, (char *) 0) < 0)
   {
      close(fdm);
      return(-1);
   }
#endif

   return(fds);
}


static int
tty_raw(int fd)
{
   struct termios buf;

   if (tcgetattr(fd, &buf) < 0)
      return (-1);

   buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
   buf.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
   buf.c_cflag &= ~(CSIZE | PARENB);
   buf.c_cflag |= CS8;
   buf.c_oflag &= ~(OPOST);
   buf.c_cc[VMIN] = 1;
   buf.c_cc[VTIME] = 0;

   if (tcsetattr(fd, TCSANOW, &buf) < 0)
      return (-1);
   return (0);
}
#endif /* _WITH_SCP1_SUPPORT */

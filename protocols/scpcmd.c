/*
 *  scpcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   scpcmd - commands to send files via the SCP protocol (SSH1)
 **
 ** SYNOPSIS
 **   int  scp_connect(char *hostname, int port, char *user,
 **                    char *passwd, char *dir)
 **   int  scp_open_file(char *filename, off_t size, mode_t mode)
 **   int  scp_close_file(void)
 **   int  scp_write(char *block, int size)
 **   void scp_quit(void)
 **
 ** DESCRIPTION
 **   scpcmd provides a set of commands to communicate with a SSH
 **   (Secure Shell) server via pipes. The following functions are
 **   required to send a file:
 **      scp_connect()    - build a Tconnection to the SSH server
 **      scp_open_file()  - open a file
 **      scp_close_file() - close a file
 **      scp_write()      - write data to the pipe
 **      scp_quit()       - disconnect from the SSH server
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
 **   22.04.2001 H.Kiehl        Created
 **   30.09.2001 H.Kiehl        Added functions scp1_cmd_connect(),
 **                             scp1_chmod() and scp1_move().
 **   10.03.2003 F.Olivie (Alf) Support for SCP2.
 **                             Added timeouts and identity file handling.
 **   10.12.2003 H.Kiehl        Remove everything with ctrl connection,
 **                             since it does not work on all systems.
 **                             The SCP protocol was not designed for this.
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
#include <signal.h>           /* signal(), kill()                        */
#include <termios.h>
#include <unistd.h>           /* select(), exit(), write(), read(),      */
                              /* close()                                 */
#include <fcntl.h>            /* O_NONBLOCK                              */
#include <errno.h>
#include "scpdefs.h"
#include "fddefs.h"           /* struct job                              */


/* External global variables */
extern int            timeout_flag;
extern char           msg_str[];
extern long           transfer_timeout;
extern char           tr_hostname[];
extern struct job     db;

#ifdef _WITH_SCP_SUPPORT
/* Local global variables */
static int            data_fd = -1;
static pid_t          data_pid = -1;
static struct timeval timeout;

/* Local function prototypes. */
static int            get_reply(int, int),
                      get_passwd_reply(int, char *),
                      ptym_open(char *),
                      ptys_open(char *),
                      ssh_cmd(char *, char *, char *, char *, int *, pid_t *),
                      tty_raw(int);
static size_t         pipe_write(int, char *, size_t);
static void           sig_ignore_handler(int);
#endif /* _WITH_SCP_SUPPORT */

/* #define SCP_DEBUG */


/*########################### scp_connect() #############################*/
int
scp_connect(char *hostname, int port, char *user, char *passwd, char *dir)
{
#ifdef _WITH_SCP_SUPPORT
   char cmd[MAX_PATH_LENGTH];

   (void)sprintf(cmd, "scp -t %s", (dir[0] == '\0') ? "." : dir);

   return(ssh_cmd(hostname, user, passwd, cmd, &data_fd, &data_pid));
#else
   return(0);
#endif /* _WITH_SCP_SUPPORT */
}


/*########################## scp_open_file() ############################*/
int
scp_open_file(char *filename, off_t size, mode_t mode)
{
#ifdef _WITH_SCP_SUPPORT
   int    status;
   size_t length;
   char   cmd[MAX_PATH_LENGTH];

   length = sprintf(cmd, "C%04o %lu %s\n",
                    (mode & (S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO)),
                    size, filename);
   if ((status = pipe_write(data_fd, cmd, length)) != length)
   {
      if (errno != 0)
      {
         cmd[length - 1] = '\0';
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to write() <%s> to pipe [%d] : %s",
                   cmd, status, strerror(errno));
      }
      status = INCORRECT;
   }
   else
   {
      status = get_reply(data_fd, YES);
   }
   return(status);
#else
   return(0);
#endif /* _WITH_SCP_SUPPORT */
}


/*########################### scp_close_file() ##########################*/
int
scp_close_file(void)
{
#ifdef _WITH_SCP_SUPPORT
   int status;

   if ((status = pipe_write(data_fd, "\0", 1)) != 1)
   {
      if (errno != 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to write() [close file] to pipe [%d] : %s",
                   status, strerror(errno));
      }
      status = INCORRECT;
   }
   else
   {
      status = get_reply(data_fd, YES);
   }
   return(status);
#else
   return(0);
#endif /* _WITH_SCP_SUPPORT */
}


/*############################## scp_write() ############################*/
int
scp_write(char *block, int size)
{
#ifdef _WITH_SCP_SUPPORT
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
           /* In some cases, the write system call hangs. */
           signal(SIGALRM, sig_ignore_handler); /* Ignore the default which */
                                                /* is to abort.             */
           siginterrupt(SIGALRM, 1); /* Allow SIGALRM to interrupt write. */

           alarm(transfer_timeout); /* Set up an alarm to interrupt write. */
           if ((status = write(data_fd, block, size)) != size)
           {
              alarm(0);
              signal(SIGALRM, SIG_DFL); /* restore the default which is abort. */
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "write() error (%d) : %s", status, strerror(errno));
              return(errno);
           }
           alarm(0);
           signal(SIGALRM, SIG_DFL); /* Restore the default which is abort. */
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
#endif /* _WITH_SCP_SUPPORT */
   
   return(SUCCESS);
}


/*############################## scp_quit() #############################*/
void
scp_quit(void)
{
#ifdef _WITH_SCP_SUPPORT
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
            trans_log(WARN_SIGN, __FILE__, __LINE__,
                      "Failed to close() write pipe to ssh process : %s",
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
            trans_log(WARN_SIGN, __FILE__, __LINE__,
                      "Failed to catch zombie of data ssh process : %s",
                      strerror(errno));
         }
         if (kill(data_pid, SIGKILL) == -1)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to kill() data ssh process %d : %s",
                      data_pid, strerror(errno));
         }
         else
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__,
                      "Killing hanging data ssh process.");
         }
      }
      data_pid = -1;
   }
#endif /* _WITH_SCP_SUPPORT */

   return;
}


#ifdef _WITH_SCP_SUPPORT
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
        len, /* To get clearer code */
        status;
   char pts_name[11],
        *identityFilePath = NULL, /* To store the identity part. */
        *password = NULL, /* To store the password part. */
        *passwdBeg = NULL, *passwdEnd, /* Temporary ptrs */
        *idBeg = NULL, *idEnd, /* Temporary ptrs */
        *ptr; /* Temporary ptr */

   /* We want to be generic and allow a user to place the */
   /* tags in any order                                   */

   if (passwd != NULL)
   {
      /* Do we have an identity tag "<i>" ? */
      if ((ptr = strstr(passwd, "<i>")))
      {
         idBeg = ptr + 3;
      }

      /* Do we have a passwd tag "<p>" ? */
      if ((ptr = strstr(passwd, "<p>")))
      {
         passwdBeg = ptr + 3;
      }

      /* Locate identity end tag. */
      if (passwdBeg < idBeg)  /* Id is after Pwd. */
      {
         idEnd = idBeg + (strlen(idBeg) - 1);
      }
      else  /* Id ends with Pwd. */
      {
         idEnd = passwdBeg - 4;
      }

      /* Locate password end tag. */
      if (idBeg < passwdBeg)  /* Pwd is after Id. */
      {
         passwdEnd = passwdBeg + (strlen(passwdBeg) - 1);
      }
      else  /* Pwd ends with Id. */
      {
         passwdEnd = idBeg - 4;
      }

      /* Last case, we have no tag. We should have a password alone. */
      if (!passwdBeg && !idBeg)
      {
         passwdBeg = passwd;
         passwdEnd = passwdBeg + (strlen(passwdBeg) - 1);
      }

      /* Copy what we found. If we don't have a password, nor an           */
      /* identity file, carry on anyways. In this case, ssh will use       */
      /* The ~/.ssh/id_dsa (or rsa) file. It should not have a passphrase! */

      if (passwdBeg)
      {
         len = passwdEnd - passwdBeg + 1;
         if ((password = malloc(len + 2)) == NULL)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "malloc() error : %s", strerror(errno));
            return(INCORRECT);
         }
         memcpy(password, passwdBeg, len);
         password[len] = '\n';
         password[len + 1] = 0;
      }

      if (idBeg)
      {
         len = idEnd - idBeg + 1;
         if ((identityFilePath = malloc(len + 1)) == NULL)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "malloc() error : %s", strerror(errno));
            return(INCORRECT);
         }
         memcpy(identityFilePath, idBeg, len);
         identityFilePath[len] = 0;
      }
   }

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
            char *args[11],
                 dummy;
            int  argcount = 0,
                 fds;

            setsid();
            if ((fds = ptys_open(pts_name)) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptys_open() error");
               (void)close(fdm);
               return(INCORRECT);
            }
            (void)close(fdm);
            (void)tty_raw(fds);

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
            if (identityFilePath != NULL)
            {
               args[argcount] = "-i";
               argcount++;
               args[argcount] = identityFilePath;
               argcount++;
            }
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
                 (void)tty_raw(fdm);
                 *fd = fdm;

                 /* Synchronize with child. */
                 (void)close(pipe_fd[0]);
                 if (pipe_write(pipe_fd[1], "", 1) != 1)
                 {
                    if (errno != 0)
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                 "write() error : %s", strerror(errno));
                    }
                    (void)close(pipe_fd[1]);
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

                       if ((posi(ptr, "assword:") != NULL) ||
                           (!strncmp(msg_str, "Enter passphrase", 16)))
                       {
                          if (password)
                          {
                             size_t length = strlen(password),
                                    prompt_length = strlen(msg_str);
                             char   *prompt;

                             if ((prompt = malloc(prompt_length + 1)) == NULL)
                             {
                                trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                          "malloc() error : %s",
                                          strerror(errno));
                                status = INCORRECT;
                             }
                             else
                             {
                                (void)memcpy(prompt, msg_str, prompt_length);
                                prompt[prompt_length] = '\0';
                                if ((status = pipe_write(fdm, password, length)) != length)
                                {
                                   if (errno != 0)
                                   {
                                      msg_str[0] = '\0';
                                      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                                "write() error [%d] : %s",
                                                status, strerror(errno));
                                   }
                                }
                                else
                                {
                                   /* Check if correct password was entered. */
                                   msg_str[0] = '\0';
                                   if ((status = get_passwd_reply(fdm, prompt)) > 0)
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
                                (void)free(prompt);
                             }
                          }
                          else /* if (!password) */
                          {
                             /* It's asking for a password or passphrase and */
                             /* we don't have one. Report error.             */
                             trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                       "ssh is asking for password (or passphrase) and none is provided. Bailing out!");
                             status = INCORRECT;
                          }
                       }
                       /*
	                * It's not asking for a password. Three cases :
	                * 1) We're using a private key (Identity file)
	                * 2) It's asking for something else (prior host key
	                *    exchange or host key mismatch).
	                * 3) It's an unknown failure. Go on, we'll catch by
	                *    later (with a timeout, and no good message. Bad).
	                */
	               else if (posi(ptr, "(yes/no)") != NULL)
	                    {
                               if ((status = pipe_write(fdm, "no\n", 3)) != 3)
                               {
                                  if (errno != 0)
                                  {
                                     msg_str[0] = '\0';
                                     trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                               "write() error [%d] : %s",
                                               status, strerror(errno));
                                  }
                               }
                               else
                               {
                                  msg_str[0] = '\0';
                                  if ((status = get_reply(fdm, YES)) != SUCCESS)
                                  {
                                     trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                               "Failed to send no to verify ssh connection. [%d]",
                                               status);
                                  }
                               }
                               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "Please connect to this host with the command line SSH utility and answer this question appropriately.");
                               status = INCORRECT;
	                    }
                            else
                            {
                               /* Replace '\n's by spaces for logging. */
                               while (*ptr++)
                               {
                                  if (*ptr == '\n')
                                  {
                                     *ptr = ' ';
                                  }
                               }
                               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "Protocol error. ssh is complaining.");
                               status = INCORRECT;
                            }
                    }
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__,
                              "get_reply() error");
                    status = INCORRECT;
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

   if (identityFilePath)
   {
      (void)free(identityFilePath);
   }
   if (password)
   {
      (void)free(password);
   }
   return(status);
}


/*+++++++++++++++++++++++++ sig_ignore_handler() ++++++++++++++++++++++++*/
static void
sig_ignore_handler(int signo)
{
   /* Do nothing. This just allows us to receive a SIGALRM, thus */
   /* interrupting a system call without being forced to abort.  */
}


/*-------------------------- get_passwd_reply() -------------------------*/
static int
get_passwd_reply(int fd, char *previous_prompt)
{
   int    status;
   fd_set rset;

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
           msg_str[0] = '\0';
           trans_log(ERROR_SIGN, __FILE__, __LINE__,
                     "select() error : %s", strerror(errno));
        }

   return(status);
}


/*----------------------------- pipe_write() ----------------------------*/
static size_t
pipe_write(int fd, char *buf, size_t count)
{
   if (fd != -1)
   {
      int    status;
      fd_set wset;

      /* Initialise descriptor set */
      FD_ZERO(&wset);
      FD_SET(fd, &wset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      /* Wait for message x seconds and then continue. */
      status = select(fd + 1, NULL, &wset, NULL, &timeout);
      if (status == 0)
      {
         /* Timeout has arrived. */
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "There is no reply from pipe, failed to send command %s.",
                   buf);
      }
      else if (FD_ISSET(fd, &wset))
           {
#ifdef _SCP_DEBUG
              char tmp_char;

              tmp_char = buf[count - 1];
              buf[count - 1] = '\0';
              trans_log(INFO_SIGN, NULL, 0, "== Pipe Writting (%d) ==",
                        count);
              buf[count - 1] = tmp_char;
#endif /* SCP_DEBUG */
              return(write(fd, buf, count));
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "select() error : %s", strerror(errno));
           }
   }
   errno = 0;
   return(INCORRECT);
}


/*----------------------------- get_reply() -----------------------------*/
static int
get_reply(int fd, int check_reply)
{
   int    status;
   fd_set rset;

   FD_ZERO(&rset);
   FD_SET(fd, &rset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   status = select(fd + 1, &rset, NULL, NULL, &timeout);

   if (status == 0)
   {
      msg_str[0] = '\0';
      timeout_flag = ON;
      status = INCORRECT;
   }
   else if (FD_ISSET(fd, &rset))
        {
           if ((status = read(fd, msg_str, MAX_RET_MSG_LENGTH)) < 0)
           {
              msg_str[0] = '\0';
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "read() error : %s", strerror(errno));
              status = INCORRECT;
           }
           else
           {
              msg_str[status] = '\0';
              if (status == 0)
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__,
                           "Other side closed the pipe.");
                 status = INCORRECT;
              }
              else
              {
                 char *ptr = msg_str;

                 while (*ptr)
                 {
                    if (*ptr == '\n')
                    {
                       *ptr = ' ';
                    }
                    ptr++;
                 }
#ifdef SCP_DEBUG
                 if (status == 1)
                 {
                    trans_log(INFO_SIGN, NULL, 0, "== Reading ONE Byte %d ==",
                              (int)msg_str[0]);
                 }
                 else
                 {
                    trans_log(INFO_SIGN, NULL, 0, "== Reading (%d) ==",
                              status);
                 }
#endif /* SCP_DEBUG */
                 if (check_reply == YES)
                 {
                    if (msg_str[status - 1] == '\n')
                    {
                       msg_str[status - 1] = '\0';
                    }

                    if (msg_str[0] == 1) /* This is a ^A */
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                 "scp error : %s", msg_str + 1);
                       status = INCORRECT; /* This could be an exit !! */
                    }
                    else
                    {
                       status = SUCCESS;
                    }
                 }
              }
           }
        }
        else
        {
           msg_str[0] = '\0';
           trans_log(ERROR_SIGN, __FILE__, __LINE__,
                     "select() error : %s", strerror(errno));
           status = INCORRECT;
        }

   return(status);
}


/*----------------------------- ptym_open() -----------------------------*/
/*                              -----------                              */
/* This code was taken from Advanced Programming in the Unix             */
/* environment by W.Richard Stevens.                                     */
/*-----------------------------------------------------------------------*/
static int
ptym_open(char *pts_name)
{
   int  fd;
#ifdef IRIX
   char *tempstr;

   if ((tempstr = _getpty(&fd, O_RDWR, 0600, 0)) == NULL)
   {
      return(-1);
   }
   (void)strcpy(pts_name, tempstr);
   return(fd);
#else
#if defined(SYSV)
   char *tempstr;

   strcpy(pts_name, "/dev/ptmx");
   if ((fd = open(pts_name, O_RDWR)) == -1)
   {
      return(-1);
   }

   if (grantpt(fd) < 0)
   {
      (void)close(fd);
      return(-1);
   }

   if (unlockpt(fd) < 0)
   {
      (void)close(fd);
      return(-1);
   }

   if ((tempstr = ptsname(fd)) == NULL)
   {
      (void)close(fd);
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
#endif
}


/*----------------------------- ptys_open() -----------------------------*/
/*                              -----------                              */
/* This code was taken from Advanced Programming in the Unix             */
/* environment by W.Richard Stevens.                                     */
/*-----------------------------------------------------------------------*/
static int
ptys_open(char *pts_name)
{
   int fds;

#if !defined(SYSV) && !defined(IRIX)
#ifdef _SCHNULLI_
   (void)chown(pts_name, getuid(), -1);
   (void)chmod(pts_name, S_IRUSR | S_IWUSR);
#endif
#endif

   if ((fds = open(pts_name, O_RDWR)) < 0)
   {
      return(-1);
   }

#if defined(SYSV)
   if (ioctl(fds, I_PUSH, "ptem") < 0)
   {
      (void)close(fds);
      return(-1);
   }

   if (ioctl(fds, I_PUSH, "ldterm") < 0)
   {
      (void)close(fds);
      return(-1);
   }

   if (ioctl(fds, I_PUSH, "ttcompat") < 0)
   {
      (void)close(fds);
      return(-1);
   }
#endif

#if !defined(SYSV) && defined(TIOCSCTTY) && !defined(CIBAUD) && !defined(IRIX)
   if (ioctl(fds, TIOCSCTTY, (char *) 0) < 0)
   {
      (void)close(fds);
      return(-1);
   }
#endif

   return(fds);
}


/*------------------------------ tty_raw() ------------------------------*/
/*                              -----------                              */
/* This code was taken from Advanced Programming in the Unix             */
/* environment by W.Richard Stevens. The modification where taken from   */
/* gFTP software package by Brian Masney.                                */
/*-----------------------------------------------------------------------*/
static int
tty_raw(int fd)
{
   struct termios buf;

   if (tcgetattr(fd, &buf) < 0)
   {
      return(-1);
   }
   buf.c_iflag |= IGNPAR;
   buf.c_iflag &= ~(ICRNL | INPCK| ISTRIP | IXON | IGNCR | IXANY | IXOFF | INLCR);
   buf.c_lflag &= ~(ECHO | ICANON | ISIG | ECHOE | ECHOK | ECHONL);
#ifdef IEXTEN
   buf.c_lflag &= ~(IEXTEN);
#endif
   buf.c_cflag &= ~(CSIZE | PARENB);
   buf.c_cflag |= CS8;
   buf.c_oflag &= ~(OPOST);  /* Output processing off. */
   buf.c_cc[VMIN] = 1;       /* Case B: 1 byte at a time, no timer. */
   buf.c_cc[VTIME] = 0;

   if (tcsetattr(fd, TCSANOW, &buf) < 0)
   {
      return(-1);
   }
   return(0);
}
#endif /* _WITH_SCP_SUPPORT */

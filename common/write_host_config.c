/*
 *  write_host_config.c - Part of AFD, an automatic file distribution
 *                        program.
 *  Copyright (c) 1997 - 2004 Deutscher Wetterdienst (DWD),
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
 **   write_host_config - writes the HOST_CONFIG file
 **
 ** SYNOPSIS
 **   time_t write_host_config(int              no_of_hosts,
 **                            char             *host_config_file,
 **                            struct host_list *hl)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   The modification time of the HOST_CONFIG file. Will exit() with
 **   INCORRECT when any system call will fail.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.12.1997 H.Kiehl Created
 **   02.03.1998 H.Kiehl Put host switching information into HOST_CONFIG.
 **   28.07.1998 H.Kiehl Change mode to 660 for HOST_CONFIG so when
 **                      two users use the same binaries each can edit
 **                      their own HOST_CONFIG file.
 **   03.08.2001 H.Kiehl Remember if we stopped the queue or transfer
 **                      and some protocol specific information.
 **   03.11.2004 H.Kiehl Write the new HOST_CONFIG to some temporary file
 **                      and when done overwrite the original file.
 **                      Otherwise when the disk is full HOST_CONFIG
 **                      can just be an empty file!
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>            /* exit()                                 */
#include <unistd.h>            /* write(), close()                       */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

/* External global definitions */
extern int    sys_log_fd;

#define HOST_CONFIG_TEXT "#\n\
#                Host configuration file for the AFD\n\
#                ===================================\n\
#\n\
# There are 12 parameters that can be configured for each remote\n\
# host. They are:\n\
#\n\
# AH:HN1:HN2:HT:PXY:AT:ME:RI:TB:SR:FSO:TT:NB:HS:SF\n\
# |   |   |   |  |  |  |  |  |  |   |  |  |  |  |\n\
# |   |   |   |  |  |  |  |  |  |   |  |  |  |  +-> Special flag\n\
# |   |   |   |  |  |  |  |  |  |   |  |  |  +----> Host status\n\
# |   |   |   |  |  |  |  |  |  |   |  |  +-------> Number of no bursts\n\
# |   |   |   |  |  |  |  |  |  |   |  +----------> Transfer timeout\n\
# |   |   |   |  |  |  |  |  |  |   +-------------> File size offset\n\
# |   |   |   |  |  |  |  |  |  +-----------------> Successful retries\n\
# |   |   |   |  |  |  |  |  +--------------------> Transfer block size\n\
# |   |   |   |  |  |  |  +-----------------------> Retry interval\n\
# |   |   |   |  |  |  +--------------------------> Max. errors\n\
# |   |   |   |  |  +-----------------------------> Allowed transfers\n\
# |   |   |   |  +--------------------------------> Proxy name\n\
# |   |   |   +-----------------------------------> Host toggle\n\
# |   |   +---------------------------------------> Real hostname 2\n\
# |   +-------------------------------------------> Real hostname 1\n\
# +-----------------------------------------------> Alias hostname\n\
#\n\
# Or if you prefer another view of the above:\n\
#\n\
#   <Alias hostname>:<Real hostname 1>:<Real hostname 2>:<Host toggle>:\n\
#   <Proxy name>:<Allowed transfers>:<Max. errors>:<Retry interval>:\n\
#   <Transfer block size>:<Successful retries>:<File size offset>:\n\
#   <Transfer timeout>:<no bursts>:<host status>:<special flag>\n\
#\n\
# The meaning of each is outlined in more detail below:\n\
#\n\
#   Alias hostname      - This is the host name that is being displayed\n\
#                         in the afd_ctrl window and is used in the log\n\
#                         files.\n\
#                         DEFAULT: None (Empty)\n\
#   Real hostname 1     - The real host name or IP number of the primary\n\
#                         host.\n\
#   Real hostname 2     - The real host name or IP number of the secondary\n\
#                         host.\n\
#   Host toggle         - Host switching information. This string holds the\n\
#                         toggling character to be displayed for the\n\
#                         primary and secondary host. The two characters\n\
#                         must be put in either curly brackets {} for\n\
#                         automatic host switching or square brackets []\n\
#                         host switching by the user.\n\
#   Proxy name          - If the remote host can only be reached via a\n\
#                         proxy, specify the name of the proxy here.\n\
#                         DEFAULT: None (Empty)\n\
#   Allowed transfers   - The maximum number of parallel transfers for\n\
#                         this host.\n\
#                         DEFAULT: 2\n\
#   Max. errors         - If max. errors is reached the destination\n\
#                         identifier turns 'red'. If error retries\n\
#                         reaches twice max. errors the queue of this\n\
#                         host will be paused.\n\
#   Retry interval      - If an error occurs, this is the delay (in\n\
#                         seconds) before another transfer is initiated.\n\
#   Transfer block size - The size of the blocks being used to send\n\
#                         files to the remote host (in Bytes).\n\
#                         DEFAULT: 1024\n\
#   Successful retries  - This is only used when there is a secondary\n\
#                         host and automatic switch over is active.\n\
#                         It is the number of successful transfers to\n\
#                         the secondary host, before it tries to switch\n\
#                         back to the main host to see if it is alive\n\
#                         again.\n\
#   File size offset    - When transmitting large files and the transfer\n\
#                         gets interrupted, the AFD can append a file\n\
#                         on the remote site. For this it needs to know\n\
#                         the file size on the remote site. And to get\n\
#                         the size it does a dir 'filename' at the remote\n\
#                         site. Due to different replies of the FTP\n\
#                         servers, the position of the file size is\n\
#                         needed. You can easily determine this value\n\
#                         simply doing an FTP to the remote site and\n\
#                         a dir and count the spaces to the file size.\n\
#                         For example:\n\
#\n\
#             -rw-r--r--   1 afd      mts-soft   14971 Jan  3 17:16\n\
#                       ^^^ ^   ^^^^^^        ^^^\n\
#                        |  |     |            |\n\
#                        |  |     |            |\n\
#                        1  2     3            4\n\
#\n\
#                         You may also put a -2 here, then AFD will try\n\
#                         to use the FTP SIZE command to get the size of\n\
#                         the remote file.\n\
#                         DEFAULT: -1 (Disabled)\n\
#\n\
#   Transfer timeout    - The time how long the AFD should wait for a\n\
#                         reply from the remote site.\n\
#                         DEFAULT: 120\n\
#   Number of no bursts - This option applies only to FTP transfers.\n\
#                         A burst is when a new job is appended to a\n\
#                         transferring job. It can happen that jobs\n\
#                         get constantly appended while other jobs\n\
#                         with a higher priority have to wait. Therefor\n\
#                         it is possible to state the number of\n\
#                         connections that may NOT burst.\n\
#                         DEFAULT: 0\n\
#   Host status         - This indicates the status of the host, currently\n\
#                         only bits number 1, 2 and 6 can be set. The\n\
#                         meaning is as follows (the values in brackets\n\
#                         are the integer values that may be set):\n\
#                         1 (1) - If set transfer is stopped for this host.\n\
#                         2 (2) - If set queue is stopped for this host.\n\
#                         3 (4) - If set host is NOT in DIR_CONFIG.\n\
#                         6 (32)- If set this host is disabled.\n\
#                         DEFAULT: 0\n\
#   Special flag        - To set some protocol specific features for\n\
#                         this host. The following bits can be set (again\n\
#                         the values in bracket are the integer values\n\
#                         that can be set):\n\
#                         11 (1024)- FTP passive mode\n\
#                         12 (2048)- Set FTP idle time to transfer timeout\n\
#                         13 (4096)- Send STAT command to keep control\n\
#                                    connection alive.\n\
#                         DEFAULT: 0\n\
#\n\
# Example: idefix:192.168.1.24:192.168.1.25:[12]::5:10:300:4096:10:0:0:1:0:0\n\n"


/*++++++++++++++++++++++++ write_host_config() ++++++++++++++++++++++++++*/
time_t
write_host_config(int              no_of_hosts,
                  char             *host_config_file,
                  struct host_list *p_hl)
{
   int           i,
                 fd,
                 lock_fd;
   static time_t mod_time;
   size_t        length;
   char          line_buffer[MAX_HOSTNAME_LENGTH +  /* Alias hostname    */
                             (2 * MAX_REAL_HOSTNAME_LENGTH) +
                                                    /* Real hostname 1+2 */
                             MAX_TOGGLE_STR_LENGTH +/* Toggle string     */
                             MAX_PROXY_NAME_LENGTH +/* Proxy name        */
                             MAX_INT_LENGTH +       /* Allowed transfers */
                             MAX_INT_LENGTH +       /* Max. errors       */
                             MAX_INT_LENGTH +       /* Retry interval    */
                             MAX_INT_LENGTH +       /* Transfer blksize  */
                             MAX_INT_LENGTH +       /* Succ. retries     */
                             MAX_INT_LENGTH +       /* File size offset  */
                             MAX_INT_LENGTH +       /* Transfer timeout  */
                             MAX_INT_LENGTH +       /* No. of no bursts  */
                             MAX_INT_LENGTH +       /* Host status       */
                             MAX_INT_LENGTH +       /* Special flag      */
                             14 +                   /* Separator signs   */
                             2],                    /* \n and \0         */
                 *new_name,
                 *ptr;
   struct stat   stat_buf;

   if ((lock_fd = lock_file(host_config_file, ON)) == INCORRECT)
   {
      exit(INCORRECT);
   }
   length = strlen(host_config_file);
   if ((new_name = malloc(length + 2)) == NULL)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not malloc() %d bytes : %s (%s %d)\n",
                length + 2, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   (void)memcpy(new_name, host_config_file, length + 1);
   ptr = new_name + length;
   while ((*ptr != '/') && (ptr > new_name))
   {
      ptr--;
   }
   if (*ptr == '/')
   {
      char *copy_ptr;

      ptr++;
      *ptr = '.';
      copy_ptr = host_config_file + (ptr - new_name);
      ptr++;
      do
      {
         *ptr = *copy_ptr;
         ptr++; copy_ptr++;
      } while (*copy_ptr != '\0');
      *ptr = '\0';
   }

   if ((fd = open(new_name,
                  (O_RDWR | O_CREAT | O_TRUNC),
                  (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Could not open() %s : %s (%s %d)\n",
                new_name, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Write introduction comment */
   length = strlen(HOST_CONFIG_TEXT);
   if (write(fd, HOST_CONFIG_TEXT, length) != length)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "write() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Commit data line by line. */
   for (i = 0; i < no_of_hosts; i++)
   {
      length = sprintf(line_buffer,
                       "%s:%s:%s:%s:%s:%d:%d:%d:%d:%d:%d:%d:%d:%d:%u\n",
                       p_hl[i].host_alias,
                       p_hl[i].real_hostname[0],
                       p_hl[i].real_hostname[1],
                       p_hl[i].host_toggle_str,
                       p_hl[i].proxy_name,
                       p_hl[i].allowed_transfers,
                       p_hl[i].max_errors,
                       p_hl[i].retry_interval,
                       p_hl[i].transfer_blksize,
                       p_hl[i].successful_retries,
                       (int)p_hl[i].file_size_offset,
                       (int)p_hl[i].transfer_timeout,
                       (int)p_hl[i].number_of_no_bursts,
                       (int)p_hl[i].host_status,
                       p_hl[i].special_flag);

      if (write(fd, line_buffer, length) != length)
      {
         (void)rec(sys_log_fd, FATAL_SIGN, "write() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   if (unlink(host_config_file) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "Failed to unlink() %s : %s (%s %d)\n",
                host_config_file, strerror(errno), __FILE__, __LINE__);
   }
   if (rename(new_name, host_config_file) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to rename() %s to %s : %s (%s %d)\n",
                new_name, host_config_file, strerror(errno),
                __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (close(lock_fd) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "Failed to close() %s : %s (%s %d)\n",
                host_config_file, strerror(errno), __FILE__, __LINE__);
   }
   free(new_name);

   if (stat(host_config_file, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Failed to stat() %s : %s (%s %d)\n",
                host_config_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   mod_time = stat_buf.st_mtime;

   return(mod_time);
}

/*
 *  write_host_config.c - Part of AFD, an automatic file distribution
 *                        program.
 *  Copyright (c) 1997 - 1999 Deutscher Wetterdienst (DWD),
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
 **   time_t write_host_config(void)
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
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <unistd.h>            /* write(), close()                       */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

/* External global definitions */
extern int    sys_log_fd,
              no_of_hosts;
extern char   host_config_file[];
extern struct host_list *hl;

#define HOST_CONFIG_TEXT "#\n\
#                Host configuration file for the AFD\n\
#                ===================================\n\
#\n\
# There are 12 parameters that can be configured for each remote\n\
# host. They are:\n\
#\n\
#   AH:HN1:HN2:HT:PXY:AT:ME:RI:TB:SR:FSO:TT:NB\n\
#   |   |   |   |  |  |  |  |  |  |   |  |  |\n\
#   |   |   |   |  |  |  |  |  |  |   |  |  +----> Number of no bursts\n\
#   |   |   |   |  |  |  |  |  |  |   |  +-------> Transfer timeout\n\
#   |   |   |   |  |  |  |  |  |  |   +----------> File size offset\n\
#   |   |   |   |  |  |  |  |  |  +--------------> Successful retries\n\
#   |   |   |   |  |  |  |  |  +-----------------> Transfer block size\n\
#   |   |   |   |  |  |  |  +--------------------> Retry interval\n\
#   |   |   |   |  |  |  +-----------------------> Max. errors\n\
#   |   |   |   |  |  +--------------------------> Allowed transfers\n\
#   |   |   |   |  +-----------------------------> Host toggle\n\
#   |   |   |   +--------------------------------> Proxy name\n\
#   |   |   +------------------------------------> Real hostname 2\n\
#   |   +----------------------------------------> Real hostname 1\n\
#   +--------------------------------------------> Alias hostname\n\
#\n\
# Or if you prefer another view of the above:\n\
#\n\
#   <Alias hostname>:<Real hostname 1>:<Real hostname 2>:<Host toggle>:\n\
#   <Proxy name>:<Allowed transfers>:<Max. errors>:<Retry interval>:\n\
#   <Transfer block size>:<Successful retries>:<File size offset>:\n\
#   <Transfer timeout>:<no bursts>\n\
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
#\n\
# Example: idefix:192.168.1.24:192.168.1.25:[12]::5:10:300:4096:10:0:0:1\n\n"


/*++++++++++++++++++++++++ write_host_config() ++++++++++++++++++++++++++*/
time_t
write_host_config(void)
{
   int           i,
                 fd;
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
                             MAX_INT_LENGTH +       /* Allowed transfers */
                             12 +                   /* Separator signs   */
                             2];                    /* \n and \0         */
   struct flock  wlock = {F_WRLCK, SEEK_SET, 0, 1};
   struct stat   stat_buf;

   if ((fd = open(host_config_file,
                  (O_RDWR | O_CREAT | O_TRUNC),
                  (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Could not open() %s : %s (%s %d)\n",
                host_config_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Lock it! */
   if (fcntl(fd, F_SETLKW, &wlock) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to lock file %s : %s (%s %d)\n",
                host_config_file, strerror(errno), __FILE__, __LINE__);
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
      length = sprintf(line_buffer, "%s:%s:%s:%s:%s:%d:%d:%d:%d:%d:%d:%d:%d\n",
                       hl[i].host_alias,
                       hl[i].real_hostname[0],
                       hl[i].real_hostname[1],
                       hl[i].host_toggle_str,
                       hl[i].proxy_name,
                       hl[i].allowed_transfers,
                       hl[i].max_errors,
                       hl[i].retry_interval,
                       hl[i].transfer_blksize,
                       hl[i].successful_retries,
                       (int)hl[i].file_size_offset,
                       (int)hl[i].transfer_timeout,
                       (int)hl[i].number_of_no_bursts);

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

      /* If we fail to close() the file lets at least try to unlock it. */
      unlock_region(fd, 0);
   }

   if (stat(host_config_file, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Failed to stat() %s : %s (%s %d)\n",
                host_config_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   mod_time = stat_buf.st_mtime;

   return(mod_time);
}

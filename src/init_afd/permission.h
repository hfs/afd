/*
 *  permission.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __permission_h
#define __permission_h

#define NO_PERMISSION        -2
#define NO_LIMIT             -3

#define STARTUP_PERM         "startup"           /* Startup AFD          */
#define SHUTDOWN_PERM        "shutdown"          /* Shutdown AFD         */
#define INITIALIZE_PERM      "initialize"        /* Initialize AFD       */
#define RESEND_PERM          "resend"            /* Resend files         */
#define SEND_PERM            "send"              /* Send files to host   */
                                                 /* not in FSA.          */
#define VIEW_PASSWD_PERM     "view_passwd"       /* View password.       */
#define SET_PASSWD_PERM      "set_passwd"        /* Set password.        */
#define LIST_LIMIT           "list_limit"        /* List limit show_xlog */
#define SHOW_SLOG_PERM       "show_slog"         /* System Log           */
#define SHOW_RLOG_PERM       "show_rlog"         /* Receive Log          */
#define SHOW_TLOG_PERM       "show_tlog"         /* Transfer Log         */
#define SHOW_DLOG_PERM       "show_dlog"         /* Debug Log            */
#define SHOW_ILOG_PERM       "show_ilog"         /* Input Log            */
#define SHOW_OLOG_PERM       "show_olog"         /* Output Log           */
#define SHOW_ELOG_PERM       "show_elog"         /* Delete Log           */
#define EDIT_HC_PERM         "edit_hc"           /* Edit HOST_CONFIG     */
#define AFD_CTRL_PERM        "afd_ctrl"          /* afd_ctrl dialog      */
#define RAFD_CTRL_PERM       "rafd_ctrl"         /* remote afd_ctrl      */
#define SHOW_TRANS_PERM      "show_trans"
#define AMG_CTRL_PERM        "AMG_control"       /* Start/Stop AMG       */
#define FD_CTRL_PERM         "FD_control"        /* Start/Stop FD        */
#define CTRL_QUEUE_PERM      "control_queue"     /* Start/Stop queue     */
#define CTRL_TRANSFER_PERM   "control_transfer"  /* Start/Stop transfer  */
#define SWITCH_HOST_PERM     "switch_host"
#define DISABLE_HOST_PERM    "disable"           /* Disable host         */
#define DISABLE_AFD_PERM     "disable_afd"       /* Disable AFD          */
#define INFO_PERM            "info"              /* Info about host      */
#define DEBUG_PERM           "debug"             /* Enable/Disable debug */
#define TRACE_PERM           "trace"             /* Enable/Disable trace */
#define FULL_TRACE_PERM      "full_trace"        /* Enable/Disable full  */
                                                 /* trace                */
#define SHOW_EXEC_STAT_PERM  "show_exec_stat"    /* Show and reset exec  */
                                                 /* stats for dir_check  */
#define RETRY_PERM           "retry"
#define VIEW_JOBS_PERM       "view_jobs"         /* Detailed transfer.   */
#define AFD_LOAD_PERM        "afd_load"
#define RR_DC_PERM           "reread_dir_config" /* Reread DIR_CONFIG.   */
#define RR_HC_PERM           "reread_host_config"/* Reread HOST_CONFIG.  */
#define AFD_CMD_PERM         "afdcmd"            /* Send commands to AFD.*/
#define AFD_CFG_PERM         "afdcfg"            /* Configure AFD.       */
#define VIEW_DIR_CONFIG_PERM "view_dir_config"   /* Info on DIR_CONFIG   */
#define SHOW_QUEUE_PERM      "show_queue"        /* Show + delete queue. */
#define DELETE_QUEUE_PERM    "delete_queue"      /* Delete queue.        */
#define XSHOW_STAT_PERM      "xshow_stat"        /* Show statistics.     */

#define MON_CTRL_PERM        "mon_ctrl"          /* mon_ctrl dialog.     */
#define MON_STARTUP_PERM     "mon_startup"       /* Startup AFD_MON.     */
#define MON_SHUTDOWN_PERM    "mon_shutdown"      /* Shutdown AFD_MON.    */
#define MAFD_CMD_PERM        "mafd_cmd"          /* Send commands to AFD */
                                                 /* monitor.             */

#define DIR_CTRL_PERM        "dir_ctrl"          /* dir_ctrl dialog.     */
#define DIR_INFO_PERM        "dir_info"          /* dir_info dialog.     */
#define DISABLE_DIR_PERM     "disable_dir"       /* Disable directory.   */
#define RESCAN_PERM          "rescan"            /* Rescan directory.    */

#define PERMISSION_DENIED_STR "You are not permitted to use this program."

#endif /* __permission_h */

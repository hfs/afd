/*
 *  init_asmtp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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

#include "asmtpdefs.h"

DESCR__S_M3
/*
 ** NAME
 **   init_asmtp - checks syntax of input for process asmtp
 **
 ** SYNOPSIS
 **   int init_asmtp(int         argc,
 **                  char        *argv[],
 **                  struct data *p_db)
 **
 ** DESCRIPTION
 **   This module checks whether the syntax of aftp is correct and
 **   stores these values into the structure job.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.11.2000 H.Kiehl    Created
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* stderr, fprintf()                  */
#include <stdlib.h>                /* exit(), atoi()                     */
#include <string.h>                /* strcpy(), strerror(), strcmp()     */
#include <ctype.h>                 /* isdigit()                          */
#include <errno.h>
#include "smtpdefs.h"

/* Global variables */
extern int  sys_log_fd;
extern char *p_work_dir;

/* Local global variables */
static char name[30];

/* local functions */
static void usage(void);


/*############################ init_asmtp() #############################*/
int
init_asmtp(int argc, char *argv[], struct data *p_db)
{
   int  correct = YES;                /* Was input/syntax correct?      */

   (void)strcpy(name, argv[0]);

   /* First initialize all values with default values */
   p_db->blocksize        = DEFAULT_TRANSFER_BLOCKSIZE;
   p_db->smtp_server[0]   = '\0';
   p_db->user[0]          = '\0';
   p_db->hostname[0]      = '\0';
   p_db->port             = DEFAULT_SMTP_PORT;
   p_db->remove           = NO;
   p_db->transfer_timeout = DEFAULT_TRANSFER_TIMEOUT;
   p_db->verbose          = NO;
   p_db->no_of_files      = 0;
   p_db->subject          = NULL;
   p_db->flag             = 0;

   /* Evaluate all arguments with '-' */
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      switch (*(argv[0] + 1))
      {
         case 'a' : /* Adress, where to send the mail, that is user@host. */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr, "ERROR   : Keine Adresse angegeben fuer Option -a.\n");
#else
               (void)fprintf(stderr, "ERROR   : No address specified for option -a.\n");
#endif
               correct = NO;
            }
            else
            {
               int  length = 0;
               char *ptr;

               ptr = argv[1];
               while ((*ptr != '@') && (*ptr != '\0') &&
                      (length < MAX_USER_NAME_LENGTH))
               {
                  p_db->user[length] = *ptr;
                  ptr++; length++;
               }
               if (*ptr == '@')
               {
                  ptr++; length++;
                  if (length > 0)
                  {
                     p_db->user[length] = '\0';
                     length = 0;
                     while ((*ptr != '\0') && (length < MAX_FILENAME_LENGTH))
                     {
                        p_db->hostname[length] = *ptr;
                        ptr++; length++;
                     }
                     if (length > 0)
                     {
                        if (length < MAX_FILENAME_LENGTH)
                        {
                           p_db->hostname[length] = '\0';
                        }
                        else
                        {
                           (void)fprintf(stderr,
#ifdef _GERMAN
                                         "ERROR   : Die Zieladdresse ist zu lang, darf maximal %d Zeichen lang sein.\n",
#else
                                         "ERROR   : The hostname is to long, it may only be %d characters long.\n",
#endif
                                         MAX_FILENAME_LENGTH);
                           correct = NO;
                        }
                     }
                     else
                     {
#ifdef _GERMAN
                        (void)fprintf(stderr, "ERROR   : Keine Zieladresse angegeben.\n");
#else
                        (void)fprintf(stderr, "ERROR   : No hostname specified.\n");
#endif
                        correct = NO;
                     }
                  }
                  else /* No user name specified. */
                  {
#ifdef _GERMAN
                     (void)fprintf(stderr, "ERROR   : Kein Nutzername angegeben.\n");
#else
                     (void)fprintf(stderr, "ERROR   : No user specified.\n");
#endif
                     correct = NO;
                  }
               }
               else if (length < MAX_USER_NAME_LENGTH)
                    {
#ifdef _GERMAN
                       (void)fprintf(stderr, "ERROR   : Kein remote host angegeben.\n");
#else
                       (void)fprintf(stderr, "ERROR   : No remote host specified.\n");
#endif
                       correct = NO;
                    }
                    else
                    {
                       (void)fprintf(stderr,
#ifdef _GERMAN
                                     "ERROR   : Der Nutzername ist zu lang, darf maximal %d Zeichen lang sein.\n",
#else
                                     "ERROR   : The user name is to long, it may only be %d characters long.\n",
#endif
                                     MAX_USER_NAME_LENGTH);
                       correct = NO;
                    }
               argc--;
               argv++;
            }
            break;

         case 'b' : /* Transfer block size */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr, "ERROR   : Keine Blockgroesse angegeben fuer Option -b.\n");
#else
               (void)fprintf(stderr, "ERROR   : No block size specified for option -b.\n");
#endif
               correct = NO;
            }
            else
            {
               p_db->blocksize = atoi(*(argv + 1));
               argc--;
               argv++;
            }
            break;

         case 'e' : /* Encode file using BASE64. */

            p_db->flag |= ATTACH_FILE;
            break;

         case 'f' : /* Configuration file for filenames. */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr, "ERROR   : Keine Dateinamendatei angegeben fuer Option -f.\n");
#else
               (void)fprintf(stderr, "ERROR   : No filename file specified for option -f.\n");
#endif
               correct = NO;
            }
            else
            {
               char filename_file[MAX_PATH_LENGTH];

               argv++;
               (void)strcpy(filename_file, argv[0]);
               argc--;

               eval_filename_file(filename_file, p_db);
            }
            break;

         case 'h' : /* Remote host name */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr, "ERROR   : Kein Rechnername oder IP-Nummer angegeben fuer Option -h.\n");
#else
               (void)fprintf(stderr, "ERROR   : No host name or IP number specified for option -h.\n");
#endif
               correct = NO;
            }
            else
            {
               argv++;
               (void)strcpy(p_db->hostname, argv[0]);
               argc--;
            }
            break;

         case 'm' : /* Mail server that will send this mail. */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr, "ERROR   : Kein Mailserver oder IP-Nummer angegeben fuer Option -m.\n");
#else
               (void)fprintf(stderr, "ERROR   : No mail server name or IP number specified for option -m.\n");
#endif
               correct = NO;
            }
            else
            {
               argv++;
               (void)strcpy(p_db->smtp_server, argv[0]);
               argc--;
            }
            break;

         case 'n' : /* Filename is subject. */

            p_db->flag |= FILE_NAME_IS_SUBJECT;
            break;

         case 'p' : /* Remote TCP port number */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr, "ERROR   : Keine Portnummer angegeben fuer Option -p.\n");
#else
               (void)fprintf(stderr, "ERROR   : No port number specified for option -p.\n");
#endif
               correct = NO;
            }
            else
            {
               p_db->port = atoi(*(argv + 1));
               argc--;
               argv++;
            }
            break;

         case 'r' : /* Remove file that was transmitted. */

            p_db->remove = YES;
            break;

         case 's' : /* Subject. */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr, "ERROR   : Keine Subject angegeben fuer Option -s.\n");
#else
               (void)fprintf(stderr, "ERROR   : No subject specified for option -s.\n");
#endif
               correct = NO;
            }
            else
            {
               size_t length;

               argv++;
               length = strlen(argv[0]);
               if ((p_db->subject = malloc(length)) == NULL)
               {
                  (void)fprintf(stderr,
                                "ERROR   : malloc() error : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
                  correct = NO;
               }
               else
               {
                  (void)strcpy(p_db->subject, argv[0]);
               }
               argc--;
            }
            break;


         case 't' : /* Transfer timeout. */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr, "ERROR   : Keine maximale Wartezeit angegeben fuer Option -t.\n");
#else
               (void)fprintf(stderr, "ERROR   : No timeout specified for option -t.\n");
#endif
               correct = NO;
            }
            else
            {
               p_db->transfer_timeout = atol(*(argv + 1));
               argc--;
               argv++;
            }
            break;

         case 'u' : /* User name. */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr,
                             "ERROR   : Kein Nutzername angegeben fuer Option -u.\n");
#else
               (void)fprintf(stderr,
                             "ERROR   : No user specified for option -u.\n");
#endif
               correct = NO;
            }
            else
            {
               argv++;
               (void)strcpy(p_db->user, argv[0]);
               argc--;
            }
            break;

         case 'v' : /* Verbose mode. */

            p_db->verbose = YES;
            break;

         case 'y' : /* Filename is user. */

            p_db->flag |= FILE_NAME_IS_USER;
            break;

         case '?' : /* Help */

            usage();
            exit(0);

         default : /* Unknown parameter */

#ifdef _GERMAN
            (void)fprintf(stderr, "ERROR   : Unbekannter Parameter <%c>. (%s %d)\n",
#else
            (void)fprintf(stderr, "ERROR   : Unknown parameter <%c>. (%s %d)\n",
#endif
                          *(argv[0] + 1), __FILE__, __LINE__);
            correct = NO;
            break;
      } /* switch(*(argv[0] + 1)) */
   }
   if ((*argv)[0] != '-')
   {
      argc++;
      argv--;
   }

   if (p_db->hostname[0] == '\0')
   {
#ifdef _GERMAN
      (void)fprintf(stderr,
                     "ERROR   : Keinen Rechnernamen oder IP-Nummer angegeben.\n");
#else
      (void)fprintf(stderr,
                    "ERROR   : No host name or IP number specified.\n");
#endif
      correct = NO;
   }

   if ((p_db->no_of_files == 0) && (argc == 0))
   {
#ifdef _GERMAN
      (void)fprintf(stderr,
                    "ERROR   : Keine Dateien zum versenden angegeben.\n");
#else
      (void)fprintf(stderr,
                    "ERROR   : No files to be send specified.\n");
#endif
      correct = NO;
   }
   else if ((correct == YES) && (argc > 0))
        {
           int i = p_db->no_of_files;

           if (i == 0)
           {
              RT_ARRAY(p_db->filename, argc, MAX_PATH_LENGTH, char);
           }
           else
           {
              REALLOC_RT_ARRAY(p_db->filename, (i + argc), MAX_PATH_LENGTH, char);
           }
           p_db->no_of_files += argc - 1;
           while ((--argc > 0) && ((*++argv)[0] != '-'))
           {
              (void)strcpy(p_db->filename[i], argv[0]);
              i++;
           }
        }

   /* If input is not correct show syntax */
   if (correct == NO)
   {
      usage();
      exit(SYNTAX_ERROR);
   }

   return(SUCCESS);
} /* eval_input() */


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(void)
{
#ifdef _GERMAN
   (void)fprintf(stderr, "SYNTAX: %s [Optionen] Datei(en)\n\n", name);
   (void)fprintf(stderr, "  Optionen                     Beschreibung\n");
   (void)fprintf(stderr, "  --version                  - Zeigt aktuelle Version.\n");
   (void)fprintf(stderr, "  -a <Nutzer@Zieladdresse>   - Die Addresse wohin die Mail geschickt\n");
   (void)fprintf(stderr, "                               werden soll.\n");
   (void)fprintf(stderr, "  -b <Blockgroesse>          - Blockgroesse in Bytes. Standard\n");
   (void)fprintf(stderr, "                               %d Bytes.\n", DEFAULT_TRANSFER_BLOCKSIZE);
   (void)fprintf(stderr, "  -e                         - Enkodiere Dateien in BASE64.\n");
   (void)fprintf(stderr, "  -f <Dateinamen Datei>      - Datei in der alle Dateinamen stehen die\n");
   (void)fprintf(stderr, "                               zu verschicken sind.\n");
   (void)fprintf(stderr, "  -h <Rechnername>           - Rechnername an den die Mail geschickt\n");
   (void)fprintf(stderr, "                               werden soll.\n");
   (void)fprintf(stderr, "  -m <Mailserver-addresse>   - Mailserver der die Mail verschicken.\n");
   (void)fprintf(stderr, "                               soll. Standard ist %s.\n", SMTP_HOST_NAME);
   (void)fprintf(stderr, "  -n                         - Dateiname ist das Subject.\n");
   (void)fprintf(stderr, "  -p <Portnummer>            - IP Portnummer des SMTP-Servers.\n");
   (void)fprintf(stderr, "                               Standard %d.\n", DEFAULT_SMTP_PORT);
   (void)fprintf(stderr, "  -r                         - Loescht die uebertragenen Dateien.\n");
   (void)fprintf(stderr, "  -s <Subject>               - .\n");
   (void)fprintf(stderr, "  -t <Wartezeit>             - Maximale Wartezeit fuer SMTP, in Sekunden.\n");
   (void)fprintf(stderr, "                               Standard %lds.\n", DEFAULT_TRANSFER_TIMEOUT);
   (void)fprintf(stderr, "  -u <Nutzer>                - Nutzername an den die Mail geschickt\n");
   (void)fprintf(stderr, "                               werden soll.\n");
   (void)fprintf(stderr, "  -v                         - Ausfuehrlich. Die Kommandos und die\n");
   (void)fprintf(stderr, "                               Antworten des Servers werden ausgegeben.\n");
   (void)fprintf(stderr, "  -y                         - Dateiname ist der Nutzer.\n");
   (void)fprintf(stderr, "  -?                         - Diese Hilfe.\n\n");
   (void)fprintf(stderr, "  Es werden folgende Rueckgabewerte zurueckgegeben:\n");
   (void)fprintf(stderr, "      %2d - Datei wurde erfolgreich Uebertragen.\n", TRANSFER_SUCCESS);
   (void)fprintf(stderr, "      %2d - Es konnte keine Verbindung hergestellt werden.\n", CONNECT_ERROR);
   (void)fprintf(stderr, "      %2d - Nutzername falsch.\n", USER_ERROR);
   (void)fprintf(stderr, "      %2d - Konnte ascii/binary-modus nicht setzen.\n", TYPE_ERROR);
   (void)fprintf(stderr, "      %2d - Remote Datei konnte nicht geoeffnet werden.\n", OPEN_REMOTE_ERROR);
   (void)fprintf(stderr, "      %2d - Fehler beim schreiben in der remote Datei.\n", WRITE_REMOTE_ERROR);
   (void)fprintf(stderr, "      %2d - Remote Datei konnte nicht geschlossen werden.\n", CLOSE_REMOTE_ERROR);
   (void)fprintf(stderr, "      %2d - Remote Datei konnte nicht umbenannt werden.\n", MOVE_REMOTE_ERROR);
   (void)fprintf(stderr, "      %2d - Zielverzeichnis konnte nicht gesetzt werden.\n", CHDIR_ERROR);
   (void)fprintf(stderr, "      %2d - Verbindung abgebrochen (FTP-timeout).\n", TIMEOUT_ERROR);
   (void)fprintf(stderr, "      %2d - Ursprungsdatei konnte nicht geoeffnet werden.\n", OPEN_LOCAL_ERROR);
   (void)fprintf(stderr, "      %2d - Fehler beim lesen der Ursprungsdatei.\n", READ_LOCAL_ERROR);
   (void)fprintf(stderr, "      %2d - Systemfehler stat().\n", STAT_LOCAL_ERROR);
   (void)fprintf(stderr, "      %2d - Systemfehler malloc().\n", ALLOC_ERROR);
   (void)fprintf(stderr, "      %2d - Falsche Eingabe.\n", SYNTAX_ERROR);
#else
   (void)fprintf(stderr, "SYNTAX: %s [options] file(s)\n\n", name);
   (void)fprintf(stderr, "  OPTIONS                      DESCRIPTION\n");
   (void)fprintf(stderr, "  --version                  - Show current version\n");
   (void)fprintf(stderr, "  -a <user@host>             - The address where the mail is sent to.\n");
   (void)fprintf(stderr, "  -b <block size>            - Transfer block size in bytes. Default %d\n", DEFAULT_TRANSFER_BLOCKSIZE);
   (void)fprintf(stderr, "                               Bytes.\n");
   (void)fprintf(stderr, "  -e                         - Encode files in BASE64.\n");
   (void)fprintf(stderr, "  -f <filename>              - File containing a list of filenames\n");
   (void)fprintf(stderr, "                               that are to be send.\n");
   (void)fprintf(stderr, "  -h <hostname | IP number>  - Recipient hostname of this mail.\n");
   (void)fprintf(stderr, "  -p <port number>           - Remote port number of SMTP-server.\n");
   (void)fprintf(stderr, "                               Default %d.\n", DEFAULT_SMTP_PORT);
   (void)fprintf(stderr, "  -m <mailserver-address>    - Mailserver that will send this mail.\n");
   (void)fprintf(stderr, "                               Default is %s.\n", SMTP_HOST_NAME);
   (void)fprintf(stderr, "  -n                         - File name is subject.\n");
   (void)fprintf(stderr, "  -r                         - Remove transmitted file.\n");
   (void)fprintf(stderr, "  -s <subject>               - Subject of this mail.\n");
   (void)fprintf(stderr, "  -t <timout>                - SMTP timeout in seconds. Default %lds.\n", DEFAULT_TRANSFER_TIMEOUT);
   (void)fprintf(stderr, "  -u <user>                  - The user who should get the mail.\n");
   (void)fprintf(stderr, "  -v                         - Verbose. Shows all SMTP commands and\n");
   (void)fprintf(stderr, "                               the reply from the SMTP server.\n");
   (void)fprintf(stderr, "  -y                         - File name is user.\n");
   (void)fprintf(stderr, "  -?                         - Display this help and exit.\n");
   (void)fprintf(stderr, "  The following values are returned on exit:\n");
   (void)fprintf(stderr, "      %2d - File transmitted successfully.\n", TRANSFER_SUCCESS);
   (void)fprintf(stderr, "      %2d - Failed to connect.\n", CONNECT_ERROR);
   (void)fprintf(stderr, "      %2d - User name wrong.\n", USER_ERROR);
   (void)fprintf(stderr, "      %2d - Failed to set ascii/binary mode.\n", TYPE_ERROR);
   (void)fprintf(stderr, "      %2d - Failed to send NLST command.\n", LIST_ERROR);
   (void)fprintf(stderr, "      %2d - Failed to open remote file.\n", OPEN_REMOTE_ERROR);
   (void)fprintf(stderr, "      %2d - Error when writting into remote file.\n", WRITE_REMOTE_ERROR);
   (void)fprintf(stderr, "      %2d - Failed to close remote file.\n", CLOSE_REMOTE_ERROR);
   (void)fprintf(stderr, "      %2d - Failed to rename remote file.\n", MOVE_REMOTE_ERROR);
   (void)fprintf(stderr, "      %2d - Remote directory could not be set.\n", CHDIR_ERROR);
   (void)fprintf(stderr, "      %2d - FTP-timeout.\n", TIMEOUT_ERROR);
   (void)fprintf(stderr, "      %2d - Could not open source file.\n", OPEN_LOCAL_ERROR);
   (void)fprintf(stderr, "      %2d - Failed to read source file.\n", READ_LOCAL_ERROR);
   (void)fprintf(stderr, "      %2d - System error stat().\n", STAT_LOCAL_ERROR);
   (void)fprintf(stderr, "      %2d - System error malloc().\n", ALLOC_ERROR);
   (void)fprintf(stderr, "      %2d - Syntax wrong.\n", SYNTAX_ERROR);
#endif

   return;
}

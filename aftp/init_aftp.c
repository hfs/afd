/*
 *  init_aftp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2001 Deutscher Wetterdienst (DWD),
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

#include "aftpdefs.h"

DESCR__S_M3
/*
 ** NAME
 **   init_aftp - checks syntax of input for process aftp
 **
 ** SYNOPSIS
 **   int init_aftp(int         argc,
 **                 char        *argv[],
 **                 struct data *p_db)
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
 **   24.09.1997 H.Kiehl    Created
 **   31.10.1997 T.Freyberg Error messages in german.
 **   17.07.2000 H.Kiehl    Added options -c and -f.
 **   30.07.2000 H.Kiehl    Added option -x.
 **   17.10.2000 H.Kiehl    Added support to retrieve files from a
 **                         remote host.
 **   14.09.2001 H.Kiehl    Added option -k.
 **   20.09.2001 H.Kiehl    Added option -A.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* stderr, fprintf()                  */
#include <stdlib.h>                /* exit(), atoi()                     */
#include <string.h>                /* strcpy(), strerror(), strcmp()     */
#include <ctype.h>                 /* isdigit()                          */
#include <errno.h>
#include "ftpdefs.h"

/* Global variables */
extern int  no_of_host,
            sys_log_fd,
            fsa_id;
extern char *p_work_dir;

/* Local global variables */
static char name[30];

/* local functions */
static void usage(void);


/*############################ init_aftp() ##############################*/
int
init_aftp(int argc, char *argv[], struct data *p_db)
{
   int  correct = YES;                /* Was input/syntax correct?      */
   char *ptr;

   ptr = argv[0] + strlen(argv[0]) - 1;
   while ((*ptr != '/') && (ptr != &argv[0][0]))
   {
      ptr--;
   }
   if (*ptr == '/')
   {
      ptr++;
   }
   (void)strcpy(name, ptr);
   if (name[0] == 'r')
   {
      p_db->aftp_mode = RETRIEVE_MODE;
   }
   else if (name[0] == 't')
        {
           p_db->aftp_mode = TEST_MODE;
        }
        else
        {
           p_db->aftp_mode = TRANSFER_MODE;
        }

   /* First initialize all values with default values */
   p_db->file_size_offset = -1;       /* No appending */
   p_db->blocksize        = DEFAULT_TRANSFER_BLOCKSIZE;
   p_db->remote_dir[0]    = '\0';
   p_db->hostname[0]      = '\0';
   p_db->lock             = DOT;
   p_db->lock_notation[0] = '.';
   p_db->lock_notation[1] = '\0';
   p_db->transfer_mode    = 'I';
   p_db->ftp_mode         = ACTIVE_MODE;
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
   p_db->keepalive        = NO;
#endif /* FTP_CTRL_KEEP_ALIVE_INTERVAL */
   p_db->port             = DEFAULT_FTP_PORT;
   (void)strcpy(p_db->user, DEFAULT_AFD_USER);
   (void)strcpy(p_db->password, DEFAULT_AFD_PASSWORD);
   p_db->remove           = NO;
   p_db->transfer_timeout = DEFAULT_TRANSFER_TIMEOUT;
   p_db->verbose          = NO;
   p_db->append           = NO;
   if (name[0] == 't')
   {
      p_db->no_of_files      = 1;
      p_db->dummy_size       = DEFAULT_TRANSFER_BLOCKSIZE;
   }
   else
   {
      p_db->no_of_files      = 0;
   }
   p_db->filename         = NULL;

   /* Evaluate all arguments with '-' */
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      switch (*(argv[0] + 1))
      {
         case 'A' : /* Search for file localy for appending. */

            p_db->append = YES;
            break;

         case 'a' : /* Remote file size offset for appending */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr, "ERROR   : Keine Spaltennummer angegeben fuer Option -a.\n");
#else
               (void)fprintf(stderr, "ERROR   : No file size offset specified for option -a.\n");
#endif
               correct = NO;
            }
            else
            {
               if ((name[0] == 'r') || (name[0] == 't'))
               {
#ifdef _GERMAN
                  (void)fprintf(stderr, "ERROR   : Diese Option ist nur gueltig fuer %s.\n", &name[1]);
#else
                  (void)fprintf(stderr, "ERROR   : This option is only for %s.\n", &name[1]);
#endif
                  correct = NO;
               }
               else
               {
                  p_db->file_size_offset = (signed char)atoi(*(argv + 1));
               }
               argc--;
               argv++;
            }
            break;

         case 'b' : /* FTP transfer block size */

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

         case 'c' : /* Configuration file for user, passwd, etc */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr, "ERROR   : Keine Konfigurationsdatei angegeben fuer Option -c.\n");
#else
               (void)fprintf(stderr, "ERROR   : No config file specified for option -c.\n");
#endif
               correct = NO;
            }
            else
            {
               char config_file[MAX_PATH_LENGTH];

               argv++;
               (void)strcpy(config_file, argv[0]);
               argc--;

               eval_config_file(config_file, p_db);
            }
            break;

         case 'd' : /* Target directory on remote host */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr, "ERROR   : Kein Verzeichnis angegeben fuer Option -d.\n");
#else
               (void)fprintf(stderr, "ERROR   : No target directory for option -d.\n");
#endif
               correct = NO;
            }
            else
            {
               argv++;
               (void)strcpy(p_db->remote_dir, argv[0]);
               argc--;
            }
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

#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
         case 'k' : /* Keep control connection alive. */

            p_db->keepalive = YES;
            break;
#endif /* FTP_CTRL_KEEP_ALIVE_INTERVAL */

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

         case 'l' : /* Lock type */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr, "ERROR   : Kein Verriegelungsmechanismus angegeben fuer Option -l.\n");
#else
               (void)fprintf(stderr, "ERROR   : No lock type specified for option -l.\n");
#endif
               correct = NO;
            }
            else
            {
               argv++;
               argc--;

               if (name[0] == 'r')
               {
#ifdef _GERMAN
                  (void)fprintf(stderr, "ERROR   : Diese Option ist nur gueltig fuer %s.\n", &name[1]);
#else
                  (void)fprintf(stderr, "ERROR   : This option is only for %s.\n", &name[1]);
#endif
                  correct = NO;
               }
               else
               {
                  /* Check which lock type is specified */
                  if (strcmp(argv[0], LOCK_DOT) == 0)
                  {
                     p_db->lock = DOT;
                  }
#ifdef _WITH_READY_FILES
                  else if (strcmp(argv[0], READY_FILE_ASCII) == 0)
                       {
                          p_db->lock = READY_A_FILE;
                       }
                  else if (strcmp(argv[0], READY_FILE_BINARY) == 0)
                       {
                          p_db->lock = READY_B_FILE;
                       }
#endif /* _WITH_READY_FILES */
                  else if (strcmp(argv[0], LOCK_OFF) == 0)
                       {
                          p_db->lock = OFF;
                       }
                       else
                       {
                          (void)strcpy(p_db->lock_notation, argv[0]);
                       }
               }
            }
            break;

         case 'm' : /* FTP transfer mode */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr, "ERROR   : Kein Uebertragungsmodus angegeben fuer Option -m.\n");
#else
               (void)fprintf(stderr, "ERROR   : No transfer mode specified for option -m.\n");
#endif
               correct = NO;
            }
            else
            {
               switch(*(argv + 1)[0])
               {
                  case 'a':
                  case 'A': /* ASCII mode */

                     p_db->transfer_mode = 'A';
                     argv++;
                     argc--;
                     break;

                  case 'i':
                  case 'I':
                  case 'b':
                  case 'B': /* Bianary mode */

                     p_db->transfer_mode = 'I';
                     argv++;
                     argc--;
                     break;

                  default : /* Wrong/unknown mode! */

                     (void)fprintf(stderr,
#ifdef _GERMAN
                                   "ERROR   : Unbekannter FTP Uebertragungsmodus <%c> angegeben fuer Option -m.\n",
#else
                                   "ERROR   : Unknown FTP transfer mode <%c> specified for option -m.\n",
#endif
                                   *(argv + 1)[0]);
                     correct = NO;
                     break;
               }
            }
            break;

         case 'n' : /* Specify the number of files. */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr, "ERROR   : Keine Anzahl von Dateien angegeben fuer Option -n.\n");
#else
               (void)fprintf(stderr, "ERROR   : No number of files specified for option -n.\n");
#endif
               correct = NO;
            }
            else
            {
               if (name[0] != 't')
               {
                  char *p_name;

                  if (name[0] == 'r')
                  {
                     p_name = &name[1];
                  }
                  else
                  {
                     p_name = name;
                  }
#ifdef _GERMAN
                  (void)fprintf(stderr, "ERROR   : Diese Option ist nur gueltig fuer t%s.\n", p_name);
#else
                  (void)fprintf(stderr, "ERROR   : This option is only for t%s.\n", p_name);
#endif
                  correct = NO;
               }
               else
               {
                  p_db->no_of_files = atoi(*(argv + 1));
               }
               argc--;
               argv++;
            }
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

         case 'u' : /* User name and password for remote login. */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr,
                             "ERROR   : Kein Nutzername und Passwort angegeben fuer Option -u.\n");
#else
               (void)fprintf(stderr,
                             "ERROR   : No user and password specified for option -u.\n");
#endif
               correct = NO;
            }
            else
            {
               argv++;
               (void)strcpy(p_db->user, argv[0]);
               argc--;

               /* If user is specified a password must be there as well! */
               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
#ifdef _GERMAN
                  (void)fprintf(stderr,
                                "ERROR   : Kein Passwort angegeben fuer Option -u.\n");
#else
                  (void)fprintf(stderr,
                                "ERROR   : No password specified for option -u.\n");
#endif
                  correct = NO;
               }
               else
               {
                  argv++;
                  (void)strcpy(p_db->password, argv[0]);
                  argc--;
               }
            }
            break;

         case 'r' : /* Remove file that was transmitted. */

            p_db->remove = YES;
            break;

         case 's' : /* Dummy file size. */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
#ifdef _GERMAN
               (void)fprintf(stderr, "ERROR   : Keine Dateigroesse angegeben fuer Option -s.\n");
#else
               (void)fprintf(stderr, "ERROR   : No file size specified for option -s.\n");
#endif
               correct = NO;
            }
            else
            {
               if (name[0] != 't')
               {
                  char *p_name;

                  if (name[0] == 'r')
                  {
                     p_name = &name[1];
                  }
                  else
                  {
                     p_name = name;
                  }
#ifdef _GERMAN
                  (void)fprintf(stderr, "ERROR   : Diese Option ist nur gueltig fuer t%s.\n", p_name);
#else
                  (void)fprintf(stderr, "ERROR   : This option is only for t%s.\n", p_name);
#endif
                  correct = NO;
               }
               else
               {
                  p_db->dummy_size = atoi(*(argv + 1));
               }
               argc--;
               argv++;
            }
            break;

         case 't' : /* FTP timeout. */

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

         case 'v' : /* Verbose mode. */

            p_db->verbose = YES;
            break;

         case 'x' : /* Use passive mode instead of active mode. */

            p_db->ftp_mode = PASSIVE_MODE;
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
           if (name[0] == 't')
           {
              if (p_db->filename == NULL)
              {
                 RT_ARRAY(p_db->filename, 1, MAX_PATH_LENGTH, char);
                 (void)strcpy(p_db->filename[0], argv[1]);
              }
              else
              {
                 /* Ignore what ever the dummy user has written. */;
              }
           }
           else
           {
              int i = p_db->no_of_files;

              if (i == 0)
              {
                 RT_ARRAY(p_db->filename, argc, MAX_PATH_LENGTH, char);
              }
              else
              {
                 REALLOC_RT_ARRAY(p_db->filename, (i + argc),
                                  MAX_PATH_LENGTH, char);
              }
              p_db->no_of_files += argc - 1;
              while ((--argc > 0) && ((*++argv)[0] != '-'))
              {
                 (void)strcpy(p_db->filename[i], argv[0]);
                 i++;
              }
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
   char *p_name;

   if ((name[0] == 'r') || (name[0] == 't'))
   {
      p_name = &name[1];
   }
   else
   {
      p_name = name;
   }
#ifdef _GERMAN
   (void)fprintf(stderr, "SYNTAX: [t|r]%s [Optionen] -h <Rechnername | IP Nummer> Datei(en)\n\n", p_name);
   (void)fprintf(stderr, "   Wenn man das Programm mit r%s aufruft, werden die Dateien\n", p_name);
   (void)fprintf(stderr, "   abgeholt, sonst (mit %s) werden die Dateien geschickt.\n\n", p_name);
   (void)fprintf(stderr, "  Optionen                             Beschreibung\n");
   (void)fprintf(stderr, "  --version                          - Zeigt aktuelle Version.\n");
   if (name[0] == 'r')
   {
      (void)fprintf(stderr, "  -A                                 - Wenn eine Datei nur teilweise uebertragen\n");
      (void)fprintf(stderr, "                                       wurde, wird nur der Rest uebertragen und ans\n");
      (void)fprintf(stderr, "                                       Ende der lokalen Datei angehaengt.\n");
   }
   if ((name[0] != 'r') && (name[0] != 't'))
   {
      (void)fprintf(stderr, "  -a <Spaltennummer>                 - Nummer der Spalte in der der Dateiname\n");
      (void)fprintf(stderr, "                                       steht bei LIST Kommando. Wenn man -2 eingibt\n");
      (void)fprintf(stderr, "                                       wird versucht automatisch die groesse mit\n");
      (void)fprintf(stderr, "                                       den SIZE Befehl zu ermitteln.\n");
   }
   (void)fprintf(stderr, "  -b <Blockgroesse>                  - FTP Blockgroesse in Bytes. Standard\n");
   (void)fprintf(stderr, "                                       %d Bytes.\n", DEFAULT_TRANSFER_BLOCKSIZE);
   (void)fprintf(stderr, "  -c <Konfigurations Datei>          - Konfigurationsdatei die den User, Passwort\n");
   (void)fprintf(stderr, "                                       und Zieleverzeichnis im URL Format beinhaltet.\n");
   (void)fprintf(stderr, "  -d <Zielverzeichnis>               - Verzeichnis auf dem Zielrechner.\n");
   (void)fprintf(stderr, "  -f <Dateinamen Datei>              - Datei in der alle Dateinamen stehen die\n");
   (void)fprintf(stderr, "                                       zu verschicken sind.\n");
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
   (void)fprintf(stderr, "  -k                                 - FTP Kontrolverbindung mit STAT-Befehlen\n");
   (void)fprintf(stderr, "                                       waerend eine Datei uebertragen wird,\n");
   (void)fprintf(stderr, "                                       aufrecht erhalten.\n");
#endif /* FTP_CTRL_KEEP_ALIVE_INTERVAL */
   if (name[0] != 'r')
   {
#ifdef _WITH_READY_FILES
      (void)fprintf(stderr, "  -l <DOT | OFF | RDYA | RDYB | xyz> - Verriegelungsart waehrend der\n");
      (void)fprintf(stderr, "                                       Uebertragung.\n");
#else
      (void)fprintf(stderr, "  -l <DOT | OFF | xyz.>              - Verriegelungsart waehrend der\n");
      (void)fprintf(stderr, "                                       Uebertragung.\n");
#endif
   }
   (void)fprintf(stderr, "  -m <A | I>                         - FTP Uebertragungsmodus, ASCII oder\n");
   (void)fprintf(stderr, "                                       Binaer (Standard).\n");
   if (name[0] == 't')
   {
      (void)fprintf(stderr, "  -n <Anzahl Dateien>                - Anzahl der zu uebertragenen Dateien.\n");
   }
   (void)fprintf(stderr, "  -p <Portnummer>                    - IP Portnummer des FTP-Servers.\n");
   (void)fprintf(stderr, "  -u <Nutzer> <Passwort>             - Nutzername und Passwort, wenn nicht\n");
   (void)fprintf(stderr, "                                       angegeben %s.\n", DEFAULT_AFD_USER);
   if (name[0] == 'r')
   {
      (void)fprintf(stderr, "  -r                                 - Loescht die Dateien die geholt wurden.\n");
   }
   else
   {
      (void)fprintf(stderr, "  -r                                 - Loescht die uebertragenen Dateien.\n");
   }
   if (name[0] == 't')
   {
      (void)fprintf(stderr, "  -s <Dateigroesse>                  - Dateigroesse der zu uebertragenen Dateien.\n");
   }
   (void)fprintf(stderr, "  -t <Wartezeit>                     - Maximale Wartezeit fuer FTP, in Sekunden.\n");
   (void)fprintf(stderr, "                                       Standard %lds.\n", DEFAULT_TRANSFER_TIMEOUT);
   (void)fprintf(stderr, "  -v                                 - Ausfuehrlich. Die Kommandos und die\n");
   (void)fprintf(stderr, "                                       Antworten des Servers werden ausgegeben.\n");
   (void)fprintf(stderr, "  -x                                 - Benutzt den FTP passive mode anstatt\n");
   (void)fprintf(stderr, "                                       von aktive mode waehrend die Daten-\n");
   (void)fprintf(stderr, "                                       verbindung aufgebaut wird.\n");
   (void)fprintf(stderr, "  -?                                 - Diese Hilfe.\n\n");
   (void)fprintf(stderr, "  Es werden folgende Rueckgabewerte zurueckgegeben:\n");
   (void)fprintf(stderr, "      %2d - Datei wurde erfolgreich Uebertragen.\n", TRANSFER_SUCCESS);
   (void)fprintf(stderr, "      %2d - Es konnte keine Verbindung hergestellt werden.\n", CONNECT_ERROR);
   (void)fprintf(stderr, "      %2d - Nutzername falsch.\n", USER_ERROR);
   (void)fprintf(stderr, "      %2d - Passwort falsch.\n", PASSWORD_ERROR);
   (void)fprintf(stderr, "      %2d - Konnte ascii/binary-modus nicht setzen.\n", TYPE_ERROR);
   (void)fprintf(stderr, "      %2d - Konnte NLST Kommando nicht schicken.\n", LIST_ERROR);
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
   (void)fprintf(stderr, "SYNTAX: [t|r]%s [options] -h <host name | IP number> file(s)\n\n", p_name);
   (void)fprintf(stderr, "   When calling it with r%s files will be retrieved from the\n", p_name);
   (void)fprintf(stderr, "   given host, otherwise (when using %s) files will be send to that host.\n\n", p_name);
   (void)fprintf(stderr, "  OPTIONS                              DESCRIPTION\n");
   (void)fprintf(stderr, "  --version                          - Show current version\n");
   if (name[0] == 'r')
   {
      (void)fprintf(stderr, "  -A                                 - If only part of a file was retrieved, you\n");
      (void)fprintf(stderr, "                                       can retrieve the rest with this option.\n");
   }
   if ((name[0] != 'r') && (name[0] != 't'))
   {
      (void)fprintf(stderr, "  -a <file size offset>              - Offset of file name when doing a LIST\n");
      (void)fprintf(stderr, "                                       command on the remote side. If you\n");
      (void)fprintf(stderr, "                                       specify -2 it will try to determine\n");
      (void)fprintf(stderr, "                                       the size with the SIZE command.\n");
   }
   (void)fprintf(stderr, "  -b <block size>                    - FTP block size in bytes. Default %d\n", DEFAULT_TRANSFER_BLOCKSIZE);
   (void)fprintf(stderr, "                                       Bytes.\n");
   (void)fprintf(stderr, "  -c <config file>                   - Configuration file holding user name,\n");
   (void)fprintf(stderr, "                                       password und target directory in URL\n");
   (void)fprintf(stderr, "                                       format.\n");
   (void)fprintf(stderr, "  -d <remote directory>              - Directory where file(s) are to be stored.\n");
   (void)fprintf(stderr, "  -f <filename>                      - File containing a list of filenames\n");
   (void)fprintf(stderr, "                                       that are to be send.\n");
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
   (void)fprintf(stderr, "  -k                                 - Keep FTP control connection with STAT\n");
   (void)fprintf(stderr, "                                       calls alive/fresh.\n");
#endif /* FTP_CTRL_KEEP_ALIVE_INTERVAL */
   if (name[0] != 'r')
   {
#ifdef _WITH_READY_FILES
      (void)fprintf(stderr, "  -l <DOT | OFF | RDYA | RDYB | xyz> - How to lock the file on the remote site.\n");
#else
      (void)fprintf(stderr, "  -l <DOT | OFF | xyz.>              - How to lock the file on the remote site.\n");
#endif
   }
   (void)fprintf(stderr, "  -m <A | I>                         - FTP transfer mode, ASCII or binary.\n");
   (void)fprintf(stderr, "                                       Default is binary.\n");
   if (name[0] == 't')
   {
      (void)fprintf(stderr, "  -n <number of files>               - Number of files to be transfered.\n");
   }
   (void)fprintf(stderr, "  -p <port number>                   - Remote port number of FTP-server.\n");
   (void)fprintf(stderr, "  -u <user> <password>               - Remote user name and password. If not\n");
   (void)fprintf(stderr, "                                       supplied, it will login as %s.\n", DEFAULT_AFD_USER);
   if (name[0] == 'r')
   {
      (void)fprintf(stderr, "  -r                                 - Remove remote file after it was\n");
      (void)fprintf(stderr, "                                       retrieved.\n");
   }
   else
   {
      (void)fprintf(stderr, "  -r                                 - Remove transmitted file.\n");
   }
   if (name[0] == 't')
   {
      (void)fprintf(stderr, "  -s <file size>                     - File size of file to be transfered.\n");
   }
   (void)fprintf(stderr, "  -t <timout>                        - FTP timeout in seconds. Default %lds.\n", DEFAULT_TRANSFER_TIMEOUT);
   (void)fprintf(stderr, "  -v                                 - Verbose. Shows all FTP commands and\n");
   (void)fprintf(stderr, "                                       the reply from the remote server.\n");
   (void)fprintf(stderr, "  -x                                 - Use passive mode instead of active\n");
   (void)fprintf(stderr, "                                       mode when doing the data connection.\n");
   (void)fprintf(stderr, "  -?                                 - Display this help and exit.\n");
   (void)fprintf(stderr, "  The following values are returned on exit:\n");
   (void)fprintf(stderr, "      %2d - File transmitted successfully.\n", TRANSFER_SUCCESS);
   (void)fprintf(stderr, "      %2d - Failed to connect.\n", CONNECT_ERROR);
   (void)fprintf(stderr, "      %2d - User name wrong.\n", USER_ERROR);
   (void)fprintf(stderr, "      %2d - Wrong password.\n", PASSWORD_ERROR);
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

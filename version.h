/*
 *  version.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __version_h
#define __version_h

/*
 * MAJOR    - Major version number.
 *            This will only change if there was a major change within
 *            the AFD.
 * MINOR    - Minor version number.
 *            When new minor features are added, this number is increased.
 * BUG_FIX  - Bug fix number.
 */
#define MAJOR          1
#define MINOR          1
#define BUG_FIX        5
/* #define PRE_RELEASE    2 */

#define VERSION_ID     "--version"

#define AFD_MAINTAINER "Holger.Kiehl@dwd.de"

#define COPYRIGHT_0 "\n\n   Copyright (C) 1995 - 2000 Deutscher Wetterdienst, Holger Kiehl.\n\n"
#define COPYRIGHT_1 "   This program is free software;  you can redistribute  it and/or\n"
#define COPYRIGHT_2 "   modify it under the terms set out in the  COPYING  file,  which\n"
#define COPYRIGHT_3 "   is included in the AFD source distribution.\n\n"
#define COPYRIGHT_4 "   This program is distributed in the hope that it will be useful,\n"
#define COPYRIGHT_5 "   but WITHOUT ANY WARRANTY;  without even the implied warranty of\n"
#define COPYRIGHT_6 "   MERCHANTABILITY  or  FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
#define COPYRIGHT_7 "   COPYING  file for more details.\n\n"

/* Print current version and compilation time. */
#ifdef PRE_RELEASE
#define PRINT_VERSION(prog)                                                  \
        {                                                                    \
           (void)fprintf(stderr,                                             \
                         "%s -- Version: PRE %d.%d.%d-%d <%s> %6.6s %s [AFD]%s%s%s%s%s%s%s%s",\
                         (prog), MAJOR, MINOR, BUG_FIX, PRE_RELEASE,         \
                         AFD_MAINTAINER, __DATE__, __TIME__, COPYRIGHT_0,    \
                         COPYRIGHT_1, COPYRIGHT_2, COPYRIGHT_3, COPYRIGHT_4, \
                         COPYRIGHT_5, COPYRIGHT_6, COPYRIGHT_7);             \
        }
#else
#define PRINT_VERSION(prog)                                                  \
        {                                                                    \
           (void)fprintf(stderr,                                             \
                         "%s -- Version: %d.%d.%d <%s> %6.6s %s [AFD]%s%s%s%s%s%s%s%s",\
                         (prog), MAJOR, MINOR, BUG_FIX,                      \
                         AFD_MAINTAINER, __DATE__, __TIME__, COPYRIGHT_0,    \
                         COPYRIGHT_1, COPYRIGHT_2, COPYRIGHT_3, COPYRIGHT_4, \
                         COPYRIGHT_5, COPYRIGHT_6, COPYRIGHT_7);             \
        }
#endif

#define CHECK_FOR_VERSION(argc, argv)               \
        {                                           \
           if (argc == 2)                           \
           {                                        \
              if (strcmp(argv[1], VERSION_ID) == 0) \
              {                                     \
                 PRINT_VERSION(argv[0]);            \
                 exit(SUCCESS);                     \
              }                                     \
           }                                        \
        }

#endif /* __version_h */

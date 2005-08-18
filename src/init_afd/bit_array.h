/*
 *  bitarray.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __bit_array_h
#define __bit_array_h

#define ALL_MINUTES      1152921504576846463L
#define ALL_HOURS        16777215
#define ALL_DAY_OF_MONTH 2147483647
#define ALL_MONTH        4095
#define ALL_DAY_OF_WEEK  127

#ifdef _WORKING_LONG_LONG
static unsigned long long bit_array_long[60] =
                   {
                      1L,                    /* 0 */
                      2L,                    /* 1 */
                      4L,                    /* 2 */
                      8L,                    /* 3 */
                      16L,                   /* 4 */
                      32L,                   /* 5 */
                      64L,                   /* 6 */
                      128L,                  /* 7 */
                      256L,                  /* 8 */
                      512L,                  /* 9 */
                      1024L,                 /* 10 */
                      2048L,                 /* 11 */
                      4096L,                 /* 12 */
                      8192L,                 /* 13 */
                      16384L,                /* 14 */
                      32768L,                /* 15 */
                      65536L,                /* 16 */
                      131072L,               /* 17 */
                      262144L,               /* 18 */
                      524288L,               /* 19 */
                      1048576L,              /* 20 */
                      2097152L,              /* 21 */
                      4194304L,              /* 22 */
                      8388608L,              /* 23 */
                      16777216L,             /* 24 */
                      33554432L,             /* 25 */
                      67108864L,             /* 26 */
                      134217728L,            /* 27 */
                      268435456L,            /* 28 */
                      536870912L,            /* 29 */
                      1073741824L,           /* 30 */
                      2147483648L,           /* 31 */
                      4294967296L,           /* 32 */
                      8589934592L,           /* 33 */
                      17179869184L,          /* 34 */
                      34359738368L,          /* 35 */
                      68719476736L,          /* 36 */
                      137438953472L,         /* 37 */
                      274877906944L,         /* 38 */
                      549755813888L,         /* 39 */
                      1099511627776L,        /* 40 */
                      2199023255552L,        /* 41 */
                      4398046511104L,        /* 42 */
                      8796093022208L,        /* 43 */
                      17592186044416L,       /* 44 */
                      35184372088832L,       /* 45 */
                      70368744177664L,       /* 46 */
                      140737488355328L,      /* 47 */
                      281474976710656L,      /* 48 */
                      562949953421312L,      /* 49 */
                      1125899906842624L,     /* 50 */
                      2251799813685248L,     /* 51 */
                      4503599627370496L,     /* 52 */
                      9007199254740992L,     /* 53 */
                      18014398509481984L,    /* 54 */
                      36028797018963968L,    /* 55 */
                      72057594037927936L,    /* 56 */
                      144115188075855872L,   /* 57 */
                      288230376151711744L,   /* 58 */
                      576460752303423488L    /* 59 */
                   };
/* 1152921504566846976 */
#endif /* _WORKING_LONG_LONG */

static unsigned int bit_array[31] =
                   {
                      1,                     /* 0 */
                      2,                     /* 1 */
                      4,                     /* 2 */
                      8,                     /* 3 */
                      16,                    /* 4 */
                      32,                    /* 5 */
                      64,                    /* 6 */
                      128,                   /* 7 */
                      256,                   /* 8 */
                      512,                   /* 9 */
                      1024,                  /* 10 */
                      2048,                  /* 11 */
                      4096,                  /* 12 */
                      8192,                  /* 13 */
                      16384,                 /* 14 */
                      32768,                 /* 15 */
                      65536,                 /* 16 */
                      131072,                /* 17 */
                      262144,                /* 18 */
                      524288,                /* 19 */
                      1048576,               /* 20 */
                      2097152,               /* 21 */
                      4194304,               /* 22 */
                      8388608,               /* 23 */
                      16777216,              /* 24 */
                      33554432,              /* 25 */
                      67108864,              /* 26 */
                      134217728,             /* 27 */
                      268435456,             /* 28 */
                      536870912,             /* 29 */
                      1073741824             /* 30 */
                   };

#endif /* __bit_array_h */

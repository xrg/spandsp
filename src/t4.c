/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4.c - ITU T.4 FAX image processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: t4.c,v 1.28 2005/03/20 04:07:17 steveu Exp $
 */

/*
 * Much of this file is based on the T.4 and T.6 support in libtiff, which requires
 * the following notice in any derived source code:
 *
 * Copyright (c) 1990-1997 Sam Leffler
 * Copyright (c) 1991-1997 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 *
 * Decoder support is derived from code in Frank Cringle's viewfax program;
 *      Copyright (C) 1990, 1995  Frank D. Cringle.
 */

/*! \file */

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>

#include <tiffio.h>

#include "spandsp.h"

/* Finite state machine state codes */
#define S_Null                  0
#define S_Pass                  1
#define S_Horiz                 2
#define S_V0                    3
#define S_VR                    4
#define S_VL                    5
#define S_Ext                   6
#define S_TermW                 7
#define S_TermB                 8
#define S_MakeUpW               9
#define S_MakeUpB               10
#define S_MakeUp                11
#define S_EOL                   12

#include "faxfont.h"

#if 1
#define STATE_TRACE(x,...) /**/
#else
void STATE_TRACE(char *format, ...)
{
    va_list arg_ptr;

    va_start (arg_ptr, format);
    vprintf (format, arg_ptr);
    va_end (arg_ptr);
}
/*- End of function --------------------------------------------------------*/
#endif

/* Finite state machine state table entry */
typedef struct
{
    uint8_t     state;        /* see above */
    uint8_t     width;        /* width of code in bits */
    uint32_t    param;        /* run length in bits */
} T4_tab_entry;

#include "t4states.h"

/* From the T.4 spec:

    a0	The reference or starting changing element on the coding line. At
        the start of the coding line, a0 is set on an imaginary white
        changing element situated just before the first element on the
        line. During the coding of the coding line, the position of a0
        is defined by the previous coding mode. (See 4.2.1.3.2.)
    a1	The next changing element to the right of a0 on the coding line.
    a2	The next changing element to the right of a1 on the coding line.
    b1	The first changing element on the reference line to the right of
        a0 and of opposite colour to a0.
    b2	The next changing element to the right of b1 on the reference line.
*/

/*
 * ITU T.4 1D Huffman run_length codes and
 * related definitions.  Given the small sizes
 * of these tables it does not seem
 * worthwhile to make code & length 8 bits.
 */
typedef struct
{
    unsigned short int length;      /* length of T.4 code, in bits */
    unsigned short int code;        /* T.4 code */
    short int          runlen;      /* run length, in bits */
} T4_table_entry;

#define isAligned(p,t)  ((((unsigned long int)(p)) & (sizeof(t) - 1)) == 0)

#define        EOL        0x001        /* EOL code value - 0000 0000 0000 1 */

/* Status values returned instead of a run length */
#define        T4CODE_EOL        -1        /* NB: ACT_EOL - ACT_WRUNT */
#define        T4CODE_INVALID    -2        /* NB: ACT_INVALID - ACT_WRUNT */
#define        T4CODE_EOF        -3        /* end of input data */
#define        T4CODE_INCOMP     -4        /* incomplete run code */

/*
 * Note that these tables are ordered such that the
 * index into the table is known to be either the
 * run length, or (run length / 64) + a fixed offset.
 *
 * NB: The T4CODE_INVALID entries are only used
 *     during state generation (see mkg3states.c).
 */
const T4_table_entry t4_white_codes[] =
{
    {  8, 0x35,    0 },        /* 0011 0101 */
    {  6, 0x07,    1 },        /* 0001 11 */
    {  4, 0x07,    2 },        /* 0111 */
    {  4, 0x08,    3 },        /* 1000 */
    {  4, 0x0B,    4 },        /* 1011 */
    {  4, 0x0C,    5 },        /* 1100 */
    {  4, 0x0E,    6 },        /* 1110 */
    {  4, 0x0F,    7 },        /* 1111 */
    {  5, 0x13,    8 },        /* 1001 1 */
    {  5, 0x14,    9 },        /* 1010 0 */
    {  5, 0x07,   10 },        /* 0011 1 */
    {  5, 0x08,   11 },        /* 0100 0 */
    {  6, 0x08,   12 },        /* 0010 00 */
    {  6, 0x03,   13 },        /* 0000 11 */
    {  6, 0x34,   14 },        /* 1101 00 */
    {  6, 0x35,   15 },        /* 1101 01 */
    {  6, 0x2A,   16 },        /* 1010 10 */
    {  6, 0x2B,   17 },        /* 1010 11 */
    {  7, 0x27,   18 },        /* 0100 111 */
    {  7, 0x0C,   19 },        /* 0001 100 */
    {  7, 0x08,   20 },        /* 0001 000 */
    {  7, 0x17,   21 },        /* 0010 111 */
    {  7, 0x03,   22 },        /* 0000 011 */
    {  7, 0x04,   23 },        /* 0000 100 */
    {  7, 0x28,   24 },        /* 0101 000 */
    {  7, 0x2B,   25 },        /* 0101 011 */
    {  7, 0x13,   26 },        /* 0010 011 */
    {  7, 0x24,   27 },        /* 0100 100 */
    {  7, 0x18,   28 },        /* 0011 000 */
    {  8, 0x02,   29 },        /* 0000 0010 */
    {  8, 0x03,   30 },        /* 0000 0011 */
    {  8, 0x1A,   31 },        /* 0001 1010 */
    {  8, 0x1B,   32 },        /* 0001 1011 */
    {  8, 0x12,   33 },        /* 0001 0010 */
    {  8, 0x13,   34 },        /* 0001 0011 */
    {  8, 0x14,   35 },        /* 0001 0100 */
    {  8, 0x15,   36 },        /* 0001 0101 */
    {  8, 0x16,   37 },        /* 0001 0110 */
    {  8, 0x17,   38 },        /* 0001 0111 */
    {  8, 0x28,   39 },        /* 0010 1000 */
    {  8, 0x29,   40 },        /* 0010 1001 */
    {  8, 0x2A,   41 },        /* 0010 1010 */
    {  8, 0x2B,   42 },        /* 0010 1011 */
    {  8, 0x2C,   43 },        /* 0010 1100 */
    {  8, 0x2D,   44 },        /* 0010 1101 */
    {  8, 0x04,   45 },        /* 0000 0100 */
    {  8, 0x05,   46 },        /* 0000 0101 */
    {  8, 0x0A,   47 },        /* 0000 1010 */
    {  8, 0x0B,   48 },        /* 0000 1011 */
    {  8, 0x52,   49 },        /* 0101 0010 */
    {  8, 0x53,   50 },        /* 0101 0011 */
    {  8, 0x54,   51 },        /* 0101 0100 */
    {  8, 0x55,   52 },        /* 0101 0101 */
    {  8, 0x24,   53 },        /* 0010 0100 */
    {  8, 0x25,   54 },        /* 0010 0101 */
    {  8, 0x58,   55 },        /* 0101 1000 */
    {  8, 0x59,   56 },        /* 0101 1001 */
    {  8, 0x5A,   57 },        /* 0101 1010 */
    {  8, 0x5B,   58 },        /* 0101 1011 */
    {  8, 0x4A,   59 },        /* 0100 1010 */
    {  8, 0x4B,   60 },        /* 0100 1011 */
    {  8, 0x32,   61 },        /* 0011 0010 */
    {  8, 0x33,   62 },        /* 0011 0011 */
    {  8, 0x34,   63 },        /* 0011 0100 */
    {  5, 0x1B,   64 },        /* 1101 1 */
    {  5, 0x12,  128 },        /* 1001 0 */
    {  6, 0x17,  192 },        /* 0101 11 */
    {  7, 0x37,  256 },        /* 0110 111 */
    {  8, 0x36,  320 },        /* 0011 0110 */
    {  8, 0x37,  384 },        /* 0011 0111 */
    {  8, 0x64,  448 },        /* 0110 0100 */
    {  8, 0x65,  512 },        /* 0110 0101 */
    {  8, 0x68,  576 },        /* 0110 1000 */
    {  8, 0x67,  640 },        /* 0110 0111 */
    {  9, 0xCC,  704 },        /* 0110 0110 0 */
    {  9, 0xCD,  768 },        /* 0110 0110 1 */
    {  9, 0xD2,  832 },        /* 0110 1001 0 */
    {  9, 0xD3,  896 },        /* 0110 1001 1 */
    {  9, 0xD4,  960 },        /* 0110 1010 0 */
    {  9, 0xD5, 1024 },        /* 0110 1010 1 */
    {  9, 0xD6, 1088 },        /* 0110 1011 0 */
    {  9, 0xD7, 1152 },        /* 0110 1011 1 */
    {  9, 0xD8, 1216 },        /* 0110 1100 0 */
    {  9, 0xD9, 1280 },        /* 0110 1100 1 */
    {  9, 0xDA, 1344 },        /* 0110 1101 0 */
    {  9, 0xDB, 1408 },        /* 0110 1101 1 */
    {  9, 0x98, 1472 },        /* 0100 1100 0 */
    {  9, 0x99, 1536 },        /* 0100 1100 1 */
    {  9, 0x9A, 1600 },        /* 0100 1101 0 */
    {  6, 0x18, 1664 },        /* 0110 00 */
    {  9, 0x9B, 1728 },        /* 0100 1101 1 */
    { 11, 0x08, 1792 },        /* 0000 0001 000 */
    { 11, 0x0C, 1856 },        /* 0000 0001 100 */
    { 11, 0x0D, 1920 },        /* 0000 0001 101 */
    { 12, 0x12, 1984 },        /* 0000 0001 0010 */
    { 12, 0x13, 2048 },        /* 0000 0001 0011 */
    { 12, 0x14, 2112 },        /* 0000 0001 0100 */
    { 12, 0x15, 2176 },        /* 0000 0001 0101 */
    { 12, 0x16, 2240 },        /* 0000 0001 0110 */
    { 12, 0x17, 2304 },        /* 0000 0001 0111 */
    { 12, 0x1C, 2368 },        /* 0000 0001 1100 */
    { 12, 0x1D, 2432 },        /* 0000 0001 1101 */
    { 12, 0x1E, 2496 },        /* 0000 0001 1110 */
    { 12, 0x1F, 2560 },        /* 0000 0001 1111 */
    { 12, 0x01, T4CODE_EOL },           /* 0000 0000 0001 */
    {  9, 0x01, T4CODE_INVALID },       /* 0000 0000 1 */
    { 10, 0x01, T4CODE_INVALID },       /* 0000 0000 01 */
    { 11, 0x01, T4CODE_INVALID },       /* 0000 0000 001 */
    { 12, 0x00, T4CODE_INVALID },       /* 0000 0000 0000 */
};

const T4_table_entry t4_black_codes[] =
{
    { 10, 0x37,    0 },        /* 0000 1101 11 */
    {  3, 0x02,    1 },        /* 010 */
    {  2, 0x03,    2 },        /* 11 */
    {  2, 0x02,    3 },        /* 10 */
    {  3, 0x03,    4 },        /* 011 */
    {  4, 0x03,    5 },        /* 0011 */
    {  4, 0x02,    6 },        /* 0010 */
    {  5, 0x03,    7 },        /* 0001 1 */
    {  6, 0x05,    8 },        /* 0001 01 */
    {  6, 0x04,    9 },        /* 0001 00 */
    {  7, 0x04,   10 },        /* 0000 100 */
    {  7, 0x05,   11 },        /* 0000 101 */
    {  7, 0x07,   12 },        /* 0000 111 */
    {  8, 0x04,   13 },        /* 0000 0100 */
    {  8, 0x07,   14 },        /* 0000 0111 */
    {  9, 0x18,   15 },        /* 0000 1100 0 */
    { 10, 0x17,   16 },        /* 0000 0101 11 */
    { 10, 0x18,   17 },        /* 0000 0110 00 */
    { 10, 0x08,   18 },        /* 0000 0010 00 */
    { 11, 0x67,   19 },        /* 0000 1100 111 */
    { 11, 0x68,   20 },        /* 0000 1101 000 */
    { 11, 0x6C,   21 },        /* 0000 1101 100 */
    { 11, 0x37,   22 },        /* 0000 0110 111 */
    { 11, 0x28,   23 },        /* 0000 0101 000 */
    { 11, 0x17,   24 },        /* 0000 0010 111 */
    { 11, 0x18,   25 },        /* 0000 0011 000 */
    { 12, 0xCA,   26 },        /* 0000 1100 1010 */
    { 12, 0xCB,   27 },        /* 0000 1100 1011 */
    { 12, 0xCC,   28 },        /* 0000 1100 1100 */
    { 12, 0xCD,   29 },        /* 0000 1100 1101 */
    { 12, 0x68,   30 },        /* 0000 0110 1000 */
    { 12, 0x69,   31 },        /* 0000 0110 1001 */
    { 12, 0x6A,   32 },        /* 0000 0110 1010 */
    { 12, 0x6B,   33 },        /* 0000 0110 1011 */
    { 12, 0xD2,   34 },        /* 0000 1101 0010 */
    { 12, 0xD3,   35 },        /* 0000 1101 0011 */
    { 12, 0xD4,   36 },        /* 0000 1101 0100 */
    { 12, 0xD5,   37 },        /* 0000 1101 0101 */
    { 12, 0xD6,   38 },        /* 0000 1101 0110 */
    { 12, 0xD7,   39 },        /* 0000 1101 0111 */
    { 12, 0x6C,   40 },        /* 0000 0110 1100 */
    { 12, 0x6D,   41 },        /* 0000 0110 1101 */
    { 12, 0xDA,   42 },        /* 0000 1101 1010 */
    { 12, 0xDB,   43 },        /* 0000 1101 1011 */
    { 12, 0x54,   44 },        /* 0000 0101 0100 */
    { 12, 0x55,   45 },        /* 0000 0101 0101 */
    { 12, 0x56,   46 },        /* 0000 0101 0110 */
    { 12, 0x57,   47 },        /* 0000 0101 0111 */
    { 12, 0x64,   48 },        /* 0000 0110 0100 */
    { 12, 0x65,   49 },        /* 0000 0110 0101 */
    { 12, 0x52,   50 },        /* 0000 0101 0010 */
    { 12, 0x53,   51 },        /* 0000 0101 0011 */
    { 12, 0x24,   52 },        /* 0000 0010 0100 */
    { 12, 0x37,   53 },        /* 0000 0011 0111 */
    { 12, 0x38,   54 },        /* 0000 0011 1000 */
    { 12, 0x27,   55 },        /* 0000 0010 0111 */
    { 12, 0x28,   56 },        /* 0000 0010 1000 */
    { 12, 0x58,   57 },        /* 0000 0101 1000 */
    { 12, 0x59,   58 },        /* 0000 0101 1001 */
    { 12, 0x2B,   59 },        /* 0000 0010 1011 */
    { 12, 0x2C,   60 },        /* 0000 0010 1100 */
    { 12, 0x5A,   61 },        /* 0000 0101 1010 */
    { 12, 0x66,   62 },        /* 0000 0110 0110 */
    { 12, 0x67,   63 },        /* 0000 0110 0111 */
    { 10, 0x0F,   64 },        /* 0000 0011 11 */
    { 12, 0xC8,  128 },        /* 0000 1100 1000 */
    { 12, 0xC9,  192 },        /* 0000 1100 1001 */
    { 12, 0x5B,  256 },        /* 0000 0101 1011 */
    { 12, 0x33,  320 },        /* 0000 0011 0011 */
    { 12, 0x34,  384 },        /* 0000 0011 0100 */
    { 12, 0x35,  448 },        /* 0000 0011 0101 */
    { 13, 0x6C,  512 },        /* 0000 0011 0110 0 */
    { 13, 0x6D,  576 },        /* 0000 0011 0110 1 */
    { 13, 0x4A,  640 },        /* 0000 0010 0101 0 */
    { 13, 0x4B,  704 },        /* 0000 0010 0101 1 */
    { 13, 0x4C,  768 },        /* 0000 0010 0110 0 */
    { 13, 0x4D,  832 },        /* 0000 0010 0110 1 */
    { 13, 0x72,  896 },        /* 0000 0011 1001 0 */
    { 13, 0x73,  960 },        /* 0000 0011 1001 1 */
    { 13, 0x74, 1024 },        /* 0000 0011 1010 0 */
    { 13, 0x75, 1088 },        /* 0000 0011 1010 1 */
    { 13, 0x76, 1152 },        /* 0000 0011 1011 0 */
    { 13, 0x77, 1216 },        /* 0000 0011 1011 1 */
    { 13, 0x52, 1280 },        /* 0000 0010 1001 0 */
    { 13, 0x53, 1344 },        /* 0000 0010 1001 1 */
    { 13, 0x54, 1408 },        /* 0000 0010 1010 0 */
    { 13, 0x55, 1472 },        /* 0000 0010 1010 1 */
    { 13, 0x5A, 1536 },        /* 0000 0010 1101 0 */
    { 13, 0x5B, 1600 },        /* 0000 0010 1101 1 */
    { 13, 0x64, 1664 },        /* 0000 0011 0010 0 */
    { 13, 0x65, 1728 },        /* 0000 0011 0010 1 */
    { 11, 0x08, 1792 },        /* 0000 0001 000 */
    { 11, 0x0C, 1856 },        /* 0000 0001 100 */
    { 11, 0x0D, 1920 },        /* 0000 0001 101 */
    { 12, 0x12, 1984 },        /* 0000 0001 0010 */
    { 12, 0x13, 2048 },        /* 0000 0001 0011 */
    { 12, 0x14, 2112 },        /* 0000 0001 0100 */
    { 12, 0x15, 2176 },        /* 0000 0001 0101 */
    { 12, 0x16, 2240 },        /* 0000 0001 0110 */
    { 12, 0x17, 2304 },        /* 0000 0001 0111 */
    { 12, 0x1C, 2368 },        /* 0000 0001 1100 */
    { 12, 0x1D, 2432 },        /* 0000 0001 1101 */
    { 12, 0x1E, 2496 },        /* 0000 0001 1110 */
    { 12, 0x1F, 2560 },        /* 0000 0001 1111 */
    { 12, 0x01, T4CODE_EOL },            /* 0000 0000 0001 */
    {  9, 0x01, T4CODE_INVALID },        /* 0000 0000 1 */
    { 10, 0x01, T4CODE_INVALID },        /* 0000 0000 01 */
    { 11, 0x01, T4CODE_INVALID },        /* 0000 0000 001 */
    { 12, 0x00, T4CODE_INVALID },        /* 0000 0000 0000 */
};

static int __inline__ flush_bits_to_image_buffer(t4_state_t *s)
{
    uint8_t *t;

    s->bit = 8;
    if (s->image_size >= s->image_buffer_size)
    {
        t = realloc(s->image_buffer, s->image_buffer_size + 10000);
        if (t == NULL)
            return -1;
        s->image_buffer_size += 10000;
        s->image_buffer = t;
    }
    s->image_buffer[s->image_size++] = s->data;
    s->data = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_run(t4_state_t *s, int black)
{
    int i;
    uint8_t *t;

    s->row_len += s->run_length;
    /* Ignore anything before the first EOL */
    /* Don't allow rows to grow too long, and overflow the buffers */
    if (s->row_len <= s->image_width  &&  s->first_eol_seen)
    {
        *s->pa++ = s->run_length;
        for (i = 0;  i < s->run_length;  i++)
        {
            s->data = (s->data << 1) | black;
            if (--s->bit == 0)
                flush_bits_to_image_buffer(s);
        }
    }
    s->run_length = 0;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_eol(t4_state_t *s)
{
    uint32_t *x;
    uint8_t *t;
    
    int total;    

    if (s->run_length)
        put_run(s, 0);
    if (s->row_len != s->image_width)
    {
        STATE_TRACE("%d Bad row - %d %d\n", s->rows, s->row_len, s->row_is_2d);
        /* Clean up the bad runs */
        while (s->a0 > s->image_width  &&  s->pa > s->curruns)
            s->a0 -= *--s->pa;
        if (s->a0 < s->image_width)
        {
            if (s->a0 < 0)
                s->a0 = 0;
            if ((s->pa - s->curruns) & 1)
                put_run(s, 0);
            s->run_length = s->image_width - s->a0;
            put_run(s, 0);
        }
        else if (s->a0 > s->image_width)
        {
            s->run_length = s->image_width;
            put_run(s, 0);
            s->run_length = 0;
            put_run(s, 0);
        }
        /* Copy the previous row over this one */
        if (s->row_starts_at + s->bytes_per_row >= s->image_buffer_size)
        {
            t = realloc(s->image_buffer, s->image_buffer_size + 10000);
            if (t == NULL)
            {
                /* TODO: take some action to report the allocation failure */
                return;
            }
            s->image_buffer_size += 10000;
            s->image_buffer = t;
        }
        memcpy(s->image_buffer + s->row_starts_at, s->image_buffer + s->last_row_starts_at, s->bytes_per_row);
        s->image_size = s->row_starts_at + s->bytes_per_row;
        s->bad_rows++;
        s->curr_bad_row_run++;
    }
    else
    {
        if (s->curr_bad_row_run)
        {
            if (s->curr_bad_row_run > s->longest_bad_row_run)
                s->longest_bad_row_run = s->curr_bad_row_run;
            s->curr_bad_row_run = 0;
        }
        STATE_TRACE("%d Good row - %d %d\n", s->rows, s->row_len, s->row_is_2d);
    }
    
#if 0
    /* Dump the runs of black and white for analysis */
    printf("Ref ");
    total = 0;
    for (x = s->refruns;  x < s->pb;  x++)
    {
        total += *x;
        printf("%d ", *x);
    }
    printf(" total = %d\n", total);
    printf("Cur ");
    total = 0;
    for (x = s->curruns;  x < s->pa;  x++)
    {
        total += *x;
        printf("%d ", *x);
    }
    printf("total = %d\n", total);
#endif

    /* Prepare the buffers for the next row. */
    s->rows++;
    s->last_row_starts_at = s->row_starts_at;
    s->row_starts_at = s->image_size;
    x = s->curruns;
    s->curruns = s->refruns;
    s->refruns = x;

    s->pa = s->curruns;
    s->pb = s->refruns;
    
    s->a0 = 0;
    s->b1 = *s->pb++;
}
/*- End of function --------------------------------------------------------*/

int t4_rx_end_page(t4_state_t *s)
{
    int ok;
    int row;
    time_t now;
    struct tm *tm;
    char buf[256 + 1];

    if (s->curr_bad_row_run)
    {
        if (s->curr_bad_row_run > s->longest_bad_row_run)
            s->longest_bad_row_run = s->curr_bad_row_run;
        s->curr_bad_row_run = 0;
    }

    if (s->image_size == 0)
        return -1;

    for (row = 0;  row < s->rows;  row++)
    {
        if (TIFFWriteScanline(s->tiff_file, s->image_buffer + row*s->bytes_per_row, row, 0) < 0)
        {
            fprintf(stderr, "%s: Write error at row %d.\n", s->file, row);
            break;
        }
    }

    time(&now);
    tm = localtime(&now);
    sprintf(buf,
    	    "%4d/%02d/%02d %02d:%02d:%02d",
            tm->tm_year + 1900,
            tm->tm_mon + 1,
            tm->tm_mday,
            tm->tm_hour,
            tm->tm_min,
            tm->tm_sec);
    TIFFSetField(s->tiff_file, TIFFTAG_DATETIME, buf);
    TIFFSetField(s->tiff_file, TIFFTAG_FAXRECVTIME, now - s->page_start_time);

    TIFFSetField(s->tiff_file, TIFFTAG_IMAGELENGTH, s->rows);
    /* Set the total pages to 1. For any one page document we will get this
       right. For multi-page documents we will need to come back and fill in
       the right answer when we know it. */
    TIFFSetField(s->tiff_file, TIFFTAG_PAGENUMBER, s->pages_transferred++, 1);
    if (s->output_compression == COMPRESSION_CCITT_T4)
    {
        if (s->bad_rows)
        {
            TIFFSetField(s->tiff_file, TIFFTAG_BADFAXLINES, s->bad_rows);
            TIFFSetField(s->tiff_file, TIFFTAG_CLEANFAXDATA, CLEANFAXDATA_REGENERATED);
            TIFFSetField(s->tiff_file, TIFFTAG_CONSECUTIVEBADFAXLINES, s->longest_bad_row_run);
        }
        else
        {
            TIFFSetField(s->tiff_file, TIFFTAG_CLEANFAXDATA, CLEANFAXDATA_CLEAN);
        }
    }
    TIFFWriteDirectory(s->tiff_file);

    s->bits = 0;
    s->bits_to_date = 0;
    s->consecutive_eols = 0;

    s->image_size = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

int t4_rx_putbit(t4_state_t *s, int bit)
{
    int seglen;
    int bits;

    /* We decompress bit by bit, as the data stream is received. We need to
       scan continuously for EOLs, so we might as well work this way. */
    s->bits_to_date = (s->bits_to_date >> 1) | ((bit & 1) << 12);
    if (++s->bits > 13)
        s->bits = 13;
    if (s->bits < 13)
        return FALSE;
    if (s->row_is_2d  &&  s->black_white == 0)
    {
        switch (T4_black_table[s->bits_to_date & 0x1FFF].state)
        {
        case S_EOL:
            STATE_TRACE("EOL\n");
            put_eol(s);
            s->row_is_2d = !(s->bits_to_date & 0x1000);
            s->bits -= T4_black_table[s->bits_to_date & 0x1FFF].width;
            s->bits--;
            s->itsblack = FALSE;
            s->row_len = 0;
            s->first_eol_seen = TRUE;
            break;
        default:
            bits = s->bits_to_date & 0x7F;
            STATE_TRACE("State %d, %d\n",
                        T4_common_table[bits].state,
                        T4_common_table[bits].width);
            switch (T4_common_table[bits].state)
            {
            case S_Pass:
                STATE_TRACE("Pass\n");
                if (s->row_len < s->image_width)
                {
                    if (s->pa != s->curruns)
                    {
                        while (s->b1 <= s->a0  &&  s->b1 < s->image_width)
                        {
                            s->b1 += s->pb[0] + s->pb[1];
                            s->pb += 2;
                        }
                    }
                    s->b1 += *s->pb++;
                    s->run_length += (s->b1 - s->a0);
                    s->a0 = s->b1;
                    s->b1 += *s->pb++;
                }
                break;
            case S_Horiz:
                STATE_TRACE("Horiz\n");
                s->itsblack = (s->pa - s->curruns) & 1;
                s->black_white = 2;
                break;
            case S_V0:
                STATE_TRACE("V0 %d %d %d %d\n",
                            s->a0,
                            s->b1,
                            s->image_width,
                            s->run_length);
                if (s->row_len < s->image_width)
                {
                    if (s->pa != s->curruns)
                    {
                        while (s->b1 <= s->a0  &&  s->b1 < s->image_width)
                        {
                            s->b1 += s->pb[0] + s->pb[1];
                            s->pb += 2;
                        }
                    }
                    s->run_length += (s->b1 - s->a0);
                    s->a0 = s->b1;
                    put_run(s, ((s->pa - s->curruns) & 1));
                    s->b1 += *s->pb++;
                }
                break;
            case S_VR:
                STATE_TRACE("VR[%d] %d %d %d %d\n",
                            T4_common_table[bits].param,
                            s->a0,
                            s->b1,
                            s->image_width,
                            s->run_length);
                if (s->row_len < s->image_width)
                {
                    if (s->pa != s->curruns)
                    {
                        while (s->b1 <= s->a0  &&  s->b1 < s->image_width)
                        {
                            s->b1 += s->pb[0] + s->pb[1];
                            s->pb += 2;
                        }
                    }
                    s->run_length += (s->b1 + T4_common_table[bits].param - s->a0);
                    s->a0 = s->b1 + T4_common_table[bits].param;
                    put_run(s, ((s->pa - s->curruns) & 1));
                    s->b1 += *s->pb++;
                }
                break;
            case S_VL:
                STATE_TRACE("VL[%d] %d %d %d %d\n",
                            T4_common_table[bits].param,
                            s->a0,
                            s->b1,
                            s->image_width,
                            s->run_length);
                if (s->row_len < s->image_width)
                {
                    if (s->pa != s->curruns)
                    {
                        while (s->b1 <= s->a0  &&  s->b1 < s->image_width)
                        {
                            s->b1 += s->pb[0] + s->pb[1];
                            s->pb += 2;
                        }
                    }
                    s->run_length += (s->b1 - T4_common_table[bits].param - s->a0);
                    s->a0 = s->b1 - T4_common_table[bits].param;
                    put_run(s, ((s->pa - s->curruns) & 1));
                    s->b1 -= *--s->pb;
                }
                break;
            case S_Ext:
                STATE_TRACE("Ext %d 0x%x\n",
                            ((s->bits_to_date >> T4_common_table[bits].width) & 0x7),
                            s->bits_to_date);
                if (s->row_len < s->image_width)
                {
                    *s->pa++ = s->image_width - s->a0;
                    //extension(s->a0);
                }
                break;
            case S_Null:
                break;
            default:
                fprintf(stderr, "Unexpected T.4 state %d\n", T4_common_table[bits].state);
                break;
            }
            s->bits -= T4_common_table[bits].width;
            break;
        }
    }
    else
    {
        if (s->itsblack)
        {
            bits = s->bits_to_date & 0x1FFF;
            STATE_TRACE("Black state %d %d\n", T4_black_table[bits].state, T4_black_table[bits].param);
            switch (T4_black_table[bits].state)
            {
            case S_MakeUpB:
            case S_MakeUp:
                if (s->row_len < s->image_width)
                {
                    s->run_length += T4_black_table[bits].param;
                    s->a0 += T4_black_table[bits].param;
                }
                break;
            case S_TermB:
                if (s->row_len < s->image_width)
                {
                    s->run_length += T4_black_table[bits].param;
                    s->a0 += T4_black_table[bits].param;
                    put_run(s, 1);
                    if (s->black_white)
                    {
                        if (s->black_white == 1)
                        {
                            if (s->pa != s->curruns)
                            {
                                while (s->b1 <= s->a0  &&  s->b1 < s->image_width)
                                {
                                    s->b1 += s->pb[0] + s->pb[1];
                                    s->pb += 2;
                                }
                            }
                        }
                        s->black_white--;
                    }
                }
                s->itsblack = FALSE;
                break;
            case S_EOL:
                STATE_TRACE("EOL\n");
                if (s->first_eol_seen)
                {
                    if (s->row_len == 0)
                    {
                        if (++s->consecutive_eols >= 5)
                        {
                            t4_rx_end_page(s);
                            return TRUE;
                        }
                    }
                    else
                    {
                        s->consecutive_eols = 0;
                        put_eol(s);
                    }
                }
                if (s->line_encoding == T4_COMPRESSION_ITU_T4_2D)
                {
                    s->row_is_2d = !(s->bits_to_date & 0x1000);
                    s->bits--;
                }
                s->itsblack = FALSE;
                s->row_len = 0;
                s->first_eol_seen = TRUE;
                break;
            default:
                /* Bad black */
                s->black_white = 0;
                break;
            }
            s->bits -= T4_black_table[bits].width;
        }
        else
        {
            bits = s->bits_to_date & 0xFFF;
            STATE_TRACE("White state %d %d\n", T4_white_table[bits].state, T4_white_table[bits].param);
            switch (T4_white_table[bits].state)
            {
            case S_MakeUpW:
            case S_MakeUp:
                if (s->row_len < s->image_width)
                {
                    s->run_length += T4_white_table[bits].param;
                    s->a0 += T4_white_table[bits].param;
                }
                break;
            case S_TermW:
                if (s->row_len < s->image_width)
                {
                    s->run_length += T4_white_table[bits].param;
                    s->a0 += T4_white_table[bits].param;
                    put_run(s, 0);
                    if (s->black_white)
                    {
                        if (s->black_white == 1)
                        {
                            if (s->pa != s->curruns)
                            {
                                while (s->b1 <= s->a0  &&  s->b1 < s->image_width)
                                {
                                    s->b1 += s->pb[0] + s->pb[1];
                                    s->pb += 2;
                                }
                            }
                        }
                        s->black_white--;
                    }
                }
                s->itsblack = TRUE;
                break;
            case S_EOL:
                STATE_TRACE("EOL\n");
                if (s->first_eol_seen)
                {
                    if (s->row_len == 0)
                    {
                        if (++s->consecutive_eols >= 5)
                        {
                            t4_rx_end_page(s);
                            return TRUE;
                        }
                    }
                    else
                    {
                        s->consecutive_eols = 0;
                        put_eol(s);
                    }
                }
                if (s->line_encoding == T4_COMPRESSION_ITU_T4_2D)
                {
                    s->row_is_2d = !(s->bits_to_date & 0x1000);
                    s->bits--;
                }
                s->itsblack = FALSE;
                s->row_len = 0;
                s->first_eol_seen = TRUE;
                break;
            default:
                /* Bad white */
                s->black_white = 0;
                break;
            }
            s->bits -= T4_white_table[bits].width;
        }
    }
    return FALSE;
}
/*- End of function --------------------------------------------------------*/

t4_state_t *t4_rx_create(const char *file, int output_encoding)
{
    t4_state_t *s;
    
    s = (t4_state_t *) malloc(sizeof(t4_state_t *));
    if (!s  ||  t4_rx_init(s, file, output_encoding))
        return NULL;
    return s;
}
/*- End of function --------------------------------------------------------*/

int t4_rx_init(t4_state_t *s, const char *file, int output_encoding)
{
    uint32_t nruns;

    fprintf(stderr, "Start rx document\n");

    memset(s, 0, sizeof(*s));

    s->tiff_file = TIFFOpen(file, "w");
    if (s->tiff_file == NULL)
        return -1;

    /* Save the file name for logging reports. */
    s->file = file;
    /* Only provide for one form of coding throughout the file, even though the
       coding on the wire could change between pages. */
    switch (output_encoding)
    {
    case T4_COMPRESSION_ITU_T4_1D:
        s->output_compression = COMPRESSION_CCITT_T4;
        s->output_t4_options = GROUP3OPT_FILLBITS | GROUP3OPT_2DENCODING;
        break;
    case T4_COMPRESSION_ITU_T4_2D:
        s->output_compression = COMPRESSION_CCITT_T4;
        s->output_t4_options = GROUP3OPT_FILLBITS | GROUP3OPT_2DENCODING | GROUP3OPT_2DENCODING;
        break;
    case T4_COMPRESSION_ITU_T6:
        s->output_compression = COMPRESSION_CCITT_T6;
        s->output_t4_options |= GROUP3OPT_2DENCODING;
        break;
    }

    /* Until we have a valid figure for the bytes per row, we need it to be set to a suitable
       value to ensure it will be seen as changing when the real value is used. */
    s->bytes_per_row = 0;

    s->pages_transferred = 0;
    /* Set some default values */
    s->column_resolution = 7700;
    s->row_resolution = T4_RESOLUTION_STANDARD;
    s->image_width = 1728;

    return 0;
}
/*- End of function --------------------------------------------------------*/

int t4_rx_start_page(t4_state_t *s)
{
    float res;
    uint16_t resunit;
    float x_resolution;
    float y_resolution;
    int bytes_per_row;
    int nruns;
    uint32_t *bufptr;
    char buf[256 + 1];

    fprintf(stderr, "Start rx page - compression %d\n", s->line_encoding);

    /* Calculate the scanline/tile width. */
    bytes_per_row = s->image_width/8;
    if (bytes_per_row != s->bytes_per_row)
    {
        /* Allocate the additional space required for decoding. */
        nruns = 2*((s->image_width + 31) & ~31);
        s->bytes_per_row = bytes_per_row;
        bufptr = (uint32_t *) realloc(s->curruns, (nruns + 3)*sizeof(uint32_t));
        if (bufptr == NULL)
            return -1;
        s->curruns = bufptr;
        bufptr = (uint32_t *) realloc(s->refruns, (nruns + 3)*sizeof(uint32_t));
        if (bufptr == NULL)
            return -1;
        s->refruns = bufptr;
    }
    TIFFSetField(s->tiff_file, TIFFTAG_COMPRESSION, s->output_compression);
    if (s->output_compression == COMPRESSION_CCITT_T4)
    {
        TIFFSetField(s->tiff_file, TIFFTAG_T4OPTIONS, s->output_t4_options);
        TIFFSetField(s->tiff_file, TIFFTAG_FAXMODE, FAXMODE_CLASSF);
    }
    TIFFSetField(s->tiff_file, TIFFTAG_IMAGEWIDTH, s->image_width);
    TIFFSetField(s->tiff_file, TIFFTAG_BITSPERSAMPLE, 1);
    TIFFSetField(s->tiff_file, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(s->tiff_file, TIFFTAG_SAMPLESPERPIXEL, 1);
    if (s->output_compression == COMPRESSION_CCITT_T4
        ||
        s->output_compression == COMPRESSION_CCITT_T6)
    {
        TIFFSetField(s->tiff_file, TIFFTAG_ROWSPERSTRIP, -1L);
    }
    else
    {
        TIFFSetField(s->tiff_file,
                     TIFFTAG_ROWSPERSTRIP,
                     TIFFDefaultStripSize(s->tiff_file, 0));
    }
    TIFFSetField(s->tiff_file, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(s->tiff_file, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
    TIFFSetField(s->tiff_file, TIFFTAG_FILLORDER, FILLORDER_LSB2MSB);

    x_resolution = s->column_resolution/100.0;
    y_resolution = s->row_resolution/100.0;
    TIFFSetField(s->tiff_file, TIFFTAG_XRESOLUTION, x_resolution);
    TIFFSetField(s->tiff_file, TIFFTAG_YRESOLUTION, y_resolution);
    resunit = RESUNIT_CENTIMETER;
    TIFFSetField(s->tiff_file, TIFFTAG_RESOLUTIONUNIT, resunit);

    /* TODO: add the version of spandsp */
    TIFFSetField(s->tiff_file, TIFFTAG_SOFTWARE, "spandsp");
    if (gethostname(buf, sizeof(buf)) == 0)
        TIFFSetField(s->tiff_file, TIFFTAG_HOSTCOMPUTER, buf);

    //TIFFSetField(s->tiff_file, TIFFTAG_FAXRECVPARAMS, ???);
    //TIFFSetField(s->tiff_file, TIFFTAG_FAXMODE, ???);
    if (s->sub_address)
        TIFFSetField(s->tiff_file, TIFFTAG_FAXSUBADDRESS, s->sub_address);
    if (s->far_ident)
        TIFFSetField(s->tiff_file, TIFFTAG_IMAGEDESCRIPTION, s->far_ident);
    if (s->vendor)
        TIFFSetField(s->tiff_file, TIFFTAG_MAKE, s->vendor);
    if (s->model)
        TIFFSetField(s->tiff_file, TIFFTAG_MODEL, s->model);

    s->bits = 0;
    s->bits_to_date = 0;

    s->row_is_2d = FALSE;
    s->itsblack = FALSE;
    s->bad_rows = 0;
    s->longest_bad_row_run = 0;
    s->curr_bad_row_run = 0;
    s->rows = 0;
    s->first_eol_seen = FALSE;
    s->consecutive_eols = 0;
    s->data = 0;
    s->bit = 8;
    s->image_buffer = NULL;
    s->image_buffer_size = 0;
    s->image_size = 0;
    s->row_starts_at = 0;
    s->last_row_starts_at = 0;
    
    s->pa = s->curruns;
    s->pb = s->refruns;

    /* Initialise the reference line to all white */
    s->refruns[0] = s->image_width;
    s->refruns[1] = 0;
    s->a0 = 0;
    s->b1 = s->image_width;
    s->run_length = 0;

    time (&s->page_start_time);

    return 0;
}
/*- End of function --------------------------------------------------------*/

int t4_rx_delete(t4_state_t *s)
{
    if (t4_rx_end(s))
        return -1;
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

int t4_rx_end(t4_state_t *s)
{
    int i;

    if (s->tiff_file)
    {
        if (s->pages_transferred > 1)
        {
            /* We need to edit the TIFF directories. Until now we did not know
               the total page count, so the TIFF file currently says one. Now we
               need to set the correct total page count associated with each page. */
            for (i = 0;  i < s->pages_transferred;  i++)
            {
                TIFFSetDirectory(s->tiff_file, i);
                TIFFSetField(s->tiff_file, TIFFTAG_PAGENUMBER, i, s->pages_transferred);
                TIFFWriteDirectory(s->tiff_file);
            }
        }
        TIFFClose(s->tiff_file);
        s->tiff_file = NULL;
        s->file = NULL;
    }
    if (s->image_buffer)
    {
        free(s->image_buffer);
        s->image_buffer = NULL;
        s->image_buffer_size = 0;
    }
    if (s->curruns)
    {
        free(s->curruns);
        s->curruns = NULL;
    }
    if (s->refruns)
    {
        free(s->refruns);
        s->refruns = NULL;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

void t4_rx_set_rx_encoding(t4_state_t *s, int encoding)
{
    s->line_encoding = encoding;
}
/*- End of function --------------------------------------------------------*/

void t4_rx_set_columns(t4_state_t *s, int columns)
{
    s->image_width = columns;
}
/*- End of function --------------------------------------------------------*/

void t4_rx_set_row_resolution(t4_state_t *s, int resolution)
{
    s->row_resolution = resolution;
}
/*- End of function --------------------------------------------------------*/

void t4_rx_set_column_resolution(t4_state_t *s, int resolution)
{
    s->column_resolution = resolution;
}
/*- End of function --------------------------------------------------------*/

void t4_rx_set_sub_address(t4_state_t *s, const char *sub_address)
{
    s->sub_address = (sub_address  &&  sub_address[0])  ?  sub_address  :  NULL;
}
/*- End of function --------------------------------------------------------*/

void t4_rx_set_far_ident(t4_state_t *s, const char *ident)
{
    s->far_ident = (ident  &&  ident[0])  ?  ident  :  NULL;
}
/*- End of function --------------------------------------------------------*/

void t4_rx_set_vendor(t4_state_t *s, const char *vendor)
{
    s->vendor = vendor;
}
/*- End of function --------------------------------------------------------*/

void t4_rx_set_model(t4_state_t *s, const char *model)
{
    s->model = model;
}
/*- End of function --------------------------------------------------------*/

static void __inline__ put_bits(t4_state_t *s, int bits, int length)
{
    static const int msbmask[9] =
    {
        0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff
    };

    s->row_bits += length;
    while (length > s->bit)
    {
        s->data |= (bits >> (length - s->bit));
        length -= s->bit;
        flush_bits_to_image_buffer(s);
    }
    s->data |= ((bits & msbmask[length]) << (s->bit - length));
    s->bit -= length;
    if (s->bit == 0)
        flush_bits_to_image_buffer(s);
}
/*- End of function --------------------------------------------------------*/

/*
 * Write the sequence of codes that describes
 * the specified span of zero's or one's.  The
 * appropriate table that holds the make-up and
 * terminating codes is supplied.
 */
static __inline__ void put_span(t4_state_t *s, int32_t span, const T4_table_entry *tab)
{
    const T4_table_entry *te;

    te = &tab[63 + (2560 >> 6)];
    while (span >= 2560 + 64)
    {
        put_bits(s, te->code, te->length);
        span -= te->runlen;
    }
    te = &tab[63 + (span >> 6)];
    if (span >= 64)
    {
        put_bits(s, te->code, te->length);
        span -= te->runlen;
    }
    put_bits(s, tab[span].code, tab[span].length);
}
/*- End of function --------------------------------------------------------*/

/*
 * Find a span of ones or zeros using the supplied
 * table.  The 'base' of the bit string is supplied
 * along with the start and end bit indices.
 */
static __inline__ int find0span(uint8_t *bp, int bs, int be)
{
    int bits;
    int n;
    int span;
    unsigned int *lp;
#if !defined(__i386__)  &&  !defined(__x86_64__)   
    static const uint8_t zeroruns[256] =
    {
        8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, /* 0x00 - 0x0f */
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* 0x10 - 0x1f */
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0x20 - 0x2f */
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0x30 - 0x3f */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x40 - 0x4f */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x50 - 0x5f */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x60 - 0x6f */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x70 - 0x7f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x80 - 0x8f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x90 - 0x9f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xa0 - 0xaf */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xb0 - 0xbf */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xc0 - 0xcf */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xd0 - 0xdf */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xe0 - 0xef */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xf0 - 0xff */
    };
#endif

    bits = be - bs;
    bp += bs >> 3;
    /* Check partial byte on LHS. */
    if (bits > 0  &&  (n = (bs & 7)))
    {
#if defined(__i386__)  ||  defined(__x86_64__)
        span = 7 - top_bit((*bp << n) & 0xFF);
#else
        span = zeroruns[(*bp << n) & 0xFF];
#endif
        if (span > 8 - n)       /* table value too generous */
            span = 8 - n;
        if (span > bits)        /* constrain span to bit range */
            span = bits;
        if (n + span < 8)       /* doesn't extend to edge of byte */
            return span;
        bits -= span;
        bp++;
    }
    else
    {
        span = 0;
    }
    if (bits >= 2*8*sizeof(unsigned int))
    {
        /* Align to natural integer boundary and check integers. */
        while (!isAligned(bp, unsigned int))
        {
            if (*bp != 0x00)
            {
#if defined(__i386__)  ||  defined(__x86_64__)
                span += (7 - top_bit(*bp));
#else
                span += zeroruns[*bp];
#endif
                return span;
            }
            span += 8;
            bits -= 8;
            bp++;
        }
        lp = (unsigned int *) bp;
        while (bits >= 8*sizeof(unsigned int)  &&  *lp == 0)
        {
            span += 8*sizeof(unsigned int);
            bits -= 8*sizeof(unsigned int);
            lp++;
        }
        bp = (uint8_t *) lp;
    }
    /* Scan full bytes for all 0's. */
    while (bits >= 8)
    {
        if (*bp != 0x00)
        {
#if defined(__i386__)  ||  defined(__x86_64__)
            span += (7 - top_bit(*bp));
#else
            span += zeroruns[*bp];
#endif
            return span;
        }
        span += 8;
        bits -= 8;
        bp++;
    }
    /* Check partial byte on RHS. */
    if (bits > 0)
    {
#if defined(__i386__)  ||  defined(__x86_64__)
        n = (7 - top_bit(*bp));
#else
        n = zeroruns[*bp];
#endif
        span += ((n > bits)  ?  bits  :  n);
    }
    return span;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int find1span(uint8_t *bp, int bs, int be)
{
    int bits;
    int n;
    int span;
    unsigned int *lp;
#if !defined(__i386__)  &&  !defined(__x86_64__)
    static const uint8_t oneruns[256] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x00 - 0x0f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x10 - 0x1f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x20 - 0x2f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x30 - 0x3f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x40 - 0x4f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x50 - 0x5f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x60 - 0x6f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x70 - 0x7f */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x80 - 0x8f */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x90 - 0x9f */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0xa0 - 0xaf */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0xb0 - 0xbf */
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0xc0 - 0xcf */
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0xd0 - 0xdf */
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* 0xe0 - 0xef */
        4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 8, /* 0xf0 - 0xff */
    };
#endif

    bits = be - bs;
    bp += bs >> 3;
    /* Check partial byte on LHS. */
    if (bits > 0  &&  (n = (bs & 7)))
    {
#if defined(__i386__)  ||  defined(__x86_64__)
        span = 7 - top_bit(((*bp << n) & 0xFF) ^ 0xFF);
#else
        span = oneruns[(*bp << n) & 0xFF];
#endif
        if (span > 8 - n)       /* table value too generous */
            span = 8 - n;
        if (span > bits)        /* constrain span to bit range */
            span = bits;
        if (n + span < 8)       /* doesn't extend to edge of byte */
            return span;
        bits -= span;
        bp++;
    }
    else
    {
        span = 0;
    }
    if (bits >= 2*8*sizeof(unsigned int))
    {
        /* Align to natural integer boundary and check integers. */
        while (!isAligned(bp, unsigned int))
        {
            if (*bp != 0xFF)
            {
#if defined(__i386__)  ||  defined(__x86_64__)
                span += (7 - top_bit(*bp ^ 0xFF));
#else
                span += oneruns[*bp];
#endif
                return span;
            }
            span += 8;
            bits -= 8;
            bp++;
        }
        lp = (unsigned int *) bp;
        while (bits >= 8*sizeof(unsigned int)  &&  *lp == ~0)
        {
            span += 8*sizeof(unsigned int);
            bits -= 8*sizeof(unsigned int);
            lp++;
        }
        bp = (uint8_t *) lp;
    }
    /* Scan full bytes for all 1's. */
    while (bits >= 8)
    {
        if (*bp != 0xFF)
        {
#if defined(__i386__)  ||  defined(__x86_64__)
            span += (7 - top_bit(*bp ^ 0xFF));
#else
            span += oneruns[*bp];
#endif
            return span;
        }
        span += 8;
        bits -= 8;
        bp++;
    }
    /* Check partial byte on RHS. */
    if (bits > 0)
    {
#if defined(__i386__)  ||  defined(__x86_64__)
        n = (7 - top_bit(*bp ^ 0xFF));
#else
        n = oneruns[*bp];
#endif
        span += ((n > bits)  ?  bits  :  n);
    }
    return span;
}
/*- End of function --------------------------------------------------------*/

/*
 * Write an EOL code to the output stream.  We also handle writing the tag
 * bit for the next scanline when doing 2D encoding.
 */
static void t4_encode_eol(t4_state_t *s)
{
    unsigned int code;
    int length;

    if (s->line_encoding == T4_COMPRESSION_ITU_T4_2D)
    {
        code = 0x0002 | (!s->row_is_2d);
        length = 13;
    }
    else
    {
        code = 0x001;
        length = 12;
    }
    /* We may need to pad the row to a minimum length. */
    if (s->row_bits + length < s->min_row_bits)
        put_bits(s, 0, s->min_row_bits - (s->row_bits + length));
    put_bits(s, code, length);
    s->row_bits = 0;
}
/*- End of function --------------------------------------------------------*/

/*
 * 2D-encode a row of pixels.  Consult ITU specification T.4 for the algorithm.
 */
static void t4_encode2Drow(t4_state_t *s, uint8_t *bp)
{
    int a0;
    int a1;
    int b1;
    int a2;
    int b2;
    int d;
    static const T4_table_entry codes[] =
    {
        { 7, 0x03 },        /* VR3          0000 011 */
        { 6, 0x03 },        /* VR2          0000 11 */
        { 3, 0x03 },        /* VR1          011 */
        { 1, 0x01 },        /* V0           1 */
        { 3, 0x02 },        /* VL1          010 */
        { 6, 0x02 },        /* VL2          0000 10 */
        { 7, 0x02 },        /* VL3          0000 010 */
        { 3, 0x01 },        /* horizontal   001 */
        { 4, 0x01 }         /* pass         0001 */
    };
    
    a0 = 0;
    a1 = (bp[0] & 0x80)  ?  0  :  find0span(bp, 0, s->image_width);
    b1 = (s->refrowbuf[0] & 0x80)  ?  0  :  find0span(s->refrowbuf, 0, s->image_width);
    for (;;)
    {
        b2 = (b1 < s->image_width)  ?  (b1 + (((s->refrowbuf[b1 >> 3] << (b1 & 7)) & 0x80)  ?  find1span(s->refrowbuf, b1, s->image_width)  :  find0span(s->refrowbuf, b1, s->image_width)))  :  s->image_width;
        if (b2 >= a1)
        {
            d = b1 - a1;
            if (-3 <= d  &&  d <= 3)
            {
                /* Vertical mode */
                put_bits(s, codes[d + 3].code, codes[d + 3].length);
                a0 = a1;
            }
            else
            {
                /* Horizontal mode */
                a2 = (a1 < s->image_width)  ?  (a1 + (((bp[a1 >> 3] << (a1 & 7)) & 0x80)  ?  find1span(bp, a1, s->image_width)  :  find0span(bp, a1, s->image_width)))  :  s->image_width;
                put_bits(s, codes[7].code, codes[7].length);
                if (a0 + a1 == 0  ||  ((bp[a0 >> 3] << (a0 & 7)) & 0x80) == 0)
                {
                    put_span(s, a1 - a0, t4_white_codes);
                    put_span(s, a2 - a1, t4_black_codes);
                }
                else
                {
                    put_span(s, a1 - a0, t4_black_codes);
                    put_span(s, a2 - a1, t4_white_codes);
                }
                a0 = a2;
            }
        }
        else
        {
            /* Pass mode */
            put_bits(s, codes[8].code, codes[8].length);
            a0 = b2;
        }
        if (a0 >= s->image_width)
            break;
        a1 = a0 + (((bp[a0 >> 3] << (a0 & 7)) & 0x80)  ?  find1span(bp, a0, s->image_width)  :  find0span(bp, a0, s->image_width));
        b1 = a0 + (((bp[a0 >> 3] << (a0 & 7)) & 0x80)  ?  find0span(s->refrowbuf, a0, s->image_width)  :  find1span(s->refrowbuf, a0, s->image_width));
        b1 = b1 + (((bp[a0 >> 3] << (a0 & 7)) & 0x80)  ?  find1span(s->refrowbuf, b1, s->image_width)  :  find0span(s->refrowbuf, b1, s->image_width));
    }
}
/*- End of function --------------------------------------------------------*/

/*
 * 1D-encode a row of pixels.  The encoding is
 * a sequence of all-white or all-black spans
 * of pixels encoded with Huffman codes.
 */
static void t4_encode1Drow(t4_state_t *s, uint8_t *bp)
{
    int span;
    int bs;

    bs = 0;
    for (;;)
    {
        span = find0span(bp, bs, s->image_width);                /* white span */
        put_span(s, span, t4_white_codes);
        bs += span;
        if (bs >= s->image_width)
            break;
        span = find1span(bp, bs, s->image_width);                /* black span */
        put_span(s, span, t4_black_codes);
        bs += span;
        if (bs >= s->image_width)
            break;
    }
}
/*- End of function --------------------------------------------------------*/

static int t4_encode_row(t4_state_t *s, uint8_t *bp)
{
    t4_encode_eol(s);
    if (s->line_encoding == T4_COMPRESSION_ITU_T6)
    {
        /* T.6 compression is a trivial step up from T.4 2D, so we just
           throw it in here. T.6 is only used with error correction,
           so it does not need idependantly compressed (i.e. 1D) lines
           to recover from data errors. */
        t4_encode2Drow(s, bp);
        memcpy(s->refrowbuf, bp, s->bytes_per_row);
    }
    else
    {
        if (s->line_encoding == T4_COMPRESSION_ITU_T4_2D)
        {
            if (s->row_is_2d)
            {
                t4_encode2Drow(s, bp);
                s->k--;
            }
            else
            {
                t4_encode1Drow(s, bp);
                s->row_is_2d = TRUE;
            }
            if (s->k == 0)
            {
                s->row_is_2d = FALSE;
                s->k = s->maxk - 1;
            }
            else
            {
                memcpy(s->refrowbuf, bp, s->bytes_per_row);
            }
        }
        else
        {
            t4_encode1Drow(s, bp);
        }
    }
    bp += s->bytes_per_row;
    s->row++;
    return 1;
}
/*- End of function --------------------------------------------------------*/

t4_state_t *t4_tx_create(const char *file)
{
    t4_state_t *s;
    
    s = (t4_state_t *) malloc(sizeof(t4_state_t *));
    if (!s  ||  t4_tx_init(s, file))
        return NULL;
    return s;
}
/*- End of function --------------------------------------------------------*/

int t4_tx_init(t4_state_t *s, const char *file)
{
    float x_resolution;
    float y_resolution;
    uint16_t res_unit;
    uint32_t parm;

    fprintf(stderr, "Start tx document\n");
    s->tiff_file = TIFFOpen(file, "r");
    if (s->tiff_file == NULL)
        return -1;

    s->file = file;
    TIFFGetField(s->tiff_file, TIFFTAG_IMAGEWIDTH, &parm);
    s->image_width = parm;
    s->bytes_per_row = (s->image_width + 7)/8;
    TIFFGetField(s->tiff_file, TIFFTAG_YRESOLUTION, &x_resolution);
    TIFFGetField(s->tiff_file, TIFFTAG_YRESOLUTION, &y_resolution);
    TIFFGetField(s->tiff_file, TIFFTAG_RESOLUTIONUNIT, &res_unit);

    if ((res_unit == RESUNIT_CENTIMETER  &&  x_resolution == 154.0)
        ||
        (res_unit == RESUNIT_INCH  &&  x_resolution == 392.0))
    {
        s->column_resolution = T4_RESOLUTION_SUPERFINE;
    }
    else if ((res_unit == RESUNIT_CENTIMETER  &&  x_resolution == 77.0)
            ||
            (res_unit == RESUNIT_INCH  &&  x_resolution == 196.0))
    {
        s->column_resolution = T4_RESOLUTION_FINE;
    }
    else
    {
        s->column_resolution = T4_RESOLUTION_STANDARD;
    }

    if ((res_unit == RESUNIT_CENTIMETER  &&  y_resolution == 154.0)
        ||
        (res_unit == RESUNIT_INCH  &&  y_resolution == 392.0))
    {
        s->row_resolution = T4_RESOLUTION_SUPERFINE;
        s->maxk = 8;
    }
    else if ((res_unit == RESUNIT_CENTIMETER  &&  y_resolution == 77.0)
             ||
             (res_unit == RESUNIT_INCH  &&  y_resolution == 196.0))
    {
        s->row_resolution = T4_RESOLUTION_FINE;
        s->maxk = 4;
    }
    else
    {
        s->row_resolution = T4_RESOLUTION_STANDARD;
        s->maxk = 2;
    }

    if (s->line_encoding != T4_COMPRESSION_ITU_T4_2D)
    {
        s->maxk =
        s->k = 0;
    }
    else
    {
        s->k = s->maxk - 1;
    }
    s->pages_transferred = 0;
    s->rowbuf = malloc(s->bytes_per_row);
    if (s->rowbuf == NULL)
        return -1;
    s->refrowbuf = malloc(s->bytes_per_row);
    if (s->refrowbuf == NULL)
    {
        free(s->rowbuf);
        s->rowbuf = NULL;
        return -1;
    }
    s->image_buffer_size = 0;
    s->header_info[0] = '\0';
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void make_header(t4_state_t *s, char *header)
{
    time_t now;
    struct tm tm;
    static const char *months[] =
    {
        "Jan",
        "Feb",
        "Mar",
        "Apr",
        "May",
        "Jun",
        "Jul",
        "Aug",
        "Sep",
        "Oct",
        "Nov",
        "Dec"
    };

    time(&now);
    tm = *localtime(&now);
    snprintf(header,
             132,
             "  %2d-%s-%d  %02d:%02d    %-50s %-21s   p.%d",
             tm.tm_mday,
             months[tm.tm_mon],
             tm.tm_year + 1900,
             tm.tm_hour,
             tm.tm_min,
             s->header_info,
             s->local_ident,
             s->pages_transferred + 1);
}
/*- End of function --------------------------------------------------------*/

int t4_tx_start_page(t4_state_t *s)
{
    int row;
    int ok;
    int i;
    int pattern;
    int data;
    int bit;
    int length;
    int rowbufptr;
    char *t;
    char header[132 + 1];
    static const int msbmask[9] =
    {
        0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff
    };

    fprintf(stderr, "Start tx page %d\n", s->pages_transferred);
    if (!TIFFSetDirectory(s->tiff_file, s->pages_transferred))
        return -1;
    s->image_size = 0;
    s->bit = 8;
    s->row_is_2d = FALSE;
    memset(s->refrowbuf, 0, s->bytes_per_row);

    if (s->header_info[0])
    {
        make_header(s, header);
        for (row = 0;  row < 16;  row++)
        {
            t = header;
            i = 0;
            data = 0;
            bit = 8;
            rowbufptr = 0;
            do
            {
                pattern = header_font[(uint8_t) *t][row];
                length = 16;
                while (length > bit)
                {
                    data |= (pattern >> (length - bit));
                    length -= bit;
                    bit = 8;
                    s->rowbuf[rowbufptr++] = data;
                    data = 0;
                }
                data |= ((pattern & msbmask[length]) << (bit - length));
                bit -= length;
                if (bit == 0)
                {
                    bit = 8;
                    s->rowbuf[rowbufptr++] = data;
                    data = 0;
                }
            }
            while (*++t  &&  rowbufptr < s->bytes_per_row - 2);
            while (rowbufptr < s->bytes_per_row - 2)
            {
                pattern = 0;
                length = 16;
                while (length > s->bit)
                {
                    data |= (pattern >> (length - bit));
                    length -= bit;
                    bit = 8;
                    s->rowbuf[rowbufptr++] = data;
                    data = 0;
                }
                data |= ((pattern & msbmask[length]) << (bit - length));
                bit -= length;
                if (bit == 0)
                {
                    bit = 8;
                    s->rowbuf[rowbufptr++] = data;
                    data = 0;
                }
            }
            if (s->row_resolution == T4_RESOLUTION_SUPERFINE)
            {
                if ((ok = t4_encode_row(s, s->rowbuf)) <= 0)
                    return -1;
                if ((ok = t4_encode_row(s, s->rowbuf)) <= 0)
                    return -1;
            }
            if (s->row_resolution == T4_RESOLUTION_FINE)
            {
                if ((ok = t4_encode_row(s, s->rowbuf)) <= 0)
                    return -1;
            }
            if ((ok = t4_encode_row(s, s->rowbuf)) <= 0)
                return -1;
        }
    }
    TIFFGetField(s->tiff_file, TIFFTAG_IMAGELENGTH, &s->rows);
    for (row = 0;  row < s->rows;  row++)
    {
        if ((ok = TIFFReadScanline(s->tiff_file, s->rowbuf, row, 0)) <= 0)
        {
            fprintf(stderr, "%s: Write error at row %d.\n", s->file, row);
            break;
        }
        if ((ok = t4_encode_row(s, s->rowbuf)) <= 0)
            return -1;
    }

    /* Now attach a return to control (RTC == 6 x EOLs) to the end of the page */
    s->row_is_2d = FALSE;
    for (i = 0;  i < 6;  i++)
    {
        t4_encode_eol(s);
        /* Suppress row padding between these EOLs */
        s->row_bits = INT_MAX - 1000;
    }
    put_bits(s, 0, 7);
    s->bit_pos = 7;
    s->bit_ptr = 0;
    s->row_bits = 0;

    return 0;
}
/*- End of function --------------------------------------------------------*/

int t4_tx_restart_page(t4_state_t *s)
{
    s->bit_pos = 7;
    s->bit_ptr = 0;
    s->row_bits = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

int t4_tx_end_page(t4_state_t *s)
{
    s->pages_transferred++;
}
/*- End of function --------------------------------------------------------*/

int t4_tx_getbit(t4_state_t *s)
{
    int bit;

    if (s->bit_ptr >= s->image_size)
        return 2;
    bit = (s->image_buffer[s->bit_ptr] >> s->bit_pos) & 1;
    if (--s->bit_pos < 0)
    {
        s->bit_pos = 7;
        s->bit_ptr++;
    }
    return bit;
}
/*- End of function --------------------------------------------------------*/

int t4_tx_delete(t4_state_t *s)
{
    if (t4_tx_end(s))
        return -1;
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

int t4_tx_end(t4_state_t *s)
{
    if (s->tiff_file)
    {
        TIFFClose(s->tiff_file);
        s->tiff_file = NULL;
        s->file = NULL;
    }
    if (s->image_buffer)
    {
        free(s->image_buffer);
        s->image_buffer = NULL;
        s->image_buffer_size = 0;
    }
    if (s->rowbuf)
    {
        free(s->rowbuf);
        s->rowbuf = NULL;
    }
    if (s->refrowbuf)
    {
        free(s->refrowbuf);
        s->refrowbuf = NULL;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

void t4_tx_set_tx_encoding(t4_state_t *s, int encoding)
{
    s->line_encoding = encoding;
}
/*- End of function --------------------------------------------------------*/

void t4_tx_set_min_row_bits(t4_state_t *s, int bits)
{
    s->min_row_bits = bits;
}
/*- End of function --------------------------------------------------------*/

void t4_tx_set_local_ident(t4_state_t *s, const char *ident)
{
    s->local_ident = (ident  &&  ident[0])  ?  ident  :  NULL;
}
/*- End of function --------------------------------------------------------*/

int t4_tx_set_header_info(t4_state_t *s, const char *info)
{
    if (info == NULL)
    {
        s->header_info[0] = '\0';
        return 0;
    }
    if (strlen(info) < sizeof(s->header_info))
    {
        strcpy(s->header_info, info);
        return 0;
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

const char *t4_tx_get_header_info(t4_state_t *s)
{
    return s->header_info;
}
/*- End of function --------------------------------------------------------*/

int t4_tx_get_row_resolution(t4_state_t *s)
{
    return s->row_resolution;
}
/*- End of function --------------------------------------------------------*/

int t4_tx_get_column_resolution(t4_state_t *s)
{
    return s->column_resolution;
}
/*- End of function --------------------------------------------------------*/

int t4_tx_get_columns(t4_state_t *s)
{
    return s->image_width;
}
/*- End of function --------------------------------------------------------*/

void t4_get_transfer_statistics(t4_state_t *s, t4_stats_t *t)
{
    t->pages_transferred = s->pages_transferred;
    t->columns = s->image_width;
    t->rows = s->rows;
    t->bad_rows = s->bad_rows;
    t->longest_bad_row_run = s->longest_bad_row_run;
    t->column_resolution = s->column_resolution;
    t->row_resolution = s->row_resolution;
    t->encoding = s->line_encoding;
    t->image_size = s->image_size;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

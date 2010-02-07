/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v18.c - V.18 text telephony for the deaf.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004-2009 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: v18.c,v 1.1 2009/04/01 13:22:40 steveu Exp $
 */
 
/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/queue.h"
#include "spandsp/async.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"
#include "spandsp/super_tone_rx.h"
#include "spandsp/power_meter.h"
#include "spandsp/fsk.h"
#include "spandsp/modem_connect_tones.h"
#include "spandsp/v8.h"
#include "spandsp/v18.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/fsk.h"
#include "spandsp/private/modem_connect_tones.h"
#include "spandsp/private/v18.h"

#include <stdlib.h>

struct dtmf_to_ascii_s
{
    const char *dtmf;
    char ascii;
};

static const struct dtmf_to_ascii_s dtmf_to_ascii[] =
{
    {"###1", 'C'},
    {"###2", 'F'},
    {"###3", 'I'},
    {"###4", 'L'},
    {"###5", 'O'},
    {"###6", 'R'},
    {"###7", 'U'},
    {"###8", 'X'},
    {"###9", ';'},
    {"###0", '!'},
    {"##*1", 'A'},
    {"##*2", 'D'},
    {"##*3", 'G'},
    {"##*4", 'J'},
    {"##*5", 'M'},
    {"##*6", 'P'},
    {"##*7", 'S'},
    {"##*8", 'V'},
    {"##*9", 'Y'},
    {"##1", 'B'},
    {"##2", 'E'},
    {"##3", 'H'},
    {"##4", 'K'},
    {"##5", 'N'},
    {"##6", 'Q'},
    {"##7", 'T'},
    {"##8", 'W'},
    {"##9", 'Z'},
    {"##0", ' '},
    {"#*1", 'æ'}, // (Note 1) 111 1011
    {"#*2", 'ø'}, // (Note 1) 111 1100
    {"#*3", 'å'}, // (Note 1) 111 1101
    {"#*4", 'Æ'}, // (Note 1) 101 1011
    {"#*5", 'Ø'}, // (Note 1) 101 1100
    {"#*6", 'Å'}, // (Note 1) 101 1101
    {"#0", '?'},
    {"#1", 'c'},
    {"#2", 'f'},
    {"#3", 'i'},
    {"#4", 'l'},
    {"#5", 'o'},
    {"#6", 'r'},
    {"#7", 'u'},
    {"#8", 'x'},
    {"#9", '.'},
    {"*#0", '0'},
    {"*#1", '1'},
    {"*#2", '2'},
    {"*#3", '3'},
    {"*#4", '4'},
    {"*#5", '5'},
    {"*#6", '6'},
    {"*#7", '7'},
    {"*#8", '8'},
    {"*#9", '9'},
    {"**1", '+'},
    {"**2", '-'},
    {"**3", '='},
    {"**4", ':'},
    {"**5", '%'},
    {"**6", '('},
    {"**7", ')'},
    {"**8", ','},
    {"**9", '\n'},
    {"*0", '\b'},
    {"*1", 'a'},
    {"*2", 'd'},
    {"*3", 'g'},
    {"*4", 'j'},
    {"*5", 'm'},
    {"*6", 'p'},
    {"*7", 's'},
    {"*8", 'v'},
    {"*9", 'y'},
    {"0", ' '},
    {"1", 'b'},
    {"2", 'e'},
    {"3", 'h'},
    {"4", 'k'},
    {"5", 'n'},
    {"6", 'q'},
    {"7", 't'},
    {"8", 'w'},
    {"9", 'z'},
    {"", '\0'}
};

static const char *ascii_to_dtmf[128] =
{
    "",         /* NULL */
    "",         /* SOH */
    "",         /* STX */
    "",         /* ETX */
    "",         /* EOT */
    "",         /* ENQ */
    "",         /* ACK */
    "",         /* BEL */
    "*0",       /* BACK SPACE */
    "0",        /* HT >> SPACE */
    "**9",      /* LF */
    "**9",      /* VT >> LF */
    "**9",      /* FF >> LF */
    "",         /* CR */
    "",         /* SO */
    "",         /* SI */
    "",         /* DLE */
    "",         /* DC1 */
    "",         /* DC2 */
    "",         /* DC3 */
    "",         /* DC4 */
    "",         /* NAK */
    "",         /* SYN */
    "",         /* ETB */
    "",         /* CAN */
    "",         /* EM */
    "#0",       /* SUB >> ? */
    "",         /* ESC */
    "**9",      /* IS4 >> LF */
    "**9",      /* IS3 >> LF */
    "**9",      /* IS2 >> LF */
    "0",        /* IS1 >> SPACE */
    "0",        /* SPACE */
    "###0",     /* ! */
    "",         /* " */
    "",         /* # */
    "",         /* $ */
    "**5",      /* % */
    "**1",      /* & >> + */
    "",         /* ’ */
    "**6",      /* ( */
    "**7",      /* ) */
    "#9",       /* _ >> . */
    "**1",      /* + */
    "**8",      /* , */
    "**2",      /* - */
    "#9",       /* . */
    "",         /* / */
    "*#0",      /* 0 */
    "*#1",      /* 1 */
    "*#2",      /* 2 */
    "*#3",      /* 3 */
    "*#4",      /* 4 */
    "*#5",      /* 5 */
    "*#6",      /* 6 */
    "*#7",      /* 7 */
    "*#8",      /* 8 */
    "*#9",      /* 9 */
    "**4",      /* : */
    "###9",     /* ; */
    "**6",      /* < >> ( */
    "**3",      /* = */
    "**7",      /* > >> ) */
    "#0",       /* ? */
    "###8",     /* @ >> X */
    "##*1",     /* A */
    "##1",      /* B */
    "###1",     /* C */
    "##*2",     /* D */
    "##2",      /* E */
    "###2",     /* F */
    "##*3",     /* G */
    "##3",      /* H */
    "###3",     /* I */
    "##*4",     /* J */
    "##4",      /* K */
    "###4",     /* L */
    "##*5",     /* M */
    "##5",      /* N */
    "###5",     /* O */
    "##*6",     /* P */
    "##6",      /* Q */
    "###6",     /* R */
    "##*7",     /* S */
    "##7",      /* T */
    "###7",     /* U */
    "##*8",     /* V */
    "##8",      /* W */
    "###8",     /* X */
    "##*9",     /* Y */
    "##9",      /* Z */
    "#*4",      /* Æ (National code) */
    "#*5",      /* Ø (National code) */
    "#*6",      /* Å (National code) */
    "",         /* ^ */
    "0",        /* _ >> SPACE */
    "",         /* ’ */
    "*1",       /* a */
    "1",        /* b */
    "#1",       /* c */
    "*2",       /* d */
    "2",        /* e */
    "#2",       /* f */
    "*3",       /* g */
    "3",        /* h */
    "#3",       /* i */
    "*4",       /* j */
    "4",        /* k */
    "#4",       /* l */
    "*5",       /* m */
    "5",        /* n */
    "#5",       /* o */
    "*6",       /* p */
    "6",        /* q */
    "#6",       /* r */
    "*7",       /* s */
    "7",        /* t */
    "#7",       /* u */
    "*8",       /* v */
    "8",        /* w */
    "#8",       /* x */
    "*9",       /* y */
    "9",        /* z */
    "#*1",      /* æ (National code) */
    "#*1",      /* ø (National code) */
    "#*3",      /* å (National code) */
    "0",        /* ~ >> SPACE */
    "*0"        /* DEL >> BACK SPACE */
};

#if 0
static int cmp(const void *s, const void *t)
{
    const char *ss;
    struct dtmf_to_ascii_s *tt;

    ss = (const char *) s;
    tt = (struct dtmf_to_ascii_s *) t;
    return strncmp(ss, tt->dtmf, strlen(tt->dtmf));
}

int main(int argc, char *argv[])
{
    char a[257];
    char b[1024];
    char c[1024];
    char *s;
    char *t;
    const char *u;
    char *v;
    struct dtmf_to_ascii_s *ss;
    int i;
    int entries;
    int x;
    int xl;
    int xh;
    int xx;
    int y;
    
    for (i = 0;  i < 128;  i++)
        a[i] = i + 1;
    a[127] = '\0';
    s = a;
    t = b;
    while (*s)
    {
        u = ascii_to_dtmf[*s & 0x7F];
        while (*u)
            *t++ = *u++;
        s++;
    }
    *t = '\0';
    printf("%s\n", b);
    
    s = b;
    t = c;
    entries = sizeof(dtmf_to_ascii)/sizeof(dtmf_to_ascii[0]) - 1;
    while (*s)
    {
        ss = bsearch(s, dtmf_to_ascii, entries, sizeof(dtmf_to_ascii[0]), cmp);
        if (ss)
        {
            *t++ = ss->ascii;
            s += strlen(ss->dtmf);
        }
        else
        {
            /* Can't match the code. Let's assume this is a code we just don't know, and skip over it */
            while (*s == '#'  ||  *s == '*')
                s++;
            if (*s)
                s++;
        }
    }
    *t = '\0';
    printf("%s\n", c);
    printf("%s\n", a + 30);
    
    return 0;
}
#endif
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_tx(v18_state_t *s, int16_t *amp, int max_len)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_rx(v18_state_t *s, const int16_t *amp, int len)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) v18_get_logging_state(v18_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v18_state_t *) v18_init(v18_state_t *s,
                                     int caller,
                                     int mode)
{
    if (s == NULL)
    {
        if ((s = (v18_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->caller = caller;
    s->mode = mode;

    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_release(v18_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_free(v18_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

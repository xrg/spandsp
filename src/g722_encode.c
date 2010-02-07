/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g722_encode.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
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
 * Based on a single channel G.722 codec which is:
 *
 *****    Copyright (c) CMU    1993      *****
 * Computer Science, Speech Group
 * Chengxiang Lu and Alex Hauptmann
 *
 * $Id: g722_encode.c,v 1.2 2005/09/04 07:40:03 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <inttypes.h>
#include <memory.h>
#include <stdlib.h>

#include "spandsp/telephony.h"
#include "spandsp/g722.h"

static int block4h(g722_encode_state_t *s, int d);
static int block4l(g722_encode_state_t *s, int d);

int g722_encode_init(g722_encode_state_t *s, int rate)
{
    memset(s, 0, sizeof(*s));
    s->detlow  = 32;
    s->dethigh = 8;
}

int g722_encode(g722_encode_state_t *s, uint16_t *outbuf, const int16_t *buf, int len)
{
    static const int q6[32] =
    {
           0,   35,   72,  110,  150,  190,  233,  276,
         323,  370,  422,  473,  530,  587,  650,  714,
         786,  858,  940, 1023, 1121, 1219, 1339, 1458,
        1612, 1765, 1980, 2195, 2557, 2919,    0,    0
    };

    static const int iln[32] =
    {
         0, 63, 62, 31, 30, 29, 28, 27,
        26, 25, 24, 23, 22, 21, 20, 19,
        18, 17, 16, 15, 14, 13, 12, 11,
        10,  9,  8,  7,  6,  5,  4,  0
    };

    static const int ilp[32] =
    {
         0, 61, 60, 59, 58, 57, 56, 55,
        54, 53, 52, 51, 50, 49, 48, 47,
        46, 45, 44, 43, 42, 41, 40, 39,
        38, 37, 36, 35, 34, 33, 32,  0
    };

    static const int wl[8] =
    {
        -60, -30, 58, 172, 334, 538, 1198, 3042
    };

    static const int rl42[16] =
    {
        0, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 0
    };

    static const int ilb[32] =
    {
        2048, 2093, 2139, 2186, 2233, 2282, 2332,
        2383, 2435, 2489, 2543, 2599, 2656, 2714,
        2774, 2834, 2896, 2960, 3025, 3091, 3158,
        3228, 3298, 3371, 3444, 3520, 3597, 3676,
        3756, 3838, 3922, 4008
    };

    static const int qm4[16] =
    {
             0, -20456,	-12896,	 -8968,
         -6288,	 -4240,	 -2584,	 -1200,
         20456,	 12896,	  8968,	  6288,
          4240,	  2584,	  1200,	     0
    };

    static const int qm2[4] =
    {
        -7408,	-1616,	7408,	1616
    };

    static const int qmf_coeffs[24] =
    {
           3,  -11,  -11,   53,   12, -156,   32,  362,
        -210, -805,  951, 3876, 3876,  951, -805, -210,
         362,   32, -156,   12,   53,  -11,  -11,    3
    };
    static const int ihn[3] = {0, 1, 0};
    static const int ihp[3] = {0, 3, 2};
    static const int wh[3] = {0, -214, 798};
    static const int rh2[4] = {2, 1, 2, 1};

    int dlowt;
    int dhigh;
    int  el;
    int mil;
    int wd;
    int wd1;
    int hdu;
    int ril;
    int wd2;
    int il4;
    int wd3;
    int eh;
    int mih;
    const int16_t *datap;
    const int *q6p;
    int ih2;
    int *xp;
    int *xp_2;
    const int *yp;
    int i;
    int j;
    /* low and high band PCM from QMF */
    int xlow;
    int xhigh;

    datap = buf;
    len >>= 2;
    for (j = 0;  j < len;  )
    {
        /* Process PCM through the QMF */
        xp = s->x + 23;
        xp_2 = s->x + 21;

        for (i = 0;  i < 22;  i++)
            *xp-- = *xp_2--;

        *xp-- = *datap++;
        *xp = *datap++;

        /* Discard every other QMF output */
        yp = qmf_coeffs;
        s->sumeven = 0;
        s->sumodd = 0;
        for (i = 0;  i < 12;  i++)
        {
            s->sumeven += *xp++ * *yp++;
            s->sumodd += *xp++ * *yp++;
        }

        xlow = (s->sumeven + s->sumodd) >> 13;
        xhigh = (s->sumeven - s->sumodd) >> 13;

        /* Block 1L, SUBTRA */
        el = xlow - s->slow;

        if (el > 32767)
            el = 32767;
        else if (el < -32768)
            el = -32768;

        /* Block 1L, QUANTL */
        wd = (el >= 0)  ?   el  :  -(el + 1);

        q6p = q6;

        mil = 1;
        for (i = 0;  i < 29;  i++)
        {
            wd2 = *++q6p * s->detlow;
            wd1 = wd2 >> 12;
            if (wd < wd1)
                break;
            mil++;
        }

        s->ilow = (el < 0)  ?  iln[mil]  :  ilp[mil];

        /* Block 2L, INVQAL */
        ril = s->ilow >> 2;
        wd2 = qm4[ril];
        dlowt = (s->detlow * wd2) >> 15;

        /* Block 3L, LOGSCL */
        il4 = rl42[ril];

        wd = (s->nbl * 127) >> 7;

        s->nbl = wd + wl[il4];

        if (s->nbl < 0)
            s->nbl = 0;
        else if (s->nbl > 18432)
            s->nbl = 18432;

        /* Block 3L, SCALEL */
        wd1 = (s->nbl >> 6) & 31;
        wd2 = s->nbl >> 11;
        wd3 = ((8 - wd2) < 0)  ?  ilb[wd1] << (wd2 - 8)  :  ilb[wd1] >> (8 - wd2);
        s->detlow = wd3 << 2;

        /* Block 3L, DELAYA */
        s->slow = block4l(s, dlowt);

        /* Block 1H, SUBTRA */
        eh = xhigh - s->shigh;

        /* Block 1H, QUANTH */
        wd = (eh >= 0)  ?  eh  :  -(eh + 1);

        hdu = 564 * s->dethigh;
        wd1 = hdu >> 12;
        mih = (wd >= wd1)  ?  2  :  1;
        s->ihigh = (eh < 0)  ?  ihn[mih]  :  ihp[mih];

        /* Block 2H, INVQAH */
        wd2 = qm2[s->ihigh];
        dhigh = (s->dethigh * wd2) >> 15;

        /* Block 3H, LOGSCH */
        ih2 = rh2[s->ihigh];
        wd = (s->nbh * 127) >> 7;
        s->nbh = wd + wh[ih2];

        if (s->nbh < 0)
            s->nbh = 0;
        else if (s->nbh > 22528)
            s->nbh = 22528;

        /* Block 3H, SCALEH */
        wd1 = (s->nbh >> 6) & 31;
        wd2 = s->nbh >> 11;
        wd3 = ((10 - wd2) < 0)  ?  ilb[wd1] << (wd2 - 10) :  ilb[wd1] >> (10 - wd2);
        s->dethigh = wd3 << 2;

        /* Block 3L, DELAYA */
        s->shigh = block4h(s, dhigh);
        if ((s->k = !s->k))
        {
            outbuf[j] = s->ilow;
            outbuf[j] = (outbuf[j] << 2) + s->ihigh;
        }
        else
        {
            outbuf[j] = (outbuf[j] << 6) + s->ilow;
            outbuf[j] = (outbuf[j] << 2) + s->ihigh;
            j++;
        }
    }

    return len;
}

static int block4l(g722_encode_state_t *s, int dl)
{
    int wd1;
    int wd2;
    int wd3;
    int wd4;
    int wd5;
    int *sgp;
    int *pltp;
    int *alp;
    int *dltp;
    int *blp;
    int *rltp;
    int *pltp_1;
    int *dltp_1;
    int *rltp_1;
    int i;

    /* Block 4L, RECONS */
    *s->dlt = dl;

    *s->rlt = s->sl + dl;

    /* Block 4L, PARREC */
    *s->plt = dl + s->szl;

    /* Block 4L, UPPOL2 */
    sgp = s->sgl;
    pltp = s->plt;
    alp = s->al;
    for (i = 0;  i < 3;  i++)
        *sgp++ = *pltp++ >> 15;

    wd1 = *++alp << 2;

    if (wd1 > 32767)
        wd1 = 32767;
    else if (wd1 < -32768)
        wd1 = -32768;

    wd2 = (*s->sgl == *(s->sgl + 1))  ?  -wd1  :  wd1;
    if (wd2 > 32767)
        wd2 = 32767;

    wd2 = wd2 >> 7;
    wd3 = (*s->sgl == *(s->sgl + 2))  ?  128  :  -128;
    wd4 = wd2 + wd3;
    wd5 = (*++alp * 32512) >> 15;

    *alp = wd4 + wd5;

    if (*alp > 12288)
        *alp =  12288;
    else if (*alp  < -12288)
        *alp = -12288;

    /* Block 4L, UPPOL1 */
    *s->sgl = *s->plt >> 15;
    *(s->sgl + 1) = *(s->plt + 1) >> 15;
    wd1 = (*s->sgl == *(s->sgl + 1))  ?  192  :  -192;

    wd2 = (*--alp * 32640) >> 15;

    *alp = wd1 + wd2;
    wd3 = (15360 - *++alp);
    if (*--alp >  wd3)
        *alp =  wd3;
    else if (*alp  < -wd3)
        *alp = -wd3;

    /* Block 4L, UPZERO */
    wd1 = ( dl == 0 )  ?  0  :  128;
    *s->sgl = dl >> 15;
    sgp = s->sgl;
    dltp = s->dlt;
    blp = s->bl;

    for (i = 0;  i < 6;  i++)
    {
        *++sgp = *++dltp >> 15;
        wd2 = (*sgp == *s->sgl) ? wd1 : -wd1;
        wd3 = (*++blp * 32640) >> 15;
        *blp = wd2 + wd3;
    }

    /* Block 4L, DELAYA */
    dltp_1 = dltp - 1;
    for (i = 0;  i < 6;  i++)
        *dltp-- = *dltp_1--;

    rltp = s->rlt + 2;
    pltp = s->plt + 2;
    rltp_1 = rltp - 1;
    pltp_1 = pltp - 1;

    for (i = 0;  i < 2;  i++)
    {
        *rltp-- = *rltp_1--;
        *pltp-- = *pltp_1--;
    }

    /* Block 4L, FILTEP */
    wd1 = (*alp * *++rltp) >> 14;

    wd2 = (*++alp * *++rltp) >> 14;

    s->spl = wd1 + wd2;
    /* Block 4L, FILTEZ */

    blp = blp - 6;
    s->szl = 0;
    for (i = 0;  i < 6;  i++)
        s->szl += (*++blp * *++dltp) >> 14;

    /* Block 4L, PREDIC */
    s->sl = s->spl + s->szl;

    return s->sl;
}

static int block4h(g722_encode_state_t *s, int d)
{
    int wd1;
    int wd2;
    int wd3;
    int wd4;
    int wd5;
    int *sgp;
    int *bhp;
    int *dhp;
    int *php;
    int *ahp;
    int *rhp;
    int *dhp_1;
    int *rhp_1;
    int *php_1;
    int i;

    /* Block 4H, RECONS */
    *s->dh = d;
    *s->rh = s->sh + d;

    /* Block 4H, PARREC */
    *s->ph = d + s->szh;

    /* Block 4H, UPPOL2 */
    sgp = s->sgh;
    php = s->ph;
    ahp = s->ah;
    *s->sgh = *s->ph >> 15;
    *++sgp = *++php >> 15;
    *++sgp = *++php >> 15;
    wd1 = (*++ahp) << 2;

    if (wd1 > 32767)
        wd1 = 32767;
    else if (wd1 < -32768)
        wd1 = -32768;

    wd2 = (*s->sgh == *--sgp)  ?  -wd1  :  wd1;
    if (wd2 > 32767)
        wd2 = 32767;

    wd2 = wd2 >> 7;
    wd3 = (*s->sgh == *++sgp)  ?  128  :  -128;

    wd4 = wd2 + wd3;
    wd5 = (*++ahp * 32512) >> 15;

    *ahp = wd4 + wd5;
    if (*ahp > 12288)
        *ahp  =  12288;
    else if (*ahp  < -12288)
        *ahp  = -12288;

    /* Block 4H, UPPOL1 */
    *s->sgh = *s->ph >> 15;
    *--sgp  = *--php >> 15;
    wd1 = (*s->sgh == *sgp)  ?  192  :  -192;

    wd2 = (*--ahp * 32640) >> 15;

    *ahp = wd1 + wd2;
    wd3 = (15360 - *++ahp);
    if (*--ahp > wd3)
        *ahp  =  wd3;
    else if (*ahp  < -wd3)
        *ahp  = -wd3;

    /* Block 4H, UPZERO */
    wd1 = (d == 0)  ?  0  :  128;

    *s->sgh = d >> 15;
    dhp = s->dh;
    bhp = s->bh;
    for (i = 0;  i < 6;  i++)
    {
        *sgp = *++dhp >> 15;
        wd2 = (*sgp++ == *s->sgh)  ?  wd1  :  -wd1;
        wd3 = (*++bhp * 32640) >> 15;
        *bhp = wd2 + wd3;
    }

    /* Block 4H, DELAYA */
    dhp_1 = dhp - 1;
    for (i = 0;  i < 6;  i++)
        *dhp-- = *dhp_1--;

    rhp = s->rh + 2;
    php++;
    rhp_1 = rhp - 1;
    php_1 = php - 1;
    for (i = 0;  i < 2;  i++)
    {
        *rhp-- = *rhp_1--;
        *php-- = *php_1--;
    }

    /* Block 4H, FILTEP */
    wd1 = (*ahp * *++rhp) >> 14;
    wd2 = (*++ahp * *++rhp) >> 14;
    s->sph = wd1 + wd2;

    /* Block 4H, FILTEZ */
    bhp -= 6;
    s->szh = 0;
    for (i = 0;  i < 6;  i++)
        s->szh += (*++bhp * *++dhp) >> 14;

    /* Block 4L, PREDIC */
    s->sh = s->sph + s->szh;

    return s->sh;
}

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g722_decode.c
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
 * $Id: g722_decode.c,v 1.2 2005/09/04 07:40:03 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <memory.h>
#include <stdlib.h>

#include "spandsp/telephony.h"
#include "spandsp/g722.h"

static int block4h(g722_decode_state_t *s, int d);
static int block4l(g722_decode_state_t *s, int dl);

int g722_decode_init(g722_decode_state_t *s, int rate)
{
    memset(s, 0, sizeof(*s));
    s->detlow = 32;
    s->dethigh = 8;
}

int g722_decode(g722_decode_state_t *s, int16_t *outbuf, const uint16_t *buf, int len)
{
    int ilowr;
    int dlowt;
    int rlow;
    int ihigh;
    int dhigh;
    int rhigh;
    int xout1;
    int j;
    int jj;

    /* Block 3l */

    register int wd1, wd2, wd3;
    static int nbl = 0;

    static const int wl[8] = {-60, -30, 58, 172, 334, 538, 1198, 3042 };
    static const int rl42[16] = {0, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3,  2, 1, 0 };
    static const int ilb[32] =
    {
        2048, 2093, 2139, 2186, 2233, 2282, 2332,
        2383, 2435, 2489, 2543, 2599, 2656, 2714,
        2774, 2834, 2896, 2960, 3025, 3091, 3158,
        3228, 3298, 3371, 3444, 3520, 3597, 3676,
        3756, 3838, 3922, 4008
    };

    /* Block 3h */
    static const int wh[3] = {0, -214, 798};
    static const int rh2[4] = {2, 1, 2, 1};
    /* Block 5l */
    static const int qm6[64] =
    {
           -136,   -136,   -136,   -136,
         -24808, -21904, -19008, -16704,
         -14984, -13512, -12280, -11192,
         -10232,  -9360,  -8576,  -7856,
          -7192,  -6576,  -6000,  -5456,
          -4944,  -4464,  -4008,  -3576,
          -3168,  -2776,  -2400,  -2032,
          -1688,  -1360,  -1040,   -728,
          24808,  21904,  19008,  16704,
          14984,  13512,  12280,  11192,
          10232,   9360,   8576,   7856,
           7192,   6576,   6000,   5456,
           4944,   4464,   4008,   3576,
           3168,   2776,   2400,   2032,
           1688,   1360,   1040,    728,
            432,    136,   -432,   -136
    };

    /* Block 2h */
    static const int qm2[4] = {-7408, -1616,  7408,   1616};

    /* Block 2l */
    static const int qm4[16] =
    {
             0, -20456, -12896,  -8968,
         -6288,  -4240,  -2584,  -1200,
         20456,  12896,   8968,   6288,
          4240,   2584,   1200,      0
    };

    static const int qmf_coeffs[] =
    {
        3, -11, 12, 32, -210, 951, 3876, -805, 362, -156, 53, -11,
        -11, 53, -156, 362, -805, 3876, 951, -210, 32, 12, -11, 3
    };

    int *xdp;
    int *xsp;
    int *xdp_1;
    int *xsp_1;
    const uint16_t *decdatap;
    int16_t *pcmoutp;
    int i;

    decdatap = buf;
    pcmoutp = outbuf;
    len <<= 2;
    ilowr = (*decdatap >> 10) & 0x3F;
    ihigh = (*decdatap >> 8) & 0x03;
    for (j = 0;  j < len;  j += 2)
    {
        /* Block 5L, LOW BAND INVQBL */
        wd2 = qm6[ilowr];
        wd2 = (s->detlow * wd2 ) >> 15;
        /* Block 5L, RECONS */
        rlow = s->slow + wd2;
        /* Block 6L, LIMIT */
        /* Block 2L, INVQAL */
        wd1 = ilowr >> 2;
        wd2 = qm4[wd1];
        dlowt = (s->detlow * wd2) >> 15;

        /* Block 3L, LOGSCL */
        wd2 = rl42[wd1];
        wd1 = (nbl * 127) >> 7;
        nbl = wd1 + wl[wd2];
        if (nbl < 0)
            nbl = 0;
        else if (nbl > 18432)
            nbl = 18432;
            
        /* Block 3L, SCALEL */
        wd1 = (nbl >> 6) & 31;
        wd2 = nbl >> 11;
        wd3 = ((8 - wd2) < 0)  ?  ilb[wd1] << (wd2 - 8)  :  ilb[wd1] >> (8 - wd2);
        s->detlow = wd3 << 2;

        s->slow = block4l(s, dlowt);

        /* Block 2H, INVQAH */
        wd2 = qm2[ihigh];
        dhigh  = (s->dethigh * wd2) >> 15;

        rhigh  = dhigh + s->shigh;

        /* Block 2H, INVQAH */
        wd2 = rh2[ihigh];
        wd1 = (s->nbh * 127) >> 7;
        s->nbh = wd1 + wh[wd2];

        if (s->nbh < 0)
            s->nbh = 0;
        else if (s->nbh > 22528)
            s->nbh = 22528;
            
        /* Block 3H, SCALEH */
        wd1 = (s->nbh >> 6) & 31;
        wd2 = s->nbh >> 11;
        wd3 = ((10 - wd2) < 0)  ?  ilb[wd1] << (wd2 - 10)  :  ilb[wd1] >> (10 - wd2);
        s->dethigh = wd3 << 2;

        s->shigh = block4h(s, dhigh);

        /* RECIEVE QMF */
        xdp = s->xd + 11;
        xsp = s->xs + 11;
        xdp_1 = s->xd + 10;
        xsp_1 = s->xs + 10;
        for (i = 0;  i < 11;  i++)
        {
            *xdp-- = *xdp_1--;
            *xsp-- = *xsp_1--;
        }

        /* RECA */
        *xdp  = rlow - rhigh;

        /* RECA */
        *xsp  = rlow + rhigh;

        /* ACCUM C&D */
        /* QMF tap coefficients */
        xout1 = 0;
        for (i = 0;  i < 12;  i++)
            xout1 += *xdp++ * qmf_coeffs[i];
        *pcmoutp++ = xout1 >> 12;

        xout1 = 0;
        for (i = 12;  i < 24;  i++)
            xout1 += *xsp++ * qmf_coeffs[i];
        *pcmoutp++ = xout1 >> 12;

        if ((s->k = !s->k))
        {
            ilowr = (*decdatap >> 10) & 0x3F;
            ihigh = (*decdatap >> 8) & 0x03;
        }
        else
        {
            ilowr = (*decdatap >> 2) & 0x3F;
            ihigh = (*decdatap++) & 0x03;
        }
    }
    return len;
}

static int block4l(g722_decode_state_t *s, int dl)
{
    int wd1;
    int wd2;
    int wd3;
    int *sgp;
    int *dltp;
    int *dltp_1;
    int *blp;
    int *bplp;
    int i;

    s->dlt[0] = dl;

    /* Block 4L, RECONS */
    s->rlt0 = s->sl + dl;

    /* Block 4L, PARREC */
    s->plt0 = dl + s->szl;

    /* Block 4L, UPPOL2 */
    s->sg0 = s->plt0 >> 15;
    s->sg1 = s->plt1 >> 15;
    s->sg2 = s->plt2 >> 15;

    wd1 = s->al1 << 2;
    if (wd1 > 32767)
        wd1 = 32767;
    else if (wd1 < -32768)
        wd1 = -32768;

    wd2 = (s->sg0 == s->sg1)  ?  -wd1  :  wd1;
    wd2 = wd2 >> 7;

    wd3 = (s->sg0 == s->sg2)  ?  128  :  -128;

    wd2 = wd2 + wd3;
    wd3 = (s->al2 * 127) >> 7;

    s->al2 = wd2 + wd3;
    if (s->al2 > 12288)
        s->al2 = 12288;
    else if (s->al2 < -12288)
        s->al2 = -12288;

    /* Block 4L, UPPOL1 */
    s->sg0 = s->plt0 >> 15;
    s->sg1 = s->plt1 >> 15;

    wd1 = (s->sg0 == s->sg1)  ?  192  :  -192;

    wd2 = (s->al1*255) >> 8;

    s->al1 = wd1 + wd2;

    wd3 = 15360 - s->al2;

    if (s->al1 > wd3)
        s->al1 =  wd3;
    else if (s->al1 < -wd3)
        s->al1 = -wd3;

    /* Block 4L, UPZERO */
    wd1 = (dl == 0)  ?  0  :  128;

    s->sg0 = dl >> 15;

    sgp = s->sg;
    dltp = s->dlt;
    blp = s->bl;
    bplp = s->bpl;
    for (i = 0;  i < 6;  i++)
    {
        *++sgp = *++dltp >> 15;
        wd2 = (*sgp == s->sg0)  ?   wd1  :  -wd1;
        wd3 = (*++blp * 255) >> 8;
        *++bplp = wd2 + wd3;
    }

    /* Block 4L, DELAYA */
    dltp = s->dlt + 6;
    dltp_1 = dltp - 1;
    blp = s->bl;
    bplp = s->bpl;
    
    for (i = 0;  i < 6;  i++)
    {
        *dltp-- = *dltp_1--;
        *++blp = *++bplp;
    }

    s->rlt2 = s->rlt1;
    s->rlt1 = s->rlt0;
    s->plt2 = s->plt1;
    s->plt1 = s->plt0;

    /* Block 4L, FILTEP */
    wd1 = (s->al1 * s->rlt1) >> 14;
    wd2 = (s->al2 * s->rlt2) >> 14;
    s->spl = wd1 + wd2;

    /* Block 4L, FILTEZ */
    dltp = s->dlt;
    blp = s->bl;
    s->szl = 0;
    for (i = 0;  i < 6;  i++)
        s->szl += (*++blp * *++dltp) >> 14;

    /* Block 4L, PREDIC */
    s->sl = s->spl + s->szl;

    return s->sl;
}

static int block4h(g722_decode_state_t *s, int d)
{
    int wd1;
    int wd2;
    int wd3;
    int *dhp;
    int *dhp_1;
    int *bhp;
    int *bphp;
    int *sgp;
    int i;

    s->dh[0] = d;

    /* Block 4H, RECONS */
    s->rh0 = s->sh + d;

    /* Block 4H, PARREC */
    s->ph0 = d + s->szh;

    /* Block 4H, UPPOL2 */
    s->sgh0 = s->ph0 >> 15;
    s->sgh1 = s->ph1 >> 15;
    s->sgh2 = s->ph2 >> 15;

    wd1 = s->ah1 << 2;

    if (wd1 > 32767)
        wd1 = 32767;
    else if (wd1 < -32768)
        wd1 = -32768;

    wd2 = (s->sgh0 == s->sgh1)  ?  -wd1  :  wd1;
    if (wd2 > 32767)
        wd2 = 32767;

    wd2 >>= 7;

    wd2 += (s->sgh0 == s->sgh2)  ?  128  :  -128;
    wd3 = (s->ah2 * 127) >> 7;

    s->ah2 = wd2 + wd3;
    if (s->ah2 > 12288)
        s->ah2 = 12288;
    else if (s->ah2 < -12288)
        s->ah2 = -12288;

    /* Block 4H, UPPOL1 */
    s->sgh0 = s->ph0 >> 15;
    s->sgh1 = s->ph1 >> 15;

    wd1 = (s->sgh0 == s->sgh1) ?  192  :  -192;

    wd2 = (s->ah1 * 255) >> 8;

    s->ah1 = wd1 + wd2;

    wd3 = (15360 - s->ah2);
    if (s->ah1 > wd3)
        s->ah1 =  wd3;
    else if (s->ah1 < -wd3)
        s->ah1 = -wd3;

    /* Block 4H, UPZERO */
    wd1 = (d == 0)  ?  0  :  128;

    s->sgh0 = d >> 15;
    sgp = s->sgh;
    dhp = s->dh;
    bhp = s->bh;
    bphp = s->bph;

    for (i = 0;  i < 6;  i++)
    {
        *++sgp = *++dhp >> 15;
        wd2 = (*sgp == s->sgh0)  ?  wd1  :  -wd1;
        wd3 = (*++bhp * 255) >> 8;
        *++bphp = wd2 + wd3;
    }

    /* Block 4H, DELAYA */
    dhp_1 = dhp - 1;
    bhp = s->bh;
    bphp = s->bph;
    
    for (i = 0;  i < 6;  i++)
    {
        *dhp-- = *dhp_1--;
        *++bhp = *++bphp;
    }
    s->rh2 = s->rh1;
    s->rh1 = s->rh0;
    s->ph2 = s->ph1;
    s->ph1 = s->ph0;
    
    /* Block 4H, FILTEP */
    wd1 = (s->ah1 * s->rh1) >> 14;
    wd2 = (s->ah2 * s->rh2) >> 14;

    s->sph = wd1 + wd2;

    /* Block 4H, FILTEZ */
    dhp = s->dh;
    bhp = s->bh;
    s->szh = 0;
    for (i = 0;  i < 6;  i++)
        s->szh += ((*++bhp) * *++dhp ) >> 14;

    /* Block 4H, PREDIC */
    s->sh = s->sph + s->szh;

    return s->sh;
}

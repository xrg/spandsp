/*
 * SpanDSP - a series of DSP components for telephony
 *
 * time_scale.c - Time scaling for linear speech data
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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
 * $Id: time_scale.c,v 1.2 2004/12/20 14:23:22 steveu Exp $
 */

/*! \file */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>

#include <tiffio.h>

#include "spandsp.h"

/*
    Time scaling for speech, based on the Pointer Interval Controlled
    OverLap and Add (PICOLA) method, developed by Morita Naotaka.
 */

static int __inline__ amdf_pitch(int min_pitch, int max_pitch, int16_t amp[], int len)
{
    int i;
    int j;
    int acc;
    int min_acc;
    int pitch;

    pitch = min_pitch;
    min_acc = INT_MAX;
    for (i = max_pitch;  i <= min_pitch;  i++)
    {
        acc = 0;
        for (j = 0;  j < len;  j++)
            acc += abs(amp[i + j] - amp[j]);
        if (acc < min_acc)
        {
            min_acc = acc;
            pitch = i;
        }
    }
    return pitch;
}
/*- End of function --------------------------------------------------------*/

static void __inline__ overlap_add(int16_t amp1[], int16_t amp2[], int len)
{
    int i;
    double weight;
    double step;
    
    step = 1.0/len;
    weight = 0.0;
    for (i = 0;  i < len;  i++)
    {
        /* TODO: saturate */
        amp2[i] = (double) amp1[i]*(1.0 - weight) + (double) amp2[i]*weight;
        weight += step;
    }
}
/*- End of function --------------------------------------------------------*/

int time_scale_init(time_scale_t *s, float rate)
{
    if (rate <= 0.0  ||  rate == 1.0)
        return -1;
    /*endif*/
    s->rate = rate;
    if (rate > 1.0)
        s->rcomp = 1.0/(rate - 1.0);
    else
        s->rcomp = rate/(1.0 - rate);
    /*endif*/
    s->rate_nudge = 0.0;
    s->fill = 0;
    s->lcp = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

int time_scale(time_scale_t *s, int16_t out[], int16_t in[], int len)
{
    double lcpf;
    int pitch;
    int out_len;
    int in_len;
    int i;
    int k;

    out_len = 0;
    in_len = 0;

    /* Top up the buffer */
    if (len < TIME_SCALE_BUF_LEN - s->fill)
    {
        /* Cannot continue without more samples */
        memcpy(s->buf + s->fill, in, sizeof(int16_t)*len);
        s->fill += len;
        return out_len;
    }
    k = (TIME_SCALE_BUF_LEN - s->fill);
    memcpy(s->buf + s->fill, in, sizeof(int16_t)*k);
    in_len += k;
    s->fill = TIME_SCALE_BUF_LEN;
    while (s->fill == TIME_SCALE_BUF_LEN)
    {
        while (s->lcp >= TIME_SCALE_BUF_LEN)
        {
            memcpy(out + out_len, s->buf, sizeof(int16_t)*TIME_SCALE_BUF_LEN);
            out_len += TIME_SCALE_BUF_LEN;
            if (len - in_len < TIME_SCALE_BUF_LEN)
            {
                /* Cannot continue without more samples */
                memcpy(s->buf, in + in_len, sizeof(int16_t)*(len - in_len));
                s->fill = len - in_len;
                s->lcp -= TIME_SCALE_BUF_LEN;
                return out_len;
            }
            memcpy(s->buf, in + in_len, sizeof(int16_t)*TIME_SCALE_BUF_LEN);
            in_len += TIME_SCALE_BUF_LEN;
            s->lcp -= TIME_SCALE_BUF_LEN;
        }
        if (s->lcp > 0)
        {
            memcpy(out + out_len, s->buf, sizeof(int16_t)*s->lcp);
            out_len += s->lcp;
            memcpy(s->buf, s->buf + s->lcp, sizeof(int16_t)*(TIME_SCALE_BUF_LEN - s->lcp));
            if (len - in_len < s->lcp)
            {
                /* Cannot continue without more samples */
                memcpy(s->buf + (TIME_SCALE_BUF_LEN - s->lcp), in + in_len, sizeof(int16_t)*(len - in_len));
                s->fill = TIME_SCALE_BUF_LEN - s->lcp + len - in_len;
                s->lcp = 0;
                return out_len;
            }
            memcpy(s->buf + (TIME_SCALE_BUF_LEN - s->lcp), in + in_len, sizeof(int16_t)*s->lcp);
            in_len += s->lcp;
            s->lcp = 0;
        }
        pitch = amdf_pitch(SAMPLE_RATE/TIME_SCALE_MIN_PITCH, SAMPLE_RATE/TIME_SCALE_MAX_PITCH, s->buf, SAMPLE_RATE/TIME_SCALE_MIN_PITCH);
        lcpf = (double) pitch*s->rcomp;
        /* Nudge around to compensate for fractional samples */
        s->lcp = lcpf;
        s->rate_nudge += s->lcp - lcpf;
        if (s->rate_nudge >= 0.5)
        {
            s->lcp--;
            s->rate_nudge -= 1.0;
        }
        else if (s->rate_nudge <= -0.5)
        {
            s->lcp++;
            s->rate_nudge += 1.0;
        }
        if (s->rate < 1.0)
        {
            /* Speed up - drop a chunk of data */
            overlap_add(s->buf, s->buf + pitch, pitch);
            memcpy(&s->buf[pitch], &s->buf[2*pitch], sizeof(int16_t)*(TIME_SCALE_BUF_LEN - 2*pitch));
            if (len - in_len < pitch)
            {
                /* Cannot continue without more samples */
                memcpy(s->buf + TIME_SCALE_BUF_LEN - pitch, in + in_len, sizeof(int16_t)*(len - in_len));
                s->fill += (len - in_len - pitch);
                return out_len;
            }
            memcpy(s->buf + TIME_SCALE_BUF_LEN - pitch, in + in_len, sizeof(int16_t)*pitch);
            in_len += pitch;
        }
        else
        {
            /* Slow down - insert a chunk of data */
            memcpy(out + out_len, s->buf, sizeof(int16_t)*pitch);
            out_len += pitch;
            overlap_add(s->buf + pitch, s->buf, pitch);
        }
    }
    return out_len;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

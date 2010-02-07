/*
 * SpanDSP - a series of DSP components for telephony
 *
 * tone_detect.c - General telephony tone detection.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001-2003, 2005 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
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
 * $Id: tone_detect.c,v 1.33 2007/08/13 13:21:08 steveu Exp $
 */
 
/*! \file tone_detect.h */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>

#include "spandsp/telephony.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"

#if !defined(M_PI)
/* C99 systems may not define M_PI */
#define M_PI 3.14159265358979323846264338327
#endif

void make_goertzel_descriptor(goertzel_descriptor_t *t, float freq, int samples)
{
#if defined(SPANDSP_USE_FIXED_POINT_EXPERIMENTAL)
    t->fac = 4096.0f*2.0f*cosf(2.0f*M_PI*(freq/(float) SAMPLE_RATE));
#else
    t->fac = 2.0f*cosf(2.0f*M_PI*(freq/(float) SAMPLE_RATE));
#endif
    t->samples = samples;
}
/*- End of function --------------------------------------------------------*/

goertzel_state_t *goertzel_init(goertzel_state_t *s,
                                goertzel_descriptor_t *t)
{
    if (s  ||  (s = malloc(sizeof(goertzel_state_t))))
    {
        s->v2 =
        s->v3 = 0.0;
        s->fac = t->fac;
        s->samples = t->samples;
        s->current_sample = 0;
    }
    return s;
}
/*- End of function --------------------------------------------------------*/

void goertzel_reset(goertzel_state_t *s)
{
    s->v2 =
    s->v3 = 0.0;
    s->current_sample = 0;
}
/*- End of function --------------------------------------------------------*/

int goertzel_update(goertzel_state_t *s,
                    const int16_t amp[],
                    int samples)
{
    int i;
#if defined(SPANDSP_USE_FIXED_POINT_EXPERIMENTAL)
    int32_t v1;
#else
    float v1;
#endif

    if (samples > s->samples - s->current_sample)
        samples = s->samples - s->current_sample;
    for (i = 0;  i < samples;  i++)
    {
        v1 = s->v2;
        s->v2 = s->v3;
#if defined(SPANDSP_USE_FIXED_POINT_EXPERIMENTAL)
        s->v3 = ((s->fac*s->v2) >> 12) - v1 + (amp[i] >> 8);
#else
        s->v3 = s->fac*s->v2 - v1 + amp[i];
#endif
    }
    s->current_sample += samples;
    return samples;
}
/*- End of function --------------------------------------------------------*/

float goertzel_result(goertzel_state_t *s)
{
#if defined(SPANDSP_USE_FIXED_POINT_EXPERIMENTAL)
    int32_t v1;
#else
    float v1;
#endif

    /* Push a zero through the process to finish things off. */
    v1 = s->v2;
    s->v2 = s->v3;
#if defined(SPANDSP_USE_FIXED_POINT_EXPERIMENTAL)
    s->v3 = ((s->fac*s->v2) >> 12) - v1;
#else
    s->v3 = s->fac*s->v2 - v1;
#endif
    /* Now calculate the non-recursive side of the filter. */
    /* The result here is not scaled down to allow for the magnification
       effect of the filter (the usual DFT magnification effect). */
#if defined(SPANDSP_USE_FIXED_POINT_EXPERIMENTAL)
    return s->v3*s->v3 + s->v2*s->v2 - ((s->v2*s->v3) >> 12)*s->fac;
#else
    return s->v3*s->v3 + s->v2*s->v2 - s->v2*s->v3*s->fac;
#endif
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

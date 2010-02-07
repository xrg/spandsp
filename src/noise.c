/*
 * SpanDSP - a series of DSP components for telephony
 *
 * noise.c - A low complexity audio noise generator, suitable for
 *           real time generation (current just approx AWGN)
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
 * $Id: noise.c,v 1.1 2005/10/10 19:42:25 steveu Exp $
 */

/*! \file */

#define	_ISOC9X_SOURCE	1
#define _ISOC99_SOURCE	1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>

#include "spandsp/dc_restore.h"
#include "spandsp/noise.h"

void noise_init(noise_state_t *s, int seed, int level, int class)
{
    double rms;

    s->rndnum = (uint32_t) seed;
    rms = 1.3108*32768.0*0.70711*pow(10.0, (level - 3.14)/20.0);
    s->rms = rms;
}
/*- End of function --------------------------------------------------------*/

int16_t noise(noise_state_t *s)
{
    int32_t val;
    int i;

    /* The central limit theorem says if you add a few random numbers together,
       the result starts to look Gaussian. Quanitities above 7 give little
       improvement. We use 7. */
    val = 0;
    for (i = 0;  i < 7;  i++)
    {
        s->rndnum = 1664525U*s->rndnum + 1013904223U;
        val += ((int32_t) s->rndnum) >> 19;
    }
    return saturate((val*s->rms) >> 13);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

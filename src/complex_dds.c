/*
 * SpanDSP - a series of DSP components for telephony
 *
 * complex_dds.c
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
 * $Id: complex_dds.c,v 1.4 2004/03/12 16:27:23 steveu Exp $
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "spandsp/telephony.h"
#include "spandsp/complex.h"
#include "spandsp/complex_dds.h"

#define SLENK	11
#define SINELEN (1 << SLENK)
#define TWO32	4.294967296e9	/* 2^32 */

static int sine_table_loaded = FALSE;
static float sine_table[SINELEN];

int32_t complex_dds_phase_step(double frequency)
{
    return  (int32_t) (frequency*65536.0*65536.0/SAMPLE_RATE);
}
/*- End of function --------------------------------------------------------*/

float real_dds(uint32_t *phase_acc, int32_t phase_rate)
{
    float amp;

    if (!sine_table_loaded)
    {
        /* Create the sine table on first use */
        int i;

        for (i = 0;  i < SINELEN;  i++)
            sine_table[i] = sin(2.0*M_PI*(float) i/(float) SINELEN);
        sine_table_loaded = TRUE;
    }
    amp = sine_table[*phase_acc >> (32 - SLENK)];
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/

/* This is a simple direct digital synthesis (DDS) function to generate complex
   sine waves. */
complex_t complex_dds(uint32_t *phase_acc, int32_t phase_rate)
{
    complex_t amp;

    if (!sine_table_loaded)
    {
        /* Create the sine table on first use */
        int i;

        for (i = 0;  i < SINELEN;  i++)
            sine_table[i] = sin(2.0*M_PI*(float) i/(float) SINELEN);
        sine_table_loaded = TRUE;
    }
    amp = complex_set(sine_table[*phase_acc >> (32 - SLENK)],
                      sine_table[(*phase_acc + (1 << 30)) >> (32 - SLENK)]);
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/

/* This is a simple direct digital synthesis (DDS) function to generate complex
   sine waves. */
complex_t complex_dds_mod(uint32_t *phase_acc, int32_t phase_rate, float scale, int32_t phase)
{
    complex_t amp;

    if (!sine_table_loaded)
    {
        /* Create the sine table on first use */
        int i;

        for (i = 0;  i < SINELEN;  i++)
            sine_table[i] = sin(2.0*M_PI*(float) i/(float) SINELEN);
        sine_table_loaded = TRUE;
    }
    amp = complex_set(sine_table[(*phase_acc + phase) >> (32 - SLENK)]*scale,
                      sine_table[(*phase_acc + phase + (1 << 30)) >> (32 - SLENK)]*scale);
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

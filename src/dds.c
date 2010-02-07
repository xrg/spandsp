/*
 * SpanDSP - a series of DSP components for telephony
 *
 * dds.c
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
 * $Id: dds.c,v 1.8 2005/08/31 19:27:52 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <math.h>

#include "spandsp/telephony.h"
#include "spandsp/dds.h"

/* In a A-law or u-law channel, and 128 step sine table is adequate to keep the spectral
   mess due to the DDS at a similar level to the spectral mess due to the A-law or u-law
   compression. */
#define SLENK	7
#define DDS_STEPS (1 << SLENK)
#define DDS_SHIFT (32 - 2 - SLENK)

static int sine_table_loaded = FALSE;
static int16_t sine_table[DDS_STEPS];

int32_t dds_phase_step(float frequency)
{
    return  (int32_t) (frequency*65536.0*65536.0/SAMPLE_RATE);
}
/*- End of function --------------------------------------------------------*/

int dds_scaling(float level)
{
    return (int) (pow(10.0, (level - 3.14)/20.0)*32767.0);
}
/*- End of function --------------------------------------------------------*/

int16_t dds_lookup(uint32_t phase)
{
    uint32_t step;
    int amp;

    if (!sine_table_loaded)
    {
        /* Create the sine table on first use */
        int i;
        int j;
        double d;

        for (i = 0;  i < DDS_STEPS;  i++)
        {
            d = sin((i + 0.5)*M_PI/(2.0*DDS_STEPS));
            j = (int) (d*32768.0 + 0.5);
            if (j > 32767)
                j = 32767;
            sine_table[i] = j;
        }
        sine_table_loaded = TRUE;
    }
    phase >>= DDS_SHIFT;
    step = phase & (DDS_STEPS - 1);
    if ((phase & DDS_STEPS))
        step = DDS_STEPS - step;
    amp = sine_table[step];
    if ((phase & (2*DDS_STEPS)))
    	amp = -amp;
    return  amp;
}
/*- End of function --------------------------------------------------------*/

/* This is a simple direct digital synthesis (DDS) function to generate sine
   waves. This version uses a 128 entry sin/cos table to cover one quadrant. */
int16_t dds_offset(uint32_t phase_acc, int32_t phase_offset)
{
    return dds_lookup(phase_acc + phase_offset);
}
/*- End of function --------------------------------------------------------*/

int16_t dds(uint32_t *phase_acc, int32_t phase_rate)
{
    int16_t amp;

    amp = dds_lookup(*phase_acc);
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/

int16_t dds_mod(uint32_t *phase_acc, int32_t phase_rate, int scale, int32_t phase)
{
    int16_t amp;

    amp = (dds_lookup(*phase_acc + phase)*scale) >> 15;
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/

icomplex_t dds_complex(uint32_t *phase_acc, int32_t phase_rate)
{
    icomplex_t amp;

    amp.re = dds_lookup(*phase_acc);
    amp.im = dds_lookup(*phase_acc + (1 << 30));
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/

icomplex_t dds_mod_complex(uint32_t *phase_acc, int32_t phase_rate, int scale, int32_t phase)
{
    icomplex_t amp;

    amp.re = (dds_lookup(*phase_acc + phase)*scale) >> 15;
    amp.im = (dds_lookup(*phase_acc + phase + (1 << 30))*scale) >> 15;
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

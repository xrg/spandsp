/*
 * SpanDSP - a series of DSP components for telephony
 *
 * ima_adpcm.c - Conversion routines between linear 16 bit PCM data and
 *	             IMA/DVI/Intel ADPCM format.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001, 2004 Steve Underwood
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
 * $Id: ima_adpcm.c,v 1.4 2005/11/25 14:51:59 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#include "spandsp/telephony.h"
#include "spandsp/ima_adpcm.h"

/*
 * Intel/DVI ADPCM coder/decoder.
 *
 * The algorithm for this coder was taken from the IMA Compatability Project
 * proceedings, Vol 2, Number 2; May 1992.
 */

/* Intel ADPCM step variation table */
static const int step_size[89] =
{
        7,     8,     9,   10,     11,    12,    13,    14,    16,    17,
       19,    21,    23,   25,     28,    31,    34,    37,    41,    45,
       50,    55,    60,   66,     73,    80,    88,    97,   107,   118,
      130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
      337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
      876,   963,  1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
     2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
     5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static const int step_adjustment[8] =
{
    -1, -1, -1, -1, 2, 4, 6, 8
};

static int16_t imaadpcm_decode(ima_adpcm_state_t *s, uint8_t adpcm)
{
    int16_t e;
    int16_t ss;
    int linear;

    /* e = (adpcm+0.5)*step/4 */

    ss = step_size[s->step_index];
    e = ss >> 3;
    if (adpcm & 0x01)
        e += (ss >> 2);
    /*endif*/
    if (adpcm & 0x02)
        e += (ss >> 1);
    /*endif*/
    if (adpcm & 0x04)
        e += ss;
    /*endif*/
    if (adpcm & 0x08)
        e = -e;
    /*endif*/
    linear = s->last + e;
    if (linear > 32767)
        linear = 32767;
    else if (linear < -32768)
        linear = -32768;
    /*endif*/
    s->last = linear;
    s->step_index += step_adjustment[adpcm & 0x07];
    if (s->step_index < 0)
        s->step_index = 0;
    else if (s->step_index > 88)
        s->step_index = 88;
    /*endif*/
    return linear;
}
/*- End of function --------------------------------------------------------*/

int imaadpcm_encode(ima_adpcm_state_t *state, int16_t linear)
{
    int e;
    int ss;
    int adpcm;
    int diff;
    int initial_e;

    ss = step_size[state->step_index];
    initial_e =
	e = linear - state->last;
	diff = ss >> 3;
    adpcm = (uint8_t) 0x00;
	if (e < 0)
    {
        adpcm = (uint8_t) 0x08;
        e = -e;
    }
    /*endif*/
	if (e >= ss)
    {
	    adpcm |= (uint8_t) 0x04;
	    e -= ss;
	}
    /*endif*/
	ss >>= 1;
	if (e >= ss)
    {
	    adpcm |= (uint8_t) 0x02;
	    e -= ss;
	}
    /*endif*/
	ss >>= 1;
	if (e >= ss)
    {
	    adpcm |= (uint8_t) 0x01;
	    e -= ss;
	}
    /*endif*/

	if (initial_e < 0)
        diff = -(diff - initial_e - e);
    else
        diff = diff + initial_e - e;
    /*endif*/
    diff += state->last;

	if (diff > 32767)
	    diff = 32767;
	else if (diff < -32768)
	    diff = -32768;
    /*endif*/
    state->last = diff;

	state->step_index += step_adjustment[adpcm & 0x07];
	if (state->step_index < 0)
        state->step_index = 0;
	else if (state->step_index > 88)
        state->step_index = 88;
    /*endif*/
    return adpcm;
}
/*- End of function --------------------------------------------------------*/

ima_adpcm_state_t *ima_adpcm_init(ima_adpcm_state_t *s)
{
    if (s == NULL)
    {
        if ((s = (ima_adpcm_state_t *) malloc(sizeof(*s))) == NULL)
        	return  NULL;
    }
    memset(s, 0, sizeof(*s));
    return  s;
}
/*- End of function --------------------------------------------------------*/

int ima_adpcm_release(ima_adpcm_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

int ima_adpcm_to_linear(ima_adpcm_state_t *s,
                        int16_t *amp,
                        const uint8_t *ima_data,
                        int ima_bytes)
{
    int i;
    int samples;

    samples = 0;
    for (i = 0;  i < ima_bytes;  i++)
    {
        amp[samples++] = imaadpcm_decode(s, ima_data[i] & 0xF);
        amp[samples++] = imaadpcm_decode(s, (ima_data[i] >> 4) & 0xF);
    }
    /*endwhile*/
    return samples;
}
/*- End of function --------------------------------------------------------*/

int ima_linear_to_adpcm(ima_adpcm_state_t *s,
                        uint8_t *ima_data,
                        const int16_t *amp,
                        int samples)
{
    int n;
    int bytes;

    bytes = 0;
    for (n = 0;  n < samples;  n++)
    {
        s->ima_byte = (s->ima_byte >> 4) | (imaadpcm_encode(s, amp[n]) << 4);
        if ((s->mark++ & 1))
            ima_data[bytes++] = s->ima_byte;
        /*endif*/
    }
    /*endfor*/
    return  bytes;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

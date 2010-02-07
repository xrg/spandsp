/*
 * SpanDSP - a series of DSP components for telephony
 *
 * okiadpcm.c - Conversion routines between linear 16 bit PCM data and
 *		OKI (Dialogic) ADPCM format.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
 *
 * Based on a bit from here, a bit from there, eye of toad,
 * ear of bat, etc - plus, of course, my own 2 cents.
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
 * $Id: oki_adpcm.c,v 1.5 2004/03/15 13:17:35 steveu Exp $
 */

/*! \file */

#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "spandsp/telephony.h"
#include "spandsp/oki_adpcm.h"

/* Routines to convert 12 bit linear samples to the Oki ADPCM coding format,
   widely used in CTI because Dialogic use it.

/* These can be found in "PC Telephony - The complete guide to
   designing, building and programming systems using Dialogic
   and Related Hardware" by Bob Edgar. pg 272-276. */

/* The 24kbps and 32kbps ADPCM coders used by Dialogic vary only
   in the sampling rate of the input data. It is 8k samples per second
   for the 32kbps format and 6k samples per second for the 24kbps
   format. Most of the Dialogic cards perform a rate conversion
   between 8k and 6k samples per second, as they must always interface
   to the PSTN at 8k samples per second. Currently these routines do no
   rate conversion, so they are limited to the 32kbps format */

static int16_t step_size[49] =
{
      16,   17,   19,   21,   23,   25,   28,   31,
      34,   37,   41,   45,   50,   55,   60,   66,
      73,   80,   88,   97,  107,  118,  130,  143,
     157,  173,  190,  209,  230,  253,  279,  307,
     337,  371,  408,  449,  494,  544,  598,  658,
     724,  796,  876,  963, 1060, 1166, 1282, 1408,
    1552
};

static int16_t step_adjustment[8] =
{
    -1, -1, -1, -1, 2, 4, 6, 8
};

static int16_t okiadpcm_decode (oki_adpcm_state_t *oki, uint8_t adpcm)
{
    int16_t E;
    int16_t SS;
    int16_t linear;

    /* Doing the next part as follows:
     *
     * x = adpcm & 0x07;
     * E = (step_size[oki->step_index]*(x + x + 1)) >> 3;
     * 
     * Seems an obvious improvement on a modern machine, but remember
     * the truncation errors do not come out the same. It would
     * not, therefore, be an exact match for what this code is doing.
     *
     * Just what the Dialogic card does I do not know!
     */

    SS = step_size[oki->step_index];
    E = SS >> 3;
    if (adpcm & 0x01)
        E += (SS >> 2);
    /*endif*/
    if (adpcm & 0x02)
        E += (SS >> 1);
    /*endif*/
    if (adpcm & 0x04)
        E += SS;
    /*endif*/
    if (adpcm & 0x08)
        E = -E;
    /*endif*/
    linear = oki->last + E;

    /* Clip the values to +/- 2^11 (supposed to be 12 bits) */
    if (linear > 2047)
        linear = 2047;
    /*endif*/
    if (linear < -2048)
        linear = -2048;
    /*endif*/

    oki->last = linear;
    oki->step_index += step_adjustment[adpcm & 0x07];
    if (oki->step_index < 0)
        oki->step_index = 0;
    /*endif*/
    if (oki->step_index > 48)
        oki->step_index = 48;
    /*endif*/
    return  linear;
}
/*- End of function --------------------------------------------------------*/

static uint8_t okiadpcm_encode (oki_adpcm_state_t *oki, int16_t linear)
{
    int16_t E;
    int16_t SS;
    uint8_t adpcm;

    SS = step_size[oki->step_index];
    E = linear - oki->last;
    adpcm = (uint8_t) 0x00;
    if (E < 0)
    {
        adpcm = (uint8_t) 0x08;
        E = -E;
    }
    /*endif*/
    if (E >= SS)
    {
        adpcm |= (uint8_t) 0x04;
        E -= SS;
    }
    /*endif*/
    if (E >= (SS >> 1))
    {
        adpcm |= (uint8_t) 0x02;
        E -= SS;
    }
    /*endif*/
    if (E >= (SS >> 2))
        adpcm |= (uint8_t) 0x01;
    /*endif*/

    /* Use the decoder to set the estimate of the last sample. */
    /* It also will adjust the step_index for us. */
    oki->last = okiadpcm_decode (oki, adpcm);
    return  adpcm;
}
/*- End of function --------------------------------------------------------*/

oki_adpcm_state_t *oki_adpcm_create(void)
{
    oki_adpcm_state_t *oki;
    
    oki = (oki_adpcm_state_t *) malloc(sizeof(*oki));
    if (oki == NULL)
    	return  NULL;
    memset(oki, 0, sizeof(*oki));
    oki->last = 0;
    oki->step_index = 0;
    oki->left_over_used = FALSE;
    oki->left_over_sample = 0;
    return  oki;
}
/*- End of function --------------------------------------------------------*/

void oki_adpcm_free(oki_adpcm_state_t *oki)
{
    free(oki);
}
/*- End of function --------------------------------------------------------*/

int oki_adpcm_to_linear(oki_adpcm_state_t *oki,
                        int16_t *amp,
                        const uint8_t oki_data[],
                        int oki_bytes)
{
    int i;
    int j;

    j = 0;
    for (i = 0;  i < oki_bytes;  i++)
    {
        amp[j++] = okiadpcm_decode (oki, oki_data[i] >> 4) << 4;
        amp[j++] = okiadpcm_decode (oki, oki_data[i] & 0xF) << 4;
    }
    /*endwhile*/
    return  j;
}
/*- End of function --------------------------------------------------------*/

int linear_to_oki_adpcm(oki_adpcm_state_t *oki,
                        uint8_t oki_data[],
                        const int16_t *amp,
                        int pcm_samples)
{
    uint8_t sample;
    int i;
    int j;

    i = 0;
    j = 0;
    if (oki->left_over_used  &&  pcm_samples > 0)
    {
        sample = okiadpcm_encode (oki, oki->left_over_sample >> 4);
        oki_data[j++] = (oki->left_over_sample << 4) | okiadpcm_encode (oki, amp[i++] >> 4);
    }
    /*endif*/
    if (((pcm_samples - i) & 1))
    {
    	pcm_samples--;
        oki->left_over_used = TRUE;
        oki->left_over_sample = amp[pcm_samples];
    }
    else
    {
        oki->left_over_used = FALSE;
    }
    /*endif*/
    while (i < pcm_samples)
    {
        sample = okiadpcm_encode (oki, amp[i++] >> 4);
        oki_data[j++] = (sample << 4) | okiadpcm_encode (oki, amp[i++] >> 4);
    }
    /*endwhile*/
    return  j;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

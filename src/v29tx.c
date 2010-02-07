/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v29tx.c - ITU V.29 modem transmit part
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
 * $Id: v29tx.c,v 1.14 2004/03/12 16:27:24 steveu Exp $
 */

/*! \file */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "spandsp/telephony.h"
#include "spandsp/complex.h"
#include "spandsp/complex_dds.h"
#include "spandsp/power_meter.h"

#include "spandsp/v29tx.h"

/* Segments of the training sequence */
#define V29_TRAINING_SEG_1      0
#define V29_TRAINING_SEG_2      48
#define V29_TRAINING_SEG_3      (V29_TRAINING_SEG_2 + 128)
#define V29_TRAINING_SEG_4      (V29_TRAINING_SEG_3 + 384)
#define V29_TRAINING_END        (V29_TRAINING_SEG_4 + 48)

#define PULSESHAPER_GAIN        3.335442363e+00
static const float pulseshaper[] =
{
    /* Raised root cosine pulse shaping; Beta = 0.5; 4 symbols either
       side of the centre. Only one side of the filter is here, as the
       other half is just a mirror image. */
    -0.0092658380,
    +0.0048917854,
    +0.0167934357,
    +0.0030315309,
    -0.0185987362,
    -0.0053888117,
    +0.0355135142,
    +0.0267182228,
    -0.0750263963,
    -0.1612000030,
    -0.0269213894,
    +0.4006012745,
    +0.9082627339,
    +1.1366197172
};

static complex_t training_get(v29_tx_state_t *s)
{
    static const complex_t abab[6] =
    {
        {-3.0,  0.0},   /* 180deg low 9600 */
        { 3.0, -3.0},   /* 315deg high     */
        {-3.0,  0.0},   /* 180deg low 7200 */
        { 1.0, -1.0},   /* 315deg low      */
        {-3.0,  0.0},   /* 180deg low 4800 */
        { 0.0, -3.0}    /* 270deg low      */
    };
    static const complex_t cdcd[6] =
    {
        { 3.0,  0.0},   /*   0deg low 9600 */
        {-3.0,  3.0},   /* 135deg high     */
        { 3.0,  0.0},   /*   0deg low 7200 */
        {-1.0,  1.0},   /* 135deg low      */
        { 3.0,  0.0},   /*   0deg low 4800 */
        { 0.0,  3.0}    /*  90deg low      */
    };
    complex_t z;
    int bit;

    /* V.29 training sequence */
    if (s->training_step < V29_TRAINING_SEG_2)
    {
    	/* Segment 1: silence */
        z = complex_set(0.0, 0.0);
    }
    else if (s->training_step < V29_TRAINING_SEG_3)
    {
        /* Segment 2: ABAB... */
        z = abab[(s->training_step & 1) + s->training_offset];
    }
    else
    {
        /* Segment 3: CDCD... */
        /* Apply the 1 + x^-6 + x^-7 scrambler */
        bit = s->training_scramble_reg & 1;
        z = cdcd[bit + s->training_offset];
        s->training_scramble_reg >>= 1;
        s->training_scramble_reg |= (((bit ^ s->training_scramble_reg) & 1) << 6);
    }
    s->training_step++;
    return z;
}
/*- End of function --------------------------------------------------------*/

static inline int scramble(v29_tx_state_t *s, int in_bit)
{
    int out_bit;

    out_bit = (in_bit ^ (s->scramble_reg >> 17) ^ (s->scramble_reg >> 22)) & 1;
    s->scramble_reg = (s->scramble_reg << 1) | out_bit;
    return out_bit;
}
/*- End of function --------------------------------------------------------*/

static complex_t getbaud(v29_tx_state_t *s)
{
    static const int phase_steps_9600[8] =
    {
        1, 0, 2, 3, 6, 7, 5, 4
    };
    static const int phase_steps_4800[4] =
    {
        0, 2, 6, 4
    };
    static const complex_t constellation[16] =
    {
        { 3.0,  0.0},   /*   0deg low  */
        { 1.0,  1.0},   /*  45deg low  */
        { 0.0,  3.0},   /*  90deg low  */
        {-1.0,  1.0},   /* 135deg low  */
        {-3.0,  0.0},   /* 180deg low  */
        {-1.0, -1.0},   /* 225deg low  */
        { 0.0, -3.0},   /* 270deg low  */
        { 1.0, -1.0},   /* 315deg low  */
        { 5.0,  0.0},   /*   0deg high */
        { 3.0,  3.0},   /*  45deg high */
        { 0.0,  5.0},   /*  90deg high */
        {-3.0,  3.0},   /* 135deg high */
        {-5.0,  0.0},   /* 180deg high */
        {-3.0, -3.0},   /* 225deg high */
        { 0.0, -5.0},   /* 270deg high */
        { 3.0, -3.0}    /* 315deg high */
    };
    int i;
    int bits;
    int amp;
    complex_t z;

    if (s->in_training)
    {
       	/* Send the training sequence */
        if (s->training_step < V29_TRAINING_SEG_4)
        {
        	z = training_get(s);
        }
        else
        {
            /* 9600bps uses the full constellation
               7200bps uses the first half only (i.e no amplitude modulation)
               4800bps uses the smaller constellation. */
            amp = 0;
            /* We only use an amplitude bit at 9600bps */
            if (s->bit_rate == 9600  &&  scramble(s, 1))
                amp = 8;
            /*endif*/
            if (s->bit_rate == 4800)
            {
                bits = scramble(s, 1);
                bits = (bits << 1) | scramble(s, 1);
                bits = phase_steps_4800[bits];
            }
            else
            {
                bits = scramble(s, 1);
                bits = (bits << 1) | scramble(s, 1);
                bits = (bits << 1) | scramble(s, 1);
                bits = phase_steps_9600[bits];
            }
            s->constellation_state = (s->constellation_state + bits) & 7;
            z = constellation[s->constellation_state | amp];
            if (++s->training_step >= V29_TRAINING_END)
                s->in_training = FALSE;
        }
    }
    else
    {
        /* 9600bps uses the full constellation
           7200bps uses the first half only (i.e no amplitude modulation)
           4800bps uses the smaller constellation. */
        amp = 0;
        /* We only use an amplitude bit at 9600bps */
        if (s->bit_rate == 9600  &&  scramble(s, s->get_bit(s->user_data)))
            amp = 8;
        /*endif*/
        bits = 0;
        if (s->bit_rate == 4800)
        {
            bits = scramble(s, s->get_bit(s->user_data));
            bits = (bits << 1) | scramble(s, s->get_bit(s->user_data));
            bits = phase_steps_4800[bits];
        }
        else
        {
            bits = scramble(s, s->get_bit(s->user_data));
            bits = (bits << 1) | scramble(s, s->get_bit(s->user_data));
            bits = (bits << 1) | scramble(s, s->get_bit(s->user_data));
            bits = phase_steps_9600[bits];
        }
        s->constellation_state = (s->constellation_state + bits) & 7;
        z = constellation[s->constellation_state | amp];
    }
    return  z;
}
/*- End of function --------------------------------------------------------*/

int v29_tx(v29_tx_state_t *s, int16_t *amp, int len)
{
    complex_t x;
    complex_t cz;
    float y;
    int i;
    int sample;
    static const float weights[4] = {0.0, 0.75, 0.25, 0.0};

    for (sample = 0;  sample < len;  sample++)
    {
        if ((s->baud_phase += 3) > 10)
        {
            s->baud_phase -= 10;
            x = getbaud(s);
            /* Use a weighted value for the first sample of the baud to correct
               for a baud not being an integral number of samples long. */
            y = x.re;
            s->rrc_filter[s->rrc_filter_step].re =
            s->rrc_filter[s->rrc_filter_step + V29TX_FILTER_STEPS].re = y - (y - s->current_point.re)*weights[s->baud_phase];
            y = x.im;
            s->rrc_filter[s->rrc_filter_step].im =
            s->rrc_filter[s->rrc_filter_step + V29TX_FILTER_STEPS].im = y - (y - s->current_point.im)*weights[s->baud_phase];
            s->current_point = x;
        }
        else
        {
            s->rrc_filter[s->rrc_filter_step] =
            s->rrc_filter[s->rrc_filter_step + V29TX_FILTER_STEPS] = s->current_point;
        }
        if (++s->rrc_filter_step >= V29TX_FILTER_STEPS)
            s->rrc_filter_step = 0;
    	/* Root raised cosine pulse shaping at baseband */
        x.re = pulseshaper[V29TX_FILTER_STEPS >> 1]*s->rrc_filter[(V29TX_FILTER_STEPS >> 1) + s->rrc_filter_step].re;
        x.im = pulseshaper[V29TX_FILTER_STEPS >> 1]*s->rrc_filter[(V29TX_FILTER_STEPS >> 1) + s->rrc_filter_step].im;
        for (i = 0;  i < (V29TX_FILTER_STEPS >> 1);  i++)
        {
            x.re += pulseshaper[i]*(s->rrc_filter[i + s->rrc_filter_step].re + s->rrc_filter[V29TX_FILTER_STEPS - 1 - i + s->rrc_filter_step].re);
            x.im += pulseshaper[i]*(s->rrc_filter[i + s->rrc_filter_step].im + s->rrc_filter[V29TX_FILTER_STEPS - 1 - i + s->rrc_filter_step].im);
        }
        /* Now create and modulate the carrier */
        cz = complex_dds(&(s->carrier_phase), s->carrier_phase_rate);
        amp[sample] = (int16_t) ((x.re*cz.re + x.im*cz.im)*32768.0/(PULSESHAPER_GAIN*5.0*4.0));
    }
    return sample;
}
/*- End of function --------------------------------------------------------*/

void v29_tx_restart(v29_tx_state_t *s, int bit_rate)
{
    memset(s->rrc_filter, 0, sizeof(s->rrc_filter));
    s->rrc_filter_step = 0;
    s->carrier_phase = 0;
    s->scramble_reg = 0;
    s->training_scramble_reg = 0x2A;
    s->constellation_state = 0;
    s->training_step = 0;
    s->bit_rate = bit_rate;
    switch (s->bit_rate)
    {
    case 4800:
        s->training_offset = 4;
        break;
    case 7200:
        s->training_offset = 2;
        break;
    default:
        s->training_offset = 0;
        break;
    }
    s->in_training = TRUE;
}
/*- End of function --------------------------------------------------------*/

void v29_tx_init(v29_tx_state_t *s, int rate, get_bit_func_t get_bit, void *user_data)
{
    memset(s, 0, sizeof(*s));
    s->get_bit = get_bit;
    s->user_data = user_data;
    s->carrier_phase_rate = complex_dds_phase_step(1700.0);
    v29_tx_restart(s, rate);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

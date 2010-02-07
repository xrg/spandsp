/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v27ter_x.c - ITU V.27ter modem transmit part
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
 * $Id: v27ter_tx.c,v 1.12 2004/03/12 16:27:24 steveu Exp $
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

#include "spandsp/v27ter_tx.h"

/* Segments of the training sequence */
#define V27_TRAINING_SEG_1      0
#define V27_TRAINING_SEG_2      320
#define V27_TRAINING_SEG_3      (V27_TRAINING_SEG_2 + 32)
#define V27_TRAINING_SEG_4      (V27_TRAINING_SEG_3 + 50)
#define V27_TRAINING_SEG_5      (V27_TRAINING_SEG_4 + 1074)
#define V27_TRAINING_END        (V27_TRAINING_SEG_5 + 8)

#define PULSESHAPER_4800_GAIN       4.998714163
#define V27TX_4800_FILTER_STEPS     41

static const float pulseshaper_4800[] =
{
    /* Raised root cosine pulse shaping; Beta = 0.5; 4 symbols either
       side of the centre. Only one side of the filter is here, as the
       other half is just a mirror image. */
    -0.0101052019,
    -0.0061365979,
    +0.0048918464,
    +0.0150576434,
    +0.0152563286,
    +0.0030314952,
    -0.0134032156,
    -0.0194477281,
    -0.0053888047,
    +0.0228163350,
    +0.0424414442,
    +0.0267182476,
    -0.0339462085,
    -0.1154099010,
    -0.1612000503,
    -0.1061032862,
    +0.0875788553,
    +0.4006013374,
    +0.7528286821,
    +1.0309660372,
    +1.1366196464
};

#define PULSESHAPER_2400_GAIN       6.681678162
#define V27TX_2400_FILTER_STEPS     53

static const float pulseshaper_2400[] =
{
    /* Raised root cosine pulse shaping; Beta = 0.5; 4 symbols either
       side of the centre. Only one side of the filter is here, as the
       other half is just a mirror image. */
    -0.0092635830,
    -0.0038205730,
    +0.0048910642,
    +0.0131488844,
    +0.0167909641,
    +0.0132242921,
    +0.0030314900,
    -0.0096792815,
    -0.0185962777,
    -0.0179062671,
    -0.0053880131,
    +0.0154672635,
    +0.0355112972,
    +0.0426593031,
    +0.0267167304,
    -0.0152421430,
    -0.0750246487,
    -0.1328587996,
    -0.1611979750,
    -0.1322087125,
    -0.0269225112,
    +0.1568404105,
    +0.4005989004,
    +0.6671440057,
    +0.9082631250,
    +1.0763490262,
    +1.1366222191
};

static inline int scramble(v27ter_tx_state_t *s, int in_bit)
{
    int out_bit;
    int test;

    out_bit = (in_bit ^ (s->scramble_reg >> 5) ^ (s->scramble_reg >> 6)) & 1;
    if (s->scrambler_pattern_count >= 33)
    {
        out_bit ^= 1;
        s->scrambler_pattern_count = 0;
    }
    else
    {
        if ((((s->scramble_reg >> 7) ^ out_bit) & ((s->scramble_reg >> 8) ^ out_bit) & ((s->scramble_reg >> 11) ^ out_bit) & 1))
            s->scrambler_pattern_count = 0;
        else
            s->scrambler_pattern_count++;
    }
    s->scramble_reg = (s->scramble_reg << 1) | out_bit;
    return out_bit;
}
/*- End of function --------------------------------------------------------*/

static complex_t getbaud(v27ter_tx_state_t *s)
{
    static const int phase_steps_4800[8] =
    {
        1, 0, 2, 3, 6, 7, 5, 4
    };
    static const int phase_steps_2400[4] =
    {
        0, 2, 6, 4
    };
    static const complex_t constellation[8] =
    {
        { 1.414,  0.0},     /*   0deg */
        { 1.0,    1.0},     /*  45deg */
        { 0.0,    1.414},   /*  90deg */
        {-1.0,    1.0},     /* 135deg */
        {-1.414,  0.0},     /* 180deg */
        {-1.0,   -1.0},     /* 225deg */
        { 0.0,   -1.414},   /* 270deg */
        { 1.0,   -1.0}      /* 315deg */
    };
    int i;
    int bits;
    complex_t z;

    if (s->in_training)
    {
       	/* Send the training sequence */
        if (s->training_step < V27_TRAINING_SEG_2)
        {
            /* Segment 1: unmodulated carrier (talker echo protection) */
            z = constellation[0];
        }
        else if (s->training_step < V27_TRAINING_SEG_3)
        {
            /* Segment 2: silence */
            z = complex_set(0.0, 0.0);
        }
        else if (s->training_step < V27_TRAINING_SEG_4)
        {
            /* Segment 3: Regular reversals... */
            s->constellation_state = (s->constellation_state + 4) & 7;
            z = constellation[s->constellation_state];
        }
        else if (s->training_step < V27_TRAINING_SEG_5)
        {
            /* Segment 4: Scrambled reversals... */
            /* Apply the 1 + x^-6 + x^-7 scrambler. We want every third
               bit from the scrambler. */
            bits = scramble(s, 1) << 2;
            scramble(s, 1);
            scramble(s, 1);
            s->constellation_state = (s->constellation_state + bits) & 7;
            z = constellation[s->constellation_state];
        }
        else
        {
            /* 4800bps uses 8 phases. 2400bps uses 4 phases. */
            if (s->bit_rate == 4800)
            {
                bits = scramble(s, 1);
                bits = (bits << 1) | scramble(s, 1);
                bits = (bits << 1) | scramble(s, 1);
                bits = phase_steps_4800[bits];
            }
            else
            {
                bits = scramble(s, 1);
                bits = (bits << 1) | scramble(s, 1);
                bits = phase_steps_2400[bits];
            }
            s->constellation_state = (s->constellation_state + bits) & 7;
            z = constellation[s->constellation_state];
            if (s->training_step >= V27_TRAINING_END - 1)
                s->in_training = FALSE;
        }
        s->training_step++;
    }
    else
    {
        /* 4800bps uses 8 phases. 2400bps uses 4 phases. */
        if (s->bit_rate == 4800)
        {
            bits = scramble(s, s->get_bit(s->user_data));
            bits = (bits << 1) | scramble(s, s->get_bit(s->user_data));
            bits = (bits << 1) | scramble(s, s->get_bit(s->user_data));
            bits = phase_steps_4800[bits];
        }
        else
        {
            bits = scramble(s, s->get_bit(s->user_data));
            bits = (bits << 1) | scramble(s, s->get_bit(s->user_data));
            bits = phase_steps_2400[bits];
        }
        s->constellation_state = (s->constellation_state + bits) & 7;
        z = constellation[s->constellation_state];
    }
    return  z;
}
/*- End of function --------------------------------------------------------*/

int v27ter_tx(v27ter_tx_state_t *s, int16_t *amp, int len)
{
    complex_t x;
    complex_t cz;
    float y;
    int i;
    int sample;
    static const float weights[4] = {0.0, 0.75, 0.25, 0.0};

    if (s->bit_rate == 4800)
    {
        for (sample = 0;  sample < len;  sample++)
        {
            if (++s->baud_phase >= 5)
            {
                s->baud_phase -= 5;
                s->current_point = getbaud(s);
            }
            s->rrc_filter[s->rrc_filter_step] =
            s->rrc_filter[s->rrc_filter_step + V27TX_4800_FILTER_STEPS] = s->current_point;
            if (++s->rrc_filter_step >= V27TX_4800_FILTER_STEPS)
                s->rrc_filter_step = 0;
            /* Root raised cosine pulse shaping at baseband */
            x.re = pulseshaper_4800[V27TX_4800_FILTER_STEPS >> 1]*s->rrc_filter[(V27TX_4800_FILTER_STEPS >> 1) + s->rrc_filter_step].re;
            x.im = pulseshaper_4800[V27TX_4800_FILTER_STEPS >> 1]*s->rrc_filter[(V27TX_4800_FILTER_STEPS >> 1) + s->rrc_filter_step].im;
            for (i = 0;  i < (V27TX_4800_FILTER_STEPS >> 1);  i++)
            {
                x.re += pulseshaper_4800[i]*(s->rrc_filter[i + s->rrc_filter_step].re + s->rrc_filter[V27TX_4800_FILTER_STEPS - 1 - i + s->rrc_filter_step].re);
                x.im += pulseshaper_4800[i]*(s->rrc_filter[i + s->rrc_filter_step].im + s->rrc_filter[V27TX_4800_FILTER_STEPS - 1 - i + s->rrc_filter_step].im);
            }
            /* Now create and modulate the carrier */
            cz = complex_dds(&(s->carrier_phase), s->carrier_phase_rate);
            amp[sample] = (int16_t) ((x.re*cz.re + x.im*cz.im)*32768.0/(PULSESHAPER_4800_GAIN*1.414*4.0));
        }
    }
    else
    {
        for (sample = 0;  sample < len;  sample++)
        {
            if ((s->baud_phase += 3) > 20)
            {
                s->baud_phase -= 20;
                x = getbaud(s);
                /* Use a weighted value for the first sample of the baud to correct
                   for a baud not being an integral number of samples long */
                y = x.re;
                s->rrc_filter[s->rrc_filter_step].re =
                s->rrc_filter[s->rrc_filter_step + V27TX_2400_FILTER_STEPS].re = y - (y - s->current_point.re)*weights[s->baud_phase];
                y = x.im;
                s->rrc_filter[s->rrc_filter_step].im =
                s->rrc_filter[s->rrc_filter_step + V27TX_2400_FILTER_STEPS].im = y - (y - s->current_point.im)*weights[s->baud_phase];
                s->current_point = x;
            }
            else
            {
                s->rrc_filter[s->rrc_filter_step] =
                s->rrc_filter[s->rrc_filter_step + V27TX_2400_FILTER_STEPS] = s->current_point;
            }
            if (++s->rrc_filter_step >= V27TX_2400_FILTER_STEPS)
                s->rrc_filter_step = 0;
            /* Root raised cosine pulse shaping at baseband */
            x.re = pulseshaper_2400[V27TX_2400_FILTER_STEPS >> 1]*s->rrc_filter[(V27TX_2400_FILTER_STEPS >> 1) + s->rrc_filter_step].re;
            x.im = pulseshaper_2400[V27TX_2400_FILTER_STEPS >> 1]*s->rrc_filter[(V27TX_2400_FILTER_STEPS >> 1) + s->rrc_filter_step].im;
            for (i = 0;  i < (V27TX_2400_FILTER_STEPS >> 1);  i++)
            {
                x.re += pulseshaper_2400[i]*(s->rrc_filter[i + s->rrc_filter_step].re + s->rrc_filter[V27TX_2400_FILTER_STEPS - 1 - i + s->rrc_filter_step].re);
                x.im += pulseshaper_2400[i]*(s->rrc_filter[i + s->rrc_filter_step].im + s->rrc_filter[V27TX_2400_FILTER_STEPS - 1 - i + s->rrc_filter_step].im);
            }
            /* Now create and modulate the carrier */
            cz = complex_dds(&(s->carrier_phase), s->carrier_phase_rate);
            amp[sample] = (int16_t) ((x.re*cz.re + x.im*cz.im)*32768.0/(PULSESHAPER_2400_GAIN*1.414*4.0));
        }
    }
    return sample;
}
/*- End of function --------------------------------------------------------*/

void v27ter_tx_restart(v27ter_tx_state_t *s, int bit_rate)
{
    memset(s->rrc_filter, 0, sizeof(s->rrc_filter));
    s->rrc_filter_step = 0;
    s->carrier_phase = 0;
    s->scramble_reg = 0x3C;
    s->constellation_state = 0;
    s->training_step = 0;
    s->in_training = TRUE;
    s->bit_rate = bit_rate;
}
/*- End of function --------------------------------------------------------*/

void v27ter_tx_init(v27ter_tx_state_t *s, int rate, get_bit_func_t get_bit, void *user_data)
{
    memset(s, 0, sizeof(*s));
    s->get_bit = get_bit;
    s->user_data = user_data;
    s->carrier_phase_rate = complex_dds_phase_step(1800.0);
    v27ter_tx_restart(s, rate);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

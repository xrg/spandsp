/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v27ter_tx.c - ITU V.27ter modem transmit part
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
 * $Id: v27ter_tx.c,v 1.22 2005/08/31 19:27:53 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/power_meter.h"

#include "spandsp/v27ter_tx.h"

/* Segments of the training sequence */
/* V.27ter defines a long and a short sequence. FAX doesn't use the
   short sequence, so it is not implemented here. */
#define V27_TRAINING_TEP_LEN        320
#define V27_TRAINING_SEG_2          0
#define V27_TRAINING_SEG_3          (V27_TRAINING_SEG_2 + 32)
#define V27_TRAINING_SEG_4          (V27_TRAINING_SEG_3 + 50)
#define V27_TRAINING_SEG_5          (V27_TRAINING_SEG_4 + 1074)
#define V27_TRAINING_END            (V27_TRAINING_SEG_5 + 8)
#define V27_TRAINING_SHUTDOWN_END   (V27_TRAINING_END + 10)

static int fake_get_bit(void *user_data)
{
    return 1;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int scramble(v27ter_tx_state_t *s, int in_bit)
{
    int out_bit;
    int test;

    /* This scrambler is really quite messy to implement. There seems to be no efficient shortcut */
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

static __inline__ int get_scrambled_bit(v27ter_tx_state_t *s)
{
    int bit;
    
    bit = s->current_get_bit(s->user_data);
    if ((bit & 2))
    {
        /* End of real data. Switch to the fake get_bit routine, until we
           have shut down completely. */
        s->current_get_bit = fake_get_bit;
        s->in_training = TRUE;
        bit = 1;
    }
    return scramble(s, bit);
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
        if (s->tep_step)
        {
            s->tep_step--;
            return constellation[0];
        }
        if (++s->training_step <= V27_TRAINING_SEG_5)
        {
            if (s->training_step <= V27_TRAINING_SEG_2)
            {
                /* Segment 1: Unmodulated carrier (talker echo protection) */
                return constellation[0];
            }
            if (s->training_step <= V27_TRAINING_SEG_3)
            {
                /* Segment 2: Silence */
                return complex_set(0.0, 0.0);
            }
            if (s->training_step <= V27_TRAINING_SEG_4)
            {
                /* Segment 3: Regular reversals... */
                s->constellation_state = (s->constellation_state + 4) & 7;
                return constellation[s->constellation_state];
            }
            /* Segment 4: Scrambled reversals... */
            /* Apply the 1 + x^-6 + x^-7 scrambler. We want every third
               bit from the scrambler. */
            bits = get_scrambled_bit(s) << 2;
            get_scrambled_bit(s);
            get_scrambled_bit(s);
            s->constellation_state = (s->constellation_state + bits) & 7;
            return constellation[s->constellation_state];
        }
        /* We should be in the block of test ones, or shutdown ones, if we get here. */
        /* There is no graceful shutdown procedure defined for V.27ter. Just
           send some ones, to ensure we get the real data bits through, even
           with bad ISI. */
        if (s->training_step == V27_TRAINING_END + 1)
        {
            /* End of the last segment - segment 5: All ones */
            /* Switch from the fake get_bit routine, to the user supplied real
               one, and we are up and running. */
            s->current_get_bit = s->get_bit;
            s->in_training = FALSE;
        }
    }
    /* 4800bps uses 8 phases. 2400bps uses 4 phases. */
    if (s->bit_rate == 4800)
    {
        bits = get_scrambled_bit(s);
        bits = (bits << 1) | get_scrambled_bit(s);
        bits = (bits << 1) | get_scrambled_bit(s);
        bits = phase_steps_4800[bits];
    }
    else
    {
        bits = get_scrambled_bit(s);
        bits = (bits << 1) | get_scrambled_bit(s);
        bits = phase_steps_2400[bits];
    }
    s->constellation_state = (s->constellation_state + bits) & 7;
    return constellation[s->constellation_state];
}
/*- End of function --------------------------------------------------------*/

int v27ter_tx(v27ter_tx_state_t *s, int16_t *amp, int len)
{
    complex_t x;
    complex_t z;
    int i;
    int sample;
    static const float weights[4] = {0.0, 0.68, 0.32, 0.0};

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

    if (s->training_step >= V27_TRAINING_SHUTDOWN_END)
    {
        /* Once we have sent the shutdown symbols, we stop sending completely. */
        return 0;
    }
    /* The symbol rates for the two bit rates are different. This makes it difficult to
       merge both generation procedures into a single efficient loop. We do not bother
       trying. We use two independent loops, filter coefficients, etc. */
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
            z = dds_complexf(&(s->carrier_phase), s->carrier_phase_rate);
            amp[sample] = (int16_t) ((x.re*z.re + x.im*z.im)*s->gain_4800);
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
                s->rrc_filter[s->rrc_filter_step].re =
                s->rrc_filter[s->rrc_filter_step + V27TX_2400_FILTER_STEPS].re = x.re - (x.re - s->current_point.re)*weights[s->baud_phase];
                s->rrc_filter[s->rrc_filter_step].im =
                s->rrc_filter[s->rrc_filter_step + V27TX_2400_FILTER_STEPS].im = x.im - (x.im - s->current_point.im)*weights[s->baud_phase];
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
            z = dds_complexf(&(s->carrier_phase), s->carrier_phase_rate);
            amp[sample] = (int16_t) ((x.re*z.re + x.im*z.im)*s->gain_2400);
        }
    }
    return sample;
}
/*- End of function --------------------------------------------------------*/

void v27ter_tx_power(v27ter_tx_state_t *s, float power)
{
    float l;

    l = 1.6*pow(10.0, (power - 3.14)/20.0);
    s->gain_2400 = l*32768.0/(PULSESHAPER_2400_GAIN*1.414);
    s->gain_4800 = l*32768.0/(PULSESHAPER_4800_GAIN*1.414);
}
/*- End of function --------------------------------------------------------*/

void v27ter_tx_set_get_bit(v27ter_tx_state_t *s, get_bit_func_t get_bit, void *user_data)
{
    if (s->get_bit == s->current_get_bit)
        s->current_get_bit = get_bit;
    s->get_bit = get_bit;
    s->user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

int v27ter_tx_restart(v27ter_tx_state_t *s, int rate, int tep)
{
    if (rate != 4800  &&  rate != 2400)
        return -1;
    s->bit_rate = rate;
    memset(s->rrc_filter, 0, sizeof(s->rrc_filter));
    s->rrc_filter_step = 0;
    s->current_point = complex_set(0.0, 0.0);
    s->scramble_reg = 0x3C;
    s->scrambler_pattern_count = 0;
    s->in_training = TRUE;
    s->tep_step = (tep) ?  V27_TRAINING_TEP_LEN  :  0;
    s->training_step = 0;
    s->carrier_phase = 0;
    s->baud_phase = 0;
    s->constellation_state = 0;
    s->current_get_bit = fake_get_bit;
    return 0;
}
/*- End of function --------------------------------------------------------*/

void v27ter_tx_init(v27ter_tx_state_t *s, int rate, int tep, get_bit_func_t get_bit, void *user_data)
{
    memset(s, 0, sizeof(*s));
    s->get_bit = get_bit;
    s->user_data = user_data;
    s->carrier_phase_rate = dds_phase_stepf(1800.0);
    v27ter_tx_power(s, -12.0);
    v27ter_tx_restart(s, rate, tep);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v17tx.c - ITU V.17 modem transmit part
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
 * $Id: v17tx.c,v 1.13 2005/08/31 19:27:53 steveu Exp $
 */

/*! \file */

/* THIS IS A WORK IN PROGRESS - KIND OF FUNCTIONAL, BUT NOT ROBUST! */

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

#include "spandsp/v17tx.h"

/* Segments of the training sequence */
#define V17_TRAINING_TEP_LEN        (480 + 48)
#define V17_TRAINING_TEP_SEG_2      48
#define V17_TRAINING_SEG_1          0
#define V17_TRAINING_SEG_2          256
#define V17_TRAINING_SEG_3          (V17_TRAINING_SEG_2 + 2976)
#define V17_TRAINING_SEG_4          (V17_TRAINING_SEG_3 + 64)
#define V17_TRAINING_END            (V17_TRAINING_SEG_4 + 48)

#define V17_TRAINING_SHORT_SEG_4    (V17_TRAINING_SEG_2 + 38)
#define V17_TRAINING_SHORT_END      (V17_TRAINING_SHORT_SEG_4 + 48)

#define V17_BRIDGE_WORD             0x8880

const complex_t v17_14400_constellation[128] =
{
    {-8, -3},       /* 0x00 */
    { 9,  2},       /* 0x01 */
    { 2, -9},       /* 0x02 */
    {-3,  8},       /* 0x03 */
    { 8,  3},       /* 0x04 */
    {-9, -2},       /* 0x05 */
    {-2,  9},       /* 0x06 */
    { 3, -8},       /* 0x07 */
    {-8,  1},       /* 0x08 */
    { 9, -2},       /* 0x09 */
    {-2, -9},       /* 0x0A */
    { 1,  8},       /* 0x0B */
    { 8, -1},       /* 0x0C */
    {-9,  2},       /* 0x0D */
    { 2,  9},       /* 0x0E */
    {-1, -8},       /* 0x0F */
    {-4, -3},       /* 0x10 */
    { 5,  2},       /* 0x11 */
    { 2, -5},       /* 0x12 */
    {-3,  4},       /* 0x13 */
    { 4,  3},       /* 0x14 */
    {-5, -2},       /* 0x15 */
    {-2,  5},       /* 0x16 */
    { 3, -4},       /* 0x17 */
    {-4,  1},       /* 0x18 */
    { 5, -2},       /* 0x19 */
    {-2, -5},       /* 0x1A */
    { 1,  4},       /* 0x1B */
    { 4, -1},       /* 0x1C */
    {-5,  2},       /* 0x1D */
    { 2,  5},       /* 0x1E */
    {-1, -4},       /* 0x1F */
    { 4, -3},       /* 0x20 */
    {-3,  2},       /* 0x21 */
    { 2,  3},       /* 0x22 */
    {-3, -4},       /* 0x23 */
    {-4,  3},       /* 0x24 */
    { 3, -2},       /* 0x25 */
    {-2, -3},       /* 0x26 */
    { 3,  4},       /* 0x27 */
    { 4,  1},       /* 0x28 */
    {-3, -2},       /* 0x29 */
    {-2,  3},       /* 0x2A */
    { 1, -4},       /* 0x2B */
    {-4, -1},       /* 0x2C */
    { 3,  2},       /* 0x2D */
    { 2, -3},       /* 0x2E */
    {-1,  4},       /* 0x2F */
    { 0, -3},       /* 0x30 */
    { 1,  2},       /* 0x31 */
    { 2, -1},       /* 0x32 */
    {-3,  0},       /* 0x33 */
    { 0,  3},       /* 0x34 */
    {-1, -2},       /* 0x35 */
    {-2,  1},       /* 0x36 */
    { 3,  0},       /* 0x37 */
    { 0,  1},       /* 0x38 */
    { 1, -2},       /* 0x39 */
    {-2, -1},       /* 0x3A */
    { 1,  0},       /* 0x3B */
    { 0, -1},       /* 0x3C */
    {-1,  2},       /* 0x3D */
    { 2,  1},       /* 0x3E */
    {-1,  0},       /* 0x3F */
    { 8, -3},       /* 0x40 */
    {-7,  2},       /* 0x41 */
    { 2,  7},       /* 0x42 */
    {-3, -8},       /* 0x43 */
    {-8,  3},       /* 0x44 */
    { 7, -2},       /* 0x45 */
    {-2, -7},       /* 0x46 */
    { 3,  8},       /* 0x47 */
    { 8,  1},       /* 0x48 */
    {-7, -2},       /* 0x49 */
    {-2,  7},       /* 0x4A */
    { 1, -8},       /* 0x4B */
    {-8, -1},       /* 0x4C */
    { 7,  2},       /* 0x4D */
    { 2, -7},       /* 0x4E */
    {-1,  8},       /* 0x4F */
    {-4, -7},       /* 0x50 */
    { 5,  6},       /* 0x51 */
    { 6, -5},       /* 0x52 */
    {-7,  4},       /* 0x53 */
    { 4,  7},       /* 0x54 */
    {-5, -6},       /* 0x55 */
    {-6,  5},       /* 0x56 */
    { 7, -4},       /* 0x57 */
    {-4,  5},       /* 0x58 */
    { 5, -6},       /* 0x59 */
    {-6, -5},       /* 0x5A */
    { 5,  4},       /* 0x5B */
    { 4, -5},       /* 0x5C */
    {-5,  6},       /* 0x5D */
    { 6,  5},       /* 0x5E */
    {-5, -4},       /* 0x5F */
    { 4, -7},       /* 0x60 */
    {-3,  6},       /* 0x61 */
    { 6,  3},       /* 0x62 */
    {-7, -4},       /* 0x63 */
    {-4,  7},       /* 0x64 */
    { 3, -6},       /* 0x65 */
    {-6, -3},       /* 0x66 */
    { 7,  4},       /* 0x67 */
    { 4,  5},       /* 0x68 */
    {-3, -6},       /* 0x69 */
    {-6,  3},       /* 0x6A */
    { 5, -4},       /* 0x6B */
    {-4, -5},       /* 0x6C */
    { 3,  6},       /* 0x6D */
    { 6, -3},       /* 0x6E */
    {-5,  4},       /* 0x6F */
    { 0, -7},       /* 0x70 */
    { 1,  6},       /* 0x71 */
    { 6, -1},       /* 0x72 */
    {-7,  0},       /* 0x73 */
    { 0,  7},       /* 0x74 */
    {-1, -6},       /* 0x75 */
    {-6,  1},       /* 0x76 */
    { 7,  0},       /* 0x77 */
    { 0,  5},       /* 0x78 */
    { 1, -6},       /* 0x79 */
    {-6, -1},       /* 0x7A */
    { 5,  0},       /* 0x7B */
    { 0, -5},       /* 0x7C */
    {-1,  6},       /* 0x7D */
    { 6,  1},       /* 0x7E */
    {-5,  0}        /* 0x7F */
};

const complex_t v17_12000_constellation[64] =
{
    { 7,  1},       /* 0x00 */
    {-5, -1},       /* 0x01 */
    {-1,  5},       /* 0x02 */
    { 1, -7},       /* 0x03 */
    {-7, -1},       /* 0x04 */
    { 5,  1},       /* 0x05 */
    { 1, -5},       /* 0x06 */
    {-1,  7},       /* 0x07 */
    { 3, -3},       /* 0x08 */
    {-1,  3},       /* 0x09 */
    { 3,  1},       /* 0x0A */
    {-3, -3},       /* 0x0B */
    {-3,  3},       /* 0x0C */
    { 1, -3},       /* 0x0D */
    {-3, -1},       /* 0x0E */
    { 3,  3},       /* 0x0F */
    { 7, -7},       /* 0x10 */
    {-5,  7},       /* 0x11 */
    { 7,  5},       /* 0x12 */
    {-7, -7},       /* 0x13 */
    {-7,  7},       /* 0x14 */
    { 5, -7},       /* 0x15 */
    {-7, -5},       /* 0x16 */
    { 7,  7},       /* 0x17 */
    {-1, -7},       /* 0x18 */
    { 3,  7},       /* 0x19 */
    { 7, -3},       /* 0x1A */
    {-7,  1},       /* 0x1B */
    { 1,  7},       /* 0x1C */
    {-3, -7},       /* 0x1D */
    {-7,  3},       /* 0x1E */
    { 7, -1},       /* 0x1F */
    { 3,  5},       /* 0x20 */
    {-1, -5},       /* 0x21 */
    {-5,  1},       /* 0x22 */
    { 5, -3},       /* 0x23 */
    {-3, -5},       /* 0x24 */
    { 1,  5},       /* 0x25 */
    { 5, -1},       /* 0x26 */
    {-5,  3},       /* 0x27 */
    {-1,  1},       /* 0x28 */
    { 3, -1},       /* 0x29 */
    {-1, -3},       /* 0x2A */
    { 1,  1},       /* 0x2B */
    { 1, -1},       /* 0x2C */
    {-3,  1},       /* 0x2D */
    { 1,  3},       /* 0x2E */
    {-1, -1},       /* 0x2F */
    {-5,  5},       /* 0x30 */
    { 7, -5},       /* 0x31 */
    {-5, -7},       /* 0x32 */
    { 5,  5},       /* 0x33 */
    { 5, -5},       /* 0x34 */
    {-7,  5},       /* 0x35 */
    { 5,  7},       /* 0x36 */
    {-5, -5},       /* 0x37 */
    {-5, -3},       /* 0x38 */
    { 7,  3},       /* 0x39 */
    { 3, -7},       /* 0x3A */
    {-3,  5},       /* 0x3B */
    { 5,  3},       /* 0x3C */
    {-7, -3},       /* 0x3D */
    {-3,  7},       /* 0x3E */
    { 3, -5}        /* 0x3F */
};

const complex_t v17_9600_constellation[32] =
{
    {-8,  2},       /* 0x00 */
    {-6, -4},       /* 0x01 */
    {-4,  6},       /* 0x02 */
    { 2,  8},       /* 0x03 */
    { 8, -2},       /* 0x04 */
    { 6,  4},       /* 0x05 */
    { 4, -6},       /* 0x06 */
    {-2, -8},       /* 0x07 */
    { 0,  2},       /* 0x08 */
    {-6,  4},       /* 0x09 */
    { 4,  6},       /* 0x0A */
    { 2,  0},       /* 0x0B */
    { 0, -2},       /* 0x0C */
    { 6, -4},       /* 0x0D */
    {-4, -6},       /* 0x0E */
    {-2,  0},       /* 0x0F */
    { 0, -6},       /* 0x10 */
    { 2, -4},       /* 0x11 */
    {-4, -2},       /* 0x12 */
    {-6,  0},       /* 0x13 */
    { 0,  6},       /* 0x14 */
    {-2,  4},       /* 0x15 */
    { 4,  2},       /* 0x16 */
    { 6,  0},       /* 0x17 */
    { 8,  2},       /* 0x18 */
    { 2,  4},       /* 0x19 */
    { 4, -2},       /* 0x1A */
    { 2, -8},       /* 0x1B */
    {-8, -2},       /* 0x1C */
    {-2, -4},       /* 0x1D */
    {-4,  2},       /* 0x1E */
    {-2,  8}        /* 0x1F */
};

const complex_t v17_7200_constellation[16] =
{
    { 6, -6},       /* 0x00 */
    {-2,  6},       /* 0x01 */
    { 6,  2},       /* 0x02 */
    {-6, -6},       /* 0x03 */
    {-6,  6},       /* 0x04 */
    { 2, -6},       /* 0x05 */
    {-6, -2},       /* 0x06 */
    { 6,  6},       /* 0x07 */
    {-2,  2},       /* 0x08 */
    { 6, -2},       /* 0x09 */
    {-2, -6},       /* 0x0A */
    { 2,  2},       /* 0x0B */
    { 2, -2},       /* 0x0C */
    {-6,  2},       /* 0x0D */
    { 2,  6},       /* 0x0E */
    {-2, -2}        /* 0x0F */
};

static __inline__ int scramble(v17_tx_state_t *s, int in_bit)
{
    int out_bit;

    out_bit = (in_bit ^ (s->scramble_reg >> 17) ^ (s->scramble_reg >> 22)) & 1;
    s->scramble_reg = (s->scramble_reg << 1) | out_bit;
    return out_bit;
}
/*- End of function --------------------------------------------------------*/

static __inline__ complex_t training_get(v17_tx_state_t *s)
{
    static const complex_t abcd[4] =
    {
        {-6.0, -2.0},
        { 2.0, -6.0},
        { 6.0,  2.0},
        {-2.0,  6.0}
    };
    static const int cdba_to_abcd[4] =
    {
        2, 3, 1, 0
    };
    static const int dibit_to_step[4] =
    {
        1, 0, 2, 3
    };
    int bits;

    /* V.17 training sequence */
    if (s->tep_step)
    {
        s->tep_step--;
        if (s->tep_step < V17_TRAINING_TEP_SEG_2)
            return complex_set(0.0, 0.0);
        return abcd[0];
    }
    s->training_step++;
    if (s->training_step <= V17_TRAINING_SEG_2)
    {
        /* Segment 1: ABAB... */
        return abcd[(s->training_step & 1) ^ 1];
    }
    if (s->training_step <= V17_TRAINING_SEG_3)
    {
        /* Segment 2: CDBA... */
        /* Apply the scrambler */
        bits = scramble(s, 1);
        bits = (bits << 1) | scramble(s, 1);
        bits = cdba_to_abcd[bits];
        s->constellation_state = bits;
        if (s->short_train  &&  s->training_step == V17_TRAINING_SHORT_SEG_4)
        {
            /* Go straight to the ones test. */
            s->training_step = V17_TRAINING_SEG_4;
        }
        return abcd[s->constellation_state];
    }
    /* Segment 3: Bridge... */
    bits = scramble(s, V17_BRIDGE_WORD >> (((s->training_step - V17_TRAINING_SEG_3 - 1) & 0x7) << 1));
    bits = (bits << 1) | scramble(s, V17_BRIDGE_WORD >> ((((s->training_step - V17_TRAINING_SEG_3 - 1) & 0x7) << 1) + 1));
    s->constellation_state = (s->constellation_state + dibit_to_step[bits]) & 3;
    return abcd[s->constellation_state];
}
/*- End of function --------------------------------------------------------*/

static int diff_and_convolutional_encode(v17_tx_state_t *s, int q)
{
    static const int diff_code[16] =
    {
        0, 1, 2, 3, 1, 2, 3, 0, 2, 3, 0, 1, 3, 0, 1, 2
    };
    int y1;
    int y2;
    int this1;
    int this2;

    /* Differentially encode */
    s->diff = diff_code[((q & 0x03) << 2) | s->diff];

    /* Convolutionally encode the redundant bit */
    y2 = s->diff >> 1;
    y1 = s->diff;
    this2 = y2 ^ y1 ^ (s->convolution >> 2) ^ ((y2 ^ (s->convolution >> 1)) & s->convolution);
    this1 = y2 ^ (s->convolution >> 1) ^ (y1 & s->convolution);
    s->convolution = ((s->convolution & 1) << 2) | ((this2 & 1) << 1) | (this1 & 1);
    return ((q << 1) & 0x78) | (s->diff << 1) | ((s->convolution >> 2) & 1);
}
/*- End of function --------------------------------------------------------*/

static int fake_get_bit(void *user_data)
{
    return 1;
}
/*- End of function --------------------------------------------------------*/

static complex_t getbaud(v17_tx_state_t *s)
{
    int i;
    int bit;
    int bits;
    int num_bits;

    if (s->in_training)
    {
        /* Send the training sequence */
        if (s->training_step < V17_TRAINING_SEG_4)
            return training_get(s);
        /* The last step in training is to send some 1's */
        if (++s->training_step > V17_TRAINING_END)
        {
            s->current_get_bit = s->get_bit;
            s->in_training = FALSE;
        }
    }
    if (s->shutdown)
    {
        /* The shutdown sequence is 32 bauds of all 1's, then 48 bauds
           of silence */
        if (s->shutdown++ >= 32)
            return complex_set(0.0, 0.0);
    }
    bits = 0;
    for (i = 0;  i < s->bits_per_symbol;  i++)
    {
        bit = s->current_get_bit(s->user_data);
        if ((bit & 2))
        {
            s->current_get_bit = fake_get_bit;
            /* Fill out this symbol with ones, and prepare to send
               the rest of the shutdown sequence. */
            s->shutdown = 1;
            for (  ;  i < s->bits_per_symbol;  i++)
                bits |= (scramble(s, s->current_get_bit(s->user_data)) << i);
            break;
        }
        bits |= (scramble(s, bit) << i);
    }
    s->constellation_state = diff_and_convolutional_encode(s, bits);
    return s->constellation[s->constellation_state];
}
/*- End of function --------------------------------------------------------*/

int v17_tx(v17_tx_state_t *s, int16_t *amp, int len)
{
    complex_t x;
    complex_t z;
    int i;
    int sample;
    /* You might expect the optimum weights here to be 0.75 and 0.25. However,
       the RRC filter warps things a bit. These values were arrived at, through
       simulation - minimise the variance between a 3 times oversampled approach
       and the weighted approach. */
    static const float weights[4] = {0.0, 0.68, 0.32, 0.0};
#define PULSESHAPER_GAIN        3.335
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

    if (s->shutdown > 80)
        return 0;
    for (sample = 0;  sample < len;  sample++)
    {
        if ((s->baud_phase += 3) > 10)
        {
            s->baud_phase -= 10;
            x = getbaud(s);
            /* Use a weighted value for the first sample of the baud to correct
               for a baud not being an integral number of samples long. This is
               almost as good as 3 times oversampling to a common multiple of the
               baud and sampling rates, but requires less compute. */
            s->rrc_filter[s->rrc_filter_step].re =
            s->rrc_filter[s->rrc_filter_step + V17TX_FILTER_STEPS].re = x.re - (x.re - s->current_point.re)*weights[s->baud_phase];
            s->rrc_filter[s->rrc_filter_step].im =
            s->rrc_filter[s->rrc_filter_step + V17TX_FILTER_STEPS].im = x.im - (x.im - s->current_point.im)*weights[s->baud_phase];
            s->current_point = x;
        }
        else
        {
            s->rrc_filter[s->rrc_filter_step] =
            s->rrc_filter[s->rrc_filter_step + V17TX_FILTER_STEPS] = s->current_point;
        }
        if (++s->rrc_filter_step >= V17TX_FILTER_STEPS)
            s->rrc_filter_step = 0;
        /* Root raised cosine pulse shaping at baseband */
        x.re = pulseshaper[V17TX_FILTER_STEPS >> 1]*s->rrc_filter[(V17TX_FILTER_STEPS >> 1) + s->rrc_filter_step].re;
        x.im = pulseshaper[V17TX_FILTER_STEPS >> 1]*s->rrc_filter[(V17TX_FILTER_STEPS >> 1) + s->rrc_filter_step].im;
        for (i = 0;  i < (V17TX_FILTER_STEPS >> 1);  i++)
        {
            x.re += pulseshaper[i]*(s->rrc_filter[i + s->rrc_filter_step].re + s->rrc_filter[V17TX_FILTER_STEPS - 1 - i + s->rrc_filter_step].re);
            x.im += pulseshaper[i]*(s->rrc_filter[i + s->rrc_filter_step].im + s->rrc_filter[V17TX_FILTER_STEPS - 1 - i + s->rrc_filter_step].im);
        }
        /* Now create and modulate the carrier */
        z = dds_complexf(&(s->carrier_phase), s->carrier_phase_rate);
        amp[sample] = (int16_t) ((x.re*z.re + x.im*z.im)*s->gain);
    }
    return sample;
}
/*- End of function --------------------------------------------------------*/

void v17_tx_power(v17_tx_state_t *s, float power)
{
    float l;

    l = 1.6*pow(10.0, (power - 3.14)/20.0);
    /* TODO: The figure 9.0 should change with the bit rate, as the
       constellations are different sizes. */
    /* 14400 -> 9.0, 12000 -> 7.0, 9600 -> 8.0, 7200 -> 6.0 */
    s->gain = l*32768.0/(PULSESHAPER_GAIN*9.0);
}
/*- End of function --------------------------------------------------------*/

void v17_tx_set_get_bit(v17_tx_state_t *s, get_bit_func_t get_bit, void *user_data)
{
    if (s->get_bit == s->current_get_bit)
        s->current_get_bit = get_bit;
    s->get_bit = get_bit;
    s->user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

int v17_tx_restart(v17_tx_state_t *s, int rate, int tep, int short_train)
{
    switch (rate)
    {
    case 14400:
        s->bits_per_symbol = 6;
        s->constellation = v17_14400_constellation;
        break;
    case 12000:
        s->bits_per_symbol = 5;
        s->constellation = v17_12000_constellation;
        break;
    case 9600:
        s->bits_per_symbol = 4;
        s->constellation = v17_9600_constellation;
        break;
    case 7200:
        s->bits_per_symbol = 3;
        s->constellation = v17_7200_constellation;
        break;
    default:
        return -1;
    }
    if (short_train)
    {
        s->diff = 0;
    }
    else
    {
        s->diff = 1;
        s->carrier_phase_rate = dds_phase_stepf(1800.0);
    }
    s->bit_rate = rate;
    memset(s->rrc_filter, 0, sizeof(s->rrc_filter));
    s->rrc_filter_step = 0;
    s->current_point = complex_set(0.0, 0.0);
    s->convolution = 0;
    s->scramble_reg = 0x2ECDD5;
    s->in_training = TRUE;
    s->tep_step = (tep) ?  V17_TRAINING_TEP_LEN  :  0;
    s->short_train = short_train;
    s->training_step = 0;
    s->carrier_phase = 0;
    s->baud_phase = 0;
    s->constellation_state = 0;
    s->shutdown = 0;
    s->current_get_bit = fake_get_bit;
    return 0;
}
/*- End of function --------------------------------------------------------*/

void v17_tx_init(v17_tx_state_t *s, int rate, int tep, get_bit_func_t get_bit, void *user_data)
{
    memset(s, 0, sizeof(*s));
    s->get_bit = get_bit;
    s->user_data = user_data;
    v17_tx_power(s, -12.0);
    v17_tx_restart(s, rate, tep, FALSE);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

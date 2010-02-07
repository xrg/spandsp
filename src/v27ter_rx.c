/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v27ter_rx.c - ITU V.27ter modem receive part
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
 * $Id: v27ter_rx.c,v 1.19 2004/03/25 13:28:37 steveu Exp $
 */

/*! \file */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "spandsp/telephony.h"
#include "spandsp/power_meter.h"
#include "spandsp/arctan2.h"
#include "spandsp/complex.h"
#include "spandsp/complex_dds.h"
#include "spandsp/complex_filters.h"

#include "spandsp/v27ter_rx.h"

/* V.27ter is a DPSK modem, but this code treats it like QAM. It nails down the
   signal to a static constellation, even though dealing with differences is all
   that is necessary. */

#define EQUALIZER_LOG

#define POWER_RESIDUE_THRESHOLD 5.0

/* Segments of the training sequence */
#define V27_TRAINING_SEG_5_LEN  1074
#define V27_TRAINING_SEG_6_LEN  8

enum
{
    TRAINING_STAGE_NORMAL_OPERATION = 0,
    TRAINING_STAGE_SYMBOL_ACQUISITION,
    TRAINING_STAGE_LOG_PHASE,
    TRAINING_STAGE_WAIT_FOR_HOP,
    TRAINING_STAGE_TRAIN_ON_ABAB,
    TRAINING_STAGE_TEST_ONES,
    TRAINING_STAGE_PARKED
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

/* Raised root cosine pulse shaping; Beta = 0.5; sinc compensation
   is used. We cannot simplify this by using only half the filter,
   as they 2 variants are each skewed by 1/2 of a sample. */

/* Sample rate 16000; baud rate 1600 */
#define PULSESHAPER_4800_GAIN   10.00724807
static const float pulseshaper_4800[2][V27RX_4800_FILTER_STEPS] =
{
    {
        -0.0135114054, 
        -0.0196679279, 
        -0.0055523370, 
        +0.0228972105, 
        +0.0428208047, 
        +0.0272175073, 
        -0.0336940087, 
        -0.1157659540, 
        -0.1622732633, 
        -0.1076042584, 
        +0.0862691369,
        +0.4001533441,
        +0.7536132181,
        +1.0328260474,
        +1.1389048225,
        +1.0328260474,
        +0.7536132181,
        +0.4001533441,
        +0.0862691369,
        -0.1076042584,
        -0.1622732633,
        -0.1157659540,
        -0.0336940087,
        +0.0272175073,
        +0.0428208047,
        +0.0228972105,
        -0.0055523370,
        -0.0196679279,
        -0.0135114054
    },
    {
        -0.0187777636, 
        -0.0152229104,
        +0.0079966176, 
        +0.0357505546,
        +0.0407313534, 
        +0.0018081289,
        -0.0750436404, 
        -0.1477644251,
        -0.1510434911, 
        -0.0284155881,
        +0.2319817316,
        +0.5787848082,
        +0.9096397245,
        +1.1117366023,
        +1.1117366023,
        +0.9096397245,
        +0.5787848082,
        +0.2319817316,
        -0.0284155881,
        -0.1510434911,
        -0.1477644251,
        -0.0750436404,
        +0.0018081289,
        +0.0407313534,
        +0.0357505546,
        +0.0079966176,
        -0.0152229104,
        -0.0187777636,
        -0.0055591015
    }
};

/* Raised root cosine pulse shaping; Beta = 0.5; sinc compensation
   is used. We cannot simplify this by using only half the filter,
   as they 3 variants are each skewed by a 1/3 of a sample. Only
   the middle one is symmetric. */

/* Sample rate 24000; baud rate 1200 */
#define PULSESHAPER_2400_GAIN   20.02468973
static const float pulseshaper_2400[3][V27RX_2400_FILTER_STEPS] =
{
    {
        +0.0296955705,
        +0.0425392534,
        +0.0351095258,
        +0.0014886616,
        -0.0540931362,
        -0.1155020097,
        -0.1570720717,
        -0.1500336656,
        -0.0714185111,
        +0.0872547931,
        +0.3144818072,
        +0.5786733841,
        +0.8340790979,
        +1.0314283514,
        +1.1303741356,
        +1.1101009921,
        +0.9749025300,
        +0.7530259002,
        +0.4891370573,
        +0.2326995097,
        +0.0257399636,
        -0.1064782395,
        -0.1594210342,
        -0.1472242163,
        -0.0958285456,
        -0.0338838744,
        +0.0155887642,
        +0.0403769802,
        +0.0400056646
    },
    {
        +0.0355750580,
        +0.0427737419,
        +0.0268452526,
        -0.0151541636,
        -0.0750329051,
        -0.1329995534,
        -0.1614710555,
        -0.1325705582,
        -0.0272928933,
        +0.1565587802,
        +0.4004931564,
        +0.6672645312,
        +0.9086060950,
        +1.0768540653,
        +1.1371865189,
        +1.0768540653,
        +0.9086060950,
        +0.6672645312,
        +0.4004931564,
        +0.1565587802,
        -0.0272928933,
        -0.1325705582,
        -0.1614710555,
        -0.1329995534,
        -0.0750329051,
        -0.0151541636,
        +0.0268452526,
        +0.0427737419,
        +0.0355750580
    },
    {
        +0.0400056646,
        +0.0403769802,
        +0.0155887642,
        -0.0338838744,
        -0.0958285456,
        -0.1472242163,
        -0.1594210342,
        -0.1064782395,
        +0.0257399636,
        +0.2326995097,
        +0.4891370573,
        +0.7530259002,
        +0.9749025300,
        +1.1101009921,
        +1.1303741356,
        +1.0314283514,
        +0.8340790979,
        +0.5786733841,
        +0.3144818072,
        +0.0872547931,
        -0.0714185111,
        -0.1500336656,
        -0.1570720717,
        -0.1155020097,
        -0.0540931362,
        +0.0014886616,
        +0.0351095258,
        +0.0425392534,
        +0.0296955705
    }
};

static void equalizer_reset(v27ter_rx_state_t *s, float delta)
{
    int i;

    /* Start with an equalizer based on everything being perfect */
    for (i = 0;  i < 2*V27_EQUALIZER_LEN + 1;  i++)
        s->eq_coeff[i] = complex_set(0.0, 0.0);
    s->eq_coeff[V27_EQUALIZER_LEN] = complex_set(5.0, 0.0);
    for (i = 0;  i <= V27_EQUALIZER_MASK;  i++)
        s->eq_buf[i] = complex_set(0.0, 0.0);

    s->eq_put_step = (s->bit_rate == 4800)  ?  5  :  10;
    s->eq_step = 0;
    s->eq_delta = delta;
}
/*- End of function --------------------------------------------------------*/

static complex_t equalizer_get(v27ter_rx_state_t *s)
{
    int i;
    int p;
    complex_t z;
    complex_t z1;

    /* Get the next equalized value. */
    z = complex_set(0.0, 0.0);
    for (i = 0;  i < 2*V27_EQUALIZER_LEN + 1;  i++)
    {
        p = (s->eq_step + i) & V27_EQUALIZER_MASK;
        z1 = complex_mul(&s->eq_coeff[i], &s->eq_buf[p]);
        z = complex_add(&z, &z1);
    }
    return z;
}
/*- End of function --------------------------------------------------------*/

#if defined(EQUALIZER_LOG)
static void equalizer_dump(v27ter_rx_state_t *s)
{
    int i;

    if (s->qam_log)
    {
        fprintf(s->qam_log, "Equalizer state:\n");
        for (i = 0;  i < 2*V27_EQUALIZER_LEN + 1;  i++)
            fprintf(s->qam_log, "%3d (%15.5f, %15.5f) -> %15.5f\n", i - V27_EQUALIZER_LEN, s->eq_coeff[i].re, s->eq_coeff[i].im, power(&(s->eq_coeff[i])));
    }
}
/*- End of function --------------------------------------------------------*/
#endif

static inline int descramble(v27ter_rx_state_t *s, int in_bit)
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
        if (s->in_training > TRAINING_STAGE_NORMAL_OPERATION  &&  s->in_training < TRAINING_STAGE_TEST_ONES)
        {
            s->scrambler_pattern_count = 0;
        }
        else
        {
            if ((((s->scramble_reg >> 7) ^ in_bit) & ((s->scramble_reg >> 8) ^ in_bit) & ((s->scramble_reg >> 11) ^ in_bit) & 1))
                s->scrambler_pattern_count = 0;
            else
                s->scrambler_pattern_count++;
        }
    }
    if (s->in_training > TRAINING_STAGE_NORMAL_OPERATION  &&  s->in_training < TRAINING_STAGE_TEST_ONES)
        s->scramble_reg = (s->scramble_reg << 1) | out_bit;
    else
        s->scramble_reg = (s->scramble_reg << 1) | in_bit;
    return out_bit;
}
/*- End of function --------------------------------------------------------*/

static int find_octant(complex_t *z)
{
    float abs_re;
    float abs_im;
    int b1;
    int b2;
    int bits;

    /* Are we near an axis or a diagonal? */
    abs_re = fabsf(z->re);
    abs_im = fabsf(z->im);
    if (abs_im > abs_re*0.4142136  &&  abs_im < abs_re*2.4142136)
    {
        /* Split the space along the two axes. */
        b1 = (z->re < 0.0);
        b2 = (z->im < 0.0);
        bits = (b2 << 2) | ((b1 ^ b2) << 1) | 1;
    }
    else
    {
        /* Split the space along the two diagonals. */
        b1 = (z->im > z->re);
        b2 = (z->im < -z->re);
        bits = (b2 << 2) | ((b1 ^ b2) << 1);
    }
    return bits;
}
/*- End of function --------------------------------------------------------*/

static int find_quadrant(complex_t *z)
{
    int b1;
    int b2;

    /* Split the space along the two diagonals. */
    b1 = (z->im > z->re);
    b2 = (z->im < -z->re);
    return (b2 << 1) | (b1 ^ b2);
}
/*- End of function --------------------------------------------------------*/

static void tune_equalizer(v27ter_rx_state_t *s, complex_t *z, int nearest)
{
    int i;
    int p;
    complex_t ez;
    complex_t deps;
    complex_t z1;
    complex_t z2;

    /* Find the x and y mismatch from the exact constellation position. */
    ez = complex_sub(&constellation[nearest], z);

    deps.re = ez.re*s->eq_delta;
    deps.im = ez.im*s->eq_delta;
    for (i = 0;  i <= 2*V27_EQUALIZER_LEN;  i++)
    {
        p = (s->eq_step + i) & V27_EQUALIZER_MASK;
        z2 = complex_conj(&s->eq_buf[p]);
        z1 = complex_mul(&deps, &z2);
        s->eq_coeff[i] = complex_add(&s->eq_coeff[i], &z1);
    }
}
/*- End of function --------------------------------------------------------*/

static int32_t track_carrier(v27ter_rx_state_t *s, complex_t *z, int nearest)
{
    int lead;
    int32_t angle;
    complex_t zz;
    int i;

    /* Integrate and dump the angular error. A tight loop filter would give trouble
       here, as buffering through the RRC filter, and the equalizer gives substantial
       lag between updates and their effect. */
    zz = complex_conj(&constellation[nearest]);
    zz = complex_mul(z, &zz);
    angle = arctan2(zz.im, zz.re);
    s->lead_lag += angle/100000.0;
    s->lead_lag_time++;

    if (s->lead_lag > 200000  ||  s->lead_lag < -200000)
    {
        //fprintf(stderr, "Phase step = %d %d %f\n", s->lead_lag, s->carrier_phase_rate, s->carrier_phase_rate*8000.0/(65536.0*65536.0));
        /* Don't allow extreme updates. This is only for fine carrier tracking. */
        if (s->lead_lag_time < 30)
            s->lead_lag_time = 30;
        s->carrier_phase_rate += 30*(s->lead_lag/s->lead_lag_time);
        s->lead_lag = 0;
        s->lead_lag_time = 0;
    }
    return angle;
}
/*- End of function --------------------------------------------------------*/

static inline void put_bit(v27ter_rx_state_t *s, int bit)
{
    int out_bit;

    bit &= 1;

    out_bit = descramble(s, bit);

    /* We need to strip the last part of the training before we let data
       go to the application. */
    if (s->in_training == TRAINING_STAGE_NORMAL_OPERATION)
    {
        s->put_bit(s->user_data, out_bit);
    }
    else
    {
        if (out_bit)
            s->training_test_ones++;
    }
}
/*- End of function --------------------------------------------------------*/

static void decode_baud(v27ter_rx_state_t *s, complex_t *z)
{
    static const uint8_t phase_steps_4800[8] =
    {
        1, 0, 2, 3, 7, 6, 4, 5
    };
    static const uint8_t phase_steps_2400[4] =
    {
        0, 1, 3, 2
    };
    int nearest;
    int raw_bits;

    switch (s->bit_rate)
    {
    case 4800:
        nearest = find_octant(z);
        track_carrier(s, z, nearest);
        tune_equalizer(s, z, nearest);
        raw_bits = phase_steps_4800[(nearest - s->constellation_state) & 7];
        //printf("Raw=%x\n", raw_bits);
        s->constellation_state = nearest;
        put_bit(s, raw_bits >> 2);
        put_bit(s, raw_bits >> 1);
        put_bit(s, raw_bits);
        break;
    case 2400:
        nearest = find_quadrant(z);
        track_carrier(s, z, nearest << 1);
        tune_equalizer(s, z, nearest << 1);
        raw_bits = phase_steps_2400[(nearest - s->constellation_state) & 3];
        //printf("Raw=%x\n", raw_bits);
        s->constellation_state = nearest;
        put_bit(s, raw_bits >> 1);
        put_bit(s, raw_bits);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_baud(v27ter_rx_state_t *s, const complex_t *sample)
{
    static const int abab_pos[2] =
    {
        0, 4
    };
    complex_t z;
    complex_t zz;
    float p;
    float q;
    int bit;
    int i;
    int j;
    int32_t angle;
    int32_t ang;
    int32_t last_angle;

    if (s->bit_rate == 4800)
    {
        s->rrc_filter[s->rrc_filter_step].re =
        s->rrc_filter[s->rrc_filter_step + V27RX_4800_FILTER_STEPS].re = sample->re;
        s->rrc_filter[s->rrc_filter_step].im =
        s->rrc_filter[s->rrc_filter_step + V27RX_4800_FILTER_STEPS].im = sample->im;
        if (++s->rrc_filter_step >= V27RX_4800_FILTER_STEPS)
            s->rrc_filter_step = 0;
        /* Put things into the equalization buffer at T/2 rate. The Gardner algorithm
           will fiddle the step to align this with the bits. */
        if ((s->eq_put_step -= 2) > 0)
        {
            //printf("Samp, %f, %f, %f, 0, 0x%X\n", z.re, z.im, sqrt(z.re*z.re + z.im*z.im), s->eq_put_step);
            return;
        }

        /* This is our interpolation filter, as well as our demod filter. */
        j = -s->eq_put_step;
        if (j > 1)
            j = 1;
        z = complex_set(0.0, 0.0);
        for (i = 0;  i < V27RX_4800_FILTER_STEPS;  i++)
        {
            z.re += pulseshaper_4800[j][i]*s->rrc_filter[i + s->rrc_filter_step].re;
            z.im += pulseshaper_4800[j][i]*s->rrc_filter[i + s->rrc_filter_step].im;
        }
        z.re *= 1.0/PULSESHAPER_4800_GAIN;
        z.im *= 1.0/PULSESHAPER_4800_GAIN;

        s->eq_put_step += 5;
    }
    else
    {
        s->rrc_filter[s->rrc_filter_step].re =
        s->rrc_filter[s->rrc_filter_step + V27RX_2400_FILTER_STEPS].re = sample->re;
        s->rrc_filter[s->rrc_filter_step].im =
        s->rrc_filter[s->rrc_filter_step + V27RX_2400_FILTER_STEPS].im = sample->im;
        if (++s->rrc_filter_step >= V27RX_2400_FILTER_STEPS)
            s->rrc_filter_step = 0;
        /* Put things into the equalization buffer at T/2 rate. The Gardner algorithm
           will fiddle the step to align this with the bits. */
        if ((s->eq_put_step -= 3) > 0)
        {
            //printf("Samp, %f, %f, %f, 0, 0x%X\n", z.re, z.im, sqrt(z.re*z.re + z.im*z.im), s->eq_put_step);
            return;
        }

        /* This is our interpolation filter, as well as our demod filter. */
        j = -s->eq_put_step;
        if (j > 2)
            j = 2;
        z = complex_set(0.0, 0.0);
        for (i = 0;  i < V27RX_2400_FILTER_STEPS;  i++)
        {
            z.re += pulseshaper_2400[j][i]*s->rrc_filter[i + s->rrc_filter_step].re;
            z.im += pulseshaper_2400[j][i]*s->rrc_filter[i + s->rrc_filter_step].im;
        }
        z.re *= 1.0/PULSESHAPER_2400_GAIN;
        z.im *= 1.0/PULSESHAPER_2400_GAIN;

        s->eq_put_step += 10;
    }

    /* Add a sample to the equalizer's circular buffer, but don't calculate anything
       at this time. */
    s->eq_buf[s->eq_step].re = z.re;
    s->eq_buf[s->eq_step].im = z.im;
    s->eq_step = (s->eq_step + 1) & V27_EQUALIZER_MASK;
        
    /* On alternate insertions we have a whole baud and must process it. */
    if ((++s->baud_phase & 1))
    {
        //printf("Samp, %f, %f, %f, -1, 0x%X\n", z.re, z.im, sqrt(z.re*z.re + z.im*z.im), s->eq_put_step);
        return;
    }
    //printf("Samp, %f, %f, %f, 1, 0x%X\n", z.re, z.im, sqrt(z.re*z.re + z.im*z.im), s->eq_put_step);

    /* Perform a Gardner test for baud alignment */
    p = s->eq_buf[(s->eq_step - 3) & V27_EQUALIZER_MASK].re
      - s->eq_buf[(s->eq_step - 1) & V27_EQUALIZER_MASK].re;
    p *= s->eq_buf[(s->eq_step - 2) & V27_EQUALIZER_MASK].re;

    q = s->eq_buf[(s->eq_step - 3) & V27_EQUALIZER_MASK].im
      - s->eq_buf[(s->eq_step - 1) & V27_EQUALIZER_MASK].im;
    q *= s->eq_buf[(s->eq_step - 2) & V27_EQUALIZER_MASK].im;

    p += q;
    if (p > 0)
        s->gardner_integrate += s->gardner_step;
    else
        s->gardner_integrate -= s->gardner_step;

    if (abs(s->gardner_integrate) >= 0x100)
    {
        /* This integrate and dump approach avoids rapid changes of the equalizer put step.
           Rapid changes, without hysteresis, are bad. They degrade the equalizer performance
           when the true symbol boundary is close to a sample boundary. */
        //printf("Hop %d\n", s->gardner_integrate);
        if (s->gardner_integrate > 0)
            s->eq_put_step++;
        else
            s->eq_put_step--;
        s->gardner_integrate = 0;
    }
    //fprintf(stderr, "Gardner=%10.5f 0x%X\n", p, s->eq_put_step);

    z = equalizer_get(s);

    //fprintf(stderr, "Equalized symbol - %15.5f %15.5f\n", z.re, z.im);
    if (s->qam_log)
        fprintf(s->qam_log, "%f %f %d %d\n", (5.0/1.414)*z.re, (5.0/1.414)*z.im, s->lead_lag, s->carrier_phase_rate);
    switch (s->in_training)
    {
    case TRAINING_STAGE_NORMAL_OPERATION:
        decode_baud(s, &z);
        break;
    case TRAINING_STAGE_SYMBOL_ACQUISITION:
        /* Allow time for the Gardner algorithm to settle the baud timing */
        //fprintf(stderr, "0: Choo choo - %15.5f %15.5f\n", z.re, z.im);
        if (++s->training_count >= 30)
        {
            s->gardner_step = 0x20;
            s->in_training = TRAINING_STAGE_LOG_PHASE;
            s->last_angle_a =
            s->start_angle_a = arctan2(z.im, z.re);
        }
        break;
    case TRAINING_STAGE_LOG_PHASE:
        /* Record the current alternate phase angle */
        angle = arctan2(z.im, z.re);
        s->last_angle_b =
        s->start_angle_b = angle;
        s->training_count = 1;
        s->in_training = TRAINING_STAGE_WAIT_FOR_HOP;
        break;
    case TRAINING_STAGE_WAIT_FOR_HOP:
        angle = arctan2(z.im, z.re);
        /* Look for the initial ABAB sequence to display a phase reversal, which will
           signal the start of the scrambled ABAB segment */
        if ((s->training_count & 1))
        {
            last_angle = s->last_angle_a;
            s->last_angle_a = angle;
        }
        else
        {
            last_angle = s->last_angle_b;
            s->last_angle_b = angle;
        }
        ang = angle - last_angle;
        if ((ang > 0x40000000  ||  ang < -0x40000000)  &&  s->training_count >= 3)
        {
            /* We seem to have a phase reversal */
            /* Slam the carrier frequency into line, based on the total phase drift over the last
               section. Use the shift from the odd bits and the shift from the even bits to get
               better jitter suppression. We need to scale here, or at the maximum specified
               frequency deviation we could overflow, and get a silly answer. */
            if ((s->training_count & 1))
                ang = (last_angle - s->start_angle_a)/(s->training_count - 1) + (s->last_angle_b - s->start_angle_b)/(s->training_count - 1);
            else
                ang = (s->last_angle_a - s->start_angle_a)/s->training_count + (last_angle - s->start_angle_b)/(s->training_count - 2);

            if (s->bit_rate == 4800)
                s->carrier_phase_rate += ang/10;
            else
                s->carrier_phase_rate += 3*(ang/40);
            printf("Coarse carrier frequency %7.2f (%d)\n", s->carrier_phase_rate*8000.0/(65536.0*65536.0), s->training_count);

            /* Make a step shift in the phase, to pull it into line. We need to rotate the RRC filter
               buffer and the equalizer buffer, as well as the carrier phase, for this to play out
               nicely. */
            zz = complex_set(cos(angle*2.0*3.14159/(65536.0*65536.0)), sin(angle*2.0*3.14159/(65536.0*65536.0)));
            zz = complex_conj(&zz);
            for (i = 0;  i < 2*V27RX_FILTER_STEPS;  i++)
                s->rrc_filter[i] = complex_mul(&s->rrc_filter[i], &zz);
            for (i = 0;  i <= V27_EQUALIZER_MASK;  i++)
                s->eq_buf[i] = complex_mul(&s->eq_buf[i], &zz);
            s->carrier_phase += angle;

            /* QAM and Gardner only play nicely with heavy damping, so we need to change to
               a slow rate of symbol timing adaption. However, it must not be so slow that it
               cannot track the worst case timing error specified in V.27ter. This should be 0.01%,
               but since we might be off in the opposite direction from the source, the total
               error could be higher.*/
            s->gardner_step = 1;
            /* We have just seen the third element of the scrambled sequence (the first two did
               not break the sequence), so skip them. */
            s->training_bc = 1;
            s->training_bc ^= descramble(s, 1);
            descramble(s, 1);
            descramble(s, 1);
            s->training_bc ^= descramble(s, 1);
            descramble(s, 1);
            descramble(s, 1);
            s->training_bc ^= descramble(s, 1);
            descramble(s, 1);
            descramble(s, 1);
            s->training_count = 3;
            s->in_training = TRAINING_STAGE_TRAIN_ON_ABAB;
        }
        else
        {
            s->training_count++;
        }
        break;
    case TRAINING_STAGE_TRAIN_ON_ABAB:
        /* Train on the scrambled ABAB section */
        s->training_bc ^= descramble(s, 1);
        descramble(s, 1);
        descramble(s, 1);
        track_carrier(s, &z, abab_pos[s->training_bc]);
        tune_equalizer(s, &z, abab_pos[s->training_bc]);
    
        //fprintf(stderr, "3: Choo choo - %d - %15.5f %15.5f %d [%15.5f %15.5f]\n", s->training_count, constellation[abab_pos[s->training_bc]].re, constellation[abab_pos[s->training_bc]].im, abab_pos[s->training_bc], z.re, z.im);
        //fprintf(stderr, "3: Choo choo - zz = %15.5f %15.5f z = %15.5f %15.5f p = %15.5f\n", zz.re, zz.im, z.re, z.im, p);
        if (++s->training_count >= V27_TRAINING_SEG_5_LEN)
        {
            s->constellation_state = (s->bit_rate == 4800)  ?  4  :  2;
            s->training_count = 0;
            s->in_training = TRAINING_STAGE_TEST_ONES;
#if defined(EQUALIZER_LOG)
            if (s->qam_log)
                fprintf(s->qam_log, "Equalizer, post ABAB:\n");
            equalizer_dump(s);
#endif
        }
        break;
    case TRAINING_STAGE_TEST_ONES:
        //fprintf(stderr, "4: Choo choo - %d\n", s->training_count);
        decode_baud(s, &z);
        if (++s->training_count >= V27_TRAINING_SEG_6_LEN)
        {
            //fprintf(stderr, "5: Choo choo - %d ones\n", s->training_test_ones);
            if ((s->bit_rate == 4800  &&  s->training_test_ones == 24)
                ||
                (s->bit_rate == 2400  &&  s->training_test_ones == 16))
            {
                /* We are up and running */
                //fprintf(stderr, "Training succeeded\n");
                s->in_training = TRAINING_STAGE_NORMAL_OPERATION;
                s->put_bit(s->user_data, PUTBIT_TRAINING_SUCCEEDED);
            }
            else
            {
                /* Training has failed */
                //fprintf(stderr, "Training failed (only %d 1's)\n", s->training_test_ones);
                /* Park this modem */
                s->in_training = TRAINING_STAGE_PARKED;
                s->put_bit(s->user_data, PUTBIT_TRAINING_FAILED);
            }
#if defined(EQUALIZER_LOG)
            if (s->qam_log)
                fprintf(s->qam_log, "Equalizer, post training:\n");
            equalizer_dump(s);
#endif
        }
        break;
    case TRAINING_STAGE_PARKED:
        /* We failed to train! */
        /* Park here until the carrier drops. */
        break;
    }
    z = constellation[s->constellation_state];
}
/*- End of function --------------------------------------------------------*/

void v27ter_rx(v27ter_rx_state_t *s, const int16_t *amp, int len)
{
    static const complex_t zero =
    {
        0.0, 0.0
    };
    int i;
    int16_t sample;
    complex_t z;
    int32_t power;
    float x;

    for (i = 0;  i < len;  i++)
    {
        sample = amp[i];
        power = power_meter_update(&(s->power), sample);
        //printf("Power = %f\n", power_meter_dbm0(&(s->power)));
        if (s->carrier_present)
        {
            /* Look for power below -48dBm0 to turn the carrier off */
            if (power < s->carrier_off_power)
            {
                v27ter_rx_restart(s, s->bit_rate);
                s->put_bit(s->user_data, PUTBIT_CARRIER_DOWN);
                continue;
            }
        }
        else
        {
            /* Look for power exceeding -43dBm0 to turn the carrier on */
            if (power < s->carrier_on_power)
                continue;
            s->carrier_present = TRUE;
            s->put_bit(s->user_data, PUTBIT_CARRIER_UP);
        }
        if (s->in_training == TRAINING_STAGE_SYMBOL_ACQUISITION)
        {
            /* Only AGC during the initial training */
            s->agc_scaling = (1.414/5.0)*3.60/sqrt(power);
            //fprintf(s->qam_log, "AGC %f %f - %d %f %f\n", 3.60/sqrt(power), s->agc_scaling, sample, power_meter_dbm0(&(s->power)), 0.018890*0.1);
            //fprintf(stderr, "AGC %f %f - %d %f %f\n", 3.60/sqrt(power), s->agc_scaling, sample, power_meter_dbm0(&(s->power)), 0.018890*0.1);
        }
        x = sample*s->agc_scaling;
        /* Shift to baseband */
        z = complex_dds(&(s->carrier_phase), s->carrier_phase_rate);
        z.re *= x;
        z.im *= x;
        process_baud(s, &z);
    }
}
/*- End of function --------------------------------------------------------*/

void v27ter_rx_restart(v27ter_rx_state_t *s, int bit_rate)
{
    int i;

    if (s->qam_log)
        fprintf(s->qam_log, "Restart at %d\n", bit_rate);
    s->bit_rate = bit_rate;

    memset(s->rrc_filter, 0, sizeof(s->rrc_filter));
    s->rrc_filter_step = 0;

    s->scramble_reg = 0x3C;
    s->scrambler_pattern_count = 0;
    s->in_training = TRAINING_STAGE_SYMBOL_ACQUISITION;
    s->training_bc = 0;
    s->training_count = 0;
    s->training_test_ones = 0;
    s->carrier_present = FALSE;

    s->carrier_phase_rate = complex_dds_phase_step(1800.0);
    s->carrier_phase = 0;
    power_meter_init(&(s->power), 4);
    s->carrier_on_power = power_meter_level(-43);
    s->carrier_off_power = power_meter_level(-48);
    s->agc_scaling = 0.0005;

    s->constellation_state = 0;

    equalizer_reset(s, 0.25);

    s->gardner_integrate = 0;
    s->gardner_step = 0x80;
    s->baud_phase = 0;
    s->lead_lag = 0;
    s->lead_lag_time = 0;
}
/*- End of function --------------------------------------------------------*/

void v27ter_rx_init(v27ter_rx_state_t *s, int bit_rate, put_bit_func_t put_bit, void *user_data)
{
    int i;
    int j;
    int k;
    int best;
    float distance;
    float best_distance;
    float x;
    float y;

    memset(s, 0, sizeof(*s));
    s->bit_rate = bit_rate;
    s->put_bit = put_bit;
    s->user_data = user_data;

    v27ter_rx_restart(s, s->bit_rate);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

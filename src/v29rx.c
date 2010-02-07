/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v29rx.c - ITU V.29 modem receive part
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
 * $Id: v29rx.c,v 1.28 2004/03/28 12:46:42 steveu Exp $
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

#include "spandsp/v29rx.h"

/* Segments of the training sequence */
#define V29_TRAINING_SEG_2_LEN      128
#define V29_TRAINING_SEG_3_LEN      384
#define V29_TRAINING_SEG_4_LEN      48

enum
{
    TRAINING_STAGE_NORMAL_OPERATION = 0,
    TRAINING_STAGE_SYMBOL_ACQUISITION,
    TRAINING_STAGE_LOG_PHASE,
    TRAINING_STAGE_WAIT_FOR_CDCD,
    TRAINING_STAGE_TRAIN_ON_CDCD,
    TRAINING_STAGE_TRAIN_ON_CDCD_AND_TEST,
    TRAINING_STAGE_TEST_ONES,
    TRAINING_STAGE_PARKED
};

const complex_t v29_constellation[16] =
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
static const float constellation_weights[16] =
{
    0.3,
    1.0,
    0.3,
    1.0,
    0.3,
    1.0,
    0.3,
    1.0,
    0.2,
    0.3,
    0.2,
    0.3,
    0.2,
    0.3,
    0.2,
    0.3
};

/* The following are not an issue for multi-threading, as this is a global table
   of constants. The first thread generates it. A second thread, starting around
   the same time as the first, might duplicate the work. The outcome will still be
   the same. */
static uint8_t space_map_9600[50][50];
static int inited = FALSE;

/* Raised root cosine pulse shaping; Beta = 0.5; 4 symbols either
   side of the centre. We cannot simplify this by using only half
   the filter, as they 3 variants are each skewed by a 1/3 of a
   sample. Only the middle one is symmetric. */
#define PULSESHAPER_GAIN   10.00736638
static const float pulseshaper[3][V29RX_FILTER_STEPS] =
{
    {
        -0.0100995945,
        -0.0011252694,
        +0.0150522372,
        +0.0104259088,
        -0.0133985873,
        -0.0150014382,
        +0.0228129663,
        +0.0402499659,
        -0.0339443882,
        -0.1470341952,
        -0.1061033321,
        +0.2329278410,
        +0.7528269497,
        +1.1095660633,
        +1.0309693302,
        +0.5786285856,
        +0.0875742834,
        -0.1496926681,
        -0.1154045274,
        +0.0013802404,
        +0.0424358420,
        +0.0080555991,
        -0.0194424096,
        -0.0055307603,
        +0.0152518611,
        +0.0107140594,
        -0.0061334500
    },
    {
        -0.0092609675,
        +0.0048899372,
        +0.0167879600,
        +0.0030316329,
        -0.0185932304,
        -0.0053871591,
        +0.0355085550,
        +0.0267150282,
        -0.0750224229,
        -0.1611955320,
        -0.0269239386,
        +0.4005959929,
        +0.9082635952,
        +1.1366252468,
        +0.9082635952,
        +0.4005959929,
        -0.0269239386,
        -0.1611955320,
        -0.0750224229,
        +0.0267150282,
        +0.0355085550,
        -0.0053871591,
        -0.0185932304,
        +0.0030316329,
        +0.0167879600,
        +0.0048899372,
        -0.0092609675
    },
    {
        -0.0061334501,
        +0.0107140594,
        +0.0152518611,
        -0.0055307603,
        -0.0194424096,
        +0.0080555991,
        +0.0424358420,
        +0.0013802404,
        -0.1154045274,
        -0.1496926681,
        +0.0875742834,
        +0.5786285856,
        +1.0309693302,
        +1.1095660633,
        +0.7528269497,
        +0.2329278410,
        -0.1061033321,
        -0.1470341952,
        -0.0339443882,
        +0.0402499659,
        +0.0228129663,
        -0.0150014382,
        -0.0133985873,
        +0.0104259088,
        +0.0150522372,
        -0.0011252694,
        -0.0100995945
    }
};

int v29_rx_equalizer_state(v29_rx_state_t *s, complex_t **coeffs)
{
    *coeffs = s->eq_coeff;
    return 2*V29_EQUALIZER_LEN + 1;
}
/*- End of function --------------------------------------------------------*/

static void equalizer_reset(v29_rx_state_t *s, float delta)
{
    int i;

    /* Start with an equalizer based on everything being perfect */
    for (i = 0;  i < 2*V29_EQUALIZER_LEN + 1;  i++)
        s->eq_coeff[i] = complex_set(0.0, 0.0);
    s->eq_coeff[V29_EQUALIZER_LEN] = complex_set(3.0, 0.0);
    for (i = 0;  i <= V29_EQUALIZER_MASK;  i++)
        s->eq_buf[i] = complex_set(0.0, 0.0);

    s->eq_put_step = 5;
    s->eq_step = 0;
    s->eq_delta = delta/(2*V29_EQUALIZER_LEN + 1);
}
/*- End of function --------------------------------------------------------*/

static complex_t equalizer_get(v29_rx_state_t *s)
{
    int i;
    int p;
    complex_t z;
    complex_t z1;

    /* Get the next equalized value. */
    z = complex_set(0.0, 0.0);
    for (i = 0;  i < 2*V29_EQUALIZER_LEN + 1;  i++)
    {
        p = (s->eq_step + i) & V29_EQUALIZER_MASK;
        z1 = complex_mul(&s->eq_coeff[i], &s->eq_buf[p]);
        z = complex_add(&z, &z1);
    }
    return z;
}
/*- End of function --------------------------------------------------------*/

static int scrambled_training_bit(v29_rx_state_t *s)
{
    int bit;

    /* Segment 3 of the training sequence - the scrambled CDCD part. */
    /* Apply the 1 + x^-6 + x^-7 scrambler */
    bit = s->training_scramble_reg & 1;
    s->training_scramble_reg >>= 1;
    if (bit ^ (s->training_scramble_reg & 1))
        s->training_scramble_reg |= 0x40;
    return bit;
}
/*- End of function --------------------------------------------------------*/

static inline int find_quadrant(complex_t *z)
{
    int b1;
    int b2;

    /* Split the space along the two diagonals. */
    b1 = (z->im > z->re);
    b2 = (z->im < -z->re);
    return (b2 << 1) | (b1 ^ b2);
}
/*- End of function --------------------------------------------------------*/

static void tune_equalizer(v29_rx_state_t *s, complex_t *z, int nearest)
{
    int i;
    int p;
    complex_t ez;
    complex_t z1;

    /* Find the x and y mismatch from the exact constellation position. */
    ez = complex_sub(&v29_constellation[nearest], z);
    //printf("%f\n", sqrt(ez.re*ez.re + ez.im*ez.im));
    /* Use weighting to put more emphasis on getting the close together
       constellation points right. */
    ez.re *= constellation_weights[nearest]*s->eq_delta;
    ez.im *= constellation_weights[nearest]*s->eq_delta;

    for (i = 0;  i <= 2*V29_EQUALIZER_LEN;  i++)
    {
        p = (s->eq_step + i) & V29_EQUALIZER_MASK;
        z1 = complex_conj(&s->eq_buf[p]);
        z1 = complex_mul(&ez, &z1);
        s->eq_coeff[i] = complex_add(&s->eq_coeff[i], &z1);
        s->eq_coeff[i].re *= 0.9999;
        s->eq_coeff[i].im *= 0.9999;
    }
}
/*- End of function --------------------------------------------------------*/

static int32_t track_carrier(v29_rx_state_t *s, complex_t *z, int nearest)
{
    int lead;
    int32_t angle;
    complex_t zz;
    int i;

    /* Integrate and dump the angular error. A tight loop filter would give trouble
       here, as buffering through the RRC filter, and the equalizer gives substantial
       lag between updates and their effect. */
    zz = complex_conj(&v29_constellation[nearest]);
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

static inline void put_bit(v29_rx_state_t *s, int bit)
{
    int out_bit;

    bit &= 1;

    /* Descramble the bit */
    out_bit = (bit ^ (s->scramble_reg >> 17) ^ (s->scramble_reg >> 22)) & 1;
    s->scramble_reg = (s->scramble_reg << 1) | bit;

    /* We need to strip the last part of the training - the test period of all 1s -
       before we let data go to the application. */
    if (s->in_training == TRAINING_STAGE_NORMAL_OPERATION)
    {
        s->put_bit(s->user_data, out_bit);
    }
    else
    {
        //fprintf(stderr, "bit %5d %d\n", s->training_cd, out_bit);
        /* The bits during the final stage of training should be all ones. However,
           buggy modems mean you cannot rely on this. Therefore we don't bother
           testing for ones, but just rely on a constellation mismatch measurement. */
    }
}
/*- End of function --------------------------------------------------------*/

static void decode_baud(v29_rx_state_t *s, complex_t *z)
{
    static const uint8_t phase_steps_9600[8] =
    {
        1, 0, 2, 3, 7, 6, 4, 5
    };
    static const uint8_t phase_steps_4800[4] =
    {
        0, 1, 3, 2
    };
    int nearest;
    int raw_bits;
    int i;
    int re;
    int im;

    switch (s->bit_rate)
    {
    case 9600:
        re = (z->re + 5.0)*5.0;
        if (re > 49)
            re = 49;
        else if (re < 0)
            re = 0;
        im = (z->im + 5.0)*5.0;
        if (im > 49)
            im = 49;
        else if (im < 0)
            im = 0;
        nearest = space_map_9600[re][im];
        track_carrier(s, z, nearest);
        tune_equalizer(s, z, nearest);
        raw_bits = phase_steps_9600[(nearest - s->constellation_state) & 7];
        s->constellation_state = nearest;
        put_bit(s, nearest >> 3);
        for (i = 0;  i < 3;  i++)
        {
            put_bit(s, raw_bits >> 2);
            raw_bits <<= 1;
        }
        break;
    case 7200:
        /* We can reuse the space map for 9600, but drop the top bit */
        re = (z->re + 5.0)*5.0;
        if (re > 49)
            re = 49;
        else if (re < 0)
            re = 0;
        im = (z->im + 5.0)*5.0;
        if (im > 49)
            im = 49;
        else if (im < 0)
            im = 0;
        nearest = space_map_9600[re][im] & 7;
        track_carrier(s, z, nearest);
        tune_equalizer(s, z, nearest);
        raw_bits = phase_steps_9600[(nearest - s->constellation_state) & 7];
        s->constellation_state = nearest;
        for (i = 0;  i < 3;  i++)
        {
            put_bit(s, raw_bits >> 2);
            raw_bits <<= 1;
        }
        break;
    case 4800:
        nearest = find_quadrant(z);
        track_carrier(s, z, nearest << 1);
        tune_equalizer(s, z, nearest << 1);
        raw_bits = phase_steps_4800[(nearest - s->constellation_state) & 3];
        s->constellation_state = nearest;
        put_bit(s, raw_bits >> 1);
        put_bit(s, raw_bits);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_baud(v29_rx_state_t *s, const complex_t *sample)
{
    static const int cdcd_pos[6] =
    {
        0, 11,
        0,  3,
        0,  2
    };
    complex_t z;
    complex_t zz;
    float p;
    float q;
    int bit;
    int i;
    int j;
    float phase;
    int32_t angle;
    int32_t ang;

    s->rrc_filter[s->rrc_filter_step].re =
    s->rrc_filter[s->rrc_filter_step + V29RX_FILTER_STEPS].re = sample->re;
    s->rrc_filter[s->rrc_filter_step].im =
    s->rrc_filter[s->rrc_filter_step + V29RX_FILTER_STEPS].im = sample->im;
    if (++s->rrc_filter_step >= V29RX_FILTER_STEPS)
        s->rrc_filter_step = 0;
    /* Put things into the equalization buffer at T/2 rate. The Gardner algorithm
       will fiddle the step to align this with the bits. */
    if ((s->eq_put_step -= 3) > 0)
    {
        //fprintf(stderr, "Samp, %f, %f, %f, 0, 0x%X\n", z.re, z.im, sqrt(z.re*z.re + z.im*z.im), s->eq_put_step);
        return;
    }

    /* This is our interpolation filter, as well as our demod filter. */
    j = -s->eq_put_step;
    if (j > 2)
        j = 2;
    z = complex_set(0.0, 0.0);
    for (i = 0;  i < V29RX_FILTER_STEPS;  i++)
    {
        z.re += pulseshaper[j][i]*s->rrc_filter[i + s->rrc_filter_step].re;
        z.im += pulseshaper[j][i]*s->rrc_filter[i + s->rrc_filter_step].im;
    }
    z.re *= 1.0/PULSESHAPER_GAIN;
    z.im *= 1.0/PULSESHAPER_GAIN;

    s->eq_put_step += 5;

    /* Add a sample to the equalizer's circular buffer, but don't calculate anything
       at this time. */
    s->eq_buf[s->eq_step].re = z.re;
    s->eq_buf[s->eq_step].im = z.im;
    s->eq_step = (s->eq_step + 1) & V29_EQUALIZER_MASK;

    /* On alternate insertions we have a whole baud and must process it. */
    if ((++s->baud_phase & 1))
        return;

    /* Perform a Gardner test for baud alignment on the three most recent samples. */
    p = s->eq_buf[(s->eq_step - 3) & V29_EQUALIZER_MASK].re
      - s->eq_buf[(s->eq_step - 1) & V29_EQUALIZER_MASK].re;
    p *= s->eq_buf[(s->eq_step - 2) & V29_EQUALIZER_MASK].re;

    q = s->eq_buf[(s->eq_step - 3) & V29_EQUALIZER_MASK].im
      - s->eq_buf[(s->eq_step - 1) & V29_EQUALIZER_MASK].im;
    q *= s->eq_buf[(s->eq_step - 2) & V29_EQUALIZER_MASK].im;

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
        if (s->gardner_integrate > 0)
        {
            s->eq_put_step++;
            if (s->in_training == TRAINING_STAGE_NORMAL_OPERATION)
            {
                for (i = 0;  i < 2*V29_EQUALIZER_LEN;  i++)
                {
                    s->eq_coeff[i].re = s->eq_coeff[i].re*0.9 + s->eq_coeff[i + 1].re*0.1;
                    s->eq_coeff[i].im = s->eq_coeff[i].im*0.9 + s->eq_coeff[i + 1].im*0.1;
                }
            }
        }
        else
        {
            s->eq_put_step--;
            if (s->in_training == TRAINING_STAGE_NORMAL_OPERATION)
            {
                for (i = 2*V29_EQUALIZER_LEN;  i > 0;  i--)
                {
                    s->eq_coeff[i].re = s->eq_coeff[i].re*0.9 + s->eq_coeff[i - 1].re*0.1;
                    s->eq_coeff[i].im = s->eq_coeff[i].im*0.9 + s->eq_coeff[i - 1].im*0.1;
                }
            }
        }
        if (s->qam_report)
            s->qam_report(s->qam_user_data, NULL, s->gardner_integrate);
        s->gardner_integrate = 0;
    }

    z = equalizer_get(s);

    switch (s->in_training)
    {
    case TRAINING_STAGE_NORMAL_OPERATION:
        /* Normal operation. */
        decode_baud(s, &z);
        break;
    case TRAINING_STAGE_SYMBOL_ACQUISITION:
        /* Allow time for the Gardner algorithm to settle the symbol timing. */
        if (++s->training_count >= 60)
        {
            /* Record the current phase angle */
            s->in_training = TRAINING_STAGE_LOG_PHASE;
            s->angles[0] =
            s->start_angle_a = arctan2(z.im, z.re);
        }
        break;
    case TRAINING_STAGE_LOG_PHASE:
        /* Record the current alternate phase angle */
        angle = arctan2(z.im, z.re);
        s->angles[1] =
        s->start_angle_b = angle;
        s->training_count = 1;
        s->in_training = TRAINING_STAGE_WAIT_FOR_CDCD;
        break;
    case TRAINING_STAGE_WAIT_FOR_CDCD:
        angle = arctan2(z.im, z.re);
        /* Look for the initial ABAB sequence to display a phase reversal, which will
           signal the start of the scrambled CDCD segment */
        ang = angle - s->angles[(s->training_count - 1) & 0xF];
        s->angles[(s->training_count + 1) & 0xF] = angle;
        if ((ang > 0x40000000  ||  ang < -0x40000000)  &&  s->training_count >= 13)
        {
            /* We seem to have a phase reversal */
            /* Slam the carrier frequency into line, based on the total phase drift over the last
               section. Use the shift from the odd bits and the shift from the even bits to get
               better jitter suppression. We need to scale here, or at the maximum specified
               frequency deviation we could overflow, and get a silly answer. */
            /* Step back a few symbols so we not get ISI distorting things. */
            s->training_count = (s->training_count - 8) & ~1;
            ang = (s->angles[s->training_count & 0xF] - s->start_angle_a)/s->training_count
                + (s->angles[(s->training_count | 0x1) & 0xF] - s->start_angle_b)/s->training_count;
            s->carrier_phase_rate += 3*(ang/20);
            fprintf(stderr, "Coarse carrier frequency %7.2f (%d)\n", s->carrier_phase_rate*8000.0/(65536.0*65536.0), s->training_count);

            /* Make a step shift in the phase, to pull it into line. We need to rotate the RRC filter
               buffer and the equalizer buffer, as well as the carrier phase, for this to play out
               nicely. */
            zz = complex_set(cos(angle*2.0*3.14159/(65536.0*65536.0)), sin(angle*2.0*3.14159/(65536.0*65536.0)));
            zz = complex_conj(&zz);
            for (i = 0;  i < 2*V29RX_FILTER_STEPS;  i++)
                s->rrc_filter[i] = complex_mul(&s->rrc_filter[i], &zz);
            for (i = 0;  i <= V29_EQUALIZER_MASK;  i++)
                s->eq_buf[i] = complex_mul(&s->eq_buf[i], &zz);
            s->carrier_phase += angle;

            /* QAM and Gardner only play nicely with heavy damping, so we need to change to
               a slow rate of symbol timing adaption. However, it must not be so slow that it
               cannot track the worst case timing error specified in V.29. This should be 0.01%,
               but since we might be off in the opposite direction from the source, the total
               error could be higher. */
            s->gardner_step = 1;
            /* We have just seen the first bit of the scrambled sequence, so skip it. */
            bit = scrambled_training_bit(s);
            s->training_count = 1;
            s->in_training = TRAINING_STAGE_TRAIN_ON_CDCD;
        }
        else
        {
            if (++s->training_count > V29_TRAINING_SEG_2_LEN)
            {
                /* This is bogus. There are not this many bits in this section
                   of a real training sequence. */
                fprintf(stderr, "Training failed (sequence failed)\n");
                /* Park this modem */
                s->in_training = TRAINING_STAGE_PARKED;
                s->put_bit(s->user_data, PUTBIT_TRAINING_FAILED);
            }
        }
        break;
    case TRAINING_STAGE_TRAIN_ON_CDCD:
        /* Train on the scrambled CDCD section. */
        bit = scrambled_training_bit(s);
        //fprintf(stderr, "%5d %15.5f, %15.5f     %15.5f, %15.5f\n", s->training_count, z.re, z.im, v29_constellation[cdcd_pos[s->training_cd + bit]].re, v29_constellation[cdcd_pos[s->training_cd + bit]].im);
        s->constellation_state = cdcd_pos[s->training_cd + bit];
        track_carrier(s, &z, s->constellation_state);
        tune_equalizer(s, &z, s->constellation_state);
        if (++s->training_count >= V29_TRAINING_SEG_3_LEN - 48)
        {
            s->in_training = TRAINING_STAGE_TRAIN_ON_CDCD_AND_TEST;
            s->training_error = 0.0;
        }
        break;
    case TRAINING_STAGE_TRAIN_ON_CDCD_AND_TEST:
        /* Continue training on the scrambled CDCD section, but measure the quality of training too. */
        bit = scrambled_training_bit(s);
        //fprintf(stderr, "%5d %15.5f, %15.5f     %15.5f, %15.5f\n", s->training_count, z.re, z.im, v29_constellation[cdcd_pos[s->training_cd + bit]].re, v29_constellation[cdcd_pos[s->training_cd + bit]].im);
        s->constellation_state = cdcd_pos[s->training_cd + bit];
        track_carrier(s, &z, s->constellation_state);
        tune_equalizer(s, &z, s->constellation_state);
        /* Measure the training error */
        zz = complex_sub(&z, &v29_constellation[s->constellation_state]);
        s->training_error += power(&zz);
        if (++s->training_count >= V29_TRAINING_SEG_3_LEN)
        {
            fprintf(stderr, "Training error %f\n", s->training_error);
            if (s->training_error < 100.0)
            {
                s->training_count = 0;
                s->training_cd = 0;
                s->training_error = 0.0;
                s->constellation_state = 0;
                s->in_training = TRAINING_STAGE_TEST_ONES;
            }
            else
            {
                fprintf(stderr, "Training failed (convergence failed)\n");
                /* Park this modem */
                s->in_training = TRAINING_STAGE_PARKED;
                s->put_bit(s->user_data, PUTBIT_TRAINING_FAILED);
            }
        }
        break;
    case TRAINING_STAGE_TEST_ONES:
        /* We are in the test phase, where we check that we can receive reliably.
           We should get a run of 1's, 48 symbols (192 bits at 9600bps) long. */
        //printf("%5d %15.5f, %15.5f\n", s->training_count, z.re, z.im);
        decode_baud(s, &z);
        /* Measure the training error */
        zz = complex_sub(&z, &v29_constellation[s->constellation_state]);
        s->training_error += power(&zz);
        if (++s->training_count >= V29_TRAINING_SEG_4_LEN)
        {
            if (s->training_error < 50.0)
            {
                /* We are up and running */
                fprintf(stderr, "Training succeeded (constellation mismatch %f)\n", s->training_error);
                s->in_training = TRAINING_STAGE_NORMAL_OPERATION;
                s->put_bit(s->user_data, PUTBIT_TRAINING_SUCCEEDED);
            }
            else
            {
                /* Training has failed */
                fprintf(stderr, "Training failed (constellation mismatch %f)\n", s->training_error);
                /* Park this modem */
                s->in_training = TRAINING_STAGE_PARKED;
                s->put_bit(s->user_data, PUTBIT_TRAINING_FAILED);
            }
        }
        break;
    case TRAINING_STAGE_PARKED:
    default:
        /* We failed to train! */
        /* Park here until the carrier drops. */
        break;
    }
    if (s->qam_report)
        s->qam_report(s->qam_user_data, &z, s->constellation_state);
}
/*- End of function --------------------------------------------------------*/

void v29_rx(v29_rx_state_t *s, const int16_t *amp, int len)
{
    int i;
    int16_t sample;
    complex_t z;
    int32_t power;
    float x;

    for (i = 0;  i < len;  i++)
    {
        sample = amp[i];
        power = power_meter_update(&(s->power), sample);
        if (s->carrier_present)
        {
            /* Look for power below -31dBm0 to turn the carrier off */
            if (power < s->carrier_off_power)
            {
                v29_rx_restart(s, s->bit_rate);
                s->put_bit(s->user_data, PUTBIT_CARRIER_DOWN);
                continue;
            }
        }
        else
        {
            /* Look for power exceeding -26dBm0 to turn the carrier on */
            if (power < s->carrier_on_power)
                continue;
            s->carrier_present = TRUE;
            s->put_bit(s->user_data, PUTBIT_CARRIER_UP);
        }
        if (s->in_training != TRAINING_STAGE_PARKED)
        {
            /* Only spend effort processing this data if the modem is not
               parked, after training failure. */
            if (s->in_training == TRAINING_STAGE_SYMBOL_ACQUISITION)
            {
                /* Only AGC during the initial training */
                s->agc_scaling = 3.60/sqrt(power);
            }
            x = sample*s->agc_scaling;
            /* Shift to baseband */
            z = complex_dds(&(s->carrier_phase), s->carrier_phase_rate);
            z.re *= x;
            z.im *= x;
            process_baud(s, &z);
        }
    }
}
/*- End of function --------------------------------------------------------*/

void v29_rx_restart(v29_rx_state_t *s, int bit_rate)
{
    int i;

    s->bit_rate = bit_rate;

    memset(s->rrc_filter, 0, sizeof(s->rrc_filter));
    s->rrc_filter_step = 0;

    s->scramble_reg = 0;
    s->training_scramble_reg = 0x2A;
    s->in_training = TRAINING_STAGE_SYMBOL_ACQUISITION;
    switch (s->bit_rate)
    {
    case 4800:
        s->training_cd = 4;
        break;
    case 7200:
        s->training_cd = 2;
        break;
    default:
        s->training_cd = 0;
        break;
    }
    s->training_count = 0;
    s->carrier_present = FALSE;

    s->carrier_phase_rate = complex_dds_phase_step(1700.0);
    s->carrier_phase = 0;
    power_meter_init(&(s->power), 5);
    s->carrier_on_power = power_meter_level(-26);
    s->carrier_off_power = power_meter_level(-31);
    s->agc_scaling = 0.0005;

    s->constellation_state = 0;

    equalizer_reset(s, 0.25);

    s->gardner_integrate = 0;
    s->gardner_step = 0x40;
    s->baud_phase = 0;
    s->lead_lag = 0;
    s->lead_lag_time = 0;
}
/*- End of function --------------------------------------------------------*/

void v29_rx_init(v29_rx_state_t *s, int bit_rate, put_bit_func_t put_bit, void *user_data)
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

    if (!inited)
    {
        /* Build the nearest point map for the constellation */
        for (i = 0;  i < 50;  i++)
        {
            for (j = 0;  j < 50;  j++)
            {
                best = 0;
                best_distance = 100000.0;
                x = (i - 25)/5.0 + 0.1;
                y = (j - 25)/5.0 + 0.1;
                for (k = 0;  k < 16;  k++)
                {
                    distance = (x - v29_constellation[k].re)*(x - v29_constellation[k].re)
                             + (y - v29_constellation[k].im)*(y - v29_constellation[k].im);
                    if (distance <= best_distance)
                    {
                        best_distance = distance;
                        best = k;
                    }
                }
                space_map_9600[i][j] = best;
            }
        }
    }
    
    v29_rx_restart(s, s->bit_rate);
}
/*- End of function --------------------------------------------------------*/

void v29_rx_set_qam_report_handler(v29_rx_state_t *s, qam_report_handler_t *handler, void *user_data)
{
    s->qam_report = handler;
    s->qam_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

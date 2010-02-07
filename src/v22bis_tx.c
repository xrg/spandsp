/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v22bis_tx.c - ITU V.22bis modem transmit part
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
 * $Id: v22bis_tx.c,v 1.9 2005/03/20 04:07:17 steveu Exp $
 */

/*! \file */

/* THIS IS A WORK IN PROGRESS - NOT YET FUNCTIONAL! */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "spandsp/telephony.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/power_meter.h"

#include "spandsp/v29rx.h"
#include "spandsp/v22bis.h"

/* Quoting from the V.22bis spec.

6.3.1.1	Interworking at 2400 bit/s

6.3.1.1.1	Calling modem

a)	On connection to line the calling modem shall be conditioned to receive signals
    in the high channel at 1200 bit/s and transmit signals in the low channel at 1200 bit/s
    in accordance with § 2.5.2.2. It shall apply an ON condition to circuit 107 in accordance
    with Recommendation V.25. The modem shall initially remain silent.

b)	After 155 ± 10 ms of unscrambled binary 1 has been detected, the modem shall remain silent
    for a further 456 ± 10 ms then transmit an unscrambled repetitive double dibit pattern of 00
    and 11 at 1200 bit/s for 100 ± 3 ms. Following this signal the modem shall transmit scrambled
    binary 1 at 1200 bit/s.

c)	If the modem detects scrambled binary 1 in the high channel at 1200 bit/s for 270 ± 40 ms,
    the handshake shall continue in accordance with §§ 6.3.1.2.1 c) and d). However, if unscrambled
    repetitive double dibit 00 and 11 at 1200 bit/s is detected in the high channel, then at the
    end of receipt of this signal the modem shall apply an ON condition to circuit 112.

d)	600 ± 10 ms after circuit 112 has been turned ON the modem shall begin transmitting scrambled
    binary 1 at 2400 bit/s, and 450 ± 10 ms after circuit 112 has been turned ON the receiver may
    begin making 16-way decisions.

e)	Following transmission of scrambled binary 1 at 2400 bit/s for 200 ± 10 ms, circuit 106 shall
    be conditioned to respond to circuit 105 and the modem shall be ready to transmit data at
    2400 bit/s.

f)	When 32 consecutive bits of scrambled binary 1 at 2400 bit/s have been detected in the high
    channel the modem shall be ready to receive data at 2400 bit/s and shall apply an ON condition
    to circuit 109.

6.3.1.1.2	Answering modem

a)	On connection to line the answering modem shall be conditioned to transmit signals in the high
    channel at 1200 bit/s in accordance with § 2.5.2.2 and receive signals in the low channel at
    1200 bit/s. Following transmission of the answer sequence in accordance with Recommendation
    V.25, the modem shall apply an ON condition to circuit 107 and then transmit unscrambled
    binary 1 at 1200 bit/s.

b)	If the modem detects scrambled binary 1 or 0 in the low channel at 1200 bit/s for 270 ± 40 ms,
    the handshake shall continue in accordance with §§ 6.3.1.2.2 b) and c). However, if unscrambled
    repetitive double dibit 00 and 11 at 1200 bit/s is detected in the low channel, at the end of
    receipt of this signal the modem shall apply an ON condition to circuit 112 and then transmit
    an unscrambled repetitive double dibit pattern of 00 and 11 at 1200 bit/s for 100 ± 3 ms.
    Following these signals the modem shall transmit scrambled binary 1 at 1200 bit/s.

c)	600 ± 10 ms after circuit 112 has been turned ON the modem shall begin transmitting scrambled
    binary 1 at 2400 bit/s, and 450 ± 10 ms after circuit 112 has been turned ON the receiver may
    begin making 16-way decisions.

d)	Following transmission of scrambled binary 1 at 2400 bit/s for 200 ± 10 ms, circuit 106 shall
    be conditioned to respond to circuit 105 and the modem shall be ready to transmit data at
    2400 bit/s.

e)	When 32 consecutive bits of scrambled binary 1 at 2400 bit/s have been detected in the low
    channel the modem shall be ready to receive data at 2400 bit/s and shall apply an ON
    condition to circuit 109.

6.3.1.2	Interworking at 1200 bit/s

The following handshake is identical to the Recommendation V.22 alternative A and B handshake.

6.3.1.2.1	Calling modem

a)	On connection to line the calling modem shall be conditioned to receive signals in the high
    channel at 1200 bit/s and transmit signals in the low channel at 1200 bit/s in accordance
    with § 2.5.2.2. It shall apply an ON condition to circuit 107 in accordance with
    Recommendation V.25. The modem shall initially remain silent.

b)	After 155 ± 10 ms of unscrambled binary 1 has been detected, the modem shall remain silent
    for a further 456 ± 10 ms then transmit scrambled binary 1 at 1200 bit/s (a preceding V.22 bis
    signal, as shown in Figure 7/V.22 bis, would not affect the operation of a V.22 answer modem).

c)	On detection of scrambled binary 1 in the high channel at 1200 bit/s for 270 ± 40 ms the modem
    shall be ready to receive data at 1200 bit/s and shall apply an ON condition to circuit 109 and
    an OFF condition to circuit 112.

d)	765 ± 10 ms after circuit 109 has been turned ON, circuit 106 shall be conditioned to respond
    to circuit 105 and the modem shall be ready to transmit data at 1200 bit/s.
 
6.3.1.2.2	Answering modem

a)  On connection to line the answering modem shall be conditioned to transmit signals in the high
    channel at 1200 bit/s in accordance with § 2.5.2.2 and receive signals in the low channel at
    1200 bit/s.

	Following transmission of the answer sequence in accordance with V.25 the modem shall apply
    an ON condition to circuit 107 and then transmit unscrambled binary 1 at 1200 bit/s.

b)	On detection of scrambled binary 1 or 0 in the low channel at 1200 bit/s for 270 ± 40 ms the
    modem shall apply an OFF condition to circuit 112 and shall then transmit scrambled binary 1
    at 1200 bit/s.

c)	After scrambled binary 1 has been transmitted at 1200 bit/s for 765 ± 10 ms the modem shall be
    ready to transmit and receive data at 1200 bit/s, shall condition circuit 106 to respond to
    circuit 105 and shall apply an ON condition to circuit 109.

Note - Manufacturers may wish to note that in certain countries, for national purposes, modems are
       in service which emit an answering tone of 2225 Hz instead of unscrambled binary 1.
*/

#define ms_to_symbols(t)    (((t)*600)/1000)

/* Segments of the training sequence */
enum
{
    V22BIS_TRAINING_STAGE_NORMAL_OPERATION = 0,
    V22BIS_TRAINING_STAGE_INITIAL_SILENCE,
    V22BIS_TRAINING_STAGE_UNSCRAMBLED_ONES,
    V22BIS_TRAINING_STAGE_UNSCRAMBLED_0011,
    V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200,
    V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_2400,
    V22BIS_TRAINING_STAGE_PARKED
};

extern uint8_t space_map_v22bis[30][30];
static int inited = FALSE;

static const int phase_steps[4] =
{
    1, 0, 2, 3
};

const complex_t v22bis_constellation[16] =
{
    { 1.0,  1.0},
    { 1.0,  3.0},
    { 3.0,  1.0},
    { 3.0,  3.0},
    {-1.0,  1.0},
    {-3.0,  1.0},
    {-1.0,  3.0},
    {-3.0,  3.0},
    {-1.0, -1.0},
    {-1.0, -3.0},
    {-3.0, -1.0},
    {-3.0, -3.0},
    { 1.0, -1.0},
    { 3.0, -1.0},
    { 1.0, -3.0},
    { 3.0, -3.0}
};

static int fake_get_bit(void *user_data)
{
    return 1;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int scramble(v22bis_state_t *s, int bit)
{
    int out_bit;

    out_bit = (bit ^ (s->tx_scramble_reg >> 14) ^ (s->tx_scramble_reg >> 17)) & 1;
    if (s->tx_scrambler_pattern_count >= 64)
    {
        out_bit ^= 1;
        s->tx_scrambler_pattern_count = 0;
    }
    if (out_bit == 1)
        s->tx_scrambler_pattern_count++;
    else
        s->tx_scrambler_pattern_count = 0;
    s->tx_scramble_reg = (s->tx_scramble_reg << 1) | out_bit;
    return out_bit;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int get_scrambled_bit(v22bis_state_t *s)
{
    int bit;

    bit = s->current_get_bit(s->user_data);
    if ((bit & 2))
    {
        /* Fill out this symbol with ones, and prepare to send
           the rest of the shutdown sequence. */
        s->current_get_bit = fake_get_bit;
        s->shutdown = 1;
        bit = 1;
    }
    return scramble(s, bit);
}
/*- End of function --------------------------------------------------------*/

static complex_t training_get(v22bis_state_t *s)
{
    complex_t z;
    int bits;

    /* V.22bis training sequence */
    switch (s->tx_training)
    {
    case V22BIS_TRAINING_STAGE_INITIAL_SILENCE:
        /* Segment 1: silence */
        s->tx_constellation_state = 0;
        z = complex_set(0.0, 0.0);
        if (s->caller)
        {
            /* The caller just waits for a signal from the far end, which should be unscrambled ones */
            if (s->detected_unscrambled_ones)
            {
                if (s->bit_rate == 2400)
                {
                    /* Try to establish at 2400bps */
fprintf(stderr, "+++ [%s] starting unscrambled 0011 at 1200\n", (s->caller)  ?  "caller"  :  "answerer");
                    s->tx_training = V22BIS_TRAINING_STAGE_UNSCRAMBLED_0011;
                }
                else
                {
                    /* Only try at 1200bps */
fprintf(stderr, "+++ [%s] starting scrambled ones at 1200 (A)\n", (s->caller)  ?  "caller"  :  "answerer");
                    s->tx_training = V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200;
                }
                s->tx_training_count = 0;
            }
        }
        else
        {
            /* The answerer waits 75ms, then sends unscrambled ones */
            if (++s->tx_training_count >= ms_to_symbols(75))
            {
                /* Inital 75ms of silence is over */
fprintf(stderr, "+++ [%s] starting unscrambled ones at 1200\n", (s->caller)  ?  "caller"  :  "answerer");
                s->tx_training = V22BIS_TRAINING_STAGE_UNSCRAMBLED_ONES;
                s->tx_training_count = 0;
            }
        }
        break;
    case V22BIS_TRAINING_STAGE_UNSCRAMBLED_ONES:
        /* Segment 2: Continuous unscrambled ones at 1200bps (i.e. reversals). */
        /* Only the answering modem sends unscrambled ones */
        s->tx_constellation_state = (s->tx_constellation_state + phase_steps[3]) & 3;
        z = v22bis_constellation[(s->tx_constellation_state << 2) | 1];
        if (s->detected_unscrambled_0011_ending)
        {
            /* We are going to work at 2400bps */
fprintf(stderr, "+++ [%s] [2400] starting unscrambled 0011 at 1200\n", (s->caller)  ?  "caller"  :  "answerer");
            s->bit_rate = 2400;
            s->tx_training = V22BIS_TRAINING_STAGE_UNSCRAMBLED_0011;
            s->tx_training_count = 0;
            break;
        }
        if (s->detected_scrambled_ones_or_zeros_at_1200bps)
        {
            /* We are going to work at 1200bps */
fprintf(stderr, "+++ [%s] [1200] starting scrambled ones at 1200 (B)\n", (s->caller)  ?  "caller"  :  "answerer");
            s->bit_rate = 1200;
            s->tx_training = V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200;
            s->tx_training_count = 0;
            break;
        }
        break;
    case V22BIS_TRAINING_STAGE_UNSCRAMBLED_0011:
        /* Segment 3: Continuous unscrambled double dibit 00 11 at 1200bps. This is only
           sent in 2400bps mode, and lasts 100+-3ms. */
        s->tx_constellation_state = (s->tx_constellation_state + phase_steps[(s->tx_training_count & 1)  ?  3  :  0]) & 3;
        z = v22bis_constellation[(s->tx_constellation_state << 2) | 1];
        if (++s->tx_training_count >= ms_to_symbols(100))
        {
fprintf(stderr, "+++ [%s] starting scrambled ones at 1200 (C)\n", (s->caller)  ?  "caller"  :  "answerer");
            s->tx_training = V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200;
            s->tx_training_count = 0;
        }
        break;
    case V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200:
        /* Segment 4: Scrambled ones at 1200bps. */
        bits = scramble(s, 1);
        bits = (bits << 1) | scramble(s, 1);
        s->tx_constellation_state = (s->tx_constellation_state + phase_steps[bits]) & 3;
        z = v22bis_constellation[(s->tx_constellation_state << 2) | 1];
        if (s->caller)
        {
            if (s->detected_unscrambled_0011_ending)
            {
                /* Continue for a further 600+-10ms */
                if (++s->tx_training_count >= ms_to_symbols(600))
                {
fprintf(stderr, "+++ [%s] starting scrambled ones at 2400 (A)\n", (s->caller)  ?  "caller"  :  "answerer");
                    s->tx_training = V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_2400;
                    s->tx_training_count = 0;
                }
            }
            else if (s->detected_scrambled_ones_or_zeros_at_1200bps)
            {
                if (s->bit_rate == 2400)
                {
                    /* Continue for a further 756+-10ms */
                    if (++s->tx_training_count >= ms_to_symbols(756))
                    {
fprintf(stderr, "+++ [%s] starting scrambled ones at 2400 (B)\n", (s->caller)  ?  "caller"  :  "answerer");
                        s->tx_training = V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_2400;
                        s->tx_training_count = 0;
                    }
                }
                else
                {
fprintf(stderr, "+++ [%s] finished\n", (s->caller)  ?  "caller"  :  "answerer");
                    s->tx_training = V22BIS_TRAINING_STAGE_NORMAL_OPERATION;
                    s->tx_training_count = 0;
                    s->current_get_bit = s->get_bit;
                }
            }
        }
        else
        {
            if (s->bit_rate == 2400)
            {
                if (++s->tx_training_count >= ms_to_symbols(500))
                {
fprintf(stderr, "+++ [%s] starting scrambled ones at 2400 (C)\n", (s->caller)  ?  "caller"  :  "answerer");
                    s->tx_training = V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_2400;
                    s->tx_training_count = 0;
                }
            }
            else
            {
                if (++s->tx_training_count >= ms_to_symbols(756))
                {
fprintf(stderr, "+++ [%s] finished\n", (s->caller)  ?  "caller"  :  "answerer");
                    s->tx_training = 0;
                    s->tx_training_count = 0;
                }
            }
        }
        break;
    case V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_2400:
        /* Segment 4: Scrambled ones at 2400bps. */
        bits = scramble(s, 1);
        bits = (bits << 1) | scramble(s, 1);
        s->tx_constellation_state = (s->tx_constellation_state + phase_steps[bits]) & 3;
        bits = scramble(s, 1);
        bits = (bits << 1) | scramble(s, 1);
        z = v22bis_constellation[(s->tx_constellation_state << 2) | 1];
        if (++s->tx_training_count >= ms_to_symbols(200))
        {
            /* We have completed training. Now handle some real work. */
fprintf(stderr, "+++ [%s] finished\n", (s->caller)  ?  "caller"  :  "answerer");
            s->tx_training = 0;
            s->tx_training_count = 0;
            s->current_get_bit = s->get_bit;
        }
        break;
    case V22BIS_TRAINING_STAGE_PARKED:
        z = complex_set(0.0, 0.0);
        break;
    }
    return z;
}
/*- End of function --------------------------------------------------------*/

static complex_t getbaud(v22bis_state_t *s)
{
    int i;
    int bits;

    if (s->tx_training)
    {
        /* Send the training sequence */
        return training_get(s);
    }

    /* There is no graceful shutdown procedure defined for V.22bis. Just
       send some ones, to ensure we get the real data bits through, even
       with bad ISI. */
    if (s->shutdown)
    {
        if (++s->shutdown > 10)
            return complex_set(0.0, 0.0);
    }
    bits = get_scrambled_bit(s);
    bits = (bits << 1) | get_scrambled_bit(s);
    s->tx_constellation_state = (s->tx_constellation_state + phase_steps[bits]) & 3;
    if (s->bit_rate == 1200)
    {
        bits = 0x01;
    }
    else
    {
        bits = get_scrambled_bit(s);
        bits = (bits << 1) | get_scrambled_bit(s);
    }
    return v22bis_constellation[(s->tx_constellation_state << 2) | bits];
}
/*- End of function --------------------------------------------------------*/

int v22bis_tx(v22bis_state_t *s, int16_t *amp, int len)
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
#define PULSESHAPER_GAIN    13.32182907
    static const float pulseshaper[] =
    {
        /* Raised root cosine pulse shaping; Beta = 0.75; 4 symbols either
           side of the centre. Only one side of the filter is here, as the
           other half is just a mirror image. */
        +0.0066813026, +0.0059389092, +0.0040872704, +0.0013503515,
        -0.0018778344, -0.0050863959, -0.0077248371, -0.0092935151,
        -0.0094333172, -0.0079995719, -0.0051065519, -0.0011325857,
        +0.0033186289, +0.0074981643, +0.0106241551, +0.0120108745,
        +0.0111950748, +0.0080386441, +0.0027883861, -0.0039213358,
        -0.0111297221, -0.0176716757, -0.0223483359, -0.0241244913,
        -0.0223248491, -0.0167982031, -0.0080199318, +0.0028908545,
        +0.0142532693, +0.0240065608, +0.0299785494, +0.0302133859,
        +0.0233201208, +0.0087965076, -0.0127190626, -0.0393099439,
        -0.0677896959, -0.0938726444, -0.1125034692, -0.1183225571,
        -0.1062250888, -0.0719569752, -0.0126819092, +0.0725475817,
        +0.1824755585, +0.3135308122, +0.4599435153, +0.6140917443,
        +0.7670536014, +0.9093168557, +1.0315789319, +1.1255580839,
        +1.1847332680, +1.2049361492
    };

    if (s->shutdown > 10)
        return 0;
    for (sample = 0;  sample < len;  sample++)
    {
        if ((s->tx_baud_phase += 3) > 40)
        {
            s->tx_baud_phase -= 40;
            x = getbaud(s);
            /* Use a weighted value for the first sample of the baud to correct
               for a baud not being an integral number of samples long. This is
               almost as good as 3 times oversampling to a common multiple of the
               baud and sampling rates, but requires less compute. */
            s->tx_rrc_filter[s->tx_rrc_filter_step].re =
            s->tx_rrc_filter[s->tx_rrc_filter_step + V22BIS_TX_FILTER_STEPS].re = x.re - (x.re - s->current_point.re)*weights[s->tx_baud_phase];
            s->tx_rrc_filter[s->tx_rrc_filter_step].im =
            s->tx_rrc_filter[s->tx_rrc_filter_step + V22BIS_TX_FILTER_STEPS].im = x.im - (x.im - s->current_point.im)*weights[s->tx_baud_phase];
            s->current_point = x;
        }
        else
        {
            s->tx_rrc_filter[s->tx_rrc_filter_step] =
            s->tx_rrc_filter[s->tx_rrc_filter_step + V22BIS_TX_FILTER_STEPS] = s->current_point;
        }
        if (++s->tx_rrc_filter_step >= V22BIS_TX_FILTER_STEPS)
            s->tx_rrc_filter_step = 0;
        /* Root raised cosine pulse shaping at baseband */
        x.re = pulseshaper[V22BIS_TX_FILTER_STEPS >> 1]*s->tx_rrc_filter[(V22BIS_TX_FILTER_STEPS >> 1) + s->tx_rrc_filter_step].re;
        x.im = pulseshaper[V22BIS_TX_FILTER_STEPS >> 1]*s->tx_rrc_filter[(V22BIS_TX_FILTER_STEPS >> 1) + s->tx_rrc_filter_step].im;
        for (i = 0;  i < (V22BIS_TX_FILTER_STEPS >> 1);  i++)
        {
            x.re += pulseshaper[i]*(s->tx_rrc_filter[s->tx_rrc_filter_step + i].re + s->tx_rrc_filter[V22BIS_TX_FILTER_STEPS - 1 + s->tx_rrc_filter_step - i].re);
            x.im += pulseshaper[i]*(s->tx_rrc_filter[s->tx_rrc_filter_step + i].im + s->tx_rrc_filter[V22BIS_TX_FILTER_STEPS - 1 + s->tx_rrc_filter_step - i].im);
        }
        /* Now create and modulate the carrier */
        z = dds_complexf(&(s->tx_carrier_phase), s->tx_carrier_phase_rate);
        amp[sample] = (int16_t) ((x.re*z.re + x.im*z.im)*s->tx_gain);
        if (s->guard_phase_rate)
        {
            /* Add the guard tone */
            amp[sample] += dds_modf(&(s->guard_phase), s->guard_phase_rate, s->guard_level, 0);
        }
    }
    return sample;
}
/*- End of function --------------------------------------------------------*/

void v22bis_tx_power(v22bis_state_t *s, float power)
{
    float l;

    l = 1.6*pow(10.0, (power - 3.14)/20.0);
    s->tx_gain = l*32768.0/(PULSESHAPER_GAIN*3.0);
}
/*- End of function --------------------------------------------------------*/

static int v22bis_tx_restart(v22bis_state_t *s, int bit_rate)
{
    s->bit_rate = bit_rate;
    memset(s->tx_rrc_filter, 0, sizeof(s->tx_rrc_filter));
    s->tx_rrc_filter_step = 0;
    s->current_point = complex_set(0.0, 0.0);
    s->tx_scramble_reg = 0;
    s->tx_scrambler_pattern_count = 0;
    s->tx_training = V22BIS_TRAINING_STAGE_INITIAL_SILENCE;
    s->tx_training_count = 0;
    s->tx_carrier_phase = 0;
    s->guard_phase = 0;
    s->tx_baud_phase = 0;
    s->tx_constellation_state = 0;
    s->current_get_bit = fake_get_bit;
    s->shutdown = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

int v22bis_restart(v22bis_state_t *s, int bit_rate)
{
    if (v22bis_tx_restart(s, bit_rate))
        return -1;
    return v22bis_rx_restart(s, bit_rate);
}
/*- End of function --------------------------------------------------------*/

void v22bis_init(v22bis_state_t *s, int bit_rate, int guard, int caller, get_bit_func_t get_bit, put_bit_func_t put_bit, void *user_data)
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
    s->caller = caller;

    s->get_bit = get_bit;
    s->put_bit = put_bit;
    s->user_data = user_data;

    if (!inited)
    {
        /* Build the nearest point map for the constellation */
        for (i = 0;  i < 30;  i++)
        {
            for (j = 0;  j < 30;  j++)
            {
                best = 0;
                best_distance = 100000.0;
                x = (i - 15)/3.0 + 0.1;
                y = (j - 15)/3.0 + 0.1;
                for (k = 0;  k < 16;  k++)
                {
                    distance = (x - v22bis_constellation[k].re)*(x - v22bis_constellation[k].re)
                             + (y - v22bis_constellation[k].im)*(y - v22bis_constellation[k].im);
                    if (distance <= best_distance)
                    {
                        best_distance = distance;
                        best = k;
                    }
                }
                space_map_v22bis[i][j] = best;
            }
        }
        inited = TRUE;
    }
    if (s->caller)
    {
        s->tx_carrier_phase_rate = dds_phase_stepf(1200.0);
    }
    else
    {
        s->tx_carrier_phase_rate = dds_phase_stepf(2400.0);
        if (guard)
        {
            if (guard == 1)
            {
                s->guard_phase_rate = dds_phase_stepf(550.0);
                s->guard_level = 1500;
            }
            else
            {
                s->guard_phase_rate = dds_phase_stepf(1800.0);
                s->guard_level = 1000;
            }
        }
    }
    v22bis_tx_power(s, -10.0);
    v22bis_restart(s, s->bit_rate);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

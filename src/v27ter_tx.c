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
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
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
 * $Id: v27ter_tx.c,v 1.47 2006/11/19 14:07:26 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/complex.h"
#include "spandsp/vector_float.h"
#include "spandsp/complex_vector_float.h"
#include "spandsp/async.h"
#include "spandsp/dds.h"
#include "spandsp/power_meter.h"

#include "spandsp/v27ter_tx.h"

#define CARRIER_NOMINAL_FREQ            1800.0f

/* Segments of the training sequence */
/* V.27ter defines a long and a short sequence. FAX doesn't use the
   short sequence, so it is not implemented here. */
#define V27TER_TRAINING_SEG_1           0
#define V27TER_TRAINING_SEG_2           (V27TER_TRAINING_SEG_1 + 320)
#define V27TER_TRAINING_SEG_3           (V27TER_TRAINING_SEG_2 + 32)
#define V27TER_TRAINING_SEG_4           (V27TER_TRAINING_SEG_3 + 50)
#define V27TER_TRAINING_SEG_5           (V27TER_TRAINING_SEG_4 + 1074)
#define V27TER_TRAINING_END             (V27TER_TRAINING_SEG_5 + 8)
#define V27TER_TRAINING_SHUTDOWN_END    (V27TER_TRAINING_END + 32)

/* Raised root cosine pulse shaping; Beta = 0.5; 4 symbols either
   side of the centre. */
/* Created with mkshape -r 0.025 0.5 181 -l and then split up */
#define PULSESHAPER_2400_GAIN           (19.972065748f/20.0f)
#define PULSESHAPER_2400_COEFF_SETS     20
static const float pulseshaper_2400[PULSESHAPER_2400_COEFF_SETS][V27TER_TX_FILTER_STEPS] =
{
    {
         0.0050051219,          /* Filter 0 */
         0.0107180844,
        -0.0150077814,
        -0.0750272071,
         0.5786341413,
         0.5786341413,
        -0.0750272071,
        -0.0150077814,
         0.0107180844
    },
    {
         0.0036624469,          /* Filter 1 */
         0.0131516633,
        -0.0107913392,
        -0.0957820135,
         0.6671466059,
         0.4891745311,
        -0.0541239470,
        -0.0179109014,
         0.0079099936
    },
    {
         0.0020204744,          /* Filter 2 */
         0.0150588729,
        -0.0053908083,
        -0.1154114754,
         0.7528295479,
         0.4006032722,
        -0.0339459430,
        -0.0194500407,
         0.0048904515
    },
    {
         0.0001596234,          /* Filter 3 */
         0.0163079778,
         0.0009858079,
        -0.1328632049,
         0.8338068363,
         0.3146585634,
        -0.0152415667,
        -0.0196492903,
         0.0018247182
    },
    {
        -0.0018233575,          /* Filter 4 */
         0.0167957238,
         0.0080554403,
        -0.1470417557,
         0.9082626683,
         0.2329352195,
         0.0013822552,
        -0.0186004475,
        -0.0011283792
    },
    {
        -0.0038199491,          /* Filter 5 */
         0.0164546659,
         0.0154676597,
        -0.1568448230,
         0.9744947974,
         0.1568443643,
         0.0154698286,
        -0.0164532877,
        -0.0038242967
    },
    {
        -0.0057152767,          /* Filter 6 */
         0.0152590213,
         0.0228163087,
        -0.1612020164,
         1.0309651039,
         0.0875801110,
         0.0267201501,
        -0.0134037738,
        -0.0061394831
    },
    {
        -0.0073941287,          /* Filter 7 */
         0.0132286539,
         0.0296547979,
        -0.1591148676,
         1.0763457753,
         0.0260941722,
         0.0349842710,
        -0.0096808822,
        -0.0079766730
    },
    {
        -0.0087472825,          /* Filter 8 */
         0.0104308721,
         0.0355146231,
        -0.1496966290,
         1.1095595051,
        -0.0269209682,
         0.0402570324,
        -0.0055327477,
        -0.0092685626
    },
    {
        -0.0096778115,          /* Filter 9 */
         0.0069798134,
         0.0399264862,
        -0.1322103702,
         1.1298123136,
        -0.0710400038,
         0.0426638320,
        -0.0012128224,
        -0.0099797659
    },
    {
        -0.0101070340,          /* Filter 10 */
         0.0030333009,
         0.0424432507,
        -0.1061038872,
         1.1366178484,
        -0.1061038872,
         0.0424432507,
         0.0030333009,
        -0.0101070340
    },
    {
        -0.0099797659,          /* Filter 11 */
        -0.0012128224,
         0.0426638320,
        -0.0710400038,
         1.1298123136,
        -0.1322103702,
         0.0399264862,
         0.0069798134,
        -0.0096778115
    },
    {
        -0.0092685626,          /* Filter 12 */
        -0.0055327477,
         0.0402570324,
        -0.0269209682,
         1.1095595051,
        -0.1496966290,
         0.0355146231,
         0.0104308721,
        -0.0087472825
    },
    {
        -0.0079766730,          /* Filter 13 */
        -0.0096808822,
         0.0349842710,
         0.0260941722,
         1.0763457753,
        -0.1591148676,
         0.0296547979,
         0.0132286539,
        -0.0073941287
    },
    {
        -0.0061394831,          /* Filter 14 */
        -0.0134037738,
         0.0267201501,
         0.0875801110,
         1.0309651039,
        -0.1612020164,
         0.0228163087,
         0.0152590213,
        -0.0057152767
    },
    {
        -0.0038242967,          /* Filter 15 */
        -0.0164532877,
         0.0154698286,
         0.1568443643,
         0.9744947974,
        -0.1568448230,
         0.0154676597,
         0.0164546659,
        -0.0038199491
    },
    {
        -0.0011283792,          /* Filter 16 */
        -0.0186004475,
         0.0013822552,
         0.2329352195,
         0.9082626683,
        -0.1470417557,
         0.0080554403,
         0.0167957238,
        -0.0018233575
    },
    {
         0.0018247182,          /* Filter 17 */
        -0.0196492903,
        -0.0152415667,
         0.3146585634,
         0.8338068363,
        -0.1328632049,
         0.0009858079,
         0.0163079778,
         0.0001596234
    },
    {
         0.0048904515,          /* Filter 18 */
        -0.0194500407,
        -0.0339459430,
         0.4006032722,
         0.7528295479,
        -0.1154114754,
        -0.0053908083,
         0.0150588729,
         0.0020204744
    },
    {
         0.0079099936,          /* Filter 19 */
        -0.0179109014,
        -0.0541239470,
         0.4891745311,
         0.6671466059,
        -0.0957820135,
        -0.0107913392,
         0.0131516633,
         0.0036624469
    },
};

/* Raised root cosine pulse shaping; Beta = 0.5; 4 symbols either
   side of the centre. */
/* Created with mkshape -r 0.1 0.5 45 -l and then split up */
#define PULSESHAPER_4800_GAIN           (4.9913162900f/5.0f)
#define PULSESHAPER_4800_COEFF_SETS     5
static const float pulseshaper_4800[PULSESHAPER_4800_COEFF_SETS][V27TER_TX_FILTER_STEPS] =
{
    {
         0.0020173211,          /* Filter 0 */
         0.0150576434,
        -0.0053888047,
        -0.1154099010,
         0.7528286821,
         0.4006013374,
        -0.0339462085,
        -0.0194477281,
         0.0048918464
    },
    {
        -0.0057162575,          /* Filter 1 */
         0.0152563286,
         0.0228163350,
        -0.1612000503,
         1.0309660372,
         0.0875788553,
         0.0267182476,
        -0.0134032156,
        -0.0061365979
    },
    {
        -0.0101052019,          /* Filter 2 */
         0.0030314952,
         0.0424414442,
        -0.1061032862,
         1.1366196464,
        -0.1061032862,
         0.0424414442,
         0.0030314952,
        -0.0101052019
    },
    {
        -0.0061365979,          /* Filter 3 */
        -0.0134032156,
         0.0267182476,
         0.0875788553,
         1.0309660372,
        -0.1612000503,
         0.0228163350,
         0.0152563286,
        -0.0057162575
    },
    {
         0.0048918464,          /* Filter 4 */
        -0.0194477281,
        -0.0339462085,
         0.4006013374,
         0.7528286821,
        -0.1154099010,
        -0.0053888047,
         0.0150576434,
         0.0020173211
    },
};

static int fake_get_bit(void *user_data)
{
    return 1;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int scramble(v27ter_tx_state_t *s, int in_bit)
{
    int out_bit;

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
    
    if ((bit = s->current_get_bit(s->user_data)) == PUTBIT_END_OF_DATA)
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

static complexf_t getbaud(v27ter_tx_state_t *s)
{
    static const int phase_steps_4800[8] =
    {
        1, 0, 2, 3, 6, 7, 5, 4
    };
    static const int phase_steps_2400[4] =
    {
        0, 2, 6, 4
    };
    static const complexf_t constellation[8] =
    {
        { 1.414f,  0.0f},       /*   0deg */
        { 1.0f,    1.0f},       /*  45deg */
        { 0.0f,    1.414f},     /*  90deg */
        {-1.0f,    1.0f},       /* 135deg */
        {-1.414f,  0.0f},       /* 180deg */
        {-1.0f,   -1.0f},       /* 225deg */
        { 0.0f,   -1.414f},     /* 270deg */
        { 1.0f,   -1.0f}        /* 315deg */
    };
    int bits;

    if (s->in_training)
    {
       	/* Send the training sequence */
        if (++s->training_step <= V27TER_TRAINING_SEG_5)
        {
            if (s->training_step <= V27TER_TRAINING_SEG_4)
            {
                if (s->training_step <= V27TER_TRAINING_SEG_2)
                {
                    /* Segment 1: Unmodulated carrier (talker echo protection) */
                    return constellation[0];
                }
                if (s->training_step <= V27TER_TRAINING_SEG_3)
                {
                    /* Segment 2: Silence */
                    return complex_setf(0.0, 0.0);
                }
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
        if (s->training_step == V27TER_TRAINING_END + 1)
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

int v27ter_tx(v27ter_tx_state_t *s, int16_t amp[], int len)
{
    complexf_t x;
    complexf_t z;
    int i;
    int sample;

    if (s->training_step >= V27TER_TRAINING_SHUTDOWN_END)
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
                s->rrc_filter[s->rrc_filter_step] =
                s->rrc_filter[s->rrc_filter_step + V27TER_TX_FILTER_STEPS] = getbaud(s);
                if (++s->rrc_filter_step >= V27TER_TX_FILTER_STEPS)
                    s->rrc_filter_step = 0;
            }
            /* Root raised cosine pulse shaping at baseband */
            x.re = 0.0f;
            x.im = 0.0f;
            for (i = 0;  i < V27TER_TX_FILTER_STEPS;  i++)
            {
                x.re += pulseshaper_4800[4 - s->baud_phase][i]*s->rrc_filter[i + s->rrc_filter_step].re;
                x.im += pulseshaper_4800[4 - s->baud_phase][i]*s->rrc_filter[i + s->rrc_filter_step].im;
            }
            /* Now create and modulate the carrier */
            z = dds_complexf(&(s->carrier_phase), s->carrier_phase_rate);
            /* Don't bother saturating. We should never clip. */
            amp[sample] = (int16_t) lrintf((x.re*z.re - x.im*z.im)*s->gain_4800);
        }
    }
    else
    {
        for (sample = 0;  sample < len;  sample++)
        {
            if ((s->baud_phase += 3) >= 20)
            {
                s->baud_phase -= 20;
                s->rrc_filter[s->rrc_filter_step] =
                s->rrc_filter[s->rrc_filter_step + V27TER_TX_FILTER_STEPS] = getbaud(s);
                if (++s->rrc_filter_step >= V27TER_TX_FILTER_STEPS)
                    s->rrc_filter_step = 0;
            }
            /* Root raised cosine pulse shaping at baseband */
            x.re = 0.0f;
            x.im = 0.0f;
            for (i = 0;  i < V27TER_TX_FILTER_STEPS;  i++)
            {
                x.re += pulseshaper_2400[19 - s->baud_phase][i]*s->rrc_filter[i + s->rrc_filter_step].re;
                x.im += pulseshaper_2400[19 - s->baud_phase][i]*s->rrc_filter[i + s->rrc_filter_step].im;
            }
            /* Now create and modulate the carrier */
            z = dds_complexf(&(s->carrier_phase), s->carrier_phase_rate);
            /* Don't bother saturating. We should never clip. */
            amp[sample] = (int16_t) lrintf((x.re*z.re - x.im*z.im)*s->gain_2400);
        }
    }
    return sample;
}
/*- End of function --------------------------------------------------------*/

void v27ter_tx_power(v27ter_tx_state_t *s, float power)
{
    float l;

    l = powf(10.0f, (power - DBM0_MAX_POWER)/20.0f)*32768.0f;
    s->gain_2400 = l/PULSESHAPER_2400_GAIN;
    s->gain_4800 = l/PULSESHAPER_4800_GAIN;
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
    cvec_zerof(s->rrc_filter, sizeof(s->rrc_filter)/sizeof(s->rrc_filter[0]));
    s->rrc_filter_step = 0;
    s->scramble_reg = 0x3C;
    s->scrambler_pattern_count = 0;
    s->in_training = TRUE;
    s->training_step = (tep)  ?  V27TER_TRAINING_SEG_1  :  V27TER_TRAINING_SEG_2;
    s->carrier_phase = 0;
    s->baud_phase = 0;
    s->constellation_state = 0;
    s->current_get_bit = fake_get_bit;
    return 0;
}
/*- End of function --------------------------------------------------------*/

v27ter_tx_state_t *v27ter_tx_init(v27ter_tx_state_t *s, int rate, int tep, get_bit_func_t get_bit, void *user_data)
{
    if (s == NULL)
    {
        if ((s = (v27ter_tx_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->get_bit = get_bit;
    s->user_data = user_data;
    s->carrier_phase_rate = dds_phase_ratef(CARRIER_NOMINAL_FREQ);
    v27ter_tx_power(s, -14.0f);
    v27ter_tx_restart(s, rate, tep);
    return s;
}
/*- End of function --------------------------------------------------------*/

int v27ter_tx_release(v27ter_tx_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

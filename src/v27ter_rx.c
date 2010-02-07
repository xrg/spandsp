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
 * $Id: v27ter_rx.c,v 1.32 2004/12/08 14:00:35 steveu Exp $
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
#include "spandsp/dds.h"
#include "spandsp/complex_filters.h"

#include "spandsp/v29rx.h"
#include "spandsp/v27ter_rx.h"

/* V.27ter is a DPSK modem, but this code treats it like QAM. It nails down the
   signal to a static constellation, even though dealing with differences is all
   that is necessary. */

/* Segments of the training sequence */
/* V.27ter defines a long and a short sequence. FAX doesn't use the
   short sequence, so it is not implemented here. */
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

static const complex_t v27ter_constellation[8] =
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

/* Raised root cosine pulse shaping filter set; beta 0.5;
   sample rate 8000; 8 phase steps; baud rate 1600 */
#define PULSESHAPER_4800_GAIN   2.4975
static const float pulseshaper_4800[8][V27RX_4800_FILTER_STEPS] =
{
    {
        -0.0056491642,    /* Filter 0 */
        -0.0006460227,
         0.0065723886,
        -0.0057152767,
        -0.0061394831,
         0.0150588729,
         0.0030333009,
        -0.0194500407,
         0.0228163087,
         0.0267201501,
        -0.1154114754,
        -0.1061038872,
         0.4006032722,
         1.0309651039,
         1.0309651039,
         0.4006032722,
        -0.1061038872,
        -0.1154114754,
         0.0267201501,
         0.0228163087,
        -0.0194500407,
         0.0030333009,
         0.0150588729,
        -0.0061394831,
        -0.0057152767,
         0.0065723886,
        -0.0006460227
    },
    {
        -0.0020967948,    /* Filter 1 */
         0.0008687615,
         0.0059879145,
        -0.0073941287,
        -0.0038242967,
         0.0163079778,
        -0.0012128224,
        -0.0179109014,
         0.0296547979,
         0.0154698286,
        -0.1328632049,
        -0.0710400038,
         0.4891745311,
         1.0763457753,
         0.9744947974,
         0.3146585634,
        -0.1322103702,
        -0.0957820135,
         0.0349842710,
         0.0154676597,
        -0.0196492903,
         0.0069798134,
         0.0131516633,
        -0.0079766730,
        -0.0038199491,
         0.0067433120,
        -0.0020967948
    },
    {
        -0.0034068373,    /* Filter 2 */
         0.0023642749,
         0.0050051219,
        -0.0087472825,
        -0.0011283792,
         0.0167957238,
        -0.0055327477,
        -0.0150077814,
         0.0355146231,
         0.0013822552,
        -0.1470417557,
        -0.0269209682,
         0.5786341413,
         1.1095595051,
         0.9082626683,
         0.2329352195,
        -0.1496966290,
        -0.0750272071,
         0.0402570324,
         0.0080554403,
        -0.0186004475,
         0.0104308721,
         0.0107180844,
        -0.0092685626,
        -0.0018233575,
         0.0065084517,
        -0.0034068373
    },
    {
        -0.0045098365,    /* Filter 3 */
         0.0037550229,
         0.0036624469,
        -0.0096778115,
         0.0018247182,
         0.0164546659,
        -0.0096808822,
        -0.0107913392,
         0.0399264862,
        -0.0152415667,
        -0.1568448230,
         0.0260941722,
         0.6671466059,
         1.1298123136,
         0.8338068363,
         0.1568443643,
        -0.1591148676,
        -0.0541239470,
         0.0426638320,
         0.0009858079,
        -0.0164532877,
         0.0132286539,
         0.0079099936,
        -0.0099797659,
         0.0001596234,
         0.0058971713,
        -0.0045098365
    },
    {
        -0.0053530229,    /* Filter 4 */
         0.0049579538,
         0.0020204744,
        -0.0101070340,
         0.0048904515,
         0.0152590213,
        -0.0134037738,
        -0.0053908083,
         0.0424432507,
        -0.0339459430,
        -0.1612020164,
         0.0875801110,
         0.7528295479,
         1.1366178484,
         0.7528295479,
         0.0875801110,
        -0.1612020164,
        -0.0339459430,
         0.0424432507,
        -0.0053908083,
        -0.0134037738,
         0.0152590213,
         0.0048904515,
        -0.0101070340,
         0.0020204744,
         0.0049579538,
        -0.0053530229
    },
    {
        -0.0058994754,    /* Filter 5 */
         0.0058971713,
         0.0001596234,
        -0.0099797659,
         0.0079099936,
         0.0132286539,
        -0.0164532877,
         0.0009858079,
         0.0426638320,
        -0.0541239470,
        -0.1591148676,
         0.1568443643,
         0.8338068363,
         1.1298123136,
         0.6671466059,
         0.0260941722,
        -0.1568448230,
        -0.0152415667,
         0.0399264862,
        -0.0107913392,
        -0.0096808822,
         0.0164546659,
         0.0018247182,
        -0.0096778115,
         0.0036624469,
         0.0037550229,
        -0.0058994754
    },
    {
        -0.0061294998,    /* Filter 6 */
         0.0065084517,
        -0.0018233575,
        -0.0092685626,
         0.0107180844,
         0.0104308721,
        -0.0186004475,
         0.0080554403,
         0.0402570324,
        -0.0750272071,
        -0.1496966290,
         0.2329352195,
         0.9082626683,
         1.1095595051,
         0.5786341413,
        -0.0269209682,
        -0.1470417557,
         0.0013822552,
         0.0355146231,
        -0.0150077814,
        -0.0055327477,
         0.0167957238,
        -0.0011283792,
        -0.0087472825,
         0.0050051219,
         0.0023642749,
        -0.0061294998
    },
    {
        -0.0060410444,    /* Filter 7 */
         0.0067433120,
        -0.0038199491,
        -0.0079766730,
         0.0131516633,
         0.0069798134,
        -0.0196492903,
         0.0154676597,
         0.0349842710,
        -0.0957820135,
        -0.1322103702,
         0.3146585634,
         0.9744947974,
         1.0763457753,
         0.4891745311,
        -0.0710400038,
        -0.1328632049,
         0.0154698286,
         0.0296547979,
        -0.0179109014,
        -0.0012128224,
         0.0163079778,
        -0.0038242967,
        -0.0073941287,
         0.0059879145,
         0.0008687615,
        -0.0060410444
    },
};

/* Raised root cosine pulse shaping filter set; beta 0.5;
   sample rate 8000; 12 phase steps; baud rate 1200 */
#define PULSESHAPER_2400_GAIN   2.223
static const float pulseshaper_2400[12][V27RX_2400_FILTER_STEPS] =
{
    {
         0.0040769982,    /* Filter 0 */
        -0.0012275605,
        -0.0049362295,
         0.0062069190,
        -0.0028157043,
        -0.0070953912,
         0.0157320351,
        -0.0033639633,
        -0.0130319146,
         0.0414077103,
        -0.0438766480,
        -0.1553493164,
         0.2731350497,
         1.0040582885,
         1.0040582885,
         0.2731350497,
        -0.1553493164,
        -0.0438766480,
         0.0414077103,
        -0.0130319146,
        -0.0033639633,
         0.0157320351,
        -0.0070953912,
        -0.0028157043,
         0.0062069190,
        -0.0049362295,
        -0.0012275605
    },
    {
        -0.0003164916,    /* Filter 1 */
        -0.0021321037,
        -0.0042318116,
         0.0065623412,
        -0.0042896738,
        -0.0055806353,
         0.0164630292,
        -0.0065745583,
        -0.0095259183,
         0.0426873303,
        -0.0592899358,
        -0.1460899979,
         0.3357675370,
         1.0434222292,
         0.9588082045,
         0.2132935683,
        -0.1602630794,
        -0.0290945663,
         0.0389452881,
        -0.0158290870,
        -0.0001275473,
         0.0146001392,
        -0.0083203646,
        -0.0013177606,
         0.0056530499,
        -0.0054835378,
        -0.0003164916
    },
    {
         0.0005737401,    /* Filter 2 */
        -0.0030022143,
        -0.0033883251,
         0.0067017066,
        -0.0056909140,
        -0.0038092458,
         0.0167547566,
        -0.0096537714,
        -0.0053779951,
         0.0426242926,
        -0.0749977137,
        -0.1322008501,
         0.4005639899,
         1.0763765976,
         0.9082687487,
         0.1568062301,
        -0.1611687097,
        -0.0152377934,
         0.0354782573,
        -0.0178756446,
         0.0030336114,
         0.0131159170,
        -0.0092322790,
         0.0001565015,
         0.0049238702,
        -0.0058609590,
         0.0005737401
    },
    {
         0.0014171277,    /* Filter 3 */
        -0.0038102922,
        -0.0024286670,
         0.0066136619,
        -0.0069710348,
        -0.0018236294,
         0.0165806913,
        -0.0124945205,
        -0.0006798630,
         0.0410826901,
        -0.0906250251,
        -0.1134586969,
         0.4668420465,
         1.1024802606,
         0.8530998842,
         0.1041650291,
        -0.1584481344,
        -0.0025545571,
         0.0311974391,
        -0.0191545113,
         0.0060256074,
         0.0113363032,
        -0.0098180338,
         0.0015619067,
         0.0040471706,
        -0.0060612167,
         0.0014171277
    },
    {
         0.0021897709,    /* Filter 4 */
        -0.0045299088,
        -0.0013800219,
         0.0062936717,
        -0.0080835175,
         0.0003254674,
         0.0159274930,
        -0.0149913641,
         0.0044529279,
         0.0379559316,
        -0.1057641102,
        -0.0897085261,
         0.5338738058,
         1.1213819589,
         0.7940136124,
         0.0557850267,
        -0.1525186365,
         0.0087549160,
         0.0263006089,
        -0.0196723029,
         0.0087640663,
         0.0093245776,
        -0.0100746997,
         0.0028570218,
         0.0030544124,
        -0.0060826017,
         0.0021897709
    },
    {
         0.0028705429,    /* Filter 5 */
        -0.0051366589,
        -0.0002731432,
         0.0057443898,
        -0.0089852184,
         0.0025801662,
         0.0147960226,
        -0.0170437835,
         0.0098826994,
         0.0331717251,
        -0.1199808311,
        -0.0608695009,
         0.6008966437,
         1.1328263630,
         0.7317620368,
         0.0120000944,
        -0.1438234060,
         0.0185393950,
         0.0209869935,
        -0.0194577305,
         0.0111761225,
         0.0071482527,
        -0.0100092186,
         0.0040052625,
         0.0019796647,
        -0.0059288690,
         0.0028705429
    },
    {
         0.0034416571,    /* Filter 6 */
        -0.0056089739,
         0.0008584964,
         0.0049758208,
        -0.0096378250,
         0.0048769273,
         0.0132019785,
        -0.0185594993,
         0.0154520586,
         0.0266965368,
        -0.1328219887,
        -0.0269397648,
         0.6671248728,
         1.1366584852,
         0.6671248728,
        -0.0269397648,
        -0.1328219887,
         0.0266965368,
         0.0154520586,
        -0.0185594993,
         0.0132019785,
         0.0048769273,
        -0.0096378250,
         0.0049758208,
         0.0008584964,
        -0.0056089739,
         0.0034416571
    },
    {
         0.0038891180,    /* Filter 7 */
        -0.0059288690,
         0.0019796647,
         0.0040052625,
        -0.0100092186,
         0.0071482527,
         0.0111761225,
        -0.0194577305,
         0.0209869935,
         0.0185393950,
        -0.1438234060,
         0.0120000944,
         0.7317620368,
         1.1328263630,
         0.6008966437,
        -0.0608695009,
        -0.1199808311,
         0.0331717251,
         0.0098826994,
        -0.0170437835,
         0.0147960226,
         0.0025801662,
        -0.0089852184,
         0.0057443898,
        -0.0002731432,
        -0.0051366589,
         0.0038891180
    },
    {
         0.0042030467,    /* Filter 8 */
        -0.0060826017,
         0.0030544124,
         0.0028570218,
        -0.0100746997,
         0.0093245776,
         0.0087640663,
        -0.0196723029,
         0.0263006089,
         0.0087549160,
        -0.1525186365,
         0.0557850267,
         0.7940136124,
         1.1213819589,
         0.5338738058,
        -0.0897085261,
        -0.1057641102,
         0.0379559316,
         0.0044529279,
        -0.0149913641,
         0.0159274930,
         0.0003254674,
        -0.0080835175,
         0.0062936717,
        -0.0013800219,
        -0.0045299088,
         0.0042030467
    },
    {
         0.0043778743,    /* Filter 9 */
        -0.0060612167,
         0.0040471706,
         0.0015619067,
        -0.0098180338,
         0.0113363032,
         0.0060256074,
        -0.0191545113,
         0.0311974391,
        -0.0025545571,
        -0.1584481344,
         0.1041650291,
         0.8530998842,
         1.1024802606,
         0.4668420465,
        -0.1134586969,
        -0.0906250251,
         0.0410826901,
        -0.0006798630,
        -0.0124945205,
         0.0165806913,
        -0.0018236294,
        -0.0069710348,
         0.0066136619,
        -0.0024286670,
        -0.0038102922,
         0.0043778743
    },
    {
         0.0044123994,    /* Filter 10 */
        -0.0058609590,
         0.0049238702,
         0.0001565015,
        -0.0092322790,
         0.0131159170,
         0.0030336114,
        -0.0178756446,
         0.0354782573,
        -0.0152377934,
        -0.1611687097,
         0.1568062301,
         0.9082687487,
         1.0763765976,
         0.4005639899,
        -0.1322008501,
        -0.0749977137,
         0.0426242926,
        -0.0053779951,
        -0.0096537714,
         0.0167547566,
        -0.0038092458,
        -0.0056909140,
         0.0067017066,
        -0.0033883251,
        -0.0030022143,
         0.0044123994
    },
    {
         0.0043097136,    /* Filter 11 */
        -0.0054835378,
         0.0056530499,
        -0.0013177606,
        -0.0083203646,
         0.0146001392,
        -0.0001275473,
        -0.0158290870,
         0.0389452881,
        -0.0290945663,
        -0.1602630794,
         0.2132935683,
         0.9588082045,
         1.0434222292,
         0.3357675370,
        -0.1460899979,
        -0.0592899358,
         0.0426873303,
        -0.0095259183,
        -0.0065745583,
         0.0164630292,
        -0.0055806353,
        -0.0042896738,
         0.0065623412,
        -0.0042318116,
        -0.0021321037,
         0.0043097136
    },
};

float v27ter_rx_carrier_frequency(v27ter_rx_state_t *s)
{
    return s->carrier_phase_rate*(float) SAMPLE_RATE/(65536.0*65536.0);
}
/*- End of function --------------------------------------------------------*/

float v27ter_rx_symbol_timing_correction(v27ter_rx_state_t *s)
{
    return s->gardner_total_correction;
}
/*- End of function --------------------------------------------------------*/

float v27ter_rx_signal_power(v27ter_rx_state_t *s)
{
    return power_meter_dbm0(&s->power);
}
/*- End of function --------------------------------------------------------*/

int v27ter_rx_equalizer_state(v27ter_rx_state_t *s, complex_t **coeffs)
{
    *coeffs = s->eq_coeff;
    return 2*V29_EQUALIZER_LEN + 1;
}
/*- End of function --------------------------------------------------------*/

static void equalizer_reset(v27ter_rx_state_t *s, float delta)
{
    int i;

    /* Start with an equalizer based on everything being perfect */
    for (i = 0;  i < 2*V27_EQUALIZER_LEN + 1;  i++)
        s->eq_coeff[i] = complex_set(0.0, 0.0);
    s->eq_coeff[V27_EQUALIZER_LEN] = complex_set(1.414, 0.0);
    for (i = 0;  i <= V27_EQUALIZER_MASK;  i++)
        s->eq_buf[i] = complex_set(0.0, 0.0);

    s->eq_put_step = (s->bit_rate == 4800)  ?  20  :  40;
    s->eq_step = 0;
    s->eq_delta = delta;
}
/*- End of function --------------------------------------------------------*/

static __inline__ complex_t equalizer_get(v27ter_rx_state_t *s)
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

static void tune_equalizer(v27ter_rx_state_t *s, const complex_t *z, const complex_t *target)
{
    int i;
    int p;
    complex_t ez;
    complex_t z1;

    /* Find the x and y mismatch from the exact constellation position. */
    ez = complex_sub(target, z);

    ez.re *= s->eq_delta;
    ez.im *= s->eq_delta;
    for (i = 0;  i <= 2*V27_EQUALIZER_LEN;  i++)
    {
        p = (s->eq_step + i) & V27_EQUALIZER_MASK;
        z1 = complex_conj(&s->eq_buf[p]);
        z1 = complex_mul(&ez, &z1);
        s->eq_coeff[i] = complex_add(&s->eq_coeff[i], &z1);
        /* If we don't leak a little bit we seem to get some wandering adaption */
        s->eq_coeff[i].re *= 0.9999;
        s->eq_coeff[i].im *= 0.9999;
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ void track_carrier(v27ter_rx_state_t *s, const complex_t *z, const complex_t *target)
{
    complex_t zz;

    /* For small errors the imaginary part of zz is now proportional to the phase error,
       for any particular target. However, the different amplitudes of the various target
       positions scale things. */
    zz = complex_conj(target);
    zz = complex_mul(z, &zz);
    
    s->carrier_phase_rate += s->carrier_track_i*zz.im;
    s->carrier_phase += s->carrier_track_p*zz.im;
    //fprintf(stderr, "Im = %15.5f   f = %15.5f\n", zz.im, s->carrier_phase_rate*8000.0/(65536.0*65536.0));
}
/*- End of function --------------------------------------------------------*/

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
    s->scramble_reg <<= 1;
    if (s->in_training > TRAINING_STAGE_NORMAL_OPERATION  &&  s->in_training < TRAINING_STAGE_TEST_ONES)
        s->scramble_reg |= out_bit;
    else
        s->scramble_reg |= in_bit;
    return out_bit;
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

static __inline__ int find_quadrant(const complex_t *z)
{
    int b1;
    int b2;

    /* Split the space along the two diagonals. */
    b1 = (z->im > z->re);
    b2 = (z->im < -z->re);
    return (b2 << 1) | (b1 ^ b2);
}
/*- End of function --------------------------------------------------------*/

static __inline__ int find_octant(complex_t *z)
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

static void decode_baud(v27ter_rx_state_t *s, complex_t *z)
{
    static const uint8_t phase_steps_4800[8] =
    {
        4, 0, 2, 6, 7, 3, 1, 5
    };
    static const uint8_t phase_steps_2400[4] =
    {
        0, 2, 3, 1
    };
    int nearest;
    int raw_bits;

    switch (s->bit_rate)
    {
    case 4800:
        nearest = find_octant(z);
        raw_bits = phase_steps_4800[(nearest - s->constellation_state) & 7];
        put_bit(s, raw_bits);
        put_bit(s, raw_bits >> 1);
        put_bit(s, raw_bits >> 2);
        s->constellation_state = nearest;
        break;
    case 2400:
        nearest = find_quadrant(z);
        raw_bits = phase_steps_2400[(nearest - s->constellation_state) & 3];
        put_bit(s, raw_bits);
        put_bit(s, raw_bits >> 1);
        s->constellation_state = nearest;
        nearest <<= 1;
        break;
    }
    track_carrier(s, z, &v27ter_constellation[nearest]);
    if (--s->eq_skip <= 0)
    {
        /* Once we are in the data the equalization should not need updating.
           However, the line characteristics may slowly drift. We, therefore,
           tune up on the occassional sample, keeping the compute down. */
        s->eq_skip = 100;
        tune_equalizer(s, z, &v27ter_constellation[nearest]);
    }
}
/*- End of function --------------------------------------------------------*/

static inline void process_baud(v27ter_rx_state_t *s, const complex_t *sample)
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
        if ((s->eq_put_step -= 8) > 0)
        {
            //fprintf(stderr, "Samp, %f, %f, %f, 0, 0x%X\n", z.re, z.im, sqrt(z.re*z.re + z.im*z.im), s->eq_put_step);
            return;
        }

        /* This is our interpolation filter, as well as our demod filter. */
        j = -s->eq_put_step;
        if (j > 8 - 1)
            j = 8 - 1;
        z = complex_set(0.0, 0.0);
        for (i = 0;  i < V27RX_4800_FILTER_STEPS;  i++)
        {
            z.re += pulseshaper_4800[j][i]*s->rrc_filter[i + s->rrc_filter_step].re;
            z.im += pulseshaper_4800[j][i]*s->rrc_filter[i + s->rrc_filter_step].im;
        }
        z.re *= 0.5*1.0/PULSESHAPER_4800_GAIN;
        z.im *= 0.5*1.0/PULSESHAPER_4800_GAIN;

        s->eq_put_step += 20;
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
        if ((s->eq_put_step -= 12) > 0)
        {
            //fprintf(stderr, "Samp, %f, %f, %f, 0, 0x%X\n", z.re, z.im, sqrt(z.re*z.re + z.im*z.im), s->eq_put_step);
            return;
        }

        /* This is our interpolation filter and phase shifter, as well as our demod filter. */
        j = -s->eq_put_step;
        if (j > 12 - 1)
            j = 12 - 1;
        z = complex_set(0.0, 0.0);
        for (i = 0;  i < V27RX_2400_FILTER_STEPS;  i++)
        {
            z.re += pulseshaper_2400[j][i]*s->rrc_filter[i + s->rrc_filter_step].re;
            z.im += pulseshaper_2400[j][i]*s->rrc_filter[i + s->rrc_filter_step].im;
        }
        z.re *= 1.0/PULSESHAPER_2400_GAIN;
        z.im *= 1.0/PULSESHAPER_2400_GAIN;

        s->eq_put_step += 40;
    }

    /* Add a sample to the equalizer's circular buffer, but don't calculate anything
       at this time. */
    s->eq_buf[s->eq_step].re = z.re;
    s->eq_buf[s->eq_step].im = z.im;
    s->eq_step = (s->eq_step + 1) & V27_EQUALIZER_MASK;
        
    /* On alternate insertions we have a whole baud, and must process it. */
    if ((s->baud_phase ^= 1))
    {
        //fprintf(stderr, "Samp, %f, %f, %f, -1, 0x%X\n", z.re, z.im, sqrt(z.re*z.re + z.im*z.im), s->eq_put_step);
        return;
    }
    //fprintf(stderr, "Samp, %f, %f, %f, 1, 0x%X\n", z.re, z.im, sqrt(z.re*z.re + z.im*z.im), s->eq_put_step);

    /* Perform a Gardner test for baud alignment */
    p = s->eq_buf[(s->eq_step - 3) & V27_EQUALIZER_MASK].re
      - s->eq_buf[(s->eq_step - 1) & V27_EQUALIZER_MASK].re;
    p *= s->eq_buf[(s->eq_step - 2) & V27_EQUALIZER_MASK].re;

    q = s->eq_buf[(s->eq_step - 3) & V27_EQUALIZER_MASK].im
      - s->eq_buf[(s->eq_step - 1) & V27_EQUALIZER_MASK].im;
    q *= s->eq_buf[(s->eq_step - 2) & V27_EQUALIZER_MASK].im;

    s->gardner_integrate += (p + q > 0.0)  ?  s->gardner_step  :  -s->gardner_step;

    if (abs(s->gardner_integrate) >= 256)
    {
        /* This integrate and dump approach avoids rapid changes of the equalizer put step.
           Rapid changes, without hysteresis, are bad. They degrade the equalizer performance
           when the true symbol boundary is close to a sample boundary. */
        //fprintf(stderr, "Hop %d\n", s->gardner_integrate);
        s->eq_put_step += (s->gardner_integrate/256);
        s->gardner_total_correction += (s->gardner_integrate/256);
        if (s->qam_report)
            s->qam_report(s->qam_user_data, NULL, NULL, s->gardner_integrate);
        s->gardner_integrate = 0;
    }
    //fprintf(stderr, "Gardner=%10.5f 0x%X\n", p, s->eq_put_step);

    z = equalizer_get(s);

    //fprintf(stderr, "Equalized symbol - %15.5f %15.5f\n", z.re, z.im);
    switch (s->in_training)
    {
    case TRAINING_STAGE_NORMAL_OPERATION:
        decode_baud(s, &z);
        break;
    case TRAINING_STAGE_SYMBOL_ACQUISITION:
        /* Allow time for the Gardner algorithm to settle the baud timing */
        /* Don't start narrowing the bandwidth of the Gardner algorithm too early.
           Some modems are a bit wobbly when they start sending the signal. Also, we start
           this analysis before our filter buffers have completely filled. */
        if (++s->training_count >= 30)
        {
            s->gardner_step = 32;
            s->in_training = TRAINING_STAGE_LOG_PHASE;
            s->angles[0] =
            s->start_angles[0] = arctan2(z.im, z.re);
        }
        break;
    case TRAINING_STAGE_LOG_PHASE:
        /* Record the current alternate phase angle */
        angle = arctan2(z.im, z.re);
        s->angles[1] =
        s->start_angles[1] = angle;
        s->training_count = 1;
        s->in_training = TRAINING_STAGE_WAIT_FOR_HOP;
        break;
    case TRAINING_STAGE_WAIT_FOR_HOP:
        angle = arctan2(z.im, z.re);
        /* Look for the initial ABAB sequence to display a phase reversal, which will
           signal the start of the scrambled ABAB segment */
        ang = angle - s->angles[(s->training_count - 1) & 0xF];
        s->angles[(s->training_count + 1) & 0xF] = angle;
        if ((ang > 0x20000000  ||  ang < -0x20000000)  &&  s->training_count >= 3)
        {
            /* We seem to have a phase reversal */
            /* Slam the carrier frequency into line, based on the total phase drift over the last
               section. Use the shift from the odd bits and the shift from the even bits to get
               better jitter suppression. We need to scale here, or at the maximum specified
               frequency deviation we could overflow, and get a silly answer. */
            /* Step back a few symbols so we don't get ISI distorting things. */
            i = (s->training_count - 8) & ~1;
            j = i & 0xF;
            ang = (s->angles[j] - s->start_angles[0])/i
                + (s->angles[j | 0x1] - s->start_angles[1])/i;
            if (s->bit_rate == 4800)
                s->carrier_phase_rate += ang/10;
            else
                s->carrier_phase_rate += 3*(ang/40);
            fprintf(stderr, "Coarse carrier frequency %7.2f (%d)\n", s->carrier_phase_rate*8000.0/(65536.0*65536.0), s->training_count);

            /* Make a step shift in the phase, to pull it into line. We need to rotate the RRC filter
               buffer and the equalizer buffer, as well as the carrier phase, for this to play out
               nicely. */
            angle += 0x80000000;
            zz = complex_set(cos(angle*2.0*3.14159/(65536.0*65536.0)), sin(angle*2.0*3.14159/(65536.0*65536.0)));
            zz = complex_conj(&zz);
            for (i = 0;  i < 2*V27RX_FILTER_STEPS;  i++)
                s->rrc_filter[i] = complex_mul(&s->rrc_filter[i], &zz);
            for (i = 0;  i <= V27_EQUALIZER_MASK;  i++)
                s->eq_buf[i] = complex_mul(&s->eq_buf[i], &zz);
            s->carrier_phase += angle;

            s->gardner_step = 1;
            /* We have just seen the first element of the scrambled sequence so skip it. */
            s->training_bc = 1;
            s->training_bc ^= descramble(s, 1);
            descramble(s, 1);
            descramble(s, 1);
            s->training_count = 1;
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
        s->constellation_state = abab_pos[s->training_bc];
        tune_equalizer(s, &z, &v27ter_constellation[s->constellation_state]);
        track_carrier(s, &z, &v27ter_constellation[s->constellation_state]);

        if (++s->training_count >= V27_TRAINING_SEG_5_LEN)
        {
            s->constellation_state = (s->bit_rate == 4800)  ?  4  :  2;
            s->training_count = 0;
            s->in_training = TRAINING_STAGE_TEST_ONES;
            s->carrier_track_i = 400.0;
            s->carrier_track_p = 1000000.0;
        }
        break;
    case TRAINING_STAGE_TEST_ONES:
        decode_baud(s, &z);
        if (++s->training_count >= V27_TRAINING_SEG_6_LEN)
        {
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
        }
        break;
    case TRAINING_STAGE_PARKED:
        /* We failed to train! */
        /* Park here until the carrier drops. */
        break;
    }
    if (s->qam_report)
    {
        s->qam_report(s->qam_user_data,
                      &z,
                      &v27ter_constellation[s->constellation_state],
                      s->constellation_state);
    }
}
/*- End of function --------------------------------------------------------*/

int v27ter_rx(v27ter_rx_state_t *s, const int16_t *amp, int len)
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
        //fprintf(stderr, "Power = %f\n", power_meter_dbm0(&(s->power)));
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
            s->agc_scaling = 1.414/sqrt(power);
            //fprintf(stderr, "AGC %f %f - %d %f %f\n", 3.60/sqrt(power), s->agc_scaling, sample, power_meter_dbm0(&(s->power)), 0.018890*0.1);
        }
        x = sample*s->agc_scaling;
        /* Shift to baseband */
        z = dds_complexf(&(s->carrier_phase), s->carrier_phase_rate);
        z.re *= x;
        z.im *= x;
        process_baud(s, &z);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int v27ter_rx_restart(v27ter_rx_state_t *s, int bit_rate)
{
    if (bit_rate != 4800  &&  bit_rate != 2400)
        return -1;
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

    s->carrier_phase_rate = dds_phase_stepf(1800.0);
    s->carrier_phase = 0;
    s->carrier_track_i = 200000.0;
    s->carrier_track_p = 10000000.0;
    power_meter_init(&(s->power), 4);
    s->carrier_on_power = power_meter_level(-43);
    s->carrier_off_power = power_meter_level(-48);
    s->agc_scaling = 0.0005;

    s->constellation_state = 0;

    equalizer_reset(s, 0.03);
    s->eq_skip = 0;

    s->gardner_integrate = 0;
    s->gardner_total_correction = 0;
    s->gardner_step = 512;
    s->baud_phase = 0;

    return 0;
}
/*- End of function --------------------------------------------------------*/

void v27ter_rx_init(v27ter_rx_state_t *s, int bit_rate, put_bit_func_t put_bit, void *user_data)
{
    memset(s, 0, sizeof(*s));
    s->put_bit = put_bit;
    s->user_data = user_data;

    v27ter_rx_restart(s, bit_rate);
}
/*- End of function --------------------------------------------------------*/

void v27ter_rx_set_qam_report_handler(v27ter_rx_state_t *s, qam_report_handler_t *handler, void *user_data)
{
    s->qam_report = handler;
    s->qam_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

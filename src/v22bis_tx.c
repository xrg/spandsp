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
 * $Id: v22bis_tx.c,v 1.33 2006/11/19 14:07:26 steveu Exp $
 */

/*! \file */

/* THIS IS A WORK IN PROGRESS - NOT YET FUNCTIONAL! */

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

#include "spandsp/v29rx.h"
#include "spandsp/v22bis.h"

/* Quoting from the V.22bis spec.

6.3.1.1 Interworking at 2400 bit/s

6.3.1.1.1   Calling modem

a)  On connection to line the calling modem shall be conditioned to receive signals
    in the high channel at 1200 bit/s and transmit signals in the low channel at 1200 bit/s
    in accordance with section 2.5.2.2. It shall apply an ON condition to circuit 107 in accordance
    with Recommendation V.25. The modem shall initially remain silent.

b)  After 155 +-10 ms of unscrambled binary 1 has been detected, the modem shall remain silent
    for a further 456 +-10 ms then transmit an unscrambled repetitive double dibit pattern of 00
    and 11 at 1200 bit/s for 100 +-3 ms. Following this signal the modem shall transmit scrambled
    binary 1 at 1200 bit/s.

c)  If the modem detects scrambled binary 1 in the high channel at 1200 bit/s for 270 +-40 ms,
    the handshake shall continue in accordance with section 6.3.1.2.1 c) and d). However, if unscrambled
    repetitive double dibit 00 and 11 at 1200 bit/s is detected in the high channel, then at the
    end of receipt of this signal the modem shall apply an ON condition to circuit 112.

d)  600 +-10 ms after circuit 112 has been turned ON the modem shall begin transmitting scrambled
    binary 1 at 2400 bit/s, and 450 +-10 ms after circuit 112 has been turned ON the receiver may
    begin making 16-way decisions.

e)  Following transmission of scrambled binary 1 at 2400 bit/s for 200 +-10 ms, circuit 106 shall
    be conditioned to respond to circuit 105 and the modem shall be ready to transmit data at
    2400 bit/s.

f)  When 32 consecutive bits of scrambled binary 1 at 2400 bit/s have been detected in the high
    channel the modem shall be ready to receive data at 2400 bit/s and shall apply an ON condition
    to circuit 109.

6.3.1.1.2   Answering modem

a)  On connection to line the answering modem shall be conditioned to transmit signals in the high
    channel at 1200 bit/s in accordance with  section 2.5.2.2 and receive signals in the low channel at
    1200 bit/s. Following transmission of the answer sequence in accordance with Recommendation
    V.25, the modem shall apply an ON condition to circuit 107 and then transmit unscrambled
    binary 1 at 1200 bit/s.

b)  If the modem detects scrambled binary 1 or 0 in the low channel at 1200 bit/s for 270 +-40 ms,
    the handshake shall continue in accordance with section 6.3.1.2.2 b) and c). However, if unscrambled
    repetitive double dibit 00 and 11 at 1200 bit/s is detected in the low channel, at the end of
    receipt of this signal the modem shall apply an ON condition to circuit 112 and then transmit
    an unscrambled repetitive double dibit pattern of 00 and 11 at 1200 bit/s for 100 +-3 ms.
    Following these signals the modem shall transmit scrambled binary 1 at 1200 bit/s.

c)  600 +-10 ms after circuit 112 has been turned ON the modem shall begin transmitting scrambled
    binary 1 at 2400 bit/s, and 450 +-10 ms after circuit 112 has been turned ON the receiver may
    begin making 16-way decisions.

d)  Following transmission of scrambled binary 1 at 2400 bit/s for 200 +-10 ms, circuit 106 shall
    be conditioned to respond to circuit 105 and the modem shall be ready to transmit data at
    2400 bit/s.

e)  When 32 consecutive bits of scrambled binary 1 at 2400 bit/s have been detected in the low
    channel the modem shall be ready to receive data at 2400 bit/s and shall apply an ON
    condition to circuit 109.

6.3.1.2 Interworking at 1200 bit/s

The following handshake is identical to the Recommendation V.22 alternative A and B handshake.

6.3.1.2.1   Calling modem

a)  On connection to line the calling modem shall be conditioned to receive signals in the high
    channel at 1200 bit/s and transmit signals in the low channel at 1200 bit/s in accordance
    with section 2.5.2.2. It shall apply an ON condition to circuit 107 in accordance with
    Recommendation V.25. The modem shall initially remain silent.

b)  After 155 +-10 ms of unscrambled binary 1 has been detected, the modem shall remain silent
    for a further 456 +-10 ms then transmit scrambled binary 1 at 1200 bit/s (a preceding V.22 bis
    signal, as shown in Figure 7/V.22 bis, would not affect the operation of a V.22 answer modem).

c)  On detection of scrambled binary 1 in the high channel at 1200 bit/s for 270 +-40 ms the modem
    shall be ready to receive data at 1200 bit/s and shall apply an ON condition to circuit 109 and
    an OFF condition to circuit 112.

d)  765 +-10 ms after circuit 109 has been turned ON, circuit 106 shall be conditioned to respond
    to circuit 105 and the modem shall be ready to transmit data at 1200 bit/s.
 
6.3.1.2.2   Answering modem

a)  On connection to line the answering modem shall be conditioned to transmit signals in the high
    channel at 1200 bit/s in accordance with section 2.5.2.2 and receive signals in the low channel at
    1200 bit/s.

    Following transmission of the answer sequence in accordance with V.25 the modem shall apply
    an ON condition to circuit 107 and then transmit unscrambled binary 1 at 1200 bit/s.

b)  On detection of scrambled binary 1 or 0 in the low channel at 1200 bit/s for 270 +-40 ms the
    modem shall apply an OFF condition to circuit 112 and shall then transmit scrambled binary 1
    at 1200 bit/s.

c)  After scrambled binary 1 has been transmitted at 1200 bit/s for 765 +-10 ms the modem shall be
    ready to transmit and receive data at 1200 bit/s, shall condition circuit 106 to respond to
    circuit 105 and shall apply an ON condition to circuit 109.

Note - Manufacturers may wish to note that in certain countries, for national purposes, modems are
       in service which emit an answering tone of 2225 Hz instead of unscrambled binary 1.


V.22bis to V.22bis
------------------
Calling party
                                                           S1       scrambled 1's                  scrambled 1's  data
                                                                    at 1200bps                     at 2400bps
|---------------------------------------------------------|XXXXXXXX|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXXX|XXXXXXXXXXXXX
                                      |<155+-10>|<456+-10>|<100+-3>|        |<------600+-10------>|<---200+-10-->|
                                      ^                            |        ^<----450+-100---->|[16 way decisions begin]
                                      |                            |        |
                                      |                            v        |
                                      |                            |<------450+-100----->|[16 way decisions begin]
                                      |                            |<----------600+-10-------->|
  |<2150+-350>|<--3300+-700->|<75+-20>|                            |<100+-3>|                  |<---200+-10-->
  |-----------|XXXXXXXXXXXXXX|--------|XXXXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXX|XXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXXX|XXXXXXXXXXXXX
   silence    2100Hz                   unscrambled 1's              S1       scrambled 1's      scrambled 1's  data
                                       at 1200bps                            at 1200bps         at 2400bps
Answering party

S1 = Unscrambled double dibit 00 and 11 at 1200bps
When the 2400bps section starts, both sides should look for 32 bits of continuous ones, as a test of integrity.




V.22 to V.22bis
---------------
Calling party
                                                           scrambled 1's                                 data
                                                           at 1200bps
|---------------------------------------------------------|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXX
                                      |<155+-10>|<456+-10>|         |<270+-40>|<--------765+-10-------->|
                                      ^                   |         ^
                                      |                   |         |
                                      |                   |         |
                                      |                   |         |
                                      |                   v         |
  |<2150+-350>|<--3300+-700->|<75+-20>|                   |<270+-40>|<---------765+-10-------->|
  |-----------|XXXXXXXXXXXXXX|--------|XXXXXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXX
   silence    2100Hz                   unscrambled 1's                scrambled 1's             data
                                       at 1200bps                     at 1200bps
Answering party

Both ends should accept unscrambled binary 1 or binary 0 as the preamble.




V.22bis to V.22
---------------
Calling party
                                                           S1      scrambled 1's                                 data
                                                                   at 1200bps
|---------------------------------------------------------|XXXXXXXX|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXX
                                      |<155+-10>|<456+-10>|<100+-3>|           |<-270+-40-><------765+-10------>|
                                      ^                            |           ^
                                      |                            |           |
                                      |                            v           |
                                      |                            |
                                      |                            |
  |<2150+-350>|<--3300+-700->|<75+-20>|                            |<-270+-40->|<------765+-10----->|
  |-----------|XXXXXXXXXXXXXX|--------|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXX
   silence    2100Hz                   unscrambled 1's                          scrambled 1's        data
                                       at 1200bps                               at 1200bps
Answering party

Both ends should accept unscrambled binary 1 or binary 0 as the preamble.
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

static const int phase_steps[4] =
{
    1, 0, 2, 3
};

const complexf_t v22bis_constellation[16] =
{
    { 1.0f,  1.0f},
    { 3.0f,  1.0f},
    { 1.0f,  3.0f},
    { 3.0f,  3.0f},
    {-1.0f,  1.0f},
    {-1.0f,  3.0f},
    {-3.0f,  1.0f},
    {-3.0f,  3.0f},
    {-1.0f, -1.0f},
    {-3.0f, -1.0f},
    {-1.0f, -3.0f},
    {-3.0f, -3.0f},
    { 1.0f, -1.0f},
    { 1.0f, -3.0f},
    { 3.0f, -1.0f},
    { 3.0f, -3.0f}
};

/* Raised root cosine pulse shaping; Beta = 0.75; 4 symbols either
   side of the centre. */
/* Created with mkshape -r 0.0125 0.75 361 -l and then split up */
#define PULSESHAPER_GAIN            (40.000612087f/40.0f)
#define PULSESHAPER_COEFF_SETS      40

static const float pulseshaper[PULSESHAPER_COEFF_SETS][V22BIS_TX_FILTER_STEPS] =
{
    {
        -0.0047287346,          /* Filter 0 */
        -0.0083947197,
        -0.0087380763,
         0.0088053673,
         0.5108981827,
         0.5108981827,
         0.0088053673,
        -0.0087380763,
        -0.0083947197
    },
    {
        -0.0044638629,          /* Filter 1 */
        -0.0089241700,
        -0.0111288952,
         0.0023412184,
         0.5623914901,
         0.4599551720,
         0.0144817755,
        -0.0063186648,
        -0.0077293609
    },
    {
        -0.0041048584,          /* Filter 2 */
        -0.0093040596,
        -0.0134459768,
        -0.0048558766,
         0.6141017035,
         0.4098822897,
         0.0193317049,
        -0.0039145680,
        -0.0069438567
    },
    {
        -0.0036565006,          /* Filter 3 */
        -0.0095231635,
        -0.0156437084,
        -0.0127148737,
         0.6656848457,
         0.3609830295,
         0.0233320755,
        -0.0015677363,
        -0.0060557371
    },
    {
        -0.0031253709,          /* Filter 4 */
        -0.0095729633,
        -0.0176768181,
        -0.0211485021,
         0.7167894869,
         0.3135419896,
         0.0264748749,
         0.0006824956,
        -0.0050839319
    },
    {
        -0.0025197700,          /* Filter 5 */
        -0.0094478866,
        -0.0195012095,
        -0.0300535107,
         0.7670600056,
         0.2678225635,
         0.0287663895,
         0.0027999985,
        -0.0040483891
    },
    {
        -0.0018496023,          /* Filter 6 */
        -0.0091454978,
        -0.0210748106,
        -0.0393111426,
         0.8161399423,
         0.2240649005,
         0.0302262769,
         0.0047523617,
        -0.0029696854
    },
    {
        -0.0011262266,          /* Filter 7 */
        -0.0086666380,
        -0.0223584207,
        -0.0487878398,
         0.8636754069,
         0.1824841563,
         0.0308864956,
         0.0065113237,
        -0.0018686358
    },
    {
        -0.0003622774,          /* Filter 8 */
        -0.0080155088,
        -0.0233165437,
        -0.0583361774,
         0.9093185032,
         0.1432690480,
         0.0307901140,
         0.0080531155,
        -0.0007659096
    },
    {
         0.0004285425,          /* Filter 9 */
        -0.0071996967,
        -0.0239181901,
        -0.0677960213,
         0.9527307304,
         0.1065807242,
         0.0299900191,
         0.0093587151,
         0.0003183408
    },
    {
         0.0012316933,          /* Filter 10 */
        -0.0062301368,
        -0.0241376359,
        -0.0769959031,
         0.9935863233,
         0.0725519600,
         0.0285475474,
         0.0104140102,
         0.0013648323
    },
    {
         0.0020320508,          /* Filter 11 */
        -0.0051210137,
        -0.0239551212,
        -0.0857546018,
         1.0315754934,
         0.0412866769,
         0.0265310587,
         0.0112098711,
         0.0023554782
    },
    {
         0.0028141763,          /* Filter 12 */
        -0.0038896008,
        -0.0233574765,
        -0.0938829156,
         1.0664075323,
         0.0128597894,
         0.0240144782,
         0.0117421344,
         0.0032736852
    },
    {
         0.0035625973,          /* Filter 13 */
        -0.0025560369,
        -0.0223386620,
        -0.1011856112,
         1.0978137424,
        -0.0126826277,
         0.0210758250,
         0.0120115019,
         0.0041046165
    },
    {
         0.0042620971,          /* Filter 14 */
        -0.0011430452,
        -0.0209002083,
        -0.1074635265,
         1.1255501596,
        -0.0353228594,
         0.0177957527,
         0.0120233576,
         0.0048354157
    },
    {
         0.0048980053,          /* Filter 15 */
         0.0003244045,
        -0.0190515462,
        -0.1125158080,
         1.1494000377,
        -0.0550707703,
         0.0142561191,
         0.0117875105,
         0.0054553898
    },
    {
         0.0054564857,          /* Filter 16 */
         0.0018194852,
        -0.0168102168,
        -0.1161422557,
         1.1691760633,
        -0.0719627404,
         0.0105386068,
         0.0113178688,
         0.0059561483
    },
    {
         0.0059248154,          /* Filter 17 */
         0.0033139475,
        -0.0142019526,
        -0.1181457502,
         1.1847222770,
        -0.0860603350,
         0.0067234125,
         0.0106320540,
         0.0063316975
    },
    {
         0.0062916504,          /* Filter 18 */
         0.0047785946,
        -0.0112606234,
        -0.1183347329,
         1.1959156771,
        -0.0974487311,
         0.0028880206,
         0.0097509621,
         0.0065784888
    },
    {
         0.0065472715,          /* Filter 19 */
         0.0061837898,
        -0.0080280420,
        -0.1165257094,
         1.2026674866,
        -0.1062349247,
        -0.0008939235,
         0.0086982833,
         0.0066954225
    },
    {
         0.0066838062,          /* Filter 20 */
         0.0074999881,
        -0.0045536271,
        -0.1125457458,
         1.2049240699,
        -0.1125457458,
        -0.0045536271,
         0.0074999881,
         0.0066838062
    },
    {
         0.0066954225,          /* Filter 21 */
         0.0086982833,
        -0.0008939235,
        -0.1062349247,
         1.2026674866,
        -0.1165257094,
        -0.0080280420,
         0.0061837898,
         0.0065472715
    },
    {
         0.0065784888,          /* Filter 22 */
         0.0097509621,
         0.0028880206,
        -0.0974487311,
         1.1959156771,
        -0.1183347329,
        -0.0112606234,
         0.0047785946,
         0.0062916504
    },
    {
         0.0063316975,          /* Filter 23 */
         0.0106320540,
         0.0067234125,
        -0.0860603350,
         1.1847222770,
        -0.1181457502,
        -0.0142019526,
         0.0033139475,
         0.0059248154
    },
    {
         0.0059561483,          /* Filter 24 */
         0.0113178688,
         0.0105386068,
        -0.0719627404,
         1.1691760633,
        -0.1161422557,
        -0.0168102168,
         0.0018194852,
         0.0054564857
    },
    {
         0.0054553898,          /* Filter 25 */
         0.0117875105,
         0.0142561191,
        -0.0550707703,
         1.1494000377,
        -0.1125158080,
        -0.0190515462,
         0.0003244045,
         0.0048980053
    },
    {
         0.0048354157,          /* Filter 26 */
         0.0120233576,
         0.0177957527,
        -0.0353228594,
         1.1255501596,
        -0.1074635265,
        -0.0209002083,
        -0.0011430452,
         0.0042620971
    },
    {
         0.0041046165,          /* Filter 27 */
         0.0120115019,
         0.0210758250,
        -0.0126826277,
         1.0978137424,
        -0.1011856112,
        -0.0223386620,
        -0.0025560369,
         0.0035625973
    },
    {
         0.0032736852,          /* Filter 28 */
         0.0117421344,
         0.0240144782,
         0.0128597894,
         1.0664075323,
        -0.0938829156,
        -0.0233574765,
        -0.0038896008,
         0.0028141763
    },
    {
         0.0023554782,          /* Filter 29 */
         0.0112098711,
         0.0265310587,
         0.0412866769,
         1.0315754934,
        -0.0857546018,
        -0.0239551212,
        -0.0051210137,
         0.0020320508
    },
    {
         0.0013648323,          /* Filter 30 */
         0.0104140102,
         0.0285475474,
         0.0725519600,
         0.9935863233,
        -0.0769959031,
        -0.0241376359,
        -0.0062301368,
         0.0012316933
    },
    {
         0.0003183408,          /* Filter 31 */
         0.0093587151,
         0.0299900191,
         0.1065807242,
         0.9527307304,
        -0.0677960213,
        -0.0239181901,
        -0.0071996967,
         0.0004285425
    },
    {
        -0.0007659096,          /* Filter 32 */
         0.0080531155,
         0.0307901140,
         0.1432690480,
         0.9093185032,
        -0.0583361774,
        -0.0233165437,
        -0.0080155088,
        -0.0003622774
    },
    {
        -0.0018686358,          /* Filter 33 */
         0.0065113237,
         0.0308864956,
         0.1824841563,
         0.8636754069,
        -0.0487878398,
        -0.0223584207,
        -0.0086666380,
        -0.0011262266
    },
    {
        -0.0029696854,          /* Filter 34 */
         0.0047523617,
         0.0302262769,
         0.2240649005,
         0.8161399423,
        -0.0393111426,
        -0.0210748106,
        -0.0091454978,
        -0.0018496023
    },
    {
        -0.0040483891,          /* Filter 35 */
         0.0027999985,
         0.0287663895,
         0.2678225635,
         0.7670600056,
        -0.0300535107,
        -0.0195012095,
        -0.0094478866,
        -0.0025197700
    },
    {
        -0.0050839319,          /* Filter 36 */
         0.0006824956,
         0.0264748749,
         0.3135419896,
         0.7167894869,
        -0.0211485021,
        -0.0176768181,
        -0.0095729633,
        -0.0031253709
    },
    {
        -0.0060557371,          /* Filter 37 */
        -0.0015677363,
         0.0233320755,
         0.3609830295,
         0.6656848457,
        -0.0127148737,
        -0.0156437084,
        -0.0095231635,
        -0.0036565006
    },
    {
        -0.0069438567,          /* Filter 38 */
        -0.0039145680,
         0.0193317049,
         0.4098822897,
         0.6141017035,
        -0.0048558766,
        -0.0134459768,
        -0.0093040596,
        -0.0041048584
    },
    {
        -0.0077293609,          /* Filter 39 */
        -0.0063186648,
         0.0144817755,
         0.4599551720,
         0.5623914901,
         0.0023412184,
        -0.0111288952,
        -0.0089241700,
        -0.0044638629
    },
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

    if ((bit = s->current_get_bit(s->user_data)) == PUTBIT_END_OF_DATA)
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

static complexf_t training_get(v22bis_state_t *s)
{
    complexf_t z;
    int bits;

    /* V.22bis training sequence */
    switch (s->tx_training)
    {
    case V22BIS_TRAINING_STAGE_INITIAL_SILENCE:
        /* Segment 1: silence */
        s->tx_constellation_state = 0;
        z = complex_setf(0.0f, 0.0f);
        if (s->caller)
        {
            /* The caller just waits for a signal from the far end, which should be unscrambled ones */
            if (s->detected_unscrambled_ones_or_zeros)
            {
                if (s->bit_rate == 2400)
                {
                    /* Try to establish at 2400bps */
                    span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting unscrambled 0011 at 1200 (S1)\n");
                    s->tx_training = V22BIS_TRAINING_STAGE_UNSCRAMBLED_0011;
                }
                else
                {
                    /* Only try at 1200bps */
                    span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting scrambled ones at 1200 (A)\n");
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
                span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting unscrambled ones at 1200\n");
                s->tx_training = V22BIS_TRAINING_STAGE_UNSCRAMBLED_ONES;
                s->tx_training_count = 0;
            }
        }
        break;
    case V22BIS_TRAINING_STAGE_UNSCRAMBLED_ONES:
        /* Segment 2: Continuous unscrambled ones at 1200bps (i.e. reversals). */
        /* Only the answering modem sends unscrambled ones. It is the first thing exchanged between the modems. */
        s->tx_constellation_state = (s->tx_constellation_state + phase_steps[3]) & 3;
        z = v22bis_constellation[(s->tx_constellation_state << 2) | 0x01];
        if (s->bit_rate == 2400  &&  s->detected_unscrambled_0011_ending)
        {
            /* We are allowed to use 2400bps, and the far end is requesting 2400bps. Result: we are going to
               work at 2400bps */
            span_log(&s->logging, SPAN_LOG_FLOW, "+++ [2400] starting unscrambled 0011 at 1200 (S1)\n");
            s->tx_training = V22BIS_TRAINING_STAGE_UNSCRAMBLED_0011;
            s->tx_training_count = 0;
            break;
        }
        if (s->detected_scrambled_ones_or_zeros_at_1200bps)
        {
            /* We are going to work at 1200bps. */
            span_log(&s->logging, SPAN_LOG_FLOW, "+++ [1200] starting scrambled ones at 1200 (B)\n");
            s->bit_rate = 1200;
            s->tx_training = V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200;
            s->tx_training_count = 0;
            break;
        }
        break;
    case V22BIS_TRAINING_STAGE_UNSCRAMBLED_0011:
        /* Segment 3: Continuous unscrambled double dibit 00 11 at 1200bps. This is termed the S1 segment in
           the V.22bis spec. It is only sent to request or accept 2400bps mode, and lasts 100+-3ms. After this
           timed burst, we unconditionally change to sending scrambled ones at 1200bps. */
        s->tx_constellation_state = (s->tx_constellation_state + phase_steps[(s->tx_training_count & 1)  ?  3  :  0]) & 3;
span_log(&s->logging, SPAN_LOG_FLOW, "U0011 Tx 0x%02x\n", s->tx_constellation_state);
        z = v22bis_constellation[(s->tx_constellation_state << 2) | 0x01];
        if (++s->tx_training_count >= ms_to_symbols(100))
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting scrambled ones at 1200 (C)\n");
            s->tx_training = V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200;
            s->tx_training_count = 0;
        }
        break;
    case V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200:
        /* Segment 4: Scrambled ones at 1200bps. */
        bits = scramble(s, 1);
        bits = (bits << 1) | scramble(s, 1);
        s->tx_constellation_state = (s->tx_constellation_state + phase_steps[bits]) & 3;
        z = v22bis_constellation[(s->tx_constellation_state << 2) | 0x01];
        if (s->caller)
        {
            if (s->detected_unscrambled_0011_ending)
            {
                /* Continue for a further 600+-10ms */
                if (++s->tx_training_count >= ms_to_symbols(600))
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting scrambled ones at 2400 (A)\n");
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
                        span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting scrambled ones at 2400 (B)\n");
                        s->tx_training = V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_2400;
                        s->tx_training_count = 0;
                    }
                }
                else
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "+++ finished\n");
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
                    span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting scrambled ones at 2400 (C)\n");
                    s->tx_training = V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_2400;
                    s->tx_training_count = 0;
                }
            }
            else
            {
                if (++s->tx_training_count >= ms_to_symbols(756))
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "+++ finished\n");
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
        z = v22bis_constellation[(s->tx_constellation_state << 2) | 0x01];
        if (++s->tx_training_count >= ms_to_symbols(200))
        {
            /* We have completed training. Now handle some real work. */
            span_log(&s->logging, SPAN_LOG_FLOW, "+++ finished\n");
            s->tx_training = 0;
            s->tx_training_count = 0;
            s->current_get_bit = s->get_bit;
        }
        break;
    case V22BIS_TRAINING_STAGE_PARKED:
    default:
        z = complex_setf(0.0f, 0.0f);
        break;
    }
    return z;
}
/*- End of function --------------------------------------------------------*/

static complexf_t getbaud(v22bis_state_t *s)
{
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
            return complex_setf(0.0f, 0.0f);
    }
    /* The first two bits define the quadrant */
    bits = get_scrambled_bit(s);
    bits = (bits << 1) | get_scrambled_bit(s);
    s->tx_constellation_state = (s->tx_constellation_state + phase_steps[bits]) & 3;
    if (s->bit_rate == 1200)
    {
        bits = 0x01;
    }
    else
    {
        /* The other two bits define the position within the quadrant */
        bits = get_scrambled_bit(s);
        bits = (bits << 1) | get_scrambled_bit(s);
    }
    return v22bis_constellation[(s->tx_constellation_state << 2) | bits];
}
/*- End of function --------------------------------------------------------*/

int v22bis_tx(v22bis_state_t *s, int16_t amp[], int len)
{
    complexf_t x;
    complexf_t z;
    int i;
    int sample;
    float famp;

    if (s->shutdown > 10)
        return 0;
    for (sample = 0;  sample < len;  sample++)
    {
        if ((s->tx_baud_phase += 3) >= 40)
        {
            s->tx_baud_phase -= 40;
            s->tx_rrc_filter[s->tx_rrc_filter_step] =
            s->tx_rrc_filter[s->tx_rrc_filter_step + V22BIS_TX_FILTER_STEPS] = getbaud(s);
            if (++s->tx_rrc_filter_step >= V22BIS_TX_FILTER_STEPS)
                s->tx_rrc_filter_step = 0;
        }
        /* Root raised cosine pulse shaping at baseband */
        x.re = 0.0f;
        x.im = 0.0f;
        for (i = 0;  i < V22BIS_TX_FILTER_STEPS;  i++)
        {
            x.re += pulseshaper[39 - s->tx_baud_phase][i]*s->tx_rrc_filter[i + s->tx_rrc_filter_step].re;
            x.im += pulseshaper[39 - s->tx_baud_phase][i]*s->tx_rrc_filter[i + s->tx_rrc_filter_step].im;
        }
        /* Now create and modulate the carrier */
        z = dds_complexf(&(s->tx_carrier_phase), s->tx_carrier_phase_rate);
        famp = (x.re*z.re - x.im*z.im)*s->tx_gain;
        if (s->guard_phase_rate  &&  (s->tx_rrc_filter[s->tx_rrc_filter_step].re != 0.0f  ||  s->tx_rrc_filter[i + s->tx_rrc_filter_step].im != 0.0f))
        {
            /* Add the guard tone */
            famp += dds_modf(&(s->guard_phase), s->guard_phase_rate, s->guard_level, 0);
        }
        /* Don't bother saturating. We should never clip. */
        amp[sample] = (int16_t) lrintf(famp);
    }
    return sample;
}
/*- End of function --------------------------------------------------------*/

void v22bis_tx_power(v22bis_state_t *s, float power)
{
    float l;

    l = 1.6f*powf(10.0f, (power - DBM0_MAX_POWER)/20.0f);
    s->tx_gain = l*32768.0f/(PULSESHAPER_GAIN*3.0f);
}
/*- End of function --------------------------------------------------------*/

static int v22bis_tx_restart(v22bis_state_t *s, int bit_rate)
{
    s->bit_rate = bit_rate;
    cvec_zerof(s->tx_rrc_filter, sizeof(s->tx_rrc_filter)/sizeof(s->tx_rrc_filter[0]));
    s->tx_rrc_filter_step = 0;
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

void v22bis_set_get_bit(v22bis_state_t *s, get_bit_func_t get_bit, void *user_data)
{
    s->get_bit = get_bit;
    s->user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

void v22bis_set_put_bit(v22bis_state_t *s, put_bit_func_t put_bit, void *user_data)
{
    s->put_bit = put_bit;
    s->user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

int v22bis_restart(v22bis_state_t *s, int bit_rate)
{
    if (v22bis_tx_restart(s, bit_rate))
        return -1;
    return v22bis_rx_restart(s, bit_rate);
}
/*- End of function --------------------------------------------------------*/

v22bis_state_t *v22bis_init(v22bis_state_t *s,
                            int bit_rate,
                            int guard,
                            int caller,
                            get_bit_func_t get_bit,
                            put_bit_func_t put_bit,
                            void *user_data)
{
    if (s == NULL)
    {
        if ((s = (v22bis_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->bit_rate = bit_rate;
    s->caller = caller;

    s->get_bit = get_bit;
    s->put_bit = put_bit;
    s->user_data = user_data;

    if (s->caller)
    {
        s->tx_carrier_phase_rate = dds_phase_ratef(1200.0f);
    }
    else
    {
        s->tx_carrier_phase_rate = dds_phase_ratef(2400.0f);
        if (guard)
        {
            if (guard == 1)
            {
                s->guard_phase_rate = dds_phase_ratef(550.0f);
                s->guard_level = 1500.0f;
            }
            else
            {
                s->guard_phase_rate = dds_phase_ratef(1800.0f);
                s->guard_level = 1000.0f;
            }
        }
    }
    v22bis_tx_power(s, -10.0f);
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "V.22bis");
    v22bis_restart(s, s->bit_rate);
    return s;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

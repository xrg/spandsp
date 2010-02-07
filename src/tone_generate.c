/*
 * SpanDSP - a series of DSP components for telephony
 *
 * tone_generate.c - General telephony tone generation, and specific
 *                   generation of DTMF, and network supervisory tones.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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
 * $Id: tone_generate.c,v 1.8 2004/03/16 13:44:48 steveu Exp $
 */

/*! \file */

#define	_ISOC9X_SOURCE	1
#define _ISOC99_SOURCE	1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>

#include "spandsp/telephony.h"
#include "spandsp/dc_restore.h"
#include "spandsp/tone_generate.h"

#if !defined(M_PI)
/* C99 systems may not define M_PI */
#define M_PI 3.14159265358979323846264338327
#endif

#define DTMF_DURATION               380
#define DTMF_PAUSE                  400
#define DTMF_CYCLE                  (DTMF_DURATION + DTMF_PAUSE)

typedef struct
{
    float       f1;         /* First freq */
    float       f2;         /* Second freq */
    int8_t      level1;     /* Level of the first freq (dB) */
    int8_t      level2;     /* Level of the second freq (dB) */
    uint8_t     on_time;    /* Tone on time (ms) */
    uint8_t     off_time;   /* Minimum post tone silence (ms) */
} mf_digit_tones_t;

int dtmf_gen_inited = FALSE;
tone_gen_descriptor_t dtmf_digit_tones[16];

int bell_mf_gen_inited = FALSE;
tone_gen_descriptor_t bell_mf_digit_tones[15];

int r2_mf_gen_inited = FALSE;
tone_gen_descriptor_t r2_mf_fwd_digit_tones[15];
tone_gen_descriptor_t r2_mf_back_digit_tones[15];

#if 0
tone_gen_descriptor_t socotel_mf_digit_tones[18];
#endif

static float dtmf_row[] =
{
     697.0,  770.0,  852.0,  941.0
};
static float dtmf_col[] =
{
    1209.0, 1336.0, 1477.0, 1633.0
};

static char dtmf_tone_codes[] = "123A" "456B" "789C" "*0#D";

/* Bell R1 tone generation specs.
 *  Power: -7dBm +- 1dB
 *  Frequency: within +-1.5%
 *  Mismatch between the start time of a pair of tones: <=6ms.
 *  Mismatch between the end time of a pair of tones: <=6ms.
 *  Tone duration: 68+-7ms, except KP which is 100+-7ms.
 *  Inter-tone gap: 68+-7ms.
 */
static mf_digit_tones_t bell_mf_tones[] =
{
    { 700.0,  900.0, -7, -7,  68, 68},
    { 700.0, 1100.0, -7, -7,  68, 68},
    { 900.0, 1100.0, -7, -7,  68, 68},
    { 700.0, 1300.0, -7, -7,  68, 68},
    { 900.0, 1300.0, -7, -7,  68, 68},
    {1100.0, 1300.0, -7, -7,  68, 68},
    { 700.0, 1500.0, -7, -7,  68, 68},
    { 900.0, 1500.0, -7, -7,  68, 68},
    {1100.0, 1500.0, -7, -7,  68, 68},
    {1300.0, 1500.0, -7, -7,  68, 68},
    { 700.0, 1700.0, -7, -7,  68, 68}, /* ST''' - use 'C' */
    { 900.0, 1700.0, -7, -7,  68, 68}, /* ST'   - use 'A' */
    {1100.0, 1700.0, -7, -7, 100, 68}, /* KP    - use '*' */
    {1300.0, 1700.0, -7, -7,  68, 68}, /* ST''  - use 'B' */
    {1500.0, 1700.0, -7, -7,  68, 68}, /* ST    - use '#' */
    {0.0, 0.0, 0, 0}
};

/* The order of the digits here must match the list above */
static char bell_mf_tone_codes[] = "1234567890CA*B#";

/* R2 tone generation specs.
 *  Power: -11.5dBm +- 1dB
 *  Frequency: within +-4Hz
 *  Mismatch between the start time of a pair of tones: <=1ms.
 *  Mismatch between the end time of a pair of tones: <=1ms.
 */
static mf_digit_tones_t r2_mf_fwd_tones[] =
{
    {1380.0, 1500.0, -11, -11, 1, 0},
    {1380.0, 1620.0, -11, -11, 1, 0},
    {1500.0, 1620.0, -11, -11, 1, 0},
    {1380.0, 1740.0, -11, -11, 1, 0},
    {1500.0, 1740.0, -11, -11, 1, 0},
    {1620.0, 1740.0, -11, -11, 1, 0},
    {1380.0, 1860.0, -11, -11, 1, 0},
    {1500.0, 1860.0, -11, -11, 1, 0},
    {1620.0, 1860.0, -11, -11, 1, 0},
    {1740.0, 1860.0, -11, -11, 1, 0},
    {1380.0, 1980.0, -11, -11, 1, 0},
    {1500.0, 1980.0, -11, -11, 1, 0},
    {1620.0, 1980.0, -11, -11, 1, 0},
    {1740.0, 1980.0, -11, -11, 1, 0},
    {1860.0, 1980.0, -11, -11, 1, 0},
    {0.0, 0.0, 0, 0}
};

static mf_digit_tones_t r2_mf_back_tones[] =
{
    {1140.0, 1020.0, -11, -11, 1, 0},
    {1140.0,  900.0, -11, -11, 1, 0},
    {1020.0,  900.0, -11, -11, 1, 0},
    {1140.0,  780.0, -11, -11, 1, 0},
    {1020.0,  780.0, -11, -11, 1, 0},
    { 900.0,  780.0, -11, -11, 1, 0},
    {1140.0,  660.0, -11, -11, 1, 0},
    {1020.0,  660.0, -11, -11, 1, 0},
    { 900.0,  660.0, -11, -11, 1, 0},
    { 780.0,  660.0, -11, -11, 1, 0},
    {1140.0,  540.0, -11, -11, 1, 0},
    {1020.0,  540.0, -11, -11, 1, 0},
    { 900.0,  540.0, -11, -11, 1, 0},
    { 780.0,  540.0, -11, -11, 1, 0},
    { 660.0,  540.0, -11, -11, 1, 0},
    {0.0, 0.0, 0, 0}
};

/* The order of the digits here must match the lists above */
static char r2_mf_tone_codes[] = "1234567890ABCDE";

#if 0
static mf_digit_tones_t socotel_tones[] =
{
    {700.0,   900.0, -11, -11, 1, 0},
    {700.0,  1100.0, -11, -11, 1, 0},
    {900.0,  1100.0, -11, -11, 1, 0},
    {700.0,  1300.0, -11, -11, 1, 0},
    {900.0,  1300.0, -11, -11, 1, 0},
    {1100.0, 1300.0, -11, -11, 1, 0},
    {700.0,  1500.0, -11, -11, 1, 0},
    {900.0,  1500.0, -11, -11, 1, 0},
    {1100.0, 1500.0, -11, -11, 1, 0},
    {1300.0, 1500.0, -11, -11, 1, 0},
    {1500.0, 1700.0, -11, -11, 1, 0},
    {700.0,  1700.0, -11, -11, 1, 0},
    {900.0,  1700.0, -11, -11, 1, 0},
    {1300.0, 1700.0, -11, -11, 1, 0},
    {1100.0, 1700.0, -11, -11, 1, 0},
    {1700.0,    0.0, -11, -11, 1, 0},   /* Use 'F' */
    {1900.0,    0.0, -11, -11, 1, 0},   /* Use 'G' */
    {0.0, 0.0, 0, 0}
};

/* The order of the digits here must match the list above */
static char socotel_mf_tone_codes[] = "1234567890ABCDEFG";
#endif

void make_tone_gen_descriptor(tone_gen_descriptor_t *s,
                              int f1,
                              int l1,
                              int f2,
                              int l2,
                              int d1,
                              int d2,
                              int d3,
                              int d4,
                              int repeat)
{
    float gain;

    if (f1)
    {    
    	gain = pow(10.0, (l1 - 3.14)/20.0)*32768.0;
#if defined(PURE_INTEGER_DSP)
        s->fac_1 = 32768.0*2.0*cos(2.0*M_PI*f1/(float) SAMPLE_RATE);
#else        
    	s->fac_1 = 2.0*cos(2.0*M_PI*f1/(float) SAMPLE_RATE);
#endif
    	s->v2_1 = sin(-4.0*M_PI*f1/(float) SAMPLE_RATE)*gain;
    	s->v3_1 = sin(-2.0*M_PI*f1/(float) SAMPLE_RATE)*gain;
    }
    else
    {
    	s->fac_1 = 0.0;
    	s->v2_1 = 0.0;
    	s->v3_1 = 0.0;
    }
    if (f2)
    {
    	gain = pow(10.0, (l2 - 3.14)/20.0)*32768.0;
#if defined(PURE_INTEGER_DSP)
        s->fac_2 = 32768.0*2.0*cos(2.0*M_PI*f2/(float) SAMPLE_RATE);
#else        
    	s->fac_2 = 2.0*cos(2.0*M_PI*f2/(float) SAMPLE_RATE);
#endif
    	s->v2_2 = sin(-4.0*M_PI*f2/(float) SAMPLE_RATE)*gain;
    	s->v3_2 = sin(-2.0*M_PI*f2/(float) SAMPLE_RATE)*gain;
    }
    else
    {
    	s->fac_2 = 0.0;
    	s->v2_2 = 0.0;
    	s->v3_2 = 0.0;
    }
    s->duration[0] = d1*8;
    s->duration[1] = d2*8;
    s->duration[2] = d3*8;
    s->duration[3] = d4*8;

    s->repeat = repeat;
}
/*- End of function --------------------------------------------------------*/

void make_tone_descriptor(tone_gen_descriptor_t *desc, cadenced_tone_t *tone)
{
    make_tone_gen_descriptor(desc,
                             tone->f1,
                             tone->level1,
                             tone->f2,
                             tone->level2,
                             tone->on_time1,
                             tone->off_time1,
                             tone->on_time2,
                             tone->off_time2,
                             tone->repeat);
}
/*- End of function --------------------------------------------------------*/

void tone_gen_init(tone_gen_state_t *s, tone_gen_descriptor_t *t)
{
    int i;

    s->fac_1 = t->fac_1;
    s->v2_1 = t->v2_1;
    s->v3_1 = t->v3_1;

    s->fac_2 = t->fac_2;
    s->v2_2 = t->v2_2;
    s->v3_2 = t->v3_2;

    for (i = 0;  i < 4;  i++)
        s->duration[i] = t->duration[i];
    s->repeat = t->repeat;

    s->current_section = 0;
    s->current_position = 0;
}
/*- End of function --------------------------------------------------------*/

int tone_gen(tone_gen_state_t *s, int16_t *amp, int max_samples)
{
#if defined(PURE_INTEGER_DSP)
    int32_t xamp;
    int32_t v1_1;
    int32_t v2_1;
    int32_t v3_1;
    int32_t fac_1;
    int32_t v1_2;
    int32_t v2_2;
    int32_t v3_2;
    int32_t fac_2;
#else
    float xamp;
    float v1_1;
    float v2_1;
    float v3_1;
    float fac_1;
    float v1_2;
    float v2_2;
    float v3_2;
    float fac_2;
#endif
    int samples;
    int limit;

    if (s->current_section < 0)
        return  0;

    /* This is a second order IIR filter, configured to oscillate */
    /* The equation is x(n) = 2*cos(2.0*PI*f))*x(n-1) - x(n-2) */
    /* This isn't particularly accurate near the bottom of the band.
       If you recast the equation as
         x(n) = 2*x(n-1) - 2*(1 - cos(2.0*PI*f))*x(n-1) - x(n-2)
       you get a better balance of errors as you move the frequency to be
       generated across the band. It takes an extra operation, though. */

    v2_1 = s->v2_1;
    v3_1 = s->v3_1;
    fac_1 = s->fac_1;
    v2_2 = s->v2_2;
    v3_2 = s->v3_2;
    fac_2 = s->fac_2;

    for (samples = 0;  samples < max_samples;  )
    {
        limit = samples + s->duration[s->current_section] - s->current_position;
        if (limit > max_samples)
            limit = max_samples;
        
        s->current_position += (limit - samples);
        if (s->current_section & 1)
        {
            /* A silent section */
            for (  ;  samples < limit;  samples++)
                amp[samples] = 0;
        }
        else
        {
            for (  ;  samples < limit;  samples++)
            {
                xamp = 0;
                if (fac_1)
                {
                    v1_1 = v2_1;
                    v2_1 = v3_1;
#if defined(PURE_INTEGER_DSP)
                    v3_1 = (fac_1*v2_1 >> 15) - v1_1;
#else
                    v3_1 = fac_1*v2_1 - v1_1;
#endif
                    xamp += v3_1;
                }
                if (fac_2)
                {
                    v1_2 = v2_2;
                    v2_2 = v3_2;
#if defined(PURE_INTEGER_DSP)
                    v3_2 = (fac_2*v2_2 >> 15) - v1_2;
#else
                    v3_2 = fac_2*v2_2 - v1_2;
#endif
                    xamp += v3_2;
                }
                /* Saturation of the answer is the right thing at this point.
                   However, we are normally generating well controlled tones,
                   that cannot clip. So, the overhead of doing saturation is
                   a waste of valuable time. */
#if 0
#if defined(PURE_INTEGER_DSP)
                amp[samples] = saturate(xamp);
#else
                amp[samples] = fsaturate(xamp);
#endif
#else
                amp[samples] = xamp;
#endif
            }
        }
        if (s->current_position >= s->duration[s->current_section])
        {
            s->current_position = 0;
            if (++s->current_section > 3  ||  s->duration[s->current_section] == 0)
            {
                if (s->repeat)
                {
                    s->current_section = 0;
                }
                else
                {
                    /* Force a quick exit */
                    s->current_section = -1;
                    break;
                }
            }
        }
    }
    s->v2_1 = v2_1;
    s->v3_1 = v3_1;
    s->v2_2 = v2_2;
    s->v3_2 = v3_2;
    return samples;
}
/*- End of function --------------------------------------------------------*/

void dtmf_gen_init(void)
{
    int row;
    int col;

    if (dtmf_gen_inited)
        return;
    for (row = 0;  row < 4;  row++)
    {
    	for (col = 0;  col < 4;  col++)
        {
    	    make_tone_gen_descriptor(&dtmf_digit_tones[row*4 + col],
                                     dtmf_row[row],
                                     -10,
                                     dtmf_col[col],
                                     -10,
                                     50,
                                     55,
                                     0,
                                     0,
                                     FALSE);
        }
    }
    dtmf_gen_inited = TRUE;
}
/*- End of function --------------------------------------------------------*/

void dtmf_tx_init(dtmf_tx_state_t *s)
{
    if (!dtmf_gen_inited)
        dtmf_gen_init();
    s->tone_codes = dtmf_tone_codes;
    s->tone_descriptors = dtmf_digit_tones;
    tone_gen_init(&(s->tones), &dtmf_digit_tones[0]);
    s->current_sample = 0;
    s->current_digits = 0;
    s->tones.current_section = -1;
}
/*- End of function --------------------------------------------------------*/

void bell_mf_gen_init(void)
{
    int i;
    mf_digit_tones_t *tones;

    if (bell_mf_gen_inited)
        return;
    i = 0;
    tones = bell_mf_tones;
    while (tones->on_time)
    {
        /* Note: The duration of KP is longer than the other signals. */
        make_tone_gen_descriptor(&bell_mf_digit_tones[i++],
                                 tones->f1,
                                 tones->level1,
                                 tones->f2,
                                 tones->level2,
                                 tones->on_time,
                                 tones->off_time,
                                 0,
                                 0,
                                 FALSE);
        tones++;
    }
    bell_mf_gen_inited = TRUE;
}
/*- End of function --------------------------------------------------------*/

void bell_mf_tx_init(dtmf_tx_state_t *s)
{
    if (!bell_mf_gen_inited)
        bell_mf_gen_init();
    s->tone_codes = bell_mf_tone_codes;
    s->tone_descriptors = bell_mf_digit_tones;
    tone_gen_init(&(s->tones), &bell_mf_digit_tones[0]);
    s->current_sample = 0;
    s->current_digits = 0;
    s->tones.current_section = -1;
}
/*- End of function --------------------------------------------------------*/

int dtmf_tx(dtmf_tx_state_t *s, int16_t *amp, int max_samples)
{
    int len;
    int dig;
    char *cp;

    len = 0;
    if (s->tones.current_section >= 0)
    {
        /* Deal with the fragment left over from last time */
        len = tone_gen(&(s->tones), amp, max_samples);
    }
    dig = 0;
    while (dig < s->current_digits  &&  len < max_samples)
    {
        /* Step to the next digit */
        if ((cp = strchr(s->tone_codes, s->digits[dig++])) == NULL)
            continue;
        tone_gen_init(&(s->tones), &(s->tone_descriptors[cp - s->tone_codes]));
        len += tone_gen(&(s->tones), amp + len, max_samples - len);
    }
    if (dig)
    {
        /* Shift out the consumed digits */
        s->current_digits -= dig;
        memmove(s->digits, s->digits + dig, s->current_digits);
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

int dtmf_put(dtmf_tx_state_t *s, const char *digits)
{
    int len;

    /* This returns the number of characters that would not fit in the buffer.
       The buffer will only be loaded if the whole string of digits will fit,
       in which case zero is returned. */
    if ((len = strlen(digits)) > 0)
    {
        if (s->current_digits + len <= MAX_DTMF_DIGITS)
        {
            memcpy(s->digits + s->current_digits, digits, len);
            s->current_digits += len;
            len = 0;
        }
        else
        {
            len = MAX_DTMF_DIGITS - s->current_digits;
        }
    }
    return  len;
}
/*- End of function --------------------------------------------------------*/

void r2_mf_tx_init(void)
{
    int i;
    mf_digit_tones_t *tones;

    if (!r2_mf_gen_inited)
    {
        i = 0;
        tones = r2_mf_fwd_tones;
        while (tones->on_time)
        {
            make_tone_gen_descriptor(&r2_mf_fwd_digit_tones[i++],
                                     tones->f1,
                                     tones->level1,
                                     tones->f2,
                                     tones->level2,
                                     tones->on_time,
                                     tones->off_time,
                                     0,
                                     0,
                                     (tones->off_time == 0));
            tones++;
        }
        i = 0;
        tones = r2_mf_back_tones;
        while (tones->on_time)
        {
            make_tone_gen_descriptor(&r2_mf_back_digit_tones[i++],
                                     tones->f1,
                                     tones->level1,
                                     tones->f2,
                                     tones->level2,
                                     tones->on_time,
                                     tones->off_time,
                                     0,
                                     0,
                                     (tones->off_time == 0));
            tones++;
        }
        r2_mf_gen_inited = TRUE;
    }
}
/*- End of function --------------------------------------------------------*/

int r2_mf_tx(tone_gen_state_t *s, int16_t *amp, int samples, int fwd, char digit)
{
    int len;
    char *cp;

    len = 0;
    if (digit == (char) 0x7F)
    {
        len = tone_gen(s, amp, samples);
    }
    else
    {
        if ((cp = strchr(r2_mf_tone_codes, digit)))
        {
            if (fwd)
                tone_gen_init(s, &r2_mf_fwd_digit_tones[cp - r2_mf_tone_codes]);
            else
                tone_gen_init(s, &r2_mf_back_digit_tones[cp - r2_mf_tone_codes]);
            len = tone_gen(s, amp, samples);
        }
    }
    return len;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

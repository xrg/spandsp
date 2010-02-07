/*
 * SpanDSP - a series of DSP components for telephony
 *
 * tone_detect.c - General telephony tone detection, and specific
 *                 detection of DTMF, Bell MF, and MFC/R2.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001-2003, 2005 Steve Underwood
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
 * $Id: tone_detect.c,v 1.18 2006/02/06 15:00:56 steveu Exp $
 */
 
/*! \file tone_detect.h */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>

#include "spandsp/telephony.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"

#if !defined(M_PI)
/* C99 systems may not define M_PI */
#define M_PI 3.14159265358979323846264338327
#endif

//#define USE_3DNOW

#define DTMF_THRESHOLD              8.0e7
#define DTMF_NORMAL_TWIST           6.3     /* 8dB */
#define DTMF_REVERSE_TWIST          2.5     /* 4dB */
#define DTMF_RELATIVE_PEAK_ROW      6.3     /* 8dB */
#define DTMF_RELATIVE_PEAK_COL      6.3     /* 8dB */
#define DTMF_TO_TOTAL_ENERGY        42.0

#define BELL_MF_THRESHOLD           1.6e9
#define BELL_MF_TWIST               4.0     /* 6dB */
#define BELL_MF_RELATIVE_PEAK       12.6    /* 11dB */

#define R2_MF_THRESHOLD             5.0e8
#define R2_MF_TWIST                 5.0     /* 7dB */
#define R2_MF_RELATIVE_PEAK         12.6    /* 11dB */

static goertzel_descriptor_t dtmf_detect_row[4];
static goertzel_descriptor_t dtmf_detect_col[4];

static goertzel_descriptor_t bell_mf_detect_desc[6];

static goertzel_descriptor_t mf_fwd_detect_desc[6];
static goertzel_descriptor_t mf_back_detect_desc[6];

static const float dtmf_row[] =
{
     697.0,  770.0,  852.0,  941.0
};
static const float dtmf_col[] =
{
    1209.0, 1336.0, 1477.0, 1633.0
};

static const char dtmf_positions[] = "123A" "456B" "789C" "*0#D";

static const float bell_mf_tones[] =
{
     700.0,  900.0, 1100.0, 1300.0, 1500.0, 1700.0
};

/* Use the follow characters for the Bell MF special signals:
    KP    - use '*'
    ST    - use '#'
    ST'   - use 'A'
    ST''  - use 'B'
    ST''' - use 'C' */
static const char bell_mf_positions[] = "1247C-358A--69*---0B----#";

static const float r2_mf_fwd_tones[] =
{
    1380.0, 1500.0, 1620.0, 1740.0, 1860.0, 1980.0
};

static const float r2_mf_back_tones[] =
{
    1140.0, 1020.0,  900.0,  780.0,  660.0,  540.0
};

static const char mf_positions[] = "1247A-358B--69C---0D----E";

void make_goertzel_descriptor(goertzel_descriptor_t *t, int freq, int samples)
{
    t->fac = 2.0*cos(2.0*M_PI*((float) freq/(float) SAMPLE_RATE));
    t->samples = samples;
}
/*- End of function --------------------------------------------------------*/

goertzel_state_t *goertzel_init(goertzel_state_t *s,
                                goertzel_descriptor_t *t)
{
    if (s  ||  (s = malloc(sizeof(goertzel_state_t))))
    {
        s->v2 =
        s->v3 = 0.0;
        s->fac = t->fac;
        s->samples = t->samples;
        s->current_sample = 0;
    }
    return s;
}
/*- End of function --------------------------------------------------------*/

int goertzel_update(goertzel_state_t *s,
                    const int16_t amp[],
                    int samples)
{
    int i;
    float v1;

    if (samples > s->samples - s->current_sample)
        samples = s->samples - s->current_sample;
    for (i = 0;  i < samples;  i++)
    {
        v1 = s->v2;
        s->v2 = s->v3;
        s->v3 = s->fac*s->v2 - v1 + amp[i];
    }
    s->current_sample += samples;
    return samples;
}
/*- End of function --------------------------------------------------------*/

float goertzel_result(goertzel_state_t *s)
{
    float v1;

    /* Push a zero through the process to finish things off. */
    v1 = s->v2;
    s->v2 = s->v3;
    s->v3 = s->fac*s->v2 - v1;
    /* Now calculate the non-recursive side of the filter. */
    /* The result here is not scaled down to allow for the magnification
       effect of the filter (the usual DFT magnification effect). */
    return s->v3*s->v3 + s->v2*s->v2 - s->v2*s->v3*s->fac;
}
/*- End of function --------------------------------------------------------*/

#if defined(USE_3DNOW)
static inline void _dtmf_goertzel_update(goertzel_state_t *s,
                                         float x[],
                                         int samples)
{
    int n;
    float v;
    int i;
    float vv[16];

    vv[4] = s[0].v2;
    vv[5] = s[1].v2;
    vv[6] = s[2].v2;
    vv[7] = s[3].v2;
    vv[8] = s[0].v3;
    vv[9] = s[1].v3;
    vv[10] = s[2].v3;
    vv[11] = s[3].v3;
    vv[12] = s[0].fac;
    vv[13] = s[1].fac;
    vv[14] = s[2].fac;
    vv[15] = s[3].fac;

    //v1 = s->v2;
    //s->v2 = s->v3;
    //s->v3 = s->fac*s->v2 - v1 + x[0];

    __asm__ __volatile__ (
             " femms;\n"

             " movq        16(%%edx),%%mm2;\n"
             " movq        24(%%edx),%%mm3;\n"
             " movq        32(%%edx),%%mm4;\n"
             " movq        40(%%edx),%%mm5;\n"
             " movq        48(%%edx),%%mm6;\n"
             " movq        56(%%edx),%%mm7;\n"

             " jmp         1f;\n"
             " .align 32;\n"

             " 1: ;\n"
             " prefetch    (%%eax);\n"
             " movq        %%mm3,%%mm1;\n"
             " movq        %%mm2,%%mm0;\n"
             " movq        %%mm5,%%mm3;\n"
             " movq        %%mm4,%%mm2;\n"

             " pfmul       %%mm7,%%mm5;\n"
             " pfmul       %%mm6,%%mm4;\n"
             " pfsub       %%mm1,%%mm5;\n"
             " pfsub       %%mm0,%%mm4;\n"

             " movq        (%%eax),%%mm0;\n"
             " movq        %%mm0,%%mm1;\n"
             " punpckldq   %%mm0,%%mm1;\n"
             " add         $4,%%eax;\n"
             " pfadd       %%mm1,%%mm5;\n"
             " pfadd       %%mm1,%%mm4;\n"

             " dec         %%ecx;\n"

             " jnz         1b;\n"

             " movq        %%mm2,16(%%edx);\n"
             " movq        %%mm3,24(%%edx);\n"
             " movq        %%mm4,32(%%edx);\n"
             " movq        %%mm5,40(%%edx);\n"

             " femms;\n"
             :
             : "c" (samples), "a" (x), "d" (vv)
             : "memory", "eax", "ecx");

    s[0].v2 = vv[4];
    s[1].v2 = vv[5];
    s[2].v2 = vv[6];
    s[3].v2 = vv[7];
    s[0].v3 = vv[8];
    s[1].v3 = vv[9];
    s[2].v3 = vv[10];
    s[3].v3 = vv[11];
}
#endif
/*- End of function --------------------------------------------------------*/

dtmf_rx_state_t *dtmf_rx_init(dtmf_rx_state_t *s,
                              void (*callback)(void *user_data, const char *digits, int len),
                              void *user_data)
{
    int i;
    static int initialised = FALSE;

    s->callback = callback;
    s->callback_data = user_data;
    s->realtime_callback = NULL;
    s->realtime_callback_data = NULL;
    s->filter_dialtone = FALSE;
    s->normal_twist = DTMF_NORMAL_TWIST;
    s->reverse_twist = DTMF_REVERSE_TWIST;

    s->hits[0] = 
    s->hits[1] =
    s->hits[2] = 0;
    s->in_digit = FALSE;

    if (!initialised)
    {
        for (i = 0;  i < 4;  i++)
        {
            make_goertzel_descriptor(&dtmf_detect_row[i], dtmf_row[i], 102);
            make_goertzel_descriptor(&dtmf_detect_col[i], dtmf_col[i], 102);
        }
        initialised = TRUE;
    }
    for (i = 0;  i < 4;  i++)
    {
        goertzel_init(&s->row_out[i], &dtmf_detect_row[i]);
        goertzel_init(&s->col_out[i], &dtmf_detect_col[i]);
    }
    s->energy = 0.0;
    s->current_sample = 0;
    s->lost_digits = 0;
    s->current_digits = 0;
    s->digits[0] = '\0';
    return s;
}
/*- End of function --------------------------------------------------------*/

void dtmf_rx_set_realtime_callback(dtmf_rx_state_t *s,
                                   void (*callback)(void *user_data, int signal),
                                   void *user_data)
{
    s->realtime_callback = callback;
    s->realtime_callback_data = user_data;
}
/*- End of function --------------------------------------------------------*/

void dtmf_rx_parms(dtmf_rx_state_t *s,
                   int filter_dialtone,
                   int twist,
                   int reverse_twist)
{
    float x;

    if (filter_dialtone >= 0)
    {
        s->z350_1 = 0.0;
        s->z350_2 = 0.0;
        s->z440_1 = 0.0;
        s->z440_2 = 0.0;
        s->filter_dialtone = filter_dialtone;
    }
    if (twist >= 0)
        s->normal_twist = powf(10.0, twist/10.0);
    if (reverse_twist >= 0)
        s->reverse_twist = powf(10.0, reverse_twist/10.0);
}
/*- End of function --------------------------------------------------------*/

int dtmf_rx(dtmf_rx_state_t *s, const int16_t *amp, int samples)
{
    float row_energy[4];
    float col_energy[4];
    float famp;
    float v1;
    int i;
    int j;
    int sample;
    int best_row;
    int best_col;
    int hit;
    int limit;

    hit = 0;
    for (sample = 0;  sample < samples;  sample = limit)
    {
        /* The block length is optimised to meet the DTMF specs. */
        if ((samples - sample) >= (102 - s->current_sample))
            limit = sample + (102 - s->current_sample);
        else
            limit = samples;
#if defined(USE_3DNOW)
        _dtmf_goertzel_update(s->row_out, amp + sample, limit - sample);
        _dtmf_goertzel_update(s->col_out, amp + sample, limit - sample);
#else
        /* The following unrolled loop takes only 35% (rough estimate) of the 
           time of a rolled loop on the machine on which it was developed */
        for (j = sample;  j < limit;  j++)
        {
            famp = amp[j];
            if (s->filter_dialtone)
            {
                /* Sharp notches applied at 350Hz and 440Hz - the two common dialtone frequencies.
                   These are rather high Q, to achieve the required narrowness, without using lots of
                   sections. */
                v1 = 0.98356*famp + 1.8954426*s->z350_1 - 0.9691396*s->z350_2;
                famp = v1 - 1.9251480*s->z350_1 + s->z350_2;
                s->z350_2 = s->z350_1;
                s->z350_1 = v1;

                v1 = 0.98456*famp + 1.8529543*s->z440_1 - 0.9691396*s->z440_2;
                famp = v1 - 1.8819938*s->z440_1 + s->z440_2;
                s->z440_2 = s->z440_1;
                s->z440_1 = v1;
            }
            s->energy += famp*famp;
            /* With GCC 2.95, the following unrolled code seems to take about 35%
               (rough estimate) as long as a neat little 0-3 loop */
            v1 = s->row_out[0].v2;
            s->row_out[0].v2 = s->row_out[0].v3;
            s->row_out[0].v3 = s->row_out[0].fac*s->row_out[0].v2 - v1 + famp;
    
            v1 = s->col_out[0].v2;
            s->col_out[0].v2 = s->col_out[0].v3;
            s->col_out[0].v3 = s->col_out[0].fac*s->col_out[0].v2 - v1 + famp;
    
            v1 = s->row_out[1].v2;
            s->row_out[1].v2 = s->row_out[1].v3;
            s->row_out[1].v3 = s->row_out[1].fac*s->row_out[1].v2 - v1 + famp;
    
            v1 = s->col_out[1].v2;
            s->col_out[1].v2 = s->col_out[1].v3;
            s->col_out[1].v3 = s->col_out[1].fac*s->col_out[1].v2 - v1 + famp;
    
            v1 = s->row_out[2].v2;
            s->row_out[2].v2 = s->row_out[2].v3;
            s->row_out[2].v3 = s->row_out[2].fac*s->row_out[2].v2 - v1 + famp;
    
            v1 = s->col_out[2].v2;
            s->col_out[2].v2 = s->col_out[2].v3;
            s->col_out[2].v3 = s->col_out[2].fac*s->col_out[2].v2 - v1 + famp;
    
            v1 = s->row_out[3].v2;
            s->row_out[3].v2 = s->row_out[3].v3;
            s->row_out[3].v3 = s->row_out[3].fac*s->row_out[3].v2 - v1 + famp;

            v1 = s->col_out[3].v2;
            s->col_out[3].v2 = s->col_out[3].v3;
            s->col_out[3].v3 = s->col_out[3].fac*s->col_out[3].v2 - v1 + famp;
        }
#endif
        s->current_sample += (limit - sample);
        if (s->current_sample < 102)
            continue;

        /* We are at the end of a DTMF detection block */
        /* Find the peak row and the peak column */
        row_energy[0] = goertzel_result(&s->row_out[0]);
        col_energy[0] = goertzel_result(&s->col_out[0]);

        for (best_row = best_col = 0, i = 1;  i < 4;  i++)
        {
            row_energy[i] = goertzel_result(&s->row_out[i]);
            if (row_energy[i] > row_energy[best_row])
                best_row = i;
            col_energy[i] = goertzel_result(&s->col_out[i]);
            if (col_energy[i] > col_energy[best_col])
                best_col = i;
        }
        hit = 0;
        /* Basic signal level test and the twist test */
        if (row_energy[best_row] >= DTMF_THRESHOLD
            &&
            col_energy[best_col] >= DTMF_THRESHOLD
            &&
            col_energy[best_col] < row_energy[best_row]*s->reverse_twist
            &&
            col_energy[best_col]*s->normal_twist > row_energy[best_row])
        {
            /* Relative peak test */
            for (i = 0;  i < 4;  i++)
            {
                if ((i != best_col  &&  col_energy[i]*DTMF_RELATIVE_PEAK_COL > col_energy[best_col])
                    ||
                    (i != best_row  &&  row_energy[i]*DTMF_RELATIVE_PEAK_ROW > row_energy[best_row]))
                {
                    break;
                }
            }
            /* ... and fraction of total energy test */
            if (i >= 4
                &&
                (row_energy[best_row] + col_energy[best_col]) > DTMF_TO_TOTAL_ENERGY*s->energy)
            {
                hit = dtmf_positions[(best_row << 2) + best_col];
                /* Look for two successive similar results */
                /* The logic in the next test is:
                   We need two successive identical clean detects, with
                   two blocks of something different preceeding it.
                   This can work with:
                     - Back to back differing digits. Back-to-back digits should
                       not happen. The spec. says there should be a gap between digits.
                       However, many real phones do not impose a gap, and rolling across
                       the keypad can produce little or no gap.
                     - It tolerates nasty phones that give a very wobbly start to a digit.
                     - VoIP can give sample slips. The phase jumps that produces will cause
                       the block it is in to give no detection. This logic will ride over a
                       single missed block, and not falsely declare a second digit. If the
                       hiccup happens in the wrong place on a minimum length digit, however
                       we would still fail to detect that digit. Could anything be done to
                       deal with that? Packet loss is clearly a no-go zone.
                       Note this is only relevant to VoIP using A-law, u-law or similar.
                       Low bit rate codecs scramble DTMF too much for it to be recognised,
                       and often slip in units large than a sample. */
                if (hit == s->hits[2]  &&  hit != s->hits[1]  &&  hit != s->hits[0])
                {
                    if (s->realtime_callback)
                    {
                        /* Confirmed start of the digit */
                        s->realtime_callback(s->realtime_callback_data, hit);
                        s->in_digit = hit;
                    }
                    else
                    {
                        if (s->current_digits < MAX_DTMF_DIGITS)
                        {
                            s->digits[s->current_digits++] = hit;
                            s->digits[s->current_digits] = '\0';
                        }
                        else
                        {
                            if (s->callback)
                            {
                                s->callback(s->callback_data,
                                            s->digits,
                                            s->current_digits);
                                s->digits[0] = hit;
                                s->digits[1] = '\0';
                                s->current_digits = 1;
                            }
                            else
                            {
                                s->lost_digits++;
                            }
                        }
                    }
                }
            }
        }
        if (s->in_digit)
        {
            /* Look for the end of the digit */
            if (s->in_digit != hit  &&  s->in_digit != s->hits[2])
            {
                s->realtime_callback(s->realtime_callback_data, 0);
                s->in_digit = 0;
            }
        }
        s->hits[0] = s->hits[1];
        s->hits[1] = s->hits[2];
        s->hits[2] = hit;
        /* Reinitialise the detector for the next block */
        for (i = 0;  i < 4;  i++)
        {
            goertzel_init(&s->row_out[i], &dtmf_detect_row[i]);
            goertzel_init(&s->col_out[i], &dtmf_detect_col[i]);
        }
        s->energy = 0.0;
        s->current_sample = 0;
    }
    if (s->current_digits  &&  s->callback)
    {
        s->callback(s->callback_data, s->digits, s->current_digits);
        s->digits[0] = '\0';
        s->current_digits = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int dtmf_get(dtmf_rx_state_t *s, char *buf, int max)
{
    if (max > s->current_digits)
        max = s->current_digits;
    if (max > 0)
    {
        memcpy(buf, s->digits, max);
        memmove(s->digits, s->digits + max, s->current_digits - max);
        s->current_digits -= max;
    }
    buf[max] = '\0';
    return  max;
}
/*- End of function --------------------------------------------------------*/

bell_mf_rx_state_t *bell_mf_rx_init(bell_mf_rx_state_t *s,
                                    void (*callback)(void *user_data, const char *digits, int len),
                                    void *user_data)
{
    int i;
    static int initialised = FALSE;

    if (!initialised)
    {
        for (i = 0;  i < 6;  i++)
            make_goertzel_descriptor(&bell_mf_detect_desc[i], bell_mf_tones[i], 120);
        initialised = TRUE;
    }
    s->callback = callback;
    s->callback_data = user_data;

    s->hits[0] = 
    s->hits[1] =
    s->hits[2] =
    s->hits[3] = 
    s->hits[4] = 0;

    for (i = 0;  i < 6;  i++)
        goertzel_init(&s->out[i], &bell_mf_detect_desc[i]);
    s->current_sample = 0;
    s->lost_digits = 0;
    s->current_digits = 0;
    s->digits[0] = '\0';
    return s;
}
/*- End of function --------------------------------------------------------*/

int bell_mf_rx(bell_mf_rx_state_t *s, const int16_t *amp, int samples)
{
    float energy[6];
    float famp;
    float v1;
    int i;
    int j;
    int sample;
    int best;
    int second_best;
    int hit;
    int limit;

    hit = 0;
    for (sample = 0;  sample < samples;  sample = limit)
    {
        if ((samples - sample) >= (120 - s->current_sample))
            limit = sample + (120 - s->current_sample);
        else
            limit = samples;
        for (j = sample;  j < limit;  j++)
        {
            famp = amp[j];
    
            /* With GCC 2.95, the following unrolled code seems to take about 35%
               (rough estimate) as long as a neat little 0-5 loop */
            v1 = s->out[0].v2;
            s->out[0].v2 = s->out[0].v3;
            s->out[0].v3 = s->out[0].fac*s->out[0].v2 - v1 + famp;
    
            v1 = s->out[1].v2;
            s->out[1].v2 = s->out[1].v3;
            s->out[1].v3 = s->out[1].fac*s->out[1].v2 - v1 + famp;
    
            v1 = s->out[2].v2;
            s->out[2].v2 = s->out[2].v3;
            s->out[2].v3 = s->out[2].fac*s->out[2].v2 - v1 + famp;
    
            v1 = s->out[3].v2;
            s->out[3].v2 = s->out[3].v3;
            s->out[3].v3 = s->out[3].fac*s->out[3].v2 - v1 + famp;
    
            v1 = s->out[4].v2;
            s->out[4].v2 = s->out[4].v3;
            s->out[4].v3 = s->out[4].fac*s->out[4].v2 - v1 + famp;
    
            v1 = s->out[5].v2;
            s->out[5].v2 = s->out[5].v3;
            s->out[5].v3 = s->out[5].fac*s->out[5].v2 - v1 + famp;
        }
        s->current_sample += (limit - sample);
        if (s->current_sample < 120)
            continue;

        /* We are at the end of an MF detection block */
        /* Find the two highest energies. The spec says to look for
           two tones and two tones only. Taking this literally -ie
           only two tones pass the minimum threshold - doesn't work
           well. The sinc function mess, due to rectangular windowing
           ensure that! Find the two highest energies and ensure they
           are considerably stronger than any of the others. */
        energy[0] = goertzel_result(&s->out[0]);
        energy[1] = goertzel_result(&s->out[1]);
        if (energy[0] > energy[1])
        {
            best = 0;
            second_best = 1;
        }
        else
        {
            best = 1;
            second_best = 0;
        }
        for (i = 2;  i < 6;  i++)
        {
            energy[i] = goertzel_result(&s->out[i]);
            if (energy[i] >= energy[best])
            {
                second_best = best;
                best = i;
            }
            else if (energy[i] >= energy[second_best])
            {
                second_best = i;
            }
        }
        /* Basic signal level and twist tests */
        hit = FALSE;
        if (energy[best] >= BELL_MF_THRESHOLD
            &&
            energy[second_best] >= BELL_MF_THRESHOLD
            &&
            energy[best] < energy[second_best]*BELL_MF_TWIST
            &&
            energy[best]*BELL_MF_TWIST > energy[second_best])
        {
            /* Relative peak test */
            hit = TRUE;
            for (i = 0;  i < 6;  i++)
            {
                if (i != best  &&  i != second_best)
                {
                    if (energy[i]*BELL_MF_RELATIVE_PEAK >= energy[second_best])
                    {
                        /* The best two are not clearly the best */
                        hit = FALSE;
                        break;
                    }
                }
            }
        }
        if (hit)
        {
            /* Get the values into ascending order */
            if (second_best < best)
            {
                i = best;
                best = second_best;
                second_best = i;
            }
            best = best*5 + second_best - 1;
            hit = bell_mf_positions[best];
            /* Look for two successive similar results */
            /* The logic in the next test is:
               For KP we need 4 successive identical clean detects, with
               two blocks of something different preceeding it. For anything
               else we need two successive identical clean detects, with
               two blocks of something different preceeding it. */
            if (hit == s->hits[4]
                &&
                hit == s->hits[3]
                &&
                   ((hit != '*'  &&  hit != s->hits[2]  &&  hit != s->hits[1])
                    ||
                    (hit == '*'  &&  hit == s->hits[2]  &&  hit != s->hits[1]  &&  hit != s->hits[0])))
            {
                if (s->current_digits < MAX_DTMF_DIGITS)
                {
                    s->digits[s->current_digits++] = hit;
                    s->digits[s->current_digits] = '\0';
                }
                else
                {
                    if (s->callback)
                    {
                        s->callback(s->callback_data,
                                    s->digits,
                                    s->current_digits);
                        s->digits[0] = hit;
                        s->digits[1] = '\0';
                        s->current_digits = 1;
                    }
                    else
                    {
                        s->lost_digits++;
                    }
                }
            }
        }
        else
        {
            hit = 0;
        }
        s->hits[0] = s->hits[1];
        s->hits[1] = s->hits[2];
        s->hits[2] = s->hits[3];
        s->hits[3] = s->hits[4];
        s->hits[4] = hit;
        /* Reinitialise the detector for the next block */
        for (i = 0;  i < 6;  i++)
            goertzel_init(&s->out[i], &bell_mf_detect_desc[i]);
        s->current_sample = 0;
    }
    if (s->current_digits  &&  s->callback)
    {
        s->callback(s->callback_data, s->digits, s->current_digits);
        s->digits[0] = '\0';
        s->current_digits = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int bell_mf_get(bell_mf_rx_state_t *s, char *buf, int max)
{
    if (max > s->current_digits)
        max = s->current_digits;
    if (max > 0)
    {
        memcpy(buf, s->digits, max);
        memmove(s->digits, s->digits + max, s->current_digits - max);
        s->current_digits -= max;
    }
    buf[max] = '\0';
    return  max;
}
/*- End of function --------------------------------------------------------*/

r2_mf_rx_state_t *r2_mf_rx_init(r2_mf_rx_state_t *s, int fwd)
{
    int i;
    static int initialised = FALSE;

    s->hits[0] = 
    s->hits[1] =
    s->hits[2] = 0;
    s->fwd = fwd;

    if (!initialised)
    {
        for (i = 0;  i < 6;  i++)
        {
            make_goertzel_descriptor(&mf_fwd_detect_desc[i], r2_mf_fwd_tones[i], 133);
            make_goertzel_descriptor(&mf_back_detect_desc[i], r2_mf_back_tones[i], 133);
        }
        initialised = TRUE;
    }
    if (fwd)
    {
        for (i = 0;  i < 6;  i++)
            goertzel_init(&s->out[i], &mf_fwd_detect_desc[i]);
    }
    else
    {
        for (i = 0;  i < 6;  i++)
            goertzel_init(&s->out[i], &mf_back_detect_desc[i]);
    }
    s->samples = 133;
    s->current_sample = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

int r2_mf_rx(r2_mf_rx_state_t *s, const int16_t *amp, int samples)
{
    float energy[6];
    float famp;
    float v1;
    int i;
    int j;
    int sample;
    int best;
    int second_best;
    int hit;
    int hit_char;
    int limit;

    hit = 0;
    hit_char = 0;
    for (sample = 0;  sample < samples;  sample = limit)
    {
        if ((samples - sample) >= (s->samples - s->current_sample))
            limit = sample + (s->samples - s->current_sample);
        else
            limit = samples;
        for (j = sample;  j < limit;  j++)
        {
            famp = amp[j];
    
            /* With GCC 2.95, the following unrolled code seems to take about 35%
               (rough estimate) as long as a neat little 0-5 loop */
            v1 = s->out[0].v2;
            s->out[0].v2 = s->out[0].v3;
            s->out[0].v3 = s->out[0].fac*s->out[0].v2 - v1 + famp;
    
            v1 = s->out[1].v2;
            s->out[1].v2 = s->out[1].v3;
            s->out[1].v3 = s->out[1].fac*s->out[1].v2 - v1 + famp;
    
            v1 = s->out[2].v2;
            s->out[2].v2 = s->out[2].v3;
            s->out[2].v3 = s->out[2].fac*s->out[2].v2 - v1 + famp;
    
            v1 = s->out[3].v2;
            s->out[3].v2 = s->out[3].v3;
            s->out[3].v3 = s->out[3].fac*s->out[3].v2 - v1 + famp;
    
            v1 = s->out[4].v2;
            s->out[4].v2 = s->out[4].v3;
            s->out[4].v3 = s->out[4].fac*s->out[4].v2 - v1 + famp;
    
            v1 = s->out[5].v2;
            s->out[5].v2 = s->out[5].v3;
            s->out[5].v3 = s->out[5].fac*s->out[5].v2 - v1 + famp;
        }
        s->current_sample += (limit - sample);
        if (s->current_sample < s->samples)
            continue;

        /* We are at the end of an MF detection block */
        /* Find the two highest energies */
        energy[0] = goertzel_result(&s->out[0]);
        energy[1] = goertzel_result(&s->out[1]);
        if (energy[0] > energy[1])
        {
            best = 0;
            second_best = 1;
        }
        else
        {
            best = 1;
            second_best = 0;
        }
        
        for (i = 2;  i < 6;  i++)
        {
            energy[i] = goertzel_result(&s->out[i]);
            if (energy[i] >= energy[best])
            {
                second_best = best;
                best = i;
            }
            else if (energy[i] >= energy[second_best])
            {
                second_best = i;
            }
        }
        /* Basic signal level and twist tests */
        hit = FALSE;
        if (energy[best] >= R2_MF_THRESHOLD
            &&
            energy[second_best] >= R2_MF_THRESHOLD
            &&
            energy[best] < energy[second_best]*R2_MF_TWIST
            &&
            energy[best]*R2_MF_TWIST > energy[second_best])
        {
            /* Relative peak test */
            hit = TRUE;
            for (i = 0;  i < 6;  i++)
            {
                if (i != best  &&  i != second_best)
                {
                    if (energy[i]*R2_MF_RELATIVE_PEAK >= energy[second_best])
                    {
                        /* The best two are not clearly the best */
                        hit = FALSE;
                        break;
                    }
                }
            }
        }
        if (hit)
        {
            /* Get the values into ascending order */
            if (second_best < best)
            {
                i = best;
                best = second_best;
                second_best = i;
            }
            best = best*5 + second_best - 1;
            hit_char = mf_positions[best];
        }
        else
        {
            hit_char = 0;
        }

        /* Reinitialise the detector for the next block */
        if (s->fwd)
        {
            for (i = 0;  i < 6;  i++)
                goertzel_init(&s->out[i], &mf_fwd_detect_desc[i]);
        }
        else
        {
            for (i = 0;  i < 6;  i++)
                goertzel_init(&s->out[i], &mf_back_detect_desc[i]);
        }
        s->current_sample = 0;
    }
    return hit_char;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

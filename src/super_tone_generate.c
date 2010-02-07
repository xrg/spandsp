/*
 * SpanDSP - a series of DSP components for telephony
 *
 * super_tone_generate.c - Flexible telephony supervisory tone generation.
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
 * $Id: super_tone_generate.c,v 1.4 2004/03/12 16:27:24 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <stdint.h>
#include <sys/socket.h>
#include <math.h>

#include "spandsp/telephony.h"
#include "spandsp/super_tone_generate.h"

super_tone_tx_step_t *super_tone_tx_make_step(super_tone_tx_step_t *s,
                                              int f1,
                                              int l1,
                                              int f2,
                                              int l2,
                                              int length,
                                              int cycles)
{
    float gain;

    if (s == NULL)
    {
        s = (super_tone_tx_step_t *) malloc(sizeof(super_tone_tx_step_t));
        if (s == NULL)
            return NULL;
    }
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
    s->tone = (f1 > 0.0);
    s->length = length*8;
    s->cycles = cycles;
    s->next = NULL;
    s->nest = NULL;
    return  s;
}
/*- End of function --------------------------------------------------------*/

void super_tone_tx_free(super_tone_tx_step_t *tree)
{
    super_tone_tx_step_t *t;

    while (tree)
    {
        /* Follow nesting... */
        if (tree->nest)
            super_tone_tx_free(tree->nest);
        t = tree;
        tree = tree->next;
        free(t);
    }
}
/*- End of function --------------------------------------------------------*/

void super_tone_tx_init(super_tone_tx_state_t *tone, super_tone_tx_step_t *tree)
{
    tone->level = 0;
    tone->levels[0] = tree;
    tone->cycles[0] = tree->cycles;

    tone->current_position = 0;
}
/*- End of function --------------------------------------------------------*/

int super_tone_tx(super_tone_tx_state_t *tone, int16_t amp[], int max_samples)
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
    int len;
    super_tone_tx_step_t *tree;

    if (tone->level < 0  ||  tone->level > 3)
        return  0;
    samples = 0;
    tree = tone->levels[tone->level];
    while (tree  &&  samples < max_samples)
    {
        if (tree->tone)
        {
            /* A period of tone. A length of zero means infinite
               length. */
            if (tone->current_position == 0)
            {
                /* New step - prepare the tone generator */
                tone->v2_1 = tree->v2_1;
                tone->v3_1 = tree->v3_1;

                tone->v2_2 = tree->v2_2;
                tone->v3_2 = tree->v3_2;
            }
            len = tree->length - tone->current_position;
            if (tree->length == 0)
            {
                len = max_samples - samples;
                /* We just need to make current position non-zero */
                tone->current_position = 1;
            }
            else if (len > max_samples - samples)
            {
                len = max_samples - samples;
                tone->current_position += len;
            }
            else
            {
                tone->current_position = 0;
            }
            /* This is a second order IIR filter, configured to oscillate */
            /* The equation is x(n) = 2*cos(2.0*PI*f))*x(n-1) - x(n-2) */
            /* This isn't particularly accurate near the bottom of the band.
               If you recast the equation as
                 x(n) = 2*x(n-1) - 2*(1 - cos(2.0*PI*f))*x(n-1) - x(n-2)
               you get a better balance of errors as you move the frequency to be
               generated across the band. It takes an extra operation, though. */

            v2_1 = tone->v2_1;
            v3_1 = tone->v3_1;
            fac_1 = tree->fac_1;
            v2_2 = tone->v2_2;
            v3_2 = tone->v3_2;
            fac_2 = tree->fac_2;

            for (limit = len + samples;  samples < limit;  samples++)
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
                amp[samples] = xamp;
            }
            tone->v2_1 = v2_1;
            tone->v3_1 = v3_1;
            tone->v2_2 = v2_2;
            tone->v3_2 = v3_2;
            if (tone->current_position)
                return samples;
        }
        else if (tree->length)
        {
            /* A period of silence. The length must always
               be explicitly stated. A length of zero does
               not give infinite silence. */
            len = tree->length - tone->current_position;
            if (len > max_samples - samples)
            {
                len = max_samples - samples;
                tone->current_position += len;
            }
            else
            {
                tone->current_position = 0;
            }
            memset(amp + samples, 0, sizeof(uint16_t)*len);
            samples += len;
            if (tone->current_position)
                return samples;
        }
        /* Nesting has priority... */
        if (tree->nest)
        {
            tree = tree->nest;
            tone->levels[++tone->level] = tree;
            tone->cycles[tone->level] = tree->cycles;
        }
        else
        {
            /* ...Next comes repeating, and finally moving forward a step. */
            /* When repeating, note that zero cycles really means endless cycles. */
            while (tree->cycles  &&  --tone->cycles[tone->level] <= 0)
            {
                tree = tree->next;
                if (tree)
                {
                    /* A fresh new step. */
                    tone->levels[tone->level] = tree;
                    tone->cycles[tone->level] = tree->cycles;
                    break;
                }
                /* If we are nested we need to pop, otherwise this is the end. */
                if (tone->level <= 0)
                {
                    /* Mark the tone as completed */
                    tone->levels[0] = NULL;
                    break;
                }
                tree = tone->levels[--tone->level];
            }
        }
        
    }
    return  samples;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

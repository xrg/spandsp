/*
 * SpanDSP - a series of DSP components for telephony
 *
 * super_tone_detect.c - Flexible telephony supervisory tone detection.
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
 * $Id: super_tone_detect.c,v 1.6 2004/03/15 13:17:35 steveu Exp $
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
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"

#include "spandsp/super_tone_detect.h"

static int add_super_tone_freq(super_tone_rx_descriptor_t *desc, int freq)
{
    int i;

    if (freq == 0)
        return -1;
    /* Look for an existing frequency */
    for (i = 0;  i < desc->used_frequencies;  i++)
    {
        if (desc->pitches[i][0] == freq)
            return desc->pitches[i][1];
    }
    /* Look for an existing tone which is very close. We may need to merge
       the detectors. */
    for (i = 0;  i < desc->used_frequencies;  i++)
    {
        if ((desc->pitches[i][0] - 10) <= freq  &&  freq <= (desc->pitches[i][0] + 10))
        {
            /* Merge these two */
            desc->pitches[desc->used_frequencies][0] = freq;
            desc->pitches[desc->used_frequencies][1] = i;
            make_goertzel_descriptor(&desc->desc[desc->pitches[i][1]], (freq + desc->pitches[i][0])/2, BINS);
            desc->used_frequencies++;
            return desc->pitches[i][1];
        }
    }
    desc->pitches[i][0] = freq;
    desc->pitches[i][1] = desc->monitored_frequencies;
    if (desc->monitored_frequencies%5 == 0)
    {
        desc->desc = (goertzel_descriptor_t *) realloc(desc->desc, (desc->monitored_frequencies + 5)*sizeof(goertzel_descriptor_t));
    }
    make_goertzel_descriptor(&desc->desc[desc->monitored_frequencies++], freq, BINS);
    desc->used_frequencies++;
    return desc->pitches[i][1];
}
/*- End of function --------------------------------------------------------*/

int super_tone_rx_add_tone(super_tone_rx_descriptor_t *desc)
{
    if (desc->tones%5 == 0)
    {
        desc->tone_list = (super_tone_rx_segment_t **) realloc(desc->tone_list, (desc->tones + 5)*sizeof(super_tone_rx_segment_t *));
        desc->tone_segs = (int *) realloc(desc->tone_segs, (desc->tones + 5)*sizeof(int));
    }
    desc->tone_list[desc->tones] = NULL;
    desc->tone_segs[desc->tones] = 0;
    desc->tones++;
    return desc->tones - 1;
}
/*- End of function --------------------------------------------------------*/

int super_tone_rx_add_element(super_tone_rx_descriptor_t *desc,
                              int tone,
                              int f1,
                              int f2,
                              int min,
                              int max)
{
    int step;

    step = desc->tone_segs[tone];
    if (step%5 == 0)
    {
        desc->tone_list[tone] = (super_tone_rx_segment_t *) realloc(desc->tone_list[tone], (step + 5)*sizeof(super_tone_rx_segment_t));
    }
    desc->tone_list[tone][step].f1 = add_super_tone_freq(desc, f1);
    desc->tone_list[tone][step].f2 = add_super_tone_freq(desc, f2);
    desc->tone_list[tone][step].min_duration = min*8;
    desc->tone_list[tone][step].max_duration = (max == 0)  ?  0x7FFFFFFF  :  max*8;
    desc->tone_segs[tone]++;
    return step;
}
/*- End of function --------------------------------------------------------*/

static int test_cadence(super_tone_rx_segment_t *pattern,
                        int steps,
                        super_tone_rx_segment_t *test,
                        int rotation)
{
    int i;
    int j;

    if (rotation >= 0)
    {
        /* Check only for the sustaining of a tone in progress. This means
           we only need to check each block if the latest step is compatible
           with the tone template. */
        if (steps < 0)
        {
            /* A -ve value for steps indicates we just changed step, and need to
               check the last one ended within spec. If we don't do this
               extra test a low duration segment might be accepted as OK. */
            steps = -steps;
            j = (rotation + steps - 2)%steps;
            if (pattern[j].f1 != test[8].f1  ||  pattern[j].f2 != test[8].f2)
                return  0;
            if (pattern[j].min_duration > test[8].min_duration*BINS
                ||
                pattern[j].max_duration < test[8].min_duration*BINS)
            {
                return  0;
            }
        }
        j = (rotation + steps - 1)%steps;
        if (pattern[j].f1 != test[9].f1  ||  pattern[j].f2 != test[9].f2)
            return  0;
        if (pattern[j].max_duration < test[9].min_duration*BINS)
            return  0;
    }
    else
    {
        /* Look for a complete template match. */
        for (i = 0;  i < steps;  i++)
        {
            j = i + 10 - steps;
            if (pattern[i].f1 != test[j].f1  ||  pattern[i].f2 != test[j].f2)
                return  0;
            if (pattern[i].min_duration > test[j].min_duration*BINS
                ||
                pattern[i].max_duration < test[j].min_duration*BINS)
            {
                return  0;
            }
        }
    }
    return  1;
}
/*- End of function --------------------------------------------------------*/

super_tone_rx_descriptor_t *super_tone_rx_make_descriptor(super_tone_rx_descriptor_t *desc)
{
    if (desc == NULL)
    {
        desc = (super_tone_rx_descriptor_t *) malloc(sizeof(super_tone_rx_descriptor_t));
        if (desc == NULL)
            return NULL;
    }
    desc->tone_list = NULL;
    desc->tone_segs = NULL;

    desc->used_frequencies = 0;
    desc->monitored_frequencies = 0;
    desc->desc = NULL;
    desc->tones = 0;
    return desc;
}
/*- End of function --------------------------------------------------------*/

void super_tone_rx_segment_callback(super_tone_rx_state_t *super,
                                    void (*callback)(void *data, int f1, int f2, int duration))
{
    super->segment_callback = callback;
}
/*- End of function --------------------------------------------------------*/

super_tone_rx_state_t *super_tone_rx_init(super_tone_rx_state_t *super,
                                          super_tone_rx_descriptor_t *desc,
                                          void (*callback)(void *data, int code),
                                          void *data)
{
    int i;

    if (desc == NULL)
        return NULL;
    if (super == NULL)
    {
        super = (super_tone_rx_state_t *) malloc(sizeof(super_tone_rx_state_t) + desc->monitored_frequencies*sizeof(goertzel_state_t));
        if (super == NULL)
            return NULL;
    }

    for (i = 0;  i < 11;  i++)
    {
        super->segments[i].f1 = -1;
        super->segments[i].f2 = -1;
        super->segments[i].min_duration = 0;
    }
    super->segment_callback = NULL;
    super->tone_callback = callback;
    super->callback_data = data;
    if (desc)
        super->desc = desc;
    super->detected_tone = -1;
    super->energy = 0.0;
    super->total_energy = 0.0;
    for (i = 0;  i < desc->monitored_frequencies;  i++)
        goertzel_init(&super->state[i], &super->desc->desc[i]);
    return  super;
}
/*- End of function --------------------------------------------------------*/

#define THRESHOLD               8.0e7

int super_tone_rx(super_tone_rx_state_t *super, const int16_t *amp, int samples)
{
    int i;
    int j;
    int k1;
    int k2;
    int x;
    float res[BINS/2];
    int sample;

    for (sample = 0;  sample < samples;  sample += x)
    {
        x = 0;
        for (i = 0;  i < super->desc->monitored_frequencies;  i++)
        {
            x = goertzel_update(&super->state[i],
                                amp + sample,
                                samples - sample);
            if (i == super->desc->monitored_frequencies - 1)
            {
                for (j = 0;  j < x;  j++)
                    super->energy += amp[sample + j]*amp[sample + j];
            }
            if (super->state[i].current_sample >= super->state[i].samples)
            {
                res[i] = goertzel_result(&super->state[i]);
                goertzel_init(&super->state[i], &super->desc->desc[i]);
                if (i == super->desc->monitored_frequencies - 1)
                {
                    /* Scale the energy so it can be compared to the results from the
                       Goertzel filters. */
                    super->total_energy = super->energy*(super->state[i].samples/2);
                    super->energy = 0;
                    /* Find our two best monitored frequencies, which also have adequate
                       energy. */
                    if (super->total_energy < THRESHOLD)
                    {
                        k1 = -1;
                        k2 = -1;
                    }
                    else
                    {
                        if (res[0] > res[1])
                        {
                            k1 = 0;
                            k2 = 1;
                        }
                        else
                        {
                            k1 = 1;
                            k2 = 0;
                        }
                        for (j = 2;  j < super->desc->monitored_frequencies;  j++)
                        {
                            if (res[j] >= res[k1])
                            {
                                k2 = k1;
                                k1 = j;
                            }
                            else if (res[j] >= res[k2])
                            {
                                k2 = j;
                            }
                        }
                        if (res[k1] + res[k2] < 0.5*super->total_energy)
                        {
                            k1 = -1;
                            k2 = -1;
                        }
                        else if (res[k1] > 4.0*res[k2])
                        {
                            k2 = -1;
                        }
                        else if (k2 < k1)
                        {
                            j = k1;
                            k1 = k2;
                            k2 = j;
                        }
                    }
                    /* See if this looks different to last time */
                    if (k1 != super->segments[10].f1  ||  k2 != super->segments[10].f2)
                    {
                        /* It is different, but this might just be a transitional quirk, or
                           a one shot hiccup (eg due to noise). Only if this same thing is
                           seen a second time should we change state. */
                        super->segments[10].f1 = k1;
                        super->segments[10].f2 = k2;
                        /* While things are hopping around, consider this a continuance of the
                           previous state. */
                        super->segments[9].min_duration++;
                    }
                    else
                    {
                        if (k1 != super->segments[9].f1  ||  k2 != super->segments[9].f2)
                        {
                            if (super->detected_tone >= 0)
                            {
                                /* Test for the continuance of the existing tone pattern, based on our new knowledge of an
                                   entire segment length. */
                                if (!test_cadence(super->desc->tone_list[super->detected_tone], -super->desc->tone_segs[super->detected_tone], super->segments, super->rotation++))
                                {
                                    super->detected_tone = -1;
                                    super->tone_callback(super->callback_data, super->detected_tone);
                                }
                            }
                            if (super->segment_callback)
                            {
                                super->segment_callback(super->callback_data,
                                                        super->segments[9].f1,
                                                        super->segments[9].f2,
                                                        super->segments[9].min_duration*BINS/8);
                            }
                            memcpy (&super->segments[0], &super->segments[1], 9*sizeof(super->segments[0]));
                            super->segments[9].f1 = k1;
                            super->segments[9].f2 = k2;
                            super->segments[9].min_duration = 1;
                        }
                        else
                        {
                            /* This is a continuance of the previous state */
                            if (super->detected_tone >= 0)
                            {
                                /* Test for the continuance of the existing tone pattern. We must do this here, so we can sense the
                                   discontinuance of the tone on an excessively long segment. */
                                if (!test_cadence(super->desc->tone_list[super->detected_tone], super->desc->tone_segs[super->detected_tone], super->segments, super->rotation))
                                {
                                    super->detected_tone = -1;
                                    super->tone_callback(super->callback_data, super->detected_tone);
                                }
                            }
                            super->segments[9].min_duration++;
                        }
                    }
                    if (super->detected_tone < 0)
                    {
                        /* Test for the start of any of the monitored tone patterns */
                        for (j = 0;  j < super->desc->tones;  j++)
                        {
                            if (test_cadence(super->desc->tone_list[j], super->desc->tone_segs[j], super->segments, -1))
                            {
                                super->detected_tone = j;
                                super->rotation = 0;
                                super->tone_callback(super->callback_data, super->detected_tone);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    return  samples;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

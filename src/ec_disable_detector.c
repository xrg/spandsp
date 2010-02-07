/*
 * SpanDSP - a series of DSP components for telephony
 *
 * ec_disable_detector.c - A detector which should eventually meet the
 *                         G.164/G.165 requirements for detecting the
 *                         2100Hz echo cancellor disable tone.
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
 * $Id: ec_disable_detector.c,v 1.6 2004/07/24 11:46:54 steveu Exp $
 */
 
/*! \file */

#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "spandsp/telephony.h"
#include "spandsp/biquad.h"
#include "spandsp/dds.h"
#include "spandsp/ec_disable_detector.h"

void echo_can_disable_tone_tx_init(echo_can_disable_tx_state_t *s, int with_am)
{
    s->with_am = with_am;
    s->tone_phase_rate = dds_phase_step(2100.0);
    s->mod_phase_rate = dds_phase_step(15.0);
    s->tone_phase = 0;
    s->mod_phase = 0;
    s->hop_timer = 450*8;
    s->level = dds_scaling(-12);
    if (s->with_am)
        s->mod_level = s->level/5;
}
/*- End of function --------------------------------------------------------*/

int echo_can_disable_tone_tx(echo_can_disable_tx_state_t *s,
                             int16_t *amp,
                             int samples)
{
    int mod;
    int i;

    for (i = 0;  i < samples;  i++)
    {
        if (s->with_am)
            mod = s->level + dds_mod(&s->mod_phase, s->mod_phase_rate, s->mod_level, 0);
        else
            mod = s->level;
        if (--s->hop_timer <= 0)
        {
            s->hop_timer = 450*8;
            s->tone_phase += 0x80000000;
        }
        amp[i] = dds_mod(&s->tone_phase, s->tone_phase_rate, mod, 0);
    }
    return samples;
}
/*- End of function --------------------------------------------------------*/

void echo_can_disable_tone_rx_init(echo_can_disable_rx_state_t *s)
{
    /* Elliptic notch */
    /* This is actually centred at 2095Hz, but gets the balance we want, due
       to the asymmetric walls of the notch */
    biquad2_init(&s->notch,
                 (int32_t) (-0.7600000*32768.0),
                 (int32_t) (-0.1183852*32768.0),
                 (int32_t) (-0.5104039*32768.0),
                 (int32_t) ( 0.1567596*32768.0),
                 (int32_t) ( 1.0000000*32768.0));

    s->channel_level = 0;
    s->notch_level = 0;    
    s->tone_present = FALSE;
    s->tone_cycle_duration = 0;
    s->good_cycles = 0;
    s->hit = 0;
}
/*- End of function --------------------------------------------------------*/

int echo_can_disable_tone_rx(echo_can_disable_rx_state_t *s,
                             const int16_t *amp,
                             int samples)
{
    int i;
    int16_t notched;
    
    for (i = 0;  i < samples;  i++)
    {
        notched = biquad2(&s->notch, amp[i]);
        /* Estimate the overall energy in the channel, and the energy in
           the notch (i.e. overall channel energy - tone energy => noise).
           Use abs instead of multiply for speed (is it really faster?).
           Damp the overall energy a little more for a stable result.
           Damp the notch energy a little less, so we don't damp out the
           blip every time the phase reverses */
        s->channel_level += ((abs(amp[i]) - s->channel_level) >> 5);
        s->notch_level += ((abs(notched) - s->notch_level) >> 4);
        if (s->channel_level > 280)
        {
            /* There is adequate energy in the channel. Is it mostly at 2100Hz? */
            if (s->notch_level*6 < s->channel_level)
            {
                /* The notch says yes, so we have the tone. */
                if (!s->tone_present)
                {
                    /* Do we get a kick every 450+-25ms? */
                    if (s->tone_cycle_duration >= 425*8
                        &&
                        s->tone_cycle_duration <= 475*8)
                    {
                        s->good_cycles++;
                        if (s->good_cycles > 2)
                            s->hit = TRUE;
                    }
                    s->tone_cycle_duration = 0;
                }
                s->tone_present = TRUE;
            }
            else
            {
                s->tone_present = FALSE;
            }
            s->tone_cycle_duration++;
        }
        else
        {
            s->tone_present = FALSE;
            s->tone_cycle_duration = 0;
            s->good_cycles = 0;
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

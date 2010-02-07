/*
 * SpanDSP - a series of DSP components for telephony
 *
 * ec_disable_detector.h - A detector which should eventually meet the
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
 * $Id: ec_disable_detector.h,v 1.5 2005/01/18 14:05:48 steveu Exp $
 */
 
/*! \file */

#if !defined(_EC_DISABLE_DETECTOR_H_)
#define _EC_DISABLE_DETECTOR_H_

/*! \page echo_can_disable_page Echo cancellor disable tone detection

\section echo_can_disable_page_sec_1 What does it do?
Some telephony terminal equipment, such as modems, require a channel which is as
clear as possible. They use their own echo cancellation. If the network is also
performing echo cancellation the two cancellors can end of squabbling about the
nature of the channel, with bad results. A special tone is defined which should
cause the network to disable any echo cancellation processes. 

\section echo_can_disable_page_sec_2 How does it work?
A sharp notch filter is implemented as a single bi-quad section. The presence of
the 2100Hz disable tone is detected by comparing the notched filtered energy
with the unfiltered energy. If the notch filtered energy is much lower than the
unfiltered energy, then a large proportion of the energy must be at the notch
frequency. This type of detector may seem less intuitive than using a narrow
bandpass filter to isolate the energy at the notch freqency. However, a sharp
bandpass implemented as an IIR filter rings badly, The reciprocal notch filter
is very well behaved. 
*/

typedef struct
{
    /*! \brief TRUE if we are generating the version with some 15Hz AM content,
        as in V.8 */
    int with_am;
    
    uint32_t tone_phase;
    int32_t tone_phase_rate;
    int level;
    /*! \brief Countdown to the next phase hop */
    int hop_timer;
    uint32_t mod_phase;
    int32_t mod_phase_rate;
    int mod_level;
} echo_can_disable_tx_state_t;

/*!
    Echo canceller disable tone detector descriptor. This defines the state
    of a single working instance of the detector.
*/
typedef struct
{
    biquad2_state_t notch;
    int notch_level;
    int channel_level;
    int tone_present;
    int tone_cycle_duration;
    int good_cycles;
    int hit;
} echo_can_disable_rx_state_t;

#ifdef __cplusplus
extern "C" {
#endif

/*! \brief Initialse an instance of the echo canceller disable tone generator.
    \param s The context.
*/
void echo_can_disable_tone_tx_init(echo_can_disable_tx_state_t *s, int with_am);
int echo_can_disable_tone_tx(echo_can_disable_tx_state_t *s,
                             int16_t *amp,
                             int samples);

/*! \brief Initialse an instance of the echo canceller disable tone detector.
    \param s The context.
*/
void echo_can_disable_tone_rx_init(echo_can_disable_rx_state_t *s);

/*! \brief Process a block of samples through an instance of the echo canceller
           disable tone detector.
    \param s The context.
    \param amp An array of signal samples.
    \param samples The number of samples in the array.
    \return The number of unprocessed samples.
*/
int echo_can_disable_tone_rx(echo_can_disable_rx_state_t *s,
                             const int16_t *amp,
                             int samples);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

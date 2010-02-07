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
 * $Id: ec_disable_detector.h,v 1.3 2004/03/19 19:12:46 steveu Exp $
 */
 
/*! \file */

#if !defined(_EC_DISABLE_DETECTOR_H_)
#define _EC_DISABLE_DETECTOR_H_

/*! \page echo_can_disable_page Echo cancellor disable tone detection
Some telephony terminal equipment, such as modems, require a channel which is as
clear as possible. They use their own echo cancellation. If the network is also
performing echo cancellation the two cancellors can end of squabbling about the
nature of the channel, with bad results. A special tone is defined which should
cause the network to disable any echo cancellation processes. 

\section echo_can_disable_page_sec_1 Theory of operation
A sharp notch filter is implemented as a single bi-quad section. The presence of
the 2100Hz disable tone is detected by comparing the notched filtered energy
with the unfiltered energy. If the notch filtered energy is much lower than the
unfiltered energy, then a large proportion of the energy must be at the notch
frequency. This type of detector may seem less intuitive than using a narrow
bandpass filter to isolate the energy at the notch freqency. However, a sharp
bandpass implemented as an IIR filter rings badly, The reciprocal notch filter
is very well behaved. 
*/

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
} echo_can_disable_detector_state_t;

/*! \brief Initialse an instance of the echo canceller disable tone detector.
    \param det The context.
*/
void echo_can_disable_detector_init(echo_can_disable_detector_state_t *det);

/*! \brief Process a block of samples through an instance of the echo canceller
           disable tone detector.
    \param det The context.
    \param amp An array of signal samples.
    \param samples The number of samples in the array.
    \return TRUE if the tone is present, otherwise FALSE.
*/
int echo_can_disable_detector_update(echo_can_disable_detector_state_t *det,
                                     const int16_t *amp,
                                     int samples);

#endif
/*- End of file ------------------------------------------------------------*/

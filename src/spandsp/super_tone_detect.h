/*
 * SpanDSP - a series of DSP components for telephony
 *
 * super_tone_detect.h - Flexible telephony supervisory tone detection.
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
 * $Id: super_tone_detect.h,v 1.3 2004/03/19 19:12:46 steveu Exp $
 */

#if !defined(_SUPER_TONE_DETECT_H_)
#define _SUPER_TONE_DETECT_H_

/*! \page super_tone_rx_page Supervisory tone detection

The supervisory tone detector may be configured to detect most of the world's
telephone supervisory tones - things like ringback, busy, number unobtainable,
and so on. 

\section super_tone_rx_page_sec_1 Theory of operation

The supervisory tone detector is passed a series of data structures describing
the tone patterns - the frequencies and cadencing - of the tones to be searched
for. It constructs one or more Goertzel filters to monitor the required tones.
If tones are close in frequency a single Goertzel set to the centre of the
frequency range will be used. This optimises the efficiency of the detector. The
Goertzel filters are applied without applying any special window functional
(i.e. they use a rectangular window), so they have a sinc like response.
However, for most tone patterns their rejection qualities are adequate. 
*/

#define BINS            128

typedef struct
{
    int f1;
    int f2;
    int recognition_duration;
    int min_duration;
    int max_duration;
} super_tone_rx_segment_t;

typedef struct
{
    int used_frequencies;
    int monitored_frequencies;
    int pitches[BINS/2][2];
    int tones;
    super_tone_rx_segment_t **tone_list;
    int *tone_segs;
    goertzel_descriptor_t *desc;
} super_tone_rx_descriptor_t;

typedef struct
{
    super_tone_rx_descriptor_t *desc;
    float energy;
    float total_energy;
    int detected_tone;
    int rotation;
    void (*tone_callback)(void *data, int code);
    void (*segment_callback)(void *data, int f1, int f2, int duration);
    void *callback_data;
    super_tone_rx_segment_t segments[11];
    goertzel_state_t state[0];
} super_tone_rx_state_t;

/*! Create Add a new tone pattern to a supervisory tone detector.
    \param desc The supervisory tone set desciptor. If NULL, the routine will allocate space for a
                descriptor.
    \return The supervisory tone set desciptor. */
super_tone_rx_descriptor_t *super_tone_rx_make_descriptor(super_tone_rx_descriptor_t *desc);
/*! Add a new tone pattern to a supervisory tone detector set.
    \param desc The supervisory tone set descriptor.
    \return The new tone ID. */
int super_tone_rx_add_tone(super_tone_rx_descriptor_t *desc);
/*! Add a new tone pattern element to a tone pattern in a supervisory tone detector.
    \param desc The supervisory tone set desciptor.
    \param tone The tone ID within the descriptor.
    \param f1 Frequency 1 (-1 for a silent period).
    \param f2 Frequency 2 (-1 for a silent period, or only one frequency).
    \param min The minimum duration, in ms.
    \param max The maximum duration, in ms.
    \return The new number of elements in the tone description. */
int super_tone_rx_add_element(super_tone_rx_descriptor_t *desc,
                              int tone,
                              int f1,
                              int f2,
                              int min,
                              int max);
/*! Initialise a supervisory tone detector.
    \param super The supervisory tone context.
    \param desc The tone descriptor.
    \param callback The callback routine called to report the valid detection or termination of
           one of the monitored tones.
    \param data An opaque pointer passed when calling the callback routine.
    \return The supervisory tone context. */
super_tone_rx_state_t *super_tone_rx_init(super_tone_rx_state_t *super,
                                          super_tone_rx_descriptor_t *desc,
                                          void (*callback)(void *data, int code),
                                          void *data);
/*! Define a callback routine to be called each time a tone pattern element is complete. This is
    mostly used when analysing a tone.
    \param super The supervisory tone context. */
void super_tone_rx_segment_callback(super_tone_rx_state_t *super,
                                    void (*callback)(void *data, int f1, int f2, int duration));
/*! Apply supervisory tone detection processing to a block of audio samples.
    \brief Apply supervisory tone detection processing to a block of audio samples.
    \param super The supervisory tone context.
    \param amp The audio sample buffer.
    \param samples The number of samples in the buffer.
    \return The number of samples processed.
*/
int super_tone_rx(super_tone_rx_state_t *super, const int16_t *amp, int samples);

#endif
/*- End of file ------------------------------------------------------------*/

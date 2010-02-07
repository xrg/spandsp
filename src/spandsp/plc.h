/*
 * SpanDSP - a series of DSP components for telephony
 *
 * plc.h
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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
 * $Id: plc.h,v 1.4 2005/01/17 13:12:15 steveu Exp $
 */

/*! \file */

#if !defined(_PLC_H_)
#define _PLC_H_

/*! \page plc_page Packet loss concealment
\section plc_page_sec_1 What does it do

The packet loss concealment module uses an algorithm somewhat like the one in
Appendix I of G.711. There are, however, a number of differences intended to
improve its speed. It is not tied to any particular codec, and could be used
with almost any codec which does not specify its own procedure for packet loss
concealment.

The algorithm in appendix I of G.711 looks for peaks in the autocorrelation function
of the speech, as an indication of its basic pitch contour. The present code uses
AMDF instead.

The algorithm in appendix I of G.711 is rigidly structured around 10ms blocks of audio,
with the implicit assumption VoIP packets will contain 10ms of audio. The present code
is not tied to a specific block size. They may be freely chosen, to suit any particular
packet duration a codec may use.

When audio is received normally, each received block is passed to the routine plc_rx.
This routine stores some signal history, such that if the next block is missing, the
algorithm is be able to synthesise a suitable fill-in signal. The G.711 algorithm
delays the audio a little, to allow OLA smoothing of the signal as erasure begins.
The present software avoids this by smoothing the start if the fill-in signal itself.
The method used may give slightly poorer results that the G.711 algorithm. However,
it gains considerably in efficiency between erasures.

When a block of audio is not received by the time it is required, the routine plc_fillin
is called. This generates a synthetic signal, based on the last known good signal. At the
beginning of an erasure we determine the pitch of the last good signal, using the historical
data kept by plc_rx. We determine the pitch of the data at the tail of the history buffer.
We overlap a 1/4 wavelength, to smooth the transition from the real to the synthetic signal,
and repeat pitch periods of the signal for the length of the erasure. As the gap between
periods of good signal widens, the likelyhood of the synthetic signal being close to correct
falls. Therefore, the volume of the synthesized signal is made to decay linearly, such that
after 50ms of missing audio it is reduced to silence.

When the real signal resumes after a period of erasure, the tail of the synthetic signal is
OLA smoothed with the start of the real signal, to avoid a sharp change in the signal.
*/

/*! Minimum allowed pitch (66 Hz) */
#define PLC_PITCH_MIN           120
/*! Maximum allowed pitch (200 Hz) */
#define PLC_PITCH_MAX           40
/*! Maximum pitch OLA window */
#define PLC_PITCH_OVERLAP_MAX   (PLC_PITCH_MIN >> 2)
/*! The length over which the AMDF function looks for similarity (20 ms) */
#define CORRELATION_SPAN        160
/*! History buffer length. The buffer much also be at leat 1.25 times
    PLC_PITCH_MIN, but that is much smaller than the buffer needs to be for
    the pitch assessment. */
#define PLC_HISTORY_LEN         (CORRELATION_SPAN + PLC_PITCH_MIN)

typedef struct
{
    /*! Consecutive erased samples */
    int missing_samples;
    /*! Current offset into pitch period */
    int pitch_offset;
    /*! Pitch estimate */
    int pitch;
    /*! Buffer for a cycle of speech */
    float pitchbuf[PLC_PITCH_MIN];
    /*! History buffer */
    int16_t history[PLC_HISTORY_LEN];
    /*! Current pointer into the history buffer */
    int buf_ptr;
} plc_state_t;


#ifdef __cplusplus
extern "C" {
#endif

int plc_rx(plc_state_t *s, int16_t amp[], int len);
int plc_fillin(plc_state_t *s, int16_t amp[], int len);
plc_state_t *plc_init(plc_state_t *s);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

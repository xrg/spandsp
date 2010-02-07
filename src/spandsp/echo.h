/*
 * SpanDSP - a series of DSP components for telephony
 *
 * echo.h - An echo cancellor, suitable for electrical and acoustic
 *	    cancellation. This code does not currently comply with
 *	    any relevant standards (e.g. G.164/5/7/8).
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
 *
 * Based on a bit from here, a bit from there, eye of toad,
 * ear of bat, etc - plus, of course, my own 2 cents.
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
 * $Id: echo.h,v 1.4 2005/01/18 14:05:49 steveu Exp $
 */

/*! \file */

#if !defined(_ECHO_H_)
#define _ECHO_H_

/*! \page echo_can_page Echo cancellation

\section echo_can_page_sec_1 What does it do?
This echo cancellation module is intended to cover the need to cancel electrical
echoes (e.g. from 2-4 wire hybrids) and acoustic echoes (e.g. room echoes for
speakerphones). 

\section echo_can_page_sec_2 How does it work?
The heart of the echo cancellor is an adaptive FIR filter. This is adapted to
match the impulse response of the environment being cancelled (say, a telephone
line or a room). It must be long enough to adequately cover the duration of that
impulse response. The signal being transmitted into the environment being
cancelled is passed through the FIR filter. The resulting output is an estimate
of the echo signal. This is then subtracted from the received signal, and the
result should be an estimate of the signal which originates within the
environment being cancelled (people talking in the room, or the signal from the
far end of a telephone line) free from the echos of our own transmitted signal. 

The FIR filter is adapted using a LMS algorithm. This method performs well,
provided certain conditions are met: 

    - The transmitted signal has poor self-correlation.
    - There is no signal being generated within the environment being cancelled.

The difficulty is that neither of these can be guaranteed. If the adaption is
performed while transmitting noise (or something fairly noise like, such as
voice) the adaption works very well. If the adaption is performed while
transmitting something highly correlative (e.g. tones, like DTMF), the adaption
can go seriously wrong. The reason is there is only one solution for the
adaption on a near random signal. For a repetitive signal, there are a number of
solutions which converge the adaption, and nothing guides the adaption to choose
the correct one. 

The adaption process is based on trying to eliminate the received signal. When
there is any signal from within the environment being cancelled it may upset the
adaption process. Similarly, if the signal we are transmitting is small, noise
may dominate and disturb the adaption process. If we can ensure that the
adaption is only performed when we are transmitting a significant signal level,
and the environment is not, things will be OK. Clearly, it is easy to tell when
we are sending a significant signal. Telling, if the environment is generating a
significant signal, and doing it with sufficient speed that the adaption will
not have diverged too much more we stop it, is a little harder. 

The key problem in detecting when the environment is sourcing significant energy
is that we must do this very quickly. Given a reasonably long sample of the
received signal, there are a number of strategies which may be used to assess
whether that signal contains a strong far end component. However, by the time
that assessment is complete the far end signal will have already caused major
mis-convergence in the adaption process. An assessment algorithm is needed which
produces a fairly accurate result from a very short burst of far end energy. 

\section echo_can_page_sec_3 How do I use it?
The echo cancellor processes both the transmit and receive streams sample by
sample. The processing function is not declared inline. Unfortunately,
cancellation requires many operations per sample, so the call overhead is only a
minor burden. 
*/

#include "fir.h"

#define NONUPDATE_DWELL_TIME	600 	/* 600 samples, or 75ms */

/* Mask bits for the adaption mode */
#define ECHO_CAN_USE_NLP            0x01
#define ECHO_CAN_USE_SUPPRESSOR     0x02
#define ECHO_CAN_USE_CNG            0x04
#define ECHO_CAN_FREEZE_ADAPTION    0x08

/*!
    G.168 echo canceller descriptor. This defines the working state for a line
    echo canceller.
*/
typedef struct
{
    int tx_power[4];
    int rx_power[3];
    int clean_rx_power;

    int rx_power_threshold;
    int nonupdate_dwell;

    fir16_state_t fir_state;
    int16_t *fir_taps16[4];	/* Echo FIR taps (16 bit version) */
    int32_t *fir_taps32;	/* Echo FIR taps (32 bit version) */

    int curr_pos;
	
    int taps;
    int tap_mask;
    int adaption_mode;
    
    int32_t supp_test1;
    int32_t supp_test2;
    int32_t supp1;
    int32_t supp2;
    int vad;
    int cng;
    int cng_level;
    int cng_filter;
    
    int16_t geigel_max;
    int geigel_lag;
    int dtd_onset;
    int tap_set;
    int tap_rotate_counter;

    int32_t latest_correction;  /* Indication of the magnitude of the latest
                                   adaption, or a code to indicate why adaption
                                   was skipped, for test purposes */
    int32_t last_acf[28];
    int32_t acf[28];
    int acf_count;
    int narrowband_score;
} echo_can_state_t;

echo_can_state_t *echo_can_create(int len, int adaption_mode);
void echo_can_free(echo_can_state_t *ec);
void echo_can_adaption_mode(echo_can_state_t *ec, int adaption_mode);
void echo_can_flush(echo_can_state_t *ec);
int16_t echo_can_update(echo_can_state_t *ec, int16_t tx, int16_t rx);

#endif
/*- End of file ------------------------------------------------------------*/

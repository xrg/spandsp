/*
 * SpanDSP - a series of DSP components for telephony
 *
 * modem_echo.h - An echo cancellor, suitable for electrical echos in GSTN modems
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001, 2004 Steve Underwood
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
 * $Id: modem_echo.h,v 1.2 2005/01/18 14:05:49 steveu Exp $
 */

/*! \file */

#if !defined(_MODEM_ECHO_H_)
#define _MODEM_ECHO_H_

/*! \page modem_echo_can_page Line echo cancellation for modems

\section modem_echo_can_page_sec_1 What does it do?
This echo cancellation module is intended to cover the need to cancel electrical
echoes (e.g. from 2-4 wire hybrids). 

\section modem_echo_can_page_sec_2 How does it work?
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

    - The transmitted signal has weak self-correlation.
    - There is no signal being generated within the environment being cancelled.

The difficulty is that neither of these can be guaranteed. If the adaption is
performed while transmitting noise (or something fairly noise like, such as
voice) the adaption works very well. If the adaption is performed while
transmitting something highly correlative (e.g. tones, like DTMF), the adaption
can go seriously wrong. The reason is there is only one solution for the
adaption on a near random signal. For a repetitive signal, there are a number of
solutions which converge the adaption, and nothing guides the adaption to choose
the correct one. 

\section modem_echo_can_page_sec_3 How do I use it?
The echo cancellor processes both the transmit and receive streams sample by
sample. The processing function is not declared inline. Unfortunately,
cancellation requires many operations per sample, so the call overhead is only a
minor burden. 
*/

#include "fir.h"

/*!
    Modem line echo canceller descriptor. This defines the working state for a line
    echo canceller.
*/
typedef struct
{
    int adapt;
    int taps;

    fir16_state_t fir_state;
    int16_t *fir_taps16;	/* Echo FIR taps (16 bit version) */
    int32_t *fir_taps32;	/* Echo FIR taps (32 bit version) */

    int tx_power;
    int rx_power;

    int curr_pos;
} modem_echo_can_state_t;

modem_echo_can_state_t *modem_echo_can_create(int len);
void modem_echo_can_free(modem_echo_can_state_t *ec);
void modem_echo_can_flush(modem_echo_can_state_t *ec);
void modem_echo_can_adaption_mode(modem_echo_can_state_t *ec, int adapt);
int16_t modem_echo_can_update(modem_echo_can_state_t *ec, int16_t tx, int16_t rx);

#endif
/*- End of file ------------------------------------------------------------*/

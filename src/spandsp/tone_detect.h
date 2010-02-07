/*
 * SpanDSP - a series of DSP components for telephony
 *
 * tone_detect.h - General telephony tone detection, and specific
 *                 detection of DTMF.
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
 * $Id: tone_detect.h,v 1.11 2005/01/18 14:05:49 steveu Exp $
 */

#if !defined(_TONE_DETECT_H_)
#define _TONE_DETECT_H_

/*! \page DTMF_rx_page DTMF receiver
\section DTMF_rx_page_sec_1 What does it do?
The DTMF receiver detects the standard DTMF digits. It is compliant with
ITU-T Q.23, ITU-T Q.24, and the local DTMF specifications of most administrations.
Its passes the test suites. It also scores *very* well on the standard
talk-off tests. 

The current design uses floating point extensively. It is not
tolerant of DC or dial tone. It is expected that a DC restore stage will be
placed before the DTMF detector. Whether dial tone tolerance matter depends on
your application. If you are using the code in an IVR application you will need
proper echo cancellation to get good performance in the prescence of speech
prompts. 

\section DTMF_rx_page_sec_2 How does it work?
Like most other DSP based DTMF detector's, this one uses the Goertzel algorithm
to look for the DTMF tones. What makes each detector design different is just how
that algorithm is used.

TODO:
*/

/*! \page MFC_R2_tone_rx_page MFC/R2 tone receiver

\section MFC_R2_tone_rx_page_sec_1 What does it do?
The MFC/R2 tone receiver module provides for the detection of the
repertoire of 15 dual tones needs for the digital MFC/R2 signalling protocol. 
It is compliant with ITU-T Q.441D.
*/

/*! \page Bell_MF_tone_rx_page Bell MF tone receiver

\section Bell_MF_tone_rx_page_sec_1 What does it do?
The Bell MF tone receiver module provides for the detection of the
repertoire of 15 dual tones needs for various Bell MF signalling protocols. 
It is compliant with ITU-T Q.320, ITU-T Q.322, ITU-T Q.323B.
*/

/*!
    Floating point Goertzel filter descriptor.
*/
typedef struct
{
    float fac;
    int samples;
} goertzel_descriptor_t;

/*!
    Floating point Goertzel filter state descriptor.
*/
typedef struct
{
    float v2;
    float v3;
    float fac;
    int samples;
    int current_sample;
} goertzel_state_t;

#if !defined(MAX_DTMF_DIGITS)
#define MAX_DTMF_DIGITS 128
#endif

/*!
    DTMF digit detector descriptor.
*/
typedef struct
{
    goertzel_state_t row_out[4];
    goertzel_state_t col_out[4];
    float energy;
    int hits[3];
    int current_sample;
    void (*callback)(void *data, char *digits, int len);
    void *callback_data;

    char digits[MAX_DTMF_DIGITS + 1];
    int current_digits;
    int detected_digits;
    int lost_digits;
    int digit_hits[16];
} dtmf_rx_state_t;

/*!
    Bell MF digit detector descriptor.
*/
typedef struct
{
    goertzel_state_t out[6];
    int hits[5];
    int current_sample;
    void (*callback)(void *data, char *digits, int len);
    void *callback_data;

    char digits[MAX_DTMF_DIGITS + 1];
    int current_digits;
    int detected_digits;
    int lost_digits;
    int digit_hits[16];
} bell_mf_rx_state_t;

/*!
    MFC/R2 tone detector descriptor.
*/
typedef struct
{
    goertzel_state_t out[6];
    int hits[3];
    int fwd;
    int samples;
    int current_sample;

    char digits[MAX_DTMF_DIGITS + 1];
    int current_digits;
    int lost_digits;
} r2_mf_rx_state_t;

#ifdef __cplusplus
extern "C" {
#endif

void make_goertzel_descriptor(goertzel_descriptor_t *t,
                              int freq,
                              int samples);
void goertzel_init(goertzel_state_t *s,
                   goertzel_descriptor_t *t);
int goertzel_update(goertzel_state_t *s,
                    const int16_t amp[],
                    int samples);
float goertzel_result(goertzel_state_t *s);

void dtmf_rx_init(dtmf_rx_state_t *s,
                  void (*callback)(void *data, char *digits, int len),
                  void *data);
int dtmf_rx(dtmf_rx_state_t *s, const int16_t *amp, int samples);
int dtmf_get(dtmf_rx_state_t *s, char *buf, int max);

void bell_mf_rx_init(bell_mf_rx_state_t *s,
                     void (*callback)(void *data, char *digits, int len),
                     void *data);
int bell_mf_rx(bell_mf_rx_state_t *s, const int16_t *amp, int samples);
int bell_mf_get(bell_mf_rx_state_t *s, char *buf, int max);

void r2_mf_rx_init(r2_mf_rx_state_t *s, int fwd);
int r2_mf_rx(r2_mf_rx_state_t *s, const int16_t *amp, int samples);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

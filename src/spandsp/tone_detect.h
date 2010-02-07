/*
 * SpanDSP - a series of DSP components for telephony
 *
 * tone_detect.h - General telephony tone detection, and specific
 *                 detection of DTMF, Bell MF, and MFC/R2.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001, 2005 Steve Underwood
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
 * $Id: tone_detect.h,v 1.20 2006/02/06 15:00:56 steveu Exp $
 */

#if !defined(_TONE_DETECT_H_)
#define _TONE_DETECT_H_

/*! \page dtmf_rx_page DTMF receiver
\section dtmf_rx_page_sec_1 What does it do?
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

\section dtmf_rx_page_sec_2 How does it work?
Like most other DSP based DTMF detector's, this one uses the Goertzel algorithm
to look for the DTMF tones. What makes each detector design different is just how
that algorithm is used.

Basic DTMF specs:
    - Minimum tone on = 40ms
    - Minimum tone off = 50ms
    - Maximum digit rate = 10 per second
    - Normal twist <= 8dB accepted
    - Reverse twist <= 4dB accepted
    - S/N >= 15dB will detect OK
    - Attenuation <= 26dB will detect OK
    - Frequency tolerance +- 1.5% will detect, +-3.5% will reject

TODO:
*/

/*! \page mfc_r2_tone_rx_page MFC/R2 tone receiver

\section mfc_r2_tone_rx_page_sec_1 What does it do?
The MFC/R2 tone receiver module provides for the detection of the
repertoire of 15 dual tones needs for the digital MFC/R2 signalling protocol. 
It is compliant with ITU-T Q.441D.

\section mfc_r2_tone_rx_page_sec_2 How does it work?
Basic MFC/R2 tone detection specs:
    - Receiver response range: -5dBm to -35dBm
    - Difference in level for a pair of frequencies
        - Adjacent tones: <5dB
        - Non-adjacent tones: <7dB
    - Receiver not to detect a signal of 2 frequencies of level -5dB and
      duration <7ms.
    - Receiver not to recognise a signal of 2 frequencies having a difference
      in level >=20dB.
    - Max received signal frequency error: +-10Hz
    - The sum of the operate and release times of a 2 frequency signal not to
      exceed 80ms (there are no individual specs for the operate and release
      times).
    - Receiver not to release for signal interruptions <=7ms.
    - System malfunction due to signal interruptions >7ms (typically 20ms) is
      prevented by further logic elements.
*/

/*! \page bell_mf_tone_rx_page Bell MF tone receiver

\section bell_mf_tone_rx_page_sec_1 What does it do?
The Bell MF tone receiver module provides for the detection of the
repertoire of 15 dual tones needs for various Bell MF signalling protocols. 
It is compliant with ITU-T Q.320, ITU-T Q.322, ITU-T Q.323B.

\section bell_mf_tone_rx_page_sec_2 How does it work?
Basic Bell MF tone detection specs:
    - Frequency tolerance +- 1.5% +-10Hz
    - Signal level -14dBm to 0dBm
    - Perform a "two and only two tones present" test.
    - Twist <= 6dB accepted
    - Receiver sensitive to signals above -22dBm per frequency
    - Test for a minimum of 55ms if KP, or 30ms of other signals.
    - Signals to be recognised if the two tones arrive within 8ms of each other.
    - Invalid signals result in the return of the re-order tone.

Note: Above -3dBm the signal starts to clip. We can detect with a little clipping,
      but not up to 0dBm, which the above spec seems to require. There isn't a lot
      we can do about that. Is the spec. incorrectly worded about the dBm0 reference
      point, or have I misunderstood it?
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
    void (*callback)(void *data, const char *digits, int len);
    void *callback_data;
    void (*realtime_callback)(void *data, int signal);
    void *realtime_callback_data;
    /*! TRUE if dialtone should be filtered before processing */
    int filter_dialtone;
    float normal_twist;
    float reverse_twist;

    /*! The state of the dialtone filter */    
    float z350_1;
    float z350_2;
    float z440_1;
    float z440_2;

    /*! Tone detector working states */
    goertzel_state_t row_out[4];
    goertzel_state_t col_out[4];
    float energy;
    int hits[3];
    int in_digit;
    int current_sample;

    /*! Received digit buffer */
    char digits[MAX_DTMF_DIGITS + 1];
    int current_digits;
    int lost_digits;
} dtmf_rx_state_t;

/*!
    Bell MF digit detector descriptor.
*/
typedef struct
{
    void (*callback)(void *data, const char *digits, int len);
    void *callback_data;
    /*! Tone detector working states */
    goertzel_state_t out[6];
    int hits[5];
    int current_sample;

    char digits[MAX_DTMF_DIGITS + 1];
    int current_digits;
    int lost_digits;
} bell_mf_rx_state_t;

/*!
    MFC/R2 tone detector descriptor.
*/
typedef struct
{
    /*! Tone detector working states */
    goertzel_state_t out[6];
    int hits[3];
    int fwd;
    int samples;
    int current_sample;
} r2_mf_rx_state_t;

#ifdef __cplusplus
extern "C" {
#endif

/*! \brief Create a descriptor for use with either a Goertzel transform */
void make_goertzel_descriptor(goertzel_descriptor_t *t,
                              int freq,
                              int samples);

/*! \brief Initialise the state of a Goertzel transform.
    \param s The Goertzel state. If NULL, a state is allocated with malloc.
    \param t The Goertzel descriptor.
    \return A pointer to the Goertzel state. */
goertzel_state_t *goertzel_init(goertzel_state_t *s,
                                goertzel_descriptor_t *t);

/*! \brief Update the state of a Goertzel transform.
    \param s The Goertzel state
    \param amp The samples to be transformed
    \param samples The number of samples
    \return The number of samples unprocessed */
int goertzel_update(goertzel_state_t *s,
                    const int16_t amp[],
                    int samples);

/*! \brief Evaluate the final result of a Goertzel transform.
    \param s The Goertzel state
    \return The result of the transform. */
float goertzel_result(goertzel_state_t *s);

/*! \brief Initialise a DTMF receiver context.
    \param s The DTMF receiver context.
    \param callback Callback routine used to report received digits.
    \param user_data An opaque pointer which is associated with the context,
           and supplied in callbacks.
    \return A pointer to the DTMF receiver context. */
dtmf_rx_state_t *dtmf_rx_init(dtmf_rx_state_t *s,
                              void (*callback)(void *user_data, const char *digits, int len),
                              void *user_data);

/*! \brief Set a realtime callback for a DTMF receiver context.
    \param s The DTMF receiver context.
    \param callback Callback routine used to report the start and end of digits.
    \param user_data An opaque pointer which is associated with the context,
           and supplied in callbacks. */
void dtmf_rx_set_realtime_callback(dtmf_rx_state_t *s,
                                   void (*callback)(void *user_data, int signal),
                                   void *user_data);

/*! \brief Adjust a DTMF receiver context.
    \param s The DTMF receiver context.
    \param filter_dialtone TRUE to enable filtering of dialtone, FALSE
           to disable, < 0 to leave unchanged.
    \param twist Acceptable twist, in dB. < 0 to leave unchanged.
    \param reverse_twist Acceptable reverse twist, in dB. < 0 to leave unchanged. */
void dtmf_rx_parms(dtmf_rx_state_t *s, int filter_dialtone, int twist, int reverse_twist);

/*! Process a block of received DTMF audio samples.
    \brief Process a block of received DTMF audio samples.
    \param s The DTMF receiver context.
    \param amp The audio sample buffer.
    \param samples The number of samples in the buffer.
    \return The number of samples unprocessed. */
int dtmf_rx(dtmf_rx_state_t *s, const int16_t *amp, int samples);

int dtmf_get(dtmf_rx_state_t *s, char *buf, int max);

/*! \brief Initialise a Bell MF receiver context.
    \param s The Bell MF receiver context.
    \param callback Callback routine used to report received digits.
    \param user_data An opaque pointer which is associated with the context,
           and supplied in callbacks.
    \return A pointer to the Bell MF receiver context.*/
bell_mf_rx_state_t *bell_mf_rx_init(bell_mf_rx_state_t *s,
                                    void (*callback)(void *user_data, const char *digits, int len),
                                    void *user_data);

/*! Process a block of received bell MF audio samples.
    \brief Process a block of received Bell MF audio samples.
    \param s The Bell MF receiver context.
    \param amp The audio sample buffer.
    \param samples The number of samples in the buffer.
    \return The number of samples unprocessed. */
int bell_mf_rx(bell_mf_rx_state_t *s, const int16_t *amp, int samples);

int bell_mf_get(bell_mf_rx_state_t *s, char *buf, int max);

/*! \brief Initialise an R2 MF receiver context.
    \param s The R2 MF receiver context.
    \param fwd TRUE if the context is for forward signals. FALSE if the
           context is for backward signals.
    \return A pointer to the R2 MF receiver context. */
r2_mf_rx_state_t *r2_mf_rx_init(r2_mf_rx_state_t *s, int fwd);

/*! Process a block of received R2 MF audio samples.
    \brief Process a block of received R2 MF audio samples.
    \param s The R2 MF receiver context.
    \param amp The audio sample buffer.
    \param samples The number of samples in the buffer.
    \return The number of samples unprocessed. */
int r2_mf_rx(r2_mf_rx_state_t *s, const int16_t *amp, int samples);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

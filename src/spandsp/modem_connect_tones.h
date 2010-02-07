/*
 * SpanDSP - a series of DSP components for telephony
 *
 * modem_connect_tones.c - Generation and detection of tones
 * associated with modems calling and answering calls.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: modem_connect_tones.h,v 1.14 2008/04/02 12:41:19 steveu Exp $
 */
 
/*! \file */

#if !defined(_SPANDSP_MODEM_CONNECT_TONES_H_)
#define _SPANDSP_MODEM_CONNECT_TONES_H_

/*! \page modem_connect_tones_page Modem connect tone detection

\section modem_connect_tones_page_sec_1 What does it do?
Some telephony terminal equipment, such as modems, require a channel which is as
clear as possible. They use their own echo cancellation. If the network is also
performing echo cancellation the two cancellors can end up squabbling about the
nature of the channel, with bad results. A special tone is defined which should
cause the network to disable any echo cancellation processes. This is the echo
canceller disable tone.

The tone detector's design assumes the channel is free of any DC component.

\section modem_connect_tones_page_sec_2 How does it work?
A sharp notch filter is implemented as a single bi-quad section. The presence of
the 2100Hz disable tone is detected by comparing the notched filtered energy
with the unfiltered energy. If the notch filtered energy is much lower than the
unfiltered energy, then a large proportion of the energy must be at the notch
frequency. This type of detector may seem less intuitive than using a narrow
bandpass filter to isolate the energy at the notch freqency. However, a sharp
bandpass implemented as an IIR filter rings badly. The reciprocal notch filter
is very well behaved. 
*/

enum
{
    MODEM_CONNECT_TONES_FAX_CNG,
    MODEM_CONNECT_TONES_FAX_CED,
    MODEM_CONNECT_TONES_EC_DISABLE,
    /*! \brief The version of EC disable with some 15Hz AM content, as in V.8 */
    MODEM_CONNECT_TONES_EC_DISABLE_MOD,
};

/*!
    Modem connect tones generator descriptor. This defines the state
    of a single working instance of the tone generator.
*/
typedef struct
{
    int tone_type;
    
    tone_gen_state_t tone_tx;
    uint32_t tone_phase;
    int32_t tone_phase_rate;
    int level;
    /*! \brief Countdown to the next phase hop */
    int hop_timer;
    uint32_t mod_phase;
    int32_t mod_phase_rate;
    int mod_level;
} modem_connect_tones_tx_state_t;

/*!
    Modem connect tones receiver descriptor. This defines the state
    of a single working instance of the tone detector.
*/
typedef struct
{
    /*! \brief The tone type being detected. */
    int tone_type;
    /*! \brief Callback routine, using to report detection of the tone. */
    tone_report_func_t tone_callback;
    /*! \brief An opaque pointer passed to tone_callback. */
    void *callback_data;

    /*! \brief The notch filter state. */
    float z1;
    float z2;
    /*! \brief The in notch power estimate */
    int notch_level;
    /*! \brief The total channel power estimate */
    int channel_level;
    /*! \brief TRUE is the tone is currently confirmed present in the audio. */
    int tone_present;
    int tone_on;
    /*! \brief A sample counter, to time the duration of tone sections. */
    int tone_cycle_duration;
    /*! \brief A count of the number of good cycles of tone reversal seen. */
    int good_cycles;
    /*! \brief TRUE if the tone has been seen since the last time the user tested for it */
    int hit;
    /*! \brief A V.21 FSK modem context used when searching for FAX preamble. */
    fsk_rx_state_t v21rx;
    /*! \brief The raw (stuffed) bit stream buffer. */
    unsigned int raw_bit_stream;
    /*! \brief The current number of bits in the octet in progress. */
    int num_bits;
    /*! \brief Number of consecutive flags seen so far. */
    int flags_seen;
    /*! \brief TRUE if framing OK has been announced. */
    int framing_ok_announced;
} modem_connect_tones_rx_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/*! \brief Initialise an instance of the modem connect tones generator.
    \param s The context.
*/
modem_connect_tones_tx_state_t *modem_connect_tones_tx_init(modem_connect_tones_tx_state_t *s,
                                                            int tone_type);

/*! \brief Free an instance of the modem connect tones generator.
    \param s The context.
    \return 0 for OK, else -1.
*/
int modem_connect_tones_tx_free(modem_connect_tones_tx_state_t *s);

/*! \brief Generate a block of modem connect tones samples.
    \param s The context.
    \param amp An array of signal samples.
    \param len The number of samples to generate.
    \return The number of samples generated.
*/
int modem_connect_tones_tx(modem_connect_tones_tx_state_t *s,
                           int16_t amp[],
                           int len);

/*! \brief Process a block of samples through an instance of the modem connect
           tones detector.
    \param s The context.
    \param amp An array of signal samples.
    \param len The number of samples in the array.
    \return The number of unprocessed samples.
*/
int modem_connect_tones_rx(modem_connect_tones_rx_state_t *s,
                           const int16_t amp[],
                           int len);
                             
/*! \brief Test if a modem_connect tone has been detected.
    \param s The context.
    \return TRUE if tone is detected, else FALSE.
*/
int modem_connect_tones_rx_get(modem_connect_tones_rx_state_t *s);

/*! \brief Initialise an instance of the modem connect tones detector.
    \param s The context.
    \param tone_type The type of connect tone being tested for.
    \param tone_callback An optional callback routine, used to report tones
    \param user_data An opaque pointer passed to the callback routine,
    \return A pointer to the context.
*/
modem_connect_tones_rx_state_t *modem_connect_tones_rx_init(modem_connect_tones_rx_state_t *s,
                                                            int tone_type,
                                                            tone_report_func_t tone_callback,
                                                            void *user_data);

/*! \brief Free an instance of the modem connect tones detector.
    \param s The context.
    \return 0 for OK, else -1. */
int modem_connect_tones_rx_free(modem_connect_tones_rx_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

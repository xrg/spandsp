/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v22bis.h - ITU V.22bis modem receive part
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
 * $Id: v22bis.h,v 1.4 2005/01/12 13:39:26 steveu Exp $
 */

/*! \file */

#if !defined(_V22BIS_H_)
#define _V22BIS_H_

#include "fsk.h"

/*! \page V22bis_page The V.22bis modem
\section V22bis_page_sec_1 What does it do
The V.22bis modem is a duplex modem for general data use on the PSTN, at rates
of 1200 and 2400 bits/second. It is a compatible extension of the V.22 modem,
which is a 1200 bits/second only design. It is a band-split design, using carriers
of 1200Hz and 2400Hz. It is the fastest PSTN modem in general use which does not
use echo-cancellation.

\section V22bis__page_sec_2 Theory of Operation
V.22bis uses 4PSK modulation at 1200 bits/second or 16QAM modulation at 2400
bits/second. At 1200bps the symbols are so long that a fixed compromise equaliser
is sufficient to recover the 4PSK signal reliably. At 2400bps an adaptive
equaliser is necessary.

The V.22bis training sequence includes sections which allow the modems to determine
if the far modem can support (or is willing to support) 2400bps operation. The
modems will automatically use 2400bps if both ends are willing to use that speed,
or 1200bps if one or both ends to not acknowledge that 2400bps is OK.
*/

#define V22BIS_EQUALIZER_LEN   7  /* this much to the left and this much to the right */
#define V22BIS_EQUALIZER_MASK  15 /* one less than a power of 2 >= (2*V22BIS_EQUALIZER_LEN + 1) */

#define V22BIS_RX_FILTER_STEPS 107
#define V22BIS_TX_FILTER_STEPS 107

/*!
    V.22bis modem receive side descriptor. This defines the working state for a
    single instance of a V.22bis modem receiver.
*/
typedef struct
{
    /*! \brief The bit rate of the modem. Valid values are 1200 and 2400. */
    int bit_rate;
    /*! \brief TRUE is this is the calling side modem. */
    int caller;
    /*! \brief The callback function used to put each bit received. */
    put_bit_func_t put_bit;
    /*! \brief The callback function used to get the next bit to be transmitted. */
    get_bit_func_t get_bit;
    /*! \brief A user specified opaque pointer passed to the callback functions. */
    void *user_data;

    /* RECEIVE SECTION */

    /*! \brief A callback function which may be enabled to report every symbol's
               constellation position. */
    qam_report_handler_t *qam_report;
    /*! \brief A user specified opaque pointer passed to the qam_report callback
               function. */
    void *qam_user_data;

    /*! \brief The route raised cosine (RRC) pulse shaping filter buffer. */
    complex_t rx_rrc_filter[2*V22BIS_RX_FILTER_STEPS];
    /*! \brief Current offset into the RRC pulse shaping filter buffer. */
    int rx_rrc_filter_step;

    /*! \brief The register for the data scrambler. */
    unsigned int rx_scramble_reg;
    /*! \brief A counter for the number of consecutive bits of repeating pattern through
               the scrambler. */
    int rx_scrambler_pattern_count;
    /*! \brief 0 if receiving user data. A training stage value during training */
    int rx_training;
    int rx_training_count;
    float training_error;
    int carrier_present;

    /*! \brief The current phase of the carrier (i.e. the DDS parameter). */
    uint32_t rx_carrier_phase;
    /*! \brief The update rate for the phase of the carrier (i.e. the DDS increment). */
    int32_t rx_carrier_phase_rate;
    float carrier_track_p;
    float carrier_track_i;

    power_meter_t rx_power;
    int32_t carrier_on_power;
    int32_t carrier_off_power;
    float agc_scaling;
    
    int rx_constellation_state;

    float eq_delta;
    /*! \brief The adaptive equalizer coefficients */
    complex_t eq_coeff[2*V22BIS_EQUALIZER_LEN + 1];
    complex_t eq_buf[V22BIS_EQUALIZER_MASK + 1];
    /*! \brief Current offset into equalizer buffer. */
    int eq_step;
    int eq_put_step;

    /*! \brief Integration variable for damping the Gardner algorithm tests. */
    int gardner_integrate;
    /*! \brief Current step size of Gardner algorithm integration. */
    int gardner_step;
    /*! \brief The total gardner timing correction, since the carrier came up.
               This is only for performance analysis purposes. */
    int gardner_total_correction;
    /*! \brief The current fractional phase of the baud timing. */
    int rx_baud_phase;

    /*! \brief Starting phase angles for the coarse carrier aquisition step. */
    int32_t start_angles[4];
    /*! \brief History list of phase angles for the coarse carrier aquisition step. */
    int32_t angles[16];

    /* TRANSMIT SECTION */

    float tx_gain;

    /*! \brief The route raised cosine (RRC) pulse shaping filter buffer. */
    complex_t tx_rrc_filter[2*V22BIS_TX_FILTER_STEPS];
    /*! \brief Current offset into the RRC pulse shaping filter buffer. */
    int tx_rrc_filter_step;
    /*! \brief The current constellation position. */
    complex_t current_point;

    /*! \brief The register for the data scrambler. */
    unsigned int tx_scramble_reg;
    /*! \brief A counter for the number of consecutive bits of repeating pattern through
               the scrambler. */
    int tx_scrambler_pattern_count;
    /*! \brief 0 if transmitting user data. A training stage value during training */
    int tx_training;
    /*! \brief A counter used to track progress through sending the training sequence. */
    int tx_training_count;

    /*! \brief The current phase of the carrier (i.e. the DDS parameter). */
    uint32_t tx_carrier_phase;
    /*! \brief The update rate for the phase of the carrier (i.e. the DDS increment). */
    int32_t tx_carrier_phase_rate;
    /*! \brief The current phase of the guard tone (i.e. the DDS parameter). */
    uint32_t guard_phase;
    /*! \brief The update rate for the phase of the guard tone (i.e. the DDS increment). */
    int32_t guard_phase_rate;
    int guard_level;
    /*! \brief The current fractional phase of the baud timing. */
    int tx_baud_phase;
    /*! \brief The code number for the current position in the constellation. */
    int tx_constellation_state;
    /*! \brief An indicator to mark that we are tidying up to stop transmission. */
    int shutdown;
    /*! \brief The get_bit function in use at any instant. */
    get_bit_func_t current_get_bit;
    
    int detected_unscrambled_ones;
    int detected_unscrambled_0011_ending;
    int detected_scrambled_ones_or_zeros_at_1200bps;
    int detected_scrambled_ones_at_2400bps;
} v22bis_state_t;

extern const complex_t v22bis_constellation[16];

#ifdef __cplusplus
extern "C" {
#endif

/*! Reinitialise an existing V.22bis modem receive context.
    \brief Reinitialise an existing V.22bis modem receive context.
    \param s The modem context.
    \param rate The bit rate of the modem. Valid values are 1200 and 2400.
    \return 0 for OK, -1 for bad parameter */
int v22bis_rx_restart(v22bis_state_t *s, int bit_rate);

/*! Process a block of received V.22bis modem audio samples.
    \brief Process a block of received V.22bis modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of samples unprocessed.
*/
int v22bis_rx(v22bis_state_t *s, const int16_t *amp, int len);

/*! Get a snapshot of the current equalizer coefficients.
    \brief Get a snapshot of the current equalizer coefficients.
    \param coeffs The vector of complex coefficients.
    \return The number of coefficients in the vector. */
int v22bis_rx_equalizer_state(v22bis_state_t *s, complex_t **coeffs);

/*! Get a current received carrier frequency.
    \param s The modem context.
    \return The frequency, in Hertz. */
float v22bis_rx_carrier_frequency(v22bis_state_t *s);

float v22bis_rx_symbol_timing_correction(v22bis_state_t *s);

/*! Get a current received signal power.
    \param s The modem context.
    \return The signal power, in dBm0. */
float v22bis_rx_signal_power(v22bis_state_t *s);

void v22bis_rx_set_qam_report_handler(v22bis_state_t *s, qam_report_handler_t *handler, void *user_data);

/*! Generate a block of V.22bis modem audio samples.
    \brief Generate a block of V.22bis modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples to be generated.
    \return The number of samples actually generated.
*/
int v22bis_tx(v22bis_state_t *s, int16_t *amp, int len);

void v22bis_tx_power(v22bis_state_t *s, float power);

/*! Reinitialise an existing V.22bis modem context, so it may be reused.
    \brief Reinitialise an existing V.22bis modem context.
    \param s The modem context.
    \param bit_rate The bit rate of the modem. Valid values are 1200 and 2400.
    \return 0 for OK, -1 for bad parameter */
int v22bis_restart(v22bis_state_t *s, int bit_rate);

/*! Initialise a V.22bis modem context. This must be called before the first
    use of the context, to initialise its contents.
    \brief Initialise a V.22bis modem context.
    \param s The modem context.
    \param bit_rate The bit rate of the modem. Valid values are 1200 and 2400.
    \param guard The guard tone option. 0 = none, 1 = 550Hz, 2 = 1800Hz.
    \param caller TRUE if this is the calling modem.
    \param get_bit The callback routine used to get the data to be transmitted.
    \param put_bit The callback routine used to get the data to be transmitted.
    \param user_data An opaque pointer, passed in calls to the get and put routines. */
void v22bis_init(v22bis_state_t *s, int bit_rate, int guard, int caller, get_bit_func_t get_bit, put_bit_func_t put_bit, void *user_data);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

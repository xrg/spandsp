/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v27ter_rx.h - ITU V.29 modem receive part
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
 * $Id: v27ter_rx.h,v 1.2 2004/03/15 13:17:36 steveu Exp $
 */

/*! \file */

#if !defined(_V27TER_RX_H_)
#define _V27TER_RX_H_

#include "fsk.h"

#define V27_EQUALIZER_LEN   7  /* this much to the left and this much to the right */
#define V27_EQUALIZER_MASK  15 /* one less than a power of 2 >= (2*V27_EQUALIZER_LEN + 1) */

#define V27RX_4800_FILTER_STEPS  29
#define V27RX_2400_FILTER_STEPS  29

#if V27RX_4800_FILTER_STEPS > V27RX_2400_FILTER_STEPS
#define V27RX_FILTER_STEPS V27RX_4800_FILTER_STEPS
#else
#define V27RX_FILTER_STEPS V27RX_2400_FILTER_STEPS
#endif

/*!
    V.27ter modem receive side descriptor. This defines the working state for a
    single instance of a V.27ter modem receiver.
*/
typedef struct
{
    /*! \brief The bit rate of the modem. Valid values are 2400 and 4800. */
    int bit_rate;
    /*! \brief The callback function used to put each bit received. */
    put_bit_func_t put_bit;
    /*! \brief A user specified opaque pointer passed to the callback function. */
    void *user_data;

    /*! \brief The route raised cosine (RRC) pulse shaping filter buffer. */
    complex_t rrc_filter[2*V27RX_FILTER_STEPS];
    /*! \brief Current offset into the RRC pulse shaping filter buffer. */
    int rrc_filter_step;

    /*! \brief The register for the training and data scrambler. */
    unsigned int scramble_reg;
    /*! \brief A counter for the number of consecutive bits of repeating pattern through
              the scrambler. */
    int scrambler_pattern_count;
    int in_training;
    int training_bc;
    int training_count;
    int training_test_ones;
    int carrier_present;

    /*! \brief The current phase of the carrier (i.e. the DDS parameter). */
    uint32_t carrier_phase;
    /*! \brief The update rate for the phase of the carrier (i.e. the DDS increment). */
    int32_t carrier_phase_rate;

    power_meter_t power;
    int32_t carrier_on_power;
    int32_t carrier_off_power;
    float agc_scaling;
    
    int constellation_state;

    float eq_delta;
    /*! \brief The adaptive equalizer coefficients */
    complex_t eq_coeff[2*V27_EQUALIZER_LEN + 1];
    complex_t eq_buf[V27_EQUALIZER_MASK + 1];
    /*! \brief Current offset into equalizer buffer. */
    int eq_step;
    int eq_put_step;

    /*! \brief Integration variable for damping the Gardner algorithm tests. */
    int gardner_integrate;
    int gardner_step;
    /*! \brief The current fractional phase of the baud timing. */
    int baud_phase;
    /*! \brief The integrated lead or lag of the carrier phase against its expected
               value. This is used in fine carrier tracking. */
    int lead_lag;
    /*! \brief The number of bauds over which lead_lag has been gathered. */
    int lead_lag_time;

    /*! \brief A starting phase angle for the coarse carrier aquisition step. */
    int32_t start_angle_a;
    /*! \brief A final phase angle for the coarse carrier aquisition step. */
    int32_t last_angle_a;
    /*! \brief A starting phase angle for the coarse carrier aquisition step. */
    int32_t start_angle_b;
    /*! \brief A final phase angle for the coarse carrier aquisition step. */
    int32_t last_angle_b;

    FILE *qam_log;
} v27ter_rx_state_t;

/*! Initialise a V.27ter modem receive context.
    \brief Initialise a V.27ter modem receive context.
    \param s The modem context.
    \param rate The bit rate of the modem. Valid values are 2400 and 4800.
    \param put_bit The callback routine used to put the received data.
    \param user_data An opaque pointer. */
void v27ter_rx_init(v27ter_rx_state_t *s, int bit_rate, put_bit_func_t put_bit, void *user_data);

/*! Reinitialise an existing V.27ter modem receive context.
    \brief Reinitialise an existing V.27ter modem receive context.
    \param s The modem context.
    \param rate The bit rate of the modem. Valid values are 2400 and 4800. */
void v27ter_rx_restart(v27ter_rx_state_t *s, int bit_rate);

/*! Process a block of received V.27ter modem audio samples.
    \brief Process a block of received V.27ter modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
*/
void v27ter_rx(v27ter_rx_state_t *s, const int16_t *amp, int len);

#endif
/*- End of file ------------------------------------------------------------*/

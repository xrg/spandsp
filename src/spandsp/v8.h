/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v8.h - V.8 modem negotiation processing.
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
 * $Id: v8.h,v 1.8 2005/12/25 17:33:37 steveu Exp $
 */
 
/*! \file */

/*! \page v8_page The V.8 modem negotiation protocol
\section v8_page_sec_1 What does it do?
The V.8 specification defines a procedure to be used as PSTN modem answer phone calls,
which allows the modems to negotiate the optimum modem standard, which both ends can
support.

\section v8_page_sec_2 How does it work?
At startup the modems communicate using the V.21 standard at 300 bits/second. They
exchange simple messages about their capabilities, and choose the modem standard they
will use for data communication. The V.8 protocol then terminates, and the modems
being negotiating and training with their chosen modem standard.
*/

#if !defined(_V8_H_)
#define _V8_H_

typedef void (v8_result_handler_t)(void *user_data, int result);

typedef struct
{
    /*! \brief TRUE if we are the calling modem */
    int caller;
    /*! \brief The current state of the V8 protocol */
    int state;
    int negotiation_timer;
    int ci_timer;
    int ci_count;
    fsk_tx_state_t v21tx;
    fsk_rx_state_t v21rx;
    queue_t tx_queue;
    echo_can_disable_tx_state_t v8_tx;
    echo_can_disable_rx_state_t v8_rx;

    v8_result_handler_t *result_handler;
    void *result_handler_user_data;

    /*! \brief Modulation schemes available at this end. */
    int available_modulations;
    int common_modulations;
    int selected_modulation;
    int far_end_modulations;

    /* V8 data parsing */
    unsigned int bit_stream;
    int bit_cnt;
    /* Indicates the type of message coming up */
    int preamble_type;
    uint8_t rx_data[64];
    int rx_data_ptr;
    
    /*! \brief a reference copy of the last CM or JM message, used when
               testing for matches. */
    uint8_t cm_jm_data[64];
    int cm_jm_count;
    int got_cm_jm;
    int got_cj;
    int zero_byte_count;
    /*! \brief Error and flow logging control */
    logging_state_t logging;
} v8_state_t;

#define V8_MOD_V17          (1 << 0)    /* V.17 duplex */
#define V8_MOD_V21          (1 << 1)    /* V.21 duplex */
#define V8_MOD_V22          (1 << 2)    /* V.22/V22.bis duplex */
#define V8_MOD_V23HALF      (1 << 3)    /* V.23 half-duplex */
#define V8_MOD_V23          (1 << 4)    /* V.23 duplex */
#define V8_MOD_V26BIS       (1 << 5)    /* V.23 duplex */
#define V8_MOD_V26TER       (1 << 6)    /* V.23 duplex */
#define V8_MOD_V27TER       (1 << 7)    /* V.23 duplex */
#define V8_MOD_V29          (1 << 8)    /* V.29 half-duplex */
#define V8_MOD_V32          (1 << 9)    /* V.32/V32.bis duplex */
#define V8_MOD_V34HALF      (1 << 10)   /* V.34 half-duplex */
#define V8_MOD_V34          (1 << 11)   /* V.34 duplex */
#define V8_MOD_V90          (1 << 12)   /* V.90 duplex */
#define V8_MOD_V92          (1 << 13)   /* V.92 duplex */

#define V8_MOD_FAILED       (1 << 15)   /* Indicates failure to negotiate */

#ifdef __cplusplus
extern "C" {
#endif

/*! Initialise a V.8 context.
    \brief Initialise a V.8 context.
    \param s The V.8 context.
    \param caller TRUE if caller mode, else answerer mode.
    \param available_modulations A bitwise list of the modulation schemes to be
           advertised as available here.
    \param result_handler The callback routine used to handle the results of negotiation.
    \param user_data An opaque pointer passed to the result_handler routine.
    \return A pointer to the V.8 context, or NULL if there was a problem. */
v8_state_t *v8_init(v8_state_t *s,
                    int caller,
                    int available_modulations,
                    v8_result_handler_t *result_handler,
                    void *user_data);

/*! Release a V.8 context.
    \brief Release a V.8 context.
    \param s The V.8 context.
    \return 0 for OK. */
int v8_release(v8_state_t *s);

/*! Generate a block of V.8 audio samples.
    \brief Generate a block of V.8 audio samples.
    \param s The V.8 context.
    \param amp The audio sample buffer.
    \param max_len The number of samples to be generated.
    \return The number of samples actually generated.
*/
int v8_tx(v8_state_t *s, int16_t *amp, int max_len);

/*! Process a block of received V.8 audio samples.
    \brief Process a block of received V.8 audio samples.
    \param s The V.8 context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
*/
int v8_rx(v8_state_t *s, const int16_t *amp, int len);

/*! Log the list of supported modulations.
    \brief Log the list of supported modulations.
    \param s The V.8 context.
    \param modulation_schemes The list of supported modulations. */
void v8_log_supported_modulations(v8_state_t *s, int modulation_schemes);

/*! Log the selected modulation.
    \brief Log the selected modulation.
    \param s The V.8 context.
    \param modulation_scheme The selected modulation. */
void v8_log_selected_modulation(v8_state_t *s, int modulation_scheme);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fax_tester.h
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2005, 2006 Steve Underwood
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
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: fax_tester.h,v 1.4 2008/07/21 12:59:48 steveu Exp $
 */

/*! \file */

#if !defined(_SPANDSP_FAX_TESTER_H_)
#define _SPANDSP_FAX_TESTER_H_

/*! \page fax_tester_page FAX over analogue modem handling

\section fax_tester_page_sec_1 What does it do?

\section fax_tester_page_sec_2 How does it work?
*/

typedef struct faxtester_state_s faxtester_state_t;

typedef void (faxtester_flush_handler_t)(faxtester_state_t *s, void *user_data, int which);

/*!
    FAX tester real time frame handler.
    \brief FAX tester real time frame handler.
    \param s The FAX tester context.
    \param user_data An opaque pointer.
    \param direction TRUE for incoming, FALSE for outgoing.
    \param msg The HDLC message.
    \param len The length of the message.
*/
typedef void (faxtester_real_time_frame_handler_t)(faxtester_state_t *s,
                                                   void *user_data,
                                                   int direction,
                                                   const uint8_t *msg,
                                                   int len);

typedef void (faxtester_front_end_step_complete_handler_t)(faxtester_state_t *s, void *user_data);

/*!
    FAX tester descriptor.
*/
struct faxtester_state_s
{
    /*! \brief Pointer to our current step in the test. */
    xmlNodePtr cur;

    /*! TRUE is talker echo protection should be sent for the image modems */
    int use_tep;

    faxtester_flush_handler_t *flush_handler;
    void *flush_user_data;

    /*! \brief A pointer to a callback routine to be called when frames are
        exchanged. */
    faxtester_real_time_frame_handler_t *real_time_frame_handler;
    /*! \brief An opaque pointer supplied in real time frame callbacks. */
    void *real_time_frame_user_data;

    faxtester_front_end_step_complete_handler_t *front_end_step_complete_handler;
    void *front_end_step_complete_user_data;

    /*! The current receive signal handler */
    span_rx_handler_t *rx_handler;
    void *rx_user_data;
    int rx_trained;

    /*! The current transmit signal handler */
    span_tx_handler_t *tx_handler;
    void *tx_user_data;
    
    const uint8_t *image_buffer;
    int image_len;
    int image_ptr;
    int image_bit_ptr;
    
    int final_delayed;

    /*! If TRUE, transmission is in progress */
    int transmit;

    /*! If TRUE, transmit silence when there is nothing else to transmit. If FALSE return only
        the actual generated audio. Note that this only affects untimed silences. Timed silences
        (e.g. the 75ms silence between V.21 and a high speed modem) will alway be transmitted as
        silent audio. */
    int transmit_on_idle;

    /*! \brief A tone generator context used to generate supervisory tones during
               FAX handling. */
    tone_gen_state_t tone_gen;
    /*! \brief An HDLC context used when receiving HDLC over V.21 messages. */
    hdlc_rx_state_t hdlcrx;
    /*! \brief An HDLC context used when transmitting HDLC over V.21 messages. */
    hdlc_tx_state_t hdlctx;
    /*! \brief A V.21 FSK modem context used when transmitting HDLC over V.21
               messages. */
    fsk_tx_state_t v21_tx;
    /*! \brief A V.21 FSK modem context used when receiving HDLC over V.21
               messages. */
    fsk_rx_state_t v21_rx;
    /*! \brief A V.17 modem context used when sending FAXes at 7200bps, 9600bps
               12000bps or 14400bps*/
    v17_tx_state_t v17_tx;
    /*! \brief A V.29 modem context used when receiving FAXes at 7200bps, 9600bps
               12000bps or 14400bps*/
    v17_rx_state_t v17_rx;
    /*! \brief A V.27ter modem context used when sending FAXes at 2400bps or
               4800bps */
    v27ter_tx_state_t v27ter_tx;
    /*! \brief A V.27ter modem context used when receiving FAXes at 2400bps or
               4800bps */
    v27ter_rx_state_t v27ter_rx;
    /*! \brief A V.29 modem context used when sending FAXes at 7200bps or
               9600bps */
    v29_tx_state_t v29_tx;
    /*! \brief A V.29 modem context used when receiving FAXes at 7200bps or
               9600bps */
    v29_rx_state_t v29_rx;
    /*! \brief Used to insert timed silences. */
    silence_gen_state_t silence_gen;
    /*! \brief */
    dc_restore_state_t dc_restore;

    /*! \brief TRUE is the short training sequence should be used. */
    int short_train;

    /*! \brief The currently select receiver type */
    int current_rx_type;
    /*! \brief The currently select transmitter type */
    int current_tx_type;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#if defined(__cplusplus)
extern "C"
{
#endif

/*! Apply T.30 receive processing to a block of audio samples.
    \brief Apply T.30 receive processing to a block of audio samples.
    \param s The FAX tester context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of samples unprocessed. This should only be non-zero if
            the software has reached the end of the FAX call.
*/
int faxtester_rx(faxtester_state_t *s, int16_t *amp, int len);

/*! Apply T.30 transmit processing to generate a block of audio samples.
    \brief Apply T.30 transmit processing to generate a block of audio samples.
    \param s The FAX tester context.
    \param amp The audio sample buffer.
    \param max_len The number of samples to be generated.
    \return The number of samples actually generated. This will be zero when
            there is nothing to send.
*/
int faxtester_tx(faxtester_state_t *s, int16_t *amp, int max_len);

void faxtester_set_tx_type(void *user_data, int type, int short_train, int use_hdlc);

void faxtester_set_rx_type(void *user_data, int type, int short_train, int use_hdlc);

void faxtester_send_hdlc_flags(faxtester_state_t *s, int flags);

void faxtester_send_hdlc_msg(faxtester_state_t *s, const uint8_t *msg, int len);

void faxtester_set_flush_handler(faxtester_state_t *s, faxtester_flush_handler_t *handler, void *user_data);

/*! Select whether silent audio will be sent when FAX transmit is idle.
    \brief Select whether silent audio will be sent when FAX transmit is idle.
    \param s The FAX tester context.
    \param transmit_on_idle TRUE if silent audio should be output when the FAX transmitter is
           idle. FALSE to transmit zero length audio when the FAX transmitter is idle. The default
           behaviour is FALSE.
*/
void faxtester_set_transmit_on_idle(faxtester_state_t *s, int transmit_on_idle);

/*! Select whether talker echo protection tone will be sent for the image modems.
    \brief Select whether TEP will be sent for the image modems.
    \param s The FAX tester context.
    \param use_tep TRUE if TEP should be sent.
*/
void faxtester_set_tep_mode(faxtester_state_t *s, int use_tep);

void faxtester_set_real_time_frame_handler(faxtester_state_t *s, faxtester_real_time_frame_handler_t *handler, void *user_data);

void faxtester_set_front_end_step_complete_handler(faxtester_state_t *s, faxtester_front_end_step_complete_handler_t *handler, void *user_data);

void faxtester_set_image_buffer(faxtester_state_t *s, const uint8_t *buf, int len);

/*! Initialise a FAX context.
    \brief Initialise a FAX context.
    \param s The FAX tester context.
    \param calling_party TRUE if the context is for a calling party. FALSE if the
           context is for an answering party.
    \return A pointer to the FAX context, or NULL if there was a problem.
*/
faxtester_state_t *faxtester_init(faxtester_state_t *s, int calling_party);

/*! Release a FAX context.
    \brief Release a FAX context.
    \param s The FAX tester context.
    \return 0 for OK, else -1. */
int faxtester_release(faxtester_state_t *s);

/*! Free a FAX context.
    \brief Free a FAX context.
    \param s The FAX tester context.
    \return 0 for OK, else -1. */
int faxtester_free(faxtester_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

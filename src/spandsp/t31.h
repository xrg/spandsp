/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t31.h - A T.31 compatible class 1 FAX modem interface.
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
 * $Id: t31.h,v 1.9 2004/11/21 13:55:05 steveu Exp $
 */

/*! \file */

#if !defined(_T31_H_)
#define _T31_H_

/*! \page T31_page T.31 CLass 1 FAX modem protocol handling
*/

typedef struct t31_state_s t31_state_t;

typedef int (t31_call_control_handler_t)(t31_state_t *s, void *user_data, const char *num);
typedef int (t31_at_tx_handler_t)(t31_state_t *s, void *user_data, const uint8_t *buf, int len);

enum t31_rx_mode_e
{
    AT_MODE_COMMAND,
    AT_MODE_CONNECTED,
    AT_MODE_HDLC,
    AT_MODE_STUFFED
};

enum t31_call_event_e
{
    T31_CALL_EVENT_ALERTING = 1,
    T31_CALL_EVENT_CONNECTED,
    T31_CALL_EVENT_ANSWERED,
    T31_CALL_EVENT_BUSY,
    T31_CALL_EVENT_NO_DIALTONE,
    T31_CALL_EVENT_NO_ANSWER
};

typedef struct
{
    int echo;
    int verbose;
    int result_code_format;
    int pulse_dial;
    int double_escape;
    uint8_t s_regs[100];
} t31_profile_t;

struct t31_state_s
{
    int country_of_installation;
    char line[256];
    char hdlc_buf[256];
    int hdlc_len;
    /*! TRUE if DLE prefix just used */
    int dled;
    int line_ptr;
    int at_rx_mode;
    /*! This is no real DTE rate. This variable is for compatibility this serially
        connected modems. */
    int dte_rate;
    int dte_char_format;
    int dte_parity;
    /*! The currently select FAX modem class. 0 = data modem mode. */
    int fclass_mode;
    char *originating_number;
    char *destination_number;
    t31_profile_t p;
    uint8_t rx_data[256];
    int rx_data_bytes;
    uint8_t tx_data[200000]; //[256];
    int tx_in_bytes;
    int tx_data_bytes;
    int bit_no;
    int current_byte;

    /*! \brief The current bit rate for the fast message transfer modem. */
    int bit_rate;
    /*! \brief TRUE is a carrier is presnt. Otherwise FALSE. */
    int rx_signal_present;
    int rx_message_received;

    /*! \brief A tone generator context used to generate supervisory tones during
               FAX handling. */
    tone_gen_state_t tone_gen;
    /*! \brief An HDLC context used when receiving HDLC over V.21 messages. */
    hdlc_rx_state_t hdlcrx;
    /*! \brief An HDLC context used when transmitting HDLC over V.21 messages. */
    hdlc_tx_state_t hdlctx;
    /*! \brief A V.21 FSK modem context used when transmitting HDLC over V.21
               messages. */
    fsk_tx_state_t v21tx;
    /*! \brief A V.21 FSK modem context used when receiving HDLC over V.21
               messages. */
    fsk_rx_state_t v21rx;

#if defined(ENABLE_V17)
    /*! \brief A V.17 modem context used when sending FAXes at 7200bps, 9600bps
               12000bps or 14400bps*/
    v17_tx_state_t v17tx;
    /*! \brief A V.29 modem context used when receiving FAXes at 7200bps, 9600bps
               12000bps or 14400bps*/
    v17_rx_state_t v17rx;
#endif

    /*! \brief A V.27ter modem context used when sending FAXes at 2400bps or
               4800bps */
    v27ter_tx_state_t v27ter_tx;
    /*! \brief A V.27ter modem context used when receiving FAXes at 2400bps or
               4800bps */
    v27ter_rx_state_t v27ter_rx;
    /*! \brief A V.29 modem context used when sending FAXes at 7200bps or
               9600bps */
    v29_tx_state_t v29tx;
    /*! \brief A V.29 modem context used when receiving FAXes at 7200bps or
               9600bps */
    v29_rx_state_t v29rx;
    /*! \brief A counter for audio samples when inserting times silences according
               to the ITU specifications. */
    int silent_samples;
    int modem;
    int transmit;
    int short_train;
    int dte_is_waiting;
    int carrier_loss_timeout;
    int dte_inactivity_timeout;
    int dte_inactivity_action;
    int hdlc_final;
    int data_final;
    queue_t rx_queue;

    t31_call_control_handler_t *call_control_handler;
    void *call_control_user_data;
    t31_at_tx_handler_t *at_tx_handler;
    void *at_tx_user_data;
};

#ifdef __cplusplus
extern "C" {
#endif

void t31_call_event(t31_state_t *s, int event);

void t31_at_rx(t31_state_t *s, const char *t, int len);

int t31_tx(t31_state_t *s, int16_t *buf, int max_len);

int t31_tx(t31_state_t *s, int16_t *buf, int max_len);

int t31_init(t31_state_t *s,
             t31_at_tx_handler_t *at_tx_handler,
             void *at_tx_user_data,
             t31_call_control_handler_t *call_control_handler,
             void *call_control_user_data);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

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
 * $Id: t31.h,v 1.22 2005/11/24 13:04:52 steveu Exp $
 */

/*! \file */

#if !defined(_T31_H_)
#define _T31_H_

/*! \page t31_page T.31 Class 1 FAX modem protocol handling
\section t31_page_sec_1 What does it do?
The T.31 class 1 FAX modem modules implements a class 1 interface to the FAX
modems in spandsp.

\section t31_page_sec_2 How does it work?
*/

typedef struct t31_state_s t31_state_t;

typedef int (t31_modem_control_handler_t)(t31_state_t *s, void *user_data, int op, const char *num);
typedef int (t31_at_tx_handler_t)(t31_state_t *s, void *user_data, const uint8_t *buf, int len);

enum t31_rx_mode_e
{
    AT_MODE_ONHOOK_COMMAND,
    AT_MODE_OFFHOOK_COMMAND,
    AT_MODE_CONNECTED,
    AT_MODE_DELIVERY,
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
    T31_CALL_EVENT_NO_ANSWER,
    T31_CALL_EVENT_HANGUP
};

enum t31_modem_control_operation_e
{
    T31_MODEM_CONTROL_CALL,
    T31_MODEM_CONTROL_ANSWER,
    T31_MODEM_CONTROL_HANGUP,
    T31_MODEM_CONTROL_DTR,
    T31_MODEM_CONTROL_RTS,
    T31_MODEM_CONTROL_CTS,
    T31_MODEM_CONTROL_CAR,
    T31_MODEM_CONTROL_RNG,
    T31_MODEM_CONTROL_DSR
};

#define T31_TX_BUF_LEN      (4096*32)

/*!
    T.31 profile.
*/
typedef struct
{
    /*! TRUE if character echo is enabled */
    int echo;
    /*! TRUE if verbose reporting is enabled */
    int verbose;
    /*! TRUE if result codes are verbose */
    int result_code_format;
    /*! TRUE if pulse dialling is the default */
    int pulse_dial;
    /*! ??? */
    int double_escape;
    /*! ??? */
    int adaptive_receive;
    /*! The state of all possible S registers */
    uint8_t s_regs[100];
} t31_profile_t;

/*!
    T.31 descriptor. This defines the working state for a single instance of
    a T.31 FAX modem.
*/
struct t31_state_s
{
    int country_of_installation;
    char line[256];
    uint8_t hdlc_buf[256];
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
    int display_callid;
    int callid_displayed;
    const char *call_date;
    const char *call_time;
    const char *originating_name;
    const char *originating_number;
    const char *originating_ani;
    const char *destination_number;
    t31_profile_t p;
    uint8_t rx_data[256];
    int rx_data_bytes;
    uint8_t tx_data[T31_TX_BUF_LEN];
    int tx_in_bytes;
    int tx_out_bytes;
    int tx_holding;
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

    /*! \brief A V.29 modem context used when sending FAXes at 7200bps or
               9600bps */
    v29_tx_state_t v29tx;
    /*! \brief A V.29 modem context used when receiving FAXes at 7200bps or
               9600bps */
    v29_rx_state_t v29rx;

    /*! \brief A V.27ter modem context used when sending FAXes at 2400bps or
               4800bps */
    v27ter_tx_state_t v27ter_tx;
    /*! \brief A V.27ter modem context used when receiving FAXes at 2400bps or
               4800bps */
    v27ter_rx_state_t v27ter_rx;
    /*! \brief Rx power meter, use to detect silence */
    power_meter_t rx_power;
    int32_t silence_threshold_power;

    /*! \brief A counter for audio samples when inserting timed silences according
               to the ITU specifications. */
    int silent_samples;
	/*! \brief Samples of silence heard */
    int silence_heard;
	/*! \brief Samples of silence awaited */
    int silence_awaited;
    /*! \brief Samples elapsed in the current call */
    int64_t call_samples;
    int64_t last_dtedata_samples;
    int dohangup;
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

    t31_modem_control_handler_t *modem_control_handler;
    void *modem_control_user_data;
    t31_at_tx_handler_t *at_tx_handler;
    void *at_tx_user_data;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#ifdef __cplusplus
extern "C" {
#endif

void t31_call_event(t31_state_t *s, int event);

int t31_at_rx(t31_state_t *s, const char *t, int len);

/*! Process a block of received T.31 modem audio samples.
    \brief Process a block of received T.31 modem audio samples.
    \param s The T.31 modem context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of samples unprocessed. */
int t31_rx(t31_state_t *s, int16_t *buf, int len);

/*! Generate a block of T.31 modem audio samples.
    \brief Generate a block of T.31 modem audio samples.
    \param s The T.31 modem context.
    \param amp The audio sample buffer.
    \param max_len The number of samples to be generated.
    \return The number of samples actually generated.
*/
int t31_tx(t31_state_t *s, int16_t *buf, int max_len);

/*! Initialise a T.31 context. This must be called before the first
    use of the context, to initialise its contents.
    \brief Initialise a T.31 context.
    \param s The T.31 context.
    \param at_tx_handler ???.
    \param at_tx_user_data ???.
    \param modem_control_handler ???.
    \param modem_control_user_data ???.
    \return ???. */
int t31_init(t31_state_t *s,
             t31_at_tx_handler_t *at_tx_handler,
             void *at_tx_user_data,
             t31_modem_control_handler_t *modem_control_handler,
             void *modem_control_user_data);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

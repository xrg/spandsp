/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t30.h - definitions for T.30 fax processing
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
 * $Id: t30.h,v 1.16 2004/11/21 13:55:05 steveu Exp $
 */

/*! \file */

#if !defined(_T30_H_)
#define _T30_H_

/*! \page T30_page T.30 FAX protocol handling
*/

#define MAXFRAME            252

#define T30_OPTION_FINE     0x0001

typedef struct t30_state_s t30_state_t;

/*!
    T.30 phase B callback handler.
    \brief T.30 phase B callback handler.
    \param s The FAX context.
    \param user_data An opaque pointer.
    \param result The phase B event code.
*/
typedef void (t30_phase_b_handler_t)(t30_state_t *s, void *user_data, int result);
/*!
    T.30 phase D callback handler.
    \brief T.30 phase D callback handler.
    \param s The FAX context.
    \param user_data An opaque pointer.
    \param result The phase D event code.
*/
typedef void (t30_phase_d_handler_t)(t30_state_t *s, void *user_data, int result);
/*!
    T.30 phase E callback handler.
    \brief T.30 phase E callback handler.
    \param s The FAX context.
    \param user_data An opaque pointer.
    \param result The phase E event code.
*/
typedef void (t30_phase_e_handler_t)(t30_state_t *s, void *user_data, int result);
typedef void (t30_flush_handler_t)(t30_state_t *s,  void *user_data, int which);


/*!
    T.30 FAX channel descriptor. This defines the state of a single working
    instance of a T.30 FAX channel.
*/
struct t30_state_s
{
    /*! \brief The local identifier string. */
    uint8_t local_ident[21];
    /*! \brief The identifier string supplied by the remote FAX machine. */
    uint8_t far_ident[21];
    /*! \brief The sub-address string supplied by the remote FAX machine. */
    uint8_t sub_address[21];
    /*! \brief A password to be associated with the FAX context. */
    uint8_t password[21];
    /*! \brief The vendor of the remote machine, if known, else NULL. */
    const char *vendor;
    /*! \brief The model of the remote machine, if known, else NULL. */
    const char *model;
    int verbose;

    /*! \brief A pointer to a callback routine to be called when phase B events
        occur. */
    t30_phase_b_handler_t *phase_b_handler;
    /*! \brief An opaque pointer supplied in event B callbacks. */
    void *phase_b_user_data;
    /*! \brief A pointer to a callback routine to be called when phase D events
        occur. */
    t30_phase_d_handler_t *phase_d_handler;
    /*! \brief An opaque pointer supplied in event D callbacks. */
    void *phase_d_user_data;
    /*! \brief A pointer to a callback routine to be called when phase E events
        occur. */
    t30_phase_e_handler_t *phase_e_handler;
    /*! \brief An opaque pointer supplied in event E callbacks. */
    void *phase_e_user_data;
    t30_flush_handler_t *t30_flush_handler;
    void *t30_flush_user_data;

    int options;
    int phase;
    int next_phase;
    int state;
    int mode;
    int msgendtime;
    int samplecount;
    
    uint8_t dtc_frame[15];
    int dtc_len;
    uint8_t dcs_frame[15];
    int dcs_len;
    uint8_t dis_frame[15];
    int dis_len;

    /*! \brief A flag to indicate a message is in progress. */
    int in_message;
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

    /*! \brief TRUE is the short training sequence should be used. */
    int short_train;

    /*! \brief A count of the number of bits in the trainability test. */
    int training_test_bits;
    int training_current_zeros;
    int training_most_zeros;

    /*! \brief The current bit rate for the fast message transfer modem. */
    int bit_rate;
    /*! \brief The current modem type for the fast message transfer modem. */
    int modem_type;
    /*! \brief TRUE is a carrier is presnt. Otherwise FALSE. */
    int rx_signal_present;

    /* timer_t0 is the answer timeout when calling another FAX machine.
       Placing calls is handled outside the FAX processing. */
    /*! \brief Remote terminal identification timeout (in audio samples) */
    int timer_t1;
    /*! \brief HDLC timer (in audio samples) */
    int timer_t2;
    /*! \brief Procedural interrupt timeout (in audio samples) */
    int timer_t3;
    /*! \brief Response timer (in audio samples) */
    int timer_t4;
    /* timer_t5 is only used with error correction */
    /*! \brief Signal on timer (in audio samples) */
    int timer_sig_on;

    int line_encoding;
    int min_row_bits;
    int resolution;
    int image_width;
    t4_state_t t4;
    char rx_file[256];
    char tx_file[256];
};

typedef struct
{
    /*! \brief The current bit rate for image transfer. */
    int bit_rate;
    /*! \brief The number of pages transferred so far. */
    int pages_transferred;
    /*! \brief The number of horizontal pixels in the most recent page. */
    int columns;
    /*! \brief The number of vertical pixels in the most recent page. */
    int rows;
    /*! \brief The number of bad pixel rows in the most recent page. */
    int bad_rows;
    /*! \brief The largest number of bad pixel rows in a block in the most recent page. */
    int longest_bad_row_run;
    /*! \brief The horizontal resolution of the page in pixels per metre */
    int column_resolution;
    /*! \brief The vertical resolution of the page in pixels per metre */
    int row_resolution;
    /*! \brief The type of compression used between the FAX machines */
    int encoding;
    /*! \brief The size of the image, in bytes */
    int image_size;
} t30_stats_t;
    
#ifdef __cplusplus
extern "C" {
#endif

/*! Return a text name for a T.30 frame type.
    \brief Return a text name for a T.30 frame type.
    \param x The frametype octet.
    \return A pointer to the text name for the frame type. If the frame type is
            not value, the string "???" is returned.
*/
char *t30_frametype(uint8_t x);

/*! Decode a DIS, DTC or DCS frame, and log the contents.
    \brief Decode a DIS, DTC or DCS frame, and log the contents.
    \param s The FAX context.
    \param dis A pointer to the frame to be decoded.
    \param len The length of the frame.
*/
void t30_decode_dis_dtc_dcs(t30_state_t *s, const uint8_t *dis, int len);

/*! Initialise a FAX context.
    \brief Initialise a FAX context.
    \param s The FAX context.
    \param calling_party TRUE if the context is for a calling party. FALSE if the
           context is for an answering party.
    \param user_data An opaque pointer which is associated with the FAX context,
           and supplied in callbacks.
    \return ???
*/
int fax_init(t30_state_t *s, int calling_party, void *user_data);

/*! Set the sub-address associated with a FAX context.
    \brief Set the sub-address associated with a FAX context.
    \param s The FAX context.
    \param sub_address A pointer to the sub-address.
    \return ???
*/
int fax_set_sub_address(t30_state_t *s, const char *sub_address);

int fax_set_header_info(t30_state_t *s, const char *info);

/*! Set the local identifier associated with a FAX context.
    \brief Set the local identifier associated with a FAX context.
    \param s The FAX context.
    \param id A pointer to the identifier.
    \return ???
*/
int fax_set_local_ident(t30_state_t *s, const char *id);

/*! Get the sub-address associated with a FAX context.
    \brief Get the sub-address associated with a FAX context.
    \param s The FAX context.
    \param sub_address A pointer to a buffer for the sub-address.  The buffer
           should be at least 21 bytes long.
    \return ???
*/
int fax_get_sub_address(t30_state_t *s, char *sub_address);

int fax_get_header_info(t30_state_t *s, char *info);

/*! Get the local FAX machine identifier associated with a FAX context.
    \brief Get the local identifier associated with a FAX context.
    \param s The FAX context.
    \param id A pointer to a buffer for the identifier. The buffer should
           be at least 21 bytes long.
    \return ???
*/
int fax_get_local_ident(t30_state_t *s, char *id);

/*! Get the remote FAX machine identifier associated with a FAX context.
    \brief Get the remote identifier associated with a FAX context.
    \param s The FAX context.
    \param id A pointer to a buffer for the identifier. The buffer should
           be at least 21 bytes long.
    \return ???
*/
int fax_get_far_ident(t30_state_t *s, char *id);

/*! Get the current transfer statistics for the file being sent or received.
    \brief Get the current transfer statistics.
    \param s The FAX context.
    \param t A pointer to a buffer for the statistics.
*/
void fax_get_transfer_statistics(t30_state_t *s, t30_stats_t *t);

void fax_set_phase_b_handler(t30_state_t *s, t30_phase_b_handler_t *handler, void *user_data);
void fax_set_phase_d_handler(t30_state_t *s, t30_phase_d_handler_t *handler, void *user_data);
void fax_set_phase_e_handler(t30_state_t *s, t30_phase_e_handler_t *handler, void *user_data);
void fax_set_flush_handler(t30_state_t *s, t30_flush_handler_t *handler, void *user_data);

/*! Specify the file name of the next TIFF file to be transmitted by a FAX
    context
    \brief Set next transmit file
    \param s The FAX context.
    \param file The file name
*/
void fax_set_rx_file(t30_state_t *s, const char *file);
/*! Specify the file name of the next TIFF file to be received by a FAX
    context
    \brief Set next receive file
    \param s The FAX context.
    \param file The file name
*/
void fax_set_tx_file(t30_state_t *s, const char *file);

/*! Apply FAX receive processing to a block of audio samples.
    \brief Apply FAX receive processing to a block of audio samples.
    \param s The FAX context.
    \param buf The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of samples unprocessed. This should only be non-zero if
            the software has reached the end of the FAX call.
*/
int fax_rx_process(t30_state_t *s, int16_t *buf, int len);

/*! Apply FAX transmit processing to generate a block of audio samples.
    \brief Apply FAX transmit processing to generate a block of audio samples.
    \param s The FAX context.
    \param buf The audio sample buffer.
    \param max_len The number of samples to be generated.
    \return The number of samples actually generated. This will be zero when
            there is nothing to send.
*/
int fax_tx_process(t30_state_t *s, int16_t *buf, int max_len);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

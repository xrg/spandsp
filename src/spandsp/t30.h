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

\section T30_page_sec_1 What does it do?
The T.30 protocol is the core protocol used for FAX transmission.

\section T30_page_sec_2 How does it work?

Some of the following is paraphrased from some notes found a while ago on the Internet.
I cannot remember exactly where they came from, but they are useful.

The answer (CED) tone

The T.30 standard says an answering fax device must send CED (a 2100Hz tone) for
approximately 3 seconds before sending the first handshake message. Some machines
send an 1100Hz or 1850Hz tone, and some send no tone at all. In fact, this answer
tone is so unpredictable, it cannot really be used. It should, however, always be
generated according to the specification.

Common Timing Deviations

The T.30 spec. specifies a number of time-outs. For example, after dialing a number,
a calling fax system should listen for a response for 35 seconds before giving up.
These time-out periods are as follows: 

    * T1 - 35 ± 5s: the maximum time for which two fax system will attempt to identify each other
    * T2 - 6 ± 1s:  a time-out used to start the sequence for changing transmit parameters
    * T3 - 10 ± 5s: a time-out used in handling operator interrupts
    * T5 - 60 ± 5s: a time-out used in error correction mode

These time-outs are sometimes misinterpreted. In addition, they are routinely
ignored, sometimes with good reason. For example, after placing a call, the
calling fax system is supposed to wait for 35 seconds before giving up. If the
answering unit does not answer on the first ring or if a voice answering machine
is connected to the line, or if there are many delays through the network,
the delay before answer can be much longer than 35 seconds. 

Fax units that support error correction mode (ECM) can respond to a post-image
handshake message with a receiver not ready (RNR) message. The calling unit then
queries the receiving fax unit with a receiver ready (RR) message. If the
answering unit is still busy (printing for example), it will repeat the RNR
message. According to the T.30 standard, this sequence (RR/RNR RR/RNR) can be
repeated for up to the end of T5 (60 ± 5 seconds). However, many fax systems
ignore the time-out and will continue the sequence indefinitely, unless the user
manually overrides. 

All the time-outs are subject to alteration, and sometimes misuse. Good T.30
implementations must do the right thing, and tolerate others doing the wrong thing.
 
Variations in the inter-carrier gap

T.30 specifies 75 ± 20ms of silence between signals using different modulation
schemes. Examples are between the end of a DCS signal and the start of a TCF signal,
and between the end of an image and the start of a post-image signal. Many fax systems
violate this requirement, especially for the silent period between DCS and TCF.
This may be stretched to well over 100ms. If this period is too long, it can interfere with
handshake signal error recovery, should a packet be corrupted on the line. Systems
should ensure they stay within the prescribed T.30 limits, and be tolerant of others
being out of spec.. 

Other timing variations

Testing is required to determine the ability of a fax system to handle
variations in the duration of pauses between unacknowledged handshake message
repetitions, and also in the pauses between the receipt of a handshake command and
the start of a response to that command. In order to reduce the total
transmission time, many fax systems start sending a response message before the
end of the command has been received. 

Other deviations from the T.30 standard

There are many other commonly encountered variations between machines, including:

    * frame sequence deviations
    * preamble and flag sequence variations
    * improper EOM usage
    * unusual data rate fallback sequences
    * common training pattern detection algorithms
    * image transmission deviations
    * use of the talker echo protect tone
    * image padding and short lines
    * RTP/RTN handshake message usage
    * long duration lines
    * nonstandard disconnect sequences
    * DCN usage
*/

#define MAXFRAME            252

typedef struct t30_state_s t30_state_t;

/*!
    T.30 phase B callback handler.
    \brief T.30 phase B callback handler.
    \param s The T.30 context.
    \param user_data An opaque pointer.
    \param result The phase B event code.
*/
typedef void (t30_phase_b_handler_t)(t30_state_t *s, void *user_data, int result);

/*!
    T.30 phase D callback handler.
    \brief T.30 phase D callback handler.
    \param s The T.30 context.
    \param user_data An opaque pointer.
    \param result The phase D event code.
*/
typedef void (t30_phase_d_handler_t)(t30_state_t *s, void *user_data, int result);

/*!
    T.30 phase E callback handler.
    \brief T.30 phase E callback handler.
    \param s The T.30 context.
    \param user_data An opaque pointer.
    \param result The phase E result code.
*/
typedef void (t30_phase_e_handler_t)(t30_state_t *s, void *user_data, int status);

typedef void (t30_flush_handler_t)(t30_state_t *s,  void *user_data, int which);

/*!
    T.30 protocol completion codes, at phase E.
*/
enum
{
    T30_ERR_OK = 0,         /* OK */

    /* External problems */
    T30_ERR_CEDTONE,        /* The CED tone exceeded 5s */
    T30_ERR_T0EXPIRED,      /* Timed out waiting for initial communication */
    T30_ERR_T1EXPIRED,      /* Timed out waiting for the first message */
    T30_ERR_T3EXPIRED,      /* Timed out waiting for procedural interrupt */
    T30_ERR_HDLCCARR,       /* The HDLC carrier did not stop in a timely manner */
    T30_ERR_CANNOTTRAIN,    /* Failed to train with any of the compatible modems */
    T30_ERR_OPERINTFAIL,    /* Operator intervention failed */
    T30_ERR_INCOMPATIBLE,   /* Far end is not compatible */
    T30_ERR_NOTRXCAPABLE,   /* Far end is not receive capable */
    T30_ERR_NOTTXCAPABLE,   /* Far end is not transmit capable */
    T30_ERR_UNEXPECTED,     /* Unexpected message received */
    T30_ERR_NORESSUPPORT,   /* Far end cannot receive at the resolution of the image */
    T30_ERR_NOSIZESUPPORT,  /* Far end cannot receive at the size of image */

    /* Internal problems */
    T30_ERR_FILEERROR,      /* TIFF/F file cannot be opened */
    T30_ERR_NOPAGE,         /* TIFF/F page not found */
    T30_ERR_BADTIFF,        /* TIFF/F format is not compatible */
    T30_ERR_UNSUPPORTED,    /* Unsupported feature */

    /* Phase E status values returned to a transmitter */
    T30_ERR_BADDCSTX,       /* Received bad response to DCS or training */
    T30_ERR_BADPGTX,        /* Received a DCN from remote after sending a page */
    T30_ERR_ECMPHDTX,       /* Invalid ECM response received from receiver */
    T30_ERR_ECMRNRTX,       /* Timer T5 expired, receiver not ready */
    T30_ERR_GOTDCNTX,       /* Received a DCN while waiting for a DIS */
    T30_ERR_INVALRSPTX,     /* Invalid response after sending a page */
    T30_ERR_NODISTX,        /* Received other than DIS while waiting for DIS */
    T30_ERR_NXTCMDTX,       /* Timed out waiting for next send_page command from driver */
    T30_ERR_PHBDEADTX,      /* Received no response to DCS, training or TCF */
    T30_ERR_PHDDEADTX,      /* No response after sending a page */

    /* Phase E status values returned to a receiver */
    T30_ERR_ECMPHDRX,       /* Invalid ECM response received from transmitter */
    T30_ERR_GOTDCSRX,       /* DCS received while waiting for DTC */
    T30_ERR_INVALCMDRX,     /* Unexpected command after page received */
    T30_ERR_NOCARRIERRX,    /* Carrier lost during fax receive */
    T30_ERR_NOEOLRX,        /* Timed out while waiting for EOL (end Of line) */
    T30_ERR_NOFAXRX,        /* Timed out while waiting for first line */
    T30_ERR_NXTCMDRX,       /* Timed out waiting for next receive page command */
    T30_ERR_T2EXPDCNRX,     /* Timer T2 expired while waiting for DCN */
    T30_ERR_T2EXPDRX,       /* Timer T2 expired while waiting for phase D */
    T30_ERR_T2EXPFAXRX,     /* Timer T2 expired while waiting for fax page */
    T30_ERR_T2EXPMPSRX,     /* Timer T2 expired while waiting for next fax page */
    T30_ERR_T2EXPRRRX,      /* Timer T2 expired while waiting for RR command */
    T30_ERR_T2EXPRX,        /* Timer T2 expired while waiting for NSS, DCS or MCF */
    T30_ERR_WHYDCNRX,       /* Unexpected DCN while waiting for DCS or DIS */
    T30_ERR_DCNDATARX,      /* Unexpected DCN while waiting for image data */
    T30_ERR_DCNFAXRX,       /* Unexpected DCN while waiting for EOM, EOP or MPS */
    T30_ERR_DCNPHDRX,       /* Unexpected DCN after EOM or MPS sequence */
    T30_ERR_DCNRRDRX,       /* Unexpected DCN after RR/RNR sequence */
    T30_ERR_DCNNORTNRX,     /* Unexpected DCN after requested retransmission */

    T30_ERR_BADPAGE,        /* TIFF/F page number tag missing */
    T30_ERR_BADTAG,         /* Incorrect values for TIFF/F tags */
    T30_ERR_BADTIFFHDR,     /* Bad TIFF/F header - incorrect values in fields */
    T30_ERR_BADPARM,        /* Invalid value for fax parameter */
    T30_ERR_BADSTATE,       /* Invalid initial state value specified */
    T30_ERR_CMDDATA,        /* Last command contained invalid data */
    T30_ERR_DISCONNECT,     /* Fax call disconnected by the other station */
    T30_ERR_INVALARG,       /* Illegal argument to function */
    T30_ERR_INVALFUNC,      /* Illegal call to function */
    T30_ERR_NODATA,         /* Data requested is not available (NSF, DIS, DCS) */
    T30_ERR_NOMEM,          /* Cannot allocate memory for more pages */
    T30_ERR_NOPOLL,         /* Poll not accepted */
    T30_ERR_NOSTATE,        /* Initial state value not set */
    T30_ERR_RETRYDCN,       /* Disconnected after permitted retries */
};

/*!
    I/O modes for the T.30 protocol.
*/
enum
{
    T30_MODEM_NONE = 0,
    T30_MODEM_PAUSE,
    T30_MODEM_CED,
    T30_MODEM_CNG,
    T30_MODEM_V21,
    T30_MODEM_V27TER_2400,
    T30_MODEM_V27TER_4800,
    T30_MODEM_V29_7200,
    T30_MODEM_V29_9600,
    T30_MODEM_V17_7200,
    T30_MODEM_V17_9600,
    T30_MODEM_V17_12000,
    T30_MODEM_V17_14400,
    T30_MODEM_DONE
};

/*!
    T.30 FAX channel descriptor. This defines the state of a single working
    instance of a T.30 FAX channel.
*/
struct t30_state_s
{
    /* This must be kept the first thing in the structure, so it can be pointed
       to reliably as the structures change over time. */
    t4_state_t t4;

    /*! \brief TRUE is behaving as the calling party */
    int calling_party;

    /*! \brief The local identifier string. */
    char local_ident[21];
    /*! \brief The identifier string supplied by the remote FAX machine. */
    char far_ident[21];
    /*! \brief The sub-address string supplied by the remote FAX machine. */
    char sub_address[21];
    /*! \brief A password to be associated with the T.30 context. */
    char password[21];
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
    
    uint8_t dtc_frame[22];
    int dtc_len;
    uint8_t dcs_frame[22];
    int dcs_len;
    uint8_t dis_frame[22];
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
    \param s The T.30 context.
    \param dis A pointer to the frame to be decoded.
    \param len The length of the frame.
*/
void t30_decode_dis_dtc_dcs(t30_state_t *s, const uint8_t *dis, int len);

/*! Initialise a T.30 context.
    \brief Initialise a T.30 context.
    \param s The T.30 context.
    \param calling_party TRUE if the context is for a calling party. FALSE if the
           context is for an answering party.
    \param user_data An opaque pointer which is associated with the T.30 context,
           and supplied in callbacks.
    \return ???
*/
int fax_init(t30_state_t *s, int calling_party, void *user_data);

/*! Release a T.30 context.
    \brief Release a T.30 context.
    \param s The T.30 context.
    \return ???
*/
int fax_release(t30_state_t *s);

/*! Set the sub-address associated with a T.30 context.
    \brief Set the sub-address associated with a T.30 context.
    \param s The T.30 context.
    \param sub_address A pointer to the sub-address.
    \return ???
*/
int fax_set_sub_address(t30_state_t *s, const char *sub_address);

int fax_set_header_info(t30_state_t *s, const char *info);

/*! Set the local identifier associated with a T.30 context.
    \brief Set the local identifier associated with a T.30 context.
    \param s The T.30 context.
    \param id A pointer to the identifier.
    \return ???
*/
int fax_set_local_ident(t30_state_t *s, const char *id);

/*! Get the sub-address associated with a T.30 context.
    \brief Get the sub-address associated with a T.30 context.
    \param s The T.30 context.
    \param sub_address A pointer to a buffer for the sub-address.  The buffer
           should be at least 21 bytes long.
    \return ???
*/
int fax_get_sub_address(t30_state_t *s, char *sub_address);

int fax_get_header_info(t30_state_t *s, char *info);

/*! Get the local FAX machine identifier associated with a T.30 context.
    \brief Get the local identifier associated with a T.30 context.
    \param s The T.30 context.
    \param id A pointer to a buffer for the identifier. The buffer should
           be at least 21 bytes long.
    \return ???
*/
int fax_get_local_ident(t30_state_t *s, char *id);

/*! Get the remote FAX machine identifier associated with a T.30 context.
    \brief Get the remote identifier associated with a T.30 context.
    \param s The T.30 context.
    \param id A pointer to a buffer for the identifier. The buffer should
           be at least 21 bytes long.
    \return ???
*/
int fax_get_far_ident(t30_state_t *s, char *id);

/*! Get the current transfer statistics for the file being sent or received.
    \brief Get the current transfer statistics.
    \param s The T.30 context.
    \param t A pointer to a buffer for the statistics.
*/
void fax_get_transfer_statistics(t30_state_t *s, t30_stats_t *t);

void fax_set_phase_b_handler(t30_state_t *s, t30_phase_b_handler_t *handler, void *user_data);
void fax_set_phase_d_handler(t30_state_t *s, t30_phase_d_handler_t *handler, void *user_data);
void fax_set_phase_e_handler(t30_state_t *s, t30_phase_e_handler_t *handler, void *user_data);
void fax_set_flush_handler(t30_state_t *s, t30_flush_handler_t *handler, void *user_data);

/*! Specify the file name of the next TIFF file to be transmitted by a T.30
    context
    \brief Set next receive file name
    \param s The T.30 context.
    \param file The file name
*/
void fax_set_rx_file(t30_state_t *s, const char *file);

/*! Specify the file name of the next TIFF file to be received by a T.30 context
    \brief Set next transmit file name
    \param s The T.30 context.
    \param file The file name
*/
void fax_set_tx_file(t30_state_t *s, const char *file);

/*! Apply FAX receive processing to a block of audio samples.
    \brief Apply FAX receive processing to a block of audio samples.
    \param s The T.30 context.
    \param buf The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of samples unprocessed. This should only be non-zero if
            the software has reached the end of the FAX call.
*/
int fax_rx_process(t30_state_t *s, int16_t *buf, int len);

/*! Apply FAX transmit processing to generate a block of audio samples.
    \brief Apply FAX transmit processing to generate a block of audio samples.
    \param s The T.30 context.
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

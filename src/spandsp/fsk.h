/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fsk.h - FSK modem transmit and receive parts
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
 * $Id: fsk.h,v 1.2 2004/03/19 19:12:46 steveu Exp $
 */

/*! \file */

#if !defined(_FSK_H_)
#define _FSK_H_

/*! \page fsk_page The FSK modem
\section fsk_page_sec_1 What does it do
Most of the oldest telephony modems use incorent FSK modulation. This module can
be used to implement both the trasmit and receive sides of a number of these
modems. There are integrated definitions for: 

    V.21
    V.23
    Bell 103
    Bell 202
    Weitbrecht (Used for TDD - Telecoms Device for the Deaf)

The audio output or input is a stream of 16 bit samples, at 8000 samples/second.
The transmit and receive sides can be used independantly. 

\section fsk_page_sec_2 The transmitter

The FSK transmitter uses a DDS generator to synthesise the waveform. This
naturally produces phase coherent transitions, as the phase update rate is
switched, producing a clean spectrum. The symbols are not generally an integer
number of samples long. However, the symbol time for the fastest data rate
generally used (1200bps) is more than 7 samples long. The jitter resulting from
switching at the nearest sample is, therefore, acceptable. No interpolation is
used. 

\section fsk_page_sec3 The receiver

The FSK receiver uses a quadrature correlation technique to demodulate the
signal. Two DDS quadrature oscillators are used. The incoming signal is
correlated with the oscillator signals over a period of one symbol. The
oscillator giving the highest net correlation from its I and Q outputs is the
one that matches the frequency being transmitted during the correlation
interval. Because the transmission is totally asynchronous, the demodulation
process must run sample by sample to find the symbol transitions. The
correlation is performed on a sliding window basis, so the computational load of
demodulating sample by sample is not great. 
*/

/* Special "bit" values for put_bit_func_t */
#define PUTBIT_CARRIER_DOWN         -1
#define PUTBIT_CARRIER_UP           -2
#define PUTBIT_TRAINING_SUCCEEDED   -3
#define PUTBIT_TRAINING_FAILED      -4

/* Message I/O functions for data pumps */
typedef void (*put_msg_func_t)(void *user_data, const uint8_t *msg, int len);
typedef int (*get_msg_func_t)(void *user_data, uint8_t *msg, int max_len);

/* Byte I/O functions for data pumps */
typedef void (*put_byte_func_t)(void *user_data, int byte);
typedef int (*get_byte_func_t)(void *user_data);

/* Bit I/O functions for data pumps */
typedef void (*put_bit_func_t)(void *user_data, int bit);
typedef int (*get_bit_func_t)(void *user_data);

/*!
    FSK modem specification. This defines the frequencies, signal levels and
    baud rate (== bit rate) for a single channel of an FSK modem.
*/
typedef struct
{
    char *name;
    int freq_zero;
    int freq_one;
    int tx_level;
    int min_level;
    int baud_rate;
} fsk_spec_t;

/* Predefined FSK modem channels */
#define FSK_V21CH1      0
#define FSK_V21CH2      1
#define FSK_V23CH1      2
#define FSK_V23CH2      3
#define FSK_BELL103CH1  4
#define FSK_BELL103CH2  5
#define FSK_BELL202     6
#define FSK_WEITBRECHT  7   /* Used for TDD (Telecomc Device for the Deaf) */

extern fsk_spec_t preset_fsk_specs[];

/*!
    FSK modem transmit descriptor. This defines the state of a single working
    instance of an FSK modem transmitter.
*/
typedef struct
{
    int baud_rate;
    get_bit_func_t get_bit;
    void *user_data;

    int32_t phase_rates[2];
    int scaling;
    int32_t current_phase_rate;
    uint32_t phase_acc;
    int baud_frac;
    int baud_inc;
} fsk_tx_state_t;

/* The longest window will probably be 106 for 75 baud */
#define FSK_MAX_WINDOW_LEN 128

/*!
    FSK modem receive descriptor. This defines the state of a single working
    instance of an FSK modem receiver.
*/
typedef struct
{
    int baud_rate;
    int sync_mode;
    put_bit_func_t put_bit;
    void *user_data;

    int min_power;
    power_meter_t power;
    int carrier_present;

    int32_t phase_rate[2];
    uint32_t phase_acc[2];

    int correlation_span;

    int32_t window_i[2][FSK_MAX_WINDOW_LEN];
    int32_t window_q[2][FSK_MAX_WINDOW_LEN];
    int32_t dot_i[2];
    int32_t dot_q[2];
    int buf_ptr;

    int baud_inc;
    int baud_pll;
    int lastbit;
    int scaling_shift;
} fsk_rx_state_t;

#define ASYNC_PARITY_NONE   0
#define ASYNC_PARITY_EVEN   1
#define ASYNC_PARITY_ODD    2

/*!
    Asynchronous data transmit descriptor. This defines the state of a single
    working instance of a byte to asynchronous serial converter, for use
    in FSK modems.
*/
typedef struct
{
    /*! \brief The number of data bits per character. */
    int data_bits;
    /*! \brief The type of parity. */
    int parity;
    /*! \brief The number of stop bits per character. */
    int stop_bits;
    /*! \brief A pointer to the callback routine used to get characters to be transmitted. */
    get_byte_func_t get_byte;
    /*! \brief An opaque pointer passed when calling get_byte. */
    void *user_data;

    /*! \brief A current, partially transmitted, character. */
    int byte_in_progress;
    /*! \brief The current bit position within a partially transmitted character. */
    int bitpos;
    int parity_bit;
} async_tx_state_t;

/*!
    Asynchronous data receive descriptor. This defines the state of a single
    working instance of an asynchronous serial to byte converter, for use
    in FSK modems.
*/
typedef struct
{
    /*! \brief The number of data bits per character. */
    int data_bits;
    /*! \brief The type of parity. */
    int parity;
    /*! \brief The number of stop bits per character. */
    int stop_bits;
    /*! \brief A pointer to the callback routine used to handle received characters. */
    put_byte_func_t put_byte;
    /*! \brief An opaque pointer passed when calling put_byte. */
    void *user_data;

    /*! \brief A current, partially complete, character. */
    int byte_in_progress;
    /*! \brief The current bit position within a partially complete character. */
    int bitpos;
    int parity_bit;

    int parity_errors;
    int framing_errors;
} async_rx_state_t;

/*! Initialise an FSK modem transmit context.
    \brief Initialise an FSK modem transmit context.
    \param s The modem context.
    \param spec The specification of the modem tones and rate.
    \param get_bit The callback routine used to get the data to be transmitted.
    \param user_data An opaque pointer. */
void fsk_tx_init(fsk_tx_state_t *s,
                 fsk_spec_t *spec,
                 get_bit_func_t get_bit,
                 void *user_data);
/*! Generate a block of FSK modem audio samples.
    \brief Generate a block of FSK modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples to be generated.
    \return The number of samples actually generated.
*/
int fsk_tx(fsk_tx_state_t *s, int16_t *amp, int len);
/*! Initialise an FSK modem receive context.
    \brief Initialise an FSK modem receive context.
    \param s The modem context.
    \param spec The specification of the modem tones and rate.
    \param sync_mode TRUE for synchronous modem. FALSE for asynchronous mode.
    \param put_bit The callback routine used to put the received data.
    \param user_data An opaque pointer. */
void fsk_rx_init(fsk_rx_state_t *s,
                 fsk_spec_t *spec,
                 int sync_mode,
                 put_bit_func_t put_bit,
                 void *user_data);
/*! Process a block of received FSK modem audio samples.
    \brief Process a block of received FSK modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
*/
void fsk_rx(fsk_rx_state_t *s, const int16_t *amp, int len);

/*! Initialise an asynchronous data transmit context.
    \brief Initialise an asynchronous data transmit context.
    \param s The transmitter context.
    \param data_bits The number of data bit.
    \param parity_bits The type of parity.
    \param stop_bits The number of stop bits.
    \param get_byte The callback routine used to get the data to be transmitted.
    \param user_data An opaque pointer. */
void async_tx_init(async_tx_state_t *s,
                   int data_bits,
                   int parity_bits,
                   int stop_bits,
                   get_byte_func_t get_byte,
                   void *user_data);
int async_tx_bit(void *user_data);
/*! Initialise an asynchronous data receiver context.
    \brief Initialise an asynchronous data receiver context.
    \param s The receiver context.
    \param data_bits The number of data bit.
    \param parity_bits The type of parity.
    \param stop_bits The number of stop bits.
    \param put_byte The callback routine used to put the received data.
    \param user_data An opaque pointer. */
void async_rx_init(async_rx_state_t *s,
                   int data_bits,
                   int parity_bits,
                   int stop_bits,
                   put_byte_func_t put_byte,
                   void *user_data);
void async_rx_bit(void *user_data, int bit);

#endif
/*- End of file ------------------------------------------------------------*/

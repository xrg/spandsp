/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v17tx.h - ITU V.17 modem transmit part
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
 * $Id: v17tx.h,v 1.19 2005/12/29 09:54:24 steveu Exp $
 */

/*! \file */

#if !defined(_V17TX_H_)
#define _V17TX_H_

/*! \page v17tx_page The V.17 transmitter
\section v17tx_page_sec_1 What does it do?
The V.17 transmitter implements the transmit side of a V.17 modem. This can
operate at data rates of 14400, 12000, 9600 and 7200 bits/second. The audio
output is a stream of 16 bit samples, at 8000 samples/second. The transmit and
receive side of V.17 modems operate independantly. V.17 is mostly used for FAX
transmission, where it provides the standard 14400 bits/second rate. 

\section v17tx_page_sec_2 How does it work?
V.17 uses QAM modulation and trellis coding. The data to be transmitted is
scrambled, to whiten it. The least significant 2 bits of each symbol are then
differentially encoded, using a simple lookup approach. The resulting 2 bits are
convolutionally encoded, producing 3 bits. The extra bit is the redundant bit
of the trellis code. The other bits of the symbol pass by the differential
and convolutional coding unchanged. The resulting bits define the constellation
point to be transmitted for the symbol. The redundant bit doubles the size of the
constellation, and so increases the error rate for detecting individual symbols
at the receiver. However, when a number of successive symbols are processed at
the receiver, the redundancy actually provides several dB of improved error
performance.

The standard method of producing a QAM modulated signal is to use a sampling
rate which is a multiple of the baud rate. The raw signal is then a series of
complex pulses, each an integer number of samples long. These can be shaped,
using a suitable complex filter, and multiplied by a complex carrier signal
to produce the final QAM signal for transmission. 

The sampling rate for our transmitter is defined by the channel - 8000 per
second. This is not a multiple of the baud rate (i.e. 2400 baud). The baud
interval is actually 10/3 sample periods. Generating at the lowest common
multiple of the baud rate and channel sample rate (i.e. 24000 samples/second),
and then decimating to 8000 samples/second, would give good results. However,
this would require considerable computation. A shortcut is to use slightly
shaped pulses, instead of simple square ones. We can achieve the effect of pulse
transitions at the 1/2 and 2/3 sample points by adjusting the first sample of
each new pulse. The adjustment is simple. We need the effect of being 60 degrees
or 120 degrees through a sine wave cycle at the Shannon rate at the sample
point. This simply means we need to step by 0.25 or 0.75 of the actual step size
on the first sample of those pulses which should start at the 1/3 or 2/3 sample
positions. The logic and computation needed for this is much less than the
computation needed for oversampling at 24000 samples/second. 

The pulse shaping filter is only vaguely defined by the V.17 spec. Some of the
other ITU modem specs. fully define the filter, typically specifying a root
raised cosine filter, with 50% excess bandwidth. This is a pity, since it
increases the variability of the received signal. However, the receiver's
adaptive equalizer will largely compensate for these differences. The current
design uses a root raised cosine filter with 50% excess bandwidth. 

The carrier is generated using the DDS method. Using two second order resonators,
started in quadrature, might be more efficient, as it would have less impact on
the processor cache than a table lookup approach. However, the DDS approach
suits the receiver better, so the same signal generator is also used for the
transmitter. 
*/

#define V17TX_FILTER_STEPS      27

/*!
    V.17 modem transmit side descriptor. This defines the working state for a
    single instance of a V.17 modem transmitter.
*/
typedef struct
{
    /*! \brief The bit rate of the modem. Valid values are 4800, 7200 and 9600. */
    int bit_rate;
    /*! \brief The callback function used to get the next bit to be transmitted. */
    get_bit_func_t get_bit;
    /*! \brief A user specified opaque pointer passed to the callback function. */
    void *user_data;

    float gain;

    /*! \brief The route raised cosine (RRC) pulse shaping filter buffer. */
    complex_t rrc_filter[2*V17TX_FILTER_STEPS];
    /*! \brief Current offset into the RRC pulse shaping filter buffer. */
    int rrc_filter_step;
    /*! \brief The current constellation position. */
    complex_t current_point;

    int diff;
    int convolution;

    /*! \brief The register for the data scrambler. */
    unsigned int scramble_reg;
    /*! \brief TRUE if transmitting the training sequence. FALSE if transmitting user data. */
    int in_training;
    /*! A counter used to track progress through the optional TEP tone burst */
    int tep_step;
    /*! \brief TRUE if the short training sequence is to be used. */
    int short_train;
    /*! \brief A counter used to track progress through sending the training sequence. */
    int training_step;

    /*! \brief The current phase of the carrier (i.e. the DDS parameter). */
    uint32_t carrier_phase;
    /*! \brief The update rate for the phase of the carrier (i.e. the DDS increment). */
    int32_t carrier_phase_rate;
    /*! \brief The current fractional phase of the baud timing. */
    int baud_phase;
    /*! \brief The code number for the current position in the constellation. */
    int constellation_state;
    
    /*! \brief A pointer to the constellation currently in use. */
    const complex_t *constellation;
    /*! \brief The current number of data bits per symbol. This does not include
               the redundant bit. */
    int bits_per_symbol;
    /*! \brief An indicator to mark that we are tidying up to stop transmission. */
    int shutdown;
    /*! \brief The get_bit function in use at any instant. */
    get_bit_func_t current_get_bit;
    /*! \brief Error and flow logging control */
    logging_state_t logging;
} v17_tx_state_t;

#ifdef __cplusplus
extern "C" {
#endif

/*! Adjust a V.17 modem transmit context's power output.
    \brief Adjust a V.17 modem transmit context's output power.
    \param s The modem context.
    \param power The power level, in dBm0 */
void v17_tx_power(v17_tx_state_t *s, float power);

/*! Initialise a V.17 modem transmit context. This must be called before the first
    use of the context, to initialise its contents.
    \brief Initialise a V.17 modem transmit context.
    \param s The modem context.
    \param rate The bit rate of the modem. Valid values are 7200, 9600, 12000 and 14400.
    \param tep TRUE is the optional TEP tone is to be transmitted.
    \param get_bit The callback routine used to get the data to be transmitted.
    \param user_data An opaque pointer.
    \return A pointer to the modem context, or NULL if there was a problem. */
v17_tx_state_t *v17_tx_init(v17_tx_state_t *s, int rate, int tep, get_bit_func_t get_bit, void *user_data);

/*! Reinitialise an existing V.17 modem transmit context, so it may be reused.
    \brief Reinitialise an existing V.17 modem transmit context.
    \param s The modem context.
    \param rate The bit rate of the modem. Valid values are 7200, 9600, 12000 and 14400.
    \param tep TRUE is the optional TEP tone is to be transmitted.
    \param short_train TRUE if the short training sequence should be used.
    \return 0 for OK, -1 for parameter error. */
int v17_tx_restart(v17_tx_state_t *s, int rate, int tep, int short_train);

/*! Release a V.17 modem transmit context.
    \brief Release a V.17 modem transmit context.
    \param s The modem context.
    \return 0 for OK */
int v17_tx_release(v17_tx_state_t *s);

/*! Change the get_bit function associated with a V.17 modem transmit context.
    \brief Change the get_bit function associated with a V.17 modem transmit context.
    \param s The modem context.
    \param get_bit The callback routine used to get the data to be transmitted.
    \param user_data An opaque pointer. */
void v17_tx_set_get_bit(v17_tx_state_t *s, get_bit_func_t get_bit, void *user_data);

/*! Generate a block of V.17 modem audio samples.
    \brief Generate a block of V.17 modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples to be generated.
    \return The number of samples actually generated.
*/
int v17_tx(v17_tx_state_t *s, int16_t *amp, int len);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

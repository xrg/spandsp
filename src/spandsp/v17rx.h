/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v17rx.h - ITU V.17 modem receive part
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
 * $Id: v17rx.h,v 1.8 2005/01/12 13:39:25 steveu Exp $
 */

/*! \file */

#if !defined(_V17RX_H_)
#define _V17RX_H_

#include "fsk.h"

/*! \page V17rx_page The V.17 receiver
\section V17rx_page_sec_1 What does it do
The V.17 receiver implements the receive side of a V.17 modem. This can operate
at data rates of 14400, 12000, 9600 and 7200 bits/second. The audio input is a stream
of 16 bit samples, at 8000 samples/second. The transmit and receive side of V.17
modems operate independantly. V.17 is mostly used for FAX transmission over PSTN
lines, where it provides the standard 14400 bits/second rate. 

\section V17rx_page_sec_2 Theory of operation
V.17 uses QAM modulation, and trellis coding. It specifies a training sequence at
the start of transmission, which makes the design of a V.17 receiver relatively
straightforward. The first stage of the training sequence consists of 256
symbols, alternating between two constellation positions. The receiver monitors
the signal power, to sense the possible presence of a valid carrier. When the
alternating signal begins, the power rising above a minimum threshold (-26dBm0)
causes the main receiver computation to begin. The initial measured power is
used to quickly set the gain of the receiver. After this initial setting, the
front end gain is locked, and the adaptive equalizer tracks any subsequent
signal level variation. The signal is multiplied by a complex carrier, generated
by a DDS, at 8000 samples/second. It is then fed at 24000 samples/second (i.e.
signal, zero, zero, signal, zero, zero, ...) to a root raised cosine pulse
shaping filter. This interpolates the samples, and pulse shapes at the same
time. Every fifth sample is taken from the output of the filter, and fed to an
adaptive equalizer. This means the adaptive equalizer receives samples at 4800
samples/second, so it is a T/2 equalizer. The Gardner algorithm is used to tune
the sampling, so the samples fed to the equalizer are close to the mid point and
edges of each symbol. Initially the algorithm is very lightly damped, to ensure
the symbol alignment pulls in quickly. Because the sampling rate will not be
precisely the same as the transmitter's (the spec. says the symbol timing should
be within 0.01%), the receiver constantly evaluates and corrects this sampling
throughout its operation. During the symbol timing maintainence phase, the
algorithm uses a heavily damped Gardner plus integrate and dump approach to
updates. This heavy damping achieves several things - the Gardner algorithm is
statistically based, so the statistics must be smoothed; a number of samples
must be fed to the equalizer buffer before the equalizer output actually
responds to a step change in the sampling; we need to prevent rapid fluctuations
in the sampling position, due to the optimal position being close to a boundary.

The carrier is specified as 1800Hz +- 1Hz at the transmitter, and 1800 +-7Hz at
the receiver. The receive carrier would only be this inaccurate if the link
includes FDM sections. These are being phased out, but the design must still
allow for the worst case. Using an initial 1800Hz signal for demodulation gives
a worst case rotation rate for the constellation of about one degree per symbol.
Once the Gardner algorithm has been given time to lock to the symbol timing of
the initial alternating pattern, the phase of the demodulated signal is recorded
on two successive symbols - once for each of the constellation positions. The
receiver then tracks the symbol alternations, until a large phase jump occurs.
This signifies the start of the next phase of the training sequence. At this
point the total phase shift between the original recorded symbol phase, and the
symbol phase just before the phase jump occurred is used to provide a coarse
estimation of the rotation rate of the constellation, and it current absolute
angle of rotation. These are used to update the current carrier phase and phase
update rate in the carrier DDS. The working data already in the pulse shaping
filter and equalizer buffers is given a similar step rotation to pull it all
into line. From this point on, a heavily damped integrate and dump approach,
based on the angular difference between each received constellation position and
its expected position, is sufficient to track the carrier, and maintain phase
alignment. A fast rough approximator for the arc-tangent function is adequate
for the estimation of the angular error. 

The next phase of the training sequence is a scrambled sequence of two
particular symbols. We train the T/2 adaptive equalizer using this sequence. The
scrambling makes the signal sufficiently diverse to ensure the equalizer
converges to the proper generalised solution. At the end of this sequence, the
equalizer should be sufficiently well adapted that is can correctly resolve the
full QAM constellation. However, the equalizer continues to adapt throughout
operation of the modem, fine tuning on the more complex data patterns of the
full QAM constellation. 

In the last phase of the training sequence, the modem enters normal data
operation, with a short defined period of all ones as data. As in most high
speed modems, data in a V.17 modem passes through a scrambler, to whiten the
spectrum of the signal. The transmitter should initialise its data scrambler,
and pass the ones through it. At the end of the ones, real data begins to pass
through the scrambler, and the transmit modem is in normal operation. The
receiver tests that ones are really received, in order to verify the modem
trained correctly. If all is well, the data following the ones is fed to the
application, and the receive modem is up and running. Unfortunately, some
transmit side of some real V.17 modems fail to initialise their scrambler before
sending the ones. This means the first 23 received bits (the length of the
scrambler register) cannot be trusted for the test. The receive modem,
therefore, only tests that bits starting at bit 24 are really ones.

The V.17 signal is trellis coded. Two bits of each symbol are convolutionally coded
to form a 3 bit trellis code - the two original bits, plus an extra redundant bit. It
is possible to ignore the trellis coding, and just decode the non-redundant bits.
However, the noise performance of the receiver would suffer. Using a proper
trellis decoder adds several dB to the noise tolerance to the receiving modem. Trellis
coding seems quite complex at first sight, but is fairly straightforward once you
get to grips with it.

Trellis decoding tracks the data in terms of the possible states of the convolutional
coder at the transmitter. There are 8 possible states of the V.17 coder. The first
step in trellis decoding is to find the best candidate constellation point
for each of these 8 states. One of thse will be our final answer. The constellation
has been designed so groups of 8 are spread fairly evenly across it. Locating them
is achieved is a reasonably fast manner, by looking up the answers in a set of space
map tables. The disadvantage is the tables are potentially large enough to affect
cache performance. The trellis decoder works over 16 successive symbols. The result
of decoding is not known until 16 symbols after the data enters the decoder. The
minimum total accumulated mismatch between each received point and the actual
constellation (termed the distance) is assessed for each of the 8 states. A little
analysis of the coder shows that each of the 8 current states could be arrived at
from 4 different previous states, through 4 different constellation bit patterns.
For each new state, the running total distance is arrived at by inspecting a previous
total plus a new distance for the appropriate 4 previous states. The minimum of the 4
values becomes the new distance for the state. Clearly, a mechanism is needed to stop
this distance from growing indefinitely. A sliding window, and several other schemes
are possible. However, a simple single pole IIR is very simple, and provides adequate
results.

For each new state we store the constellation bit pattern, or path, to that state, and
the number of the previous state. We find the minimum distance amongst the 8 new
states for each new symbol. We then trace back through the states, until we reach the
one 16 states ago which leads to the current minimum distance. The bit pattern stored
there is the error corrected bit pattern for that symbol.
*/

#define V17_EQUALIZER_LEN   7  /* this much to the left and this much to the right */
#define V17_EQUALIZER_MASK  15 /* one less than a power of 2 >= (2*V17_EQUALIZER_LEN + 1) */

#define V17RX_FILTER_STEPS  27

/*!
    V.17 modem receive side descriptor. This defines the working state for a
    single instance of a V.17 modem receiver.
*/
typedef struct
{
    /*! \brief The bit rate of the modem. Valid values are 4800, 7200 and 9600. */
    int bit_rate;
    /*! \brief The callback function used to put each bit received. */
    put_bit_func_t put_bit;
    /*! \brief A user specified opaque pointer passed to the callback function. */
    void *user_data;
    /*! \brief A callback function which may be enabled to report every symbol's
               constellation position. */
    qam_report_handler_t *qam_report;
    /*! \brief A user specified opaque pointer passed to the qam_report callback
               function. */
    void *qam_user_data;

    /*! \brief The route raised cosine (RRC) pulse shaping filter buffer. */
    complex_t rrc_filter[2*V17RX_FILTER_STEPS];
    /*! \brief Current offset into the RRC pulse shaping filter buffer. */
    int rrc_filter_step;

    /*! \brief The state of the differential decoder */
    int diff;
    /*! \brief The register for the data scrambler. */
    unsigned int scramble_reg;
    /*! \brief TRUE if the short training sequence is to be used. */
    int short_train;
    int in_training;
    int training_count;
    float training_error;
    int carrier_present;

    /*! \brief The current phase of the carrier (i.e. the DDS parameter). */
    uint32_t carrier_phase;
    /*! \brief The update rate for the phase of the carrier (i.e. the DDS increment). */
    int32_t carrier_phase_rate;
    /*! \brief The carrier update rate saved for reuse when using short training. */
    int32_t carrier_phase_rate_save;
    float carrier_track_p;
    float carrier_track_i;

    /*! \brief The received signal power monitor. */
    power_meter_t power;
    int32_t carrier_on_power;
    int32_t carrier_off_power;
    float agc_scaling;

    float eq_delta;
    /*! \brief The adaptive equalizer coefficients */
    complex_t eq_coeff_save[2*V17_EQUALIZER_LEN + 1];
    complex_t eq_coeff[2*V17_EQUALIZER_LEN + 1];
    complex_t eq_buf[V17_EQUALIZER_MASK + 1];
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
    int baud_phase;

    /*! \brief Starting phase angles for the coarse carrier aquisition step. */
    int32_t start_angles[2];
    /*! \brief History list of phase angles for the coarse carrier aquisition step. */
    int32_t angles[16];
    /*! \brief A pointer to the current constellation. */
    const complex_t *constellation;
    /*! \brief A pointer to the current space map. There is a space map for
               each trellis state. */
    uint8_t (*space_map)[90][8];
    /*! \brief The number of bits in each symbol at the current bit rate. */
    int bits_per_symbol;

    /*! \brief Current pointer to the trellis buffers */
    int trellis_ptr;
    /*! \brief The trellis. */
    int full_path_to_past_state_locations[16][8];
    /*! \brief The trellis. */
    int past_state_locations[16][8];
    /*! \brief Euclidean distances (actually the sqaures of the distances)
               from the last states of the trellis. */
    float distances[8];
} v17_rx_state_t;

extern const complex_t v17_14400_constellation[128];
extern const complex_t v17_12000_constellation[64];
extern const complex_t v17_9600_constellation[32];
extern const complex_t v17_7200_constellation[16];

#ifdef __cplusplus
extern "C" {
#endif

/*! Initialise a V.17 modem receive context.
    \brief Initialise a V.17 modem receive context.
    \param s The modem context.
    \param bit_rate The bit rate of the modem. Valid values are 7200, 9600, 12000 and 14400.
    \param put_bit The callback routine used to put the received data.
    \param user_data An opaque pointer. */
void v17_rx_init(v17_rx_state_t *s, int bit_rate, put_bit_func_t put_bit, void *user_data);

/*! Reinitialise an existing V.17 modem receive context.
    \brief Reinitialise an existing V.17 modem receive context.
    \param s The modem context.
    \param bit_rate The bit rate of the modem. Valid values are 7200, 9600, 12000 and 14400.
    \param short_train TRUE if a short training sequence is expected.
    \return 0 for OK, -1 for bad parameter */
    
int v17_rx_restart(v17_rx_state_t *s, int bit_rate, int short_train);

/*! Process a block of received V.17 modem audio samples.
    \brief Process a block of received V.17 modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
*/
void v17_rx(v17_rx_state_t *s, const int16_t *amp, int len);

/*! Get a snapshot of the current equalizer coefficients.
    \brief Get a snapshot of the current equalizer coefficients.
    \param s The modem context.
    \param coeffs The vector of complex coefficients.
    \return The number of coefficients in the vector. */
int v17_rx_equalizer_state(v17_rx_state_t *s, complex_t **coeffs);

/*! Get a current received carrier frequency.
    \param s The modem context.
    \return The frequency, in Hertz. */
float v17_rx_carrier_frequency(v17_rx_state_t *s);

float v17_rx_symbol_timing_correction(v17_rx_state_t *s);

/*! Get a current received signal power.
    \param s The modem context.
    \return The signal power, in dBm0. */
float v17_rx_signal_power(v17_rx_state_t *s);

void v17_rx_set_qam_report_handler(v17_rx_state_t *s, qam_report_handler_t *handler, void *user_data);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

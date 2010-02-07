/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v18.h - V.18 text telephony for the deaf.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004-2009 Steve Underwood
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
 * $Id: v18.h,v 1.1 2009/04/01 13:22:40 steveu Exp $
 */
 
/*! \file */

/*! \page v18_page The V.18 text telephony protocols
\section v18_page_sec_1 What does it do?

\section v18_page_sec_2 How does it work?
*/

#if !defined(_SPANDSP_V18_H_)
#define _SPANDSP_V18_H_

typedef struct v18_state_s v18_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

SPAN_DECLARE(logging_state_t *) v18_get_logging_state(v18_state_t *s);

/*! Initialise a V.18 context.
    \brief Initialise a V.18 context.
    \param s The V.18 context.
    \param caller TRUE if caller mode, else answerer mode.
    \param mode Mode of operation.
    \return A pointer to the V.18 context, or NULL if there was a problem. */
SPAN_DECLARE(v18_state_t *) v18_init(v18_state_t *s,
                                     int caller,
                                     int mode);

/*! Release a V.18 context.
    \brief Release a V.18 context.
    \param s The V.18 context.
    \return 0 for OK. */
SPAN_DECLARE(int) v18_release(v18_state_t *s);

/*! Free a V.18 context.
    \brief Release a V.18 context.
    \param s The V.18 context.
    \return 0 for OK. */
SPAN_DECLARE(int) v18_free(v18_state_t *s);

/*! Generate a block of V.18 audio samples.
    \brief Generate a block of V.18 audio samples.
    \param s The V.18 context.
    \param amp The audio sample buffer.
    \param max_len The number of samples to be generated.
    \return The number of samples actually generated.
*/
SPAN_DECLARE(int) v18_tx(v18_state_t *s, int16_t *amp, int max_len);

/*! Process a block of received V.18 audio samples.
    \brief Process a block of received V.18 audio samples.
    \param s The V.18 context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
*/
SPAN_DECLARE(int) v18_rx(v18_state_t *s, const int16_t *amp, int len);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

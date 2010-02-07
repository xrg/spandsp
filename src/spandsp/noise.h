/*
 * SpanDSP - a series of DSP components for telephony
 *
 * noise.h - A low complexity audio noise generator, suitable for
 *           real time generation (current just approx AWGN)
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
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
 * $Id: noise.h,v 1.1 2005/10/10 19:42:25 steveu Exp $
 */

/*! \file */

#if !defined(_NOISE_H_)
#define _NOISE_H_

/*! \page noise_page Noise generation

\section noise_page_sec_1 What does it do?
It generates audio noise. Currently it only generates reasonable quality
AWGN. It is designed to be of sufficiently low complexity to generate large
volumes of reasonable quality noise, in real time.

\section awgn_page_sec_2 How does it work?
The central limit theorem says if you add a few random numbers together,
the result starts to look Gaussian. In this case we sum 8 random numbers.
The result is fast, and perfectly good as a noise source for many purposes.
It should not be trusted as a high quality AWGN generator, for elaborate
modelling purposes.
*/

#define NOISE_CLASS_AWGN    1

/*!
    Noise generator descriptor. This contains all the state information for an instance
    of the noise generator.
 */
typedef struct
{
    int32_t rms;
    uint32_t rndnum;
} noise_state_t;

#ifdef __cplusplus
extern "C" {
#endif

/*! Initialise an audio noise generator.
    \brief Initialise an audio noise generator.
    \param s The noise generator context.
    \param seed A seed for the underlying random number generator.
    \param level The noise power level in dBmO.
    \param class The class of noise (e.g. AWGN).
*/
void noise_init(noise_state_t *s, int seed, int level, int class);

/*! Generate a sample of audio noise.
    \brief Generate a sample of audio noise.
    \param s The noise generator context.
    \return The generated sample.
*/
int16_t noise(noise_state_t *s);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

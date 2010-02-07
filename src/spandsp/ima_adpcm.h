/*
 * SpanDSP - a series of DSP components for telephony
 *
 * imaadpcm.c - Conversion routines between linear 16 bit PCM data and
 *		        IMA/DVI/Intel ADPCM format.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
 *
 * Based on a bit from here, a bit from there, eye of toad,
 * ear of bat, etc - plus, of course, my own 2 cents.
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
 * $Id: ima_adpcm.h,v 1.2 2005/01/18 14:05:49 steveu Exp $
 */

/*! \file */

#if !defined(_IMA_ADPCM_H_)
#define _IMA_ADPCM_H_

/*! \page IMA_ADPCM_page IMA/DVI/Intel ADPCM encoding and decoding
\section IMA_ADPCM_page_sec_1 What does it do?
IMA ADPCM offers a good balance of simplicity and quality at a rate of
32kbps.

\section IMA_ADPCM_page_sec_2 How does it work?

\section IMA_ADPCM_page_sec_3 How do I use it?
*/

/*!
    IMA (DVI/Intel) ADPCM conversion state descriptor. This defines the state of
    a single working instance of the IMA ADPCM converter. This is used for
    either linear to ADPCM or ADPCM to linear conversion.
*/
typedef struct
{
    int16_t last;
    int16_t step_index;
    uint8_t ima_byte;
    int mark;
} ima_adpcm_state_t;

#ifdef __cplusplus
extern "C" {
#endif

/*! Create an IMA ADPCM encode or decode context.
    \return The newly created context, or NULL for error. */
ima_adpcm_state_t *ima_adpcm_create(void);

/*! Free an IMA ADPCM encode or decode context.
    \param s The IMA ADPCM context.
    \return 0 for OK. */
int ima_adpcm_free(ima_adpcm_state_t *s);

/*! Decode a buffer of IMA ADPCM data to linear PCM.
    \param s The IMA ADPCM context.
    \param amp
    \param ima_data
    \param ima_bytes
    \return The number of samples returned. */
int ima_adpcm_to_linear(ima_adpcm_state_t *s,
                        int16_t *amp,
                        const uint8_t *ima_data,
                        int ima_bytes);

/*! Encode a buffer of linear PCM data to IMA ADPCM.
    \param s The IMA ADPCM context.
    \param ima_data
    \param amp
    \param samples
    \return The number of bytes of IMA ADPCM data produced. */
int ima_linear_to_adpcm(ima_adpcm_state_t *s,
                        uint8_t *ima_data,
                        const int16_t *amp,
                        int samples);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

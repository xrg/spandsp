/*
 * SpanDSP - a series of DSP components for telephony
 *
 * okiadpcm.c - Conversion routines between linear 16 bit PCM data and
 *		OKI (Dialogic) ADPCM format.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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
 * $Id: oki_adpcm.h,v 1.2 2004/03/15 13:17:36 steveu Exp $
 */

/*! \file */

#if !defined(_OKI_ADPCM_H_)
#define _OKI_ADPCM_H_

/*!
    OKI (Dialogic) ADPCM conversion state descriptor. This defines the state of
    a single working instance of the OKI ADPCM converter. This is used for
    either linear to ADPCM or ADPCM to linear conversion.
*/
typedef struct
{
    int16_t last;
    int16_t step_index;
    int16_t left_over_sample;
    int8_t  left_over_used;
} oki_adpcm_state_t;

oki_adpcm_state_t *oki_adpcm_create(void);
void oki_adpcm_free(oki_adpcm_state_t *oki);
int oki_adpcm_to_linear(oki_adpcm_state_t *oki,
                        int16_t *amp,
                        const uint8_t oki_data[],
                        int oki_bytes);
int linear_to_oki_adpcm(oki_adpcm_state_t *oki,
                        uint8_t oki_data[],
                        const int16_t *amp,
                        int pcm_samples);
#endif
/*- End of file ------------------------------------------------------------*/

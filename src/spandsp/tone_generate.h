/*
 * SpanDSP - a series of DSP components for telephony
 *
 * tone_generate.h - General telephony tone generation, and specific
 *                   generation of DTMF, and network supervisory tones.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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
 * $Id: tone_generate.h,v 1.4 2004/03/19 19:12:46 steveu Exp $
 */

/*! \file */

#if !defined(_TONE_GENERATE_H_)
#define _TONE_GENERATE_H_

/*! \page tone_generation_page Tone generation

The tone generation module provides for the generation of cadenced tones,
suitable for a wide range of telephony applications. 

\section tone_generation_page_sec_1 Theory of operation

Oscillators are a problem. They oscillate due to instability, and yet we need
them to behave in a stable manner. A look around the web will reveal many papers
on this subject. Many describe rather complex solutions to the problem. However,
we are only concerned with telephony applications. It is possible to generate
the tones we need with a very simple efficient scheme. It is also practical to
use an exhaustive test to prove the oscillator is stable under all the
conditions in which we will use it. 
*/

#if !defined(MAX_DTMF_DIGITS)
#define MAX_DTMF_DIGITS 128
#endif

/*!
    Cadenced dual tone generator descriptor.
*/
typedef struct
{
#if defined(PURE_INTEGER_DSP)
    int v2_1;
    int v3_1;
    int fac_1;

    int v2_2;
    int v3_2;
    int fac_2;
#else
    float v2_1;
    float v3_1;
    float fac_1;

    float v2_2;
    float v3_2;
    float fac_2;
#endif
    
    int duration[4];

    int repeat;
} tone_gen_descriptor_t;

/*!
    Cadenced dual tone generator state descriptor. This defines the state of
    a single working instance of a generator.
*/
typedef struct
{
#if defined(PURE_INTEGER_DSP)
    int v2_1;
    int v3_1;
    int fac_1;

    int v2_2;
    int v3_2;
    int fac_2;
#else
    float v2_1;
    float v3_1;
    float fac_1;

    float v2_2;
    float v3_2;
    float fac_2;
#endif
    int duration[4];
    
    int repeat;

    int current_section;
    int current_position;
} tone_gen_state_t;

typedef enum
{
    BELL_MF_TONES,
    R2_MF_TONES,
    SOCOTEL_TONES
} mf_tone_types_e;

/*!
    DTMF generator state descriptor. This defines the state of a single
    working instance of a DTMF generator.
*/
typedef struct
{
    char *tone_codes;
    tone_gen_descriptor_t *tone_descriptors;
    tone_gen_state_t tones;
    char digits[MAX_DTMF_DIGITS + 1];
    int current_sample;
    int current_digits;
} dtmf_tx_state_t;

typedef struct
{
    int         f1;         /* First freq */
    int         f2;         /* Second freq */
    int8_t      level1;     /* Level of the first freq (dB) */
    int8_t      level2;     /* Level of the second freq (dB) */
    uint16_t    on_time1;   /* Tone on time (ms) */
    uint16_t    off_time1;  /* Minimum post tone silence (ms) */
    uint16_t    on_time2;   /* Tone on time (ms) */
    uint16_t    off_time2;  /* Minimum post tone silence (ms) */
    int8_t      repeat;     /* True if cyclic tone, false for one shot. */
} cadenced_tone_t;

void make_tone_descriptor(tone_gen_descriptor_t *desc, cadenced_tone_t *tone);
void make_tone_gen_descriptor(tone_gen_descriptor_t *s,
                              int f1,
                              int l1,
                              int f2,
                              int l2,
                              int d1,
                              int d2,
                              int d3,
                              int d4,
                              int repeat);

void tone_gen_init(tone_gen_state_t *s, tone_gen_descriptor_t *t);
int tone_gen(tone_gen_state_t *s, int16_t *amp, int max_samples);

/*! \brief Initialise DTMF tone generation. This should be called before
           any other use of the DTMF tone features.
*/
void dtmf_gen_init(void);

/*! \brief Initialise a DTMF tone generator context.
    \param s The DTMF generator context.
*/
void dtmf_tx_init(dtmf_tx_state_t *s);

/*! \brief Generate a buffer of DTMF tones.
    \param s The DTMF generator context.
    \param amp The buffer for the generated signal.
    \param max_samples The required number of generated samples.
    \return The number of samples actually generated. This may be less than 
            samples if the input buffer empties.
*/
int dtmf_tx(dtmf_tx_state_t *s, int16_t *amp, int max_samples);

/*! \brief Put a string of digits in a DTMF generator's input buffer.
    \param s The DTMF generator context.
    \param digits The string of digits to be added.
    \return The number of digits actually added. This may be less than the
            length of the digit string, if the buffer fills up.
*/
int dtmf_put(dtmf_tx_state_t *s, const char *digits);

/*! \brief Initialise Bell MF tone generation. This should be called before
           any other use of the Bell MF tone features.
*/
void bell_mf_gen_init(void);

/*! \brief Initialise a Bell MF tone generator context.
    \param s The Bell MF generator context (same type as a DTMF context).
*/
void bell_mf_tx_init(dtmf_tx_state_t *s);

/*! \brief Initialise MFC/R2 tone generation. This should be called before
           any other use of the MFC/R2 tone features.
*/
void r2_mf_tx_init(void);

/*! \brief Generate a buffer of MFC/R2 tones.
    \param s The tone generate context.
    \param amp The buffer for the generated signal.
    \param samples The required number of generated samples.
    \param fwd TRUE to use the forward tone set. FALSE to use the reverse tone set.
    \param digit The digit to be generated. When continuing to generate the same
           digit as during the last call to this function, digit should be set to 0x7F.
    \return The number of samples actually generated.
*/
int r2_mf_tx(tone_gen_state_t *s, int16_t *amp, int samples, int fwd, char digit);

#endif
/*- End of file ------------------------------------------------------------*/

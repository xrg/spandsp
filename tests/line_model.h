/*
 * SpanDSP - a series of DSP components for telephony
 *
 * line_model.h - Model a telephone line.
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
 * $Id: line_model.h,v 1.3 2005/03/03 14:19:00 steveu Exp $
 */

/*! \file */

/*! \page line_model_page Telephone line model
\section line_model_page_sec_1 What does it do?
The telephone line modelling module provides simple modelling of one way and two
way telephone lines.

The path being modelled is:

    -    terminal
    -      | < hybrid echo (2-way models)
    -      |
    -      | < noise and filtering
    -      |
    -      | < hybrid echo (2-way models)
    -     CO
    -      |
    -      | < A-law distortion + bulk delay
    -      |
    -     CO
    -      | < hybrid echo (2-way models)
    -      |
    -      | < noise and filtering
    -      |
    -      | < hybrid echo (2-way models)
    -    terminal
*/

#if !defined(_LINE_MODEL_H_)
#define _LINE_MODEL_H_

#define LINE_FILTER_SIZE 129

typedef struct
{
    int alaw_munge;

    float *line_filter;
    int line_filter_len;
    float buf[LINE_FILTER_SIZE];    /* last transmitted samples (ring buffer, 
                                       used by the line filter) */
    int buf_ptr;                    /* pointer of the last transmitted sample in buf */
    float cpe_hybrid_echo;
    float co_hybrid_echo;
    int16_t bulk_delay_buf[8000];
    int bulk_delay;
    int bulk_delay_ptr;
    awgn_state_t noise;
} one_way_line_model_state_t;

typedef struct
{
    one_way_line_model_state_t line1;
    one_way_line_model_state_t line2;
    float fout1;
    float fout2; 
} both_ways_line_model_state_t;

#ifdef __cplusplus
extern "C" {
#endif

void both_ways_line_model(both_ways_line_model_state_t *s, 
                          int16_t *output1,
                          const int16_t *input1,
                          int16_t *output2,
                          const int16_t *input2,
                          int samples);

both_ways_line_model_state_t *both_ways_line_model_init(int model1,
                                                        int noise1,
                                                        int model2,
                                                        int noise2);

void one_way_line_model(one_way_line_model_state_t *s, 
                        int16_t *output,
                        const int16_t *input,
                        int samples);

one_way_line_model_state_t *one_way_line_model_init(int model, int noise);
#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

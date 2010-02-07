/*
 * SpanDSP - a series of DSP components for telephony
 *
 * line_model.c - Model a telephone line.
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
 * $Id: line_model.c,v 1.7 2005/09/01 17:06:45 steveu Exp $
 */

#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <audiofile.h>
#include <tiffio.h>

#define GEN_CONST
#include <math.h>

#include "spandsp.h"
#include "spandsp/g168models.h"

#include "line_model.h"
#include "line_models.h"

#if !defined(NULL)
#define NULL (void *) 0
#endif

float null_line_model[] =
{
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        1.0
};

static float *models[] =
{
    null_line_model,
    proakis_line_model,
    ad_1_edd_1_model,
    ad_1_edd_2_model,
    ad_1_edd_3_model,
    ad_5_edd_1_model,
    ad_5_edd_2_model,
    ad_5_edd_3_model,
    ad_6_edd_1_model,
    ad_6_edd_2_model,
    ad_6_edd_3_model,
    ad_7_edd_1_model,
    ad_7_edd_2_model,
    ad_7_edd_3_model,
    ad_8_edd_1_model,
    ad_8_edd_2_model,
    ad_8_edd_3_model,
    ad_9_edd_1_model,
    ad_9_edd_2_model,
    ad_9_edd_3_model
};

static float calc_line_filter(one_way_line_model_state_t *s, float v)
{
    float sum;
    float noise;
    int j;
    int p;

    /* Add the sample in the filter buffer */
    p = s->buf_ptr;
    s->buf[p] = v;
    if (++p == s->line_filter_len)
        p = 0;
    s->buf_ptr = p;
    
    /* Apply the filter */
    sum = 0;
    for (j = 0;  j < s->line_filter_len;  j++)
    {
        sum += s->line_filter[j]*s->buf[p];
        if (++p >= s->line_filter_len)
            p = 0;
    }
    
    /* Add noise */
    noise = awgn(&s->noise);
    sum += noise;
    
    return sum;
}
/*- End of function --------------------------------------------------------*/

void one_way_line_model(one_way_line_model_state_t *s, 
                        int16_t *output,
                        const int16_t *input,
                        int samples)
{
    int i;
    float in;
    float out;
    float out1;

    /* The path being modelled is:
        terminal
          | < hybrid
          |
          | < noise and filtering
          |
          | < hybrid
         CO
          |
          | < A-law distortion + bulk delay
          |
         CO
          | < hybrid
          |
          | < noise and filtering
          |
          | < hybrid
        terminal
     */
    for (i = 0;  i < samples;  i++)
    {
        in = input[i];

        /* Line model filters & noise */
        out = calc_line_filter(s, in);
        /* Introduce distortion due to A-law munging in the long distance digital link.
           The effects of u-law should be very similar, so a separate test for
           this is not needed. */
        if (s->alaw_munge)
            out = alaw_to_linear(linear_to_alaw(out));
        /* Introduce the bulk delay of the long distance digital link. */
        out1 = s->bulk_delay_buf[s->bulk_delay_ptr];
        s->bulk_delay_buf[s->bulk_delay_ptr] = out;
        if (++s->bulk_delay_ptr >= s->bulk_delay)
            s->bulk_delay_ptr = 0;
        output[i] = out1;
    }
}
/*- End of function --------------------------------------------------------*/

void both_ways_line_model(both_ways_line_model_state_t *s, 
                          int16_t *output1,
                          const int16_t *input1,
                          int16_t *output2,
                          const int16_t *input2,
                          int samples)
{
    int i;
    float in1;
    float in2;
    float out1;
    float out2;
    float tmp1;
    float tmp2;

    /* The path being modelled is:
        terminal
          | < hybrid echo
          |
          | < noise and filtering
          |
          | < hybrid echo
         CO
          |
          | < A-law distortion + bulk delay
          |
         CO
          | < hybrid echo
          |
          | < noise and filtering
          |
          | < hybrid echo
        terminal
     */
    for (i = 0;  i < samples;  i++)
    {
        in1 = input1[i];
        in2 = input2[i];

        /* Echo from each modem's CO hybrid */
        tmp1 = in1 + s->fout2*s->line1.co_hybrid_echo;
        tmp2 = in2 + s->fout1*s->line2.co_hybrid_echo;

        /* Line model filters & noise */
        s->fout1 = calc_line_filter(&s->line1, tmp1);
        s->fout2 = calc_line_filter(&s->line2, tmp2);

        /* Introduce distortion due to A-law munging in the long distance digital link.
           The effects of u-law should be very similar, so a separate test for
           this is not needed. */
        if (s->line1.alaw_munge)
            s->fout1 = alaw_to_linear(linear_to_alaw(s->fout1));
        if (s->line2.alaw_munge)
            s->fout2 = alaw_to_linear(linear_to_alaw(s->fout2));

        /* Introduce the bulk delay of the long distance digital link. */
        out1 = s->line1.bulk_delay_buf[s->line1.bulk_delay_ptr];
        s->line1.bulk_delay_buf[s->line1.bulk_delay_ptr] = s->fout1;
        if (++s->line1.bulk_delay_ptr >= s->line1.bulk_delay)
            s->line1.bulk_delay_ptr = 0;

        out2 = s->line2.bulk_delay_buf[s->line2.bulk_delay_ptr];
        s->line2.bulk_delay_buf[s->line2.bulk_delay_ptr] = s->fout2;
        if (++s->line2.bulk_delay_ptr >= s->line2.bulk_delay)
            s->line2.bulk_delay_ptr = 0;

        /* Echo from each modem's own hybrid */
        out1 += in2*s->line1.cpe_hybrid_echo;
        out2 += in1*s->line2.cpe_hybrid_echo;
        output1[i] = fsaturate(out1);
        output2[i] = fsaturate(out2);
    }
}
/*- End of function --------------------------------------------------------*/

one_way_line_model_state_t *one_way_line_model_init(int model, int noise)
{
    float p;
    int i;
    one_way_line_model_state_t *s;

    if ((s = (one_way_line_model_state_t *) malloc(sizeof(*s))) == NULL)
        return NULL;
    memset(s, 0, sizeof(*s));

    s->bulk_delay = 8;
    s->bulk_delay_ptr = 0;

    s->alaw_munge = TRUE;

    s->line_filter = models[model];
    s->line_filter_len = 129;

    awgn_init(&s->noise, 1234567, noise);
    return s;
}
/*- End of function --------------------------------------------------------*/

both_ways_line_model_state_t *both_ways_line_model_init(int model1,
                                                        int noise1,
                                                        int model2,
                                                        int noise2)
{
    float p;
    float echo_level;
    int i;
    both_ways_line_model_state_t *s;

    if ((s = (both_ways_line_model_state_t *) malloc(sizeof(*s))) == NULL)
        return NULL;
    memset(s, 0, sizeof(*s));

    s->line1.alaw_munge = TRUE;
    s->line2.alaw_munge = TRUE;

    s->line1.bulk_delay = 8;
    s->line2.bulk_delay = 8;

    s->line1.bulk_delay_ptr = 0;
    s->line2.bulk_delay_ptr = 0;

    s->line1.line_filter = models[model1];
    s->line1.line_filter_len = 129;
    s->line2.line_filter = models[model2];
    s->line2.line_filter_len = 129;

    awgn_init(&s->line1.noise, 1234567, noise1);
    awgn_init(&s->line2.noise, 7654321, noise2);

    /* Echos */
    echo_level = -15; /* in dB */
    s->line1.co_hybrid_echo = pow(10, echo_level/20.0);
    s->line2.co_hybrid_echo = pow(10, echo_level/20.0);
    s->line1.cpe_hybrid_echo = pow(10, echo_level/20.0);
    s->line2.cpe_hybrid_echo = pow(10, echo_level/20.0);
    
    return s;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

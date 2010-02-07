/*
 * SpanDSP - a series of DSP components for telephony
 *
 * bert.c - Bit error rate tests.
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
 * $Id: bert.c,v 1.5 2004/11/05 19:28:50 steveu Exp $
 */

#define	_ISOC9X_SOURCE	1
#define _ISOC99_SOURCE	1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#include <tiffio.h>

#include "spandsp.h"

int bert_get_bit(bert_state_t *s)
{
    int bit;

    switch (s->pattern_class)
    {
    case 0:
        bit = s->tx_reg & 1;
        s->tx_reg = (s->tx_reg >> 1) | ((s->tx_reg & 1) << s->shift2);
        break;
    case 1:
        bit = s->tx_reg & 1;
        s->tx_reg = (s->tx_reg >> 1) | (((s->tx_reg ^ (s->tx_reg >> s->shift)) & 1) << s->shift2);
        if (s->max_zeros)
        {
            /* This generator suppresses runs >s->max_zeros */
            if (bit)
            {
                if (++s->tx_zeros > s->max_zeros)
                {
                    s->tx_zeros = 0;
                    bit ^= 1;
                }
            }
            else
            {
                s->tx_zeros = 0;
            }
        }
        bit ^= s->invert;
        break;
    case 2:
        break;
    }
    s->tx_bits++;
    return bit;
}
/*- End of function --------------------------------------------------------*/

void bert_put_bit(bert_state_t *s, int bit)
{
    int i;
    int j;
    int len;
    int sum;
    int test;

    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_TRAINING_FAILED:
            printf("Training failed\n");
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            printf("Training succeeded\n");
            break;
        case PUTBIT_CARRIER_UP:
            printf("Carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            printf("Carrier down\n");
            break;
        default:
            printf("Eh!\n");
            break;
        }
        return;
    }
    bit = (bit & 1) ^ s->invert;
    s->rx_bits++;
    switch (s->pattern_class)
    {
    case 0:
        if (s->resync)
        {
            s->rx_reg = (s->rx_reg >> 1) | (bit << s->shift2);
            s->ref_reg = (s->ref_reg >> 1) | ((s->ref_reg & 1) << s->shift2);
            if (s->rx_reg == s->ref_reg)
            {
                if (++s->resync > s->resync_time)
                {
                    s->resync = 0;
                    if (s->reporter)
                        s->reporter(s->user_data, BERT_REPORT_SYNCED);
                }
            }
            else
            {
                s->resync = 2;
                s->ref_reg = s->master_reg;
            }
        }
        else
        {
            s->total_bits++;
            if ((bit ^ s->ref_reg) & 1)
                s->bad_bits++;
            s->ref_reg = (s->ref_reg >> 1) | ((s->ref_reg & 1) << s->shift2);
        }
        break;
    case 1:
        if (s->resync)
        {
            /* If we get a reasonable period for which we correctly predict the
               next bit, we must be in sync. */
            /* Don't worry about max. zeros tests when resyncing.
               It might just extend the resync time a little. Trying
               to include the test might affect robustness. */
            if (bit == ((s->rx_reg >> s->shift) & 1))
            {
                if (++s->resync > s->resync_time)
                {
                    s->resync = 0;
                    if (s->reporter)
                        s->reporter(s->user_data, BERT_REPORT_SYNCED);
                }
            }
            else
            {
                s->resync = 2;
                s->rx_reg ^= s->mask;
            }
        }
        else
        {
            if (s->max_zeros)
            {
                if ((s->rx_reg & s->mask))
                {
                    if (++s->rx_zeros > s->max_zeros)
                    {
                        s->rx_zeros = 0;
                        bit ^= 1;
                    }
                }
                else
                {
                    s->rx_zeros = 0;
                }
            }
            s->total_bits++;
            if (bit != ((s->rx_reg >> s->shift) & 1))
            {
                s->bad_bits++;
                s->resync_bad_bits++;
                s->decade_bad[2][s->decade_ptr[2]]++;
            }
            if (--s->step <= 0)
            {
                s->step = 100;
                test = TRUE;
                for (i = 2;  i <= 7;  i++)
                {
                    if (++s->decade_ptr[i] < 10)
                        break;
                    s->decade_ptr[i] = 0;
                    for (sum = 0, j = 0;  j < 10;  j++)
                        sum += s->decade_bad[i][j];
                    if (test  &&  sum > 10)
                    {
                        test = FALSE;
                        if (s->error_rate != i  &&  s->reporter)
                            s->reporter(s->user_data, BERT_REPORT_GT_10_2 + i - 2);
                        s->error_rate = i;
                    }
                    s->decade_bad[i][0] = 0;
                    if (i < 7)
                        s->decade_bad[i + 1][s->decade_ptr[i + 1]] = sum;
                }
                if (i > 7)
                {
                    if (s->decade_ptr[i] >= 10)
                        s->decade_ptr[i] = 0;
                    if (test)
                    {
                        if (s->error_rate != i  &&  s->reporter)
                            s->reporter(s->user_data, BERT_REPORT_GT_10_2 + i - 2);
                        s->error_rate = i;
                    }
                }
                else
                {
                    s->decade_bad[i][s->decade_ptr[i]] = 0;
                }
            }
            if (--s->resync_cnt <= 0)
            {
                /* Check if there were enough bad bits during this period to
                   justify a resync. */
                if (s->resync_bad_bits >= (s->resync_len*s->resync_percent)/100)
                {
                    s->resync = 1;
                    s->resyncs++;
                    if (s->reporter)
                        s->reporter(s->user_data, BERT_REPORT_UNSYNCED);
                }
                s->resync_cnt = s->resync_len;
                s->resync_bad_bits = 0;
            }
        }
        s->rx_reg = (s->rx_reg >> 1) | (((s->rx_reg ^ (s->rx_reg >> s->shift)) & 1) << s->shift2);
        break;
    case 2:
        break;
    }
    if (s->report_frequency > 0)
    {
        if (--s->report_countdown <= 0)
        {
            if (s->reporter)
                s->reporter(s->user_data, BERT_REPORT_REGULAR);
            s->report_countdown = s->report_frequency;
        }
    }
}
/*- End of function --------------------------------------------------------*/

int bert_result(bert_state_t *s, bert_results_t *results)
{
    results->total_bits = s->total_bits;
    results->bad_bits = s->bad_bits;
    results->resyncs = s->resyncs;
    return sizeof(*results);
}
/*- End of function --------------------------------------------------------*/

int bert_set_report(bert_state_t *s, int freq, bert_report_func_t reporter, void *user_data)
{
    s->report_frequency = freq;
    s->reporter = reporter;
    s->user_data = user_data;
    
    s->report_countdown = s->report_frequency;
}
/*- End of function --------------------------------------------------------*/

int bert_init(bert_state_t *s, int limit, int pattern, int resync_len, int resync_percent)
{
    int i;
    int j;

    s->pattern = pattern;
    s->limit = limit;
    s->reporter = NULL;
    s->user_data = NULL;
    s->report_frequency = 0;

    s->resync_time = 72;
    s->invert = 0;
    switch (s->pattern)
    {
    case BERT_PATTERN_ZEROS:
        s->tx_reg = 0;
        s->shift2 = 31;
        s->pattern_class = 0;
        break;
    case BERT_PATTERN_ONES:
        s->tx_reg = ~0;
        s->shift2 = 31;
        s->pattern_class = 0;
        break;
    case BERT_PATTERN_7_TO_1:
        s->tx_reg = 0xFEFEFEFE;
        s->shift2 = 31;
        s->pattern_class = 0;
        break;
    case BERT_PATTERN_3_TO_1:
        s->tx_reg = 0xEEEEEEEE;
        s->shift2 = 31;
        s->pattern_class = 0;
        break;
    case BERT_PATTERN_1_TO_1:
        s->tx_reg = 0xAAAAAAAA;
        s->shift2 = 31;
        s->pattern_class = 0;
        break;
    case BERT_PATTERN_1_TO_3:
        s->tx_reg = 0x11111111;
        s->shift2 = 31;
        s->pattern_class = 0;
        break;
    case BERT_PATTERN_1_TO_7:
        s->tx_reg = 0x01010101;
        s->shift2 = 31;
        s->pattern_class = 0;
        break;
    case BERT_PATTERN_QBF:
        s->pattern_class = 2;
        break;
    case BERT_PATTERN_ITU_O151_23:
        s->pattern_class = 1;
        s->tx_reg = 0x7FFFFF;
        s->mask = 0x20;
        s->shift = 5;
        s->shift2 = 22;
        s->invert = 1;
        s->resync_time = 56;
        s->max_zeros = 0;
        break;
    case BERT_PATTERN_ITU_O151_20:
        s->pattern_class = 1;
        s->tx_reg = 0xFFFFF;
        s->mask = 0x8;
        s->shift = 3;
        s->shift2 = 19;
        s->invert = 1;
        s->resync_time = 50;
        s->max_zeros = 14;
        break;
    case BERT_PATTERN_ITU_O151_15:
        s->pattern_class = 1;
        s->tx_reg = 0x7FFF;
        s->mask = 0x2;
        s->shift = 1;
        s->shift2 = 14;
        s->invert = 1;
        s->resync_time = 40;
        s->max_zeros = 0;
        break;
    case BERT_PATTERN_ITU_O152_11:
        s->pattern_class = 1;
        s->tx_reg = 0x7FF;
        s->mask = 0x4;
        s->shift = 2;
        s->shift2 = 10;
        s->invert = 0;
        s->resync_time = 32;
        s->max_zeros = 0;
        break;
    case BERT_PATTERN_ITU_O153_9:
        s->pattern_class = 1;
        s->tx_reg = 0x1FF;
        s->mask = 0x10;
        s->shift = 4;
        s->shift2 = 8;
        s->invert = 0;
        s->resync_time = 28;
        s->max_zeros = 0;
        break;
    }
    s->tx_bits = 0;

    s->rx_reg = s->tx_reg;
    s->ref_reg = s->rx_reg;
    s->master_reg = s->ref_reg;
    s->rx_bits = 0;

    s->resync = 1;
    s->total_bits = 0;
    s->bad_bits = 0;
    s->resync_cnt = resync_len;
    s->resync_bad_bits = 0;
    s->resync_len = resync_len;
    s->resync_percent = resync_percent;
    s->resyncs = 0;

    s->report_countdown = 0;

    for (i = 0;  i < 8;  i++)
    {
        for (j = 0;  j < 10;  j++)
            s->decade_bad[i][j] = 0;
        s->decade_ptr[i] = 0;
    }
    s->error_rate = 8;
    s->step = 100;
    
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

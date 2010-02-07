/*
 * SpanDSP - a series of DSP components for telephony
 *
 * bert_tests.c - Tests for the BER tester.
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
 * $Id: bert_tests.c,v 1.10 2005/12/29 09:54:24 steveu Exp $
 */

/*! \file */

/*! \page bert_tests_page BERT tests
\section bert_tests_page_sec_1 What does it do?
These tests exercise each of the BERT standards supported by the BERT module.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"

void reporter(void *user_data, int reason)
{
    bert_state_t *s;
    bert_results_t bert_results;

    s = (bert_state_t *) user_data;
    switch (reason)
    {
    case BERT_REPORT_SYNCED:
        printf("BERT report synced\n");
        break;
    case BERT_REPORT_UNSYNCED:
        printf("BERT report unsync'ed\n");
        break;
    case BERT_REPORT_REGULAR:
        bert_result(s, &bert_results);
        printf("BERT report regular - %d bits, %d bad bits, %d resyncs\r", bert_results.total_bits, bert_results.bad_bits, bert_results.resyncs);
        break;
    case BERT_REPORT_GT_10_2:
        printf("BERT report > 1 in 10^2\n");
        break;
    case BERT_REPORT_LT_10_2:
        printf("BERT report < 1 in 10^2\n");
        break;
    case BERT_REPORT_LT_10_3:
        printf("BERT report < 1 in 10^3\n");
        break;
    case BERT_REPORT_LT_10_4:
        printf("BERT report < 1 in 10^4\n");
        break;
    case BERT_REPORT_LT_10_5:
        printf("BERT report < 1 in 10^5\n");
        break;
    case BERT_REPORT_LT_10_6:
        printf("BERT report < 1 in 10^6\n");
        break;
    case BERT_REPORT_LT_10_7:
        printf("BERT report < 1 in 10^7\n");
        break;
    default:
        printf("BERT report reason %d\n", reason);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

int8_t test[0x800000];

int main(int argc, char *argv[])
{
    bert_state_t tx_bert;
    bert_state_t rx_bert;
    bert_state_t bert;
    bert_results_t bert_results;
    int i;
    int j;
    int bit;
    int zeros;
    int max_zeros;

    bert_init(&tx_bert, 0, BERT_PATTERN_ZEROS, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_ZEROS, 300, 20);
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);        
    }
    printf("Zeros:     Bad bits %d/%d\n", rx_bert.bad_bits, rx_bert.total_bits);


    bert_init(&tx_bert, 0, BERT_PATTERN_ONES, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_ONES, 300, 20);
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);        
    }
    printf("Ones:      Bad bits %d/%d\n", rx_bert.bad_bits, rx_bert.total_bits);

    bert_init(&tx_bert, 0, BERT_PATTERN_1_TO_7, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_1_TO_7, 300, 20);
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);
    }
    printf("1 to 7:    Bad bits %d/%d\n", rx_bert.bad_bits, rx_bert.total_bits);


    bert_init(&tx_bert, 0, BERT_PATTERN_1_TO_3, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_1_TO_3, 300, 20);
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);        
    }
    printf("1 to 3:    Bad bits %d/%d\n", rx_bert.bad_bits, rx_bert.total_bits);


    bert_init(&tx_bert, 0, BERT_PATTERN_1_TO_1, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_1_TO_1, 300, 20);
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);
    }
    printf("1 to 1:    Bad bits %d/%d\n", rx_bert.bad_bits, rx_bert.total_bits);


    bert_init(&tx_bert, 0, BERT_PATTERN_3_TO_1, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_3_TO_1, 300, 20);
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);
    }
    printf("3 to 1:    Bad bits %d/%d\n", rx_bert.bad_bits, rx_bert.total_bits);
    

    bert_init(&tx_bert, 0, BERT_PATTERN_7_TO_1, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_7_TO_1, 300, 20);
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);
    }
    printf("7 to 1:    Bad bits %d/%d\n", rx_bert.bad_bits, rx_bert.total_bits);


    bert_init(&tx_bert, 0, BERT_PATTERN_ITU_O153_9, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_ITU_O153_9, 300, 20);
    for (i = 0;  i < 0x200;  i++)
        test[i] = 0;
    max_zeros = 0;
    zeros = 0;
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        if (bit)
        {
            if (zeros > max_zeros)
                max_zeros = zeros;
            zeros = 0;
        }
        else
        {
            zeros++;
        }
        bert_put_bit(&rx_bert, bit);        
        test[tx_bert.tx_reg]++;
    }
    if (test[0] != 0)
        printf("XXX %d %d\n", 0, test[0]);
    for (i = 1;  i < 0x200;  i++)
    {
        if (test[i] != 2)
            printf("XXX %d %d\n", i, test[i]);
    }
    printf("O.153(9):  Bad bits %d/%d, max zeros %d\n", rx_bert.bad_bits, rx_bert.total_bits, max_zeros);


    bert_init(&tx_bert, 0, BERT_PATTERN_ITU_O152_11, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_ITU_O152_11, 300, 20);
    for (i = 0;  i < 0x800;  i++)
        test[i] = 0;
    max_zeros = 0;
    zeros = 0;
    for (i = 0;  i < 2047*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        if (bit)
        {
            if (zeros > max_zeros)
                max_zeros = zeros;
            zeros = 0;
        }
        else
        {
            zeros++;
        }
        bert_put_bit(&rx_bert, bit);        
        test[tx_bert.tx_reg]++;
    }
    if (test[0] != 0)
        printf("XXX %d %d\n", 0, test[0]);
    for (i = 1;  i < 0x800;  i++)
    {
        if (test[i] != 2)
            printf("XXX %d %d\n", i, test[i]);
    }
    printf("O.152(11): Bad bits %d/%d, max zeros %d\n", rx_bert.bad_bits, rx_bert.total_bits, max_zeros);


    bert_init(&tx_bert, 0, BERT_PATTERN_ITU_O151_15, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_ITU_O151_15, 300, 20);
    for (i = 0;  i < 0x8000;  i++)
        test[i] = 0;
    max_zeros = 0;
    zeros = 0;
    for (i = 0;  i < 32767*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        if (bit)
        {
            if (zeros > max_zeros)
                max_zeros = zeros;
            zeros = 0;
        }
        else
        {
            zeros++;
        }
        bert_put_bit(&rx_bert, bit);        
        test[tx_bert.tx_reg]++;
    }
    if (test[0] != 0)
        printf("XXX %d %d\n", 0, test[0]);
    for (i = 1;  i < 0x8000;  i++)
    {
        if (test[i] != 2)
            printf("XXX %d %d\n", i, test[i]);
    }
    printf("O.151(15): Bad bits %d/%d, max zeros %d\n", rx_bert.bad_bits, rx_bert.total_bits, max_zeros);


    bert_init(&tx_bert, 0, BERT_PATTERN_ITU_O151_20, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_ITU_O151_20, 300, 20);
    for (i = 0;  i < 0x100000;  i++)
        test[i] = 0;    
    max_zeros = 0;
    zeros = 0;
    for (i = 0;  i < 1048575*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        if (bit)
        {
            if (zeros > max_zeros)
                max_zeros = zeros;
            zeros = 0;
        }
        else
        {
            zeros++;
        }
        bert_put_bit(&rx_bert, bit);        
        test[tx_bert.tx_reg]++;
    }
    if (test[0] != 0)
        printf("XXX %d %d\n", 0, test[0]);
    for (i = 1;  i < 0x100000;  i++)
    {
        if (test[i] != 2)
            printf("XXX %d %d\n", i, test[i]);
    }
    printf("O.151(20): Bad bits %d/%d, max zeros %d\n", rx_bert.bad_bits, rx_bert.total_bits, max_zeros);


    bert_init(&tx_bert, 0, BERT_PATTERN_ITU_O151_23, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_ITU_O151_23, 300, 20);
    for (i = 0;  i < 0x800000;  i++)
        test[i] = 0;    
    max_zeros = 0;
    zeros = 0;
    for (i = 0;  i < 8388607*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        if (bit)
        {
            if (zeros > max_zeros)
                max_zeros = zeros;
            zeros = 0;
        }
        else
        {
            zeros++;
        }
        bert_put_bit(&rx_bert, bit);        
        test[tx_bert.tx_reg]++;
    }
    if (test[0] != 0)
        printf("XXX %d %d\n", 0, test[0]);
    for (i = 1;  i < 0x800000;  i++)
    {
        if (test[i] != 2)
            printf("XXX %d %d\n", i, test[i]);
    }
    printf("O.151(23): Bad bits %d/%d, max zeros %d\n", rx_bert.bad_bits, rx_bert.total_bits, max_zeros);

    bert_init(&tx_bert, 0, BERT_PATTERN_QBF, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_QBF, 300, 20);
    for (i = 0;  i < 100000;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);        
    }
    printf("QBF:       Bad bits %d/%d\n", rx_bert.bad_bits, rx_bert.total_bits);

    /* Test the mechanism for categorising the error rate into <10^x bands */
    bert_init(&bert, 15000000, BERT_PATTERN_ITU_O152_11, 300, 20);
    bert_set_report(&bert, 100000, reporter, &bert);
    for (;;)
    {
        bit = bert_get_bit(&bert);
        if (bit == 2)
        {
            bert_result(&bert, &bert_results);
            printf("Rate test: %d bits, %d bad bits, %d resyncs\n", bert_results.total_bits, bert_results.bad_bits, bert_results.resyncs);
            bert_init(&bert, 15000000, BERT_PATTERN_ITU_O152_11, 300, 20);
            bert_set_report(&bert, 100000, reporter, &bert);
        }
        else
        {
            if ((rand() & 0x3FFFF) == 0)
                bit ^= 1;
            //if ((rand() & 0xFFF) == 0)
            //    bert_put_bit(&bert, bit);
            bert_put_bit(&bert, bit);
        }
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

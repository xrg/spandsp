/*
 * SpanDSP - a series of DSP components for telephony
 *
 * bert.h - Bit error rate tests.
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
 * $Id: bert.h,v 1.4 2005/01/18 14:05:48 steveu Exp $
 */

#if !defined(_BERT_H_)
#define _BERT_H_

/*! \page BERT_page The Bit Error Rate tester
\section BERT_page_sec_1 What does it do?
The Bit Error Rate tester generates a pseudo random bit stream. It also accepts such
a pattern, synchronises to it, and checks the bit error rate in this stream.

\section BERT_page_sec_2 How does it work?
The Bit Error Rate tester generates a bit stream, with a repeating 2047 bit pseudo
random pattern, using an 11 stage polynomial generator. It also accepts such a pattern,
synchronises to it, and checks the bit error rate in this stream. If the error rate is
excessive the tester assumes synchronisation has been lost, and it attempts to
resynchronise with the stream.

The bit error rate is continuously assessed against decadic ranges -
    > 1 in 10^2
    > 1 in 10^3
    > 1 in 10^4
    > 1 in 10^5
    > 1 in 10^6
    > 1 in 10^7
    < 1 in 10^7
To ensure fairly smooth results from this assessment, each decadic level is assessed
over 10/error rate bits. That is, to assess if the signal's BER is above or below 1 in 10^5
the software looks over 10*10^5 => 10^6 bits.
*/

enum
{
    BERT_REPORT_SYNCED,
    BERT_REPORT_UNSYNCED,
    BERT_REPORT_REGULAR,
    BERT_REPORT_GT_10_2,
    BERT_REPORT_LT_10_2,
    BERT_REPORT_LT_10_3,
    BERT_REPORT_LT_10_4,
    BERT_REPORT_LT_10_5,
    BERT_REPORT_LT_10_6,
    BERT_REPORT_LT_10_7
};

/* The QBF strings should be:
    "VoyeZ Le BricK GeanT QuE J'ExaminE PreS Du WharF 123 456 7890 + - * : = $ % ( )"
    "ThE QuicK BrowN FoX JumpS OveR ThE LazY DoG 123 456 7890 + - * : = $ % ( )"
*/

enum
{
    BERT_PATTERN_ZEROS,
    BERT_PATTERN_ONES,
    BERT_PATTERN_7_TO_1,
    BERT_PATTERN_3_TO_1,
    BERT_PATTERN_1_TO_1,
    BERT_PATTERN_1_TO_3,
    BERT_PATTERN_1_TO_7,
    BERT_PATTERN_QBF,
    BERT_PATTERN_ITU_O151_23,
    BERT_PATTERN_ITU_O151_20,
    BERT_PATTERN_ITU_O151_15,
    BERT_PATTERN_ITU_O152_11,
    BERT_PATTERN_ITU_O153_9
};

typedef void (*bert_report_func_t)(void *user_data, int reason);

typedef struct
{
    int pattern;
    int pattern_class;
    bert_report_func_t reporter;
    void *user_data;
    int report_frequency;
    int limit;

    uint32_t tx_reg;
    int tx_bits;
    int tx_zeros;

    uint32_t rx_reg;
    uint32_t ref_reg;
    uint32_t master_reg;
    int resync;
    int rx_bits;
    int rx_zeros;
    int resync_len;
    int resync_percent;
    int resync_bad_bits;
    int resync_cnt;
    int total_bits;
    int bad_bits;
    int resyncs;
    
    uint32_t mask;
    int shift;
    int shift2;
    int max_zeros;
    int invert;
    int resync_time;

    int decade_ptr[8];
    int decade_bad[8][10];
    int step;
    int error_rate;

    int bit_error_status;
    int report_countdown;
} bert_state_t;

typedef struct
{
    int total_bits;
    int bad_bits;
    int resyncs;
} bert_results_t;

#ifdef __cplusplus
extern "C" {
#endif

int bert_init(bert_state_t *s, int limit, int pattern, int resync_len, int resync_percent);
int bert_get_bit(bert_state_t *s);
void bert_put_bit(bert_state_t *s, int bit);
int bert_set_reporting(bert_state_t *s, int freq, bert_report_func_t reporter, void *user_data);
int bert_result(bert_state_t *s, bert_results_t *results);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

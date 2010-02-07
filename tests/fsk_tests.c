/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fsk_tests.c - Tests for the low speed FSK modem code (V.21, V.23, etc.).
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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
 * $Id: fsk_tests.c,v 1.16 2005/12/25 15:08:36 steveu Exp $
 */

/*! \page fsk_tests_page FSK modem tests
\section fsk_tests_page_sec_1 What does it do?
These tests allow either:

 - An FSK transmit modem to feed an FSK receive modem, of the same type,
   through a telephone line model. BER testing is then used to evaluate
   performance under various line conditions. This is effective for testing
   the basic performance of the receive modem. It is also the only test mode
   provided for evaluating the transmit modem.

 - An FSK receive modem is used to decode FSK audio, stored in a wave file.
   This is good way to evaluate performance with audio recorded from other
   models of modem, and with real world problematic telephone lines.

\section fsk_tests_page_sec_2 How does it work?
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

#define SAMPLES_PER_CHUNK   160

#define OUTPUT_FILE_NAME    "fsk.wav"

char *decode_test_file = NULL;

int rx_bits = 0;

static void put_bit(void *user_data, int bit)
{
    int i;
    int len;
    
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

    printf("Rx bit %d - %d\n", rx_bits++, bit);
}
/*- End of function --------------------------------------------------------*/

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
        printf("BERT report regular - %d bits, %d bad bits, %d resyncs\n", bert_results.total_bits, bert_results.bad_bits, bert_results.resyncs);
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

int main(int argc, char *argv[])
{
    fsk_tx_state_t tx;
    fsk_rx_state_t rx;
    bert_state_t bert;
    bert_results_t bert_results;
    int16_t amp[SAMPLES_PER_CHUNK];
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int inframes;    
    int outframes;    
    int i;
    int j;
    int len;
    int test_bps;
    int noise_level;
    int bits_per_test;
    awgn_state_t noise_source;

    decode_test_file = NULL;
    for (i = 1;  i < argc;  i++)
    {
        if (strcmp(argv[i], "-d") == 0)
        {
            i++;
            decode_test_file = argv[i];
        }
    }
    filesetup = afNewFileSetup();
    if (filesetup == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    if (decode_test_file)
    {
        inhandle = afOpenFile(decode_test_file, "r", NULL);
        if (inhandle == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot open wave file '%s'\n", decode_test_file);
            exit(2);
        }
        fsk_rx_init(&rx, &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) put_bit, NULL);
        test_bps = preset_fsk_specs[FSK_V21CH2].baud_rate;
    }
    else
    {
        printf("Test with BERT\n");
        fsk_tx_init(&tx, &preset_fsk_specs[FSK_V21CH2], (get_bit_func_t) bert_get_bit, &bert);
        fsk_rx_init(&rx, &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) bert_put_bit, &bert);
        test_bps = preset_fsk_specs[FSK_V21CH2].baud_rate;

        bits_per_test = 50000000;
        noise_level = -16; //-17;
        awgn_init(&noise_source, 1234567, noise_level);

        bert_init(&bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
        bert_set_report(&bert, 100000, reporter, &bert);
    }
    outhandle = afOpenFile(OUTPUT_FILE_NAME, "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }

    for (;;)
    {
        if (decode_test_file)
        {
            len = afReadFrames(inhandle,
                               AF_DEFAULT_TRACK,
                               amp,
                               SAMPLES_PER_CHUNK);
            if (len < SAMPLES_PER_CHUNK)
                break;
        }
        else
        {
            len = fsk_tx(&tx, amp, SAMPLES_PER_CHUNK);
            for (i = 0;  i < len;  i++)
                amp[i] = alaw_to_linear(linear_to_alaw(saturate(amp[i] + awgn(&noise_source))));
        }
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  len);
        if (outframes != len)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
        fsk_rx(&rx, amp, len);
        if (len < SAMPLES_PER_CHUNK)
        {
            bert_result(&bert, &bert_results);
            fprintf(stderr, "%ddB AWGN, %d bits, %d bad bits, %d resyncs\n", noise_level, bert_results.total_bits, bert_results.bad_bits, bert_results.resyncs);

            /* Put a little silence between the chunks in the file. */
            memset(amp, 0, sizeof(amp));
            for (i = 0;  i < 200;  i++)
            {
                outframes = afWriteFrames(outhandle,
                                          AF_DEFAULT_TRACK,
                                          amp,
                                          SAMPLES_PER_CHUNK);
            }
            fsk_tx_init(&tx, &preset_fsk_specs[FSK_V21CH2], (get_bit_func_t) bert_get_bit, &bert);
            fsk_rx_init(&rx, &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) bert_put_bit, &bert);
            awgn_init(&noise_source, 1234567, ++noise_level);
            bert_init(&bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
            bert_set_report(&bert, 100000, reporter, &bert);
        }
    }

    if (decode_test_file)
    {
        if (afCloseFile(inhandle) != 0)
        {
            fprintf(stderr, "    Cannot close wave file '%s'\n", decode_test_file);
            exit(2);
        }
    }
    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }
    afFreeFileSetup(filesetup);
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

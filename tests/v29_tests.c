/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v29_tests.c
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
 * $Id: v29_tests.c,v 1.32 2004/12/27 13:25:17 steveu Exp $
 */

/*! \page v29_tests_page V.29 modem tests
\section v29_tests_page_sec_1 What does it do
*/

#define	_ISOC9X_SOURCE	1
#define _ISOC99_SOURCE	1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_FL_FL_H)  &&  defined(HAVE_FL_FL_CARTESIAN_H)
#define ENABLE_GUI
#endif

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"
#include "line_model.h"
#if defined(ENABLE_GUI)
#include "constel.h"
#endif

#define BLOCK_LEN       160

#define IN_FILE_NAME    "v29_samp.wav"
#define OUT_FILE_NAME   "v29.wav"

int decode_test = FALSE;

int symbol_no = 0;

int rx_bits = 0;

bert_state_t bert;
one_way_line_model_state_t *line_model;

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

static void v29putbit(void *user_data, int bit)
{
    v29_rx_state_t *rx;
    int i;
    int len;
    complex_t *coeffs;
    
    rx = (v29_rx_state_t *) user_data;
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
            len = v29_rx_equalizer_state(rx, &coeffs);
            printf("Equalizer:\n");
            for (i = 0;  i < len;  i++)
                printf("%3d (%15.5f, %15.5f) -> %15.5f\n", i, coeffs[i].re, coeffs[i].im, power(&coeffs[i]));
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

    if (decode_test)
        printf("Rx bit %d - %d\n", rx_bits++, bit);
    else
        bert_put_bit(&bert, bit);
}
/*- End of function --------------------------------------------------------*/

static int v29getbit(void *user_data)
{
    return bert_get_bit(&bert);
}
/*- End of function --------------------------------------------------------*/

void qam_report(void *user_data, const complex_t *constel, const complex_t *target, int symbol)
{
    int i;
    int len;
    complex_t *coeffs;
    float fpower;
    v29_rx_state_t *rx;
    static float smooth_power = 0.0;

    rx = (v29_rx_state_t *) user_data;
    if (constel)
    {
        fpower = (constel->re - target->re)*(constel->re - target->re)
               + (constel->im - target->im)*(constel->im - target->im);
        smooth_power = 0.95*smooth_power + 0.05*fpower;
#if defined(ENABLE_GUI)
        update_qam_monitor(constel);
        //update_qam_carrier_tracking(v29_rx_carrier_frequency(rx));
        update_qam_carrier_tracking((fpower)  ?  fpower  :  0.001);
        update_qam_symbol_tracking(v29_rx_symbol_timing_correction(rx));
#endif
        printf("%8d [%8.4f, %8.4f] [%8.4f, %8.4f] %8.4f %8.4f %9.4f %7.3f %7.1f\n",
               symbol_no,
               constel->re,
               constel->im,
               target->re,
               target->im,
               fpower,
               smooth_power,
               v29_rx_carrier_frequency(rx),
               v29_rx_signal_power(rx),
               v29_rx_symbol_timing_correction(rx));
        symbol_no++;
    }
    else
    {
        printf("Gardner step %d\n", symbol);
        len = v29_rx_equalizer_state(rx, &coeffs);
        printf("Equalizer A:\n");
        for (i = 0;  i < len;  i++)
            printf("%3d (%15.5f, %15.5f) -> %15.5f\n", i, coeffs[i].re, coeffs[i].im, power(&coeffs[i]));
#if defined(ENABLE_GUI)
        update_qam_equalizer_monitor(coeffs, len);
#endif
    }
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    v29_rx_state_t rx;
    v29_tx_state_t tx;
    bert_results_t bert_results;
    int16_t gen_amp[BLOCK_LEN];
    int16_t amp[BLOCK_LEN];
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int inframes;
    int outframes;
    int samples;
    int i;
    int test_bps;
    int noise_level;
    int total_samples;
    int line_model_no;
    int block;
    power_meter_t power_meter;
    
    power_meter_init(&power_meter, 8);
    test_bps = 9600;
    line_model_no = 5;
    i = 1;
    decode_test = FALSE;
    if (argc > i)
    {
        if (strcmp(argv[i], "-d") == 0)
        {
            decode_test = TRUE;
            i++;
        }
    }
    if (argc > i)
    {
        if (strcmp(argv[i], "9600") == 0)
            test_bps = 9600;
        else if (strcmp(argv[i], "7200") == 0)
            test_bps = 7200;
        else if (strcmp(argv[i], "4800") == 0)
            test_bps = 4800;
        else
        {
            fprintf(stderr, "Invalid bit rate\n");
            exit(2);
        }
    }
    if (argc > i)
    {
        line_model_no = atoi(argv[i]);
        i++;
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

    if (decode_test)
    {
        /* We will decode the audio from a wave file. */
        inhandle = afOpenFile(IN_FILE_NAME, "r", NULL);
        if (inhandle == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot open wave file '%s'\n", IN_FILE_NAME);
            exit(2);
        }
    }
    else
    {
        /* We will generate V.29 audio, and add some noise to it. */
        outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup);
        if (outhandle == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
            exit(2);
        }
    }
    v29_tx_init(&tx, test_bps, v29getbit, NULL);
    v29_tx_power(&tx, -18.0);
    /* Move the carrier off a bit */
    tx.carrier_phase_rate = dds_phase_stepf(1692.0);
    v29_rx_init(&rx, test_bps, v29putbit, &rx);
    v29_rx_set_qam_report_handler(&rx, qam_report, (void *) &rx);
    /* Rotate the starting phase */
    rx.carrier_phase = 0x80000000;

    noise_level = -55;

    bert_init(&bert, 50000, BERT_PATTERN_ITU_O152_11, test_bps, 20);
    bert_set_report(&bert, 10000, reporter, &bert);

    if ((line_model = one_way_line_model_init(line_model_no, -50)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }
#if defined(ENABLE_GUI)
    start_qam_monitor(6.0);
#endif

    total_samples = 0;
    for (block = 0;  ;  block++)
    {
        if (decode_test)
        {
            samples = afReadFrames(inhandle,
                                   AF_DEFAULT_TRACK,
                                   amp,
                                   BLOCK_LEN);
            if (samples == 0)
                break;
            total_samples += samples;
        }
        else
        {
            samples = v29_tx(&tx, gen_amp, BLOCK_LEN);
            if (samples == 0)
            {
                printf("Restarting on zero output\n");
                bert_result(&bert, &bert_results);
                fprintf(stderr, "%ddB AWGN, %d bits, %d bad bits, %d resyncs\n", noise_level, bert_results.total_bits, bert_results.bad_bits, bert_results.resyncs);
                v29_tx_restart(&tx, test_bps);
                v29_rx_restart(&rx, test_bps);
                bert_init(&bert, 50000, BERT_PATTERN_ITU_O152_11, test_bps, 20);
                bert_set_report(&bert, 10000, reporter, &bert);
            }
            total_samples += samples;
            outframes = afWriteFrames(outhandle,
                                      AF_DEFAULT_TRACK,
                                      gen_amp,
                                      samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing wave file\n");
                exit(2);
            }
            for (i = 0;  i < samples;  i++)
                power_meter_update(&power_meter, gen_amp[i]);
            one_way_line_model(line_model, amp, gen_amp, samples);
            printf("smooth power %f\n", power_meter_dbm0(&power_meter));
        }
        v29_rx(&rx, amp, samples);
        if (block%500 == 0)
        {
            printf("Noise level is %d\n", noise_level);
        }
    }
    if (decode_test)
    {
#if defined(ENABLE_GUI)
        qam_wait_to_end();
#endif
    }
    else
    {
        bert_result(&bert, &bert_results);
        fprintf(stderr, "%d bits, %d bad bits, %d resyncs\n", bert_results.total_bits, bert_results.bad_bits, bert_results.resyncs);
        if (afCloseFile(outhandle))
        {
            fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_NAME);
            exit(2);
        }
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

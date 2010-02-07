/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v22bis_tests.c
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
 * $Id: v22bis_tests.c,v 1.12 2005/01/16 08:26:55 steveu Exp $
 */

/*! \page v22bis_tests_page V.22bis modem tests
\section v22bis_tests_page_sec_1 What does it do
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

#define IN_FILE_NAME    "v22bis_samp.wav"
#define OUT_FILE_NAME   "v22bis.wav"

int in_bit = 0;
int out_bit = 0;

int in_bit_no = 0;
int out_bit_no = 0;

uint8_t tx_buf[1000];
int rx_ptr = 0;
int tx_ptr = 0;

int rx_bits = 0;
int rx_bad_bits = 0;

int symbol_no = 0;

both_ways_line_model_state_t *model;

v22bis_state_t caller;
v22bis_state_t answerer;

static void v22bis_putbit(void *user_data, int bit)
{
    v22bis_state_t *s;
    int i;
    int len;
    complex_t *coeffs;
    
    s = (v22bis_state_t *) user_data;
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
            len = v22bis_rx_equalizer_state(s, &coeffs);
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

    if (bit != tx_buf[rx_ptr])
    {
        printf("Rx bit %d - %d\n", rx_bits, bit);
        rx_bad_bits++;
    }
    rx_ptr++;
    if (rx_ptr > 1000)
        rx_ptr = 0;
    rx_bits++;
    if ((rx_bits % 100000) == 0)
    {
        printf("%d bits received, %d bad bits\r", rx_bits, rx_bad_bits);
        fflush(stdout);
    }
}

static int v22bis_getbit(void *user_data)
{
    int bit;
    static tx_bits = 0;

    bit = rand() & 1;
    tx_buf[tx_ptr++] = bit;
    if (tx_ptr > 1000)
        tx_ptr = 0;
    //printf("Tx bit %d\n", bit);
    if (++tx_bits > 50000)
    {
        tx_bits = 0;
        bit = 2;
    }
    return bit;
}

void qam_report(void *user_data, const complex_t *constel, const complex_t *target, int symbol)
{
    int i;
    int len;
    complex_t *coeffs;
    float fpower;
    v22bis_state_t *rx;
    static float smooth_power = 0.0;

    rx = (v22bis_state_t *) user_data;
    if (constel)
    {
#if defined(ENABLE_GUI)
        update_qam_monitor(constel);
        update_qam_carrier_tracking(v22bis_rx_carrier_frequency(rx));
        update_qam_symbol_tracking(v22bis_rx_symbol_timing_correction(rx));
#endif
        fpower = (constel->re - target->re)*(constel->re - target->re)
               + (constel->im - target->im)*(constel->im - target->im);
        smooth_power = 0.95*smooth_power + 0.05*fpower;
        printf("%8d [%8.4f, %8.4f] [%8.4f, %8.4f] %8.4f %8.4f\n", symbol_no, constel->re, constel->im, target->re, target->im, fpower, smooth_power);
        symbol_no++;
    }
    else
    {
        printf("Gardner step %d\n", symbol);
        len = v22bis_rx_equalizer_state(rx, &coeffs);
        printf("Equalizer A:\n");
        for (i = 0;  i < len;  i++)
            printf("%3d (%15.5f, %15.5f) -> %15.5f\n", i, coeffs[i].re, coeffs[i].im, power(&coeffs[i]));
#if defined(ENABLE_GUI)
        update_qam_equalizer_monitor(coeffs, len);
#endif
    }
}

int main(int argc, char *argv[])
{
    int16_t caller_amp[160];
    int16_t answerer_amp[160];
    int16_t caller_model_amp[160];
    int16_t answerer_model_amp[160];
    int16_t out_amp[2*160];
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int inframes;
    int outframes;
    int samples;
    int i;
    int test_bps;
    awgn_state_t noise_source;

    test_bps = 2400;
    i = 1;
    if (argc > i)
    {
        if (strcmp(argv[i], "2400") == 0)
            test_bps = 2400;
        else if (strcmp(argv[i], "1200") == 0)
            test_bps = 1200;
        else
        {
            fprintf(stderr, "Invalid bit rate\n");
            exit(2);
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
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 2);

    outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    v22bis_init(&caller, test_bps, 2, TRUE, v22bis_getbit, v22bis_putbit, &caller);
    /* Move the carrier off a bit */
    //caller.tx_carrier_phase_rate = dds_phase_stepf(1205.0);
    v22bis_init(&answerer, test_bps, 2, FALSE, v22bis_getbit, v22bis_putbit, &answerer);
    //answerer.tx_carrier_phase_rate = dds_phase_stepf(2405.0);
    v22bis_rx_set_qam_report_handler(&caller, qam_report, (void *) &caller);

    awgn_init(&noise_source, 1234567, -50);

#if defined(ENABLE_GUI)
    start_qam_monitor(6.0);
#endif

    if ((model = both_ways_line_model_init(5, -50, 6, -35)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }
    for (;;)
    {
        samples = v22bis_tx(&caller, caller_amp, 160);
        if (samples == 0)
        {
            printf("Restarting on zero output\n");
            v22bis_restart(&caller, test_bps);
            rx_ptr = 0;
            tx_ptr = 0;
        }
        for (i = 0;  i < samples;  i++)
            out_amp[2*i] = caller_amp[i];

        samples = v22bis_tx(&answerer, answerer_amp, 160);
        if (samples == 0)
        {
            printf("Restarting on zero output\n");
            v22bis_restart(&answerer, test_bps);
            rx_ptr = 0;
            tx_ptr = 0;
        }
        for (i = 0;  i < samples;  i++)
            out_amp[2*i + 1] = answerer_amp[i];

        both_ways_line_model(model, 
                             caller_model_amp,
                             caller_amp,
                             answerer_model_amp,
                             answerer_amp,
                             samples);
        v22bis_rx(&answerer, caller_model_amp, samples);
        v22bis_rx(&caller, answerer_model_amp, samples);

        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  out_amp,
                                  samples);
        if (outframes != samples)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
    }
    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

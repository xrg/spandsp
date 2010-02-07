#define DECODE_TEST
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
 * $Id: v29_tests.c,v 1.18 2004/03/28 12:46:42 steveu Exp $
 */

#define	_ISOC9X_SOURCE	1
#define _ISOC99_SOURCE	1

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

extern int start_qam_monitor(void);
extern int update_qam_monitor(complex_t *pt);
extern int update_qam_equalizer_monitor(complex_t *coeffs, int len);

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

#if 1
    if (symbol_no == 9370) //rx_bits%50000 == 0)
    {
        len = v29_rx_equalizer_state(rx, &coeffs);
        printf("Equalizer:\n");
        for (i = 0;  i < len;  i++)
            printf("%3d (%15.5f, %15.5f) -> %15.5f\n", i, coeffs[i].re, coeffs[i].im, power(&coeffs[i]));
    }
#endif
#if 0
    if (bit == 1) //(bit != tx_buf[rx_ptr])
    {
        printf("Rx bit %d - %d\n", rx_bits, bit);
        rx_bad_bits++;
    }
#endif
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

static int v29getbit(void *user_data)
{
    int bit;

    bit = rand() & 1;
    tx_buf[tx_ptr++] = bit;
    if (tx_ptr > 1000)
        tx_ptr = 0;
    //printf("Tx bit %d\n", bit);
    return bit;
}

void qam_report(void *user_data, complex_t *constel, int symbol)
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
        update_qam_monitor(constel);
        fpower = (constel->re - v29_constellation[symbol].re)*(constel->re - v29_constellation[symbol].re)
               + (constel->im - v29_constellation[symbol].im)*(constel->im - v29_constellation[symbol].im);
        smooth_power = 0.95*smooth_power + 0.05*fpower;
        //printf("%8d [%8.4f, %8.4f] [%8.4f, %8.4f] %8.4f %8.4f\n", symbol_no, constel->re, constel->im, v29_constellation[symbol].re, v29_constellation[symbol].im, fpower, smooth_power);
        symbol_no++;
    }
    else
    {
        printf("Gardner step %d\n", symbol);
        len = v29_rx_equalizer_state(rx, &coeffs);
        printf("Equalizer A:\n");
        for (i = 0;  i < len;  i++)
            printf("%3d (%15.5f, %15.5f) -> %15.5f\n", i, coeffs[i].re, coeffs[i].im, power(&coeffs[i]));
        update_qam_equalizer_monitor(coeffs, len);
    }
}

int main(int argc, char *argv[])
{
    v29_rx_state_t rx;
    v29_tx_state_t tx;
    int16_t amp[160];
#if defined(DECODE_TEST)
    AFfilehandle inhandle;
#endif
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int inframes;
    int outframes;
    int samples;
    int i;
    int test_bps;
    awgn_state_t noise_source;

    test_bps = 9600;
    if (argc > 1)
    {
        if (strcmp(argv[1], "9600") == 0)
            test_bps = 9600;
        else if (strcmp(argv[1], "7200") == 0)
            test_bps = 7200;
        else if (strcmp(argv[1], "4800") == 0)
            test_bps = 4800;
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
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    outhandle = afOpenFile("v29.wav", "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", "v29.wav");
        exit(2);
    }
#if defined(DECODE_TEST)
    inhandle = afOpenFile("v29_samp.wav", "r", NULL);
    if (inhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open wave file '%s'\n", "v29_samp.wav");
        exit(2);
    }
#endif

    v29_tx_init(&tx, test_bps, v29getbit, NULL);
    /* Move the carrier off a bit */
    tx.carrier_phase_rate = complex_dds_phase_step(1692.0);
    v29_rx_init(&rx, test_bps, v29putbit, &rx);
    v29_rx_set_qam_report_handler(&rx, qam_report, (void *) &rx);
    /* Rotate the starting phase */
    rx.carrier_phase = 0x80000000;

    awgn_init(&noise_source, 1234567, -50);

    start_qam_monitor();

    for (;;)
    {
        samples = v29_tx(&tx, amp, 160);
#if 1
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  samples);
        if (outframes != samples)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
#endif
        for (i = 0;  i < samples;  i++)
            amp[i] = alaw_to_linear(linear_to_alaw(saturate(amp[i] + awgn(&noise_source))));
#if defined(DECODE_TEST)
        samples = afReadFrames(inhandle,
                               AF_DEFAULT_TRACK,
                               amp,
                               160);
#endif
        v29_rx(&rx, amp, samples);
    }

    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", "v29.wav");
        exit(2);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

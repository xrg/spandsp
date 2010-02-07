//#define DECODE_TEST
/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v27ter_tests.c
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
 * $Id: v27ter_tests.c,v 1.12 2004/03/25 13:28:37 steveu Exp $
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

int in_bit = 0;
int out_bit = 0;

int in_bit_no = 0;
int out_bit_no = 0;

uint8_t tx_buf[1000];
int rx_ptr = 0;
int tx_ptr = 0;

int rx_bits = 0;
int rx_bad_bits = 0;

static void v27terputbit(void *user_data, int bit)
{
    static int doc_bit;
    static int consecutive_eols;
    static uint32_t bits_to_date;
    uint8_t byte;

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

static int v27tergetbit(void *user_data)
{
    int bit;

    bit = rand() & 1;
    tx_buf[tx_ptr] = bit;
    if (++tx_ptr > 1000)
        tx_ptr = 0;
    //printf("Tx bit %d\n", bit);
    return bit;
}

int main(int argc, char *argv[])
{
    v27ter_rx_state_t rx;
    v27ter_tx_state_t tx;
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

    test_bps = 2400;
    if (argc > 1)
    {
        if (strcmp(argv[1], "2400") == 0)
            test_bps = 2400;
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

    outhandle = afOpenFile("v27ter.wav", "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", "v27ter.wav");
        exit(2);
    }
#if defined(DECODE_TEST)
    inhandle = afOpenFile("v27ter_samp.wav", "r", NULL);
    if (inhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open wave file '%s'\n", "v27ter_samp.wav");
        exit(2);
    }
#endif

    v27ter_tx_init(&tx, test_bps, v27tergetbit, NULL);
    /* Move the carrier off a bit */
    tx.carrier_phase_rate = complex_dds_phase_step(1808.0);
    v27ter_rx_init(&rx, test_bps, v27terputbit, NULL);
    if (rx.qam_log == NULL)
        rx.qam_log = fopen("points.txt", "w");

    awgn_init(&noise_source, 1234567, -60);

    for (;;)
    {
        samples = v27ter_tx(&tx, amp, 160);
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
        v27ter_rx(&rx, amp, samples);
    }

    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", "v27ter.wav");
        exit(2);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

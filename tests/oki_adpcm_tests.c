/*
 * SpanDSP - a series of DSP components for telephony
 *
 * oki_adpcm_tests.c - Test the Oki (Dialogic) ADPCM encode and decode
 *                     software at 24kbps and 32kbps.
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
 * $Id: oki_adpcm_tests.c,v 1.8 2006/01/11 08:06:10 steveu Exp $
 */

/*! \file */

/*! \page oki_adpcm_tests_page OKI (Dialogic) ADPCM tests
\section oki_adpcm_tests_page_sec_1 What does it do?
These tests require a file of speech, recorded at 16 bits/sample, 8000 samples/second.
The file should be called "pre_oki_adpcm.wav". The tests compress this file, decompress
it, and store the results in another file called "post_oki_adpcm.wav". Listening tests
may be used to evaluate the degradation in quality caused by the compression. Both 32k bps
and 24k bps compression may be tested.

\section oki_adpcm_tests_page_sec_2 How is it used?
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"

#define IN_FILE_NAME    "pre_oki_adpcm.wav"
#define OUT_FILE_NAME   "post_oki_adpcm.wav"

static void alaw_munge(int16_t amp[], int len)
{
    int i;
    
    for (i = 0;  i < len;  i++)
        amp[i] = alaw_to_linear (linear_to_alaw (amp[i]));
}
/*- End of function --------------------------------------------------------*/

int16_t amp[10000];
uint8_t oki_data[10000];

int main(int argc, char *argv[])
{
    int i;
    int j;
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int frames;
    int dec_frames;
    int outframes;
    int oki_bytes;
    int bit_rate;
    float x;
    oki_adpcm_state_t *oki_enc_state;
    oki_adpcm_state_t *oki_dec_state;

    i = 1;
    bit_rate = 32000;
    if (argc > i)
    {
        if (strcmp(argv[i], "-2") == 0)
        {
            bit_rate = 24000;
            i++;
        }
    }

    if ((inhandle = afOpenFile(IN_FILE_NAME, "r", 0)) == AF_NULL_FILEHANDLE)
    {
        printf("    Cannot open wave file '%s'\n", IN_FILE_NAME);
        exit(2);
    }
    if ((x = afGetFrameSize(inhandle, AF_DEFAULT_TRACK, 1)) != 2.0)
    {
        printf("    Unexpected frame size in wave file '%s'\n", IN_FILE_NAME);
        exit(2);
    }
    if ((x = afGetRate(inhandle, AF_DEFAULT_TRACK)) != (float) SAMPLE_RATE)
    {
        printf("    Unexpected sample rate in wave file '%s'\n", IN_FILE_NAME);
        exit(2);
    }
    if ((x = afGetChannels(inhandle, AF_DEFAULT_TRACK)) != 1.0)
    {
        printf("    Unexpected number of channels in wave file '%s'\n", IN_FILE_NAME);
        exit(2);
    }
    if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }

    if ((oki_enc_state = oki_adpcm_init(NULL, bit_rate)) == NULL)
    {
        fprintf(stderr, "    Cannot create encoder\n");
        exit(2);
    }
        
    if ((oki_dec_state = oki_adpcm_init(NULL, bit_rate)) == NULL)
    {
        fprintf(stderr, "    Cannot create decoder\n");
        exit(2);
    }

    while ((frames = afReadFrames(inhandle, AF_DEFAULT_TRACK, amp, 159)))
    {
        oki_bytes = oki_linear_to_adpcm(oki_enc_state, oki_data, amp, frames);
        dec_frames = oki_adpcm_to_linear(oki_dec_state, amp, oki_data, oki_bytes);
        outframes = afWriteFrames(outhandle, AF_DEFAULT_TRACK, amp, dec_frames);
    }
    if (afCloseFile(inhandle) != 0)
    {
        printf("    Cannot close wave file '%s'\n", IN_FILE_NAME);
        exit(2);
    }
    if (afCloseFile(outhandle) != 0)
    {
        printf("    Cannot close wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    afFreeFileSetup(filesetup);
    oki_adpcm_release(oki_enc_state);
    oki_adpcm_release(oki_dec_state);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v8_tests.c
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
 * $Id: v8_tests.c,v 1.1 2004/07/24 11:46:55 steveu Exp $
 */

#define	_ISOC9X_SOURCE	1
#define _ISOC99_SOURCE	1

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"

#define FALSE 0
#define TRUE (!FALSE)

#define SAMPLES_PER_CHUNK 160

void handler(void *user_data, int result)
{
    printf("V.8 result is %d\n", result);
}

int main(int argc, char *argv[])
{
    int i;
    int j;
    int pitch;
    int16_t amp[160];
    int16_t out_amp[2*160];
    v8_state_t v8_caller;
    v8_state_t v8_answerer;
    int len;
    int hits;
    int frames;
    int outframes;
    int samples;
    int remnant;
    float x;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    
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

    outhandle = afOpenFile("v8.wav", "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", "v17.wav");
        exit(2);
    }

    v8_init(&v8_caller, TRUE, 0xFFFFFFFF, handler, NULL);
    v8_init(&v8_answerer, FALSE, 0xFFFFFFFF, handler, NULL);
    for (i = 0;  i < 1000;  i++)
    {
        samples = v8_tx(&v8_caller, amp, SAMPLES_PER_CHUNK);
        if (samples < SAMPLES_PER_CHUNK)
        {
            memset(amp + samples, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - samples));
            samples = SAMPLES_PER_CHUNK;
        }
        remnant = v8_rx(&v8_answerer, amp, samples);
        for (i = 0;  i < samples;  i++)
            out_amp[2*i] = amp[i];
        
        samples = v8_tx(&v8_answerer, amp, SAMPLES_PER_CHUNK);
        if (samples < SAMPLES_PER_CHUNK)
        {
            memset(amp + samples, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - samples));
            samples = SAMPLES_PER_CHUNK;
        }
        if (v8_rx(&v8_caller, amp, samples)  &&  remnant)
            break;
        for (i = 0;  i < samples;  i++)
            out_amp[2*i + 1] = amp[i];

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
    if (afCloseFile(outhandle))
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", "v17.wav");
        exit(2);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

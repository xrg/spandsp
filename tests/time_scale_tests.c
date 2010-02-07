/*
 * SpanDSP - a series of DSP components for telephony
 *
 * time_scale_tests.c
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
 * $Id: time_scale_tests.c,v 1.2 2004/12/20 14:23:22 steveu Exp $
 */

/*! \page time_scale_tests_page Time scaling tests
\section time_scale_tests_page_sec_1 What does it do?
*/

//#define _ISOC9X_SOURCE  1
//#define _ISOC99_SOURCE  1

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"

#define IN_FILE_NAME    "pre_time_scaling.wav"
#define OUT_FILE_NAME   "post_time_scaling.wav"

int main(int argc, char *argv[])
{
    int i;
    int out_len;
    int new_len;
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int frames;
    int new_frames;
    int out_frames;
    int count;
    time_scale_t state;
    float x;
    float rate;
    int16_t in[160];
    int16_t out[5*160];
    
    inhandle = afOpenFile(IN_FILE_NAME, "r", 0);
    if (inhandle == AF_NULL_FILEHANDLE)
    {
        printf("    Cannot open wave file '%s'\n", IN_FILE_NAME);
        exit(2);
    }
    x = afGetFrameSize(inhandle, AF_DEFAULT_TRACK, 1);
    if (x != 2.0)
    {
        printf("    Unexpected frame size in wave file '%s'\n", IN_FILE_NAME);
        exit(2);
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

    outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }

    rate = 1.8;

    time_scale_init(&state, rate);
    count = 0;
    while ((frames = afReadFrames(inhandle, AF_DEFAULT_TRACK, in, 160)))
    {
        new_frames = time_scale(&state, out, in, frames);
        out_frames = afWriteFrames(outhandle, AF_DEFAULT_TRACK, out, new_frames);
        if (out_frames != new_frames)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
        if (++count > 100)
        {
            if (rate > 0.5)
            {
                rate -= 0.1;
                if (rate >= 0.99  &&  rate <= 1.01)
                    rate -= 0.1;
                printf("Rate is %f\n", rate);
                time_scale_init(&state, rate);
            }
            count = 0;
        }
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
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

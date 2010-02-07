/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g722_tests.c - Test G.722 encode and decode.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
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
 * $Id: g722_tests.c,v 1.2 2005/09/04 07:40:03 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <inttypes.h>
#include <memory.h>
#include <stdlib.h>
#include <math.h>
#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"

#define G722_SAMPLE_RATE    16000

#define BLOCK_LEN           320

#define IN_FILE_NAME    "g722_before.wav"
#define OUT_FILE_NAME   "g722_after.wav"

int16_t indata[BLOCK_LEN];
int16_t outdata[BLOCK_LEN];
uint16_t adpcmdata[BLOCK_LEN];

int main(int argc, char *argv[])
{
    g722_encode_state_t enc_state;
    g722_decode_state_t dec_state;
    int len;
    int len2;
    int len3;
    int i;
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int inframes;
    int outframes;
    int samples;

    if (argc != 1)
    {
        fprintf(stderr, "Usage: %s   < .pcm   > .adpcm\n", argv[0]);
        exit(1);
    }

    filesetup = afNewFileSetup();
    if (filesetup == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) G722_SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }

    inhandle = afOpenFile(IN_FILE_NAME, "r", NULL);
    if (inhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open wave file '%s'\n", IN_FILE_NAME);
        exit(2);
    }

    g722_encode_init(&enc_state, 64000);
    g722_decode_init(&dec_state, 64000);
    for (;;)
    {
        samples = afReadFrames(inhandle,
                               AF_DEFAULT_TRACK,
                               indata,
                               BLOCK_LEN);
        if (samples <= 0)
            break;
        len2 = g722_encode(&enc_state, adpcmdata, indata, samples);
        len3 = g722_decode(&dec_state, outdata, adpcmdata, len2);
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  outdata,
                                  len3);
        if (outframes != len3)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
    }
    if (afCloseFile(inhandle))
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", IN_FILE_NAME);
        exit(2);
    }
    if (afCloseFile(outhandle))
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }

    return 0;
}

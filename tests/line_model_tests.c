/*
 * SpanDSP - a series of DSP components for telephony
 *
 * line_model_tests.c - Tests for the telephone line model.
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
 * $Id: line_model_tests.c,v 1.3 2005/09/01 17:06:45 steveu Exp $
 */

#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <audiofile.h>
#include <tiffio.h>

#include <fftw.h>

#define GEN_CONST
#include <math.h>

#include "spandsp.h"

#include "line_model.h"

#if !defined(NULL)
#define NULL (void *) 0
#endif

#define BLOCK_LEN       160

#define IN_FILE_NAME1   "line_model_test_in1.wav"
#define IN_FILE_NAME2   "line_model_test_in2.wav"
#define OUT_FILE_NAME1  "line_model_one_way_test_out.wav"
#define OUT_FILE_NAME   "line_model_test_out.wav"

void test_one_way_model(int line_model_no)
{
    one_way_line_model_state_t *model;
    int16_t input1[BLOCK_LEN];
    int16_t output1[BLOCK_LEN];
    int16_t amp[2*BLOCK_LEN];
    AFfilehandle inhandle1;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int inframes;
    int outframes;
    int samples;
    int i;
    int j;
    awgn_state_t noise1;
    
    if ((model = one_way_line_model_init(line_model_no, -50)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }
    
    awgn_init(&noise1, 1234567, -10);

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

    inhandle1 = afOpenFile(IN_FILE_NAME1, "r", NULL);
    if (inhandle1 == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open wave file '%s'\n", IN_FILE_NAME1);
        exit(2);
    }
    outhandle = afOpenFile(OUT_FILE_NAME1, "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME1);
        exit(2);
    }
    for (i = 0;  i < 10000;  i++)
    {
#if 0
        samples = afReadFrames(inhandle1,
                               AF_DEFAULT_TRACK,
                               input1,
                               BLOCK_LEN);
        if (samples == 0)
            break;
#else
        for (j = 0;  j < BLOCK_LEN;  j++)
        {
            input1[j] = awgn(&noise1);
        }
        samples = BLOCK_LEN;
#endif
        for (j = 0;  j < samples;  j++)
        {
            one_way_line_model(model, 
                               &output1[j],
                               &input1[j],
                               1);
            amp[j] = output1[j];
        }
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  samples);
        if (outframes != samples)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
    }
    if (afCloseFile(inhandle1))
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", IN_FILE_NAME1);
        exit(2);
    }
    if (afCloseFile(outhandle))
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_NAME1);
        exit(2);
    }
}

void test_both_ways_model(int line_model_no)
{
    both_ways_line_model_state_t *model;
    int16_t input1[BLOCK_LEN];
    int16_t input2[BLOCK_LEN];
    int16_t output1[BLOCK_LEN];
    int16_t output2[BLOCK_LEN];
    int16_t amp[2*BLOCK_LEN];
    AFfilehandle inhandle1;
    AFfilehandle inhandle2;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int inframes;
    int outframes;
    int samples;
    int i;
    int j;
    awgn_state_t noise1;
    awgn_state_t noise2;
    
    if ((model = both_ways_line_model_init(line_model_no, -50, line_model_no + 1, -35)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }
    
    awgn_init(&noise1, 1234567, -10);
    awgn_init(&noise2, 1234567, -10);

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

    inhandle1 = afOpenFile(IN_FILE_NAME1, "r", NULL);
    if (inhandle1 == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open wave file '%s'\n", IN_FILE_NAME1);
        exit(2);
    }
    inhandle2 = afOpenFile(IN_FILE_NAME2, "r", NULL);
    if (inhandle2 == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open wave file '%s'\n", IN_FILE_NAME2);
        exit(2);
    }
    outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    for (i = 0;  i < 10000;  i++)
    {
#if 0
        samples = afReadFrames(inhandle1,
                               AF_DEFAULT_TRACK,
                               input1,
                               BLOCK_LEN);
        if (samples == 0)
            break;
        samples = afReadFrames(inhandle2,
                               AF_DEFAULT_TRACK,
                               input2,
                               samples);
        if (samples == 0)
            break;
#else
        for (j = 0;  j < BLOCK_LEN;  j++)
        {
            input1[j] = awgn(&noise1);
            input2[j] = awgn(&noise2);
        }
        samples = BLOCK_LEN;
#endif
        for (j = 0;  j < samples;  j++)
        {
            both_ways_line_model(model, 
                                 &output1[j],
                                 &input1[j],
                                 &output2[j],
                                 &input2[j],
                                 1);
            amp[2*j] = output1[j];
            amp[2*j + 1] = output2[j];
        }
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  samples);
        if (outframes != samples)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
    }
    if (afCloseFile(inhandle1))
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", IN_FILE_NAME1);
        exit(2);
    }
    if (afCloseFile(inhandle2))
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", IN_FILE_NAME2);
        exit(2);
    }
    if (afCloseFile(outhandle))
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int line_model_no;

    line_model_no = 5;
    if (argc > 1)
        line_model_no = atoi(argv[1]);
    test_one_way_model(line_model_no);
    test_both_ways_model(line_model_no);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

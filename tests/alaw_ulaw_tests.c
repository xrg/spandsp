/*
 * SpanDSP - a series of DSP components for telephony
 *
 * alaw_ulaw_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
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
 * $Id: alaw_ulaw_tests.c,v 1.1 2006/01/18 13:21:37 steveu Exp $
 */

/*! \page alaw_ulaw_tests_page A-law and u-law conversion tests
\section alaw_ulaw_tests_page_sec_1 What does it do?

\section alaw_ulaw_tests_page_sec_2 How is it used?
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_FL_FL_H)  &&  defined(HAVE_FL_FL_CARTESIAN_H)
#define ENABLE_GUI
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <tgmath.h>
#include <assert.h>
#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"

#define OUT_FILE_NAME   "alaw_ulaw.wav"

int16_t amp[65536];

int main(int argc, char *argv[])
{
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int outframes;
    int i;
    int block;
    int pre;
    int post;
    uint8_t law;
    int alaw_failures;
    int ulaw_failures;
    float worst_alaw;
    float worst_ulaw;
    float tmp;
    
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

    if ((outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    alaw_failures = 0;
    ulaw_failures = 0;
    worst_alaw = 0.0;
    worst_ulaw = 0.0;
    for (block = 0;  block < 1;  block++)
    {
        for (i = 0;  i < 65536;  i++)
        {
            pre = i - 32768;
            post = alaw_to_linear(linear_to_alaw(pre));
            if (abs(pre) > 140)
            {
                tmp = (float) abs(post - pre)/(float) abs(pre);
                if (tmp > 0.10)
                {
                    printf("A-law: Excessive error at %d (%d)\n", pre, post);
                    alaw_failures++;
                }
                if (tmp > worst_alaw)
                    worst_alaw = tmp;
            }
            else
            {
                /* Small values need different handling for sensible measurement */
                if (abs(post - pre) > 15)
                {
                    printf("A-law: Excessive error at %d (%d)\n", pre, post);
                    alaw_failures++;
                }
            }
            amp[i] = post;
        }
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  65536);
        if (outframes != 65536)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
        for (i = 0;  i < 65536;  i++)
        {
            pre = i - 32768;
            post = ulaw_to_linear(linear_to_ulaw(pre));
            if (abs(pre) > 40)
            {
                tmp = (float) abs(post - pre)/(float) abs(pre);
                if (tmp > 0.10)
                {
                    printf("u-law: Excessive error at %d (%d)\n", pre, post);
                    ulaw_failures++;
                }
                if (tmp > worst_ulaw)
                    worst_ulaw = tmp;
            }
            else
            {
                /* Small values need different handling for sensible measurement */
                if (abs(post - pre) > 4)
                {
                    printf("u-law: Excessive error at %d (%d)\n", pre, post);
                    ulaw_failures++;
                }
            }
            amp[i] = post;
        }
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  65536);
        if (outframes != 65536)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
    }
    if (afCloseFile(outhandle))
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    afFreeFileSetup(filesetup);
    printf("Worst A-law error (ignoring small values) %f%%\n", worst_alaw*100.0);
    printf("Worst u-law error (ignoring small values) %f%%\n", worst_ulaw*100.0);
    if (alaw_failures  ||  ulaw_failures)
    {
        printf("%d A-law values with excessive error\n", alaw_failures);
        printf("%d u-law values with excessive error\n", ulaw_failures);
        printf("Tests failed\n");
        exit(2);
    }
    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

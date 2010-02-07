/*
 * SpanDSP - a series of DSP components for telephony
 *
 * dds_tests.c
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
 * $Id: dds_tests.c,v 1.10 2005/12/25 15:08:36 steveu Exp $
 */

/*! \file */

/*! \page dds_tests_page Direct digital synthesis tests
\section dds_tests_page_sec_1 What does it do?
???.

\section dds_tests_page_sec_2 How does it work?
???.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <memory.h>
#include <tgmath.h>
#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"

#define OUTPUT_FILE_NAME            "dds.wav"
#define OUTPUT_FILE_NAME_COMPLEX    "complex_dds.wav"

int main(int argc, char *argv[])
{
    int i;
    uint32_t phase;
    int32_t phase_inc;
    int outframes;
    complex_t camp;
    int16_t buf[80000];
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
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    outhandle = afOpenFile(OUTPUT_FILE_NAME, "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }

    phase = 0;
    phase_inc = dds_phase_step(123.456789);
    for (i = 0;  i < 40000;  i++)
        buf[i] = alaw_to_linear(linear_to_alaw(dds(&phase, phase_inc)));
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              buf,
                              40000);
    if (outframes != 40000)
    {
        fprintf(stderr, "    Error writing wave file\n");
        exit(2);
    }

    phase_inc = dds_phase_step(12.3456789);
    for (i = 0;  i < 40000;  i++)
        buf[i] = alaw_to_linear(linear_to_alaw(dds(&phase, phase_inc)));
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              buf,
                              40000);
    if (outframes != 40000)
    {
        fprintf(stderr, "    Error writing wave file\n");
        exit(2);
    }

    phase_inc = dds_phase_step(2345.6789);
    for (i = 0;  i < 40000;  i++)
        buf[i] = alaw_to_linear(linear_to_alaw(dds(&phase, phase_inc)));
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              buf,
                              40000);
    if (outframes != 40000)
    {
        fprintf(stderr, "    Error writing wave file\n");
        exit(2);
    }

    phase_inc = dds_phase_step(3456.789);
    for (i = 0;  i < 40000;  i++)
        buf[i] = alaw_to_linear(linear_to_alaw(dds(&phase, phase_inc)));
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              buf,
                              40000);
    if (outframes != 40000)
    {
        fprintf(stderr, "    Error writing wave file\n");
        exit(2);
    }

    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }
    afFreeFileSetup(filesetup);

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

    outhandle = afOpenFile(OUTPUT_FILE_NAME_COMPLEX, "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUTPUT_FILE_NAME_COMPLEX);
        exit(2);
    }

    phase_inc = dds_phase_stepf(123.456789);
    for (i = 0;  i < 40000;  i++)
    {
        camp = dds_complexf(&phase, phase_inc);
        buf[2*i] = camp.re*10000.0;
        buf[2*i + 1] = camp.im*10000.0;
    }
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              buf,
                              40000);
    if (outframes != 40000)
    {
        fprintf(stderr, "    Error writing wave file\n");
        exit(2);
    }

    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUTPUT_FILE_NAME_COMPLEX);
        exit(2);
    }
    afFreeFileSetup(filesetup);

    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * plc_tests.c
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
 * $Id: plc_tests.c,v 1.11 2005/12/25 15:08:37 steveu Exp $
 */

/*! \page plc_tests_page Packet loss concealment tests
\section plc_tests_page_sec_1 What does it do?
These tests run a speech file through the packet loss concealment routines.
The loss rate, in percent, and the packet size, in samples, may be specified
on the command line.

\section plc_tests_page_sec_2 How are the tests run?
These tests process a speech file called pre_plc.wav. This file should contain
8000 sample/second 16 bits/sample linear audio. The tests read this file in
blocks, of a size specified on the command line. Some of these blocks are
dropped, to simulate packet loss. The rate of loss is also specified on the
command line. The PLC module is then used to reconstruct an acceptable
approximation to the original signal. The resulting audio is written to a new
audio file, called post_plc.wav. This file contains 8000 sample/second
16 bits/sample linear audio.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <tgmath.h>
#include <tiffio.h>

#include <audiofile.h>

#include "spandsp.h"

#define INPUT_FILE_NAME     "pre_plc.wav"
#define OUTPUT_FILE_NAME    "post_plc.wav"

int main(int argc, char *argv[])
{
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    plc_state_t plc;
    int inframes;
    int outframes;
    int16_t amp[1024];
    int i;
    int block_no;
    int lost_blocks;
    int block_len;
    int loss_rate;
    int dropit;
    int skip;

    loss_rate = 25;
    block_len = 80;
    if (argc > 1)
        loss_rate = atoi(argv[1]);
    if (argc > 2)
        block_len = atoi(argv[2]);
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

    inhandle = afOpenFile(INPUT_FILE_NAME, "r", NULL);
    if (inhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Failed to open wave file '%s'\n", INPUT_FILE_NAME);
        exit(2);
    }
    outhandle = afOpenFile(OUTPUT_FILE_NAME, "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Failed to open wave file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }
    plc_init(&plc);
    lost_blocks = 0;
    for (block_no = 0;  ;  block_no++)
    {
        inframes = afReadFrames(inhandle,
                                AF_DEFAULT_TRACK,
                                amp,
                                block_len);
        if (inframes != block_len)
            break;
        dropit = rand()/(RAND_MAX/100);
        if (dropit > loss_rate)
            plc_rx(&plc, amp, inframes);
        else
            plc_fillin(&plc, amp, inframes);
        if (dropit <= loss_rate)
            lost_blocks++;
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  inframes);
        if (outframes != inframes)
        {
            fprintf(stderr, "    Error writing out sound\n");
            exit(2);
        }
    }
    printf("Dropped %d of %d blocks\n", lost_blocks, block_no);
    if (afCloseFile(inhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", INPUT_FILE_NAME);
        exit(2);
    }
    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }
    afFreeFileSetup(filesetup);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

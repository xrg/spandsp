/*
 * SpanDSP - a series of DSP components for telephony
 *
 * ec_disable_detector_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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
 * $Id: ec_disable_detector_tests.c,v 1.4 2004/12/08 14:00:36 steveu Exp $
 */

/*! \page echo_cancellor_disable_tone_detection_tests_page Echo canceller disable tone tests
\section echo_cancellor_disable_tone_detection_tests_page_sec_1 What does it do?
*/

#define _ISOC9X_SOURCE  1
#define _ISOC99_SOURCE  1

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

#define BELLCORE_DIR    "/home/iso/bellcore/"

#define FALSE 0
#define TRUE (!FALSE)

char *bellcore_files[] =
{
    BELLCORE_DIR "tr-tsy-00763-1.wav",
    BELLCORE_DIR "tr-tsy-00763-2.wav",
    BELLCORE_DIR "tr-tsy-00763-3.wav",
    BELLCORE_DIR "tr-tsy-00763-4.wav",
    BELLCORE_DIR "tr-tsy-00763-5.wav",
    BELLCORE_DIR "tr-tsy-00763-6.wav",
    ""
};

int main(int argc, char *argv[])
{
    int i;
    int j;
    int pitch;
    int16_t amp[8000];
    echo_can_disable_rx_state_t echo_det;
    echo_can_disable_tx_state_t echo_dis;
    awgn_state_t chan_noise_source;
    int len;
    int hits;
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int frames;
    int outframes;
    int samples;
    float x;
    
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

    printf("Test 1: Simple generation to a file\n");
    outhandle = afOpenFile("ec_disable.wav", "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", "ec_disable.wav");
        exit(2);
    }
    /* Some with modulation */
    echo_can_disable_tone_tx_init(&echo_dis, TRUE);
    for (i = 0;  i < 1000;  i++)
    {
        samples = echo_can_disable_tone_tx(&echo_dis, amp, 160);
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
    /* Some without modulation */
    echo_can_disable_tone_tx_init(&echo_dis, FALSE);
    for (i = 0;  i < 1000;  i++)
    {
        samples = echo_can_disable_tone_tx(&echo_dis, amp, 160);
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
    if (afCloseFile(outhandle) != 0)
    {
        printf("    Cannot close wave file '%s'\n", "ec_disable.wav");
        exit(2);
    }
    
    printf("Test 2: Basic detection\n");
    awgn_init(&chan_noise_source, 7162534, -50);
    for (pitch = 2000;  pitch < 2200;  pitch++)
    {
        /* Use the transmitter to test the receiver */
        echo_can_disable_tone_tx_init(&echo_dis, FALSE);
        /* Fudge things for the test */
        echo_dis.tone_phase_rate = dds_phase_step(pitch);
        echo_dis.level = dds_scaling(-25);
        echo_can_disable_tone_rx_init(&echo_det);
        for (i = 0;  i < 500;  i++)
        {
            samples = echo_can_disable_tone_tx(&echo_dis, amp, 160);
            for (j = 0;  j < samples;  j++)
                amp[j] += awgn(&chan_noise_source);
            /*endfor*/
            echo_can_disable_tone_rx(&echo_det, amp, samples);
        }
        if (echo_det.hit)
            printf("%5d %12d %12d %d\n", pitch, echo_det.channel_level, echo_det.notch_level, echo_det.hit);
    }    

    /* Talk-off test */
    /* Here we use the BellCore talk off test tapes, intended for DTMF detector
       testing. Presumably they should also have value here, but I am not sure.
       If those voice snippets were chosen to be tough on DTMF detectors, they
       might go easy on detectors looking for different pitches. However, the
       Mitel DTMF test tape is known (the hard way) to exercise 2280Hz tone
       detectors quite well. */
    printf("Test 3: Talk-off test\n");
    echo_can_disable_tone_rx_init(&echo_det);
    for (j = 0;  bellcore_files[j][0];  j++)
    {
        inhandle = afOpenFile(bellcore_files[j], "r", 0);
        if (inhandle == AF_NULL_FILEHANDLE)
        {
            printf ("    Cannot open speech file '%s'\n", bellcore_files[j]);
            exit (2);
        }
        x = afGetFrameSize(inhandle, AF_DEFAULT_TRACK, 1);
        if (x != 2.0)
        {
            printf ("    Unexpected frame size in speech file '%s'\n", bellcore_files[j]);
            exit (2);
        }

        hits = 0;
        /* The input is a wave file, with a header. Just ignore that, and
           scan the whole file. The header will not cause detections, and
           what follows is guranteed to be word aligned. This does assume
           the wave files are the expected ones in 16 bit little endian PCM. */
        while ((frames = afReadFrames(inhandle, AF_DEFAULT_TRACK, amp, 8000)))
        {
            if (echo_can_disable_tone_rx(&echo_det, amp, len/sizeof (int16_t)))
            {
                /* This is not a true measure of hits, as there might be more
                   than one in a block of data. However, since the only good
                   result is no hits, this approximation is OK. */
                hits++;
            }
        }
        if (afCloseFile(inhandle) != 0)
        {
            printf("    Cannot close speech file '%s'\n", bellcore_files[j]);
            exit(2);
        }
        printf("    File %d gave %d false hits.\n", j + 1, hits);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

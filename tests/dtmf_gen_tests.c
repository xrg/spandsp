/*
 * SpanDSP - a series of DSP components for telephony
 *
 * dtmf_gen_tests.c - Test the DTMF generator.
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
 * $Id: dtmf_gen_tests.c,v 1.5 2005/09/01 17:06:45 steveu Exp $
 */

//#define _ISOC9X_SOURCE	1
//#define _ISOC99_SOURCE	1

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

int main (int argc, char *argv[])
{
    dtmf_tx_state_t gen;
    int16_t amp[16384];
    int len;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int outframes;
    int add_digits;

    filesetup = afNewFileSetup ();
    if (filesetup == AF_NULL_FILESETUP)
    {
    	fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, 8000.0);
    //afInitCompression(filesetup, AF_DEFAULT_TRACK, AF_COMPRESSION_G711_ALAW);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    outhandle = afOpenFile ("audio.wav", "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open audio file '%s'\n", "audio.wav");
        exit(2);
    }

    bell_mf_gen_init();
    bell_mf_tx_init(&gen);

    len = dtmf_tx(&gen, amp, 16384);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (dtmf_put(&gen, "123"))
        printf("Ooops\n");
    len = dtmf_tx(&gen, amp, 16384);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (dtmf_put(&gen, "456"))
        printf("Ooops\n");
    len = dtmf_tx(&gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (dtmf_put(&gen, "789"))
        printf("Ooops\n");
    len = dtmf_tx(&gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (dtmf_put(&gen, "*#"))
        printf("Ooops\n");
    len = dtmf_tx(&gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    add_digits = 1;
    do
    {
        len = dtmf_tx(&gen, amp, 160);
        printf("Generated %d samples\n", len);
        if (len > 0)
        {
            outframes = afWriteFrames(outhandle,
   	         	    	      AF_DEFAULT_TRACK,
		    	              amp,
			              len);
        }
        if (add_digits)
        {
            if (dtmf_put(&gen, "1234567890"))
            {
                printf("Digit buffer full\n");
                add_digits = 0;
            }
        }
    }
    while (len > 0);

    dtmf_gen_init();
    dtmf_tx_init(&gen);
    len = dtmf_tx(&gen, amp, 16384);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (dtmf_put(&gen, "123"))
        printf("Ooops\n");
    len = dtmf_tx(&gen, amp, 16384);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (dtmf_put(&gen, "456"))
        printf("Ooops\n");
    len = dtmf_tx(&gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (dtmf_put(&gen, "789"))
        printf("Ooops\n");
    len = dtmf_tx(&gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (dtmf_put(&gen, "0*#"))
        printf("Ooops\n");
    len = dtmf_tx(&gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (dtmf_put(&gen, "ABCD"))
        printf("Ooops\n");
    len = dtmf_tx(&gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    add_digits = 1;
    do
    {
        len = dtmf_tx(&gen, amp, 160);
        printf("Generated %d samples\n", len);
        if (len > 0)
        {
            outframes = afWriteFrames(outhandle,
   	         	    	      AF_DEFAULT_TRACK,
		    	              amp,
			              len);
        }
        if (add_digits)
        {
            if (dtmf_put(&gen, "1234567890"))
            {
                printf("Digit buffer full\n");
                add_digits = 0;
            }
        }
    }
    while (len > 0);

    if (afCloseFile (outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", "audio.wav");
        exit (2);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * ec_disable_detector_test.c
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
 */

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

#define BELLCORE_DIR	"/home/iso/bellcore/"

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

int main (int argc, char *argv[])
{
    int i;
    int j;
    int pitch;
    int16_t amp[8000];
    echo_can_disable_detector_state_t echo_det;
    awgn_state_t chan_noise_source;
    tone_gen_descriptor_t tone_desc;
    tone_gen_state_t tone_state;
    int hit_types[256];
    int len;
    int hits;
    AFfilehandle inhandle;
    int frames;
    float x;
    
    printf ("Test 1: Basic detection\n");
    awgn_init (&chan_noise_source, 7162534, -40);
    for (pitch = 2000;  pitch < 2200;  pitch++)
    {
    	make_tone_gen_descriptor (&tone_desc,
	    	    	          pitch,
				  -25,
				  0,
				  0,
				  1,
				  0,
				  0,
				  0,
				  TRUE);
	tone_gen_init (&tone_state, &tone_desc);
        echo_can_disable_detector_init (&echo_det);
    	for (i = 0;  i < 5;  i++)
	{
            tone_gen (&tone_state, amp, 8000);
            for (j = 0;  j < 8000;  j++)
	    {
		/* Phase invert every 450ms */
	        if ((i*8000 + j)%(450*8*2) >= 450*8)
	    	    amp[j] = -amp[j];
	        /*endif*/
	    	amp[j] += awgn (&chan_noise_source);
            }
            /*endfor*/
            if (echo_can_disable_detector_update (&echo_det, amp, 8000))
                break;
	}
	if (echo_det.hit)
            printf ("%5d %12d %12d %d\n", pitch, echo_det.channel_level, echo_det.notch_level, echo_det.hit);
    }    

    /* Talk-off test */
    /* Here we use the BellCore talk off test tapes, intended for DTMF detector
       testing. Presumably they should also have value here, but I am not sure.
       If those voice snippets were chosen to be tough on DTMF detectors, they
       might go easy on detectors looking for different pitches. However, the
       Mitel DTMF test tape is known (the hard way) to exercise 2280Hz tone
       detectors quite well. */
    printf ("Test 2: Talk-off test\n");
    memset (hit_types, '\0', sizeof(hit_types));
    echo_can_disable_detector_init (&echo_det);
    for (j = 0;  bellcore_files[j][0];  j++)
    {
        inhandle = afOpenFile (bellcore_files[j], "r", 0);
    	if (inhandle == AF_NULL_FILEHANDLE)
    	{
    	    printf ("    Cannot open speech file '%s'\n", bellcore_files[j]);
	    exit (2);
    	}
        x = afGetFrameSize (inhandle, AF_DEFAULT_TRACK, 1);
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
        while ((frames = afReadFrames (inhandle, AF_DEFAULT_TRACK, amp, 8000)))
    	{
            if (echo_can_disable_detector_update (&echo_det, amp, len/sizeof (int16_t)))
	    {
	    	/* This is not a true measure of hits, as there might be more
		   than one in a block of data. However, since the only good
		   result is no hits, this approximation is OK. */
	    	hits++;
	    }
    	}
        if (afCloseFile (inhandle) != 0)
    	{
    	    printf ("    Cannot close speech file '%s'\n", bellcore_files[j]);
	    exit (2);
    	}
	printf ("    File %d gave %d false hits.\n", j + 1, hits);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

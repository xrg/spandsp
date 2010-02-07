/*
 * SpanDSP - a series of DSP components for telephony
 *
 * tone_generate_tests.c
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
 * $Id: tone_generate_tests.c,v 1.3 2004/12/08 14:00:36 steveu Exp $
 */

/*! \page tone_generation_tests_page Tone generation tests
\section tone_generation_tests_page_sec_1 What does it do
*/

/* "We can generate exactly repetitive tones" demo */

#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <tiffio.h>

#include "spandsp.h"

int main(int argc, char *argv[])
{
    tone_gen_descriptor_t tone_desc;
    tone_gen_state_t tone_state;
    int pitch;
    int level;
    int i;
    int longest_span;
    int16_t amp[1];
    int initial_v2;
    int initial_v3;


    /* Try this test for all telephony frequencies (1Hz steps), and varying
       levels from clip (+3dBm) down, in 1dB steps. This takes ages to run,
       but I like to be thorough :) Enjoy coffee, the arms of your lover,
       or whatever takes your fancy for a while */

    /* For each test we generate the tone until it repeats exactly, or the
       world ends (or until we reach 0x7FFFFFFF samples, whichever occurs
       first). */
    printf ("Test 1: An exhaustive check that all tones are repeatable.\n");
    printf ("        This takes a while!\n");
    longest_span = 0;
    for (level = +3;  level > -55;  level--)
    {
    	for (pitch = 300;  pitch < 3400;  pitch++)
    	{
	    make_tone_gen_descriptor (&tone_desc,
		    	    	      pitch,
				      level,
				      0,
				      0,
				      1,
				      0,
				      0,
				      0,
				      TRUE);
	    tone_gen_init (&tone_state, &tone_desc);
            /* Remember the starting conditions, and wait for them to repeat. If
               they ever do, the oscillator must be long term stable. */
            initial_v2 = tone_desc.v2_1;
            initial_v3 = tone_desc.v3_1;
            /* If these conditions do not repeat before a 32 bit int rolls over,
               something is probably amiss! */
            for (i = 0;  i < 0x7FFFFFFF;  i++)
            {
            	tone_gen (&tone_state, amp, 1);
                //printf ("%12d %12d %12d %12d %12d\n", tone_state.v2_1, tone_state.v3_1,initial_v2, initial_v3, amp[0]);
                if (initial_v2 == tone_state.v2_1  &&  initial_v3 == tone_state.v3_1)
                {
            	    //printf ("%4dHz tone repeats accurately with an offset of %d samples at %ddBm\n", pitch, i, level);
                    if (i > longest_span)
                        longest_span = i;
                    break;
                }
            }
            if (i == 0x7FFFFFFF)
	    {
	    	printf ("ERROR: %dHz tone does not repeat accurately at %ddBm\n", pitch, level);
	    	exit (2);
	    }
    	}
    	printf ("All OK at %ddBm\n", level);
    }
    printf ("Hurrah! It worked.\n");
    printf ("Things eventually repeated for every frequency, and level tested.\n");
    printf ("The longest repeat span was %d\n", longest_span);
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

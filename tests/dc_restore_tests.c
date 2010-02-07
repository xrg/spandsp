/*
 * SpanDSP - a series of DSP components for telephony
 *
 * dc_restore_test.c - Tests for the dc_restore functions.
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
 * $Id: dc_restore_tests.c,v 1.8 2005/09/01 17:06:45 steveu Exp $
 */

/*! \page dc_restore_tests_page DC restoration tests
\section dc_restore_tests_page_sec_1 What does it do?
*/

#define _ISOC9X_SOURCE  1
#define _ISOC99_SOURCE  1

#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <memory.h>
#include <tgmath.h>
#include <time.h>
#include <tiffio.h>

#include "spandsp.h"
    
int main (int argc, char *argv[])
{
    awgn_state_t noise_source;
    dc_restore_state_t dc_state;
    int i;
    int idum = 1234567;
    int16_t dirty;
    int16_t clean;

    awgn_init (&noise_source, idum, -10);
    dc_restore_init (&dc_state);
    for (i = 0;  i < 100000;  i++)
    {
    	dirty = awgn (&noise_source) + 5000;
        clean = dc_restore (&dc_state, dirty);
	//if ((i % 1000) == 0)
	{
            printf ("Sample %6d: %d (expect %d)\n",
	    	    i,
	    	    dc_restore_estimate (&dc_state),
		    5000);
	}
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

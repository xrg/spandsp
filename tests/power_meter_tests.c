/*
 * SpanDSP - a series of DSP components for telephony
 *
 * power_meter_tests.c
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
 * $Id: power_meter_tests.c,v 1.2 2004/03/12 16:27:25 steveu Exp $
 */

#define _ISOC9X_SOURCE  1
#define _ISOC99_SOURCE  1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <tiffio.h>

#include "spandsp.h"
    
int main(int argc, char *argv[])
{
    awgn_state_t noise_source;
    power_meter_t meter;
    tone_gen_descriptor_t tone_desc;
    tone_gen_state_t gen;
    int i;
    int idum = 1234567;
    int16_t amp[1000];
    int len;
    int32_t level;

    make_tone_gen_descriptor(&tone_desc,
                             1000,
                             -10,
                             0,
                             1,
                             1,
                             0,
                             0,
                             0,
                             TRUE);
    tone_gen_init(&gen, &tone_desc);
    awgn_init(&noise_source, idum, -10);
    power_meter_init(&meter, 7);

    /* Check with a tone */
    len = tone_gen(&gen, amp, 1000);
    for (i = 0;  i < len;  i++)
    {
        power_meter_update(&meter, amp[i]);
        printf("%f\n", power_meter_dbm0(&meter));
    }
    /* Check with noise */
    for (i = 0;  i < 1000;  i++)
        amp[i] = awgn(&noise_source);
    for (i = 0;  i < 1000;  i++)
    {
        level = power_meter_update(&meter, amp[i]);
        printf("%12d %f\n", level, power_meter_dbm0(&meter));
    }
    level = power_meter_level(-10);
    printf("Expected level %d\n", level);
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

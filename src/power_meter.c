/*
 * SpanDSP - a series of DSP components for telephony
 *
 * power_meter.c
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
 * $Id: power_meter.c,v 1.7 2005/08/31 19:27:52 steveu Exp $
 */

#define	_ISOC9X_SOURCE	1
#define _ISOC99_SOURCE	1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/power_meter.h"

power_meter_t *power_meter_init(power_meter_t *meter, int shift)
{
    meter->shift = shift;
    meter->reading = 0;
    return  meter;
}
/*- End of function --------------------------------------------------------*/

power_meter_t *power_meter_damping(power_meter_t *meter, int shift)
{
    meter->shift = shift;
    return  meter;
}
/*- End of function --------------------------------------------------------*/

int32_t power_meter_update(power_meter_t *meter, int16_t amp)
{
    meter->reading += ((amp*amp - meter->reading) >> meter->shift);
    return meter->reading;
}
/*- End of function --------------------------------------------------------*/

int32_t power_meter_level(float level)
{
    float l;

    l = pow(10.0, (level - 3.14)/20.0)*(32768.0*0.70711);
    return l*l;
}
/*- End of function --------------------------------------------------------*/

float power_meter_dbm0(power_meter_t *meter)
{
    float val;
    
    if ((val = sqrt((float) meter->reading)) <= 0.0)
        return -INFINITY;
    return log10(val/(32768.0*0.70711))*20.0 + 3.14;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

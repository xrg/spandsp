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
 * $Id: power_meter.c,v 1.12 2005/11/27 12:36:22 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/power_meter.h"

power_meter_t *power_meter_init(power_meter_t *s, int shift)
{
    if (s == NULL)
        return NULL;
    s->shift = shift;
    s->reading = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

power_meter_t *power_meter_damping(power_meter_t *s, int shift)
{
    s->shift = shift;
    return s;
}
/*- End of function --------------------------------------------------------*/

int32_t power_meter_update(power_meter_t *s, int16_t amp)
{
    s->reading += ((amp*amp - s->reading) >> s->shift);
    return s->reading;
}
/*- End of function --------------------------------------------------------*/

int32_t power_meter_level_dbm0(float level)
{
    float l;

    l = pow(10.0, (level - 3.14)/20.0)*(32768.0*0.70711);
    return l*l;
}
/*- End of function --------------------------------------------------------*/

int32_t power_meter_level_dbov(float level)
{
    float l;

    l = pow(10.0, (level + 3.14)/20.0)*(32768.0*0.70711);
    return l*l;
}
/*- End of function --------------------------------------------------------*/

float power_meter_dbm0(power_meter_t *s)
{
    float val;
    
    if ((val = sqrt((float) s->reading)) <= 0.0)
        return FLT_MIN;
    return log10(val/(32768.0*0.70711))*20.0 + 3.14;
}
/*- End of function --------------------------------------------------------*/

float power_meter_dbov(power_meter_t *s)
{
    float val;
    
    if ((val = sqrt((float) s->reading)) <= 0.0)
        return FLT_MIN;
    return log10(val/(32768.0*0.70711))*20.0 - 3.14;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * power_meter.h
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
 * $Id: power_meter.h,v 1.4 2005/01/19 14:40:20 steveu Exp $
 */

#if !defined(_POWER_METER_H_)
#define _POWER_METER_H_

/*! \page power_meter_page Power metering

\section power_meter_page_sec_1 What does it do?
The power metering module implements a simple IIR type running power meter. The damping
factor of the IIR is selectable when the meter instance is created.

\section power_meter_page_sec_2 How does it work?
*/

/*!
    Power meter descriptor. This defines the working state for a
    single instance of a power measurement device.
*/
typedef struct
{
    int shift;

    int32_t reading;
} power_meter_t;

#ifdef __cplusplus
extern "C" {
#endif

power_meter_t *power_meter_init(power_meter_t *meter, int shift);
power_meter_t *power_meter_damping(power_meter_t *meter, int shift);
int32_t power_meter_update(power_meter_t *meter, int16_t amp);
int32_t power_meter_level(float level);
float power_meter_dbm0(power_meter_t *meter);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

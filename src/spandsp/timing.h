/*
 * SpanDSP - a series of DSP components for telephony
 *
 * timing.h - Provide access to the Pentium/Athlon TSC timer register
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
 * $Id: timing.h,v 1.2 2004/03/30 14:29:41 steveu Exp $
 */

#if !defined(_TIMING_H_)
#define _TIMING_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__i386__)
static inline uint64_t rdtscll(void)
{
    uint64_t now;

    __asm__ __volatile__(" rdtsc\n" : "=A" (now));
    return now;
}
/*- End of function --------------------------------------------------------*/
#endif

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

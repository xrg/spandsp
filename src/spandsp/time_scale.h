/*
 * SpanDSP - a series of DSP components for telephony
 *
 * time_scale.h - Time scaling for linear speech data
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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
 * $Id: time_scale.h,v 1.1 2004/10/14 13:47:16 steveu Exp $
 */

#if !defined(_TIME_SCALE_H_)
#define _TIME_SCALE_H_

/*! \page Time scaling speech
    Time scaling for speech, based on the Pointer Interval Controlled
    OverLap and Add (PICOLA) method, developed by Morita Naotaka.

    Mikio Ikeda has an excellent web page on this subject at
    http://keizai.yokkaichi-u.ac.jp/~ikeda/research/picola.html
    There is also working code there. This implementation uses
    exactly the same algorithms, but the code is a complete rewrite.
    Mikio's code batch processes files. This version works incrementally
    on streams, and allows multiple streams to be processed concurrently.
*/

#define TIME_SCALE_MIN_PITCH    60
#define TIME_SCALE_MAX_PITCH    250
#define TIME_SCALE_BUF_LEN      (2*SAMPLE_RATE/TIME_SCALE_MIN_PITCH)

typedef struct
{
    double rate;
    double rcomp;
    double rate_nudge;
    int fill;
    int lcp;
    int16_t buf[TIME_SCALE_BUF_LEN];
} time_scale_t;

#ifdef __cplusplus
extern "C" {
#endif

int time_scale(time_scale_t *s, int16_t out[], int16_t in[], int len);
int time_scale_init(time_scale_t *s, float rate);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

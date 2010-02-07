/*
 * SpanDSP - a series of DSP components for telephony
 *
 * vector.h
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
 * $Id: vector.h,v 1.1 2004/03/12 16:27:25 steveu Exp $
 */

#if !defined(_VECTOR_H_)
#define _VECTOR_H_

static inline int vec_dot_prod(const int16_t *vec1,
                               const int16_t *vec2,
                               int len)
{
    int i;
    int sum;

    sum = 0;
    for (i = 0;  i < len;  i++)
        sum += vec1[i]*vec2[i];
    return sum;
}

static inline int vec_norm2(const int16_t *vec, int len)
{
    int i;
    int sum;

    sum = 0;
    for (i = 0;  i < len;  i++)
        sum += vec[i]*vec[i];
    return sum;
}

static inline void vec_sar(int16_t *vec, int len, int shift)
{
    int i;

    for (i = 0;  i < len;  i++)
        vec[i] >>= shift;
}

static inline int vec_max_bits(const int16_t *vec, int len)
{
    int i;
    int max;
    int v;
    int b;

    max = 0;
    for (i = 0;  i < len;  i++)
    {
        v = abs(vec[i]);
        if (v > max)
            max = v;
    }
    b = 0;
    while (max != 0)
    {
        b++;
        max >>= 1;
    }
    return b;
}

#endif
/*- End of file ------------------------------------------------------------*/

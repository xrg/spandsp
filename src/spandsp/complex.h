/*
 * SpanDSP - a series of DSP components for telephony
 *
 * complex.h
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
 * $Id: complex.h,v 1.3 2005/11/23 17:09:47 steveu Exp $
 */

/*! \file */

/*! \page complex_page Complex number support
\section complex_page_sec_1 What does it do?
Complex number support is part of the C99 standard. However, support for this
in C compilers is still patchy. A set of complex number feaures is provided as
a "temporary" measure, until native C language complex number support is
widespread.
*/

#if !defined(_COMPLEX_H_)
#define _COMPLEX_H_

/*!
    Complex type.
*/
typedef struct
{
    float re;
    float im;
} complex_t;

/*!
    Complex integer type.
*/
typedef struct
{
    int re;
    int im;
} icomplex_t;

/*!
    Complex 16 bit integer type.
*/
typedef struct
{
    int16_t re;
    int16_t im;
} i16complex_t;

/*!
    Complex 32 bit integer type.
*/
typedef struct
{
    int32_t re;
    int32_t im;
} i32complex_t;

#ifdef __cplusplus
extern "C" {
#endif

static inline complex_t complex_set(float re, float im)
{
    complex_t z;

    z.re = re;
    z.im = im;
    return z;
}
/*- End of function --------------------------------------------------------*/

static inline icomplex_t icomplex_set(int re, int im)
{
    icomplex_t z;

    z.re = re;
    z.im = im;
    return z;
}
/*- End of function --------------------------------------------------------*/

static inline complex_t complex_add(const complex_t *x, const complex_t *y)
{
    complex_t z;

    z.re = x->re + y->re;
    z.im = x->im + y->im;
    return z;
}
/*- End of function --------------------------------------------------------*/

static inline icomplex_t icomplex_add(const icomplex_t *x, const icomplex_t *y)
{
    icomplex_t z;

    z.re = x->re + y->re;
    z.im = x->im + y->im;
    return z;
}
/*- End of function --------------------------------------------------------*/

static inline complex_t complex_sub(const complex_t *x, const complex_t *y)
{
    complex_t z;

    z.re = x->re - y->re;
    z.im = x->im - y->im;
    return z;
}
/*- End of function --------------------------------------------------------*/

static inline icomplex_t icomplex_sub(const icomplex_t *x, const icomplex_t *y)
{
    icomplex_t z;

    z.re = x->re - y->re;
    z.im = x->im - y->im;
    return z;
}
/*- End of function --------------------------------------------------------*/

static inline complex_t complex_mul(const complex_t *x, const complex_t *y)
{
    complex_t z;

    z.re = x->re*y->re - x->im*y->im;
    z.im = x->re*y->im + x->im*y->re;
    return z;
}
/*- End of function --------------------------------------------------------*/

static inline complex_t complex_div(const complex_t *x, const complex_t *y)
{
    complex_t z;
    float f;
    
    f = y->re*y->re + y->im*y->im;
    z.re = ( x->re*y->re + x->im*y->im)/f;
    z.im = (-x->re*y->im + x->im*y->re)/f;
    return z;
}
/*- End of function --------------------------------------------------------*/

static inline complex_t complex_conj(const complex_t *x)
{
    complex_t z;

    z.re = x->re;
    z.im = -x->im;
    return z;
}
/*- End of function --------------------------------------------------------*/

static inline icomplex_t icomplex_conj(const icomplex_t *x)
{
    icomplex_t z;

    z.re = x->re;
    z.im = -x->im;
    return z;
}
/*- End of function --------------------------------------------------------*/

static inline float power(const complex_t *x)
{
    return x->re*x->re + x->im*x->im;
}
/*- End of function --------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * alaw_ulaw.h - In line A-law and u-law conversion routines
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
 * $Id: alaw_ulaw.h,v 1.13 2006/01/18 13:21:37 steveu Exp $
 */

/*! \file */

/*! \page alaw_ulaw_page A-law and mu-law handling
Lookup tables for A-law and u-law look attractive, until you consider the impact
on the CPU cache. If it causes a substantial area of your processor cache to get
hit too often, cache sloshing will severely slow things down. The main reason
these routines are slow in C, is the lack of direct access to the CPU's "find
the first 1" instruction. A little in-line assembler fixes that, and the
conversion routines can be faster than lookup tables, in most real world usage.
A "find the first 1" instruction is available on most modern CPUs, and is a
much underused feature. 

If an assembly language method of bit searching is not available, these routines
revert to a method that can be a little slow, so the cache thrashing might not
seem so bad :(

Feel free to submit patches to add fast "find the first 1" support for your own
favourite processor.
*/

#if !defined(_ALAW_ULAW_H_)
#define _ALAW_ULAW_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__i386__)
static __inline__ int top_bit(unsigned int bits)
{
    int res;

    __asm__ __volatile__(" movl $-1,%%edx;\n"
                         " bsrl %%eax,%%edx;\n"
                         : "=d" (res)
                         : "a" (bits));
    return res;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int bottom_bit(unsigned int bits)
{
    int res;

    __asm__ __volatile__(" movl $-1,%%edx;\n"
                         " bsfl %%eax,%%edx;\n"
                         : "=d" (res)
                         : "a" (bits));
    return res;
}
/*- End of function --------------------------------------------------------*/
#elif defined(__x86_64__)
static __inline__ int top_bit(unsigned int bits)
{
    int res;

    __asm__ __volatile__(" movq $-1,%%rdx;\n"
                         " bsrq %%rax,%%rdx;\n"
                         : "=d" (res)
                         : "a" (bits));
    return res;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int bottom_bit(unsigned int bits)
{
    int res;

    __asm__ __volatile__(" movq $-1,%%rdx;\n"
                         " bsfq %%eax,%%edx;\n"
                         : "=d" (res)
                         : "a" (bits));
    return res;
}
/*- End of function --------------------------------------------------------*/
#else
static __inline__ int top_bit(unsigned int bits)
{
    int i;
    
    if (bits == 0)
        return -1;
    i = 0;
    if (bits & 0xFFFF0000)
    {
        bits &= 0xFFFF0000;
        i += 16;
    }
    if (bits & 0xFF00FF00)
    {
        bits &= 0xFF00FF00;
        i += 8;
    }
    if (bits & 0xF0F0F0F0)
    {
        bits &= 0xF0F0F0F0;
        i += 4;
    }
    if (bits & 0xCCCCCCCC)
    {
        bits &= 0xCCCCCCCC;
        i += 2;
    }
    if (bits & 0xAAAAAAAA)
    {
        bits &= 0xAAAAAAAA;
        i += 1;
    }
    return i;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int bottom_bit(unsigned int bits)
{
    int i;
    
    if (bits == 0)
        return -1;
    i = 32;
    if (bits & 0x0000FFFF)
    {
        bits &= 0x0000FFFF;
        i -= 16;
    }
    if (bits & 0x00FF00FF)
    {
        bits &= 0x00FF00FF;
        i -= 8;
    }
    if (bits & 0x0F0F0F0F)
    {
        bits &= 0x0F0F0F0F;
        i -= 4;
    }
    if (bits & 0x33333333)
    {
        bits &= 0x33333333;
        i -= 2;
    }
    if (bits & 0x55555555)
    {
        bits &= 0x55555555;
        i -= 1;
    }
    return i;
}
/*- End of function --------------------------------------------------------*/
#endif

/* N.B. It is tempting to use look-up tables for A-law and u-law conversion.
 *      However, you should consider the cache footprint.
 *
 *      A 64K byte table for linear to x-law and a 512 byte sound like peanuts
 *      these days, and shouldn't an array lookup be real fast? No! When the
 *      cache sloshes as badly as this one will, a tight calculation is better.
 *      The messiest part is normally finding the segment, but a little inline
 *      assembly can fix that on an i386.
 */
 
/*
 * Mu-law is basically as follows:
 *
 *      Biased Linear Input Code        Compressed Code
 *      ------------------------        ---------------
 *      00000001wxyza                   000wxyz
 *      0000001wxyzab                   001wxyz
 *      000001wxyzabc                   010wxyz
 *      00001wxyzabcd                   011wxyz
 *      0001wxyzabcde                   100wxyz
 *      001wxyzabcdef                   101wxyz
 *      01wxyzabcdefg                   110wxyz
 *      1wxyzabcdefgh                   111wxyz
 *
 * Each biased linear code has a leading 1 which identifies the segment
 * number. The value of the segment number is equal to 7 minus the number
 * of leading 0's. The quantization interval is directly available as the
 * four bits wxyz.  * The trailing bits (a - h) are ignored.
 *
 * Ordinarily the complement of the resulting code word is used for
 * transmission, and so the code word is complemented before it is returned.
 *
 * For further information see John C. Bellamy's Digital Telephony, 1982,
 * John Wiley & Sons, pps 98-111 and 472-476.
 */

//#define ZEROTRAP                      /* turn on the trap as per the MIL-STD */
#define BIAS             0x84           /* Bias for linear code. */

static __inline__ uint8_t linear_to_ulaw(int linear)
{
    uint8_t u_val;
    int mask;
    int seg;

    /* Get the sign and the magnitude of the value. */
    if (linear < 0)
    {
        linear = BIAS - linear;
        mask = 0x7F;
    }
    else
    {
        linear = BIAS + linear;
        mask = 0xFF;
    }

    seg = top_bit(linear | 0xFF) - 7;

    /*
     * Combine the sign, segment, quantization bits,
     * and complement the code word.
     */
    if (seg >= 8)
        u_val = 0x7F ^ mask;
    else
        u_val = ((seg << 4) | ((linear >> (seg + 3)) & 0xF)) ^ mask;
#ifdef ZEROTRAP
    /* optional ITU trap */
    if (u_val == 0)
        u_val = 0x02;
#endif
    return  u_val;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t ulaw_to_linear(uint8_t ulaw)
{
    int t;
    
    /* Complement to obtain normal u-law value. */
    ulaw = ~ulaw;
    /*
     * Extract and bias the quantization bits. Then
     * shift up by the segment number and subtract out the bias.
     */
    t = (((ulaw & 0x0F) << 3) + BIAS) << (((int) ulaw & 0x70) >> 4);
    return  ((ulaw & 0x80)  ?  (BIAS - t) : (t - BIAS));
}
/*- End of function --------------------------------------------------------*/

/*
 * A-law is basically as follows:
 *
 *      Linear Input Code        Compressed Code
 *      -----------------        ---------------
 *      0000000wxyza             000wxyz
 *      0000001wxyza             001wxyz
 *      000001wxyzab             010wxyz
 *      00001wxyzabc             011wxyz
 *      0001wxyzabcd             100wxyz
 *      001wxyzabcde             101wxyz
 *      01wxyzabcdef             110wxyz
 *      1wxyzabcdefg             111wxyz
 *
 * For further information see John C. Bellamy's Digital Telephony, 1982,
 * John Wiley & Sons, pps 98-111 and 472-476.
 */

#define AMI_MASK        0x55

static __inline__ uint8_t linear_to_alaw(int linear)
{
    int mask;
    int seg;
    
    if (linear >= 0)
    {
        /* Sign (7th) bit = 1 */
        mask = AMI_MASK | 0x80;
    }
    else
    {
        /* Sign (7th) bit = 0 */
        mask = AMI_MASK;
        linear = -linear - 8;
    }

    /* Convert the scaled magnitude to segment number. */
    seg = top_bit(linear | 0xFF) - 7;
    if (seg >= 8)
    {
        if (linear >= 0)
        {
            /* Out of range. Return maximum value. */
            return (0x7F ^ mask);
        }
        /* We must be just a tiny step below zero */
        return (0x00 ^ mask);
    }
    /* Combine the sign, segment, and quantization bits. */
    return  ((seg << 4) | ((linear >> ((seg)  ?  (seg + 3)  :  4)) & 0x0F)) ^ mask;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t alaw_to_linear(uint8_t alaw)
{
    int i;
    int seg;

    alaw ^= AMI_MASK;
    i = ((alaw & 0x0F) << 4);
    seg = (((int) alaw & 0x70) >> 4);
    if (seg)
        i = (i + 0x108) << (seg - 1);
    else
        i += 8;
    return (int16_t) ((alaw & 0x80)  ?  i  :  -i);
}
/*- End of function --------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

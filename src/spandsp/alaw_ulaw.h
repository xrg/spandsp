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
 * $Id: alaw_ulaw.h,v 1.2 2004/03/19 19:12:46 steveu Exp $
 */

/*! \file */

/*! \page ALaw_uLaw_page A-law and mu-law handling
Lookup tables for A-law and u-law look attractive, until you consider the impact
on the CPU cache. If it causes a substantial area of your processor cache to get
hit too often, cache sloshing will severely slow things down. The main reason
these routines are slow in C, is the lack of direct access to the CPU's "find
the first 1" instruction. A little in-line assembler fixes that, and the
conversion routines can be faster than lookup tables, in most real world usage.
A "find the first 1" instruction is available on most modern CPUs, and is a
much underused feature. 

Submit patches to add support for your own favourite processor.

If an assembly language method of bit searching is not available, these routines
revert to a method that can be a little slow, so the cache thrashing might not
seem so bad :(
*/

#if !defined(_ALAW_ULAW_H_)
#define _ALAW_ULAW_H_

#if defined(__i386__)
static inline int top_bit(unsigned int bits)
{
    int res;

    __asm__ __volatile__(" bsrl %%eax,%%edx;"
                         : "=d" (res)
                         : "a" (bits));
    return res;
}
/*- End of function --------------------------------------------------------*/

static inline int bottom_bit(unsigned int bits)
{
    int res;

    __asm__ __volatile__(" bsfl %%eax,%%edx;"
                         : "=d" (res)
                         : "a" (bits));
    return res;
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

#define ZEROTRAP                        /* turn on the trap as per the MIL-STD */
#define BIAS             0x84           /* Bias for linear code. */

static inline uint8_t linear_to_ulaw(int16_t linear)
{
    uint8_t u_val;
    int mask;
    int seg;
    int pcm_val;
#if !defined(__i386__)
    static short seg_end[8] =
    {
         0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF, 0x3FFF, 0x7FFF
    };
#endif

    pcm_val = linear;
    /* Get the sign and the magnitude of the value. */
    if (pcm_val < 0)
    {
        pcm_val = BIAS - pcm_val;
        mask = 0x7F;
    }
    else
    {
        pcm_val = BIAS + pcm_val;
        mask = 0xFF;
    }

#if defined(__i386__)
    seg = top_bit (pcm_val | 0xFF) - 7;
#else
    /* Convert the scaled magnitude to segment number. */
    for (seg = 0;  seg < 8;  seg++)
    {
        if (pcm_val <= seg_end[seg])
            break;
    }
#endif

    /*
     * Combine the sign, segment, quantization bits,
     * and complement the code word.
     */
    if (seg >= 8)
        u_val = 0x7F ^ mask;
    else
        u_val = ((seg << 4) | ((pcm_val >> (seg + 3)) & 0xF)) ^ mask;
#ifdef ZEROTRAP
    /* optional CCITT trap */
    if (u_val == 0)
        u_val = 0x02;
#endif
    return  u_val;
}
/*- End of function --------------------------------------------------------*/

static inline int16_t ulaw_to_linear(uint8_t ulaw)
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

static inline uint8_t linear_to_alaw(int16_t linear)
{
    int mask;
    int seg;
    int pcm_val;
#if !defined(__i386__)
    static int seg_end[8] =
    {
         0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF, 0x3FFF, 0x7FFF
    };
#endif
    
    pcm_val = linear;
    if (pcm_val >= 0)
    {
        /* Sign (7th) bit = 1 */
        mask = AMI_MASK | 0x80;
    }
    else
    {
        /* Sign bit = 0 */
        mask = AMI_MASK;
        pcm_val = -pcm_val;
    }

    /* Convert the scaled magnitude to segment number. */
#if defined(__i386__)
    seg = top_bit (pcm_val | 0xFF) - 7;
#else
    for (seg = 0;  seg < 8;  seg++)
    {
        if (pcm_val <= seg_end[seg])
        break;
    }
#endif
    /* Combine the sign, segment, and quantization bits. */
    return  ((seg << 4) | ((pcm_val >> ((seg)  ?  (seg + 3)  :  4)) & 0x0F)) ^ mask;
}
/*- End of function --------------------------------------------------------*/

static inline int16_t alaw_to_linear(uint8_t alaw)
{
    int i;
    int seg;

    alaw ^= AMI_MASK;
    i = ((alaw & 0x0F) << 4);
    seg = (((int) alaw & 0x70) >> 4);
    if (seg)
        i = (i + 0x100) << (seg - 1);
    return (short int) ((alaw & 0x80)  ?  i  :  -i);
}
/*- End of function --------------------------------------------------------*/

#endif
/*- End of file ------------------------------------------------------------*/

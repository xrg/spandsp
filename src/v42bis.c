/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v42bis.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
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
 * $Id: v42bis.c,v 1.5 2005/08/31 19:27:53 steveu Exp $
 */

/* THIS IS A WORK IN PROGRESS. IT IS NOT FINISHED. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/alaw_ulaw.h"
#include "spandsp/v42bis.h"

/* Fixed parameters from the spec. */
#define V42BIS_N3               8   /* Character size (bits) */
#define V42BIS_N4               256 /* Number of characters in the alphabet */
#define V42BIS_N5               (V42BIS_N4 + V42BIS_N6)  /* Index number of first dictionary entry used to store a string */
#define V42BIS_N6               3   /* Number of control codewords */

/* Control code words in compressed mode */
#define V42BIS_ETM              0   /* Enter transparent mode */
#define V42BIS_FLUSH            1   /* Flush data */
#define V42BIS_STEPUP           2   /* Step up codeword size */

/* Command codes in transparent mode */
#define V42BIS_ECM              0   /* Enter compression mode */
#define V42BIS_EID              1   /* Escape character in data */
#define V42BIS_RESET            2   /* Force reinitialisation */
/* Values 3 to 255 are reserved */

int v42bis_flush(v42bis_state_t *s)
{
    uint8_t ch;

    if (!s->compress.transparent)
    {
        /* Output the last state of the string */
        s->compress.output_bit_buffer |= s->compress.string_code << (32 - s->compress.v42bis_parm_c2 - s->compress.output_bit_count);
        s->compress.output_bit_count += s->compress.v42bis_parm_c2;
        while (s->compress.output_bit_count >= 8)
        {
            ch = s->compress.output_bit_buffer >> 24;
            s->frame_handler(s->frame_user_data, &ch, 1);
            s->compress.output_bit_buffer <<= 8;
            s->compress.output_bit_count -= 8;
        }
        if (s->compress.output_bit_count > 0)
        {
            /* We have some bits left. Use a flush to force them out */
            s->compress.output_bit_buffer |= V42BIS_FLUSH << (32 - s->compress.v42bis_parm_c2 - s->compress.output_bit_count);
            s->compress.output_bit_count += s->compress.v42bis_parm_c2;
            while (s->compress.output_bit_count > 0)
            {
                ch = s->compress.output_bit_buffer >> 24;
                s->frame_handler(s->frame_user_data, &ch, 1);
                s->compress.output_bit_buffer <<= 8;
                s->compress.output_bit_count -= 8;
            }
        }
    }
}

int v42bis_compress(v42bis_state_t *s, const uint8_t *buf, int len)
{
    int index;
    int offset;
    uint32_t octet;
    uint32_t code;
    int ptr;
    uint8_t ch;

    if ((s->v42bis_parm_p0 & 2) == 0)
    {
        /* Compression is off */
        s->frame_handler(s->frame_user_data, buf, len);
        return 0;
    }
    for (ptr = 0;  ptr < len;  ptr++)
    {
        octet = buf[ptr];
        if (s->compress.first)
        {
            s->compress.string_code = octet + V42BIS_N6;
            s->compress.first = FALSE;
        }
        else
        {
            /* Hash to find a match */
            index = (octet << (V42BIS_MAX_BITS - V42BIS_N3)) ^ s->compress.string_code;
            offset = (index == 0)  ?  1  :  V42BIS_TABLE_SIZE - index;
            for (;;)
            {
                if (s->compress.code[index] == 0xFFFF)
                    break;
                code = s->compress.code[index];
                if (s->compress.prior_code[code] == s->compress.string_code  &&  s->compress.node_octet[code] == octet)
                    break;
                index -= offset;
                if (index < 0)
                    index += V42BIS_TABLE_SIZE;
            }
            if (s->compress.code[index] != 0xFFFF)
            {
                /* The string was found */
                s->compress.string_code = s->compress.code[index];
                s->compress.string_length++;
            }
            else
            {
                /* The string is not in the table. */
                if (s->compress.v42bis_parm_c1 >= s->compress.v42bis_parm_c3)
                {
                    /* We need to increase the codeword size */
                    s->compress.output_bit_buffer |= V42BIS_STEPUP << (32 - s->compress.v42bis_parm_c2 - s->compress.output_bit_count);
                    s->compress.output_bit_count += s->compress.v42bis_parm_c2;
                    while (s->compress.output_bit_count >= 8)
                    {
                        ch = s->compress.output_bit_buffer >> 24;
                        s->frame_handler(s->frame_user_data, &ch, 1);
                        s->compress.output_bit_buffer <<= 8;
                        s->compress.output_bit_count -= 8;
                    }
                    s->compress.v42bis_parm_c2++;
                    s->compress.v42bis_parm_c3 <<= 1;
                }
                if (s->compress.string_length <= s->v42bis_parm_n7)
                {
                    /* Add this to the table */
                    /* The length is in range for adding to the dictionary */
                    s->compress.code[index] = s->compress.v42bis_parm_c1;
                    s->compress.prior_code[s->compress.string_code] &= 0x7FFF;
                    s->compress.prior_code[s->compress.v42bis_parm_c1] = 0x8000 | s->compress.string_code;
                    s->compress.node_octet[s->compress.v42bis_parm_c1] = octet;
                    /* Select the next node to be used */
                    for (;;)
                    {
                        if (++s->compress.v42bis_parm_c1 >= s->v42bis_parm_n2)
                            s->compress.v42bis_parm_c1 = V42BIS_N5;
                        if ((s->compress.prior_code[s->compress.v42bis_parm_c1] & 0x8000))
                        {
                            /* This is a leaf node, so it can be detached and reused. */
                            if (s->compress.prior_code[s->compress.v42bis_parm_c1] != 0xFFFF)
                            {
                                /* This was in use, so it needs to be removed from the hash table */
                                index = (s->compress.node_octet[s->compress.v42bis_parm_c1] << (V42BIS_MAX_BITS - V42BIS_N3)) ^ (s->compress.prior_code[s->compress.v42bis_parm_c1] & 0x7FFF);
                                offset = (index == 0)  ?  1  :  V42BIS_TABLE_SIZE - index;
                                for (;;)
                                {
                                    if (s->compress.code[index] == s->compress.v42bis_parm_c1)
                                    {
                                        s->compress.code[index] = 0xFFFF;
                                        break;
                                    }
                                    index -= offset;
                                    if (index < 0)
                                        index += V42BIS_TABLE_SIZE;
                                }
                            }
                            s->compress.prior_code[s->compress.v42bis_parm_c1] = 0xFFFF;
                            break;
                        }
                    }
                }
                if (!s->compress.transparent)
                {
                    /* Output the last state of the string */
                    s->compress.output_bit_buffer |= s->compress.string_code << (32 - s->compress.v42bis_parm_c2 - s->compress.output_bit_count);
                    s->compress.output_bit_count += s->compress.v42bis_parm_c2;
                    while (s->compress.output_bit_count >= 8)
                    {
                        ch = s->compress.output_bit_buffer >> 24;
                        s->frame_handler(s->frame_user_data, &ch, 1);
                        s->compress.output_bit_buffer <<= 8;
                        s->compress.output_bit_count -= 8;
                    }
                }
                s->compress.string_code = octet + V42BIS_N6;
                s->compress.string_length = 1;
            }
            if (s->compress.transparent)
            {
                ch = octet;
                s->frame_handler(s->frame_user_data, &ch, 1);
            }
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int v42bis_decompress(v42bis_state_t *s, const uint8_t *buf, int len)
{
    int ptr;
    uint8_t *string;
    uint32_t code;
    uint32_t new_code;

    if ((s->v42bis_parm_p0 & 1) == 0)
    {
        /* Compression is off */
        s->data_handler(s->data_user_data, buf, len);
        return 0;
    }
    ptr = 0;
    for (;;)
    {
        if (s->compress.transparent)
        {
            /* TODO: implement transparent mode */
            while (ptr < len)
            {
                code = buf[ptr++];
                if (code == s->decompress.escape_code)
                {
                    s->decompress.escape_code++;
                }
            }
            break;
        }
        
        while (s->decompress.input_bit_count <= 24  &&  ptr < len)
        {
            s->decompress.input_bit_count += 8;
            s->decompress.input_bit_buffer |= (uint32_t) buf[ptr++] << (32 - s->decompress.input_bit_count);
        }
        if (s->decompress.input_bit_count <= 24)
            break;
        new_code = s->decompress.input_bit_buffer >> (32 - s->compress.v42bis_parm_c2);
        s->decompress.input_bit_buffer <<= s->compress.v42bis_parm_c2;
        s->decompress.input_bit_count -= s->compress.v42bis_parm_c2;
        if (new_code < V42BIS_N6)
        {
            /* We have a control code. */
            switch (new_code)
            {
            case V42BIS_ETM:
                s->compress.transparent = TRUE;
                break;
            case V42BIS_FLUSH:
                break;
            case V42BIS_STEPUP:
                /* We need to increase the codeword size */
                s->decompress.v42bis_parm_c2++;
                s->decompress.v42bis_parm_c3 <<= 1;
                break;
            }
            continue;
        }
        if (s->decompress.first)
        {
            s->decompress.first = FALSE;
            s->decompress.octet = new_code;
            s->decompress.decode_buf[0] = new_code - V42BIS_N6;
            s->data_handler(s->data_user_data, s->decompress.decode_buf, 1);
            s->decompress.old_code = new_code;
            continue;
        }
        /* Start at the end of the buffer, and decode backwards */
        string = &s->decompress.decode_buf[V42BIS_MAX_STRING_SIZE - 1];
        if (new_code >= s->decompress.v42bis_parm_c1)
        {
            /*
             * This code checks for the special STRING+OCTET+STRING+OCTET+STRING
             * case which generates an undefined code.  It handles it by decoding
             * the last code, and adding a single character to the end of the decode string.
             */
            *string-- = s->decompress.octet;
            code = s->decompress.old_code;
        }
        else
        {
            /* Otherwise we do a straight decode of the new code. */
            code = new_code;
        }
        /* Trace back through the octets which form the string */
        while (code >= V42BIS_N5)
        {
            *string-- = s->decompress.node_octet[code];
            code = s->decompress.prior_code[code] & 0x7FFF;
        }
        *string = code - V42BIS_N6;
        s->decompress.octet = code;
        /* Output the decoded string. */
        s->data_handler(s->data_user_data, string, s->decompress.decode_buf + V42BIS_MAX_STRING_SIZE - string);
        /* Finally add a new code to the string table, clearing space if needed. */
        if ((&s->decompress.decode_buf[V42BIS_MAX_STRING_SIZE - 1] - string) <= s->v42bis_parm_n7)
        {
            /* The string length is in range for adding */
            /* If the last code was a leaf, it no longer is */
            s->decompress.prior_code[new_code] &= 0x7FFF;
            s->decompress.prior_code[s->decompress.v42bis_parm_c1] = 0x8000 | s->decompress.old_code;
            s->decompress.node_octet[s->decompress.v42bis_parm_c1] = s->decompress.octet;
            /* Release a leaf node */
            for (;;)
            {
                if (++s->decompress.v42bis_parm_c1 >= s->v42bis_parm_n2)
                    s->decompress.v42bis_parm_c1 = V42BIS_N5;
                if ((s->decompress.prior_code[s->decompress.v42bis_parm_c1] & 0x8000))
                {
                    /* This is a leaf node, so it can be detached and reused. */
                    s->decompress.prior_code[s->decompress.v42bis_parm_c1] = 0xFFFF;
                    break;
                }
            }
        }
        s->decompress.old_code = new_code;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int v42bis_init(v42bis_state_t *s,
                int negotiated_p0,
                int negotiated_p1,
                int negotiated_p2,
                v42bis_frame_handler_t frame_handler,
                void *frame_user_data,
                v42bis_data_handler_t data_handler,
                void *data_user_data)
{
    int i;

    if (negotiated_p1 < 512  ||  negotiated_p1 > 65535)
        return -1;
    if (negotiated_p2 < 6  ||  negotiated_p2 > V42BIS_MAX_STRING_SIZE)
        return -1;
    memset(s, 0, sizeof(*s));

    s->frame_handler = frame_handler;
    s->frame_user_data = frame_user_data;

    s->data_handler = data_handler;
    s->data_user_data = data_user_data;

    s->v42bis_parm_p0 = negotiated_p0;  /* default is both ways off */

    s->v42bis_parm_n1 = top_bit(negotiated_p1 - 1) + 1;
    s->v42bis_parm_n2 = negotiated_p1;
    s->v42bis_parm_n7 = negotiated_p2;

    s->compress.v42bis_parm_c1 =
    s->decompress.v42bis_parm_c1 = V42BIS_N5;
    s->compress.v42bis_parm_c2 =
    s->decompress.v42bis_parm_c2 = V42BIS_N3 + 1;
    s->compress.v42bis_parm_c3 =
    s->decompress.v42bis_parm_c3 = 2*V42BIS_N4;

    s->compress.first =
    s->decompress.first = TRUE;
    for (i = 0;  i < V42BIS_TABLE_SIZE;  i++)
        s->compress.code[i] = 0xFFFF;
    for (i = 0;  i < V42BIS_MAX_CODEWORDS;  i++)
    {
        s->compress.prior_code[i] =
        s->decompress.prior_code[i] = 0xFFFF;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

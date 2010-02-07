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
 * $Id: v42bis.c,v 1.13 2006/01/27 14:29:09 steveu Exp $
 */

/* THIS IS A WORK IN PROGRESS. IT IS NOT FINISHED. */

/*! \file */

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
enum
{
    V42BIS_ETM = 0,         /* Enter transparent mode */
    V42BIS_FLUSH = 1,       /* Flush data */
    V42BIS_STEPUP = 2       /* Step up codeword size */
};

/* Command codes in transparent mode */
enum
{
    V42BIS_ECM = 0,         /* Enter compression mode */
    V42BIS_EID = 1,         /* Escape character in data */
    V42BIS_RESET = 2        /* Force reinitialisation */
};

static __inline__ void push_compressed_octet(v42bis_compress_state_t *ss, int octet)
{
    ss->output_buf[ss->output_octet_count++] = octet;
    if (ss->output_octet_count >= ss->max_len)
    {
        ss->handler(ss->user_data, ss->output_buf, ss->output_octet_count);
        ss->output_octet_count = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ void push_compressed_code(v42bis_compress_state_t *ss, int code)
{
    ss->output_bit_buffer |= code << (32 - ss->v42bis_parm_c2 - ss->output_bit_count);
    ss->output_bit_count += ss->v42bis_parm_c2;
    while (ss->output_bit_count >= 8)
    {
        push_compressed_octet(ss, ss->output_bit_buffer >> 24);
        ss->output_bit_buffer <<= 8;
        ss->output_bit_count -= 8;
    }
}
/*- End of function --------------------------------------------------------*/

int v42bis_compress(v42bis_state_t *s, const uint8_t *buf, int len)
{
    int index;
    int offset;
    int ptr;
    int i;
    uint32_t octet;
    uint32_t code;
    uint8_t ch;
    v42bis_compress_state_t *ss;

    ss = &s->compress;
    if ((s->v42bis_parm_p0 & 2) == 0)
    {
        /* Compression is off - just push the incoming data out */
        for (i = 0;  i < len - ss->max_len;  i += ss->max_len)
            ss->handler(ss->user_data, buf + i, ss->max_len);
        if (i < len)
            ss->handler(ss->user_data, buf + i, len - i);
        return 0;
    }
    ptr = 0;
    if (ss->first  &&  len > 0)
    {
        octet = buf[ptr++];
        ss->string_code = octet + V42BIS_N6;
        if (ss->transparent)
            push_compressed_octet(ss, octet);
        ss->first = FALSE;
    }
    while (ptr < len)
    {
        octet = buf[ptr++];
        /* Hash to find a match */
        index = (octet << (V42BIS_MAX_BITS - V42BIS_N3)) ^ ss->string_code;
        offset = (index == 0)  ?  1  :  V42BIS_TABLE_SIZE - index;
        for (;;)
        {
            if (ss->code[index] == 0xFFFF)
                break;
            code = ss->code[index];
            if (ss->prior_code[code] == ss->string_code  &&  ss->node_octet[code] == octet)
                break;
            if ((index -= offset) < 0)
                index += V42BIS_TABLE_SIZE;
        }
        ss->compressibility_filter += (((8 << 20) - ss->compressibility_filter) >> 10);
        if (ss->code[index] != 0xFFFF)
        {
            /* The string was found */
            ss->string_code = ss->code[index];
            ss->string_length++;
        }
        else
        {
            /* The string is not in the table. */
            if (!ss->transparent)
            {
                /* 7.4 Encoding - we now have the longest matchable string, and will need to output the code for it. */
                while (ss->v42bis_parm_c1 >= ss->v42bis_parm_c3)
                {
                    /* We need to increase the codeword size */
                    /* 7.4(a) */
                    push_compressed_code(ss, V42BIS_STEPUP);
                    /* 7.4(b) */
                    ss->v42bis_parm_c2++;
                    /* 7.4(c) */
                    ss->v42bis_parm_c3 <<= 1;
                    /* 7.4(d) this might need to be repeated, so we loop */
                }
                /* 7.5 Transfer - output the last state of the string */
                push_compressed_code(ss, ss->string_code);
            }
            /* 7.6    Dictionary updating */
            /* 6.4    Add the string to the dictionary */
            /* 6.4(b) The string is not in the table. */
            if (ss->string_length < s->v42bis_parm_n7)
            {
                /* 6.4(a) The length of the string is in range for adding to the dictionary */
                ss->code[index] = ss->v42bis_parm_c1;
                /* If the last code was a leaf, it no longer is */
                ss->leaves[ss->string_code]++;
                /* The new one is definitely a leaf */
                ss->prior_code[ss->v42bis_parm_c1] = ss->string_code;
                ss->leaves[ss->v42bis_parm_c1] = 0;
                ss->node_octet[ss->v42bis_parm_c1] = octet;
                /* 7.7    Node recovery */
                /* 6.5    Recovering a dictionary entry to use next */
                for (;;)
                {
                    /* 6.5(a) and (b) */
                    if (++ss->v42bis_parm_c1 >= s->v42bis_parm_n2)
                        ss->v42bis_parm_c1 = V42BIS_N5;
                    /* 6.5(c) We need to reuse a leaf node */
                    if (ss->leaves[ss->v42bis_parm_c1])
                        continue;
                    if (ss->prior_code[ss->v42bis_parm_c1] == 0xFFFF)
                        break;
                    /* 6.5(d) Detach the leaf node from its parent, and re-use it */
                    /* Clear the entry from the hash table */
                    index = (ss->node_octet[ss->v42bis_parm_c1] << (V42BIS_MAX_BITS - V42BIS_N3)) ^ ss->prior_code[ss->v42bis_parm_c1];
                    offset = (index == 0)  ?  1  :  V42BIS_TABLE_SIZE - index;
                    for (;;)
                    {
                        if (ss->code[index] == ss->v42bis_parm_c1)
                        {
                            ss->code[index] = 0xFFFF;
                            break;
                        }
                        if ((index -= offset) < 0)
                            index += V42BIS_TABLE_SIZE;
                    }
                    /* Possibly make the parent a leaf node again */
                    ss->leaves[ss->prior_code[ss->v42bis_parm_c1]]--;
                    ss->prior_code[ss->v42bis_parm_c1] = 0xFFFF;
                    break;
                }
            }
            /* 7.8   Data compressibility test */
            ss->compressibility_filter += (((-ss->v42bis_parm_c2 << 20) - ss->compressibility_filter) >> 10);
#if 0
            if (ss->transparent)
            {
                /* 7.8.1 Transition to compressed mode - TODO */
                if (ss->compressibility_filter > 0x1000)
                {
                    /* Switch out of transparent now, between codes. We need to send the octet which did not
                       match, just before switching. */
                    if (octet == ss->escape_code)
                    {
                        push_compressed_octet(ss, ss->escape_code++);
                        push_compressed_octet(ss, V42BIS_EID);
                    }
                    else
                    {
                        push_compressed_octet(ss, octet);
                    }
                    push_compressed_octet(ss, ss->escape_code++);
                    push_compressed_octet(ss, V42BIS_ECM);
                    ss->transparent = FALSE;
                }
            }
            else
            {
                /* 7.8.2 Transition to transparent mode - TODO */
                if (ss->compressibility_filter < 0)
                {
                    /* Switch into transparent now, between codes, and the unmatched octet should
                       go out in transparent mode, just below */
                    push_compressed_code(ss, V42BIS_ETM);
                    ss->transparent = TRUE;
                }
            }
printf("Compress %x\n", ss->compressibility_filter);
#endif
            /* 7.8.3 Reset function - TODO */
            ss->string_code = octet + V42BIS_N6;
            ss->string_length = 1;
        }
        if (ss->transparent)
        {
            if (octet == ss->escape_code)
            {
                push_compressed_octet(ss, ss->escape_code++);
                push_compressed_octet(ss, V42BIS_EID);
            }
            else
            {
                push_compressed_octet(ss, octet);
            }
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int v42bis_compress_flush(v42bis_state_t *s)
{
    v42bis_compress_state_t *ss;

    ss = &s->compress;
    if (!ss->transparent)
    {
        /* Output the last state of the string */
        push_compressed_code(ss, ss->string_code);
        /* TODO: We use a positive FLUSH at all times. It is really needed, if the
           previous step resulted in no leftover bits. */
        push_compressed_code(ss, V42BIS_FLUSH);
        while (ss->output_bit_count > 0)
        {
            push_compressed_octet(ss, ss->output_bit_buffer >> 24);
            ss->output_bit_buffer <<= 8;
            ss->output_bit_count -= 8;
        }
    }
    /* Now push out anything remaining. */
    if (ss->output_octet_count > 0)
    {
        ss->handler(ss->user_data, ss->output_buf, ss->output_octet_count);
        ss->output_octet_count = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

#if 0
int v42bis_compress_dump(v42bis_state_t *s)
{
    int i;
    
    for (i = 0;  i < V42BIS_MAX_CODEWORDS;  i++)
    {
        if (s->compress.prior_code[i] != 0xFFFF)
        {
            printf("Entry %4x, prior %4x, leaves %d, octet %2x\n", i, s->compress.prior_code[i], s->compress.leaves[i], s->compress.node_octet[i]);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
#endif

int v42bis_decompress(v42bis_state_t *s, const uint8_t *buf, int len)
{
    int ptr;
    int i;
    int n;
    int this_length;
    uint8_t *string;
    uint32_t code;
    uint32_t new_code;
    v42bis_decompress_state_t *ss;

    ss = &s->decompress;
    if ((s->v42bis_parm_p0 & 1) == 0)
    {
        /* Compression is off - just push the incoming data out */
        for (i = 0;  i < len - ss->max_len;  i += ss->max_len)
            ss->handler(ss->user_data, buf + i, ss->max_len);
        if (i < len)
            ss->handler(ss->user_data, buf + i, len - i);
        return 0;
    }
    ptr = 0;
    for (;;)
    {
        if (ss->transparent)
        {
            /* TODO: implement transparent mode */
            while (ptr < len)
            {
                code = buf[ptr++];
                if (ss->escaped)
                {
                    ss->escaped = FALSE;
                    switch (code)
                    {
                    V42BIS_ECM:
                        break;
                    V42BIS_EID:
                        ss->transparent = FALSE;
                        break;
                    V42BIS_RESET:
                        break;
                    }
                }
                else if (code == ss->escape_code)
                {
                    ss->escape_code++;
                    ss->escaped = TRUE;
                }
                else
                {
                }
            }
            if (ss->transparent)
                break;
        }
        /* Gather enough bits for at least one whole code word */
        while (ss->input_bit_count <= ss->v42bis_parm_c2  &&  ptr < len)
        {
            ss->input_bit_count += 8;
            ss->input_bit_buffer |= (uint32_t) buf[ptr++] << (32 - ss->input_bit_count);
        }
        if (ss->input_bit_count <= ss->v42bis_parm_c2)
            break;
        new_code = ss->input_bit_buffer >> (32 - ss->v42bis_parm_c2);
        ss->input_bit_count -= ss->v42bis_parm_c2;
        ss->input_bit_buffer <<= ss->v42bis_parm_c2;
        if (new_code < V42BIS_N6)
        {
            /* We have a control code. */
            switch (new_code)
            {
            case V42BIS_ETM:
                printf("Hit V42BIS_ETM\n");
                ss->transparent = TRUE;
                break;
            case V42BIS_FLUSH:
                printf("Hit V42BIS_FLUSH\n");
                v42bis_decompress_flush(s);
                break;
            case V42BIS_STEPUP:
                /* We need to increase the codeword size */
                printf("Hit V42BIS_STEPUP\n");
                if (ss->v42bis_parm_c3 >= s->v42bis_parm_n2)
                {
                    /* Invalid condition */
                    return -1;
                }
                ss->v42bis_parm_c2++;
                ss->v42bis_parm_c3 <<= 1;
                break;
            }
            continue;
        }
        if (ss->first)
        {
            ss->first = FALSE;
            ss->octet = new_code - V42BIS_N6;
            ss->output_buf[0] = ss->octet;
            ss->output_octet_count = 1;
            if (ss->output_octet_count >= ss->max_len - s->v42bis_parm_n7)
            {
                ss->handler(ss->user_data, ss->output_buf, ss->output_octet_count);
                ss->output_octet_count = 0;
            }
            ss->old_code = new_code;
            continue;
        }
        /* Start at the end of the buffer, and decode backwards */
        string = &ss->decode_buf[V42BIS_MAX_STRING_SIZE - 1];
        if (new_code == ss->v42bis_parm_c1)
        {
            /*
             * This code deals with special situations like "XXXXXXX". The first X goes out
             * as 0x5B. A code is then added to the encoder's dictionary for "XX". Immediately,
             * the second and third X's match this code, even though it has not yet gone into
             * the decoder's directory. We handle this by decoding the last code, and adding a
             * single character to the end of the decoded string.
             */
            *string-- = ss->octet;
            code = ss->old_code;
        }
        else
        {
            /* Check the received code is valid. It can't be too big, as we pulled only the expected number
               of bits from the input stream. It could, however, be unknown. */
            if (ss->prior_code[new_code] == 0xFFFF)
                return -1;
            /* Otherwise we do a straight decode of the new code. */
            code = new_code;
        }
        /* Trace back through the octets which form the string, and output them. */
        while (code >= V42BIS_N5)
        {
            *string-- = ss->node_octet[code];
            code = ss->prior_code[code];
        }
        *string = code - V42BIS_N6;
        ss->octet = code - V42BIS_N6;
        /* Output the decoded string. */
        this_length = ss->decode_buf + V42BIS_MAX_STRING_SIZE - string;
        memcpy(ss->output_buf + ss->output_octet_count, string, this_length);
        ss->output_octet_count += this_length;
        if (ss->output_octet_count >= ss->max_len - s->v42bis_parm_n7)
        {
            ss->handler(ss->user_data, ss->output_buf, ss->output_octet_count);
            ss->output_octet_count = 0;
        }
        /* 6.4    Add the string to the dictionary */
        /* 6.4(b) The string is not in the table. */
        if (ss->last_length < s->v42bis_parm_n7)
        {
            /* 6.4(a) The length of the string is in range for adding to the dictionary */
            ss->leaves[ss->old_code]++;
            /* The new one is definitely a leaf */
            ss->prior_code[ss->v42bis_parm_c1] = ss->old_code;
            ss->leaves[ss->v42bis_parm_c1] = 0;
            ss->node_octet[ss->v42bis_parm_c1] = ss->octet;
            /* 6.5    Recovering a dictionary entry to use next */
            for (;;)
            {
                /* 6.5(a) and (b) */
                if (++ss->v42bis_parm_c1 >= s->v42bis_parm_n2)
                    ss->v42bis_parm_c1 = V42BIS_N5;
                /* 6.5(c) We need to reuse a leaf node */
                if (ss->leaves[ss->v42bis_parm_c1])
                    continue;
                /* 6.5(d) This is a leaf node, so re-use it */
                /* Possibly make the parent a leaf node again */
                if (ss->prior_code[ss->v42bis_parm_c1] != 0xFFFF)
                    ss->leaves[ss->prior_code[ss->v42bis_parm_c1]]--;
                ss->prior_code[ss->v42bis_parm_c1] = 0xFFFF;
                break;
            }
        }
        ss->old_code = new_code;
        ss->last_length = this_length;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int v42bis_decompress_flush(v42bis_state_t *s)
{
    v42bis_decompress_state_t *ss;

    ss = &s->decompress;
    /* Push out anything remaining. */
    if (ss->output_octet_count > 0)
    {
        ss->handler(ss->user_data, ss->output_buf, ss->output_octet_count);
        ss->output_octet_count = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

#if 0
int v42bis_decompress_dump(v42bis_state_t *s)
{
    int i;
    
    for (i = 0;  i < V42BIS_MAX_CODEWORDS;  i++)
    {
        if (s->decompress.prior_code[i] != 0xFFFF)
        {
            printf("Entry %4x, prior %4x, leaves %d, octet %2x\n", i, s->decompress.prior_code[i], s->decompress.leaves[i], s->decompress.node_octet[i]);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
#endif

v42bis_state_t *v42bis_init(v42bis_state_t *s,
                            int negotiated_p0,
                            int negotiated_p1,
                            int negotiated_p2,
                            v42bis_frame_handler_t frame_handler,
                            void *frame_user_data,
                            int max_frame_len,
                            v42bis_data_handler_t data_handler,
                            void *data_user_data,
                            int max_data_len)
{
    int i;

    if (negotiated_p1 < 512  ||  negotiated_p1 > 65535)
        return NULL;
    if (negotiated_p2 < 6  ||  negotiated_p2 > V42BIS_MAX_STRING_SIZE)
        return NULL;
    if (s == NULL)
    {
        if ((s = (v42bis_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    s->compress.handler = frame_handler;
    s->compress.user_data = frame_user_data;
    s->compress.max_len = (max_frame_len < 1024)  ?  max_frame_len  :  1024;

    s->decompress.handler = data_handler;
    s->decompress.user_data = data_user_data;
    s->decompress.max_len = (max_data_len < 1024)  ?  max_data_len  :  1024;

    s->v42bis_parm_p0 = negotiated_p0;  /* default is both ways off */

    s->v42bis_parm_n1 = top_bit(negotiated_p1 - 1) + 1;
    s->v42bis_parm_n2 = negotiated_p1;
    s->v42bis_parm_n7 = negotiated_p2;

    /* 6.5 */
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
        s->compress.leaves[i] =
        s->decompress.leaves[i] = 0;
    }
    /* Point the root nodes for decompression to themselves. It doesn't matter much what
       they are set to, as long as they are considered "known" codes. */
    for (i = 0;  i < V42BIS_N5;  i++)
        s->decompress.prior_code[i] = i;
    return s;
}
/*- End of function --------------------------------------------------------*/

int v42bis_release(v42bis_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v42bis.h
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
 * $Id: v42bis.h,v 1.3 2005/06/10 16:03:12 steveu Exp $
 */

#if !defined(_V42BIS_H_)
#define _V42BIS_H_

/*! \page V42bis_page V.42bis modem data compression
\section V42bis_page_sec_1 What does it do?

\section V42bis_page_sec_2 How does it work?
*/

#define V42BIS_MAX_BITS         12
#define V42BIS_MAX_CODEWORDS    4096    /* 2^V42BIS_MAX_BITS */
#define V42BIS_TABLE_SIZE       5021    /* This should be a prime >(2^V42BIS_MAX_BITS) */
#define V42BIS_MAX_STRING_SIZE  250

enum
{
    V42BIS_P0_NEITHER_DIRECTION = 0,
    V42BIS_P0_INITIATOR_RESPONDER,
    V42BIS_P0_RESPONDER_INITIATOR,
    V42BIS_P0_BOTH_DIRECTIONS
};

typedef void (*v42bis_frame_handler_t)(void *user_data, const uint8_t *pkt, int len);
typedef void (*v42bis_data_handler_t)(void *user_data, const uint8_t *buf, int len);

typedef struct
{
    /*! \brief V.42bis data compression directions. */
    int v42bis_parm_p0;

    v42bis_frame_handler_t frame_handler;
    void *frame_user_data;

    v42bis_data_handler_t data_handler;
    void *data_user_data;

    struct
    {
        uint32_t string_code;
        int string_length;
        uint32_t output_bit_buffer;
        int output_bit_count;
        /*! \brief The code hash table. */
        uint16_t code[V42BIS_TABLE_SIZE];
        /*! \brief The prior code for each defined code. */
        uint16_t prior_code[V42BIS_MAX_CODEWORDS];
        /*! \brief This leaf octet for each defined code. */
        uint8_t node_octet[V42BIS_MAX_CODEWORDS];
        /*! \brief TRUE if we are in transparent (i.e. uncompressable) mode */
        int transparent;

        /*! \brief Next empty dictionary entry */
        uint32_t v42bis_parm_c1;
        /*! \brief Current codeword size */
        int v42bis_parm_c2;
        /*! \brief Threshold for codeword size change */
        uint32_t v42bis_parm_c3;

        /*! \brief Mark that this is the first octet/code to be processed */
        int first;
        uint8_t escape_code;
    } compress;
    struct
    {
        uint32_t old_code;
        uint32_t input_bit_buffer;
        int input_bit_count;
        int octet;
        /*! \brief The prior code for each defined code. */
        uint16_t prior_code[V42BIS_MAX_CODEWORDS];
        /*! \brief This leaf octet for each defined code. */
        uint8_t node_octet[V42BIS_MAX_CODEWORDS];
        /*! \brief This buffer holds the decoded string */
        uint8_t decode_buf[V42BIS_MAX_STRING_SIZE];
        /*! \brief TRUE if we are in transparent (i.e. uncompressable) mode */
        int transparent;

        /*! \brief Next empty dictionary entry */
        int v42bis_parm_c1;
        /*! \brief Current codeword size */
        int v42bis_parm_c2;
        /*! \brief Threshold for codeword size change */
        int v42bis_parm_c3;
        
        /*! \brief Mark that this is the first octet/code to be processed */
        int first;
        uint8_t escape_code;
    } decompress;
    
    /*! \brief Maximum codeword size (bits) */
    int v42bis_parm_n1;
    /*! \brief Total number of codewords */
    int v42bis_parm_n2;
    /*! \brief Maximum string length */
    int v42bis_parm_n7;
} v42bis_state_t;

#ifdef __cplusplus
extern "C" {
#endif

int v42bis_compress(v42bis_state_t *s, const uint8_t *buf, int len);
int v42bis_flush(v42bis_state_t *s);

int v42bis_decompress(v42bis_state_t *s, const uint8_t *buf, int len);

int v42bis_init(v42bis_state_t *s,
                int negotiated_p0,
                int negotiated_p1,
                int negotiated_p2,
                v42bis_frame_handler_t frame_handler,
                void *frame_user_data,
                v42bis_data_handler_t data_handler,
                void *data_user_data);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

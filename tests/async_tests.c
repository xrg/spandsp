/*
 * SpanDSP - a series of DSP components for telephony
 *
 * async_tests.c - Tests for asynchronous serial processing.
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
 * $Id: async_tests.c,v 1.2 2004/10/19 14:53:41 steveu Exp $
 */

#define	_ISOC9X_SOURCE	1
#define _ISOC99_SOURCE	1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"

#define NB_SAMPLES 160

async_rx_state_t rx_async;
async_tx_state_t tx_async;

int full_len;
uint8_t old_buf[1000];
uint8_t new_buf[1000];

int tx_async_chars;
int rx_async_chars;
int rx_async_char_mask;

int v14_test_async_tx_bit(void *user_data)
{
    async_tx_state_t *s;
    int bit;
    
    /* Special routine to test V.14 rate adaption, by randomly skipping
       stop bits. */
    s = (async_tx_state_t *) user_data;
    if (s->bitpos == 0)
    {
        /* Start bit */
        bit = 0;
        s->byte_in_progress = s->get_byte(s->user_data);
        s->parity_bit = 0;
        s->bitpos++;
    }
    else if (s->bitpos <= s->data_bits)
    {
        bit = s->byte_in_progress & 1;
        s->parity_bit ^= bit;
        s->byte_in_progress >>= 1;
        s->bitpos++;
        if (!s->parity  &&  s->bitpos == s->data_bits + 1)
        {
            if ((rand() & 1))
            {
                s->parity_bit = 0;
                s->bitpos = 0;
            }
        }
    }
    else if (s->parity  &&  s->bitpos == s->data_bits + 1)
    {
        if (s->parity == ASYNC_PARITY_ODD)
            s->parity_bit ^= 1;
        bit = s->parity_bit;
        s->bitpos++;
        if ((rand() & 1))
        {
            s->parity_bit = 0;
            s->bitpos = 0;
        }
    }
    else
    {
        /* Stop bit(s) */
        bit = 1;
        s->bitpos++;
        if (s->bitpos > s->data_bits + s->stop_bits)
            s->bitpos = 0;
    }
    return bit;
}
/*- End of function --------------------------------------------------------*/

static int test_get_async_byte(void *user_data)
{
    int byte;
    
    byte = tx_async_chars & 0xFF;
    tx_async_chars++;
    //printf("Send %x\n", byte);
    return byte;
}
/*- End of function --------------------------------------------------------*/

static void test_put_async_byte(void *user_data, int byte)
{
    if ((rx_async_chars & rx_async_char_mask) != byte)
        printf("Received byte is 0x%X (expected 0x%X)\n", byte, rx_async_chars);
    rx_async_chars++;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int bit;

    printf("Test with async 8N1\n");
    async_tx_init(&tx_async, 8, ASYNC_PARITY_NONE, 1, FALSE, test_get_async_byte, NULL);
    async_rx_init(&rx_async, 8, ASYNC_PARITY_NONE, 1, FALSE, test_put_async_byte, NULL);
    tx_async_chars = 0;
    rx_async_chars = 0;
    rx_async_char_mask = 0xFF;
    for (  ;  rx_async_chars < 1000;  )
    {
        bit = async_tx_bit(&tx_async);
        async_rx_bit(&rx_async, bit);
    }
    printf("Chars=%d/%d, PE=%d, FE=%d\n", tx_async_chars, rx_async_chars, rx_async.parity_errors, rx_async.framing_errors);
    
    printf("Test with async 7E1\n");
    async_tx_init(&tx_async, 7, ASYNC_PARITY_EVEN, 1, FALSE, test_get_async_byte, NULL);
    async_rx_init(&rx_async, 7, ASYNC_PARITY_EVEN, 1, FALSE, test_put_async_byte, NULL);
    tx_async_chars = 0;
    rx_async_chars = 0;
    rx_async_char_mask = 0x7F;
    for (  ;  rx_async_chars < 1000;  )
    {
        bit = async_tx_bit(&tx_async);
        async_rx_bit(&rx_async, bit);
    }
    printf("Chars=%d/%d, PE=%d, FE=%d\n", tx_async_chars, rx_async_chars, rx_async.parity_errors, rx_async.framing_errors);

    printf("Test with async 8O1\n");
    async_tx_init(&tx_async, 8, ASYNC_PARITY_ODD, 1, FALSE, test_get_async_byte, NULL);
    async_rx_init(&rx_async, 8, ASYNC_PARITY_ODD, 1, FALSE, test_put_async_byte, NULL);
    tx_async_chars = 0;
    rx_async_chars = 0;
    rx_async_char_mask = 0xFF;
    for (  ;  rx_async_chars < 1000;  )
    {
        bit = async_tx_bit(&tx_async);
        async_rx_bit(&rx_async, bit);
    }
    printf("Chars=%d/%d, PE=%d, FE=%d\n", tx_async_chars, rx_async_chars, rx_async.parity_errors, rx_async.framing_errors);

    printf("Test with async 8O1 and V.14\n");
    async_tx_init(&tx_async, 8, ASYNC_PARITY_ODD, 1, TRUE, test_get_async_byte, NULL);
    async_rx_init(&rx_async, 8, ASYNC_PARITY_ODD, 1, TRUE, test_put_async_byte, NULL);
    tx_async_chars = 0;
    rx_async_chars = 0;
    rx_async_char_mask = 0xFF;
    for (  ;  rx_async_chars < 1000;  )
    {
        bit = v14_test_async_tx_bit(&tx_async);
        async_rx_bit(&rx_async, bit);
    }
    printf("Chars=%d/%d, PE=%d, FE=%d\n", tx_async_chars, rx_async_chars, rx_async.parity_errors, rx_async.framing_errors);

    printf("Test with async 5N2\n");
    async_tx_init(&tx_async, 5, ASYNC_PARITY_NONE, 2, FALSE, test_get_async_byte, NULL);
    async_rx_init(&rx_async, 5, ASYNC_PARITY_NONE, 2, FALSE, test_put_async_byte, NULL);
    tx_async_chars = 0;
    rx_async_chars = 0;
    rx_async_char_mask = 0x1F;
    for (  ;  rx_async_chars < 1000;  )
    {
        bit = async_tx_bit(&tx_async);
        async_rx_bit(&rx_async, bit);
    }
    printf("Chars=%d/%d, PE=%d, FE=%d\n", tx_async_chars, rx_async_chars, rx_async.parity_errors, rx_async.framing_errors);

    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

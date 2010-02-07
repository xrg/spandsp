/*
 * SpanDSP - a series of DSP components for telephony
 *
 * hdlc_tests.c
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
 * $Id: hdlc_tests.c,v 1.4 2004/03/12 16:27:25 steveu Exp $
 */

//#define _ISOC9X_SOURCE  1
//#define _ISOC99_SOURCE  1

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>

#include "spandsp.h"

/* Yes, yes, I know. HDLC is not DSP. It is needed to go with some DSP
   components, though. */

/* Tests for CRC and HDLC processing. */

int full_len;
uint8_t old_buf[1000];
uint8_t buf[1000];

static void pkt_handler(void *user_data, uint8_t *pkt, int len)
{
    if (len < 0)
    {
        /* Special conditions */
        switch (len)
        {
        case PUTBIT_TRAINING_FAILED:
            printf("Training failed\n");
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            printf("Training succeeded\n");
            break;
        case PUTBIT_CARRIER_UP:
            printf("Carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            printf("Carrier down\n");
            break;
        default:
            printf("Eh!\n");
            break;
        }
        return;
    }
    if (len != full_len)
    {
        printf("Len error - %d %d\n", len, full_len);
        return;
    }
    if (memcmp(pkt, old_buf, len))
    {
        printf("Packet data error\n");
        return;
    }
    printf("Hit - %d\n", len);
}
/*- End of function --------------------------------------------------------*/

int main (int argc, char *argv[])
{
    int i;
    int j;
    int len;
    int nextbyte;
    hdlc_rx_state_t rx;
    hdlc_tx_state_t tx;

    /* Try a few random messages through the CRC logic. */
    for (i = 0;  i < 100;  i++)
    {
        len = (rand() & 0x3F) + 100;
        for (j = 0;  j < len;  j++)
            buf[j] = rand();
        full_len = append_crc_itu16(buf, len);
        if (!check_crc_itu16(buf, full_len))
        {
            printf("CRC failure\n");
            exit(2);
        }
    }
    
    /* Now try sending HDLC messages */
    hdlc_tx_init(&tx, NULL, NULL);
    hdlc_rx_init(&rx, pkt_handler, NULL);

    hdlc_tx_preamble(&tx, 40);
    len = 2;
    for (i = 0;  i < 10000;  i++)
    {
        nextbyte = hdlc_tx_getbyte(&tx);
        //printf("%x\n", nextbyte);
        hdlc_rx_byte(&rx, nextbyte);
        if (tx.len == 0)
        {
            memcpy(old_buf, buf, len);
            full_len = len;
            len = (rand() & 0x3F) + 100;
            for (j = 0;  j < len;  j++)
                buf[j] = rand();
            hdlc_tx_packet(&tx, buf, len);
        }
    }

    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

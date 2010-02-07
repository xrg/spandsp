/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v42_tests.c
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
 * $Id: v42_tests.c,v 1.4 2005/01/12 13:39:26 steveu Exp $
 */

//#define _ISOC9X_SOURCE  1
//#define _ISOC99_SOURCE  1

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>

#include "spandsp.h"

v42_state_t caller;
v42_state_t answerer;

void v42_status(void *user_data, int status)
{
    int x;
    
    x = (int) user_data;
    printf("Status of %d is %d\n", x, status);
    //if (status == LAPM_DATA)
    //    lapm_tx_iframe((x == 1)  ?  &caller.lapm  :  &answerer.lapm, "ABCDEFGHIJ", 10, 1);
}

int rx_next[3] = {0};
int tx_next[3] = {0};

void v42_frames(void *user_data, const uint8_t *msg, int len)
{
    int i;
    int x;
    
    x = (int) user_data;
    for (i = 0;  i < len;  i++)
    {
        if (msg[i] != (rx_next[x] & 0xFF))
            printf("Mismatch 0x%02X 0x%02X\n", msg[i], rx_next[x] & 0xFF);
        rx_next[x]++;
    }
    printf("Got frame %d, len %d\n", x, len);
}

int main(int argc, char *argv[])
{
    int i;
    int j;
    int len;
    int bit;
    uint8_t buf[1024];

    v42_init(&caller, TRUE, TRUE, v42_frames, (void *) 1);
    v42_init(&answerer, FALSE, TRUE, v42_frames, (void *) 2);
    v42_set_status_callback(&caller, v42_status, (void *) 1);
    v42_set_status_callback(&answerer, v42_status, (void *) 2);
    for (i = 0;  i < 100000;  i++)
    {
        bit = v42_tx_bit(&caller);
        v42_rx_bit(&answerer, bit);
        bit = v42_tx_bit(&answerer);
        v42_rx_bit(&caller, bit);
        sp_schedule_update(&caller.lapm.sched, 4);
        sp_schedule_update(&answerer.lapm.sched, 4);
        buf[0] = tx_next[1];
        if (lapm_tx(&caller.lapm, buf, 1) == 1)
            tx_next[1]++;
        buf[0] = tx_next[2];
        if (lapm_tx(&answerer.lapm, buf, 1) == 1)
            tx_next[2]++;
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

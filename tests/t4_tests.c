/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4_tests.c - ITU T.4 FAX image to and from TIFF file tests
 * This depends on libtiff (see <http://www.libtiff.org>)
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
 * $Id: t4_tests.c,v 1.4 2004/03/12 16:27:25 steveu Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>

#include <tiffio.h>

#include "spandsp.h"

#define XSIZE        1728

t4_state_t send_state;
t4_state_t receive_state;

int main(int argc, char* argv[])
{
    int page_no;
    int bit;
    uint8_t byte;
    int bits;
    int buf_ptr;
    int end_of_page;
    TIFFErrorHandler whandler;

    /* Create a send and a receive end */
    memset(&send_state, 0, sizeof(send_state));
    memset(&receive_state, 0, sizeof(receive_state));

    if (!receive_state.verbose)
        whandler = TIFFSetWarningHandler(NULL);
    if (!receive_state.verbose)
        TIFFSetWarningHandler(whandler);

    receive_state.verbose = 1;

    send_state.verbose = 1;

    if (!send_state.verbose)
        whandler = TIFFSetWarningHandler(NULL);

    /* Send end gets TIFF from a file, using 1D compression */
    strcpy(send_state.tx_file, "t4_tests_send.tif");
    send_state.remote_compression = 1;
    if (t4_tx_init(&send_state))
    {
        printf("Failed to init TIFF send\n");
        exit(0);
    }

    /* Receive end puts TIFF to a new file. We assume the receive width here. */
    strcpy(receive_state.rx_file, "t4_tests_receive.tif");
    receive_state.remote_compression = 1;
    receive_state.resolution = 0;
    receive_state.image_width = XSIZE;
    if (t4_rx_init(&receive_state))
    {
        printf("Failed to init\n");
        exit(0);
    }
    
    /* Now send and receive all the pages in the source TIFF file */
    page_no = 1;
    while (t4_tx_start_page(&send_state) == 0)
    {
        t4_rx_start_page(&receive_state);
        do
        {
            bit = t4_tx_getbit(&send_state);
            end_of_page = t4_rx_putbit(&receive_state, bit);
        }
        while (!end_of_page);
        t4_rx_end_page(&receive_state);
    }
    /* And we should now have a matching received TIFF file. Note this will only match at the image
       level. TIFF files allow a lot of ways to express the same thing, so bit matching of the files
       is not the normal case. */
    t4_tx_end(&send_state);
    t4_rx_end(&receive_state);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

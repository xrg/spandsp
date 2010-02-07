#define STREAM_TEST
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
 * $Id: t4_tests.c,v 1.13 2004/09/25 15:13:16 steveu Exp $
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
    int sends;
    int page_no;
    int bit;
    uint8_t byte;
    uint8_t *s;
    int bits;
    int buf_ptr;
    int end_of_page;
    int next_hit;
    int end_marks;
    int i;
    int j;
    int k;
    int decode_test;
    int compression;
    TIFFErrorHandler whandler;
#if defined(STREAM_TEST)
    char buf[512];
#endif
    t4_stats_t stats;

    i = 1;
    decode_test = FALSE;
    compression = T4_COMPRESSION_ITU_T4_1D;
    while (i < argc)
    {
        if (strcmp(argv[i], "-d") == 0)
            decode_test = TRUE;
        else if (strcmp(argv[i], "-1") == 0)
            compression = T4_COMPRESSION_ITU_T4_1D;
        else if (strcmp(argv[i], "-2") == 0)
            compression = T4_COMPRESSION_ITU_T4_2D;
        i++;
    }
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

    if (decode_test)
    {
        /* Receive end puts TIFF to a new file. We assume the receive width here. */
        if (t4_rx_init(&receive_state, "t4_tests_receive.tif", T4_COMPRESSION_ITU_T4_2D))
        {
            printf("Failed to init\n");
            exit(0);
        }
        
        t4_rx_set_rx_encoding(&receive_state, T4_COMPRESSION_ITU_T4_2D);
        t4_rx_set_row_resolution(&receive_state, T4_RESOLUTION_STANDARD);
        t4_rx_set_column_resolution(&receive_state, T4_RESOLUTION_FINE);
        t4_rx_set_columns(&receive_state, XSIZE);

        page_no = 1;
        t4_rx_start_page(&receive_state);
        while (fgets(buf, 511, stdin))
        {
            if (sscanf(buf, "Rx bit %*d - %d", &bit) == 1)
            {
                end_of_page = t4_rx_putbit(&receive_state, bit);
                if (end_of_page)
                {
                    printf("End of page detected\n");
                    break;
                }
            }
        }
#if 0
        /* Dump the entire image as text 'X's and spaces */
        s = receive_state.image_buffer;
        for (i = 0;  i < receive_state.rows;  i++)
        {
            for (j = 0;  j < receive_state.bytes_per_row;  j++)
            {
                for (k = 0;  k < 8;  k++)
                {
                    printf((receive_state.image_buffer[i*receive_state.bytes_per_row + j] & (0x80 >> k))  ?  "X"  :  " ");
                }
            }
            printf("\n");
        }
#endif
        t4_rx_end_page(&receive_state);
        t4_get_transfer_statistics(&receive_state, &stats);
        printf("Pages = %d\n", stats.pages_transferred);
        printf("Image size = %dx%d\n", stats.columns, stats.rows);
        printf("Image resolution = %dx%d\n", stats.column_resolution, stats.row_resolution);
        printf("Bad rows = %d\n", stats.bad_rows);
        printf("Longest bad row run = %d\n", stats.longest_bad_row_run);
        t4_rx_end(&receive_state);
    }
    else
    {
        /* Send end gets TIFF from a file, using 1D compression */
        if (t4_tx_init(&send_state, "t4_tests_send.tif"))
        {
            printf("Failed to init TIFF send\n");
            exit(0);
        }

        /* Receive end puts TIFF to a new file. We assume the receive width here. */
        if (t4_rx_init(&receive_state, "t4_tests_receive.tif", T4_COMPRESSION_ITU_T4_2D))
        {
            printf("Failed to init\n");
            exit(0);
        }
    
        t4_tx_set_min_row_bits(&send_state, 144);

        t4_rx_set_row_resolution(&receive_state, t4_tx_get_row_resolution(&send_state));
printf("column res %dx%d\n", t4_tx_get_column_resolution(&send_state), t4_tx_get_row_resolution(&send_state));
        t4_rx_set_column_resolution(&receive_state, t4_tx_get_column_resolution(&send_state));
        t4_rx_set_columns(&receive_state, t4_tx_get_columns(&send_state));

        /* Now send and receive all the pages in the source TIFF file */
        page_no = 1;
        t4_tx_set_local_ident(&send_state, "852 2666 0542");
        sends = 0;
        for (;;)
        {
            end_marks = 0;
            if ((sends & 2))
                t4_tx_set_header_info(&send_state, "Header");
            else
                t4_tx_set_header_info(&send_state, NULL);
            if ((sends & 1))
            {
                if (t4_tx_restart_page(&send_state))
                    break;
            }
            else
            {
                if ((sends & 2))
                    compression = T4_COMPRESSION_ITU_T4_1D;
                else
                    compression = T4_COMPRESSION_ITU_T4_2D;
                t4_tx_set_tx_encoding(&send_state, compression);
                t4_rx_set_rx_encoding(&receive_state, compression);

                if (t4_tx_start_page(&send_state))
                    break;
            }
            t4_rx_start_page(&receive_state);
            do
            {
                bit = t4_tx_getbit(&send_state);
#if 0
                if (--next_hit <= 0)
                {
                    do
                        next_hit = rand() & 0x3FF;
                    while (next_hit < 20);
                    bit ^= (rand() & 1);
                }
#endif
                if ((bit & 2))
                {
                    if (++end_marks > 50)
                    {
                        printf("Receiver missed the end of page mark\n");
                        break;
                    }
                }
                end_of_page = t4_rx_putbit(&receive_state, bit & 1);
            }
            while (!end_of_page);
            t4_get_transfer_statistics(&receive_state, &stats);
            printf("Pages = %d\n", stats.pages_transferred);
            printf("Image size = %dx%d\n", stats.columns, stats.rows);
            printf("Image resolution = %dx%d\n", stats.column_resolution, stats.row_resolution);
            printf("Bad rows = %d\n", stats.bad_rows);
            printf("Longest bad row run = %d\n", stats.longest_bad_row_run);
            if ((sends & 1))
                t4_tx_end_page(&send_state);
            t4_rx_end_page(&receive_state);
            sends++;
        }
        /* And we should now have a matching received TIFF file. Note this will only match
           at the image level. TIFF files allow a lot of ways to express the same thing,
           so bit matching of the files is not the normal case. */
        t4_tx_end(&send_state);
        t4_rx_end(&receive_state);
    }
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

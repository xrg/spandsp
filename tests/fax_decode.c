/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fax_decode.c - a simple FAX audio decoder
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
 * $Id: fax_decode.c,v 1.5 2005/03/13 15:58:57 steveu Exp $
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

int decode_test = FALSE;

int rx_bits = 0;

t4_state_t t4_state;
int t4_up = FALSE;

static void print_frame(const char *io, const uint8_t *fr, int frlen)
{
    int i;
    
    fprintf(stderr, "%s %s:", io, t30_frametype(fr[0]));
    for (i = 0;  i < frlen;  i++)
        fprintf(stderr, " %02x", fr[i]);
    fprintf(stderr, "\n");
}
/*- End of function --------------------------------------------------------*/

static void hdlc_accept(void *user_data, int ok, const uint8_t *msg, int len)
{
    int final_frame;
    
    if (len < 0)
    {
        /* Special conditions */
        switch (len)
        {
        case PUTBIT_CARRIER_UP:
            fprintf(stderr, "Slow carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            fprintf(stderr, "Slow carrier down\n");
            break;
        case PUTBIT_FRAMING_OK:
            /* Just ignore these */
            break;
        default:
            fprintf(stderr, "Unexpected HDLC special length - %d!\n", len);
            break;
        }
        return;
    }
    
    if (msg[0] != 0xFF  ||  !(msg[1] == 0x03  ||  msg[1] == 0x13))
    {
        fprintf(stderr, "Bad frame header - %02x %02x", msg[0], msg[1]);
        return;
    }
    print_frame("HDLC: ", &msg[2], len - 2);
}
/*- End of function --------------------------------------------------------*/

void t4_begin(void)
{
    if (t4_rx_init(&t4_state, "fax_decode.tif", T4_COMPRESSION_ITU_T4_2D))
    {
        printf("Failed to init\n");
        exit(0);
    }
        
    t4_rx_set_rx_encoding(&t4_state, T4_COMPRESSION_ITU_T4_2D);
    t4_rx_set_row_resolution(&t4_state, T4_RESOLUTION_STANDARD);
    t4_rx_set_column_resolution(&t4_state, T4_RESOLUTION_FINE);
    t4_rx_set_columns(&t4_state, 1728);

    t4_rx_start_page(&t4_state);
    t4_up = TRUE;
}
/*- End of function --------------------------------------------------------*/

void t4_end(void)
{
    t4_stats_t stats;

    if (!t4_up)
        return;
    t4_rx_end_page(&t4_state);
    t4_get_transfer_statistics(&t4_state, &stats);
    printf("Pages = %d\n", stats.pages_transferred);
    printf("Image size = %dx%d\n", stats.columns, stats.rows);
    printf("Image resolution = %dx%d\n", stats.column_resolution, stats.row_resolution);
    printf("Bad rows = %d\n", stats.bad_rows);
    printf("Longest bad row run = %d\n", stats.longest_bad_row_run);
    t4_rx_end(&t4_state);
    t4_up = FALSE;
}
/*- End of function --------------------------------------------------------*/

#if defined(ENABLE_V17)
static void v17_put_bit(void *user_data, int bit)
{
    int i;
    int len;
    int end_of_page;
    
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_TRAINING_FAILED:
            //printf("V.17 Training failed\n");
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            printf("V.17 Training succeeded\n");
            t4_begin();
            break;
        case PUTBIT_CARRIER_UP:
            //printf("V.17 Carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            //printf("V.17 Carrier down\n");
            t4_end();
            break;
        default:
            printf("V.17 Eh!\n");
            break;
        }
        return;
    }

    end_of_page = t4_rx_putbit(&t4_state, bit);
    if (end_of_page)
    {
        printf("End of page detected\n");
    }
    //printf("V.17 Rx bit %d - %d\n", rx_bits++, bit);
}
/*- End of function --------------------------------------------------------*/
#endif

static void v29_put_bit(void *user_data, int bit)
{
    int i;
    int len;
    int end_of_page;
    
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_TRAINING_FAILED:
            //printf("V.29 Training failed\n");
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            printf("V.29 Training succeeded\n");
            t4_begin();
            break;
        case PUTBIT_CARRIER_UP:
            //printf("V.29 Carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            //printf("V.29 Carrier down\n");
            t4_end();
            break;
        default:
            printf("V.29 Eh!\n");
            break;
        }
        return;
    }

    end_of_page = t4_rx_putbit(&t4_state, bit);
    if (end_of_page)
    {
        printf("End of page detected\n");
    }
    //printf("V.29 Rx bit %d - %d\n", rx_bits++, bit);
}
/*- End of function --------------------------------------------------------*/

static void v27ter_put_bit(void *user_data, int bit)
{
    int i;
    int len;
    
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_TRAINING_FAILED:
            //printf("V.27ter Training failed\n");
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            printf("V.27ter Training succeeded\n");
            break;
        case PUTBIT_CARRIER_UP:
            //printf("V.27ter Carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            //printf("V.27ter Carrier down\n");
            break;
        default:
            printf("V.27ter Eh!\n");
            break;
        }
        return;
    }

    printf("V.27ter Rx bit %d - %d\n", rx_bits++, bit);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    hdlc_rx_state_t hdlcrx;
    fsk_rx_state_t fsk;
#if defined(ENABLE_V17)
    v17_rx_state_t v17;
#endif
    v29_rx_state_t v29;
    v27ter_rx_state_t v27ter;
    int16_t amp[NB_SAMPLES];
    AFfilehandle inhandle;
    int inframes;    
    int i;
    int j;
    int len;
    char *filename;
    
    filename = "fax_samp.wav";

    if (argc > 1)
        filename = argv[1];

    inhandle = afOpenFile(filename, "r", NULL);
    if (inhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open wave file '%s'\n", filename);
        exit(2);
    }
    hdlc_rx_init(&hdlcrx, FALSE, hdlc_accept, NULL);
    fsk_rx_init(&fsk, &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) hdlc_rx_bit, &hdlcrx);
#if defined(ENABLE_V17)
    v17_rx_init(&v17, 14400, v17_put_bit, NULL);
#endif
    v29_rx_init(&v29, 9600, v29_put_bit, NULL);
    v27ter_rx_init(&v27ter, 4800, v27ter_put_bit, NULL);
    fsk_rx_signal_cutoff(&fsk, -45.0);
#if defined(ENABLE_V17)
    //v17_rx_signal_cutoff(&v17, -45.0);
#endif
    //v29_rx_signal_cutoff(&v29, -45.0);
    v27ter_rx_signal_cutoff(&v27ter, -40.0);

    for (;;)
    {
        len = afReadFrames(inhandle, AF_DEFAULT_TRACK, amp, NB_SAMPLES);
        if (len < NB_SAMPLES)
            break;
        fsk_rx(&fsk, amp, len);
#if defined(ENABLE_V17)
        v17_rx(&v17, amp, len);
#endif
        v29_rx(&v29, amp, len);
        //v27ter_rx(&v27ter, amp, len);
    }

    if (afCloseFile(inhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", filename);
        exit(2);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

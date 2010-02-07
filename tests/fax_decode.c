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
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
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
 * $Id: fax_decode.c,v 1.26 2007/03/31 12:36:59 steveu Exp $
 */

/*! \page fax_decode_page FAX decoder
\section fax_decode_page_sec_1 What does it do?
???.

\section fax_decode_tests_page_sec_2 How does it work?
???.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include <assert.h>
#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"

#define SAMPLES_PER_CHUNK   160

enum
{
    FAX_NONE,
    FAX_V27TER_RX,
    FAX_V29_RX,
    FAX_V17_RX
};

int decode_test = FALSE;
int ecm_mode = FALSE;
int rx_bits = 0;

t30_state_t t30_dummy;
t4_state_t t4_state;
int t4_up = FALSE;

hdlc_rx_state_t hdlcrx;

int fast_trained = FAX_NONE;

uint8_t ecm_data[256][260];
int16_t ecm_len[256];

static void print_frame(const char *io, const uint8_t *fr, int frlen)
{
    int i;
    int type;
    const char *country;
    const char *vendor;
    const char *model;
    
    fprintf(stderr, "%s %s:", io, t30_frametype(fr[2]));
    for (i = 2;  i < frlen;  i++)
        fprintf(stderr, " %02x", fr[i]);
    fprintf(stderr, "\n");
    type = fr[2] & 0xFE;
    if (type == T30_DIS  ||  type == T30_DTC  ||  type == T30_DCS)
        t30_decode_dis_dtc_dcs(&t30_dummy, fr, frlen);
    if (type == T30_NSF)
    {
        if (t35_decode(&fr[3], frlen - 3, &country, &vendor, &model))
        {
            if (country)
                fprintf(stderr, "The remote was made in '%s'\n", country);
            if (vendor)
                fprintf(stderr, "The remote was made by '%s'\n", vendor);
            if (model)
                fprintf(stderr, "The remote is a '%s'\n", model);
        }
    }
}
/*- End of function --------------------------------------------------------*/

static void hdlc_accept(void *user_data, int ok, const uint8_t *msg, int len)
{
    int type;
    int frame_no;
    int i;

    if (len < 0)
    {
        /* Special conditions */
        switch (len)
        {
        case PUTBIT_CARRIER_UP:
            fprintf(stderr, "HDLC carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            fprintf(stderr, "HDLC carrier down\n");
            break;
        case PUTBIT_FRAMING_OK:
            fprintf(stderr, "HDLC framing OK\n");
            break;
        case PUTBIT_ABORT:
            /* Just ignore these */
            break;
        default:
            fprintf(stderr, "Unexpected HDLC special length - %d!\n", len);
            break;
        }
        return;
    }
    
    if (ok)
    {
        if (msg[0] != 0xFF  ||  !(msg[1] == 0x03  ||  msg[1] == 0x13))
        {
            fprintf(stderr, "Bad frame header - %02x %02x", msg[0], msg[1]);
            return;
        }
        print_frame("HDLC: ", msg, len);
        type = msg[2] & 0xFE;
        if (type == T4_FCD)
        {
            if (len <= 4 + 256)
            {
                frame_no = msg[3];
                /* Just store the actual image data, and record its length */
                memcpy(&ecm_data[frame_no][0], &msg[4], len - 4);
                ecm_len[frame_no] = (int16_t) (len - 4);
            }
        }
    }
    else
    {
        fprintf(stderr, "Bad HDLC frame ");
        for (i = 0;  i < len;  i++)
            fprintf(stderr, " %02x", msg[i]);
        fprintf(stderr, "\n");
    }
}
/*- End of function --------------------------------------------------------*/

static void t4_begin(void)
{
    int i;

    t4_rx_set_rx_encoding(&t4_state, T4_COMPRESSION_ITU_T4_2D);
    t4_rx_set_x_resolution(&t4_state, T4_X_RESOLUTION_R8);
    t4_rx_set_y_resolution(&t4_state, T4_Y_RESOLUTION_STANDARD);
    t4_rx_set_image_width(&t4_state, 1728);

    t4_rx_start_page(&t4_state);
    t4_up = TRUE;

    for (i = 0;  i < 256;  i++)
        ecm_len[i] = -1;
}
/*- End of function --------------------------------------------------------*/

static void t4_end(void)
{
    t4_stats_t stats;
    int i;

    if (!t4_up)
        return;
    t4_rx_end_page(&t4_state);
    t4_get_transfer_statistics(&t4_state, &stats);
    fprintf(stderr, "Pages = %d\n", stats.pages_transferred);
    fprintf(stderr, "Image size = %dx%d\n", stats.width, stats.length);
    fprintf(stderr, "Image resolution = %dx%d\n", stats.x_resolution, stats.y_resolution);
    fprintf(stderr, "Bad rows = %d\n", stats.bad_rows);
    fprintf(stderr, "Longest bad row run = %d\n", stats.longest_bad_row_run);
    for (i = 0;  i < 256;  i++)
        printf("%d", (ecm_len[i] < 0)  ?  0  :  1);
    printf("\n");
    t4_up = FALSE;
}
/*- End of function --------------------------------------------------------*/

static void v21_put_bit(void *user_data, int bit)
{
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_TRAINING_FAILED:
            fprintf(stderr, "V.21 Training failed\n");
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            fprintf(stderr, "V.21 Training succeeded\n");
            t4_begin();
            break;
        case PUTBIT_CARRIER_UP:
            fprintf(stderr, "V.21 Carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            fprintf(stderr, "V.21 Carrier down\n");
            t4_end();
            break;
        default:
            fprintf(stderr, "V.21 Eh!\n");
            break;
        }
        return;
    }
    if (fast_trained == FAX_NONE)
        hdlc_rx_put_bit(&hdlcrx, bit);
    //printf("V.21 Rx bit %d - %d\n", rx_bits++, bit);
}
/*- End of function --------------------------------------------------------*/

#if defined(ENABLE_V17)
static void v17_put_bit(void *user_data, int bit)
{
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_TRAINING_FAILED:
            fprintf(stderr, "V.17 Training failed\n");
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            fprintf(stderr, "V.17 Training succeeded\n");
            fast_trained = FAX_V17_RX;
            t4_begin();
            break;
        case PUTBIT_CARRIER_UP:
            fprintf(stderr, "V.17 Carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            fprintf(stderr, "V.17 Carrier down\n");
            t4_end();
            if (fast_trained == FAX_V17_RX)
            {
                fast_trained = FAX_NONE;
                ecm_mode = TRUE;
            }
            break;
        default:
            fprintf(stderr, "V.17 Eh!\n");
            break;
        }
        return;
    }
    if (ecm_mode)
    {
        hdlc_rx_put_bit(&hdlcrx, bit);
    }
    else
    {
        if (t4_rx_put_bit(&t4_state, bit))
        {
            t4_end();
            fprintf(stderr, "End of page detected\n");
        }
    }
    //printf("V.17 Rx bit %d - %d\n", rx_bits++, bit);
}
/*- End of function --------------------------------------------------------*/
#endif

static void v29_put_bit(void *user_data, int bit)
{
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_TRAINING_FAILED:
            //fprintf(stderr, "V.29 Training failed\n");
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            fprintf(stderr, "V.29 Training succeeded\n");
            fast_trained = FAX_V29_RX;
            t4_begin();
            break;
        case PUTBIT_CARRIER_UP:
            //fprintf(stderr, "V.29 Carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            //fprintf(stderr, "V.29 Carrier down\n");
            t4_end();
            if (fast_trained == FAX_V29_RX)
                fast_trained = FAX_NONE;
            break;
        default:
            fprintf(stderr, "V.29 Eh!\n");
            break;
        }
        return;
    }

    if (ecm_mode)
    {
        hdlc_rx_put_bit(&hdlcrx, bit);
    }
    else
    {
        if (t4_rx_put_bit(&t4_state, bit))
        {
            t4_end();
            fprintf(stderr, "End of page detected\n");
        }
    }
    //printf("V.29 Rx bit %d - %d\n", rx_bits++, bit);
}
/*- End of function --------------------------------------------------------*/

static void v27ter_put_bit(void *user_data, int bit)
{
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_TRAINING_FAILED:
            //fprintf(stderr, "V.27ter Training failed\n");
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            fprintf(stderr, "V.27ter Training succeeded\n");
            fast_trained = FAX_V27TER_RX;
            t4_begin();
            break;
        case PUTBIT_CARRIER_UP:
            //fprintf(stderr, "V.27ter Carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            //fprintf(stderr, "V.27ter Carrier down\n");
            if (fast_trained == FAX_V27TER_RX)
                fast_trained = FAX_NONE;
            break;
        default:
            fprintf(stderr, "V.27ter Eh!\n");
            break;
        }
        return;
    }

    if (ecm_mode)
    {
        hdlc_rx_put_bit(&hdlcrx, bit);
    }
    else
    {
        if (t4_rx_put_bit(&t4_state, bit))
        {
            t4_end();
            fprintf(stderr, "End of page detected\n");
        }
    }
    //printf("V.27ter Rx bit %d - %d\n", rx_bits++, bit);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    fsk_rx_state_t fsk;
#if defined(ENABLE_V17)
    v17_rx_state_t v17;
#endif
    v29_rx_state_t v29;
    v27ter_rx_state_t v27ter;
    int16_t amp[SAMPLES_PER_CHUNK];
    AFfilehandle inhandle;
    int len;
    const char *filename;
    
    filename = "fax_samp.wav";

    if (argc > 1)
        filename = argv[1];

    inhandle = afOpenFile(filename, "r", NULL);
    if (inhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open wave file '%s'\n", filename);
        exit(2);
    }
    memset(&t30_dummy, 0, sizeof(t30_dummy));
    span_log_init(&t30_dummy.logging, SPAN_LOG_FLOW, NULL);
    span_log_set_protocol(&t30_dummy.logging, "T.30");

    hdlc_rx_init(&hdlcrx, FALSE, TRUE, 5, hdlc_accept, NULL);
    fsk_rx_init(&fsk, &preset_fsk_specs[FSK_V21CH2], TRUE, v21_put_bit, NULL);
#if defined(ENABLE_V17)
    v17_rx_init(&v17, 14400, v17_put_bit, NULL);
#endif
    v29_rx_init(&v29, 9600, v29_put_bit, NULL);
    v27ter_rx_init(&v27ter, 4800, v27ter_put_bit, NULL);
    fsk_rx_signal_cutoff(&fsk, -45.0);
#if defined(ENABLE_V17)
    v17_rx_signal_cutoff(&v17, -45.0);
#endif
    v29_rx_signal_cutoff(&v29, -45.0);
    v27ter_rx_signal_cutoff(&v27ter, -40.0);

    //span_log_init(&v29.logging, SPAN_LOG_FLOW, NULL);
    //span_log_set_protocol(&v29.logging, "V.29");
    //span_log_set_level(&v29.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);

    if (t4_rx_init(&t4_state, "fax_decode.tif", T4_COMPRESSION_ITU_T4_2D))
    {
       fprintf(stderr, "Failed to init\n");
        exit(0);
    }
        
    for (;;)
    {
        len = afReadFrames(inhandle, AF_DEFAULT_TRACK, amp, SAMPLES_PER_CHUNK);
        if (len < SAMPLES_PER_CHUNK)
            break;
        fsk_rx(&fsk, amp, len);
#if defined(ENABLE_V17)
        v17_rx(&v17, amp, len);
#endif
        v29_rx(&v29, amp, len);
        v27ter_rx(&v27ter, amp, len);
    }
    t4_rx_end(&t4_state);

    if (afCloseFile(inhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", filename);
        exit(2);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fsk_tests.c - Tests for the low speed FSK modem code (V.21, V.23, etc.).
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
 * $Id: fsk_tests.c,v 1.3 2004/03/12 16:27:25 steveu Exp $
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

hdlc_rx_state_t rxhdlc;
hdlc_tx_state_t txhdlc;

async_rx_state_t rxasync;
async_tx_state_t txasync;

int full_len;
uint8_t old_buf[1000];
uint8_t new_buf[1000];

int good_hdlc_messages;

int async_chars;
int rx_async_chars;
int rx_async_char_mask;

static int test_get_bit(void *user_data)
{
    return hdlc_tx_getbit(&txhdlc);
}
/*- End of function --------------------------------------------------------*/

static void test_put_bit(void *user_data, int bit)
{
    hdlc_rx_bit(&rxhdlc, bit);
}
/*- End of function --------------------------------------------------------*/

static int test_get_async_byte(void *user_data)
{
    int byte;
    
    byte = async_chars & 0xFF;
    async_chars++;
    //printf("Transmitted byte is 0x%X\n", byte);
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

static void pkt_handler(void *user_data, uint8_t *pkt, int len)
{
    printf("Good message (%d bytes)\n", len);
    good_hdlc_messages++;
#if 0
    {
        int i;
    
        for (i = 0;  i < len;  i++)
        {
            printf("%x ", pkt[i]);
            if ((i & 0xf) == 0xf)
                printf("\n");
        }
    }
    printf("\n");
#endif
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    fsk_tx_state_t tx;
    fsk_rx_state_t rx;
    int16_t amp[NB_SAMPLES];
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int outframes;    
    int i;
    int j;
    int hdlc_len;
    int len;
    int hdlc_messages;
    awgn_state_t noise_source;

    filesetup = afNewFileSetup();
    if (filesetup == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    outhandle = afOpenFile("fsk.wav", "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", "fsk.wav");
        exit(2);
    }

    awgn_init(&noise_source, 1234567, -15);

    printf("Test with HDLC\n");
    fsk_tx_init(&tx, &preset_fsk_specs[FSK_V21CH2], test_get_bit, NULL);
    fsk_rx_init(&rx, &preset_fsk_specs[FSK_V21CH2], TRUE, test_put_bit, NULL);

    hdlc_tx_init(&txhdlc, NULL, NULL);
    hdlc_rx_init(&rxhdlc, pkt_handler, NULL);

    hdlc_tx_preamble(&txhdlc, 40);
    hdlc_len = 2;

    hdlc_messages = 0;
    good_hdlc_messages = 0;
    for (;;)
    {
        len = fsk_tx(&tx, amp, NB_SAMPLES);
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  NB_SAMPLES);
        if (outframes != len)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
        for (i = 0;  i < len;  i++)
            amp[i] = alaw_to_linear(linear_to_alaw(saturate(amp[i] + awgn(&noise_source))));
//printf("%2x %2x %2x %2x\n", amp[0], amp[1], amp[2], amp[3]);
        fsk_rx(&rx, amp, len);

        if (txhdlc.len == 0)
        {
            if (hdlc_messages > 100)
                break;
            memcpy(old_buf, new_buf, hdlc_len);
            full_len = hdlc_len;
            hdlc_len = (rand() & 0x3F) + 100;
            for (j = 0;  j < hdlc_len;  j++)
                new_buf[j] = rand();
//printf("Suck - %d - %x %x %x %x\n", len, new_buf[0], new_buf[1], new_buf[2], new_buf[3]);
            hdlc_tx_packet(&txhdlc, new_buf, hdlc_len);
            hdlc_messages++;
        }
    }
    printf("%d HDLC messages sent. %d good HDLC messages received\n",
           hdlc_messages,
           good_hdlc_messages);

    printf("Test with async 8N1\n");
    fsk_tx_init(&tx, &preset_fsk_specs[FSK_V21CH2], async_tx_bit, &txasync);
    fsk_rx_init(&rx, &preset_fsk_specs[FSK_V21CH2], FALSE, async_rx_bit, &rxasync);

    async_tx_init(&txasync, 8, ASYNC_PARITY_NONE, 1, test_get_async_byte, NULL);
    async_rx_init(&rxasync, 8, ASYNC_PARITY_NONE, 1, test_put_async_byte, NULL);

    rx_async_chars = 0;
    rx_async_char_mask = 0xFF;
    async_chars = 0;
    for (;;)
    {
        len = fsk_tx(&tx, amp, NB_SAMPLES);
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  len);
        if (outframes != len)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
        for (i = 0;  i < len;  i++)
            amp[i] = alaw_to_linear(linear_to_alaw(saturate(amp[i] + awgn(&noise_source))));
        
//printf("%2x %2x %2x %2x\n", amp[0], amp[1], amp[2], amp[3]);
        fsk_rx(&rx, amp, len);

        if (async_chars > 1000)
            break;
    }

    printf("Chars=%d, PE=%d, FE=%d\n", rx_async_chars, rxasync.parity_errors, rxasync.framing_errors);
    
    printf("Test with async 7E1\n");
    fsk_tx_init(&tx, &preset_fsk_specs[FSK_V21CH2], async_tx_bit, &txasync);
    fsk_rx_init(&rx, &preset_fsk_specs[FSK_V21CH2], FALSE, async_rx_bit, &rxasync);

    async_tx_init(&txasync, 7, ASYNC_PARITY_EVEN, 1, test_get_async_byte, NULL);
    async_rx_init(&rxasync, 7, ASYNC_PARITY_EVEN, 1, test_put_async_byte, NULL);

    rx_async_chars = 0;
    rx_async_char_mask = 0x7F;
    async_chars = 0;
    for (;;)
    {
        len = fsk_tx(&tx, amp, NB_SAMPLES);
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  len);
        if (outframes != len)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
        for (i = 0;  i < len;  i++)
            amp[i] = alaw_to_linear(linear_to_alaw(saturate(amp[i] + awgn(&noise_source))));
        
//printf("%2x %2x %2x %2x\n", amp[0], amp[1], amp[2], amp[3]);
        fsk_rx(&rx, amp, len);

        if (async_chars > 1000)
            break;
    }

    printf("Chars=%d, PE=%d, FE=%d\n", rx_async_chars, rxasync.parity_errors, rxasync.framing_errors);

    printf("Test with async 8O1\n");
    fsk_tx_init(&tx, &preset_fsk_specs[FSK_V21CH2], async_tx_bit, &txasync);
    fsk_rx_init(&rx, &preset_fsk_specs[FSK_V21CH2], FALSE, async_rx_bit, &rxasync);

    async_tx_init(&txasync, 8, ASYNC_PARITY_ODD, 1, test_get_async_byte, NULL);
    async_rx_init(&rxasync, 8, ASYNC_PARITY_ODD, 1, test_put_async_byte, NULL);

    rx_async_chars = 0;
    rx_async_char_mask = 0xFF;
    async_chars = 0;
    for (;;)
    {
        len = fsk_tx(&tx, amp, NB_SAMPLES);
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  len);
        if (outframes != len)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
        for (i = 0;  i < len;  i++)
            amp[i] = alaw_to_linear(linear_to_alaw(saturate(amp[i] + awgn(&noise_source))));
        
//printf("%2x %2x %2x %2x\n", amp[0], amp[1], amp[2], amp[3]);
        fsk_rx(&rx, amp, len);

        if (async_chars > 1000)
            break;
    }

    printf("Chars=%d, PE=%d, FE=%d\n", rx_async_chars, rxasync.parity_errors, rxasync.framing_errors);

    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", "fsk.wav");
        exit(2);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

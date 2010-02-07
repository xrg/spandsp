/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t31_tests.c - Tests for the T.31 command interpreter.
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
 * $Id: t31_tests.c,v 1.18 2005/12/25 15:08:37 steveu Exp $
 */

/*! \file */

/*! \page t31_tests_page T.31 tests
\section t31_tests_page_sec_1 What does it do?
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE

#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"
#include "spandsp/t30_fcf.h"

#define OUTPUT_FILE_NAME_T30    "t31_tests_t30.wav"
#define OUTPUT_FILE_NAME_T31    "t31_tests_t31.wav"

#define DLE 0x10
#define ETX 0x03
#define SUB 0x1A

int countdown = 0;

int t31_send(t31_state_t *s, char *t)
{
    printf("%s", t);
    t31_at_rx(s, t, strlen(t));
}

int t31_expect(t31_state_t *s, char *t)
{
}

int t31_send_hdlc(t31_state_t *s, uint8_t *t, int len)
{
    uint8_t buf[100];
    int i;
    int j;

    for (i = 0, j = 0;  i < len;  i++)
    {
        if (*t == DLE)
            buf[j++] = DLE;
        buf[j++] = *t++;
    }
    buf[j++] = DLE;
    buf[j++] = ETX;
    t31_at_rx(s, (char *) buf, j);
}

int general_test(t31_state_t *s)
{
    t31_send(s, "ATA\n");
    //t31_send(s, "ATDT123 456-7890PFVLD\n", 22);
    t31_send(s, "ATH\n");
    t31_send(s, "ATH1\n");
    t31_send(s, "ATH1\n");
    t31_send(s, "atI0");
    t31_send(s, "\n");
    t31_send(s, "ATI3\n");
    t31_send(s, "ATI8\n");
    t31_send(s, "ATI9\n");
    t31_send(s, "ATL\n");
    t31_send(s, "ATM\n");
    t31_send(s, "ATN\n");
    t31_send(s, "ATO\n");
    t31_send(s, "ATQ\n");
    t31_send(s, "ATS42?\n");
    t31_send(s, "ATS42=1\n");
    t31_send(s, "ATS42.5?\n");
    t31_send(s, "ATS42.5=1\n");
    t31_send(s, "ATS42.5?\n");
    t31_send(s, "ATS42?\n");
    t31_send(s, "ATV1\n");
    t31_send(s, "ATZ1\n");
    t31_send(s, "AT+FAA=0\n");
    t31_send(s, "AT+FAA=?\n");
    t31_send(s, "AT+FAA?\n");
    t31_send(s, "AT+FCLASS?\n");
    t31_send(s, "AT+FCLASS=?\n");
    t31_send(s, "AT+FMFR?\n");
    t31_send(s, "AT+FMDL?\n");
    t31_send(s, "AT+FREV?\n");
    t31_send(s, "AT+FRH=1\n");
    t31_send(s, "AT+FRH=3\n");
    t31_send(s, "AT+FRM=96\n");
    t31_send(s, "AT+FRS=1\n");
    t31_send(s, "AT+FTH=3\n");
    t31_send(s, "AT+FTM=144\n");
    t31_send(s, "AT+FTS=1\n");
    t31_send(s, "AT+FREV?\n");
    t31_send(s, "AT+V\n");
    t31_send(s, "AT#CID=?\n");
    t31_send(s, "AT#CID=0\n");
    t31_send(s, "AT#CID=10\n");
    t31_send(s, "AT#CID?\n");
    t31_send(s, "AT&C1\n");
    t31_send(s, "AT&D2\n");
    t31_send(s, "AT&F\n");
    t31_send(s, "AT&H7\n");
    t31_send(s, "atE1\n");
    t31_send(s, "atL1\n");
    t31_send(s, "atM1\n");
    t31_send(s, "atN1\n");
    t31_send(s, "atO\n");
    
    return 0;
}

int fax_send_test(t31_state_t *s)
{
    uint8_t frame[100];
    int i;

    t31_send(s, "ATI0\n");
    t31_send(s, "ATI3\n");
    t31_send(s, "ATS3=12\n");
    t31_send(s, "ATS3=?\n");
    t31_send(s, "ATS3?\n");
    t31_send(s, "AT+FCLASS?\n");
    t31_send(s, "AT+FCLASS=1\n");
    t31_send(s, "AT+FCLASS=?\n");
    t31_expect(s, "OK");
    t31_send(s, "ATD123456789\n");
    t31_expect(s, "CONNECT");
    //<NSF frame>         AT+FRH=3 is implied when dialing in AT+FCLASS=1 state
    //<DLE><ETX>
    t31_expect(s, "OK");
    t31_send(s, "AT+FRH=3\n");
    t31_expect(s, "CONNECT");
    //<CSI frame data>
    //<DLE><ETX>
    t31_expect(s, "OK");
    t31_send(s, "AT+FRH=3\n");	
    t31_expect(s, "CONNECT");
    //<DIS frame data>
    //<DLE><ETX>
    t31_expect(s, "OK");
    t31_send(s, "AT+FRH=3\n");
    t31_expect(s, "NO CARRIER");
    t31_send(s, "AT+FTH=3\n");
    t31_expect(s, "CONNECT");
    frame[0] = 0xFF;
    frame[1] = 0x03;
    frame[2] = T30_TSI;
    for (i = 3;  i < 3 + 21;  i++)
        frame[i] = ' ';
    t31_send_hdlc(s, frame, i);
    t31_expect(s, "CONNECT");
    frame[0] = 0xFF;
    frame[1] = 0x13;
    frame[2] = T30_DCS;
    for (i = 3;  i < 5;  i++)
        frame[i] = 0;
    t31_send_hdlc(s, frame, i);
    t31_expect(s, "OK");
    t31_send(s, "AT+FTS=8;+FTM=96\n");
    t31_expect(s, "CONNECT");
    t31_at_rx(s, "\x10\x04", 2);
//<TCF data pattern>
//<DLE><ETX>
    t31_expect(s, "OK");
    t31_send(s, "AT+FRH=3\n");
    t31_expect(s, "CONNECT");
    //<CFR frame data>
    //<DLE><ETX>
    t31_expect(s, "OK");
    t31_send(s, "AT+FRH=3\n");
    t31_expect(s, "NO CARRIER");
    t31_send(s, "AT+FTM=96\n");
    t31_expect(s, "CONNECT");
    t31_at_rx(s, "\x10\x04", 2);
//<page image data>
//<DLE><ETX>
    t31_expect(s, "OK");
    t31_send(s, "AT+FTS=8;+FTH=3\n");
    t31_expect(s, "CONNECT");
    frame[0] = 0xFF;
    frame[1] = 0x13;
    frame[2] = T30_EOP;
    t31_send_hdlc(s, frame, i);
    t31_expect(s, "OK");
    t31_send(s, "AT+FRH=3\n");
    t31_expect(s, "CONNECT");
    //<MCF frame data>
    //<DLE><ETX>
    t31_expect(s, "OK");
    t31_send(s, "AT+FRH=3\n");
    t31_expect(s, "NO CARRIER");
    t31_send(s, "AT+FTH=3\n");
    t31_expect(s, "CONNECT");
    frame[0] = 0xFF;
    frame[1] = 0x13;
    frame[2] = T30_DCN;
    t31_send_hdlc(s, frame, i);
    t31_expect(s, "OK");
    t31_send(s, "ATH0\n");
    t31_expect(s, "OK");
    
    return 0;
}

int fax_receive_test(t31_state_t *s)
{
    t31_send(s, "AT+FCLASS=1\n");
    t31_expect(s, "OK");
    t31_expect(s, "RING");
    t31_send(s, "ATA\n");
    t31_expect(s, "CONNECT");
//<CSI frame data>
//<DLE><ETX>
    t31_expect(s, "CONNECT");
//<DIS frame data>
//<DLE><ETX>
    t31_expect(s, "OK");
    t31_send(s, "AT+FRH=3\n");
    t31_expect(s, "CONNECT");
    //<TSI frame data>
    //<DLE><ETX>
    t31_expect(s, "OK");
    t31_send(s, "AT+FRH=3\n");
    t31_expect(s, "CONNECT");
    //<DCS frame data>
    //<DLE><ETX>
    t31_expect(s, "OK");
    t31_send(s, "AT+FRH=3\n");
    t31_expect(s, "NO CARRIER");
    t31_send(s, "AT+FRM=96\n");
    t31_expect(s, "CONNECT");
    //<TCF data>
    //<DLE><ETX>
    t31_expect(s, "NO CARRIER");	
    t31_send(s, "AT+FTH=3\n");
    t31_expect(s, "CONNECT");
//<CFR frame data>
//<DLE><ETX>
    t31_expect(s, "CONNECT");
    t31_send(s, "AT+FRM=96\n");
    t31_expect(s, "CONNECT");
    //<page image data>
    //<DLE><ETX>
    t31_expect(s, "NO CARRIER");
    t31_send(s, "AT+FTS=8;AT+FTH=3\n");
    t31_expect(s, "CONNECT");
    //<EOP frame data>
    //<DLE><ETX>
    t31_expect(s, "OK");
    t31_send(s, "AT+FRH=3\n");
    t31_expect(s, "NO CARRIER");
    t31_send(s, "AT+FTH=3\n");
    t31_expect(s, "CONNECT");
//<MCF frame data>
//<DLE><ETX>
    t31_expect(s, "OK");
    t31_send(s, "AT+FRH=3\n");
    t31_expect(s, "NO CARRIER");
    t31_send(s, "ATH0\n");
    t31_expect(s, "OK");
    return 0;
}

#define SAMPLES_PER_CHUNK 160

void phase_b_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    
    i = (intptr_t) user_data;
    printf("Phase B handler on channel %d - 0x%X\n", i, result);
}
/*- End of function --------------------------------------------------------*/

void phase_d_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    t30_stats_t t;
    char ident[21];

    i = (intptr_t) user_data;
    printf("Phase D handler on channel %d - 0x%X\n", i, result);
    t30_get_transfer_statistics(s, &t);
    printf("Phase D: bit rate %d\n", t.bit_rate);
    printf("Phase D: pages transferred %d\n", t.pages_transferred);
    printf("Phase D: image size %d x %d\n", t.columns, t.rows);
    printf("Phase D: image resolution %d x %d\n", t.column_resolution, t.row_resolution);
    printf("Phase D: bad rows %d\n", t.bad_rows);
    printf("Phase D: longest bad row run %d\n", t.longest_bad_row_run);
    printf("Phase D: coding method %d\n", t.encoding);
    printf("Phase D: image size %d\n", t.image_size);
    t30_get_local_ident(s, ident);
    printf("Phase D: local ident '%s'\n", ident);
    t30_get_far_ident(s, ident);
    printf("Phase D: remote ident '%s'\n", ident);
}
/*- End of function --------------------------------------------------------*/

void phase_e_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    
    i = (intptr_t) user_data;
    printf("Phase E handler on channel %d\n", i);
}
/*- End of function --------------------------------------------------------*/

static int modem_call_control(t31_state_t *s, void *user_data, int op, const char *num)
{
    switch (op)
    {
    case T31_MODEM_CONTROL_ANSWER:
        printf("\nAnswering\n");
        break;
    case T31_MODEM_CONTROL_CALL:
        printf("\nDialing '%s'\n", num);
        break;
    case T31_MODEM_CONTROL_HANGUP:
        printf("\nHanging up\n");
        break;
    default:
        printf("\nModem control operation %d\n", op);
        break;
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

int at_tx_handler(t31_state_t *s, void *user_data, const uint8_t *buf, int len)
{
    int i;

    for (i = 0;  i < len;  i++)
        putchar(buf[i]);
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int i;
    int j;
    int outframes;
    char buf[128 + 1];
    int16_t silence[SAMPLES_PER_CHUNK];
    t30_state_t t30_state;
    t31_state_t t31_state;
    int16_t t30_amp[SAMPLES_PER_CHUNK];
    int16_t t31_amp[SAMPLES_PER_CHUNK];
    int t30_len;
    int t31_len;
    AFfilesetup filesetup;
    AFfilehandle t30_handle;
    AFfilehandle t31_handle;
    char *pts_name;
    char *tty_name;
    
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

    memset(silence, 0, sizeof(silence));
 
    t30_handle = afOpenFile(OUTPUT_FILE_NAME_T30, "w", filesetup);
    if (t30_handle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUTPUT_FILE_NAME_T30);
        exit(2);
    }
    t31_handle = afOpenFile(OUTPUT_FILE_NAME_T31, "w", filesetup);
    if (t31_handle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUTPUT_FILE_NAME_T31);
        exit(2);
    }

    fax_init(&t30_state, TRUE, NULL);
    t30_set_local_ident(&t30_state, "11111111");
    t30_set_tx_file(&t30_state, "itutests.tif", -1, -1);
    t30_set_phase_b_handler(&t30_state, phase_b_handler, (void *) 0);
    t30_set_phase_d_handler(&t30_state, phase_d_handler, (void *) 0);
    t30_set_phase_e_handler(&t30_state, phase_e_handler, (void *) 0);
    memset(t30_amp, 0, sizeof(t30_amp));

    if (t31_init(&t31_state, at_tx_handler, NULL, modem_call_control, NULL) < 0)
    {
        fprintf(stderr, "Cannot start the fax modem\n");
        exit(2);
    }
    countdown = 250;
    for (;;)
    {
        t30_len = fax_tx(&t30_state, t30_amp, SAMPLES_PER_CHUNK);
        //memset(t30_amp, 0, SAMPLES_PER_CHUNK*2);
        //t30_len = 160;
        /* The receive side always expects a full block of samples, but the
           transmit side may not be sending any when it doesn't need to. We
           may need to pad with some silence. */
        if (t30_len < SAMPLES_PER_CHUNK)
        {
            memset(t30_amp + t30_len, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - t30_len));
            t30_len = SAMPLES_PER_CHUNK;
        }
        outframes = afWriteFrames(t30_handle, AF_DEFAULT_TRACK, t30_amp, t30_len);
        if (outframes != t30_len)
            break;
        if (t31_rx(&t31_state, t30_amp, t30_len))
            break;
        if (countdown   &&  --countdown == 0)
        {
            t31_call_event(&t31_state, T31_CALL_EVENT_ALERTING);
            countdown = 250;
        }

        t31_len = t31_tx(&t31_state, t31_amp, SAMPLES_PER_CHUNK);
        if (t31_len < SAMPLES_PER_CHUNK)
        {
            memset(t31_amp + t31_len, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - t31_len));
            t31_len = SAMPLES_PER_CHUNK;
        }
        outframes = afWriteFrames(t31_handle, AF_DEFAULT_TRACK, t31_amp, t31_len);
        if (outframes != t31_len)
            break;
        if (fax_rx(&t30_state, t31_amp, SAMPLES_PER_CHUNK))
            break;
            
        usleep(10000);
    }
    if (afCloseFile(t30_handle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUTPUT_FILE_NAME_T30);
        exit(2);
    }
    if (afCloseFile(t31_handle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUTPUT_FILE_NAME_T31);
        exit(2);
    }
    afFreeFileSetup(filesetup);
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

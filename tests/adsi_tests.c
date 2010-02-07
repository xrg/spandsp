/*
 * SpanDSP - a series of DSP components for telephony
 *
 * adsi_tests.c
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
 * $Id: adsi_tests.c,v 1.13 2005/09/01 17:06:45 steveu Exp $
 */

/*! \page adsi_tests_page ADSI tests
\section adsi_tests_page_sec_1 What does it do?
\section adsi_tests_page_sec_2 How does it work?
*/

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"

#define OUT_FILE_NAME   "adsi.wav"

//#define TEST_CLASS
//#define TEST_CLIP
//#define TEST_ACLIP
//#define TEST_JCLIP
#define TEST_CLIP_DTMF

#define NB_SAMPLES 160

int errors = 0;

adsi_rx_state_t rx_adsi;
adsi_tx_state_t tx_adsi;


#if 0
void adsi_create_message4(void)
{
    uint8_t msg[256];
    int len;
    adsi_tx_state_t state;
    adsi_tx_state_t *s;
    
    s = &state;
    adsi_tx_init(s, ADSI_STANDARD_CLASS);
    len = adsi_add_field(s, msg, -1, CLASS_SDMF_CALLERID, NULL, 0);
    len = adsi_add_field(s, msg, len, 0, "10011750", 8);
    len = adsi_add_field(s, msg, len, 0, "6095551212", 10);
}

void adsi_create_message5(void)
{
    uint8_t msg[256];
    int len;
    adsi_tx_state_t state;
    adsi_tx_state_t *s;
    
    s = &state;
    adsi_tx_init(s, ADSI_STANDARD_CLASS);
    len = adsi_add_field(s, msg, -1, CLASS_SDMF_MSG_WAITING, NULL, 0);
    /* Inactive */
    len = adsi_add_field(s, msg, len, 0, "\x6F", 1);
    len = adsi_add_field(s, msg, len, 0, "\x6F", 1);
    len = adsi_add_field(s, msg, len, 0, "\x6F", 1);
}

void adsi_create_message6(void)
{
    uint8_t msg[256];
    int len;
    adsi_tx_state_t state;
    adsi_tx_state_t *s;
    
    s = &state;
    adsi_tx_init(s, ADSI_STANDARD_CLASS);
    len = adsi_add_field(s, msg, -1, CLASS_SDMF_MSG_WAITING, NULL, 0);
    /* Active */
    len = adsi_add_field(s, msg, len, 0, "\x42", 1);
    len = adsi_add_field(s, msg, len, 0, "\x42", 1);
    len = adsi_add_field(s, msg, len, 0, "\x42", 1);
}

void adsi_create_message8(void)
{
    uint8_t msg[256];
    int len;
    adsi_tx_state_t state;
    adsi_tx_state_t *s;
    
    s = &state;
    adsi_tx_init(s, ADSI_STANDARD_CLIP);
    len = adsi_add_field(s, msg, -1, CLIP_MDMF_CALLERID, NULL, 0);
    len = adsi_add_field(s, msg, len, CLIP_NUM_MSG, "\x03", 1);
}

void adsi_create_message9(void)
{
    uint8_t msg[256];
    int len;
    adsi_tx_state_t state;
    adsi_tx_state_t *s;
    
    s = &state;
    adsi_tx_init(s, ADSI_STANDARD_CLIP);
    len = adsi_add_field(s, msg, -1, CLIP_MDMF_MSG_WAITING, NULL, 0);
    /* Inactive */
    len = adsi_add_field(s, msg, len, CLIP_VISUAL_INDICATOR, "\x00", 1);
}

void adsi_create_message10(void)
{
    uint8_t msg[256];
    int len;
    adsi_tx_state_t state;
    adsi_tx_state_t *s;
    
    s = &state;
    adsi_tx_init(s, ADSI_STANDARD_CLIP);
    len = adsi_add_field(s, msg, -1, CLIP_MDMF_MSG_WAITING, NULL, 0);
    /* Active */
    len = adsi_add_field(s, msg, len, CLIP_VISUAL_INDICATOR, "\xFF", 1);
    len = adsi_add_field(s, msg, len, CLIP_NUM_MSG, "\x05", 1);
}

void adsi_create_message11(void)
{
    uint8_t msg[256];
    int len;
    adsi_tx_state_t state;
    adsi_tx_state_t *s;
    
    s = &state;
    adsi_tx_init(s, ADSI_STANDARD_CLIP);
    len = adsi_add_field(s, msg, -1, CLIP_MDMF_SMS, NULL, 0);
    /* Active */
    len = adsi_add_field(s, msg, len, CLIP_DISPLAY_INFO, "\x00ABC", 4);
}
#endif

#if defined(TEST_CLASS)
int adsi_create_message(adsi_tx_state_t *s, uint8_t *msg)
{
    int len;
    
    len = adsi_add_field(s, msg, -1, CLASS_MDMF_CALLERID, NULL, 0);
    len = adsi_add_field(s, msg, len, MCLASS_DATETIME, "10011750", 8);
    len = adsi_add_field(s, msg, len, MCLASS_CALLER_NUMBER, "12345678", 8);
    len = adsi_add_field(s, msg, len, MCLASS_DIALED_NUMBER, "87654321", 8);
    len = adsi_add_field(s, msg, len, MCLASS_CALLER_NAME, "Steve Underwood", 15);
    return len;
}
#define STANDARD ADSI_STANDARD_CLASS
#endif

#if defined(TEST_CLIP)
int adsi_create_message(adsi_tx_state_t *s, uint8_t *msg)
{
    int len;
    
    len = adsi_add_field(s, msg, -1, CLIP_MDMF_CALLERID, NULL, 0);
    len = adsi_add_field(s, msg, len, CLIP_CALLTYPE, "\x81", 1);
    len = adsi_add_field(s, msg, len, CLIP_DATETIME, "10011750", 8);
    len = adsi_add_field(s, msg, len, CLIP_DIALED_NUMBER, "12345678", 8);
    len = adsi_add_field(s, msg, len, CLIP_CALLER_NUMBER, "87654321", 8);
    len = adsi_add_field(s, msg, len, CLIP_CALLER_NAME, "Steve Underwood", 15);
    return len;
}
#define STANDARD ADSI_STANDARD_CLIP
#endif
#if defined(TEST_CLIP_DTMF)
int adsi_create_message(adsi_tx_state_t *s, uint8_t *msg)
{
    int len;
    
    len = adsi_add_field(s, msg, 0, 'A', (uint8_t *) "12345678", 8);
    return len;
}
#define STANDARD ADSI_STANDARD_CLIP_DTMF
#endif
#if defined(TEST_JCLIP)
int adsi_create_message(adsi_tx_state_t *s, uint8_t *msg)
{
    int len;

    len = adsi_add_field(s, msg, -1, JCLIP_MDMF_CALLERID, NULL, 0);
    len = adsi_add_field(s, msg, len, JCLIP_CALLER_NUMBER, "12345678", 8);
    len = adsi_add_field(s, msg, len, JCLIP_CALLER_NUM_DES, "215", 3);
    len = adsi_add_field(s, msg, len, JCLIP_DIALED_NUMBER, "87654321", 8);
    len = adsi_add_field(s, msg, len, JCLIP_DIALED_NUM_DES, "215", 3);
    return len;
}

#define STANDARD ADSI_STANDARD_JCLIP
#endif

static void put_adsi_msg(void *user_data, const uint8_t *msg, int len)
{
    int i;
    int l;
    uint8_t field_type;
    const uint8_t *field_body;
    int field_len;
    uint8_t body[256];
    
    printf("Good message received (%d bytes)\n", len);

    for (i = 0;  i < len;  i++)
    {
        printf("%2x ", msg[i]);
        if ((i & 0xf) == 0xf)
            printf("\n");
    }
    printf("\n");
    l = -1;
    do
    {
        l = adsi_next_field(&rx_adsi, msg, len, l, &field_type, &field_body, &field_len);
printf("l = %d\n", l);
        if (l > 0)
        {
            if (field_body)
            {
                memcpy(body, field_body, field_len);
                body[field_len] = '\0';
                printf("Type %x, len %d, '%s'\n", field_type, field_len, body);
            }
            else
            {
                printf("Message type %x\n", field_type);
            }
        }
    }
    while (l > 0);
    printf("\n");
}

int main(int argc, char *argv[])
{
    int16_t buf[NB_SAMPLES];
    uint8_t adsi_msg[256 + 42];
    int adsi_msg_len;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int outframes;
    int len;
    char *s;
    uint8_t ch;
    int xx;
    int yy;
    int i;

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

    outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }

    adsi_tx_init(&tx_adsi, STANDARD);
    adsi_rx_init(&rx_adsi, STANDARD, put_adsi_msg, NULL);

    for (i = 0;  i < 1000;  i++)
    {
        len = adsi_tx(&tx_adsi, buf, NB_SAMPLES);
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  buf,
                                  len);
        if (outframes != len)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
        adsi_rx(&rx_adsi, buf, len);

        adsi_msg_len = adsi_create_message(&tx_adsi, adsi_msg);
        adsi_put_message(&tx_adsi, adsi_msg, adsi_msg_len);
    }

    /* Now test TDD operation */
    
#if 0
    /* Check the character encode/decode cycle */
    adsi_tx_init(&tx_adsi, ADSI_STANDARD_TDD);
    adsi_rx_init(&rx_adsi, ADSI_STANDARD_TDD, put_adsi_msg, NULL);
    s = "The quick Brown Fox Jumps Over The Lazy dog 0123456789!@#$%^&*()";
    while ((ch = *s++))
    {
        xx = adsi_encode_baudot(&tx_adsi, ch);
        if ((xx & 0x3E0))
        {
            yy = adsi_decode_baudot(&rx_adsi, (xx >> 5) & 0x1F);
            if (yy)
                printf("%c", yy);
        }
        yy = adsi_decode_baudot(&rx_adsi, xx & 0x1F);
        if (yy)
            printf("%c", yy);
    }
    printf("\n");
#endif
    
    adsi_tx_init(&tx_adsi, ADSI_STANDARD_TDD);
    adsi_rx_init(&rx_adsi, ADSI_STANDARD_TDD, put_adsi_msg, NULL);

    s = "The quick Brown Fox Jumps Over The Lazy dog 0123456789!@#$%^&*()";
    for (i = 0;  i < 3000;  i++)
    {
        len = adsi_tx(&tx_adsi, buf, NB_SAMPLES);
#if 0
        if (len == 0)
            break;
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  buf,
                                  len);
        if (outframes != len)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
#endif
        adsi_rx(&rx_adsi, buf, len);
        adsi_msg_len = adsi_add_field(&tx_adsi, adsi_msg, -1, 0, (uint8_t *) s, strlen(s));
        adsi_put_message(&tx_adsi, adsi_msg, adsi_msg_len);
    }

    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

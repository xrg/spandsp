/*
 * SpanDSP - a series of DSP components for telephony
 *
 * adsi.c - Analogue display services interface and other call ID related handling.
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
 * $Id: adsi.c,v 1.13 2004/03/18 13:24:46 steveu Exp $
 */

/*! \file */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/vector.h"
#include "spandsp/dds.h"
#include "spandsp/power_meter.h"
#include "spandsp/fsk.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"
#include "spandsp/adsi.h"

/*
    The caller ID message formats currently supported are:

        CLASS (Custom Local Area Signaling Services) standard, published by Bellcore:

        ACLIP (Analog Calling Line Identity Presentation) standard, published by the
        Telecommunications Authority of Singapore:

        CLIP (Calling Line Identity Presentation) standard, published by ETSI.

        JCLIP (Japanese Calling Line Identity Presentation) standard, published by NTT.
*/

/*
    Most FSK based CLI formats are similar to the US CLASS one, which is
    as follows:

    The alert tone for CLI during a call is at least 100ms of silence, then
    2130Hz + 2750Hz for 88ms to 110ms. When CLI is presented at ringing time,
    this tone is not sent. In the US, CLI is usually sent between the first
    two rings. This silence period is long in the US, so the message fits easily.
    In other places, where the standard ring tone has much smaller silences,
    a line voltage reversal is used to wake up a power saving receiver, then the
    message is sent, then the phone begins to ring.
    
    The message is sent at Bell 202 modem tones (US), or V.23 modem tones (most other
    places). These are sufficiently similar, that most decoders will work whichever
    modem is used at the transmitter. The data rate is 1200 bits per second. The message
    protocol uses 8-bit data words (bytes), each bounded by a start bit and a stop bit.

    Channel	Carrier	Message	Message	Data		Checksum
    Seizure	Signal	 Type	 Length	Word(s)	  Word
    Signal			 Word	  Word
    
    CHANNEL SEIZURE SIGNAL
    The channel seizure is 30 continuous bytes of 55h (01010101), including
    the start and stop bits (i.e. 300 bits of alternations in total).
    This provides a detectable alternating function to the CPE (i.e. the
    modem data pump).
    
    CARRIER SIGNAL
    The carrier signal consists of 180 bits of 1s. This may be reduced to 80
    bits of 1s for caller ID on call waiting.
    
    MESSAGE TYPE WORD
    Various message types are defined. The common ones for the US CLASS 
    standard are:
        Type 0x04 (SDMF) - single data message. Simple caller ID (CND)
        Type 0x80 (MDMF) - multiple data message. A more flexible caller ID,
                           with extra information.
    
    MESSAGE LENGTH WORD
    The message length word specifies the total number of data words
    to follow.
    
    DATA WORDS
    The data words contain the actual message.
    
    CHECKSUM WORD
    The Checksum Word contains the twos complement of the modulo 256
    sum of the other words in the data message (i.e., message type,
    message length, and data words).  The receiving equipment may
    calculate the modulo 256 sum of the received words and add this
    sum to the received checksum word.  A result of zero generally
    indicates that the message was correctly received.  Message
    retransmission is not supported. The sumcheck word should be followed
    by a minimum of two stop bits.
*/

/*
    CLI by DTMF is usually sent in a very simple way. The exchange does not give
    any prior warning (no reversal, or ring) to wake up the receiver. It just
    sends one of the following DTMF strings:
    
    A<phone number>#
    D1#     Number not available because the caller has restricted it.
    D2#     Number not available because the call is international.
    D3#     Number not available due to technical reasons.
*/

static int adsi_tx_bit(void *user_data)
{
    int bit;
    adsi_tx_state_t *s;
    
    s = (adsi_tx_state_t *) user_data;
    /* This is similar to the async. handling code in fsk.c, but a few special
       things are needed in the preamble, and postamble of an ADSI message. */
    if (s->bitno < 300)
    {
        /* Alternating bit preamble */
        bit = s->bitno & 1;
        s->bitno++;
    }
    else if (s->bitno < 300 + s->ones_len)
    {
        /* All 1s for receiver conditioning */
        /* NB: The receiver is an async one. It needs a rest after the
               alternating 1/0 sequence so it can reliably pick up on
               the next start bit, and sync to the byte stream. */
        /* The length of this period varies with the circumstance */
        bit = 1;
        s->bitno++;
    }
    else if (s->bitno < 300 + s->ones_len + 1)
    {
        if (s->bitpos == 0)
        {
            /* Start bit */
            bit = 0;
            s->bitpos++;
        }
        else if (s->bitpos < 9)
        {
            bit = (s->msg[s->byteno] >> (s->bitpos - 1)) & 1;
            s->bitpos++;
        }
        else
        {
            /* Stop bit */
            bit = 1;
            s->bitpos = 0;
            s->byteno++;
            if (s->byteno > s->msg_len)
                s->bitno++;
        }
    }
    else if (s->bitno < 300 + s->ones_len + 5)
    {
        /* Extra stop bits beyond the last character, to meet the specs., and ensure
           all bits are out of the DSP before we shut off the FSK modem. */
        bit = 1;
        s->bitno++;
    }
    else
    {
        bit = 1;
        if (s->fsk_on)
        {
            /* The FSK should now be switched off. */
            s->fsk_on = FALSE;
            s->msg_len = 0;
        }
    }
    return bit;
}
/*- End of function --------------------------------------------------------*/

static void adsi_rx_bit(void *user_data, int bit)
{
    adsi_rx_state_t *s;
    int i;
    int sum;

    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_CARRIER_UP:
            break;
        case PUTBIT_CARRIER_DOWN:
            break;
        default:
            fprintf(stderr, "Unexpected special put bit value - %d!\n", bit);
            break;
        }
        return;
    }
    s = (adsi_rx_state_t *) user_data;
    bit &= 1;
    if (s->bitpos == 0)
    {
        if (bit == 0)
        {
            /* Start bit */
            s->bitpos++;
            if (s->consecutive_ones > 10)
            {
                /* This is a line idle condition, which means we should
                   restart message acquisition */
                s->consecutive_ones = 0;
                s->msg_len = 0;
            }
        }
        else
        {
            s->consecutive_ones++;
        }
    }
    else if (s->bitpos <= 8)
    {
        s->in_progress >>= 1;
        if (bit)
            s->in_progress |= 0x80;
        s->bitpos++;
    }
    else
    {
        /* Stop bit */
        if (s->msg_len < 256)
        {
            s->msg[s->msg_len++] = s->in_progress;
            if (s->standard == ADSI_STANDARD_JCLIP)
            {
                if (s->msg_len >= 11  &&  s->msg_len == ((s->msg[6] & 0x7F) + 11))
                {
                    /* Test the ITU CRC-16 */
                    if (check_crc_itu16(s->msg, s->msg_len))
                    {
                        /* Strip off the parity bits. It doesn't seem
                           worthwhile actually checking the parity if a
                           CRC check has succeeded. */
                        for (i = 0;  i < s->msg_len - 2;  i++)
                            s->msg[i] &= 0x7F;
                        s->put_msg(s->user_data, s->msg, s->msg_len - 2);
                    }
                    else
                    {
                        fprintf(stderr, "CRC fail\n");
                    }
                    s->msg_len = 0;
                }
            }
            else
            {
                if (s->msg_len >= 3  &&  s->msg_len == (s->msg[1] + 3))
                {
                    /* Test the checksum */
                    sum = 0;
                    for (i = 0;  i < s->msg_len - 1;  i++)
                        sum += s->msg[i];
                    if ((-sum & 0xFF) == s->msg[i])
                        s->put_msg(s->user_data, s->msg, s->msg_len - 1);
                    s->msg_len = 0;
                }
            }
        }
        if (bit != 1)
            s->framing_errors++;
        s->bitpos = 0;
        s->in_progress = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static void adsi_rx_dtmf(void *user_data, char *digits, int len)
{
    adsi_rx_state_t *s;

    s = (adsi_rx_state_t *) user_data;
    if (s->msg_len == 0)
    {
        /* Message starting. Start a 10s timeout, to make things more noise
           tolerant for a detector running continuously when on hook. */
        s->in_progress = 80000;
    }
    for (  ;  len  &&  s->msg_len < 256;  len--)
    {
        if (*digits == '#')
        {
            s->put_msg(s->user_data, s->msg, s->msg_len);
            s->msg_len = 0;
        }
        else
        {
            s->msg[s->msg_len++] = *digits++;
        }
    }
}
/*- End of function --------------------------------------------------------*/

void adsi_rx(adsi_rx_state_t *s, const int16_t *amp, int len)
{
    switch (s->standard)
    {
    case ADSI_STANDARD_CLIP_DTMF:
        /* Apply a message timeout. */
        s->in_progress -= len;
        if (s->in_progress <= 0)
            s->msg_len = 0;
        dtmf_rx(&(s->dtmfrx), amp, len);
        break;
    default:
        fsk_rx(&(s->fskrx), amp, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

void adsi_rx_init(adsi_rx_state_t *s,
                  int standard,
                  put_msg_func_t put_msg,
                  void *user_data)
{
    memset(s, 0, sizeof(*s));
    s->put_msg = put_msg;
    s->user_data = user_data;
    switch (standard)
    {
    case ADSI_STANDARD_CLASS:
        fsk_rx_init(&(s->fskrx), &preset_fsk_specs[FSK_BELL202], FALSE, adsi_rx_bit, s);
        break;
    case ADSI_STANDARD_CLIP:
    case ADSI_STANDARD_ACLIP:
    case ADSI_STANDARD_JCLIP:
        fsk_rx_init(&(s->fskrx), &preset_fsk_specs[FSK_V23CH1], FALSE, adsi_rx_bit, s);
        break;
    case ADSI_STANDARD_CLIP_DTMF:
        dtmf_rx_init(&(s->dtmfrx), adsi_rx_dtmf, s);
        break;
    }
    s->standard = standard;
}
/*- End of function --------------------------------------------------------*/

int adsi_tx(adsi_tx_state_t *s, int16_t *amp, int max_len)
{
    int len;

    len = tone_gen(&(s->alert_tone_gen), amp, max_len);
    switch (s->standard)
    {
    case ADSI_STANDARD_CLIP_DTMF:
        if (len < max_len)
            len += dtmf_tx(&(s->dtmftx), amp, max_len - len);
        break;
    default:
        if (len < max_len  &&  s->fsk_on)
            len += fsk_tx(&(s->fsktx), amp + len, max_len - len);
        break;
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

void adsi_send_alert_tone(adsi_tx_state_t *s)
{
    tone_gen_init(&(s->alert_tone_gen), &(s->alert_tone_desc));
}
/*- End of function --------------------------------------------------------*/

int adsi_put_message(adsi_tx_state_t *s, uint8_t *msg, int len)
{
    int i;
    int j;
    int k;
    int byte;
    int parity;
    int sum;

    if (s->msg_len > 0)
        return 0;
    switch (s->standard)
    {
    case ADSI_STANDARD_CLIP_DTMF:
        if (len >= 128)
            return -1;
        msg[len] = '\0';
        len = dtmf_put(&(s->dtmftx), msg);
        break;
    case ADSI_STANDARD_JCLIP:
        if (len > 128 - 9)
            return -1;
        s->msg[0] = 0x10; //DLE
        s->msg[1] = 0x01; //SOH
        s->msg[2] = 0x07; //header
        s->msg[3] = 0x10; //DLE
        s->msg[4] = 0x02; //STX
        memcpy(&s->msg[5], msg, len);
        /* Force the length in case it is wrong */
        s->msg[6] = len - 2;
        i = len + 5;
        s->msg[i++] = 0x10; //DLE
        s->msg[i++] = 0x03; //ETX
        /* Set the parity bits */
        for (j = 0;  j < i;  j++)
        {
            byte = s->msg[j];
            parity = 0;
            for (k = 1;  k <= 7;  k++)
                parity ^= (byte << k);
            s->msg[j] = (s->msg[j] & 0x7F) | (parity & 0x80);
        }
        s->msg_len = append_crc_itu16(s->msg, i);

        s->ones_len = 80;
        break;
    default:
        if (len > 255)
            return -1;
        memcpy(s->msg, msg, len);
        /* Force the length in case it is wrong */
        s->msg[1] = len - 2;
        /* Add the sumcheck */
        sum = 0;
        for (i = 0;  i < len;  i++)
            sum += s->msg[i];
        s->msg[len] = (-sum) & 0xFF;
        s->msg_len = len + 1;
        s->ones_len = 80;
        break;
    }    
    /* Prepare the bit sequencing */
    s->byteno = 0;
    s->bitpos = 0;
    s->bitno = 0;
    s->fsk_on = TRUE;
    return len;
}
/*- End of function --------------------------------------------------------*/

void adsi_tx_init(adsi_tx_state_t *s, int standard)
{
    memset(s, 0, sizeof(*s));
    make_tone_gen_descriptor(&(s->alert_tone_desc),
                             2130,
                             -13,
                             2750,
                             -13,
                             110,
                             60,
                             0,
                             0,
                             FALSE);
    switch (standard)
    {
    case ADSI_STANDARD_CLASS:
        fsk_tx_init(&(s->fsktx), &preset_fsk_specs[FSK_BELL202], adsi_tx_bit, s);
        break;
    case ADSI_STANDARD_CLIP:
        fsk_tx_init(&(s->fsktx), &preset_fsk_specs[FSK_V23CH1], adsi_tx_bit, s);
        break;
    case ADSI_STANDARD_ACLIP:
        fsk_tx_init(&(s->fsktx), &preset_fsk_specs[FSK_V23CH1], adsi_tx_bit, s);
        break;
    case ADSI_STANDARD_JCLIP:
        fsk_tx_init(&(s->fsktx), &preset_fsk_specs[FSK_V23CH1], adsi_tx_bit, s);
        break;
    case ADSI_STANDARD_CLIP_DTMF:
        dtmf_tx_init(&(s->dtmftx));
        break;
    }
    s->standard = standard;
}
/*- End of function --------------------------------------------------------*/

int adsi_next_field(adsi_rx_state_t *s, const uint8_t *msg, int msg_len, int pos, uint8_t *field_type, uint8_t const **field_body, int *field_len)
{
    int i;

    switch (s->standard)
    {
    case ADSI_STANDARD_CLASS:
    case ADSI_STANDARD_CLIP:
    case ADSI_STANDARD_ACLIP:
        if (pos >= msg_len)
            return -1;
        /* These standards all use "IE" type fields - type, length, body - and similar headers */
        if (pos <= 0)
        {
            /* Return the message type */
            *field_type = msg[0];
            *field_len = 0;
            *field_body = NULL;
            pos = 2;
        }
        else
        {
            *field_type = msg[pos++];
            *field_len = msg[pos++];
            *field_body = msg + pos;
            pos += *field_len;
        }
        break;
    case ADSI_STANDARD_JCLIP:
        if (pos >= msg_len - 2)
            return -1;
        if (pos <= 0)
        {
            /* Return the message type */
            *field_type = msg[5];
            *field_len = 0;
            *field_body = NULL;
            pos = 7;
        }
        else
        {
            *field_type = msg[pos++];
            *field_len = msg[pos++];
            *field_body = msg + pos;
            pos += *field_len;
        }
        break;
    case ADSI_STANDARD_CLIP_DTMF:
        if (pos >= msg_len)
            return -1;
        *field_type = msg[pos++];
        *field_body = msg + pos;
        i = pos;
        while (i < msg_len  &&  msg[i] != '#')
            i++;
        *field_len = i - pos;
        pos = i;
        if (msg[pos] == '#')
            pos++;
        break;
    }
    return pos;
}
/*- End of function --------------------------------------------------------*/

int adsi_add_field(adsi_tx_state_t *s, uint8_t *msg, int len, uint8_t field_type, uint8_t const *field_body, int field_len)
{
    switch (s->standard)
    {
    case ADSI_STANDARD_CLASS:
    case ADSI_STANDARD_CLIP:
    case ADSI_STANDARD_ACLIP:
    case ADSI_STANDARD_JCLIP:
        /* These standards all use "IE" type fields - type, length, body - and similar headers */
        if (len <= 0)
        {
            /* Initialise a new message. The field type is actually the message type. */
            msg[0] = field_type;
            msg[1] = 0;
            len = 2;
        }
        else
        {
            /* Add to a message in progress. */
            if (field_type)
            {
                msg[len] = field_type;
                msg[len + 1] = field_len;
                memcpy(msg + len + 2, field_body, field_len);
                len += (field_len + 2);
            }
            else
            {
                /* No field type or length, for restricted single message formats */
                memcpy(msg + len, field_body, field_len);
                len += field_len;
            }
        }
        break;
    case ADSI_STANDARD_CLIP_DTMF:
        if (len < 0)
        {
            len = 0;
        }
        else
        {
            msg[len] = field_type;
            memcpy(msg + len + 1, field_body, field_len);
            msg[len + field_len + 1] = '#';
            len += (field_len + 2);
        }
        break;
    }

    return len;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

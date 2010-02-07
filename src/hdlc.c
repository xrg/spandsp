/*
 * SpanDSP - a series of DSP components for telephony
 *
 * hdlc.c
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
 * $Id: hdlc.c,v 1.6 2004/03/12 16:27:24 steveu Exp $
 */

/*! \file */

//#define _ISOC9X_SOURCE  1
//#define _ISOC99_SOURCE  1

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "spandsp/telephony.h"
#include "spandsp/power_meter.h"
#include "spandsp/fsk.h"
#include "spandsp/hdlc.h"

#include "spandsp/timing.h"

/* Yes, yes, I know. HDLC is not DSP. It is needed to go with some DSP
   components, though. */

static const unsigned short int crc_itu16_table[] =
{
    0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
    0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
    0x1081, 0x0108, 0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E,
    0x9CC9, 0x8D40, 0xBFDB, 0xAE52, 0xDAED, 0xCB64, 0xF9FF, 0xE876,
    0x2102, 0x308B, 0x0210, 0x1399, 0x6726, 0x76AF, 0x4434, 0x55BD,
    0xAD4A, 0xBCC3, 0x8E58, 0x9FD1, 0xEB6E, 0xFAE7, 0xC87C, 0xD9F5,
    0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E, 0x54B5, 0x453C,
    0xBDCB, 0xAC42, 0x9ED9, 0x8F50, 0xFBEF, 0xEA66, 0xD8FD, 0xC974,
    0x4204, 0x538D, 0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB,
    0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1, 0xAB7A, 0xBAF3,
    0x5285, 0x430C, 0x7197, 0x601E, 0x14A1, 0x0528, 0x37B3, 0x263A,
    0xDECD, 0xCF44, 0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72,
    0x6306, 0x728F, 0x4014, 0x519D, 0x2522, 0x34AB, 0x0630, 0x17B9,
    0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5, 0xA96A, 0xB8E3, 0x8A78, 0x9BF1,
    0x7387, 0x620E, 0x5095, 0x411C, 0x35A3, 0x242A, 0x16B1, 0x0738,
    0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862, 0x9AF9, 0x8B70,
    0x8408, 0x9581, 0xA71A, 0xB693, 0xC22C, 0xD3A5, 0xE13E, 0xF0B7,
    0x0840, 0x19C9, 0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF,
    0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324, 0xF1BF, 0xE036,
    0x18C1, 0x0948, 0x3BD3, 0x2A5A, 0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E,
    0xA50A, 0xB483, 0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5,
    0x2942, 0x38CB, 0x0A50, 0x1BD9, 0x6F66, 0x7EEF, 0x4C74, 0x5DFD,
    0xB58B, 0xA402, 0x9699, 0x8710, 0xF3AF, 0xE226, 0xD0BD, 0xC134,
    0x39C3, 0x284A, 0x1AD1, 0x0B58, 0x7FE7, 0x6E6E, 0x5CF5, 0x4D7C,
    0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1, 0xA33A, 0xB2B3,
    0x4A44, 0x5BCD, 0x6956, 0x78DF, 0x0C60, 0x1DE9, 0x2F72, 0x3EFB,
    0xD68D, 0xC704, 0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232,
    0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68, 0x3FF3, 0x2E7A,
    0xE70E, 0xF687, 0xC41C, 0xD595, 0xA12A, 0xB0A3, 0x8238, 0x93B1,
    0x6B46, 0x7ACF, 0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9,
    0xF78F, 0xE606, 0xD49D, 0xC514, 0xB1AB, 0xA022, 0x92B9, 0x8330,
    0x7BC7, 0x6A4E, 0x58D5, 0x495C, 0x3DE3, 0x2C6A, 0x1EF1, 0x0F78
};

int append_crc_itu16(uint8_t *buf, int len)
{
    uint16_t crc = 0xFFFF;
    int new_len;

    new_len = len + 2;
    while (len-- > 0)
        crc = (crc >> 8) ^ crc_itu16_table[(crc ^ *buf++) & 0xFF];
    crc ^= 0xFFFF;
    *buf++ = crc;
    *buf++ = crc >> 8;
    return new_len;
}
/*- End of function --------------------------------------------------------*/

int check_crc_itu16(const uint8_t *buf, int len)
{
    uint16_t crc = 0xFFFF;

    while (len-- > 0)
        crc = (crc >> 8) ^ crc_itu16_table[(crc ^ *buf++) & 0xFF];
    return (crc & 0xFFFF) == 0xF0B8;
}
/*- End of function --------------------------------------------------------*/

void hdlc_rx_bit(hdlc_rx_state_t *s, int new_bit)
{
    if (new_bit < 0)
    {
        /* Special conditions */
        switch (new_bit)
        {
        case PUTBIT_CARRIER_UP:
        case PUTBIT_CARRIER_DOWN:
        case PUTBIT_TRAINING_SUCCEEDED:
        case PUTBIT_TRAINING_FAILED:
            s->packet_handler(s->user_data, NULL, new_bit);
            break;
        default:
            //printf("Eh!\n");
            break;
        }
        return;
    }
    s->bitbuf |= (new_bit & 1);
    if ((s->bitbuf & 0x7F) == 0x7E)
    {
        if ((s->bitbuf & 0x80))
        {
            /* Hit HDLC abort */
            s->rx_aborts++;
            s->rx_state = FALSE;
        }
        else
        {
            /* Hit HDLC flag */
            if (s->rx_state)
            {
                if (s->len >= 4)
                {
                    if (!check_crc_itu16(s->buffer, s->len))
                    {
                        //printf("CRC error - %d bytes\n", s->len);
                        s->rx_crc_errors++;
                    }
                    else
                    {
                        s->packet_handler(s->user_data,
                                          s->buffer,
                                          s->len - 2);
                        s->rx_packets++;
                    }
                }
            }
            else
            {
                s->rx_state = TRUE;
            }
            s->len = 0;
            s->numbits = 0;
        }
    }
    else
    {
        if (s->rx_state  &&  (s->bitbuf & 0x3F) != 0x3E)
        {
            s->byteinprogress |= ((s->bitbuf & 0x01) << 8);
            s->byteinprogress >>= 1;
            if (++s->numbits == 8)
            {
                if (s->len >= sizeof(s->buffer))
                {
                    /* Packet too long */
                    s->rx_state = FALSE;
                }
                else
                {
                    s->buffer[s->len++] = s->byteinprogress;
                    s->numbits = 0;
                }
            }
        }
    }
    s->bitbuf <<= 1;
}
/*- End of function --------------------------------------------------------*/

void hdlc_rx_byte(hdlc_rx_state_t *s, int new_byte)
{
    int i;

    s->bitbuf |= new_byte;
    for (i = 0;  i < 8;  i++)
    {
        if ((s->bitbuf & 0x7F00) == 0x7E00)
        {
            if ((s->bitbuf & 0x8000))
            {
                /* Hit HDLC abort */
                s->rx_aborts++;
                s->rx_state = FALSE;
            }
            else
            {
                /* Hit HDLC flag */
                if (s->rx_state)
                {
                    if (s->len >= 4)
                    {
                        if (!check_crc_itu16(s->buffer, s->len))
                        {
                            //printf("CRC error - %d bytes\n", s->len);
                            s->rx_crc_errors++;
                        }
                        else
                        {
                            s->packet_handler(s->user_data,
                                              s->buffer,
                                              s->len - 2);
                            s->rx_packets++;
                        }
                    }
                }
                else
                {
                    s->rx_state = TRUE;
                }
                s->len = 0;
                s->numbits = 0;
            }
        }
        else
        {
            if (s->rx_state  &&  (s->bitbuf & 0x3F00) != 0x3E00)
            {
                s->byteinprogress |= (s->bitbuf & 0x0100);
                s->byteinprogress >>= 1;
                if (++s->numbits == 8)
                {
                    if (s->len >= sizeof(s->buffer))
                    {
                        /* Packet too long */
                        s->rx_state = FALSE;
                    }
                    else
                    {
                        s->buffer[s->len++] = s->byteinprogress;
                        s->numbits = 0;
                    }
                }
            }
        }
        s->bitbuf <<= 1;
    }
}
/*- End of function --------------------------------------------------------*/

void hdlc_tx_packet(hdlc_tx_state_t *s, uint8_t *packet, int len)
{
    int i;
    int byteinprogress;
    int ones;
    int bits;
 
    len = append_crc_itu16(packet, len);

    bits = 0;
    if (s->numbits)
    {
        /* Complete the flag byte currently in progress */
        byteinprogress = 0x7E7E >> (8 - s->numbits);
        for (bits = 0;  bits < s->numbits;  bits++)
        {
            s->buffer[s->len] = (s->buffer[s->len] << 1) | (byteinprogress & 0x01);
            byteinprogress >>= 1;
        }
    }
    ones = 0;
    while (len--)
    {
        byteinprogress = *packet++;
        for (i = 0;  i < 8;  i++)
        {
            if (byteinprogress & 0x01)
            {
                s->buffer[s->len] = (s->buffer[s->len] << 1) | 0x01;
                if (++ones >= 5)
                {
                    if (++bits == 8)
                    {
                        s->len++;
                        bits = 0;
                    }
                    s->buffer[s->len] <<= 1;
                    ones = 0;
                }
            }
            else
            {
                s->buffer[s->len] <<= 1;
                ones = 0;
            }
            if (++bits == 8)
            {
                s->len++;
                bits = 0;
            }
            byteinprogress >>= 1;
        }
    }
    /* Finish off the current byte with some flag bits. If we are at the
       start of a byte we need a whole byte of flag to ensure we cannot
       end up with back to back packets, and no flag byte at all */
    s->numbits = bits;
    byteinprogress = 0x7E7E;
    while (bits++ < 8)
    {
        s->buffer[s->len] <<= 1;
        s->buffer[s->len] |= (byteinprogress & 0x01);
        byteinprogress >>= 1;
    }
    s->len++;
    /* Now a full byte of flag */
    for (bits = 0;  bits < 8;  bits++)
    {
        s->idle_byte = (s->idle_byte << 1) | (byteinprogress & 0x01);
        byteinprogress >>= 1;
    }
    s->idle_byte &= 0xFF;
}
/*- End of function --------------------------------------------------------*/

void hdlc_tx_preamble(hdlc_tx_state_t *s, int len)
{
    /* Some HDLC applications require the ability to force a period of HDLC
       flag words. */
    while (len-- > 0)
        s->buffer[s->len++] = s->idle_byte;
}
/*- End of function --------------------------------------------------------*/

int hdlc_tx_getbyte(hdlc_tx_state_t *s)
{
    int txbyte;

    if (s->len)
    {
        txbyte = s->buffer[s->pos++];
        if (s->pos >= s->len)
        {
            s->pos =
            s->len = 0;
        }
        s->underflow_reported = FALSE;
    }
    else
    {
        txbyte = s->idle_byte;
        if (!s->underflow_reported)
        {
            if (s->underflow_handler)
                s->underflow_handler(s->user_data);
            s->underflow_reported = TRUE;
        }
    }
    return  txbyte;
}
/*- End of function --------------------------------------------------------*/

int hdlc_tx_getbit(hdlc_tx_state_t *s)
{
    int txbit;

    if (s->bits-- == 0)
    {
        s->byte = hdlc_tx_getbyte(s);
        s->bits = 7;
    }
    txbit = (s->byte >> 7) & 0x01;
    s->byte <<= 1;
    return  txbit;
}
/*- End of function --------------------------------------------------------*/

hdlc_rx_state_t *hdlc_rx_init(hdlc_rx_state_t *s,
                              hdlc_packet_handler_t *handler,
                              void *user_data)
{
    memset(s, 0, sizeof(*s));
    s->packet_handler = handler;
    s->user_data = user_data;
    return s;
}
/*- End of function --------------------------------------------------------*/

hdlc_tx_state_t *hdlc_tx_init(hdlc_tx_state_t *s,
                              hdlc_underflow_handler_t *handler,
                              void *user_data)
{
    memset(s, 0, sizeof(*s));
    s->idle_byte = 0x7E;
    s->buffer[s->len++] = s->idle_byte;
    s->buffer[s->len++] = s->idle_byte;
    s->underflow_handler = handler;
    s->user_data = user_data;
    return s;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

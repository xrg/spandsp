/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t31.c - A T.31 compatible class 1 FAX modem interface.
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
 * $Id: t31.c,v 1.17 2005/01/29 09:12:05 steveu Exp $
 */

/*! \file */

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <tiffio.h>

#include "spandsp.h"

enum
{
    ASCII_RESULT_CODES = 1,
    NUMERIC_RESULT_CODES,
    NO_RESULT_CODES
};

t31_profile_t profiles[3] =
{
    {
        .echo = TRUE,
        .verbose = TRUE,
        .result_code_format = ASCII_RESULT_CODES,
        .pulse_dial = FALSE,
        .double_escape = FALSE,
        .s_regs[0] = 0,
        .s_regs[3] = '\r',
        .s_regs[4] = '\n',
        .s_regs[5] = '\b',
        .s_regs[6] = 1,
        .s_regs[7] = 60,
        .s_regs[8] = 5,
        .s_regs[10] = 0
    }
};

typedef const char *(*at_cmd_service_t)(t31_state_t *s, const char *cmd);

typedef struct
{
    const char *tag;
    at_cmd_service_t serv;
} at_cmd_item_t;

static char *manufacturer = "www.opencall.org";
static char *model = "spandsp";
static char *revision = "0.0.2";

#define DLE 0x10
#define ETX 0x03
#define SUB 0x1A

enum
{
    T31_SILENCE_TX,
    T31_CED_TONE,
    T31_CNG_TONE,
    T31_V21_TX,
    T31_V17_TX,
    T31_V27TER_TX,
    T31_V29_TX,
    T31_V21_RX,
    T31_V17_RX,
    T31_V27TER_RX,
    T31_V29_RX
};

enum
{
    RESPONSE_CODE_OK = 0,
    RESPONSE_CODE_CONNECT,
    RESPONSE_CODE_RING,
    RESPONSE_CODE_NO_CARRIER,
    RESPONSE_CODE_ERROR,
    RESPONSE_CODE_XXX,
    RESPONSE_CODE_NO_DIALTONE,
    RESPONSE_CODE_BUSY,
    RESPONSE_CODE_NO_ANSWER,
    RESPONSE_CODE_FCERROR
};

const char *response_codes[] =
{
    "OK",
    "CONNECT",
    "RING",
    "NO CARRIER",
    "ERROR",
    "???",
    "NO DIALTONE",
    "BUSY",
    "NO ANSWER",
    "+FCERROR",
};

static void at_put_response(t31_state_t *s, const char *t)
{
    char buf[3];
    
    buf[0] = s->p.s_regs[3];
    buf[1] = s->p.s_regs[4];
    buf[2] = '\0';
    if (s->p.result_code_format == ASCII_RESULT_CODES)
        s->at_tx_handler(s, s->at_tx_user_data, buf, 2);
    s->at_tx_handler(s, s->at_tx_user_data, t, strlen(t));
    s->at_tx_handler(s, s->at_tx_user_data, buf, 2);
}
/*- End of function --------------------------------------------------------*/

static void at_put_numeric_response(t31_state_t *s, int val)
{
    char buf[20];

    snprintf(buf, sizeof(buf), "%d", val);
    at_put_response(s, buf);
}
/*- End of function --------------------------------------------------------*/

static void at_put_response_code(t31_state_t *s, int code)
{
    char buf[20];

    switch (s->p.result_code_format)
    {
    case ASCII_RESULT_CODES:
        at_put_response(s, response_codes[code]);
        break;
    case NUMERIC_RESULT_CODES:
        snprintf(buf, sizeof(buf), "%d%c", code, s->p.s_regs[3]);
        s->at_tx_handler(s, s->at_tx_user_data, buf, strlen(buf));
        break;
    default:
        /* No result codes */
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void fast_putbit(void *user_data, int bit)
{
    t31_state_t *s;

    s = (t31_state_t *) user_data;
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_TRAINING_FAILED:
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            /* The modem is now trained */
            at_put_response_code(s, RESPONSE_CODE_CONNECT);
            s->rx_signal_present = TRUE;
            break;
        case PUTBIT_CARRIER_UP:
            break;
        case PUTBIT_CARRIER_DOWN:
            if (s->rx_signal_present)
            {
                s->rx_data[s->rx_data_bytes++] = DLE;
                s->rx_data[s->rx_data_bytes++] = ETX;
                s->at_tx_handler(s, s->at_tx_user_data, s->rx_data, s->rx_data_bytes);
                s->rx_data_bytes = 0;
                at_put_response_code(s, RESPONSE_CODE_NO_CARRIER);
                s->at_rx_mode = AT_MODE_COMMAND;
            }
            s->rx_signal_present = FALSE;
            break;
        default:
            if (s->p.result_code_format)
                fprintf(stderr, "Eh!\n");
            break;
        }
        return;
    }
    s->current_byte = (s->current_byte >> 1) | (bit << 7);
    if (++s->bit_no >= 8)
    {
        if (s->current_byte == DLE)
            s->rx_data[s->rx_data_bytes++] = s->current_byte;
        s->rx_data[s->rx_data_bytes++] = s->current_byte;
        if (s->rx_data_bytes >= 250)
        {
            s->at_tx_handler(s, s->at_tx_user_data, s->rx_data, s->rx_data_bytes);
            s->rx_data_bytes = 0;
        }
        s->bit_no = 0;
        s->current_byte = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static int fast_getbit(void *user_data)
{
    t31_state_t *s;
    int bit;

    s = (t31_state_t *) user_data;
    if (s->bit_no <= 0)
    {
        if (s->tx_data_bytes < s->tx_in_bytes)
        {
            s->current_byte = s->tx_data[s->tx_data_bytes++];
        }
        else
        {
            if (s->data_final)
            {
                s->data_final = FALSE;
                /* This will put the modem into its shutdown sequence. When
                   it has finally shut down, an OK response will be sent. */
                return 3;
            }
            /* Fill with 0xFF bytes. This is appropriate at the start of transmission,
               as per T.30. If it happens in the middle of data, it is bad. What else
               can be done, though? */
            s->current_byte = 0xFF;
        }
        s->bit_no = 8;
    }
    s->bit_no--;
    bit = s->current_byte & 1;
    s->current_byte >>= 1;
    return bit;
}
/*- End of function --------------------------------------------------------*/

static void hdlc_tx_underflow(void *user_data)
{
    t31_state_t *s;

    s = (t31_state_t *) user_data;
    if (s->hdlc_final)
    {
        at_put_response_code(s, RESPONSE_CODE_OK);
        s->at_rx_mode = AT_MODE_COMMAND;
        s->hdlc_final = FALSE;
    }
    else
    {
        at_put_response_code(s, RESPONSE_CODE_CONNECT);
    }
}
/*- End of function --------------------------------------------------------*/

static void hdlc_accept(void *user_data, int ok, const uint8_t *msg, int len)
{
    char buf[256];
    t31_state_t *s;
    int i;

    s = (t31_state_t *) user_data;
    if (len < 0)
    {
        /* Special conditions */
        switch (len)
        {
        case PUTBIT_CARRIER_UP:
            s->rx_signal_present = TRUE;
            s->rx_message_received = FALSE;
            break;
        case PUTBIT_CARRIER_DOWN:
            if (s->rx_message_received)
            {
                if (s->dte_is_waiting)
                {
                    at_put_response_code(s, RESPONSE_CODE_NO_CARRIER);
                }
                else
                {
                    buf[0] = RESPONSE_CODE_NO_CARRIER;
                    queue_write_msg(&(s->rx_queue), buf, 1);
                }
            }
            s->rx_signal_present = FALSE;
            break;
        case PUTBIT_FRAMING_OK:
            if (s->modem == T31_CNG_TONE)
            {
                /* Once we get any valid HDLC the CNG tone stops, and we drop
                   to the V.21 receive modem on its own. */
                s->modem = T31_V21_RX;
                s->transmit = FALSE;
            }
            break;
        default:
            fprintf(stderr, "Unexpected HDLC special length - %d!\n", len);
            break;
        }
        return;
    }
    
    s->rx_message_received = TRUE;
    if (s->dte_is_waiting)
    {
        at_put_response_code(s, RESPONSE_CODE_CONNECT);
        /* Send straight away */
        for (i = 0;  i < len;  i++)
        {
            if (msg[i] == DLE)
                s->rx_data[s->rx_data_bytes++] = DLE;
            s->rx_data[s->rx_data_bytes++] = msg[i];
        }
        /* Fake CRC */
        /* Is there any point consuming CPU cycles calculating a real CRC? Does anything care? */
        s->rx_data[s->rx_data_bytes++] = 0;
        s->rx_data[s->rx_data_bytes++] = 0;
        s->rx_data[s->rx_data_bytes++] = DLE;
        s->rx_data[s->rx_data_bytes++] = ETX;
        s->at_tx_handler(s, s->at_tx_user_data, s->rx_data, s->rx_data_bytes);
        s->rx_data_bytes = 0;
        at_put_response_code(s, (ok)  ?  RESPONSE_CODE_OK  :  RESPONSE_CODE_FCERROR);
        s->dte_is_waiting = FALSE;
        s->at_rx_mode = AT_MODE_COMMAND;
    }
    else
    {
        /* Queue it */
        buf[0] = (ok)  ?  RESPONSE_CODE_OK  :  RESPONSE_CODE_FCERROR;
        memcpy(buf + 1, msg, len);
        queue_write_msg(&(s->rx_queue), buf, len + 1);
    }
}
/*- End of function --------------------------------------------------------*/

static int restart_modem(t31_state_t *s, int new_modem)
{
    tone_gen_descriptor_t tone_desc;

printf("Restart modem %d\n", new_modem);
    if (s->modem == new_modem)
        return 0;
    queue_flush(&(s->rx_queue));
    s->modem = new_modem;
    s->data_final = FALSE;
    switch (s->modem)
    {
    case T31_CED_TONE:
        s->silent_samples += SAMPLE_RATE*0.2;
        make_tone_gen_descriptor(&tone_desc,
                                 2100,
                                 -11,
                                 0,
                                 0,
                                 2600,
                                 75,
                                 0,
                                 0,
                                 FALSE);
        tone_gen_init(&(s->tone_gen), &tone_desc);
        s->transmit = TRUE;
        break;
    case T31_CNG_TONE:
        /* CNG is special, since we need to receive V.21 HDLC messages while sending the
           tone. Everything else in FAX processing sends only one way at a time. */
        /* 0.5s of 1100Hz + 3.0s of silence repeating */
        make_tone_gen_descriptor(&tone_desc,
                                 1100,
                                 -11,
                                 0,
                                 0,
                                 500,
                                 3000,
                                 0,
                                 0,
                                 TRUE);
        tone_gen_init(&(s->tone_gen), &tone_desc);
        /* Do V.21/HDLC receive in parallel. The other end may send its
           first message at any time. The CNG tone will continue until
           we get a valid preamble. */
        hdlc_rx_init(&(s->hdlcrx), FALSE, hdlc_accept, s);
        hdlc_rx_bad_frame_control(&(s->hdlcrx), TRUE);
        s->hdlc_final = FALSE;
        s->hdlc_len = 0;
        s->dled = FALSE;
        fsk_rx_init(&(s->v21rx), &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) hdlc_rx_bit, &(s->hdlcrx));
        s->transmit = TRUE;
        break;
    case T31_V21_TX:
        hdlc_tx_init(&(s->hdlctx), FALSE, hdlc_tx_underflow, s);
        hdlc_tx_preamble(&(s->hdlctx), 40);
        s->hdlc_final = FALSE;
        s->hdlc_len = 0;
        s->dled = FALSE;
        fsk_tx_init(&(s->v21tx), &preset_fsk_specs[FSK_V21CH2], (get_bit_func_t) hdlc_tx_getbit, &(s->hdlctx));
        s->transmit = TRUE;
        break;
    case T31_V21_RX:
        hdlc_rx_init(&(s->hdlcrx), FALSE, hdlc_accept, s);
        hdlc_rx_bad_frame_control(&(s->hdlcrx), TRUE);
        s->hdlc_final = FALSE;
        s->hdlc_len = 0;
        s->dled = FALSE;
        fsk_rx_init(&(s->v21rx), &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) hdlc_rx_bit, &(s->hdlcrx));
        s->transmit = FALSE;
        break;
#if defined(ENABLE_V17)
    case T31_V17_TX:
        v17_tx_restart(&(s->v17tx), s->bit_rate, FALSE, s->short_train);
        s->tx_data_bytes = 0;
        s->transmit = TRUE;
        break;
    case T31_V17_RX:
        v17_rx_restart(&(s->v17rx), s->bit_rate, s->short_train);
        s->transmit = FALSE;
        break;
#endif
    case T31_V27TER_TX:
        v27ter_tx_restart(&(s->v27ter_tx), FALSE, s->bit_rate);
        s->tx_data_bytes = 0;
        s->transmit = TRUE;
        break;
    case T31_V27TER_RX:
        v27ter_rx_restart(&(s->v27ter_rx), s->bit_rate);
        s->transmit = FALSE;
        break;
    case T31_V29_TX:
        v29_tx_restart(&(s->v29tx), FALSE, s->bit_rate);
        s->tx_data_bytes = 0;
        s->transmit = TRUE;
        break;
    case T31_V29_RX:
        v29_rx_restart(&(s->v29rx), s->bit_rate);
        s->transmit = FALSE;
        break;
    }
    s->bit_no = 0;
    s->current_byte = 0xFF;
    s->tx_in_bytes = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void dle_unstuff_hdlc(t31_state_t *s, const char *stuffed, int len)
{
    int i;

    for (i = 0;  i < len;  i++)
    {
        if (s->dled)
        {
            s->dled = FALSE;
            if (stuffed[i] == ETX)
            {
                hdlc_tx_preamble(&(s->hdlctx), 2);
                hdlc_tx_frame(&(s->hdlctx), s->hdlc_buf, s->hdlc_len);
                hdlc_tx_preamble(&(s->hdlctx), 2);
                s->hdlc_final = (s->hdlc_buf[1] & 0x10);
                s->hdlc_len = 0;
            }
            else if (stuffed[i] == SUB)
            {
                s->hdlc_buf[s->hdlc_len++] = DLE;
                s->hdlc_buf[s->hdlc_len++] = DLE;
            }
            else
            {
                s->hdlc_buf[s->hdlc_len++] = stuffed[i];
            }
        }
        else
        {
            if (stuffed[i] == DLE)
                s->dled = TRUE;
            else
                s->hdlc_buf[s->hdlc_len++] = stuffed[i];
        }
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ void dle_unstuff(t31_state_t *s, const char *stuffed, int len)
{
    int i;
    
    for (i = 0;  i < len;  i++)
    {
        if (s->dled)
        {
            s->dled = FALSE;
            if (stuffed[i] == ETX)
            {
                fprintf(stderr, "%d byte data\n", s->tx_in_bytes);
                s->data_final = TRUE;
                s->at_rx_mode = AT_MODE_COMMAND;
                return;
            }
            s->tx_data[s->tx_in_bytes++] = stuffed[i];
        }
        else
        {
            if (stuffed[i] == DLE)
                s->dled = TRUE;
            else
                s->tx_data[s->tx_in_bytes++] = stuffed[i];
        }
    }
}
/*- End of function --------------------------------------------------------*/

static int parse_num(const char **s, int max_value)
{
    int i;
    
    /* The spec. says no digits is valid, and should be treated as zero. */
    i = 0;
    while (isdigit(**s))
    {
        i = i*10 + ((**s) - '0');
        (*s)++;
    }
    if (i > max_value)
        i = -1;
    return i;
}
/*- End of function --------------------------------------------------------*/

static int parse_hex_num(const char **s, int max_value)
{
    int i;
    
    /* The spec. says a hex value is always 2 digits, and the alpha digits are
       upper case. */
    i = 0;
    if (isdigit(**s))
        i = **s - '0';
    else if (**s >= 'A'  &&  **s <= 'F')
        i = **s - 'A';
    else
        return -1;
    *s++;

    if (isdigit(**s))
        i = (i << 4)  | (**s - '0');
    else if (**s >= 'A'  &&  **s <= 'F')
        i = (i << 4)  | (**s - 'A');
    else
        return -1;
    *s++;
    if (i > max_value)
        i = -1;
    return i;
}
/*- End of function --------------------------------------------------------*/

static int parse_out(t31_state_t *s, const char **t, int *target, int max_value, const char *prefix, const char *def)
{
    char buf[100];
    int val;

    switch (*(*t)++)
    {
    case '=':
        switch (**t)
        {
        case '?':
            /* Show possible values */
            (*t)++;
            snprintf(buf, sizeof(buf), "%s%s", (prefix)  ?  prefix  :  "", def);
            at_put_response(s, buf);
            break;
        default:
            /* Set value */
            if ((val = parse_num(t, max_value)) < 0)
                return FALSE;
            if (target)
                *target = val;
            break;
        }
        break;
    case '?':
printf("XXXX\n");
        /* Show current value */
        val = (target)  ?  *target  :  0;
        snprintf(buf, sizeof(buf), "%s%d", (prefix)  ?  prefix  :  "", val);
        at_put_response(s, buf);
printf("YYYY\n");
        break;
    default:
        return FALSE;
    }
    return TRUE;
}
/*- End of function --------------------------------------------------------*/

static int parse_2_out(t31_state_t *s, const char **t, int *target1, int max_value1, int *target2, int max_value2, const char *prefix, const char *def)
{
    char buf[100];
    int val1;
    int val2;

    switch (*(*t)++)
    {
    case '=':
        switch (**t)
        {
        case '?':
            /* Show possible values */
            (*t)++;
            snprintf(buf, sizeof(buf), "%s%s", (prefix)  ?  prefix  :  "", def);
            at_put_response(s, buf);
            break;
        default:
            /* Set value */
            if ((val1 = parse_num(t, max_value1)) < 0)
                return FALSE;
            if (target1)
                *target1 = val1;
            if (**t == ',')
            {
                *t++;
                if ((val2 = parse_num(t, max_value2)) < 0)
                    return FALSE;
                if (target2)
                    *target2 = val2;
            }
            break;
        }
        break;
    case '?':
        /* Show current value */
        val1 = (target1)  ?  *target1  :  0;
        val2 = (target2)  ?  *target2  :  0;
        snprintf(buf, sizeof(buf), "%s%d,%d", (prefix)  ?  prefix  :  "", val1, val2);
        at_put_response(s, buf);
        break;
    default:
        return FALSE;
    }
    return TRUE;
}
/*- End of function --------------------------------------------------------*/

static int parse_hex_out(t31_state_t *s, const char **t, int *target, int max_value, const char *prefix, const char *def)
{
    char buf[100];
    int val;

    switch (*(*t)++)
    {
    case '=':
        switch (**t)
        {
        case '?':
            /* Show possible values */
            (*t)++;
            snprintf(buf, sizeof(buf), "%s%s", (prefix)  ?  prefix  :  "", def);
            at_put_response(s, buf);
            break;
        default:
            /* Set value */
            if ((val = parse_hex_num(t, max_value)) < 0)
                return FALSE;
            if (target)
                *target = val;
            break;
        }
        break;
    case '?':
        /* Show current value */
        val = (target)  ?  *target  :  0;
        snprintf(buf, sizeof(buf), "%s%02X", (prefix)  ?  prefix  :  "", val);
        at_put_response(s, buf);
        break;
    default:
        return FALSE;
    }
    return TRUE;
}
/*- End of function --------------------------------------------------------*/

static int match_element(const char **variant, const char *variants)
{
    int element;
    int i;
    int len;
    char const *s;
    char const *t;

    s = variants;
    for (i = 1;  *s;  i++)
    {
        if ((t = strchr(s, ',')))
            len = t - s;
        else
            len = strlen(s);
        if (memcmp(*variant, s, len) == 0)
        {
            *variant += len;
            return  i;
        }
        s += len;
        if (*s == ',')
            s++;
    }
    return  -1;
}
/*- End of function --------------------------------------------------------*/

static int parse_string_out(t31_state_t *s, const char **t, int *target, int max_value, const char *prefix, const char *def)
{
    char buf[100];
    int val;

    switch (*(*t)++)
    {
    case '=':
        switch (**t)
        {
        case '?':
            /* Show possible values */
            (*t)++;
            snprintf(buf, sizeof(buf), "%s%s", (prefix)  ?  prefix  :  "", def);
            at_put_response(s, buf);
            break;
        default:
            /* Set value */
            if ((val = match_element(t, def)) < 0)
                return FALSE;
            if (target)
                *target = val;
            break;
        }
        break;
    case '?':
        /* Show current value */
        val = (target)  ?  *target  :  0;
        snprintf(buf, sizeof(buf), "%s%d", (prefix)  ?  prefix  :  "", val);
        at_put_response(s, buf);
        break;
    default:
        return FALSE;
    }
    return TRUE;
}
/*- End of function --------------------------------------------------------*/

const char *s_reg_handler(t31_state_t *s, const char *t, int reg)
{
    int val;
    uint8_t byte_val;
    int b;
    char buf[4];

    /* Set or get an S register */
    switch (*t++)
    {
    case '=':
        switch (*t)
        {
        case '?':
            t++;
            snprintf(buf, sizeof(buf), "%3.3d", 0);
            at_put_response(s, buf);
            break;
        default:
            if ((val = parse_num(&t, 255)) < 0)
                return NULL;
            s->p.s_regs[reg] = val;
            break;
        }
        break;
    case '?':
        snprintf(buf, sizeof(buf), "%3.3d", s->p.s_regs[reg]);
        at_put_response(s, buf);
        break;
    case '.':
        if ((b = parse_num(&t, 7)) < 0)
            return NULL;
        switch (*t++)
        {
        case '=':
            switch (*t)
            {
            case '?':
                t++;
                at_put_numeric_response(s, 0);
                break;
            default:
                if ((val = parse_num(&t, 1)) < 0)
                    return NULL;
                if (val)
                    s->p.s_regs[reg] |= (1 << b);
                else
                    s->p.s_regs[reg] &= ~(1 << b);
                break;
            }
            break;
        case '?':
            at_put_numeric_response(s, (unsigned int) ((s->p.s_regs[reg] >> b) & 1));
            break;
        default:
            return NULL;
        }
        break;
    default:
        return NULL;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static int process_class1_cmd(t31_state_t *s, const char **t)
{
    int val;
    int operation;
    int new_modem;
    int new_transmit;
    int i;
    int len;
    char *allowed;
    uint8_t msg[256];

    new_transmit = (*(*t + 2) == 'T');
    operation = *(*t + 3);
    /* Step past the "+Fxx" */
    *t += 4;
    switch (operation)
    {
    case 'S':
        allowed = "0-255";
        break;
    case 'H':
        allowed = "3";
        break;
    default:
#if defined(ENABLE_V17)
        allowed = "24,48,72,96";
#else
        allowed = "24,48,72,73,74,96,97,98,121,122,145,146";
#endif
        break;
    }
    
    val = -1;
    if (!parse_out(s, t, &val, 255, NULL, allowed))
        return TRUE;
    if (val < 0)
    {
        /* It was just a query */
        return  TRUE;
    }
    /* All class 1 FAX commands are supposed to give an ERROR response, if the phone
       is on-hook. */
    switch (operation)
    {
    case 'S':
        s->transmit = new_transmit;
        s->modem = T31_SILENCE_TX;
        if (new_transmit)
        {
            /* Send some silence to space transmissions. */
            s->silent_samples += val*80;
        }
        else
        {
            /* Wait until we have received a specified period of silence. */
            queue_flush(&(s->rx_queue));
            /* TODO: wait */
        }
        /* If this is the last thing on the command line, inhibit an immediate response */
        if (*t == '\0')
            *t = (const char *) -1;
printf("Silence %dms\n", val*10);
        break;
    case 'H':
        switch (val)
        {
        case 3:
            new_modem = (new_transmit)  ?  T31_V21_TX  :  T31_V21_RX;
            s->short_train = FALSE;
            s->bit_rate = 300;
            break;
        default:
            return FALSE;
        }
printf("HDLC\n");
        if (new_modem != s->modem)
        {
            restart_modem(s, new_modem);
            *t = (const char *) -1;
        }
        s->transmit = new_transmit;
        if (new_transmit)
        {
            at_put_response_code(s, RESPONSE_CODE_CONNECT);
            s->at_rx_mode = AT_MODE_HDLC;
        }
        else
        {
            /* Send straight away, if there is something queued. */
            do
            {
                if (!queue_empty(&(s->rx_queue)))
                {
                    len = queue_read_msg(&(s->rx_queue), msg, 256);
                    if (len > 1)
                    {
                        if (msg[0] == RESPONSE_CODE_OK)
                            at_put_response_code(s, RESPONSE_CODE_CONNECT);
                        for (i = 1;  i < len;  i++)
                        {
                            if (msg[i] == DLE)
                                s->rx_data[s->rx_data_bytes++] = DLE;
                            s->rx_data[s->rx_data_bytes++] = msg[i];
                        }
                        /* Fake CRC */
                        s->rx_data[s->rx_data_bytes++] = 0;
                        s->rx_data[s->rx_data_bytes++] = 0;
                        s->rx_data[s->rx_data_bytes++] = DLE;
                        s->rx_data[s->rx_data_bytes++] = ETX;
                        s->at_tx_handler(s, s->at_tx_user_data, s->rx_data, s->rx_data_bytes);
                        s->rx_data_bytes = 0;
                    }
                    at_put_response_code(s, msg[0]);
                }
                else
                {
                    s->dte_is_waiting = TRUE;
                    break;
                }
            }
            while (msg[0] == RESPONSE_CODE_CONNECT);
        }
        *t = (const char *) -1;
        break;
    default:
        switch (val)
        {
        case 24:
            new_modem = (new_transmit)  ?  T31_V27TER_TX  :  T31_V27TER_RX;
            s->short_train = FALSE;
            s->bit_rate = 2400;
            break;
        case 48:
            new_modem = (new_transmit)  ?  T31_V27TER_TX  :  T31_V27TER_RX;
            s->short_train = FALSE;
            s->bit_rate = 4800;
            break;
        case 72:
            new_modem = (new_transmit)  ?  T31_V29_TX  :  T31_V29_RX;
            s->short_train = FALSE;
            s->bit_rate = 7200;
            break;
        case 96:
            new_modem = (new_transmit)  ?  T31_V29_TX  :  T31_V29_RX;
            s->short_train = FALSE;
            s->bit_rate = 9600;
            break;
#if defined(ENABLE_V17)
        case 73:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
            s->short_train = FALSE;
            s->bit_rate = 7200;
            break;
        case 74:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
            s->short_train = TRUE;
            s->bit_rate = 7200;
            break;
        case 97:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
            s->short_train = FALSE;
            s->bit_rate = 9600;
            break;
        case 98:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
            s->short_train = TRUE;
            s->bit_rate = 9600;
            break;
        case 121:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
            s->short_train = FALSE;
            s->bit_rate = 12000;
            break;
        case 122:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
            s->short_train = TRUE;
            s->bit_rate = 12000;
            break;
        case 145:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
            s->short_train = FALSE;
            s->bit_rate = 14400;
            break;
        case 146:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
            s->short_train = TRUE;
            s->bit_rate = 14400;
            break;
#endif
        default:
            return FALSE;
        }
        fprintf(stderr, "Short training = %d, bit rate = %d\n", s->short_train, s->bit_rate);
        if (new_transmit)
        {
            at_put_response_code(s, RESPONSE_CODE_CONNECT);
            s->at_rx_mode = AT_MODE_STUFFED;
        }
        restart_modem(s, new_modem);
        *t = (const char *) -1;
        break;
    }
    return TRUE;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_dummy(t31_state_t *s, const char *t)
{
    /* Dummy routine to absorb delimiting characters from a command string */
    return t + 1;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_A(t31_state_t *s, const char *t)
{
    tone_gen_descriptor_t tone_desc;
    int ok;

    /* V.250 6.3.5 - Answer (abortable) */ 
    t += 1;
    if ((ok = s->call_control_handler(s, s->call_control_user_data, "")) < 0)
    {
        at_put_response_code(s, RESPONSE_CODE_ERROR);
        return NULL;
    }
    /* Answering should now be in progress. No AT response should be
       issued at this point. */
    return (const char *) -1;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_D(t31_state_t *s, const char *t)
{
    int ok;
    char *u;
    const char *w;
    char num[100 + 1];
    char ch;

    /* V.250 6.3.1 - Dial (abortable) */ 
    t += 1;
    ok = FALSE;
    /* There are a numbers of options in a dial command string.
       Many are completely irrelevant in this application. */
    w = t;
    u = num;
    for (  ;  (ch = *t);  t++)
    {
        if (isdigit(ch))
        {
            /* V.250 6.3.1.1 Basic digit set */
            *u++ = ch;
        }
        else
        {
            switch (ch)
            {
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case '*':
            case '#':
                /* V.250 6.3.1.1 Full DTMF repertoire */
                if (!s->p.pulse_dial)
                    *u++ = ch;
                break;
            case '+':
                /* V.250 6.3.1.1 International access code */
                /* TODO: */
                break;
            case ',':
                /* V.250 6.3.1.2 Pause */
                /* TODO: */
                break;
            case 'T':
                /* V.250 6.3.1.3 Tone dial */
                s->p.pulse_dial = FALSE;
                break;
            case 'P':
                /* V.250 6.3.1.4 Pulse dial */
                s->p.pulse_dial = TRUE;
                break;
            case '!':
                /* V.250 6.3.1.5 Hook flash, register recall */
                /* TODO: */
                break;
            case 'W':
                /* V.250 6.3.1.6 Wait for dial tone */
                /* TODO: */
                break;
            case '@':
                /* V.250 6.3.1.7 Wait for quiet answer */
                /* TODO: */
                break;
            case 'S':
                /* V.250 6.3.1.8 Invoke stored string */
                /* TODO: */
                break;
            default:
                return NULL;
            }
        }
    }
    *u = '\0';
    if ((ok = s->call_control_handler(s, s->call_control_user_data, num)) < 0)
        return NULL;
    /* Dialing should now be in progress. No AT response should be
       issued at this point. */
    return (const char *) -1;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_E(t31_state_t *s, const char *t)
{
    int val;

    /* V.250 6.2.4 - Command echo */ 
    t += 1;
    if ((val = parse_num(&t, 1)) < 0)
        return NULL;
    s->p.echo = val;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_H(t31_state_t *s, const char *t)
{
    int val;

    /* V.250 6.3.6 - Hook control */ 
    t += 1;
    if ((val = parse_num(&t, 0)) < 0)
        return NULL;
    s->call_control_handler(s, s->call_control_user_data, NULL);
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_I(t31_state_t *s, const char *t)
{
    int val;
    char buf[132 + 1];

    /* V.250 6.1.3 - Request identification information */ 
    /* N.B. The information supplied in response to an ATIx command is very
       variable. It was widely used in different ways before the AT command
       set was standardised by the ITU. */
    t += 1;
    switch (val = parse_num(&t, 255))
    {
    case 0:
        at_put_response(s, model);
        break;
    case 3:
        at_put_response(s, manufacturer);
        break;
    case 8:
        sprintf(buf, "NMBR = %s", (s->originating_number)  ?  s->originating_number  :  "");
        at_put_response(s, buf);
        break;
    case 9:
        sprintf(buf, "NDID = %s", (s->destination_number)  ?  s->destination_number  :  "");
        at_put_response(s, buf);
        break;
    default:
        return NULL;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_L(t31_state_t *s, const char *t)
{
    int val;

    /* V.250 6.3.13 - Monitor speaker loudness */
    /* Just absorb this command, as we have no speaker */
    t += 1;
    if ((val = parse_num(&t, 255)) < 0)
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_M(t31_state_t *s, const char *t)
{
    int val;

    /* V.250 6.3.14 - Monitor speaker mode */ 
    /* Just absorb this command, as we have no speaker */
    t += 1;
    if ((val = parse_num(&t, 255)) < 0)
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_O(t31_state_t *s, const char *t)
{
    int val;

    /* V.250 6.3.7 - Return to online data state */ 
    t += 1;
    if ((val = parse_num(&t, 1)) < 0)
        return NULL;
    if (val == 0)
    {
        s->at_rx_mode = AT_MODE_CONNECTED;
        at_put_response_code(s, RESPONSE_CODE_CONNECT);
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_P(t31_state_t *s, const char *t)
{
    /* V.250 6.3.3 - Select pulse dialling (command) */ 
    t += 1;
    s->p.pulse_dial = TRUE;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_Q(t31_state_t *s, const char *t)
{
    int val;

    /* V.250 6.2.5 - Result code suppression */ 
    t += 1;
    switch (val = parse_num(&t, 1))
    {
    case 0:
        s->p.result_code_format = (s->p.verbose)  ?  ASCII_RESULT_CODES  :  NUMERIC_RESULT_CODES;
        break;
    case 1:
        s->p.result_code_format = NO_RESULT_CODES;
        break;
    default:
        return NULL;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S0(t31_state_t *s, const char *t)
{
    /* V.250 6.3.8 - Automatic answer */ 
    t += 2;
    return s_reg_handler(s, t, 0);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S10(t31_state_t *s, const char *t)
{
    /* V.250 6.3.12 - Automatic disconnect delay */ 
    t += 3;
    return s_reg_handler(s, t, 10);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S3(t31_state_t *s, const char *t)
{
    /* V.250 6.2.1 - Command line termination character */ 
    t += 2;
    return s_reg_handler(s, t, 3);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S4(t31_state_t *s, const char *t)
{
    /* V.250 6.2.2 - Response formatting character */ 
    t += 2;
    return s_reg_handler(s, t, 4);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S5(t31_state_t *s, const char *t)
{
    /* V.250 6.2.3 - Command line editing character */ 
    t += 2;
    return s_reg_handler(s, t, 5);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S6(t31_state_t *s, const char *t)
{
    /* V.250 6.3.9 - Pause before blind dialling */ 
    t += 2;
    return s_reg_handler(s, t, 6);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S7(t31_state_t *s, const char *t)
{
    /* V.250 6.3.10 - Connection completion timeout */ 
    t += 2;
    return s_reg_handler(s, t, 7);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_S8(t31_state_t *s, const char *t)
{
    /* V.250 6.3.11 - Comma dial modifier time */ 
    t += 2;
    return s_reg_handler(s, t, 8);
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_T(t31_state_t *s, const char *t)
{
    /* V.250 6.3.2 - Select tone dialling (command) */ 
    t += 1;
    s->p.pulse_dial = FALSE;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_V(t31_state_t *s, const char *t)
{
    int val;

    /* V.250 6.2.6 - DCE response format */ 
    t += 1;
    switch (val = parse_num(&t, 1))
    {
    case 0:
        s->p.verbose = FALSE;
        if (s->p.result_code_format != NO_RESULT_CODES)
            s->p.result_code_format = (s->p.verbose)  ?  ASCII_RESULT_CODES  :  NUMERIC_RESULT_CODES;
        break;
    case 1:
        s->p.verbose = TRUE;
        if (s->p.result_code_format != NO_RESULT_CODES)
            s->p.result_code_format = (s->p.verbose)  ?  ASCII_RESULT_CODES  :  NUMERIC_RESULT_CODES;
        break;
    default:
        return NULL;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_X(t31_state_t *s, const char *t)
{
    int val;

    /* V.250 6.2.7 - Result code selection and call progress monitoring control */
    /* TODO: */
    t += 1;
    switch (val = parse_num(&t, 4))
    {
    case 0:
        /* CONNECT result code is given upon entering online data state.
           Dial tone and busy detection are disabled. */
        break;
    case 1:
        /* CONNECT <text> result code is given upon entering online data state.
           Dial tone and busy detection are disabled. */
        break;
    case 2:
        /* CONNECT <text> result code is given upon entering online data state.
           Dial tone detection is enabled, and busy detection is disabled. */
        break;
    case 3:
        /* CONNECT <text> result code is given upon entering online data state.
           Dial tone detection is disabled, and busy detection is enabled. */
        break;
    case 4:
        /* CONNECT <text> result code is given upon entering online data state.
           Dial tone and busy detection are both enabled. */
        break;
    default:
        return NULL;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_Z(t31_state_t *s, const char *t)
{
    int val;

    /* V.250 6.1.1 - Reset to default configuration */ 
    t += 1;
    if ((val = parse_num(&t, sizeof(profiles)/sizeof(profiles[0]) - 1)) < 0)
        return NULL;
    /* Just make sure we are on hook */
    s->call_control_handler(s, s->call_control_user_data, NULL);
    s->p = profiles[val];
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_amp_C(t31_state_t *s, const char *t)
{
    int val;

    /* V.250 6.2.8 - Circuit 109 (received line signal detector), behaviour */ 
    /* We have no RLSD pin, so just absorb this. */
    t += 2;
    if ((val = parse_num(&t, 1)) < 0)
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_amp_D(t31_state_t *s, const char *t)
{
    int val;

    /* V.250 6.2.9 - Circuit 108 (data terminal ready) behaviour */ 
    t += 2;
    if ((val = parse_num(&t, 2)) < 0)
        return NULL;
    /* TODO: We have no DTR pin, but we need this to get into online
             command state. */
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_amp_F(t31_state_t *s, const char *t)
{
    t += 2;

    /* V.250 6.1.2 - Set to factory-defined configuration */ 
    /* Just make sure we are on hook */
    s->call_control_handler(s, s->call_control_user_data, NULL);
    s->p = profiles[0];
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_A8E(t31_state_t *s, const char *t)
{
    int val;

    /* V.251 5.1 - V.8 and V.8bis operation controls */
    /* Syntax: +A8E=<v8o>,<v8a>,<v8cf>[,<v8b>][,<cfrange>][,<protrange>] */
    /* <v8o>=0  Disable V.8 origination negotiation
       <v8o>=1  Enable DCE-controlled V.8 origination negotiation
       <v8o>=2  Enable DTE-controlled V.8 origination negotiation, send V.8 CI only
       <v8o>=3  Enable DTE-controlled V.8 origination negotiation, send 1100Hz CNG only
       <v8o>=4  Enable DTE-controlled V.8 origination negotiation, send 1300Hz CT only
       <v8o>=5  Enable DTE-controlled V.8 origination negotiation, send no tones
       <v8o>=6  Enable DCE-controlled V.8 origination negotiation, issue +A8x indications
       <v8a>=0  Disable V.8 answer negotiation
       <v8a>=1  Enable DCE-controlled V.8 answer negotiation
       <v8a>=2  Enable DTE-controlled V.8 answer negotiation, send ANSam
       <v8a>=3  Enable DTE-controlled V.8 answer negotiation, send no signal
       <v8a>=4  Disable DTE-controlled V.8 answer negotiation, send ANS
       <v8a>=5  Enable DCE-controlled V.8 answer negotiation, issue +A8x indications
       <v8cf>=X..Y	Set the V.8 CI signal call function to the hexadecimal octet value X..Y
       <v8b>=0	Disable V.8bis negotiation
       <v8b>=1  Enable DCE-controlled V.8bis negotiation
       <v8b>=2  Enable DTE-controlled V.8bis negotiation
       <cfrange>="<string of values>"   Set to alternative list of call function "option bit"
                                        values that the answering DCE shall accept from the caller
       <protrange>="<string of values>" Set to alternative list of protocol "option bit" values that
                                        the answering DCE shall accept from the caller
    */
    /* TODO: */
    t += 4;
    if (!parse_out(s, &t, &val, 6, "+A8E:", "(0-6),(0-5),(00-FF)"))
        return NULL;
    if (*t != ',')
        return t;
    if ((val = parse_num(&t, 5)) < 0)
        return NULL;
    if (*t != ',')
        return t;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_A8M(t31_state_t *s, const char *t)
{
    /* V.251 5.2 - Send V.8 menu signals */
    /* Syntax: +A8M=<hexadecimal coded CM or JM octet string>  */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_A8T(t31_state_t *s, const char *t)
{
    int val;

    /* V.251 5.3 - Send V.8bis signal and/or message(s) */
    /* Syntax: +A8T=<signal>[,<1st message>][,<2nd message>][,<sig_en>][,<msg_en>][,<supp_delay>] */
    /*  <signal>=0  None
        <signal>=1  Initiating Mre
        <signal>=2  Initiating MRd
        <signal>=3  Initiating CRe, low power
        <signal>=4  Initiating CRe, high power
        <signal>=5  Initiating CRd
        <signal>=6  Initiating Esi
        <signal>=7  Responding MRd, low power
        <signal>=8  Responding MRd, high power
        <signal>=9  Responding CRd
        <signal>=10 Responding Esr
    */
    /* TODO: */
    t += 4;
    if (!parse_out(s, &t, &val, 10, "+A8T:", "(0-10)"))
        return NULL;
    if (*t != ',')
        return t;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ASTO(t31_state_t *s, const char *t)
{
    /* V.250 6.3.15 - Store telephone number */ 
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAAP(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.25 - Automatic answer for eMLPP Service */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CACM(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.25 - Accumulated call meter */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CACSP(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.7 - Voice Group or Voice Broadcast Call State Attribute Presentation */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAEMLPP(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.22 - eMLPP Priority Registration and Interrogation */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAHLD(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.3 - Leave an ongoing Voice Group or Voice Broadcast Call */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAJOIN(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.1 - Accept an incoming Voice Group or Voice Broadcast Call */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CALA(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.16 - Alarm */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CALCC(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.6 - List current Voice Group and Voice Broadcast Calls */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CALD(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.38 - Delete alar m */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CALM(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.20 - Alert sound mode */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAMM(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.26 - Accumulated call meter maximum */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CANCHEV(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.8 - NCH Support Indication */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAOC(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.16 - Advice of Charge */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAPD(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.39 - Postpone or dismiss an alarm */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAPTT(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.4 - Talker Access for Voice Group Call */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAREJ(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.2 - Reject an incoming Voice Group or Voice Broadcast Call */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CAULEV(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.5 - Voice Group Call Uplink Status Presentation */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CBC(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.4 - Battery charge */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CBCS(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.3.2 - VBS subscriptions and GId status */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CBST(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.7 - Select bearer service type */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CCFC(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.11 - Call forwarding number and conditions */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CCLK(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.15 - Clock */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CCUG(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.10 - Closed user group */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CCWA(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.12 - Call waiting */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CCWE(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.28 - Call Meter maximum event */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CDIP(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.9 - Called line identification presentation */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CDIS(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.8 - Display control */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CEER(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.10 - Extended error report */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CFCS(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.24 - Fast call setup conditions */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CFUN(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.2 - Set phone functionality */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGACT(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.10 - PDP context activate or deactivate */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGANS(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.16 - Manual response to a network request for PDP context activation */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGATT(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.9 - PS attach or detach */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGAUTO(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.15 - Automatic response to a network request for PDP context activation */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGCLASS(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.17 - GPRS mobile station class (GPRS only) */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGCLOSP(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.13 - Configure local Octet Stream PAD parameters (Obsolete) 
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGCLPAD(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.12 - Configure local triple-X PAD parameters (GPRS only) (Obsolete) */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGCMOD(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.11 - PDP Context Modify */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGCS(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.3.1 - VGCS subscriptions and GId status */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGDATA(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.12 - Enter data state */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGDCONT(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.1 - Define PDP Context */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGDSCONT(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.2 - Define Secondary PDP Context */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGEQMIN(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.7 - 3G Quality of Service Profile (Minimum acceptable) */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGEQNEG(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.8 - 3G Quality of Service Profile (Negotiated) */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGEQREQ(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.6 - 3G Quality of Service Profile (Requested) */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGEREP(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.18 - Packet Domain event reporting */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGMI(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.1 - Request manufacturer identification */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGMM(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.2 - Request model identification */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGMR(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.3 - Request revision identification */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGPADDR(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.14 - Show PDP address */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGQMIN(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.5 - Quality of Service Profile (Minimum acceptable) */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGQREQ(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.4 - Quality of Service Profile (Requested) */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGREG(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.19 - GPRS network registration status */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGSMS(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.20 - Select service for MO SMS messages */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGSN(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.4 - Request product serial number identification */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CGTFT(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 10.1.3 - Traffic Flow Template */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHLD(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.13 - Call related supplementary services */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHSA(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.18 - HSCSD non-transparent asymmetry configuration */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHSC(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.15 - HSCSD current call parameters */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHSD(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.12 - HSCSD device parameters */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHSN(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.14 - HSCSD non-transparent call configuration */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHSR(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.16 - HSCSD parameters report */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHST(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.13 - HSCSD transparent call configuration */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHSU(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.17 - HSCSD automatic user initiated upgrading */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CHUP(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.5 - Hangup call */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CIMI(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.6 - Request international mobile subscriber identity */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CIND(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.9 - Indicator control */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CKPD(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.7 - Keypad control */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLAC(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.37 - List all available AT commands */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLAE(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.31 - Language Event */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLAN(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.30 - Set Language */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLCC(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.18 - List current calls */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLCK(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.4 - Facility lock */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLIP(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.6 - Calling line identification presentation */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLIR(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.7 - Calling line identification restriction */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CLVL(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.23 - Loudspeaker volume level */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMAR(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.36 - Master Reset */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMEC(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.6 - Mobile Termination control mode */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMER(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.10 - Mobile Termination event reporting */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMOD(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.4 - Call mode */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMUT(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.24 - Mute control */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CMUX(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.7 - Multiplexing mode */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CNUM(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.1 - Subscriber number */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_COLP(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.8 - Connected line identification presentation */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_COPN(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.21 - Read operator names */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_COPS(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.3 - PLMN selection */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_COTDI(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 11.1.9 - Originator to Dispatcher Information */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPAS(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.1 - Phone activity status */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPBF(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.13 - Find phonebook entries */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPBR(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.12 - Read phonebook entries */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPBS(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.11 - Select phonebook memory storage */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPBW(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.14 - Write phonebook entry */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPIN(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.3 - Enter PIN */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPLS(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.20 - Selection of preferred PLMN list */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPOL(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.19 - Preferred PLMN list */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPPS(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.23 - eMLPP subscriptions */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPROT(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.42 - Enter protocol mode */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPUC(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.27 - Price per unit and currency table */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPWC(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.29 - Power class */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CPWD(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.5 - Change password */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CR(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.9 - Service reporting control */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CRC(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.11 - Cellular result codes */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CREG(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.2 - Network registration */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CRLP(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.8 - Radio link protocol */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CRMC(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.34 - Ring Melody Control */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CRMP(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.35 - Ring Melody Playback */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CRSL(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.21 - Ringer sound level */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CRSM(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.18 - Restricted SIM access */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSCC(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.19 - Secure control command */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSCS(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.5 - Select TE character set */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSDF(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.22 - Settings date format */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSGT(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.32 - Set Greeting Text */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSIL(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.23 - Silence Command */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSIM(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.17 - Generic SIM access */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSNS(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.19 - Single numbering scheme */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSQ(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.5 - Signal quality */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSSN(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.17 - Supplementary service notifications */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSTA(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.1 - Select type of address */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSTF(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.24 - Settings time format */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CSVM(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.33 - Set Voice Mail Number */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CTFR(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.14 - Call deflection */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CTZR(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.41 - Time Zone Reporting */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CTZU(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.40 - Automatic Time Zone Update */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CUSD(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.15 - Unstructured supplementary service data */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CUUS1(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 7.26 - User to User Signalling Service 1 */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CV120(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.21 - V.120 rate adaption protocol */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CVHU(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 6.20 - Voice Hangup Control */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_CVIB(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 8.22 - Vibrator mode */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_DR(t31_state_t *s, const char *t)
{
    /* V.250 6.6.2 - Data compression reporting */ 
    /* TODO: */
    t += 3;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_DS(t31_state_t *s, const char *t)
{
    /* V.250 6.6.1 - Data compression */ 
    /* TODO: */
    t += 3;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_EB(t31_state_t *s, const char *t)
{
    /* V.250 6.5.2 - Break handling in error control operation */ 
    /* TODO: */
    t += 3;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_EFCS(t31_state_t *s, const char *t)
{
    /* V.250 6.5.4 - 32-bit frame check sequence */ 
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 2, "+EFCS:", "(0-2)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_EFRAM(t31_state_t *s, const char *t)
{
    /* V.250 6.5.8 - Frame length */ 
    /* TODO: */
    t += 6;
    if (!parse_2_out(s, &t, NULL, 65535, NULL, 65535, "+EFRAM:", "(1-65535),(1-65535)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ER(t31_state_t *s, const char *t)
{
    /* V.250 6.5.5 - Error control reporting */ 
    /*  0   Error control reporting disabled (no +ER intermediate result code transmitted)
        1   Error control reporting enabled (+ER intermediate result code transmitted)
    /* TODO: */
    t += 3;
    if (!parse_out(s, &t, NULL, 1, "+ER:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ES(t31_state_t *s, const char *t)
{
    /* V.250 6.5.1 - Error control selection */ 
    /* TODO: */
    t += 3;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ESR(t31_state_t *s, const char *t)
{
    /* V.250 6.5.3 - Selective repeat */ 
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ETBM(t31_state_t *s, const char *t)
{
    /* V.250 6.5.6 - Call termination buffer management */ 
    /* TODO: */
    t += 5;
    if (!parse_2_out(s, &t, NULL, 2, NULL, 2, "+ETBM:", "(0-2),(0-2),(0-30)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_EWIND(t31_state_t *s, const char *t)
{
    /* V.250 6.5.7 - Window size */ 
    /* TODO: */
    t += 6;
    if (!parse_2_out(s, &t, NULL, 127, NULL, 127, "+EWIND:", "(1-127),(1-127)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FAR(t31_state_t *s, const char *t)
{
    /* T.31 8.5.1 - Adaptive reception control */ 
    /* TODO: */
    t += 4;
    if (!parse_out(s, &t, NULL, 1, NULL, "0"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FCL(t31_state_t *s, const char *t)
{
    int val;
    
    /* T.31 8.5.2 - Carrier loss timeout */ 
    t += 4;
    if ((val = parse_num(&t, 255)) < 0)
        return NULL;
    s->carrier_loss_timeout = val;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FCLASS(t31_state_t *s, const char *t)
{
    /* T.31 8.2 - Capabilities identification and control */ 
    t += 7;
    /* T.31 says the reply string should be "0,1.0", however making
       it "0,1,1.0" makes things compatible with a lot more software
       that may be expecting a pre-T.31 modem. */
    if (!parse_out(s, &t, &s->fclass_mode, 1, NULL, "0,1,1.0"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FDD(t31_state_t *s, const char *t)
{
    /* T.31 8.5.3 - Double escape character replacement */ 
    t += 4;
    if (!parse_out(s, &t, &s->p.double_escape, 1, NULL, "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FIT(t31_state_t *s, const char *t)
{
    int val1;
    int val2;
    
    /* T.31 8.5.4 - DTE inactivity timeout */ 
    t += 4;
    if ((val1 = parse_num(&t, 255)) < 0)
        return NULL;
    if (*t != ',')
        return NULL;
    t++;
    if ((val2 = parse_num(&t, 255)) < 0)
        return NULL;
    s->dte_inactivity_timeout = val1;
    s->dte_inactivity_action = val2;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FLO(t31_state_t *s, const char *t)
{
    /* T.31 Annex A */ 
    /* Implement something similar to the V.250 +IFC command */ 
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FPR(t31_state_t *s, const char *t)
{
    /* T.31 Annex A */ 
    /* Implement something similar to the V.250 +IPR command */ 
    t += 4;
    if (!parse_out(s, &t, &s->dte_rate, 115200, NULL, "115200"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FRH(t31_state_t *s, const char *t)
{
    /* T.31 8.3.6 - HDLC receive */ 
    if (!process_class1_cmd(s, &t))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FRM(t31_state_t *s, const char *t)
{
    /* T.31 8.3.4 - Facsimile receive */ 
    if (!process_class1_cmd(s, &t))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FRS(t31_state_t *s, const char *t)
{
    /* T.31 8.3.2 - Receive silence */ 
    if (!process_class1_cmd(s, &t))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FTH(t31_state_t *s, const char *t)
{
    /* T.31 8.3.5 - HDLC transmit */ 
    if (!process_class1_cmd(s, &t))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FTM(t31_state_t *s, const char *t)
{
    /* T.31 8.3.3 - Facsimile transmit */ 
    if (!process_class1_cmd(s, &t))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_FTS(t31_state_t *s, const char *t)
{
    /* T.31 8.3.1 - Transmit silence */ 
    if (!process_class1_cmd(s, &t))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_GCAP(t31_state_t *s, const char *t)
{
    /* V.250 6.1.9 - Request complete capabilities list */
    t += 5;
    /* Response elements
       +FCLASS     +F (FAX) commands
       +MS         +M (modulation control) commands +MS and +MR
       +MV18S      +M (modulation control) commands +MV18S and +MV18R
       +ES         +E (error control) commands +ES, +EB, +ER, +EFCS, and +ETBM
       +DS         +D (data compression) commands +DS and +DR */
    /* TODO: make this adapt to the configuration we really have. */
    if (t[0] == '='  &&  t[1] == '?')
    {
        at_put_response(s, "+GCAP:+FCLASS");
        t += 2;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_GCI(t31_state_t *s, const char *t)
{
    int val;
    char buf[50];

    /* V.250 6.1.10 - Country of installation, */ 
    t += 4;
    if (!parse_hex_out(s, &t, &s->country_of_installation, 255, "+GCI:", "(00-FF)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_GMI(t31_state_t *s, const char *t)
{
    /* V.250 6.1.4 - Request manufacturer identification */ 
    t += 4;
    if (t[0] == '='  &&  t[1] == '?')
    {
        at_put_response(s, manufacturer);
        t += 2;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_GMM(t31_state_t *s, const char *t)
{
    /* V.250 6.1.5 - Request model identification */ 
    t += 4;
    if (t[0] == '='  &&  t[1] == '?')
    {
        at_put_response(s, model);
        t += 2;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_GMR(t31_state_t *s, const char *t)
{
    /* V.250 6.1.6 - Request revision identification */ 
    t += 4;
    if (t[0] == '='  &&  t[1] == '?')
    {
        at_put_response(s, revision);
        t += 2;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_GOI(t31_state_t *s, const char *t)
{
    /* V.250 6.1.8 - Request global object identification */ 
    /* TODO: */
    t += 4;
    if (t[0] == '='  &&  t[1] == '?')
    {
        at_put_response(s, "42");
        t += 2;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_GSN(t31_state_t *s, const char *t)
{
    /* V.250 6.1.7 - Request product serial number identification */ 
    /* TODO: */
    t += 4;
    if (t[0] == '='  &&  t[1] == '?')
    {
        at_put_response(s, "42");
        t += 2;
    }
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ICF(t31_state_t *s, const char *t)
{
    /* V.250 6.2.11 - DTE-DCE character framing */ 
    t += 4;
    /* Character format
        0    auto detect
        1    8 data 2 stop
        2    8 data 1 parity 1 stop
        3    8 data 1 stop
        4    7 data 2 stop
        5    7 data 1 parity 1 stop
        6    7 data 1 stop
    
       parity
        0    Odd
        1    Even
        2    Mark
        3    Space */
    if (!parse_2_out(s, &t, &s->dte_char_format, 6, &s->dte_parity, 3, "+ICF:", "(0-6),(0-3)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ICLOK(t31_state_t *s, const char *t)
{
    /* V.250 6.2.14 - Select sync transmit clock source */ 
    t += 6;
    if (!parse_out(s, &t, NULL, 2, "+ICLOK:", "(0-2)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_IDSR(t31_state_t *s, const char *t)
{
    /* V.250 6.2.16 - Select data set ready option */ 
    t += 5;
    if (!parse_out(s, &t, NULL, 2, "+IDSR:", "(0-2)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_IFC(t31_state_t *s, const char *t)
{
    /* V.250 6.2.12 - DTE-DCE local flow control */ 
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ILRR(t31_state_t *s, const char *t)
{
    /* V.250 6.2.13 - DTE-DCE local rate reporting */ 
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_ILSD(t31_state_t *s, const char *t)
{
    /* V.250 6.2.15 - Select long space disconnect option */ 
    t += 5;
    if (!parse_out(s, &t, NULL, 2, "+ILSD:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_IPR(t31_state_t *s, const char *t)
{
    /* V.250 6.2.10 - Fixed DTE rate */ 
    /* TODO: */
    t += 4;
    if (!parse_out(s, &t, &s->dte_rate, 115200, "+IPR:", "(115200),(115200)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_IRTS(t31_state_t *s, const char *t)
{
    /* V.250 6.2.17 - Select synchronous mode RTS option */ 
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+IRTS:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MA(t31_state_t *s, const char *t)
{
    /* V.250 6.4.2 - Modulation automode control */ 
    /* TODO: */
    t += 3;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MR(t31_state_t *s, const char *t)
{
    /* V.250 6.4.3 - Modulation reporting control */ 
    /*  0    Disables reporting of modulation connection (+MCR: and +MRR: are not transmitted)
        1    Enables reporting of modulation connection (+MCR: and +MRR: are transmitted) */
    /* TODO: */
    t += 3;
    if (!parse_out(s, &t, NULL, 1, "+MR:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MS(t31_state_t *s, const char *t)
{
    /* V.250 6.4.1 - Modulation selection */ 
    /* TODO: */
    t += 3;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MSC(t31_state_t *s, const char *t)
{
    /* V.250 6.4.8 - Seamless rate change enable */ 
    /*  0   Disables V.34 seamless rate change 
        1   Enables V.34 seamless rate change */
    /* TODO: */
    t += 4;
    if (!parse_out(s, &t, NULL, 1, "+MSC:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MV18AM(t31_state_t *s, const char *t)
{
    /* V.250 6.4.6 - V.18 answering message editing */ 
    /* TODO: */
    t += 7;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MV18P(t31_state_t *s, const char *t)
{
    /* V.250 6.4.7 - Order of probes */ 
    /*  2    Send probe message in 5-bit (Baudot) mode
        3    Send probe message in DTMF mode
        4    Send probe message in EDT mode
        5    Send Rec. V.21 carrier as a probe
        6    Send Rec. V.23 carrier as a probe
        7    Send Bell 103 carrier as a probe */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 7, "+MV18P:", "(2-7)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MV18R(t31_state_t *s, const char *t)
{
    /* V.250 6.4.5 - V.18 reporting control */ 
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+MV18R:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_MV18S(t31_state_t *s, const char *t)
{
    /* V.250 6.4.4 - V.18 selection */ 
    /*  mode:
        0    Disables V.18 operation
        1    V.18 operation, auto detect mode
        2    V.18 operation, connect in 5-bit (Baudot) mode
        3    V.18 operation, connect in DTMF mode
        4    V.18 operation, connect in EDT mode
        5    V.18 operation, connect in V.21 mode
        6    V.18 operation, connect in V.23 mode
        7    V.18 operation, connect in Bell 103-type mode

        dflt_ans_mode:
        0    Disables V.18 answer operation
        1    No default specified (auto detect)
        2    V.18 operation connect in 5-bit (Baudot) mode
        3    V.18 operation connect in DTMF mode
        4    V.18 operation connect in EDT mode

        fbk_time_enable:
        0    Disable
        1    Enable

        ans_msg_enable
        0    Disable
        1    Enable

        probing_en
        0    Disable probing
        1    Enable probing
        2    Initiate probing */
    /* TODO: */
    t += 6;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TADR(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.9 - Local V.54 address */ 
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TAL(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.15 - Local analogue loop */ 
    /* Action
        0   Disable analogue loop
        1   Enable analogue loop
       Band
        0   Low frequency band
        1   High frequency band */
    /* TODO: */
    t += 4;
    if (!parse_2_out(s, &t, NULL, 1, NULL, 1, "+TAL:", "(0,1),(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TALS(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.6 - Analogue loop status */ 
    /*  0   Inactive
        1   V.24 circuit 141 invoked
        2   Front panel invoked
        3   Network management system invoked */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 3, "+TALS:", "(0-3)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TDLS(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.7 - Local digital loop status */ 
    /*  0   Disabled
        1   Enabled, inactive
        2   Front panel invoked
        3   Network management system invoked
        4   Remote invoked */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 3, "+TDLS:", "(0-4)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TE140(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.1 - Enable ckt 140 */ 
    /*  0   Disabled
        1   Enabled */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+TE140:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TE141(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.2 - Enable ckt 141 */ 
    /*  0   Response is disabled
        1   Response is enabled */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+TE141:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TEPAL(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.5 - Enable front panel analogue loop */ 
    /*  0   Disabled
        1   Enabled */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+TEPAL:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TEPDL(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.4 - Enable front panel RDL */ 
    /*  0   Disabled
        1   Enabled */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+TEPDL:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TERDL(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.3 - Enable RDL from remote */ 
    /*  0   Local DCE will ignore command from remote
        1   Local DCE will obey command from remote */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+TERDL:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TLDL(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.13 - Local digital loop */
    /*  0   Stop test
        1   Start test */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+TLDL:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TMODE(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.10 - Set V.54 mode */ 
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+TMODE:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TNUM(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.12 - Errored bit and block counts */ 
    /* TODO: */
    t += 5;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TRDL(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.14 - Request remote digital loop */ 
    /*  0   Stop RDL
        1   Start RDL */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+TRDL:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TRDLS(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.8 - Remote digital loop status */ 
    /* TODO: */
    t += 6;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TRES(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.17 - Self test result */ 
    /*  0   No test
        1   Pass
        2   Fail */
    /* TODO: */
    t += 5;
    if (!parse_out(s, &t, NULL, 1, "+TRES:", "(0-2)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TSELF(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.16 - Self test */ 
    /*  0   Intrusive full test
        1   Safe partial test */
    /* TODO: */
    t += 6;
    if (!parse_out(s, &t, NULL, 1, "+TSELF:", "(0,1)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_TTER(t31_state_t *s, const char *t)
{
    /* V.250 6.7.2.11 - Test error rate */ 
    /* TODO: */
    t += 5;
    if (!parse_2_out(s, &t, NULL, 65535, NULL, 65535, "+TTER:", "(0-65535),(0-65535)"))
        return NULL;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VBT(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 C.2.2 - Buffer threshold setting */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VCID(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 C.2.3 - Calling number ID presentation */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VDR(t31_state_t *s, const char *t)
{
    /* V.253 10.3.1 - Distinctive ring (ring cadence reporting) */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VDT(t31_state_t *s, const char *t)
{
    /* V.253 10.3.2 - Control tone cadence reporting */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VDX(t31_state_t *s, const char *t)
{
    /* V.253 10.5.6 - Speakerphone duplex mode */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VEM(t31_state_t *s, const char *t)
{
    /* V.253 10.5.7 - Deliver event reports */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VGM(t31_state_t *s, const char *t)
{
    /* V.253 10.5.2 - Microphone gain */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VGR(t31_state_t *s, const char *t)
{
    /* V.253 10.2.1 - Receive gain selection */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VGS(t31_state_t *s, const char *t)
{
    /* V.253 10.5.3 - Speaker gain */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VGT(t31_state_t *s, const char *t)
{
    /* V.253 10.2.2 - Volume selection */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VIP(t31_state_t *s, const char *t)
{
    /* V.253 10.1.1 - Initialize voice parameters */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VIT(t31_state_t *s, const char *t)
{
    /* V.253 10.2.3 - DTE/DCE inactivity timer */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VLS(t31_state_t *s, const char *t)
{
    /* V.253 10.2.4 - Analogue source/destination selection */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VPP(t31_state_t *s, const char *t)
{
    /* V.253 10.4.2 - Voice packet protocol */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VRA(t31_state_t *s, const char *t)
{
    /* V.253 10.2.5 - Ringing tone goes away timer */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VRL(t31_state_t *s, const char *t)
{
    /* V.253 10.1.2 - Ring local phone */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VRN(t31_state_t *s, const char *t)
{
    /* V.253 10.2.6 - Ringing tone never appeared timer */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VRX(t31_state_t *s, const char *t)
{
    /* V.253 10.1.3 - Voice receive state */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VSD(t31_state_t *s, const char *t)
{
    /* V.253 10.2.7 - Silence detection (QUIET and SILENCE) */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VSM(t31_state_t *s, const char *t)
{
    /* V.253 10.2.8 - Compression method selection */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VSP(t31_state_t *s, const char *t)
{
    /* V.253 10.5.1 - Voice speakerphone state */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VTA(t31_state_t *s, const char *t)
{
    /* V.253 10.5.4 - Train acoustic echo-canceller */ 
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VTD(t31_state_t *s, const char *t)
{
    /* V.253 10.2.9 - Beep tone duration timer */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VTH(t31_state_t *s, const char *t)
{
    /* V.253 10.5.5 - Train line echo-canceller */ 
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VTR(t31_state_t *s, const char *t)
{
    /* V.253 10.1.4 - Voice duplex state */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VTS(t31_state_t *s, const char *t)
{
    /* V.253 10.1.5 - DTMF and tone generation in voice */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_VTX(t31_state_t *s, const char *t)
{
    /* V.253 10.1.6 - Transmit data state */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

static const char *at_cmd_plus_WS46(t31_state_t *s, const char *t)
{
    /* 3GPP TS 27.007 5.9 - PCCA STD-101 [17] select wireless network */
    /* TODO: */
    t += 4;
    return t;
}
/*- End of function --------------------------------------------------------*/

/*
    AT command group prefixes:

    +A    Call control (network addressing) issues, common, PSTN, ISDN, Rec. X.25, switched digital
    +C    Digital cellular extensions
    +D    Data compression, Rec. V.42bis
    +E    Error control, Rec. V.42
    +F    Facsimile, Rec. T.30, etc.
    +G    Generic issues such as identity and capabilities
    +I    DTE-DCE interface issues, Rec. V.24, etc.
    +M    Modulation, Rec. V.32bis, etc.
    +S    Switched or simultaneous data types
    +T    Test issues
    +V    Voice extensions
    +W    Wireless extensions
*/

at_cmd_item_t at_commands[] =
{
    {" ", at_cmd_dummy},                /* Dummy to absorb spaces in commands */
    {"&C", at_cmd_amp_C},               /* V.250 6.2.8 - Circuit 109 (received line signal detector), behaviour */ 
    {"&D", at_cmd_amp_D},               /* V.250 6.2.9 - Circuit 108 (data terminal ready) behaviour */ 
    {"&F", at_cmd_amp_F},               /* V.250 6.1.2 - Set to factory-defined configuration */ 
    {"+A8E", at_cmd_plus_A8E},          /* V.251 5.1 - V.8 and V.8bis operation controls */
    {"+A8M", at_cmd_plus_A8M},          /* V.251 5.2 - Send V.8 menu signals */
    {"+A8T", at_cmd_plus_A8T},          /* V.251 5.3 - Send V.8bis signal and/or message(s) */
    {"+ASTO", at_cmd_plus_ASTO},        /* V.250 6.3.15 - Store telephone number */ 
    {"+CAAP", at_cmd_plus_CAAP},        /* 3GPP TS 27.007 7.25 - Automatic answer for eMLPP Service */
    {"+CACM", at_cmd_plus_CACM},        /* 3GPP TS 27.007 8.25 - Accumulated call meter */
    {"+CACSP", at_cmd_plus_CACSP},      /* 3GPP TS 27.007 11.1.7 - Voice Group or Voice Broadcast Call State Attribute Presentation */
    {"+CAEMLPP", at_cmd_plus_CAEMLPP},  /* 3GPP TS 27.007 7.22 - eMLPP Priority Registration and Interrogation */
    {"+CAHLD", at_cmd_plus_CAHLD},      /* 3GPP TS 27.007 11.1.3 - Leave an ongoing Voice Group or Voice Broadcast Call */
    {"+CAJOIN", at_cmd_plus_CAJOIN},    /* 3GPP TS 27.007 11.1.1 - Accept an incoming Voice Group or Voice Broadcast Call */
    {"+CALA", at_cmd_plus_CALA},        /* 3GPP TS 27.007 8.16 - Alarm */
    {"+CALCC", at_cmd_plus_CALCC},      /* 3GPP TS 27.007 11.1.6 - List current Voice Group and Voice Broadcast Calls */
    {"+CALD", at_cmd_plus_CALD},        /* 3GPP TS 27.007 8.38 - Delete alar m */
    {"+CALM", at_cmd_plus_CALM},        /* 3GPP TS 27.007 8.20 - Alert sound mode */
    {"+CAMM", at_cmd_plus_CAMM},        /* 3GPP TS 27.007 8.26 - Accumulated call meter maximum */
    {"+CANCHEV", at_cmd_plus_CANCHEV},  /* 3GPP TS 27.007 11.1.8 - NCH Support Indication */
    {"+CAOC", at_cmd_plus_CAOC},        /* 3GPP TS 27.007 7.16 - Advice of Charge */
    {"+CAPD", at_cmd_plus_CAPD},        /* 3GPP TS 27.007 8.39 - Postpone or dismiss an alarm */
    {"+CAPTT", at_cmd_plus_CAPTT},      /* 3GPP TS 27.007 11.1.4 - Talker Access for Voice Group Call */
    {"+CAREJ", at_cmd_plus_CAREJ},      /* 3GPP TS 27.007 11.1.2 - Reject an incoming Voice Group or Voice Broadcast Call */
    {"+CAULEV", at_cmd_plus_CAULEV},    /* 3GPP TS 27.007 11.1.5 - Voice Group Call Uplink Status Presentation */
    {"+CBC", at_cmd_plus_CBC},          /* 3GPP TS 27.007 8.4 - Battery charge */
    {"+CBCS", at_cmd_plus_CBCS},        /* 3GPP TS 27.007 11.3.2 - VBS subscriptions and GId status */
    {"+CBST", at_cmd_plus_CBST},        /* 3GPP TS 27.007 6.7 - Select bearer service type */
    {"+CCFC", at_cmd_plus_CCFC},        /* 3GPP TS 27.007 7.11 - Call forwarding number and conditions */
    {"+CCLK", at_cmd_plus_CCLK},        /* 3GPP TS 27.007 8.15 - Clock */
    {"+CCUG", at_cmd_plus_CCUG},        /* 3GPP TS 27.007 7.10 - Closed user group */
    {"+CCWA", at_cmd_plus_CCWA},        /* 3GPP TS 27.007 7.12 - Call waiting */
    {"+CCWE", at_cmd_plus_CCWE},        /* 3GPP TS 27.007 8.28 - Call Meter maximum event */
    {"+CDIP", at_cmd_plus_CDIP},        /* 3GPP TS 27.007 7.9 - Called line identification presentation */
    {"+CDIS", at_cmd_plus_CDIS},        /* 3GPP TS 27.007 8.8 - Display control */
    {"+CEER", at_cmd_plus_CEER},        /* 3GPP TS 27.007 6.10 - Extended error report */
    {"+CFCS", at_cmd_plus_CFCS},        /* 3GPP TS 27.007 7.24 - Fast call setup conditions */
    {"+CFUN", at_cmd_plus_CFUN},        /* 3GPP TS 27.007 8.2 - Set phone functionality */
    {"+CGACT", at_cmd_plus_CGACT},      /* 3GPP TS 27.007 10.1.10 - PDP context activate or deactivate */
    {"+CGANS", at_cmd_plus_CGANS},      /* 3GPP TS 27.007 10.1.16 - Manual response to a network request for PDP context activation */
    {"+CGATT", at_cmd_plus_CGATT},      /* 3GPP TS 27.007 10.1.9 - PS attach or detach */
    {"+CGAUTO", at_cmd_plus_CGAUTO},    /* 3GPP TS 27.007 10.1.15 - Automatic response to a network request for PDP context activation */
    {"+CGCLASS", at_cmd_plus_CGCLASS},  /* 3GPP TS 27.007 10.1.17 - GPRS mobile station class (GPRS only) */
    {"+CGCLOSP", at_cmd_plus_CGCLOSP},  /* 3GPP TS 27.007 10.1.13 - Configure local octet stream PAD parameters (Obsolete) */
    {"+CGCLPAD", at_cmd_plus_CGCLPAD},  /* 3GPP TS 27.007 10.1.12 - Configure local triple-X PAD parameters (GPRS only) (Obsolete) */
    {"+CGCMOD", at_cmd_plus_CGCMOD},    /* 3GPP TS 27.007 10.1.11 - PDP Context Modify */
    {"+CGCS", at_cmd_plus_CGCS},        /* 3GPP TS 27.007 11.3.1 - VGCS subscriptions and GId status */
    {"+CGDATA", at_cmd_plus_CGDATA},    /* 3GPP TS 27.007 10.1.12 - Enter data state */
    {"+CGDCONT", at_cmd_plus_CGDCONT},  /* 3GPP TS 27.007 10.1.1 - Define PDP Context */
    {"+CGDSCONT", at_cmd_plus_CGDSCONT},/* 3GPP TS 27.007 10.1.2 - Define Secondary PDP Context */
    {"+CGEQMIN", at_cmd_plus_CGEQMIN},  /* 3GPP TS 27.007 10.1.7 - 3G Quality of Service Profile (Minimum acceptable) */
    {"+CGEQNEG", at_cmd_plus_CGEQNEG},  /* 3GPP TS 27.007 10.1.8 - 3G Quality of Service Profile (Negotiated) */
    {"+CGEQREQ", at_cmd_plus_CGEQREQ},  /* 3GPP TS 27.007 10.1.6 - 3G Quality of Service Profile (Requested) */
    {"+CGEREP", at_cmd_plus_CGEREP},    /* 3GPP TS 27.007 10.1.18 - Packet Domain event reporting */
    {"+CGMI", at_cmd_plus_CGMI},        /* 3GPP TS 27.007 5.1 - Request manufacturer identification */
    {"+CGMM", at_cmd_plus_CGMM},        /* 3GPP TS 27.007 5.2 - Request model identification */
    {"+CGMR", at_cmd_plus_CGMR},        /* 3GPP TS 27.007 5.3 - Request revision identification */
    {"+CGPADDR", at_cmd_plus_CGPADDR},  /* 3GPP TS 27.007 10.1.14 - Show PDP address */
    {"+CGQMIN", at_cmd_plus_CGQMIN},    /* 3GPP TS 27.007 10.1.5 - Quality of Service Profile (Minimum acceptable) */
    {"+CGQREQ", at_cmd_plus_CGQREQ},    /* 3GPP TS 27.007 10.1.4 - Quality of Service Profile (Requested) */
    {"+CGREG", at_cmd_plus_CGREG},      /* 3GPP TS 27.007 10.1.19 - GPRS network registration status */
    {"+CGSMS", at_cmd_plus_CGSMS},      /* 3GPP TS 27.007 10.1.20 - Select service for MO SMS messages */
    {"+CGSN", at_cmd_plus_CGSN},        /* 3GPP TS 27.007 5.4 - Request product serial number identification */
    {"+CGTFT", at_cmd_plus_CGTFT},      /* 3GPP TS 27.007 10.1.3 - Traffic Flow Template */
    {"+CHLD", at_cmd_plus_CHLD},        /* 3GPP TS 27.007 7.13 - Call related supplementary services */
    {"+CHSA", at_cmd_plus_CHSA},        /* 3GPP TS 27.007 6.18 - HSCSD non-transparent asymmetry configuration */
    {"+CHSC", at_cmd_plus_CHSC},        /* 3GPP TS 27.007 6.15 - HSCSD current call parameters */
    {"+CHSD", at_cmd_plus_CHSD},        /* 3GPP TS 27.007 6.12 - HSCSD device parameters */
    {"+CHSN", at_cmd_plus_CHSN},        /* 3GPP TS 27.007 6.14 - HSCSD non-transparent call configuration */
    {"+CHSR", at_cmd_plus_CHSR},        /* 3GPP TS 27.007 6.16 - HSCSD parameters report */
    {"+CHST", at_cmd_plus_CHST},        /* 3GPP TS 27.007 6.13 - HSCSD transparent call configuration */
    {"+CHSU", at_cmd_plus_CHSU},        /* 3GPP TS 27.007 6.17 - HSCSD automatic user initiated upgrading */
    {"+CHUP", at_cmd_plus_CHUP},        /* 3GPP TS 27.007 6.5 - Hangup call */
    {"+CIMI", at_cmd_plus_CIMI},        /* 3GPP TS 27.007 5.6 - Request international mobile subscriber identity */
    {"+CIND", at_cmd_plus_CIND},        /* 3GPP TS 27.007 8.9 - Indicator control */
    {"+CKPD", at_cmd_plus_CKPD},        /* 3GPP TS 27.007 8.7 - Keypad control */
    {"+CLAC", at_cmd_plus_CLAC},        /* 3GPP TS 27.007 8.37 - List all available AT commands */
    {"+CLAE", at_cmd_plus_CLAE},        /* 3GPP TS 27.007 8.31 - Language Event */
    {"+CLAN", at_cmd_plus_CLAN},        /* 3GPP TS 27.007 8.30 - Set Language */
    {"+CLCC", at_cmd_plus_CLCC},        /* 3GPP TS 27.007 7.18 - List current calls */
    {"+CLCK", at_cmd_plus_CLCK},        /* 3GPP TS 27.007 7.4 - Facility lock */
    {"+CLIP", at_cmd_plus_CLIP},        /* 3GPP TS 27.007 7.6 - Calling line identification presentation */
    {"+CLIR", at_cmd_plus_CLIR},        /* 3GPP TS 27.007 7.7 - Calling line identification restriction */
    {"+CLVL", at_cmd_plus_CLVL},        /* 3GPP TS 27.007 8.23 - Loudspeaker volume level */
    {"+CMAR", at_cmd_plus_CMAR},        /* 3GPP TS 27.007 8.36 - Master Reset */
    {"+CMEC", at_cmd_plus_CMEC},        /* 3GPP TS 27.007 8.6 - Mobile Termination control mode */
    {"+CMER", at_cmd_plus_CMER},        /* 3GPP TS 27.007 8.10 - Mobile Termination event reporting */
    {"+CMOD", at_cmd_plus_CMOD},        /* 3GPP TS 27.007 6.4 - Call mode */
    {"+CMUT", at_cmd_plus_CMUT},        /* 3GPP TS 27.007 8.24 - Mute control */
    {"+CMUX", at_cmd_plus_CMUX},        /* 3GPP TS 27.007 5.7 - Multiplexing mode */
    {"+CNUM", at_cmd_plus_CNUM},        /* 3GPP TS 27.007 7.1 - Subscriber number */
    {"+COLP", at_cmd_plus_COLP},        /* 3GPP TS 27.007 7.8 - Connected line identification presentation */
    {"+COPN", at_cmd_plus_COPN},        /* 3GPP TS 27.007 7.21 - Read operator names */
    {"+COPS", at_cmd_plus_COPS},        /* 3GPP TS 27.007 7.3 - PLMN selection */
    {"+COTDI", at_cmd_plus_COTDI},      /* 3GPP TS 27.007 11.1.9 - Originator to Dispatcher Information */
    {"+CPAS", at_cmd_plus_CPAS},        /* 3GPP TS 27.007 8.1 - Phone activity status */
    {"+CPBF", at_cmd_plus_CPBF},        /* 3GPP TS 27.007 8.13 - Find phonebook entries */
    {"+CPBR", at_cmd_plus_CPBR},        /* 3GPP TS 27.007 8.12 - Read phonebook entries */
    {"+CPBS", at_cmd_plus_CPBS},        /* 3GPP TS 27.007 8.11 - Select phonebook memory storage */
    {"+CPBW", at_cmd_plus_CPBW},        /* 3GPP TS 27.007 8.14 - Write phonebook entry */
    {"+CPIN", at_cmd_plus_CPIN},        /* 3GPP TS 27.007 8.3 - Enter PIN */
    {"+CPLS", at_cmd_plus_CPLS},        /* 3GPP TS 27.007 7.20 - Selection of preferred PLMN list */
    {"+CPOL", at_cmd_plus_CPOL},        /* 3GPP TS 27.007 7.19 - Preferred PLMN list */
    {"+CPPS", at_cmd_plus_CPPS},        /* 3GPP TS 27.007 7.23 - eMLPP subscriptions */
    {"+CPROT", at_cmd_plus_CPROT},      /* 3GPP TS 27.007 8.42 - Enter protocol mode */
    {"+CPUC", at_cmd_plus_CPUC},        /* 3GPP TS 27.007 8.27 - Price per unit and currency table */
    {"+CPWC", at_cmd_plus_CPWC},        /* 3GPP TS 27.007 8.29 - Power class */
    {"+CPWD", at_cmd_plus_CPWD},        /* 3GPP TS 27.007 7.5 - Change password */
    {"+CR", at_cmd_plus_CR},            /* 3GPP TS 27.007 6.9 - Service reporting control */
    {"+CRC", at_cmd_plus_CRC},          /* 3GPP TS 27.007 6.11 - Cellular result codes */
    {"+CREG", at_cmd_plus_CREG},        /* 3GPP TS 27.007 7.2 - Network registration */
    {"+CRLP", at_cmd_plus_CRLP},        /* 3GPP TS 27.007 6.8 - Radio link protocol */
    {"+CRMC", at_cmd_plus_CRMC},        /* 3GPP TS 27.007 8.34 - Ring Melody Control */
    {"+CRMP", at_cmd_plus_CRMP},        /* 3GPP TS 27.007 8.35 - Ring Melody Playback */
    {"+CRSL", at_cmd_plus_CRSL},        /* 3GPP TS 27.007 8.21 - Ringer sound level */
    {"+CRSM", at_cmd_plus_CRSM},        /* 3GPP TS 27.007 8.18 - Restricted SIM access */
    {"+CSCC", at_cmd_plus_CSCC},        /* 3GPP TS 27.007 8.19 - Secure control command */
    {"+CSCS", at_cmd_plus_CSCS},        /* 3GPP TS 27.007 5.5 - Select TE character set */
    {"+CSDF", at_cmd_plus_CSDF},        /* 3GPP TS 27.007 6.22 - Settings date format */
    {"+CSGT", at_cmd_plus_CSGT},        /* 3GPP TS 27.007 8.32 - Set Greeting Text */
    {"+CSIL", at_cmd_plus_CSIL},        /* 3GPP TS 27.007 6.23 - Silence Command */
    {"+CSIM", at_cmd_plus_CSIM},        /* 3GPP TS 27.007 8.17 - Generic SIM access */
    {"+CSNS", at_cmd_plus_CSNS},        /* 3GPP TS 27.007 6.19 - Single numbering scheme */
    {"+CSQ", at_cmd_plus_CSQ},          /* 3GPP TS 27.007 8.5 - Signal quality */
    {"+CSSN", at_cmd_plus_CSSN},        /* 3GPP TS 27.007 7.17 - Supplementary service notifications */
    {"+CSTA", at_cmd_plus_CSTA},        /* 3GPP TS 27.007 6.1 - Select type of address */
    {"+CSTF", at_cmd_plus_CSTF},        /* 3GPP TS 27.007 6.24 - Settings time format */
    {"+CSVM", at_cmd_plus_CSVM},        /* 3GPP TS 27.007 8.33 - Set Voice Mail Number */
    {"+CTFR", at_cmd_plus_CTFR},        /* 3GPP TS 27.007 7.14 - Call deflection */
    {"+CTZR", at_cmd_plus_CTZR},        /* 3GPP TS 27.007 8.41 - Time Zone Reporting */
    {"+CTZU", at_cmd_plus_CTZU},        /* 3GPP TS 27.007 8.40 - Automatic Time Zone Update */
    {"+CUSD", at_cmd_plus_CUSD},        /* 3GPP TS 27.007 7.15 - Unstructured supplementary service data */
    {"+CUUS1", at_cmd_plus_CUUS1},      /* 3GPP TS 27.007 7.26 - User to User Signalling Service 1 */
    {"+CV120", at_cmd_plus_CV120},      /* 3GPP TS 27.007 6.21 - V.120 rate adaption protocol */
    {"+CVHU", at_cmd_plus_CVHU},        /* 3GPP TS 27.007 6.20 - Voice Hangup Control */
    {"+CVIB", at_cmd_plus_CVIB},        /* 3GPP TS 27.007 8.22 - Vibrator mode */
    {"+DR", at_cmd_plus_DR},            /* V.250 6.6.2 - Data compression reporting */ 
    {"+DS", at_cmd_plus_DS},            /* V.250 6.6.1 - Data compression */ 
    {"+EB", at_cmd_plus_EB},            /* V.250 6.5.2 - Break handling in error control operation */ 
    {"+EFCS", at_cmd_plus_EFCS},        /* V.250 6.5.4 - 32-bit frame check sequence */ 
    {"+EFRAM", at_cmd_plus_EFRAM},      /* V.250 6.5.8 - Frame length */ 
    {"+ER", at_cmd_plus_ER},            /* V.250 6.5.5 - Error control reporting */ 
    {"+ES", at_cmd_plus_ES},            /* V.250 6.5.1 - Error control selection */ 
    {"+ESR", at_cmd_plus_ESR},          /* V.250 6.5.3 - Selective repeat */ 
    {"+ETBM", at_cmd_plus_ETBM},        /* V.250 6.5.6 - Call termination buffer management */ 
    {"+EWIND", at_cmd_plus_EWIND},      /* V.250 6.5.7 - Window size */ 
    {"+FAR", at_cmd_plus_FAR},          /* T.31 8.5.1 - Adaptive reception control */ 
    {"+FCL", at_cmd_plus_FCL},          /* T.31 8.5.2 - Carrier loss timeout */ 
    {"+FCLASS", at_cmd_plus_FCLASS},    /* T.31 8.2 - Capabilities identification and control */ 
    {"+FDD", at_cmd_plus_FDD},          /* T.31 8.5.3 - Double escape character replacement */ 
    {"+FIT", at_cmd_plus_FIT},          /* T.31 8.5.4 - DTE inactivity timeout */ 
    {"+FLO", at_cmd_plus_FLO},          /* T.31 says to implement something similar to +IFC */ 
    {"+FMI", at_cmd_plus_GMI},          /* T.31 says to duplicate +GMI */ 
    {"+FMM", at_cmd_plus_GMM},          /* T.31 says to duplicate +GMM */ 
    {"+FMR", at_cmd_plus_GMR},          /* T.31 says to duplicate +GMR */ 
    {"+FPR", at_cmd_plus_FPR},          /* T.31 says to implement something similar to +IPR */ 
    {"+FRH", at_cmd_plus_FRH},          /* T.31 8.3.6 - HDLC receive */ 
    {"+FRM", at_cmd_plus_FRM},          /* T.31 8.3.4 - Facsimile receive */ 
    {"+FRS", at_cmd_plus_FRS},          /* T.31 8.3.2 - Receive silence */ 
    {"+FTH", at_cmd_plus_FTH},          /* T.31 8.3.5 - HDLC transmit */ 
    {"+FTM", at_cmd_plus_FTM},          /* T.31 8.3.3 - Facsimile transmit */ 
    {"+FTS", at_cmd_plus_FTS},          /* T.31 8.3.1 - Transmit silence */ 
    {"+GCAP", at_cmd_plus_GCAP},        /* V.250 6.1.9 - Request complete capabilities list */ 
    {"+GCI", at_cmd_plus_GCI},          /* V.250 6.1.10 - Country of installation, */ 
    {"+GMI", at_cmd_plus_GMI},          /* V.250 6.1.4 - Request manufacturer identification */ 
    {"+GMM", at_cmd_plus_GMM},          /* V.250 6.1.5 - Request model identification */ 
    {"+GMR", at_cmd_plus_GMR},          /* V.250 6.1.6 - Request revision identification */ 
    {"+GOI", at_cmd_plus_GOI},          /* V.250 6.1.8 - Request global object identification */ 
    {"+GSN", at_cmd_plus_GSN},          /* V.250 6.1.7 - Request product serial number identification */ 
    {"+ICF", at_cmd_plus_ICF},          /* V.250 6.2.11 - DTE-DCE character framing */ 
    {"+ICLOK", at_cmd_plus_ICLOK},      /* V.250 6.2.14 - Select sync transmit clock source */ 
    {"+IDSR", at_cmd_plus_IDSR},        /* V.250 6.2.16 - Select data set ready option */ 
    {"+IFC", at_cmd_plus_IFC},          /* V.250 6.2.12 - DTE-DCE local flow control */ 
    {"+ILRR", at_cmd_plus_ILRR},        /* V.250 6.2.13 - DTE-DCE local rate reporting */ 
    {"+ILSD", at_cmd_plus_ILSD},        /* V.250 6.2.15 - Select long space disconnect option */ 
    {"+IPR", at_cmd_plus_IPR},          /* V.250 6.2.10 - Fixed DTE rate */ 
    {"+IRTS", at_cmd_plus_IRTS},        /* V.250 6.2.17 - Select synchronous mode RTS option */ 
    {"+MA", at_cmd_plus_MA},            /* V.250 6.4.2 - Modulation automode control */ 
    {"+MR", at_cmd_plus_MR},            /* V.250 6.4.3 - Modulation reporting control */ 
    {"+MS", at_cmd_plus_MS},            /* V.250 6.4.1 - Modulation selection */ 
    {"+MSC", at_cmd_plus_MSC},          /* V.250 6.4.8 - Seamless rate change enable */ 
    {"+MV18AM", at_cmd_plus_MV18AM},    /* V.250 6.4.6 - V.18 answering message editing */ 
    {"+MV18P", at_cmd_plus_MV18P},      /* V.250 6.4.7 - Order of probes */ 
    {"+MV18R", at_cmd_plus_MV18R},      /* V.250 6.4.5 - V.18 reporting control */ 
    {"+MV18S", at_cmd_plus_MV18S},      /* V.250 6.4.4 - V.18 selection */ 
    {"+TADR", at_cmd_plus_TADR},        /* V.250 6.7.2.9 - Local V.54 address */ 
    {"+TAL", at_cmd_plus_TAL},          /* V.250 6.7.2.15 - Local analogue loop */ 
    {"+TALS", at_cmd_plus_TALS},        /* V.250 6.7.2.6 - Analogue loop status */ 
    {"+TDLS", at_cmd_plus_TDLS},        /* V.250 6.7.2.7 - Local digital loop status */ 
    {"+TE140", at_cmd_plus_TE140},      /* V.250 6.7.2.1 - Enable ckt 140 */ 
    {"+TE141", at_cmd_plus_TE141},      /* V.250 6.7.2.2 - Enable ckt 141 */ 
    {"+TEPAL", at_cmd_plus_TEPAL},      /* V.250 6.7.2.5 - Enable front panel analogue loop */ 
    {"+TEPDL", at_cmd_plus_TEPDL},      /* V.250 6.7.2.4 - Enable front panel RDL */ 
    {"+TERDL", at_cmd_plus_TERDL},      /* V.250 6.7.2.3 - Enable RDL from remote */ 
    {"+TLDL", at_cmd_plus_TLDL},        /* V.250 6.7.2.13 - Local digital loop */ 
    {"+TMODE", at_cmd_plus_TMODE},      /* V.250 6.7.2.10 - Set V.54 mode */ 
    {"+TNUM", at_cmd_plus_TNUM},        /* V.250 6.7.2.12 - Errored bit and block counts */ 
    {"+TRDL", at_cmd_plus_TRDL},        /* V.250 6.7.2.14 - Request remote digital loop */ 
    {"+TRDLS", at_cmd_plus_TRDLS},      /* V.250 6.7.2.8 - Remote digital loop status */ 
    {"+TRES", at_cmd_plus_TRES},        /* V.250 6.7.2.17 - Self test result */ 
    {"+TSELF", at_cmd_plus_TSELF},      /* V.250 6.7.2.16 - Self test */ 
    {"+TTER", at_cmd_plus_TTER},        /* V.250 6.7.2.11 - Test error rate */ 
    {"+VBT", at_cmd_plus_VBT},          /* 3GPP TS 27.007 C.2.2 - Buffer threshold setting */
    {"+VCID", at_cmd_plus_VCID},        /* 3GPP TS 27.007 C.2.3 - Calling number ID presentation */
    {"+VDR", at_cmd_plus_VDR},          /* V.253 10.3.1 - Distinctive ring (ring cadence reporting) */
    {"+VDT", at_cmd_plus_VDT},          /* V.253 10.3.2 - Control tone cadence reporting */
    {"+VDX", at_cmd_plus_VDX},          /* V.253 10.5.6 - Speakerphone duplex mode */
    {"+VEM", at_cmd_plus_VEM},          /* V.253 10.5.7 - Deliver event reports */
    {"+VGM", at_cmd_plus_VGM},          /* V.253 10.5.2 - Microphone gain */
    {"+VGR", at_cmd_plus_VGR},          /* V.253 10.2.1 - Receive gain selection */
    {"+VGS", at_cmd_plus_VGS},          /* V.253 10.5.3 - Speaker gain */
    {"+VGT", at_cmd_plus_VGT},          /* V.253 10.2.2 - Volume selection */
    {"+VIP", at_cmd_plus_VIP},          /* V.253 10.1.1 - Initialize voice parameters */
    {"+VIT", at_cmd_plus_VIT},          /* V.253 10.2.3 - DTE/DCE inactivity timer */
    {"+VLS", at_cmd_plus_VLS},          /* V.253 10.2.4 - Analogue source/destination selection */
    {"+VPP", at_cmd_plus_VPP},          /* V.253 10.4.2 - Voice packet protocol */
    {"+VRA", at_cmd_plus_VRA},          /* V.253 10.2.5 - Ringing tone goes away timer */
    {"+VRL", at_cmd_plus_VRL},          /* V.253 10.1.2 - Ring local phone */
    {"+VRN", at_cmd_plus_VRN},          /* V.253 10.2.6 - Ringing tone never appeared timer */
    {"+VRX", at_cmd_plus_VRX},          /* V.253 10.1.3 - Voice receive state */
    {"+VSD", at_cmd_plus_VSD},          /* V.253 10.2.7 - Silence detection (QUIET and SILENCE) */
    {"+VSM", at_cmd_plus_VSM},          /* V.253 10.2.8 - Compression method selection */
    {"+VSP", at_cmd_plus_VSP},          /* V.253 10.5.1 - Voice speakerphone state */
    {"+VTA", at_cmd_plus_VTA},          /* V.253 10.5.4 - Train acoustic echo-canceller */ 
    {"+VTD", at_cmd_plus_VTD},          /* V.253 10.2.9 - Beep tone duration timer */
    {"+VTH", at_cmd_plus_VTH},          /* V.253 10.5.5 - Train line echo-canceller */ 
    {"+VTR", at_cmd_plus_VTR},          /* V.253 10.1.4 - Voice duplex state */
    {"+VTS", at_cmd_plus_VTS},          /* V.253 10.1.5 - DTMF and tone generation in voice */
    {"+VTX", at_cmd_plus_VTX},          /* V.253 10.1.6 - Transmit data state */
    {"+WS46", at_cmd_plus_WS46},        /* 3GPP TS 27.007 5.9 - PCCA STD-101 [17] select wireless network */
    {";", at_cmd_dummy},                /* Dummy to absorb semi-colon delimiters in commands */
    {"A", at_cmd_A},                    /* V.250 6.3.5 - Answer */ 
    {"D", at_cmd_D},                    /* V.250 6.3.1 - Dial */ 
    {"E", at_cmd_E},                    /* V.250 6.2.4 - Command echo */ 
    {"H", at_cmd_H},                    /* V.250 6.3.6 - Hook control */ 
    {"I", at_cmd_I},                    /* V.250 6.1.3 - Request identification information */ 
    {"L", at_cmd_L},                    /* V.250 6.3.13 - Monitor speaker loudness */ 
    {"M", at_cmd_M},                    /* V.250 6.3.14 - Monitor speaker mode */ 
    {"O", at_cmd_O},                    /* V.250 6.3.7 - Return to online data state */ 
    {"P", at_cmd_P},                    /* V.250 6.3.3 - Select pulse dialling (command) */ 
    {"Q", at_cmd_Q},                    /* V.250 6.2.5 - Result code suppression */ 
    {"S0", at_cmd_S0},                  /* V.250 6.3.8 - Automatic answer */ 
    {"S10", at_cmd_S10},                /* V.250 6.3.12 - Automatic disconnect delay */ 
    {"S3", at_cmd_S3},                  /* V.250 6.2.1 - Command line termination character */ 
    {"S4", at_cmd_S4},                  /* V.250 6.2.2 - Response formatting character */ 
    {"S5", at_cmd_S5},                  /* V.250 6.2.3 - Command line editing character */ 
    {"S6", at_cmd_S6},                  /* V.250 6.3.9 - Pause before blind dialling */ 
    {"S7", at_cmd_S7},                  /* V.250 6.3.10 - Connection completion timeout */ 
    {"S8", at_cmd_S8},                  /* V.250 6.3.11 - Comma dial modifier time */ 
    {"T", at_cmd_T},                    /* V.250 6.3.2 - Select tone dialling (command) */ 
    {"V", at_cmd_V},                    /* V.250 6.2.6 - DCE response format */ 
    {"X", at_cmd_X},                    /* V.250 6.2.7 - Result code selection and call progress monitoring control */ 
    {"Z", at_cmd_Z},                    /* V.250 6.1.1 - Reset to default configuration */ 
};

static int cmd_compare(const void *a, const void *b)
{
    /* V.250 5.4.1 says upper and lower case are equivalent in commands */
    return strncasecmp(((at_cmd_item_t *) a)->tag, ((at_cmd_item_t *) b)->tag, strlen(((at_cmd_item_t *) b)->tag));
}
/*- End of function --------------------------------------------------------*/

static void at_interpreter(t31_state_t *s, const char *cmd, int len)
{
    int i;
    int c;
    at_cmd_item_t xxx;
    at_cmd_item_t *yyy;
    const char *t;

    for (i = 0;  i < len;  i++)
    {
        /* The spec says the top bit should be ignored */
        c = *cmd++ & 0x7F;
        /* Handle incoming character */
        if (s->line_ptr < 2)
        {
            /* Look for the initial "at", "AT", "a/" or "A/", and ignore anything before it */
            /* V.250 5.2.1 only shows "at" and "AT" as command prefixes. "At" and "aT" are
               not specified, despite 5.4.1 saying upper and lower case are equivalent in
               commands. */
            if (tolower(c) == 'a')
            {
                s->line_ptr = 0;
                s->line[s->line_ptr++] = c;
            }
            else if (s->line_ptr == 1)
            {
                if ((c == 't'  &&  s->line[0] == 'a')
                    ||
                    (c == 'T'  &&  s->line[0] == 'A'))
                {
                    /* We have an "AT" command */
                    s->line[s->line_ptr++] = c;
                }
                else if (c == '/')
                {
                    /* We have an "A/" command */
                    /* TODO: implement "A/" command repeat */
                    s->line[s->line_ptr++] = c;
                }
                else
                {
                    s->line_ptr = 0;
                }
            }
        }
        else
        {
            /* We are beyond the initial AT */
            if (c >= 0x20)
            {
                /* Add a new char */
                if (s->line_ptr < (sizeof(s->line) - 1))
                    s->line[s->line_ptr++] = toupper(c);
            }
            else if (c == s->p.s_regs[3])
            {
                /* End of command line. Do line validation */
                s->line[s->line_ptr] = '\0';
                if (s->line_ptr > 2)
                {
                    /* The spec says the commands within a command line are executed in order, until
                       an error is found, or the end of the command line is reached. */
                    if (s->p.echo)
                        s->at_tx_handler(s, s->at_tx_user_data, s->line, strlen(s->line));
                    t = s->line + 2;
                    while (t  &&  *t)
                    {
                        xxx.tag = t;
                        xxx.serv = 0;
                        yyy = (at_cmd_item_t *) bsearch(&xxx, at_commands, sizeof(at_commands)/sizeof(at_cmd_item_t), sizeof(at_cmd_item_t), cmd_compare);
                        if (yyy  &&  (t = yyy->serv(s, t)))
                        {
                            if (t == (const char *) -1)
                                break;
                        }
                        else
                        {
                            t = NULL;
                            break;
                        }
                    }
                    if (t != (const char *) -1)
                    {
                        if (t == NULL)
                            at_put_response_code(s, RESPONSE_CODE_ERROR);
                        else
                            at_put_response_code(s, RESPONSE_CODE_OK);
                    }
                }
                s->line_ptr = 0;
            }
            else if (c == s->p.s_regs[5])
            {
                /* Command line editing character (backspace) */
                if (s->line_ptr > 0)
                    s->line_ptr--;
            }
            /* The spec says control characters, other than those
               explicitly handled, should be ignored. */
        }
    }
}
/*- End of function --------------------------------------------------------*/

void t31_call_event(t31_state_t *s, int event)
{
    tone_gen_descriptor_t tone_desc;

printf("Call event %d received\n", event);
    switch (event)
    {
    case T31_CALL_EVENT_ALERTING:
        at_put_response_code(s, RESPONSE_CODE_RING);
        break;
    case T31_CALL_EVENT_ANSWERED:
        if (s->fclass_mode == 0)
        {
            /* Normal data modem connection */
            /* TODO: */
            s->at_rx_mode = AT_MODE_CONNECTED;
        }
        else
        {
            /* FAX modem connection */
            s->at_rx_mode = AT_MODE_COMMAND;
            restart_modem(s, T31_CED_TONE);
        }
        break;
    case T31_CALL_EVENT_CONNECTED:
printf("Dial call - connected. fclass=%d\n", s->fclass_mode);
        if (s->fclass_mode == 0)
        {
            /* Normal data modem connection */
            /* TODO: */
            s->at_rx_mode = AT_MODE_CONNECTED;
        }
        else
        {
            /* FAX modem connection */
            s->at_rx_mode = AT_MODE_COMMAND;
            restart_modem(s, T31_CNG_TONE);
            s->dte_is_waiting = TRUE;
        }
        break;
    case T31_CALL_EVENT_BUSY:
        s->at_rx_mode = AT_MODE_COMMAND;
        at_put_response_code(s, RESPONSE_CODE_BUSY);
        break;
    case T31_CALL_EVENT_NO_DIALTONE:
        s->at_rx_mode = AT_MODE_COMMAND;
        at_put_response_code(s, RESPONSE_CODE_NO_DIALTONE);
        break;
    case T31_CALL_EVENT_NO_ANSWER:
        s->at_rx_mode = AT_MODE_COMMAND;
        at_put_response_code(s, RESPONSE_CODE_NO_ANSWER);
        break;
    default:
        break;
    }
}
/*- End of function --------------------------------------------------------*/

void t31_at_rx(t31_state_t *s, const char *t, int len)
{
    switch (s->at_rx_mode)
    {
    case AT_MODE_COMMAND:
        at_interpreter(s, t, len);
        break;
    case AT_MODE_CONNECTED:
        break;
    case AT_MODE_HDLC:
        dle_unstuff_hdlc(s, t, len);
        break;
    case AT_MODE_STUFFED:
        dle_unstuff(s, t, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

int t31_rx(t31_state_t *s, int16_t *buf, int len)
{
    int i;
    int read_len;

    if (!s->transmit  ||  s->modem == T31_CNG_TONE)
    {
        switch (s->modem)
        {
        case T31_CED_TONE:
            break;
        case T31_CNG_TONE:
        case T31_V21_RX:
            fsk_rx(&(s->v21rx), buf, len);
            break;
#if defined(ENABLE_V17)
        case T31_V17_RX:
            v17_rx(&(s->v17rx), buf, len);
            break;
#endif
        case T31_V27TER_RX:
            v27ter_rx(&(s->v27ter_rx), buf, len);
            break;
        case T31_V29_RX:
            v29_rx(&(s->v29rx), buf, len);
            break;
        default:
            /* Absorb the data, but ignore it. */
            break;
        }
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/

int t31_tx(t31_state_t *s, int16_t *buf, int max_len)
{
    int len;
    int lenx;

    len = 0;
    if (s->transmit)
    {
        if (s->silent_samples)
        {
            len = s->silent_samples;
            if (len > max_len)
                len = max_len;
            s->silent_samples -= len;
            max_len -= len;
            memset(buf, 0, len*sizeof(int16_t));
            if (max_len > 0  &&  s->modem == T31_SILENCE_TX)
            {
                at_put_response_code(s, RESPONSE_CODE_OK);
                max_len = 0;
            }
        }
        if (max_len > 0)
        {
            switch (s->modem)
            {
            case T31_CED_TONE:
                if ((lenx = tone_gen(&(s->tone_gen), buf + len, max_len)) <= 0)
                {
                    /* Go directly to V.21/HDLC transmit. */
                    restart_modem(s, T31_V21_TX);
                    s->at_rx_mode = AT_MODE_HDLC;
                    at_put_response_code(s, RESPONSE_CODE_CONNECT);
                }
                len += lenx;
                break;
            case T31_CNG_TONE:
                len += tone_gen(&(s->tone_gen), buf + len, max_len);
                break;
            case T31_V21_TX:
                len += fsk_tx(&(s->v21tx), buf + len, max_len);
                break;
#if defined(ENABLE_V17)
            case T31_V17_TX:
                if ((lenx = v17_tx(&(s->v17tx), buf + len, max_len)) <= 0)
                    at_put_response_code(s, RESPONSE_CODE_OK);
                len += lenx;
                break;
#endif
            case T31_V27TER_TX:
                if ((lenx = v27ter_tx(&(s->v27ter_tx), buf + len, max_len)) <= 0)
                    at_put_response_code(s, RESPONSE_CODE_OK);
                len += lenx;
                break;
            case T31_V29_TX:
                if ((lenx = v29_tx(&(s->v29tx), buf + len, max_len)) <= 0)
                    at_put_response_code(s, RESPONSE_CODE_OK);
                len += lenx;
                break;
            }
        }
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

int t31_init(t31_state_t *s,
             t31_at_tx_handler_t *at_tx_handler,
             void *at_tx_user_data,
             t31_call_control_handler_t *call_control_handler,
             void *call_control_user_data)
{
    memset(s, 0, sizeof(*s));
    if (at_tx_handler == NULL  ||  call_control_handler == NULL)
        return -1;
#if defined(ENABLE_V17)
    v17_rx_init(&(s->v17rx), 14400, fast_putbit, s);
    v17_tx_init(&(s->v17tx), 14400, FALSE, fast_getbit, s);
#endif
    v29_rx_init(&(s->v29rx), 9600, fast_putbit, s);
    v29_tx_init(&(s->v29tx), 9600, FALSE, fast_getbit, s);
    v27ter_rx_init(&(s->v27ter_rx), 4800, fast_putbit, s);
    v27ter_tx_init(&(s->v27ter_tx), 4800, FALSE, fast_getbit, s);
    s->rx_signal_present = FALSE;

    s->line_ptr = 0;
    s->at_rx_mode = AT_MODE_COMMAND;
    s->originating_number = NULL;
    s->destination_number = NULL;
    s->modem = -1;
    s->transmit = -1;
    s->p = profiles[0];
    if (queue_create(&(s->rx_queue), 4096, QUEUE_WRITE_ATOMIC | QUEUE_READ_ATOMIC) < 0)
        return -1;
    s->call_control_handler = call_control_handler;
    s->call_control_user_data = call_control_user_data;
    s->at_tx_handler = at_tx_handler;
    s->at_tx_user_data = at_tx_user_data;
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * faxtester_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2005, 2006 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: fax_tester.c,v 1.5 2008/07/30 14:47:06 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "floating_fudge.h"
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#if defined(HAVE_LIBXML_XMLMEMORY_H)
#include <libxml/xmlmemory.h>
#endif
#if defined(HAVE_LIBXML_PARSER_H)
#include <libxml/parser.h>
#endif
#if defined(HAVE_LIBXML_XINCLUDE_H)
#include <libxml/xinclude.h>
#endif

#include "spandsp.h"
#include "fax_tester.h"

#define HDLC_FRAMING_OK_THRESHOLD       5

static void timer_update(faxtester_state_t *s, int len)
{
    s->timer += len;
    if (s->timer > s->timeout)
    {
        s->timeout = 0x7FFFFFFFFFFFFFFFLL;
        if (s->front_end_step_timeout_handler)
            s->front_end_step_timeout_handler(s, s->front_end_step_timeout_user_data);
    }
}
/*- End of function --------------------------------------------------------*/

static void front_end_step_complete(faxtester_state_t *s)
{
    if (s->front_end_step_complete_handler)
        s->front_end_step_complete_handler(s, s->front_end_step_complete_user_data);
}
/*- End of function --------------------------------------------------------*/

void faxtester_send_hdlc_flags(faxtester_state_t *s, int flags)
{
    hdlc_tx_flags(&(s->modems.hdlc_tx), flags);
}
/*- End of function --------------------------------------------------------*/

void faxtester_send_hdlc_msg(faxtester_state_t *s, const uint8_t *msg, int len, int crc_ok)
{
    hdlc_tx_frame(&(s->modems.hdlc_tx), msg, len);
    if (!crc_ok)
        hdlc_tx_corrupt_frame(&(s->modems.hdlc_tx));
}
/*- End of function --------------------------------------------------------*/

static void hdlc_underflow_handler(void *user_data)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    front_end_step_complete(s);
}
/*- End of function --------------------------------------------------------*/

static int modem_tx_status(void *user_data, int status)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    switch (status)
    {
    case MODEM_TX_STATUS_DATA_EXHAUSTED:
        printf("Tx data exhausted\n");
        break;
    case MODEM_TX_STATUS_SHUTDOWN_COMPLETE:
        printf("Tx shutdown complete\n");
        front_end_step_complete(s);
        break;
    default:
        printf("Tx status is %d\n", status);
        break;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int non_ecm_get_bit(void *user_data)
{
    faxtester_state_t *s;
    int bit;

    s = (faxtester_state_t *) user_data;
    if (s->image_bit_ptr == 0)
    {
        if (s->image_ptr >= s->image_len)
            return PUTBIT_END_OF_DATA;
        s->image_bit_ptr = 8;
        s->image_ptr++;
    }
    s->image_bit_ptr--;
    bit = (s->image_buffer[s->image_ptr] >> (7 - s->image_bit_ptr)) & 0x01;
    //printf("Rx bit - %d\n", bit);
    return bit;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_image_buffer(faxtester_state_t *s, const uint8_t *buf, int len)
{
    s->image_ptr = 0;
    s->image_bit_ptr = 8;
    s->image_len = len;
    s->image_buffer = buf;
}
/*- End of function --------------------------------------------------------*/

static void non_ecm_put_bit(void *user_data, int bit)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_TRAINING_IN_PROGRESS:
            break;
        case PUTBIT_TRAINING_FAILED:
            span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM carrier training failed\n");
            s->modems.rx_trained = FALSE;
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            /* The modem is now trained */
            span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM carrier trained\n");
            s->modems.rx_trained = TRUE;
            break;
        case PUTBIT_CARRIER_UP:
            span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM carrier up\n");
            s->modems.rx_signal_present = TRUE;
            break;
        case PUTBIT_CARRIER_DOWN:
            span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM carrier down\n");
            s->modems.rx_signal_present = FALSE;
            s->modems.rx_trained = FALSE;
            break;
        default:
            span_log(&s->logging, SPAN_LOG_WARNING, "Unexpected non-ECM special bit - %d!\n", bit);
            break;
        }
        return;
    }
}
/*- End of function --------------------------------------------------------*/

static void hdlc_accept(void *user_data, const uint8_t *msg, int len, int ok)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    if (len < 0)
    {
        /* Special conditions */
        switch (len)
        {
        case PUTBIT_TRAINING_IN_PROGRESS:
            break;
        case PUTBIT_TRAINING_FAILED:
            span_log(&s->logging, SPAN_LOG_FLOW, "HDLC carrier training failed\n");
            s->modems.rx_trained = FALSE;
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            /* The modem is now trained */
            span_log(&s->logging, SPAN_LOG_FLOW, "HDLC carrier trained\n");
            s->modems.rx_trained = TRUE;
            break;
        case PUTBIT_CARRIER_UP:
            span_log(&s->logging, SPAN_LOG_FLOW, "HDLC carrier up\n");
            s->modems.rx_signal_present = TRUE;
            break;
        case PUTBIT_CARRIER_DOWN:
            span_log(&s->logging, SPAN_LOG_FLOW, "HDLC carrier down\n");
            s->modems.rx_signal_present = FALSE;
            s->modems.rx_trained = FALSE;
            break;
        case PUTBIT_FRAMING_OK:
            span_log(&s->logging, SPAN_LOG_FLOW, "HDLC framing OK\n");
            break;
        case PUTBIT_ABORT:
            /* Just ignore these */
            break;
        default:
            span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected HDLC special length - %d!\n", len);
            break;
        }
        return;
    }
    if (s->real_time_frame_handler)
        s->real_time_frame_handler(s, s->real_time_frame_user_data, TRUE, msg, len);
}
/*- End of function --------------------------------------------------------*/

static int dummy_rx(void *user_data, const int16_t amp[], int len)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int early_v17_rx(void *user_data, const int16_t amp[], int len)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    v17_rx(&(s->modems.v17_rx), amp, len);
    fsk_rx(&(s->modems.v21_rx), amp, len);
    if (s->modems.rx_trained)
    {
        /* The fast modem has trained, so we no longer need to run the slow
           one in parallel. */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.17 + V.21 to V.17 (%.2fdBm0)\n", v17_rx_signal_power(&(s->modems.v17_rx)));
        s->modems.rx_handler = (span_rx_handler_t *) &v17_rx;
        s->modems.rx_user_data = &(s->modems.v17_rx);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int early_v27ter_rx(void *user_data, const int16_t amp[], int len)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    v27ter_rx(&(s->modems.v27ter_rx), amp, len);
    fsk_rx(&(s->modems.v21_rx), amp, len);
    if (s->modems.rx_trained)
    {
        /* The fast modem has trained, so we no longer need to run the slow
           one in parallel. */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.27ter + V.21 to V.27ter (%.2fdBm0)\n", v27ter_rx_signal_power(&(s->modems.v27ter_rx)));
        s->modems.rx_handler = (span_rx_handler_t *) &v27ter_rx;
        s->modems.rx_user_data = &(s->modems.v27ter_rx);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int early_v29_rx(void *user_data, const int16_t amp[], int len)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    v29_rx(&(s->modems.v29_rx), amp, len);
    fsk_rx(&(s->modems.v21_rx), amp, len);
    if (s->modems.rx_trained)
    {
        /* The fast modem has trained, so we no longer need to run the slow
           one in parallel. */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.29 + V.21 to V.29 (%.2fdBm0)\n", v29_rx_signal_power(&(s->modems.v29_rx)));
        s->modems.rx_handler = (span_rx_handler_t *) &v29_rx;
        s->modems.rx_user_data = &(s->modems.v29_rx);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int faxtester_rx(faxtester_state_t *s, int16_t *amp, int len)
{
    int i;

    for (i = 0;  i < len;  i++)
        amp[i] = dc_restore(&(s->modems.dc_restore), amp[i]);
    s->modems.rx_handler(s->modems.rx_user_data, amp, len);
    timer_update(s, len);
    if (s->wait_for_silence)
    {
        if (!s->modems.rx_signal_present)
        {
            s->wait_for_silence = FALSE;
            front_end_step_complete(s);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int faxtester_tx(faxtester_state_t *s, int16_t *amp, int max_len)
{
    int len;
    int required_len;
    
    required_len = max_len;
    len = 0;
    if (s->transmit)
    {
        while ((len += s->modems.tx_handler(s->modems.tx_user_data, amp + len, max_len - len)) < max_len)
        {
            /* Allow for a change of tx handler within a block */
            front_end_step_complete(s);
            if (!s->transmit)
            {
                if (s->modems.transmit_on_idle)
                {
                    /* Pad to the requested length with silence */
                    memset(amp + len, 0, (max_len - len)*sizeof(int16_t));
                    len = max_len;        
                }
                break;
            }
        }
    }
    else
    {
        if (s->modems.transmit_on_idle)
        {
            /* Pad to the requested length with silence */
            memset(amp, 0, max_len*sizeof(int16_t));
            len = max_len;        
        }
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

void faxtest_set_rx_silence(faxtester_state_t *s)
{
    s->wait_for_silence = TRUE;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_rx_type(void *user_data, int type, int short_train, int use_hdlc)
{
    faxtester_state_t *s;
    put_bit_func_t put_bit_func;
    void *put_bit_user_data;

    s = (faxtester_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "Set rx type %d\n", type);
    if (s->current_rx_type == type)
        return;
    s->current_rx_type = type;
    if (use_hdlc)
    {
        put_bit_func = (put_bit_func_t) hdlc_rx_put_bit;
        put_bit_user_data = (void *) &(s->modems.hdlc_rx);
        hdlc_rx_init(&(s->modems.hdlc_rx), FALSE, FALSE, HDLC_FRAMING_OK_THRESHOLD, hdlc_accept, s);
    }
    else
    {
        put_bit_func = non_ecm_put_bit;
        put_bit_user_data = (void *) s;
    }
    switch (type)
    {
    case T30_MODEM_V21:
        if (s->flush_handler)
            s->flush_handler(s, s->flush_user_data, 3);
        fsk_rx_init(&(s->modems.v21_rx), &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) hdlc_rx_put_bit, put_bit_user_data);
        fsk_rx_signal_cutoff(&(s->modems.v21_rx), -45.5);
        s->modems.rx_handler = (span_rx_handler_t *) &fsk_rx;
        s->modems.rx_user_data = &(s->modems.v21_rx);
        break;
    case T30_MODEM_V27TER_2400:
        v27ter_rx_restart(&(s->modems.v27ter_rx), 2400, FALSE);
        v27ter_rx_set_put_bit(&(s->modems.v27ter_rx), put_bit_func, put_bit_user_data);
        s->modems.rx_handler = (span_rx_handler_t *) &early_v27ter_rx;
        s->modems.rx_user_data = s;
        break;
    case T30_MODEM_V27TER_4800:
        v27ter_rx_restart(&(s->modems.v27ter_rx), 4800, FALSE);
        v27ter_rx_set_put_bit(&(s->modems.v27ter_rx), put_bit_func, put_bit_user_data);
        s->modems.rx_handler = (span_rx_handler_t *) &early_v27ter_rx;
        s->modems.rx_user_data = s;
        break;
    case T30_MODEM_V29_7200:
        v29_rx_restart(&(s->modems.v29_rx), 7200, FALSE);
        v29_rx_set_put_bit(&(s->modems.v29_rx), put_bit_func, put_bit_user_data);
        s->modems.rx_handler = (span_rx_handler_t *) &early_v29_rx;
        s->modems.rx_user_data = s;
        break;
    case T30_MODEM_V29_9600:
        v29_rx_restart(&(s->modems.v29_rx), 9600, FALSE);
        v29_rx_set_put_bit(&(s->modems.v29_rx), put_bit_func, put_bit_user_data);
        s->modems.rx_handler = (span_rx_handler_t *) &early_v29_rx;
        s->modems.rx_user_data = s;
        break;
    case T30_MODEM_V17_7200:
        v17_rx_restart(&(s->modems.v17_rx), 7200, short_train);
        v17_rx_set_put_bit(&(s->modems.v17_rx), put_bit_func, put_bit_user_data);
        s->modems.rx_handler = (span_rx_handler_t *) &early_v17_rx;
        s->modems.rx_user_data = s;
        break;
    case T30_MODEM_V17_9600:
        v17_rx_restart(&(s->modems.v17_rx), 9600, short_train);
        v17_rx_set_put_bit(&(s->modems.v17_rx), put_bit_func, put_bit_user_data);
        s->modems.rx_handler = (span_rx_handler_t *) &early_v17_rx;
        s->modems.rx_user_data = s;
        break;
    case T30_MODEM_V17_12000:
        v17_rx_restart(&(s->modems.v17_rx), 12000, short_train);
        v17_rx_set_put_bit(&(s->modems.v17_rx), put_bit_func, put_bit_user_data);
        s->modems.rx_handler = (span_rx_handler_t *) &early_v17_rx;
        s->modems.rx_user_data = s;
        break;
    case T30_MODEM_V17_14400:
        v17_rx_restart(&(s->modems.v17_rx), 14400, short_train);
        v17_rx_set_put_bit(&(s->modems.v17_rx), put_bit_func, put_bit_user_data);
        s->modems.rx_handler = (span_rx_handler_t *) &early_v17_rx;
        s->modems.rx_user_data = s;
        break;
    case T30_MODEM_DONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "FAX exchange complete\n");
    default:
        s->modems.rx_handler = (span_rx_handler_t *) &dummy_rx;
        s->modems.rx_user_data = s;
        break;
    }
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_tx_type(void *user_data, int type, int short_train, int use_hdlc)
{
    faxtester_state_t *s;
    tone_gen_descriptor_t tone_desc;
    get_bit_func_t get_bit_func;
    void *get_bit_user_data;

    s = (faxtester_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "Set tx type %d\n", type);
    if (s->current_tx_type == type)
        return;
    if (use_hdlc)
    {
        get_bit_func = (get_bit_func_t) hdlc_tx_get_bit;
        get_bit_user_data = (void *) &(s->modems.hdlc_tx);
    }
    else
    {
        get_bit_func = non_ecm_get_bit;
        get_bit_user_data = (void *) s;
    }
    switch (type)
    {
    case T30_MODEM_PAUSE:
        silence_gen_alter(&(s->modems.silence_gen), ms_to_samples(short_train));
        s->modems.tx_handler = (span_tx_handler_t *) &silence_gen;
        s->modems.tx_user_data = &(s->modems.silence_gen);
        s->transmit = TRUE;
        break;
    case T30_MODEM_CNG:
        /* 0.5s of 1100Hz+-38Hz + 3.0s of silence repeating. Timing +-15% */
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
        tone_gen_init(&(s->modems.tone_gen), &tone_desc);
        s->modems.tx_handler = (span_tx_handler_t *) &tone_gen;
        s->modems.tx_user_data = &(s->modems.tone_gen);
        s->transmit = TRUE;
        break;
    case T30_MODEM_CED:
        /* 0.2s of silence, then 2.6s to 4s of 2100Hz+-15Hz tone, then 75ms of silence. The 75ms of silence
           will be inserted by the pre V.21 pause we use for any switch to V.21. */
        silence_gen_alter(&(s->modems.silence_gen), ms_to_samples(200));
        make_tone_gen_descriptor(&tone_desc,
                                 2100,
                                 -11,
                                 0,
                                 0,
                                 2600,
                                 0,
                                 0,
                                 0,
                                 FALSE);
        tone_gen_init(&(s->modems.tone_gen), &tone_desc);
        s->modems.tx_handler = (span_tx_handler_t *) &tone_gen;
        s->modems.tx_user_data = &(s->modems.tone_gen);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V21:
        fsk_tx_init(&(s->modems.v21_tx), &preset_fsk_specs[FSK_V21CH2], get_bit_func, get_bit_user_data);
        fsk_tx_set_modem_status_handler(&(s->modems.v21_tx), modem_tx_status, (void *) s);
        s->modems.tx_handler = (span_tx_handler_t *) &fsk_tx;
        s->modems.tx_user_data = &(s->modems.v21_tx);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V27TER_2400:
        v27ter_tx_restart(&(s->modems.v27ter_tx), 2400, s->modems.use_tep);
        v27ter_tx_set_get_bit(&(s->modems.v27ter_tx), get_bit_func, get_bit_user_data);
        v27ter_tx_set_modem_status_handler(&(s->modems.v27ter_tx), modem_tx_status, (void *) s);
        s->modems.tx_handler = (span_tx_handler_t *) &v27ter_tx;
        s->modems.tx_user_data = &(s->modems.v27ter_tx);
        hdlc_tx_flags(&(s->modems.hdlc_tx), 60);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V27TER_4800:
        v27ter_tx_restart(&(s->modems.v27ter_tx), 4800, s->modems.use_tep);
        v27ter_tx_set_get_bit(&(s->modems.v27ter_tx), get_bit_func, get_bit_user_data);
        v27ter_tx_set_modem_status_handler(&(s->modems.v27ter_tx), modem_tx_status, (void *) s);
        s->modems.tx_handler = (span_tx_handler_t *) &v27ter_tx;
        s->modems.tx_user_data = &(s->modems.v27ter_tx);
        hdlc_tx_flags(&(s->modems.hdlc_tx), 120);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V29_7200:
        v29_tx_restart(&(s->modems.v29_tx), 7200, s->modems.use_tep);
        v29_tx_set_get_bit(&(s->modems.v29_tx), get_bit_func, get_bit_user_data);
        v29_tx_set_modem_status_handler(&(s->modems.v29_tx), modem_tx_status, (void *) s);
        s->modems.tx_handler = (span_tx_handler_t *) &v29_tx;
        s->modems.tx_user_data = &(s->modems.v29_tx);
        hdlc_tx_flags(&(s->modems.hdlc_tx), 180);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V29_9600:
        v29_tx_restart(&(s->modems.v29_tx), 9600, s->modems.use_tep);
        v29_tx_set_get_bit(&(s->modems.v29_tx), get_bit_func, get_bit_user_data);
        v29_tx_set_modem_status_handler(&(s->modems.v29_tx), modem_tx_status, (void *) s);
        s->modems.tx_handler = (span_tx_handler_t *) &v29_tx;
        s->modems.tx_user_data = &(s->modems.v29_tx);
        hdlc_tx_flags(&(s->modems.hdlc_tx), 240);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V17_7200:
        v17_tx_restart(&(s->modems.v17_tx), 7200, s->modems.use_tep, short_train);
        v17_tx_set_get_bit(&(s->modems.v17_tx), get_bit_func, get_bit_user_data);
        v17_tx_set_modem_status_handler(&(s->modems.v17_tx), modem_tx_status, (void *) s);
        s->modems.tx_handler = (span_tx_handler_t *) &v17_tx;
        s->modems.tx_user_data = &(s->modems.v17_tx);
        hdlc_tx_flags(&(s->modems.hdlc_tx), 180);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V17_9600:
        v17_tx_restart(&(s->modems.v17_tx), 9600, s->modems.use_tep, short_train);
        v17_tx_set_get_bit(&(s->modems.v17_tx), get_bit_func, get_bit_user_data);
        v17_tx_set_modem_status_handler(&(s->modems.v17_tx), modem_tx_status, (void *) s);
        s->modems.tx_handler = (span_tx_handler_t *) &v17_tx;
        s->modems.tx_user_data = &(s->modems.v17_tx);
        hdlc_tx_flags(&(s->modems.hdlc_tx), 240);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V17_12000:
        v17_tx_restart(&(s->modems.v17_tx), 12000, s->modems.use_tep, short_train);
        v17_tx_set_get_bit(&(s->modems.v17_tx), get_bit_func, get_bit_user_data);
        v17_tx_set_modem_status_handler(&(s->modems.v17_tx), modem_tx_status, (void *) s);
        s->modems.tx_handler = (span_tx_handler_t *) &v17_tx;
        s->modems.tx_user_data = &(s->modems.v17_tx);
        hdlc_tx_flags(&(s->modems.hdlc_tx), 300);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V17_14400:
        v17_tx_restart(&(s->modems.v17_tx), 14400, s->modems.use_tep, short_train);
        v17_tx_set_get_bit(&(s->modems.v17_tx), get_bit_func, get_bit_user_data);
        v17_tx_set_modem_status_handler(&(s->modems.v17_tx), modem_tx_status, (void *) s);
        s->modems.tx_handler = (span_tx_handler_t *) &v17_tx;
        s->modems.tx_user_data = &(s->modems.v17_tx);
        hdlc_tx_flags(&(s->modems.hdlc_tx), 360);
        s->transmit = TRUE;
        break;
    case T30_MODEM_DONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "FAX exchange complete\n");
        /* Fall through */
    default:
        silence_gen_alter(&(s->modems.silence_gen), 0);
        s->modems.tx_handler = (span_tx_handler_t *) &silence_gen;
        s->modems.tx_user_data = &(s->modems.silence_gen);
        s->transmit = FALSE;
        break;
    }
    s->current_tx_type = type;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_timeout(faxtester_state_t *s, int timeout)
{
    if (timeout >= 0)
        s->timeout = s->timer + timeout*SAMPLE_RATE/1000;
    else
        s->timeout = 0x7FFFFFFFFFFFFFFFLL;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_transmit_on_idle(faxtester_state_t *s, int transmit_on_idle)
{
    s->modems.transmit_on_idle = transmit_on_idle;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_tep_mode(faxtester_state_t *s, int use_tep)
{
    s->modems.use_tep = use_tep;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_real_time_frame_handler(faxtester_state_t *s, faxtester_real_time_frame_handler_t *handler, void *user_data)
{
    s->real_time_frame_handler = handler;
    s->real_time_frame_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_front_end_step_complete_handler(faxtester_state_t *s, faxtester_front_end_step_complete_handler_t *handler, void *user_data)
{
    s->front_end_step_complete_handler = handler;
    s->front_end_step_complete_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_front_end_step_timeout_handler(faxtester_state_t *s, faxtester_front_end_step_complete_handler_t *handler, void *user_data)
{
    s->front_end_step_timeout_handler = handler;
    s->front_end_step_timeout_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

faxtester_state_t *faxtester_init(faxtester_state_t *s, int calling_party)
{
    if (s == NULL)
    {
        if ((s = (faxtester_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }

    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "FAX");
    hdlc_rx_init(&(s->modems.hdlc_rx), FALSE, FALSE, HDLC_FRAMING_OK_THRESHOLD, hdlc_accept, &s);
    fsk_rx_init(&(s->modems.v21_rx), &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) hdlc_rx_put_bit, &(s->modems.hdlc_rx));
    fsk_rx_signal_cutoff(&(s->modems.v21_rx), -45.5);
    hdlc_tx_init(&(s->modems.hdlc_tx), FALSE, 2, FALSE, hdlc_underflow_handler, s);
    fsk_tx_init(&(s->modems.v21_tx), &preset_fsk_specs[FSK_V21CH2], (get_bit_func_t) hdlc_tx_get_bit, &(s->modems.hdlc_tx));
    fsk_tx_set_modem_status_handler(&(s->modems.v21_tx), modem_tx_status, (void *) s);
    v17_rx_init(&(s->modems.v17_rx), 14400, non_ecm_put_bit, s);
    v17_tx_init(&(s->modems.v17_tx), 14400, s->modems.use_tep, non_ecm_get_bit, s);
    v17_tx_set_modem_status_handler(&(s->modems.v17_tx), modem_tx_status, (void *) s);
    v29_rx_init(&(s->modems.v29_rx), 9600, non_ecm_put_bit, s);
    v29_rx_signal_cutoff(&(s->modems.v29_rx), -45.5);
    v29_tx_init(&(s->modems.v29_tx), 9600, s->modems.use_tep, non_ecm_get_bit, s);
    v29_tx_set_modem_status_handler(&(s->modems.v29_tx), modem_tx_status, (void *) s);
    v27ter_rx_init(&(s->modems.v27ter_rx), 4800, non_ecm_put_bit, s);
    v27ter_tx_init(&(s->modems.v27ter_tx), 4800, s->modems.use_tep, non_ecm_get_bit, s);
    v27ter_tx_set_modem_status_handler(&(s->modems.v27ter_tx), modem_tx_status, (void *) s);
    silence_gen_init(&(s->modems.silence_gen), 0);
    dc_restore_init(&(s->modems.dc_restore));
    s->modems.rx_handler = (span_rx_handler_t *) &dummy_rx;
    s->modems.rx_user_data = NULL;
    faxtester_set_timeout(s, -1);
    faxtester_set_tx_type(s, T30_MODEM_NONE, FALSE, FALSE);

    return s;
}
/*- End of function --------------------------------------------------------*/

int faxtester_release(faxtester_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

int faxtester_free(faxtester_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_flush_handler(faxtester_state_t *s, faxtester_flush_handler_t *handler, void *user_data)
{
    s->flush_handler = handler;
    s->flush_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
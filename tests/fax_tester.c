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
 * $Id: fax_tester.c,v 1.1 2008/07/15 14:28:20 steveu Exp $
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
#include <audiofile.h>

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
}
/*- End of function --------------------------------------------------------*/

static void front_end_step_complete(faxtester_state_t *s)
{
}
/*- End of function --------------------------------------------------------*/

static void send_hdlc(void *user_data, const uint8_t *msg, int len)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    
    hdlc_tx_frame(&(s->hdlctx), msg, len);
}
/*- End of function --------------------------------------------------------*/

static void hdlc_underflow_handler(void *user_data)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    t30_front_end_status(s, T30_FRONT_END_SEND_STEP_COMPLETE);
}
/*- End of function --------------------------------------------------------*/

static int non_ecm_get_bit(void *user_data)
{
    return 0;
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
            s->rx_trained = FALSE;
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            /* The modem is now trained */
            span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM carrier trained\n");
            s->rx_trained = TRUE;
            break;
        case PUTBIT_CARRIER_UP:
            span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM carrier down\n");
            s->rx_trained = FALSE;
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
            s->rx_trained = FALSE;
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            /* The modem is now trained */
            span_log(&s->logging, SPAN_LOG_FLOW, "HDLC carrier trained\n");
            s->rx_trained = TRUE;
            break;
        case PUTBIT_CARRIER_UP:
            span_log(&s->logging, SPAN_LOG_FLOW, "HDLC carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            span_log(&s->logging, SPAN_LOG_FLOW, "HDLC carrier down\n");
            s->rx_trained = FALSE;
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
    v17_rx(&(s->v17_rx), amp, len);
    fsk_rx(&(s->v21_rx), amp, len);
    if (s->rx_trained)
    {
        /* The fast modem has trained, so we no longer need to run the slow
           one in parallel. */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.17 + V.21 to V.17 (%.2fdBm0)\n", v17_rx_signal_power(&(s->v17_rx)));
        s->rx_handler = (span_rx_handler_t *) &v17_rx;
        s->rx_user_data = &(s->v17_rx);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int early_v27ter_rx(void *user_data, const int16_t amp[], int len)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    v27ter_rx(&(s->v27ter_rx), amp, len);
    fsk_rx(&(s->v21_rx), amp, len);
    if (s->rx_trained)
    {
        /* The fast modem has trained, so we no longer need to run the slow
           one in parallel. */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.27ter + V.21 to V.27ter (%.2fdBm0)\n", v27ter_rx_signal_power(&(s->v27ter_rx)));
        s->rx_handler = (span_rx_handler_t *) &v27ter_rx;
        s->rx_user_data = &(s->v27ter_rx);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int early_v29_rx(void *user_data, const int16_t amp[], int len)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    v29_rx(&(s->v29_rx), amp, len);
    fsk_rx(&(s->v21_rx), amp, len);
    if (s->rx_trained)
    {
        /* The fast modem has trained, so we no longer need to run the slow
           one in parallel. */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.29 + V.21 to V.29 (%.2fdBm0)\n", v29_rx_signal_power(&(s->v29_rx)));
        s->rx_handler = (span_rx_handler_t *) &v29_rx;
        s->rx_user_data = &(s->v29_rx);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int faxtester_rx(faxtester_state_t *s, int16_t *amp, int len)
{
    int i;

    if (s->audio_rx_log >= 0)
        write(s->audio_rx_log, amp, len*sizeof(int16_t));
    for (i = 0;  i < len;  i++)
        amp[i] = dc_restore(&(s->dc_restore), amp[i]);
    s->rx_handler(s->rx_user_data, amp, len);
    timer_update(s, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int set_next_tx_type(faxtester_state_t *s)
{
    if (s->next_tx_handler)
    {
        s->tx_handler = s->next_tx_handler;
        s->tx_user_data = s->next_tx_user_data;
        s->next_tx_handler = NULL;
        return 0;
    }
    /* If there is nothing else to change to, so use zero length silence */
    silence_gen_alter(&(s->silence_gen), 0);
    s->tx_handler = (span_tx_handler_t *) &silence_gen;
    s->tx_user_data = &(s->silence_gen);
    s->next_tx_handler = NULL;
    s->transmit = FALSE;
    return -1;
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
        while ((len += s->tx_handler(s->tx_user_data, amp + len, max_len - len)) < max_len)
        {
            /* Allow for a change of tx handler within a block */
            if (set_next_tx_type(s)  &&  s->current_tx_type != T30_MODEM_NONE  &&  s->current_tx_type != T30_MODEM_DONE)
                front_end_step_complete(s);
            if (!s->transmit)
            {
                if (s->transmit_on_idle)
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
        if (s->transmit_on_idle)
        {
            /* Pad to the requested length with silence */
            memset(amp, 0, max_len*sizeof(int16_t));
            len = max_len;        
        }
    }
    if (s->audio_tx_log >= 0)
    {
        if (len < required_len)
            memset(amp + len, 0, (required_len - len)*sizeof(int16_t));
        write(s->audio_tx_log, amp, required_len*sizeof(int16_t));
    }
    return len;
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
        put_bit_user_data = (void *) &(s->hdlcrx);
        hdlc_rx_init(&(s->hdlcrx), FALSE, FALSE, HDLC_FRAMING_OK_THRESHOLD, hdlc_accept, s);
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
        fsk_rx_init(&(s->v21_rx), &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) hdlc_rx_put_bit, put_bit_user_data);
        fsk_rx_signal_cutoff(&(s->v21_rx), -45.5);
        s->rx_handler = (span_rx_handler_t *) &fsk_rx;
        s->rx_user_data = &(s->v21_rx);
        break;
    case T30_MODEM_V27TER_2400:
        v27ter_rx_restart(&(s->v27ter_rx), 2400, FALSE);
        v27ter_rx_set_put_bit(&(s->v27ter_rx), put_bit_func, put_bit_user_data);
        s->rx_handler = (span_rx_handler_t *) &early_v27ter_rx;
        s->rx_user_data = s;
        break;
    case T30_MODEM_V27TER_4800:
        v27ter_rx_restart(&(s->v27ter_rx), 4800, FALSE);
        v27ter_rx_set_put_bit(&(s->v27ter_rx), put_bit_func, put_bit_user_data);
        s->rx_handler = (span_rx_handler_t *) &early_v27ter_rx;
        s->rx_user_data = s;
        break;
    case T30_MODEM_V29_7200:
        v29_rx_restart(&(s->v29_rx), 7200, FALSE);
        v29_rx_set_put_bit(&(s->v29_rx), put_bit_func, put_bit_user_data);
        s->rx_handler = (span_rx_handler_t *) &early_v29_rx;
        s->rx_user_data = s;
        break;
    case T30_MODEM_V29_9600:
        v29_rx_restart(&(s->v29_rx), 9600, FALSE);
        v29_rx_set_put_bit(&(s->v29_rx), put_bit_func, put_bit_user_data);
        s->rx_handler = (span_rx_handler_t *) &early_v29_rx;
        s->rx_user_data = s;
        break;
    case T30_MODEM_V17_7200:
        v17_rx_restart(&(s->v17_rx), 7200, short_train);
        v17_rx_set_put_bit(&(s->v17_rx), put_bit_func, put_bit_user_data);
        s->rx_handler = (span_rx_handler_t *) &early_v17_rx;
        s->rx_user_data = s;
        break;
    case T30_MODEM_V17_9600:
        v17_rx_restart(&(s->v17_rx), 9600, short_train);
        v17_rx_set_put_bit(&(s->v17_rx), put_bit_func, put_bit_user_data);
        s->rx_handler = (span_rx_handler_t *) &early_v17_rx;
        s->rx_user_data = s;
        break;
    case T30_MODEM_V17_12000:
        v17_rx_restart(&(s->v17_rx), 12000, short_train);
        v17_rx_set_put_bit(&(s->v17_rx), put_bit_func, put_bit_user_data);
        s->rx_handler = (span_rx_handler_t *) &early_v17_rx;
        s->rx_user_data = s;
        break;
    case T30_MODEM_V17_14400:
        v17_rx_restart(&(s->v17_rx), 14400, short_train);
        v17_rx_set_put_bit(&(s->v17_rx), put_bit_func, put_bit_user_data);
        s->rx_handler = (span_rx_handler_t *) &early_v17_rx;
        s->rx_user_data = s;
        break;
    case T30_MODEM_DONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "FAX exchange complete\n");
    default:
        s->rx_handler = (span_rx_handler_t *) &dummy_rx;
        s->rx_user_data = s;
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
        get_bit_user_data = (void *) &(s->hdlctx);
    }
    else
    {
        get_bit_func = non_ecm_get_bit;
        get_bit_user_data = (void *) &s;
    }
    switch (type)
    {
    case T30_MODEM_PAUSE:
        silence_gen_alter(&(s->silence_gen), ms_to_samples(short_train));
        s->tx_handler = (span_tx_handler_t *) &silence_gen;
        s->tx_user_data = &(s->silence_gen);
        s->next_tx_handler = NULL;
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
        tone_gen_init(&(s->tone_gen), &tone_desc);
        s->tx_handler = (span_tx_handler_t *) &tone_gen;
        s->tx_user_data = &(s->tone_gen);
        s->next_tx_handler = NULL;
        s->transmit = TRUE;
        break;
    case T30_MODEM_CED:
        /* 0.2s of silence, then 2.6s to 4s of 2100Hz+-15Hz tone, then 75ms of silence. The 75ms of silence
           will be inserted by the pre V.21 pause we use for any switch to V.21. */
        silence_gen_alter(&(s->silence_gen), ms_to_samples(200));
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
        tone_gen_init(&(s->tone_gen), &tone_desc);
        s->tx_handler = (span_tx_handler_t *) &silence_gen;
        s->tx_user_data = &(s->silence_gen);
        s->next_tx_handler = (span_tx_handler_t *) &tone_gen;
        s->next_tx_user_data = &(s->tone_gen);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V21:
        fsk_tx_init(&(s->v21_tx), &preset_fsk_specs[FSK_V21CH2], get_bit_func, get_bit_user_data);
        s->tx_handler = (span_tx_handler_t *) &fsk_tx;
        s->tx_user_data = &(s->v21_tx);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V27TER_2400:
        silence_gen_alter(&(s->silence_gen), ms_to_samples(75));
        v27ter_tx_restart(&(s->v27ter_tx), 2400, s->use_tep);
        v27ter_tx_set_get_bit(&(s->v27ter_tx), get_bit_func, get_bit_user_data);
        s->tx_handler = (span_tx_handler_t *) &silence_gen;
        s->tx_user_data = &(s->silence_gen);
        s->next_tx_handler = (span_tx_handler_t *) &v27ter_tx;
        s->next_tx_user_data = &(s->v27ter_tx);
        hdlc_tx_flags(&(s->hdlctx), 60);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V27TER_4800:
        silence_gen_alter(&(s->silence_gen), ms_to_samples(75));
        v27ter_tx_restart(&(s->v27ter_tx), 4800, s->use_tep);
        v27ter_tx_set_get_bit(&(s->v27ter_tx), get_bit_func, get_bit_user_data);
        s->tx_handler = (span_tx_handler_t *) &silence_gen;
        s->tx_user_data = &(s->silence_gen);
        s->next_tx_handler = (span_tx_handler_t *) &v27ter_tx;
        s->next_tx_user_data = &(s->v27ter_tx);
        hdlc_tx_flags(&(s->hdlctx), 120);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V29_7200:
        silence_gen_alter(&(s->silence_gen), ms_to_samples(75));
        v29_tx_restart(&(s->v29_tx), 7200, s->use_tep);
        v29_tx_set_get_bit(&(s->v29_tx), get_bit_func, get_bit_user_data);
        s->tx_handler = (span_tx_handler_t *) &silence_gen;
        s->tx_user_data = &(s->silence_gen);
        s->next_tx_handler = (span_tx_handler_t *) &v29_tx;
        s->next_tx_user_data = &(s->v29_tx);
        hdlc_tx_flags(&(s->hdlctx), 180);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V29_9600:
        silence_gen_alter(&(s->silence_gen), ms_to_samples(75));
        v29_tx_restart(&(s->v29_tx), 9600, s->use_tep);
        v29_tx_set_get_bit(&(s->v29_tx), get_bit_func, get_bit_user_data);
        s->tx_handler = (span_tx_handler_t *) &silence_gen;
        s->tx_user_data = &(s->silence_gen);
        s->next_tx_handler = (span_tx_handler_t *) &v29_tx;
        s->next_tx_user_data = &(s->v29_tx);
        hdlc_tx_flags(&(s->hdlctx), 240);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V17_7200:
        silence_gen_alter(&(s->silence_gen), ms_to_samples(75));
        v17_tx_restart(&(s->v17_tx), 7200, s->use_tep, short_train);
        v17_tx_set_get_bit(&(s->v17_tx), get_bit_func, get_bit_user_data);
        s->tx_handler = (span_tx_handler_t *) &silence_gen;
        s->tx_user_data = &(s->silence_gen);
        s->next_tx_handler = (span_tx_handler_t *) &v17_tx;
        s->next_tx_user_data = &(s->v17_tx);
        hdlc_tx_flags(&(s->hdlctx), 180);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V17_9600:
        silence_gen_alter(&(s->silence_gen), ms_to_samples(75));
        v17_tx_restart(&(s->v17_tx), 9600, s->use_tep, short_train);
        v17_tx_set_get_bit(&(s->v17_tx), get_bit_func, get_bit_user_data);
        s->tx_handler = (span_tx_handler_t *) &silence_gen;
        s->tx_user_data = &(s->silence_gen);
        s->next_tx_handler = (span_tx_handler_t *) &v17_tx;
        s->next_tx_user_data = &(s->v17_tx);
        hdlc_tx_flags(&(s->hdlctx), 240);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V17_12000:
        silence_gen_alter(&(s->silence_gen), ms_to_samples(75));
        v17_tx_restart(&(s->v17_tx), 12000, s->use_tep, short_train);
        v17_tx_set_get_bit(&(s->v17_tx), get_bit_func, get_bit_user_data);
        s->tx_handler = (span_tx_handler_t *) &silence_gen;
        s->tx_user_data = &(s->silence_gen);
        s->next_tx_handler = (span_tx_handler_t *) &v17_tx;
        s->next_tx_user_data = &(s->v17_tx);
        hdlc_tx_flags(&(s->hdlctx), 300);
        s->transmit = TRUE;
        break;
    case T30_MODEM_V17_14400:
        silence_gen_alter(&(s->silence_gen), ms_to_samples(75));
        v17_tx_restart(&(s->v17_tx), 14400, s->use_tep, short_train);
        v17_tx_set_get_bit(&(s->v17_tx), get_bit_func, get_bit_user_data);
        s->tx_handler = (span_tx_handler_t *) &silence_gen;
        s->tx_user_data = &(s->silence_gen);
        s->next_tx_handler = (span_tx_handler_t *) &v17_tx;
        s->next_tx_user_data = &(s->v17_tx);
        hdlc_tx_flags(&(s->hdlctx), 360);
        s->transmit = TRUE;
        break;
    case T30_MODEM_DONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "FAX exchange complete\n");
        /* Fall through */
    default:
        silence_gen_alter(&(s->silence_gen), 0);
        s->tx_handler = (span_tx_handler_t *) &silence_gen;
        s->tx_user_data = &(s->silence_gen);
        s->next_tx_handler = NULL;
        s->transmit = FALSE;
        break;
    }
    s->current_tx_type = type;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_transmit_on_idle(faxtester_state_t *s, int transmit_on_idle)
{
    s->transmit_on_idle = transmit_on_idle;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_tep_mode(faxtester_state_t *s, int use_tep)
{
    s->use_tep = use_tep;
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
    hdlc_rx_init(&(s->hdlcrx), FALSE, FALSE, HDLC_FRAMING_OK_THRESHOLD, hdlc_accept, &s);
    fsk_rx_init(&(s->v21_rx), &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) hdlc_rx_put_bit, &(s->hdlcrx));
    fsk_rx_signal_cutoff(&(s->v21_rx), -45.5);
    hdlc_tx_init(&(s->hdlctx), FALSE, 2, FALSE, hdlc_underflow_handler, s);
    fsk_tx_init(&(s->v21_tx), &preset_fsk_specs[FSK_V21CH2], (get_bit_func_t) hdlc_tx_get_bit, &(s->hdlctx));
    v17_rx_init(&(s->v17_rx), 14400, non_ecm_put_bit, s);
    v17_tx_init(&(s->v17_tx), 14400, s->use_tep, non_ecm_get_bit, s);
    v29_rx_init(&(s->v29_rx), 9600, non_ecm_put_bit, s);
    v29_rx_signal_cutoff(&(s->v29_rx), -45.5);
    v29_tx_init(&(s->v29_tx), 9600, s->use_tep, non_ecm_get_bit, s);
    v27ter_rx_init(&(s->v27ter_rx), 4800, non_ecm_put_bit, s);
    v27ter_tx_init(&(s->v27ter_tx), 4800, s->use_tep, non_ecm_get_bit, s);
    silence_gen_init(&(s->silence_gen), 0);
    dc_restore_init(&(s->dc_restore));
    s->rx_handler = (span_rx_handler_t *) &dummy_rx;
    s->rx_user_data = NULL;
    faxtester_set_tx_type(s, T30_MODEM_NONE, FALSE, FALSE);

    {
        char buf[100 + 1];
        struct tm *tm;
        time_t now;

        time(&now);
        tm = localtime(&now);
        sprintf(buf,
                "/tmp/fax-rx-audio-%p-%02d%02d%02d%02d%02d%02d",
                s,
                tm->tm_year%100,
                tm->tm_mon + 1,
                tm->tm_mday,
                tm->tm_hour,
                tm->tm_min,
                tm->tm_sec);
        s->audio_rx_log = open(buf, O_CREAT | O_TRUNC | O_WRONLY, 0666);
        sprintf(buf,
                "/tmp/fax-tx-audio-%p-%02d%02d%02d%02d%02d%02d",
                s,
                tm->tm_year%100,
                tm->tm_mon + 1,
                tm->tm_mday,
                tm->tm_hour,
                tm->tm_min,
                tm->tm_sec);
        s->audio_tx_log = open(buf, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    }
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

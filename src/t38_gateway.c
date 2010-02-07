//#define LOG_FAX_AUDIO
/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t38_gateway.c - A T.38 gateway, less the packet exchange part
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005, 2006, 2007 Steve Underwood
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
 * $Id: t38_gateway.c,v 1.129 2008/07/25 13:56:54 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include "floating_fudge.h"
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include <assert.h>
#if defined(LOG_FAX_AUDIO)
#include <unistd.h>
#endif
#include <tiffio.h>

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/queue.h"
#include "spandsp/dc_restore.h"
#include "spandsp/bit_operations.h"
#include "spandsp/power_meter.h"
#include "spandsp/complex.h"
#include "spandsp/tone_generate.h"
#include "spandsp/async.h"
#include "spandsp/crc.h"
#include "spandsp/hdlc.h"
#include "spandsp/silence_gen.h"
#include "spandsp/fsk.h"
#include "spandsp/v29rx.h"
#include "spandsp/v29tx.h"
#include "spandsp/v27ter_rx.h"
#include "spandsp/v27ter_tx.h"
#include "spandsp/v17rx.h"
#include "spandsp/v17tx.h"
#include "spandsp/t4.h"
#include "spandsp/t30_fcf.h"
#include "spandsp/t35.h"
#include "spandsp/t30.h"
#include "spandsp/t30_logging.h"

#include "spandsp/fax_modems.h"
#include "spandsp/t38_core.h"
#include "spandsp/t38_gateway.h"

/* This is the target time per transmission chunk. The actual
   packet timing will sync to the data octets. */
#define MS_PER_TX_CHUNK                 30
#define HDLC_START_BUFFER_LEVEL         8

#define INDICATOR_TX_COUNT              3
#define DATA_TX_COUNT                   1
#define DATA_END_TX_COUNT               3

enum
{
    DISBIT1 = 0x01,
    DISBIT2 = 0x02,
    DISBIT3 = 0x04,
    DISBIT4 = 0x08,
    DISBIT5 = 0x10,
    DISBIT6 = 0x20,
    DISBIT7 = 0x40,
    DISBIT8 = 0x80
};

enum
{
    T38_NONE,
    T38_V27TER_RX,
    T38_V29_RX,
    T38_V17_RX
};

enum
{
    HDLC_FLAG_FINISHED = 0x01,
    HDLC_FLAG_CORRUPT_CRC = 0x02,
    HDLC_FLAG_PROCEED_WITH_OUTPUT = 0x04,
    HDLC_FLAG_MISSING_DATA = 0x08
};

enum
{
    FLAG_INDICATOR = 0x100,
    FLAG_DATA = 0x200
};

#define MAX_NSX_SUPPRESSION     10

static uint8_t nsx_overwrite[2][MAX_NSX_SUPPRESSION] =
{
    {0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static int restart_rx_modem(t38_gateway_state_t *s);
static void add_to_non_ecm_modem_buffer(t38_gateway_state_t *s, const uint8_t *buf, int len);
static int non_ecm_get_bit(void *user_data);
static int process_rx_indicator(t38_core_state_t *t, void *user_data, int indicator);

static int dummy_rx(void *user_data, const int16_t amp[], int len)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void set_rx_handler(t38_gateway_state_t *s, span_rx_handler_t *handler, void *user_data)
{
    if (s->audio.immediate_rx_handler != dummy_rx)
        s->audio.immediate_rx_handler = handler;
    s->audio.rx_handler = handler;
    s->audio.rx_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

static void set_rx_active(t38_gateway_state_t *s, int active)
{
    s->audio.immediate_rx_handler = (active)  ?  s->audio.rx_handler  :  dummy_rx;
}
/*- End of function --------------------------------------------------------*/

static int early_v17_rx(void *user_data, const int16_t amp[], int len)
{
    t38_gateway_state_t *s;

    s = (t38_gateway_state_t *) user_data;
    v17_rx(&(s->audio.modems.v17_rx), amp, len);
    fsk_rx(&(s->audio.modems.v21_rx), amp, len);
    if (s->audio.rx_signal_present)
    {
        if (s->audio.rx_trained)
        {
            /* The fast modem has trained, so we no longer need to run the slow
               one in parallel. */
            span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.17 + V.21 to V.17 (%.2fdBm0)\n", v17_rx_signal_power(&(s->audio.modems.v17_rx)));
            set_rx_handler(s, (span_rx_handler_t *) &v17_rx, &(s->audio.modems.v17_rx));
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.17 + V.21 to V.21 (%.2fdBm0)\n", fsk_rx_signal_power(&(s->audio.modems.v21_rx)));
            set_rx_handler(s, (span_rx_handler_t *) &fsk_rx, &(s->audio.modems.v21_rx));
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int early_v27ter_rx(void *user_data, const int16_t amp[], int len)
{
    t38_gateway_state_t *s;

    s = (t38_gateway_state_t *) user_data;
    v27ter_rx(&(s->audio.modems.v27ter_rx), amp, len);
    fsk_rx(&(s->audio.modems.v21_rx), amp, len);
    if (s->audio.rx_signal_present)
    {
        if (s->audio.rx_trained)
        {
            /* The fast modem has trained, so we no longer need to run the slow
               one in parallel. */
            span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.27ter + V.21 to V.27ter (%.2fdBm0)\n", v27ter_rx_signal_power(&(s->audio.modems.v27ter_rx)));
            set_rx_handler(s, (span_rx_handler_t *) &v27ter_rx, &(s->audio.modems.v27ter_rx));
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.27ter + V.21 to V.21 (%.2fdBm0)\n", fsk_rx_signal_power(&(s->audio.modems.v21_rx)));
            set_rx_handler(s, (span_rx_handler_t *) &fsk_rx, &(s->audio.modems.v21_rx));
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int early_v29_rx(void *user_data, const int16_t amp[], int len)
{
    t38_gateway_state_t *s;

    s = (t38_gateway_state_t *) user_data;
    v29_rx(&(s->audio.modems.v29_rx), amp, len);
    fsk_rx(&(s->audio.modems.v21_rx), amp, len);
    if (s->audio.rx_signal_present)
    {
        if (s->audio.rx_trained)
        {
            /* The fast modem has trained, so we no longer need to run the slow
               one in parallel. */
            span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.29 + V.21 to V.29 (%.2fdBm0)\n", v29_rx_signal_power(&(s->audio.modems.v29_rx)));
            set_rx_handler(s, (span_rx_handler_t *) &v29_rx, &(s->audio.modems.v29_rx));
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.29 + V.21 to V.21 (%.2fdBm0)\n", fsk_rx_signal_power(&(s->audio.modems.v21_rx)));
            set_rx_handler(s, (span_rx_handler_t *) &fsk_rx, &(s->audio.modems.v21_rx));
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void hdlc_underflow_handler(void *user_data)
{
    t38_gateway_state_t *s;
    int old_data_type;
    
    s = (t38_gateway_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "HDLC underflow at %d\n", s->hdlc_to_modem.out);
    /* If the current HDLC buffer is not at the HDLC_FLAG_PROCEED_WITH_OUTPUT stage, this
       underflow must be an end of preamble condition. */
    if ((s->hdlc_to_modem.flags[s->hdlc_to_modem.out] & HDLC_FLAG_PROCEED_WITH_OUTPUT))
    {
        old_data_type = s->hdlc_to_modem.contents[s->hdlc_to_modem.out];
        s->hdlc_to_modem.len[s->hdlc_to_modem.out] = 0;
        s->hdlc_to_modem.flags[s->hdlc_to_modem.out] = 0;
        s->hdlc_to_modem.contents[s->hdlc_to_modem.out] = 0;
        if (++s->hdlc_to_modem.out >= T38_TX_HDLC_BUFS)
            s->hdlc_to_modem.out = 0;
        span_log(&s->logging, SPAN_LOG_FLOW, "HDLC next is 0x%X\n", s->hdlc_to_modem.contents[s->hdlc_to_modem.out]);
        if ((s->hdlc_to_modem.contents[s->hdlc_to_modem.out] & FLAG_INDICATOR))
        {
            /* The next thing in the queue is an indicator, so we need to stop this modem. */
            span_log(&s->logging, SPAN_LOG_FLOW, "HDLC shutdown\n");
            hdlc_tx_frame(&s->audio.modems.hdlc_tx, NULL, 0);
        }
        else if ((s->hdlc_to_modem.contents[s->hdlc_to_modem.out] & FLAG_DATA))
        {
            /* Check if we should start sending the next frame */
            if ((s->hdlc_to_modem.flags[s->hdlc_to_modem.out] & HDLC_FLAG_PROCEED_WITH_OUTPUT))
            {
                /* This frame is ready to go, and uses the same modem we are running now. So, send
                   whatever we have. This might or might not be an entire frame. */
                span_log(&s->logging, SPAN_LOG_FLOW, "HDLC start next frame\n");
                hdlc_tx_frame(&s->audio.modems.hdlc_tx, s->hdlc_to_modem.buf[s->hdlc_to_modem.out], s->hdlc_to_modem.len[s->hdlc_to_modem.out]);
                if ((s->hdlc_to_modem.flags[s->hdlc_to_modem.out] & HDLC_FLAG_CORRUPT_CRC))
                    hdlc_tx_corrupt_frame(&s->audio.modems.hdlc_tx);
            }
        }
    }
}
/*- End of function --------------------------------------------------------*/

static int set_next_tx_type(t38_gateway_state_t *s)
{
    tone_gen_descriptor_t tone_desc;
    get_bit_func_t get_bit_func;
    void *get_bit_user_data;
    int indicator;

    if (s->audio.next_tx_handler)
    {
        /* There is a handler queued, so that is the next one */
        s->audio.tx_handler = s->audio.next_tx_handler;
        s->audio.tx_user_data = s->audio.next_tx_user_data;
        s->audio.next_tx_handler = NULL;
        if (s->audio.tx_handler == (span_tx_handler_t *) &(silence_gen)
            ||
            s->audio.tx_handler == (span_tx_handler_t *) &(tone_gen))
        {
            set_rx_active(s, TRUE);
        }
        else
        {
            set_rx_active(s, FALSE);
        }
        return TRUE;
    }
    if (s->hdlc_to_modem.in == s->hdlc_to_modem.out)
        return FALSE;
    if ((s->hdlc_to_modem.contents[s->hdlc_to_modem.out] & FLAG_INDICATOR) == 0)
        return FALSE;
    indicator = (s->hdlc_to_modem.contents[s->hdlc_to_modem.out] & 0xFF);
    s->hdlc_to_modem.len[s->hdlc_to_modem.out] = 0;
    s->hdlc_to_modem.flags[s->hdlc_to_modem.out] = 0;
    s->hdlc_to_modem.contents[s->hdlc_to_modem.out] = 0;
    if (++s->hdlc_to_modem.out >= T38_TX_HDLC_BUFS)
        s->hdlc_to_modem.out = 0;
    span_log(&s->logging, SPAN_LOG_FLOW, "Changing to %s\n", t38_indicator_to_str(indicator));
    if (s->image_data_mode  &&  s->ecm_mode)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "HDLC mode\n");
        hdlc_tx_init(&s->audio.modems.hdlc_tx, FALSE, 2, TRUE, hdlc_underflow_handler, s);
        get_bit_func = (get_bit_func_t) hdlc_tx_get_bit;
        get_bit_user_data = (void *) &s->audio.modems.hdlc_tx;
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "non-ECM mode\n");
        get_bit_func = non_ecm_get_bit;
        get_bit_user_data = (void *) s;
    }
    switch (indicator)
    {
    case T38_IND_NO_SIGNAL:
        /* Impose 75ms minimum on transmitted silence */
        //silence_gen_set(&s->audio.modems.silence_gen, ms_to_samples(75));
        s->audio.tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.tx_user_data = &(s->audio.modems.silence_gen);
        s->audio.next_tx_handler = NULL;
        set_rx_active(s, TRUE);
        break;
    case T38_IND_CNG:
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
        tone_gen_init(&(s->audio.modems.tone_gen), &tone_desc);
        s->audio.tx_handler = (span_tx_handler_t *) &(tone_gen);
        s->audio.tx_user_data = &(s->audio.modems.tone_gen);
        silence_gen_set(&s->audio.modems.silence_gen, 0);
        s->audio.next_tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.next_tx_user_data = &(s->audio.modems.silence_gen);
        set_rx_active(s, TRUE);
        break;
    case T38_IND_CED:
        /* 0.2s of silence, then 2.6s to 4s of 2100Hz tone, then 75ms of silence. */
        silence_gen_alter(&s->audio.modems.silence_gen, ms_to_samples(200));
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
        tone_gen_init(&(s->audio.modems.tone_gen), &tone_desc);
        s->audio.tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.tx_user_data = &(s->audio.modems.silence_gen);
        s->audio.next_tx_handler = (span_tx_handler_t *) &(tone_gen);
        s->audio.next_tx_user_data = &(s->audio.modems.tone_gen);
        set_rx_active(s, TRUE);
        break;
    case T38_IND_V21_PREAMBLE:
        hdlc_tx_init(&s->audio.modems.hdlc_tx, FALSE, 2, TRUE, hdlc_underflow_handler, s);
        hdlc_tx_flags(&s->audio.modems.hdlc_tx, 32);
        s->hdlc_to_modem.len[s->hdlc_to_modem.in] = 0;
        fsk_tx_init(&(s->audio.modems.v21_tx), &preset_fsk_specs[FSK_V21CH2], (get_bit_func_t) hdlc_tx_get_bit, &s->audio.modems.hdlc_tx);
        /* Impose a minimum silence */
        silence_gen_alter(&s->audio.modems.silence_gen, ms_to_samples(75));
        s->audio.tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.tx_user_data = &(s->audio.modems.silence_gen);
        s->audio.next_tx_handler = (span_tx_handler_t *) &(fsk_tx);
        s->audio.next_tx_user_data = &(s->audio.modems.v21_tx);
        set_rx_active(s, TRUE);
        break;
    case T38_IND_V27TER_2400_TRAINING:
        silence_gen_alter(&s->audio.modems.silence_gen, ms_to_samples(75));
        hdlc_tx_flags(&s->audio.modems.hdlc_tx, 60);
        v27ter_tx_restart(&(s->audio.modems.v27ter_tx), 2400, s->audio.use_tep);
        v27ter_tx_set_get_bit(&(s->audio.modems.v27ter_tx), get_bit_func, get_bit_user_data);
        s->audio.tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.tx_user_data = &(s->audio.modems.silence_gen);
        s->audio.next_tx_handler = (span_tx_handler_t *) &(v27ter_tx);
        s->audio.next_tx_user_data = &(s->audio.modems.v27ter_tx);
        set_rx_active(s, TRUE);
        break;
    case T38_IND_V27TER_4800_TRAINING:
        hdlc_tx_flags(&s->audio.modems.hdlc_tx, 120);
        silence_gen_alter(&s->audio.modems.silence_gen, ms_to_samples(75));
        v27ter_tx_restart(&(s->audio.modems.v27ter_tx), 4800, s->audio.use_tep);
        v27ter_tx_set_get_bit(&(s->audio.modems.v27ter_tx), get_bit_func, get_bit_user_data);
        s->audio.tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.tx_user_data = &(s->audio.modems.silence_gen);
        s->audio.next_tx_handler = (span_tx_handler_t *) &(v27ter_tx);
        s->audio.next_tx_user_data = &(s->audio.modems.v27ter_tx);
        set_rx_active(s, TRUE);
        break;
    case T38_IND_V29_7200_TRAINING:
        hdlc_tx_flags(&s->audio.modems.hdlc_tx, 180);
        silence_gen_alter(&s->audio.modems.silence_gen, ms_to_samples(75));
        v29_tx_restart(&(s->audio.modems.v29_tx), 7200, s->audio.use_tep);
        v29_tx_set_get_bit(&(s->audio.modems.v29_tx), get_bit_func, get_bit_user_data);
        s->audio.tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.tx_user_data = &(s->audio.modems.silence_gen);
        s->audio.next_tx_handler = (span_tx_handler_t *) &(v29_tx);
        s->audio.next_tx_user_data = &(s->audio.modems.v29_tx);
        set_rx_active(s, TRUE);
        break;
    case T38_IND_V29_9600_TRAINING:
        hdlc_tx_flags(&s->audio.modems.hdlc_tx, 240);
        silence_gen_alter(&s->audio.modems.silence_gen, ms_to_samples(75));
        v29_tx_restart(&(s->audio.modems.v29_tx), 9600, s->audio.use_tep);
        v29_tx_set_get_bit(&(s->audio.modems.v29_tx), get_bit_func, get_bit_user_data);
        s->audio.tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.tx_user_data = &(s->audio.modems.silence_gen);
        s->audio.next_tx_handler = (span_tx_handler_t *) &(v29_tx);
        s->audio.next_tx_user_data = &(s->audio.modems.v29_tx);
        set_rx_active(s, TRUE);
        break;
    case T38_IND_V17_7200_SHORT_TRAINING:
        hdlc_tx_flags(&s->audio.modems.hdlc_tx, 180);
        silence_gen_alter(&s->audio.modems.silence_gen, ms_to_samples(75));
        v17_tx_restart(&(s->audio.modems.v17_tx), 7200, s->audio.use_tep, s->audio.short_train);
        v17_tx_set_get_bit(&(s->audio.modems.v17_tx), get_bit_func, get_bit_user_data);
        s->audio.tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.tx_user_data = &(s->audio.modems.silence_gen);
        s->audio.next_tx_handler = (span_tx_handler_t *) &(v17_tx);
        s->audio.next_tx_user_data = &(s->audio.modems.v17_tx);
        set_rx_active(s, TRUE);
        break;
    case T38_IND_V17_7200_LONG_TRAINING:
        hdlc_tx_flags(&s->audio.modems.hdlc_tx, 180);
        silence_gen_alter(&s->audio.modems.silence_gen, ms_to_samples(75));
        v17_tx_restart(&(s->audio.modems.v17_tx), 7200, s->audio.use_tep, s->audio.short_train);
        v17_tx_set_get_bit(&(s->audio.modems.v17_tx), get_bit_func, get_bit_user_data);
        s->audio.tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.tx_user_data = &(s->audio.modems.silence_gen);
        s->audio.next_tx_handler = (span_tx_handler_t *) &(v17_tx);
        s->audio.next_tx_user_data = &(s->audio.modems.v17_tx);
        set_rx_active(s, TRUE);
        break;
    case T38_IND_V17_9600_SHORT_TRAINING:
        hdlc_tx_flags(&s->audio.modems.hdlc_tx, 240);
        silence_gen_alter(&s->audio.modems.silence_gen, ms_to_samples(75));
        v17_tx_restart(&(s->audio.modems.v17_tx), 9600, s->audio.use_tep, s->audio.short_train);
        v17_tx_set_get_bit(&(s->audio.modems.v17_tx), get_bit_func, get_bit_user_data);
        s->audio.tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.tx_user_data = &(s->audio.modems.silence_gen);
        s->audio.next_tx_handler = (span_tx_handler_t *) &(v17_tx);
        s->audio.next_tx_user_data = &(s->audio.modems.v17_tx);
        set_rx_active(s, TRUE);
        break;
    case T38_IND_V17_9600_LONG_TRAINING:
        hdlc_tx_flags(&s->audio.modems.hdlc_tx, 240);
        silence_gen_alter(&s->audio.modems.silence_gen, ms_to_samples(75));
        v17_tx_restart(&(s->audio.modems.v17_tx), 9600, s->audio.use_tep, s->audio.short_train);
        v17_tx_set_get_bit(&(s->audio.modems.v17_tx), get_bit_func, get_bit_user_data);
        s->audio.tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.tx_user_data = &(s->audio.modems.silence_gen);
        s->audio.next_tx_handler = (span_tx_handler_t *) &(v17_tx);
        s->audio.next_tx_user_data = &(s->audio.modems.v17_tx);
        set_rx_active(s, TRUE);
        break;
    case T38_IND_V17_12000_SHORT_TRAINING:
        hdlc_tx_flags(&s->audio.modems.hdlc_tx, 300);
        silence_gen_alter(&s->audio.modems.silence_gen, ms_to_samples(75));
        v17_tx_restart(&(s->audio.modems.v17_tx), 12000, s->audio.use_tep, s->audio.short_train);
        v17_tx_set_get_bit(&(s->audio.modems.v17_tx), get_bit_func, get_bit_user_data);
        s->audio.tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.tx_user_data = &(s->audio.modems.silence_gen);
        s->audio.next_tx_handler = (span_tx_handler_t *) &(v17_tx);
        s->audio.next_tx_user_data = &(s->audio.modems.v17_tx);
        set_rx_active(s, TRUE);
        break;
    case T38_IND_V17_12000_LONG_TRAINING:
        hdlc_tx_flags(&s->audio.modems.hdlc_tx, 300);
        silence_gen_alter(&s->audio.modems.silence_gen, ms_to_samples(75));
        v17_tx_restart(&(s->audio.modems.v17_tx), 12000, s->audio.use_tep, s->audio.short_train);
        v17_tx_set_get_bit(&(s->audio.modems.v17_tx), get_bit_func, get_bit_user_data);
        s->audio.tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.tx_user_data = &(s->audio.modems.silence_gen);
        s->audio.next_tx_handler = (span_tx_handler_t *) &(v17_tx);
        s->audio.next_tx_user_data = &(s->audio.modems.v17_tx);
        set_rx_active(s, TRUE);
        break;
    case T38_IND_V17_14400_SHORT_TRAINING:
        hdlc_tx_flags(&s->audio.modems.hdlc_tx, 360);
        silence_gen_alter(&s->audio.modems.silence_gen, ms_to_samples(75));
        v17_tx_restart(&(s->audio.modems.v17_tx), 14400, s->audio.use_tep, s->audio.short_train);
        v17_tx_set_get_bit(&(s->audio.modems.v17_tx), get_bit_func, get_bit_user_data);
        s->audio.tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.tx_user_data = &(s->audio.modems.silence_gen);
        s->audio.next_tx_handler = (span_tx_handler_t *) &(v17_tx);
        s->audio.next_tx_user_data = &(s->audio.modems.v17_tx);
        set_rx_active(s, TRUE);
        break;
    case T38_IND_V17_14400_LONG_TRAINING:
        hdlc_tx_flags(&s->audio.modems.hdlc_tx, 360);
        silence_gen_alter(&s->audio.modems.silence_gen, ms_to_samples(75));
        v17_tx_restart(&(s->audio.modems.v17_tx), 14400, s->audio.use_tep, s->audio.short_train);
        v17_tx_set_get_bit(&(s->audio.modems.v17_tx), get_bit_func, get_bit_user_data);
        s->audio.tx_handler = (span_tx_handler_t *) &(silence_gen);
        s->audio.tx_user_data = &(s->audio.modems.silence_gen);
        s->audio.next_tx_handler = (span_tx_handler_t *) &(v17_tx);
        s->audio.next_tx_user_data = &(s->audio.modems.v17_tx);
        set_rx_active(s, TRUE);
        break;
    case T38_IND_V8_ANSAM:
        break;
    case T38_IND_V8_SIGNAL:
        break;
    case T38_IND_V34_CNTL_CHANNEL_1200:
        break;
    case T38_IND_V34_PRI_CHANNEL:
        break;
    case T38_IND_V34_CC_RETRAIN:
        break;
    case T38_IND_V33_12000_TRAINING:
        break;
    case T38_IND_V33_14400_TRAINING:
        break;
    default:
        break;
    }
    if (s->non_ecm_to_modem.in_octets  ||  s->non_ecm_to_modem.out_octets)
    {
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "%d incoming non-ECM octets, %d rows.  %d outgoing non-ECM octets, %d rows\n",
                 s->non_ecm_to_modem.in_octets,
                 s->non_ecm_to_modem.in_rows,
                 s->non_ecm_to_modem.out_octets,
                 s->non_ecm_to_modem.out_rows);
        s->non_ecm_to_modem.in_octets = 0;
        s->non_ecm_to_modem.out_octets = 0;
    }
    s->non_ecm_to_modem.in_rows = 0;
    s->non_ecm_to_modem.out_rows = 0;
    if (s->non_ecm_to_modem.flow_control_fill_octets)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM flow control generated %d octets\n", s->non_ecm_to_modem.flow_control_fill_octets);
        s->non_ecm_to_modem.flow_control_fill_octets = 0;
    }
    s->non_ecm_to_modem.bit_no = 0;
    s->non_ecm_to_modem.rx_bit_stream = 0xFFFF;
    s->non_ecm_to_modem.tx_octet = 0xFF;
    s->non_ecm_to_modem.flow_control_fill_octet = 0xFF;
    s->non_ecm_to_modem.at_initial_all_ones = TRUE;
    s->non_ecm_to_modem.bit_stream = 0xFFFF;
    s->t38x.in_progress_rx_indicator = indicator;
    return TRUE;
}
/*- End of function --------------------------------------------------------*/

static void pump_out_final_hdlc(t38_gateway_state_t *s, int good_fcs)
{
    if (!good_fcs)
        s->hdlc_to_modem.flags[s->hdlc_to_modem.in] |= HDLC_FLAG_CORRUPT_CRC;
    if (s->hdlc_to_modem.in == s->hdlc_to_modem.out)
    {
        /* This is the frame in progress at the output. */
        if ((s->hdlc_to_modem.flags[s->hdlc_to_modem.out] & HDLC_FLAG_PROCEED_WITH_OUTPUT) == 0)
        {
            /* Output of this frame has not yet begun. Throw it all out now. */
            hdlc_tx_frame(&s->audio.modems.hdlc_tx, s->hdlc_to_modem.buf[s->hdlc_to_modem.out], s->hdlc_to_modem.len[s->hdlc_to_modem.out]);
        }
        if ((s->hdlc_to_modem.flags[s->hdlc_to_modem.out] & HDLC_FLAG_CORRUPT_CRC))
            hdlc_tx_corrupt_frame(&s->audio.modems.hdlc_tx);
    }
    s->hdlc_to_modem.flags[s->hdlc_to_modem.in] |= (HDLC_FLAG_PROCEED_WITH_OUTPUT | HDLC_FLAG_FINISHED);
    if (++s->hdlc_to_modem.in >= T38_TX_HDLC_BUFS)
        s->hdlc_to_modem.in = 0;
}
/*- End of function --------------------------------------------------------*/

static void edit_control_messages(t38_gateway_state_t *s, int from_modem, uint8_t *buf, int len)
{
    /* Frames need to be fed to this routine byte by byte as they arrive. It basically just
       edits the last byte received, based on the frame up to that point. */
    if (s->t38x.corrupt_current_frame[from_modem])
    {
        /* We simply need to overwrite a section of the message, so it is not recognisable at
           the receiver. This is used for the NSF, NSC, and NSS messages. Several strategies are
           possible for the replacement data. If you have a manufacturer code of your own, the
           sane thing is to overwrite the original data with that. */
        if (len <= s->t38x.suppress_nsx_len[from_modem])
            buf[len - 1] = nsx_overwrite[from_modem][len - 4];
        return;
    }
    /* Edit the message, if we need to control the communication between the end points. */
    switch (len)
    {
    case 3:
        switch (buf[2])
        {
        case T30_NSF:
        case T30_NSC:
        case T30_NSS:
            if (s->t38x.suppress_nsx_len[from_modem])
            {
                /* Corrupt the message, so it will be ignored by the far end. If it were
                   processed, 2 machines which recognise each other might do special things
                   we cannot handle as a middle man. */
                span_log(&s->logging, SPAN_LOG_FLOW, "Corrupting %s message to prevent recognition\n", t30_frametype(buf[2]));
                s->t38x.corrupt_current_frame[from_modem] = TRUE;
            }
            break;
        }
        break;
    case 5:
        switch (buf[2])
        {
        case T30_DIS:
            /* We may need to adjust the capabilities, so they do not exceed our own */
            span_log(&s->logging, SPAN_LOG_FLOW, "Applying fast modem type constraints.\n");
            switch (buf[4] & (DISBIT6 | DISBIT5 | DISBIT4 | DISBIT3))
            {
            case 0:
            case DISBIT4:
                /* V.27ter only */
                break;
            case DISBIT3:
            case (DISBIT4 | DISBIT3):
                /* V.27ter and V.29 */
                if (!(s->supported_modems & T30_SUPPORT_V29))
                    buf[4] &= ~DISBIT3;
                break;
            case (DISBIT6 | DISBIT4 | DISBIT3):
                /* V.27ter, V.29 and V.17 */
                if (!(s->supported_modems & T30_SUPPORT_V17))
                    buf[4] &= ~DISBIT6;
                if (!(s->supported_modems & T30_SUPPORT_V29))
                    buf[4] &= ~DISBIT3;
                break;
            case (DISBIT5 | DISBIT4):
            case (DISBIT6 | DISBIT4):
            case (DISBIT6 | DISBIT5 | DISBIT4):
            case (DISBIT6 | DISBIT5 | DISBIT4 | DISBIT3):
                /* Reserved */
                buf[4] &= ~(DISBIT6 | DISBIT5);
                buf[4] |= (DISBIT4 | DISBIT3);
                break;
            default:
                /* Not used */
                buf[4] &= ~(DISBIT6 | DISBIT5);
                buf[4] |= (DISBIT4 | DISBIT3);
                break;
            }
            break;
        }
        break;
    case 7:
        switch (buf[2])
        {
        case T30_DIS:
            if (!s->ecm_allowed)
            {
                /* Do not allow ECM or T.6 coding */
                span_log(&s->logging, SPAN_LOG_FLOW, "Inhibiting ECM\n");
                buf[6] &= ~(DISBIT3 | DISBIT7);
            }
            break;
        }
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void monitor_control_messages(t38_gateway_state_t *s, int from_modem, uint8_t *buf, int len)
{
    /* Monitor the control messages, at the point where we have the whole message, so we can
       see what is happening to things like training success/failure. */
    span_log(&s->logging, SPAN_LOG_FLOW, "Monitoring %s\n", t30_frametype(buf[2]));
    if (len < 3)
        return;
    s->tcf_mode_predictable_modem_start = 0;
    switch (buf[2])
    {
    case T30_CFR:
        /* We are changing from TCF exchange to image exchange */
        /* Successful training means we should change to short training */
        s->image_data_mode = TRUE;
        s->audio.short_train = TRUE;
        span_log(&s->logging, SPAN_LOG_FLOW, "CFR - short train = %d, ECM = %d\n", s->audio.short_train, s->ecm_mode);
        if (!from_modem)
            restart_rx_modem(s);
        break;
    case T30_RTN:
    case T30_RTP:
        /* We are going back to the exchange of fresh TCF */
        s->image_data_mode = FALSE;
        s->audio.short_train = FALSE;
        break;
    case T30_CTR:
        /* T.30 says the first image data after this does full training. */
        s->audio.short_train = FALSE;
        break;
    case T30_DTC:
    case T30_DCS:
    case T30_DCS | 1:
        /* We need to check which modem type is about to be used. */
        if (len >= 5)
        {
            switch (buf[4] & (DISBIT6 | DISBIT5 | DISBIT4 | DISBIT3))
            {
            case 0:
                s->fast_bit_rate = 2400;
                s->fast_modem = T38_V27TER_RX;
                break;
            case DISBIT4:
                s->fast_bit_rate = 4800;
                s->fast_modem = T38_V27TER_RX;
                break;
            case DISBIT3:
                s->fast_bit_rate = 9600;
                s->fast_modem = T38_V29_RX;
                break;
            case (DISBIT4 | DISBIT3):
                s->fast_bit_rate = 7200;
                s->fast_modem = T38_V29_RX;
                break;
            case DISBIT6:
                s->fast_bit_rate = 14400;
                s->fast_modem = T38_V17_RX;
                break;
            case (DISBIT6 | DISBIT4):
                s->fast_bit_rate = 12000;
                s->fast_modem = T38_V17_RX;
                break;
            case (DISBIT6 | DISBIT3):
                s->fast_bit_rate = 9600;
                s->fast_modem = T38_V17_RX;
                break;
            case (DISBIT6 | DISBIT4 | DISBIT3):
                s->fast_bit_rate = 7200;
                s->fast_modem = T38_V17_RX;
                break;
            case (DISBIT5 | DISBIT3):
            case (DISBIT5 | DISBIT4 | DISBIT3):
            case (DISBIT6 | DISBIT5):
            case (DISBIT6 | DISBIT5 | DISBIT3):
            case (DISBIT6 | DISBIT5 | DISBIT4):
            case (DISBIT6 | DISBIT5 | DISBIT4 | DISBIT3):
                /* Reserved */
                s->fast_bit_rate = 0;
                s->fast_modem = T38_NONE;
                break;
            default:
                /* Not used */
                s->fast_bit_rate = 0;
                s->fast_modem = T38_NONE;
                break;
            }
        }
        if (len >= 7)
            s->ecm_mode = (buf[6] & DISBIT3);
        else
            s->ecm_mode = FALSE;
        s->audio.short_train = FALSE;
        s->image_data_mode = FALSE;
        if (from_modem)
            s->tcf_mode_predictable_modem_start = 2;
        break;
    case T30_PPS:
    case T30_PPS | 1:
        switch (buf[3] & 0xFE)
        {
        case T30_EOP:
        case T30_EOM:
        case T30_MPS:
        case T30_PRI_EOP:
        case T30_PRI_EOM:
        case T30_PRI_MPS:
            s->count_page_on_mcf = TRUE;
            break;
        }
        break;
    case T30_EOP:
    case T30_EOM:
    case T30_MPS:
    case T30_PRI_EOP:
    case T30_PRI_EOM:
    case T30_PRI_MPS:
    case T30_EOP | 1:
    case T30_EOM | 1:
    case T30_MPS | 1:
    case T30_PRI_EOP | 1:
    case T30_PRI_EOM | 1:
    case T30_PRI_MPS | 1:
        s->count_page_on_mcf = TRUE;
        break;
    case T30_MCF:
    case T30_MCF | 1:
        if (s->count_page_on_mcf)
        {
            s->pages_confirmed++;
            span_log(&s->logging, SPAN_LOG_FLOW, "Pages confirmed = %d\n", s->pages_confirmed);
            s->count_page_on_mcf = FALSE;
        }
        break;
    default:
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void queue_missing_indicator(t38_gateway_state_t *s, int data_type)
{
    t38_core_state_t *t;
    
    t = &s->t38x.t38;
    /* Missing packets might have lost us the indicator that should have put us in
       the required mode of operation. It might be a bit late to fill in such a gap
       now, but we should try. We may also want to force indicators into the queue,
       such as when the data says 'end of signal'. */
    switch (data_type)
    {
    case T38_DATA_NONE:
        if (t->current_rx_indicator != T38_IND_NO_SIGNAL)
            process_rx_indicator(t, (void *) s, T38_IND_NO_SIGNAL);
        break;
    case T38_DATA_V21:
        if (t->current_rx_indicator != T38_IND_V21_PREAMBLE)
            process_rx_indicator(t, (void *) s, T38_IND_V21_PREAMBLE);
        break;
    case T38_DATA_V27TER_2400:
        if (t->current_rx_indicator != T38_IND_V27TER_2400_TRAINING)
            process_rx_indicator(t, (void *) s, T38_IND_V27TER_2400_TRAINING);
        break;
    case T38_DATA_V27TER_4800:
        if (t->current_rx_indicator != T38_IND_V27TER_4800_TRAINING)
            process_rx_indicator(t, (void *) s, T38_IND_V27TER_4800_TRAINING);
        break;
    case T38_DATA_V29_7200:
        if (t->current_rx_indicator != T38_IND_V29_7200_TRAINING)
            process_rx_indicator(t, (void *) s, T38_IND_V29_7200_TRAINING);
        break;
    case T38_DATA_V29_9600:
        if (t->current_rx_indicator != T38_IND_V29_9600_TRAINING)
            process_rx_indicator(t, (void *) s, T38_IND_V29_9600_TRAINING);
        break;
    case T38_DATA_V17_7200:
        if (t->current_rx_indicator != T38_IND_V17_7200_SHORT_TRAINING  &&  t->current_rx_indicator != T38_IND_V17_7200_LONG_TRAINING)
            process_rx_indicator(t, (void *) s, T38_IND_V17_7200_LONG_TRAINING);
        break;
    case T38_DATA_V17_9600:
        if (t->current_rx_indicator != T38_IND_V17_9600_SHORT_TRAINING  &&  t->current_rx_indicator != T38_IND_V17_9600_LONG_TRAINING)
            process_rx_indicator(t, (void *) s, T38_IND_V17_9600_LONG_TRAINING);
        break;
    case T38_DATA_V17_12000:
        if (t->current_rx_indicator != T38_IND_V17_12000_SHORT_TRAINING  &&  t->current_rx_indicator != T38_IND_V17_12000_LONG_TRAINING)
            process_rx_indicator(t, (void *) s, T38_IND_V17_12000_LONG_TRAINING);
        break;
    case T38_DATA_V17_14400:
        if (t->current_rx_indicator != T38_IND_V17_14400_SHORT_TRAINING  &&  t->current_rx_indicator != T38_IND_V17_14400_LONG_TRAINING)
            process_rx_indicator(t, (void *) s, T38_IND_V17_14400_LONG_TRAINING);
        break;
    case T38_DATA_V8:
        break;
    case T38_DATA_V34_PRI_RATE:
        break;
    case T38_DATA_V34_CC_1200:
        break;
    case T38_DATA_V34_PRI_CH:
        break;
    case T38_DATA_V33_12000:
        break;
    case T38_DATA_V33_14400:
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static int process_rx_missing(t38_core_state_t *t, void *user_data, int rx_seq_no, int expected_seq_no)
{
    t38_gateway_state_t *s;
    
    s = (t38_gateway_state_t *) user_data;
    s->hdlc_to_modem.flags[s->hdlc_to_modem.in] |= HDLC_FLAG_MISSING_DATA;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_indicator(t38_core_state_t *t, void *user_data, int indicator)
{
    t38_gateway_state_t *s;
    
    s = (t38_gateway_state_t *) user_data;

    if (t->current_rx_indicator == indicator)
    {
        /* This is probably due to the far end repeating itself. Ignore it. Its harmless */
        return 0;
    }
    if (s->hdlc_to_modem.contents[s->hdlc_to_modem.in])
    {
        if (++s->hdlc_to_modem.in >= T38_TX_HDLC_BUFS)
            s->hdlc_to_modem.in = 0;
    }
    s->hdlc_to_modem.contents[s->hdlc_to_modem.in] = (indicator | FLAG_INDICATOR);
    if (++s->hdlc_to_modem.in >= T38_TX_HDLC_BUFS)
        s->hdlc_to_modem.in = 0;
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "Queued change - (%d) %s -> %s\n",
             silence_gen_remainder(&(s->audio.modems.silence_gen)),
             t38_indicator_to_str(t->current_rx_indicator),
             t38_indicator_to_str(indicator));
    s->t38x.current_rx_field_class = T38_FIELD_CLASS_NONE;
    /* We need to set this here, since we might have been called as a fake
       indication when the real one was missing */
    t->current_rx_indicator = indicator;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_data(t38_core_state_t *t, void *user_data, int data_type, int field_type, const uint8_t *buf, int len)
{
    int i;
    int previous;
    t38_gateway_state_t *s;
    t38_gateway_t38_state_t *xx;

    s = (t38_gateway_state_t *) user_data;
    xx = &s->t38x;
    switch (field_type)
    {
    case T38_FIELD_HDLC_DATA:
        xx->current_rx_field_class = T38_FIELD_CLASS_HDLC;
        if (s->hdlc_to_modem.contents[s->hdlc_to_modem.in] != (data_type | FLAG_DATA))
            queue_missing_indicator(s, data_type);
        previous = s->hdlc_to_modem.len[s->hdlc_to_modem.in];
        /* Check if this data would overflow the buffer. */
        if (s->hdlc_to_modem.len[s->hdlc_to_modem.in] + len > T38_MAX_HDLC_LEN)
            break;
        s->hdlc_to_modem.contents[s->hdlc_to_modem.in] = (data_type | FLAG_DATA);
        bit_reverse(&s->hdlc_to_modem.buf[s->hdlc_to_modem.in][s->hdlc_to_modem.len[s->hdlc_to_modem.in]], buf, len);
        /* We need to send out the control messages as they are arriving. They are
           too slow to capture a whole frame, and then pass it on.
           For the faster frames, take in the whole frame before sending it out. Also, there
           is no need to monitor, or modify, the contents of the faster frames. */
        if (data_type == T38_DATA_V21)
        {
            for (i = 1;  i <= len;  i++)
                edit_control_messages(s, 0, s->hdlc_to_modem.buf[s->hdlc_to_modem.in], s->hdlc_to_modem.len[s->hdlc_to_modem.in] + i);
            /* Don't start pumping data into the actual output stream until there is
               enough backlog to create some elasticity for jitter tolerance. */
            if (s->hdlc_to_modem.len[s->hdlc_to_modem.in] + len >= HDLC_START_BUFFER_LEVEL)
            {
                if (s->hdlc_to_modem.in == s->hdlc_to_modem.out)
                {
                    if ((s->hdlc_to_modem.flags[s->hdlc_to_modem.in] & HDLC_FLAG_PROCEED_WITH_OUTPUT) == 0)
                        previous = 0;
                    hdlc_tx_frame(&s->audio.modems.hdlc_tx, s->hdlc_to_modem.buf[s->hdlc_to_modem.out] + previous, s->hdlc_to_modem.len[s->hdlc_to_modem.out] - previous + len);
                }
                s->hdlc_to_modem.flags[s->hdlc_to_modem.in] |= HDLC_FLAG_PROCEED_WITH_OUTPUT;
            }
        }
        s->hdlc_to_modem.len[s->hdlc_to_modem.in] += len;
        break;
    case T38_FIELD_HDLC_FCS_OK:
        xx->current_rx_field_class = T38_FIELD_CLASS_HDLC;
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_FCS_OK!\n");
            /* The sender has incorrectly included data in this message. It is unclear what we should do
               with it, to maximise tolerance of buggy implementations. */
        }
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_FCS_OK messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (t->current_rx_data_type != data_type
            ||
            t->current_rx_field_type != field_type)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "HDLC frame type %s - CRC good\n", t30_frametype(s->hdlc_to_modem.buf[s->hdlc_to_modem.in][2]));
            if (s->hdlc_to_modem.contents[s->hdlc_to_modem.in] != (data_type | FLAG_DATA))
                queue_missing_indicator(s, data_type);
            s->hdlc_to_modem.contents[s->hdlc_to_modem.in] = (data_type | FLAG_DATA);
            if (data_type == T38_DATA_V21)
            {
                if ((s->hdlc_to_modem.flags[s->hdlc_to_modem.in] & HDLC_FLAG_MISSING_DATA) == 0)
                {
                    monitor_control_messages(s, FALSE, s->hdlc_to_modem.buf[s->hdlc_to_modem.in], s->hdlc_to_modem.len[s->hdlc_to_modem.in]);
                    if (s->real_time_frame_handler)
                        s->real_time_frame_handler(s, s->real_time_frame_user_data, FALSE, s->hdlc_to_modem.buf[s->hdlc_to_modem.in], s->hdlc_to_modem.len[s->hdlc_to_modem.in]);
                }
            }
            else
            {
                /* Make sure we go back to short training if CTC/CTR has kicked us into
                   long training. Theer has to be more than one value HDLC frame in a
                   chunk of image data, so just setting short training mode heer should
                   be enough. */
                s->audio.short_train = TRUE;
            }
            pump_out_final_hdlc(s, (s->hdlc_to_modem.flags[s->hdlc_to_modem.in] & HDLC_FLAG_MISSING_DATA) == 0);
        }
        s->hdlc_to_modem.len[s->hdlc_to_modem.in] = 0;
        s->hdlc_to_modem.flags[s->hdlc_to_modem.in] = 0;
        xx->corrupt_current_frame[0] = FALSE;
        break;
    case T38_FIELD_HDLC_FCS_BAD:
        xx->current_rx_field_class = T38_FIELD_CLASS_HDLC;
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_FCS_BAD!\n");
            /* The sender has incorrectly included data in this message. We can safely ignore it, as the
               bad FCS means we will throw away the whole message, anyway. */
        }
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_FCS_BAD messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (t->current_rx_data_type != data_type  ||  t->current_rx_field_type != field_type)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "HDLC frame type %s - CRC bad\n", t30_frametype(s->hdlc_to_modem.buf[s->hdlc_to_modem.in][2]));
            if (s->hdlc_to_modem.contents[s->hdlc_to_modem.in] != (data_type | FLAG_DATA))
                queue_missing_indicator(s, data_type);
            if (s->hdlc_to_modem.len[s->hdlc_to_modem.in] > 0)
            {
                s->hdlc_to_modem.contents[s->hdlc_to_modem.in] = (data_type | FLAG_DATA);
                pump_out_final_hdlc(s, FALSE);
            }
        }
        s->hdlc_to_modem.len[s->hdlc_to_modem.in] = 0;
        s->hdlc_to_modem.flags[s->hdlc_to_modem.in] = 0;
        xx->corrupt_current_frame[0] = FALSE;
        break;
    case T38_FIELD_HDLC_FCS_OK_SIG_END:
        xx->current_rx_field_class = T38_FIELD_CLASS_HDLC;
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_FCS_OK_SIG_END!\n");
            /* The sender has incorrectly included data in this message. It is unclear what we should do
               with it, to maximise tolerance of buggy implementations. */
        }
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_FCS_OK_SIG_END messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (t->current_rx_data_type != data_type  ||  t->current_rx_field_type != field_type)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "HDLC frame type %s - CRC OK, sig end\n", t30_frametype(s->hdlc_to_modem.buf[s->hdlc_to_modem.in][2]));
            if (s->hdlc_to_modem.contents[s->hdlc_to_modem.in] != (data_type | FLAG_DATA))
                queue_missing_indicator(s, data_type);
            s->hdlc_to_modem.contents[s->hdlc_to_modem.in] = (data_type | FLAG_DATA);
            if (data_type == T38_DATA_V21  &&  (s->hdlc_to_modem.flags[s->hdlc_to_modem.in] & HDLC_FLAG_MISSING_DATA) == 0)
            {
                monitor_control_messages(s, FALSE, s->hdlc_to_modem.buf[s->hdlc_to_modem.in], s->hdlc_to_modem.len[s->hdlc_to_modem.in]);
                if (s->real_time_frame_handler)
                    s->real_time_frame_handler(s, s->real_time_frame_user_data, FALSE, s->hdlc_to_modem.buf[s->hdlc_to_modem.in], s->hdlc_to_modem.len[s->hdlc_to_modem.in]);
            }
            pump_out_final_hdlc(s, (s->hdlc_to_modem.flags[s->hdlc_to_modem.in] & HDLC_FLAG_MISSING_DATA) == 0);
            s->hdlc_to_modem.len[s->hdlc_to_modem.in] = 0;
            s->hdlc_to_modem.flags[s->hdlc_to_modem.in] = 0;
            s->hdlc_to_modem.contents[s->hdlc_to_modem.in] = 0;
            queue_missing_indicator(s, T38_DATA_NONE);
            xx->current_rx_field_class = T38_FIELD_CLASS_NONE;
        }
        xx->corrupt_current_frame[0] = FALSE;
        break;
    case T38_FIELD_HDLC_FCS_BAD_SIG_END:
        xx->current_rx_field_class = T38_FIELD_CLASS_HDLC;
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_FCS_BAD_SIG_END!\n");
            /* The sender has incorrectly included data in this message. We can safely ignore it, as the
               bad FCS means we will throw away the whole message, anyway. */
        }
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_FCS_BAD_SIG_END messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (t->current_rx_data_type != data_type  ||  t->current_rx_field_type != field_type)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "HDLC frame type %s - CRC bad, sig end\n", t30_frametype(s->hdlc_to_modem.buf[s->hdlc_to_modem.in][2]));
            if (s->hdlc_to_modem.contents[s->hdlc_to_modem.in] != (data_type | FLAG_DATA))
                queue_missing_indicator(s, data_type);
            if (s->hdlc_to_modem.len[s->hdlc_to_modem.in] > 0)
            {
                s->hdlc_to_modem.contents[s->hdlc_to_modem.in] = (data_type | FLAG_DATA);
                pump_out_final_hdlc(s, FALSE);
            }
            s->hdlc_to_modem.len[s->hdlc_to_modem.in] = 0;
            s->hdlc_to_modem.flags[s->hdlc_to_modem.in] = 0;
            s->hdlc_to_modem.contents[s->hdlc_to_modem.in] = 0;
            queue_missing_indicator(s, T38_DATA_NONE);
            xx->current_rx_field_class = T38_FIELD_CLASS_NONE;
        }
        xx->corrupt_current_frame[0] = FALSE;
        break;
    case T38_FIELD_HDLC_SIG_END:
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_SIG_END!\n");
            /* The sender has incorrectly included data in this message, but there seems nothing meaningful
               it could be. There could not be an FCS good/bad report beyond this. */
        }
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_SIG_END messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (t->current_rx_data_type != data_type  ||  t->current_rx_field_type != field_type)
        {
            if (s->hdlc_to_modem.contents[s->hdlc_to_modem.in] != (data_type | FLAG_DATA))
                queue_missing_indicator(s, data_type);
            /* WORKAROUND: At least some Mediatrix boxes have a bug, where they can send this message at the
                           end of non-ECM data. We need to tolerate this. */
            if (xx->current_rx_field_class == T38_FIELD_CLASS_NON_ECM)
            {
                span_log(&s->logging, SPAN_LOG_WARNING, "T38_FIELD_HDLC_SIG_END received at the end of non-ECM data!\n");
                /* Don't flow control the data any more. Just pump out the remainder as fast as we can. */
                s->non_ecm_to_modem.tx_latest_eol_ptr = s->non_ecm_to_modem.tx_in_ptr;
                s->non_ecm_to_modem.data_finished = TRUE;
            }
            else
            {
                /* This message is expected under 2 circumstances. One is as an alternative to T38_FIELD_HDLC_FCS_OK_SIG_END - 
                   i.e. they send T38_FIELD_HDLC_FCS_OK, and then T38_FIELD_HDLC_SIG_END when the carrier actually drops.
                   The other is because the HDLC signal drops unexpectedly - i.e. not just after a final frame. */
                s->hdlc_to_modem.len[s->hdlc_to_modem.in] = 0;
                s->hdlc_to_modem.flags[s->hdlc_to_modem.in] = 0;
                s->hdlc_to_modem.contents[s->hdlc_to_modem.in] = 0;
            }
            queue_missing_indicator(s, T38_DATA_NONE);
            xx->current_rx_field_class = T38_FIELD_CLASS_NONE;
        }
        xx->corrupt_current_frame[0] = FALSE;
        break;
    case T38_FIELD_T4_NON_ECM_DATA:
        xx->current_rx_field_class = T38_FIELD_CLASS_NON_ECM;
        if (s->hdlc_to_modem.contents[s->hdlc_to_modem.in] != (data_type | FLAG_DATA))
            queue_missing_indicator(s, data_type);
        add_to_non_ecm_modem_buffer(s, buf, len);
        xx->corrupt_current_frame[0] = FALSE;
        break;
    case T38_FIELD_T4_NON_ECM_SIG_END:
        /* Some T.38 implementations send multiple T38_FIELD_T4_NON_ECM_SIG_END messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (t->current_rx_data_type != data_type  ||  t->current_rx_field_type != field_type)
        {
            /* WORKAROUND: At least some Mediatrix boxes have a bug, where they can send HDLC signal end where
                           they should send non-ECM signal end. It is possible they also do the opposite.
                           We need to tolerate this. */
            if (xx->current_rx_field_class == T38_FIELD_CLASS_NON_ECM)
            {
                if (len > 0)
                {
                    if (s->hdlc_to_modem.contents[s->hdlc_to_modem.in] != (data_type | FLAG_DATA))
                        queue_missing_indicator(s, data_type);
                    add_to_non_ecm_modem_buffer(s, buf, len);
                }
                if (s->hdlc_to_modem.contents[s->hdlc_to_modem.in] != (data_type | FLAG_DATA))
                    queue_missing_indicator(s, data_type);
                /* Don't flow control the data any more. Just pump out the remainder as fast as we can. */
                s->non_ecm_to_modem.tx_latest_eol_ptr = s->non_ecm_to_modem.tx_in_ptr;
                s->non_ecm_to_modem.data_finished = TRUE;
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_WARNING, "T38_FIELD_NON_ECM_SIG_END received at the end of HDLC data!\n");
                if (s->hdlc_to_modem.contents[s->hdlc_to_modem.in] != (data_type | FLAG_DATA))
                    queue_missing_indicator(s, data_type);
                s->hdlc_to_modem.len[s->hdlc_to_modem.in] = 0;
                s->hdlc_to_modem.flags[s->hdlc_to_modem.in] = 0;
                s->hdlc_to_modem.contents[s->hdlc_to_modem.in] = 0;
            }
            queue_missing_indicator(s, T38_DATA_NONE);
            xx->current_rx_field_class = T38_FIELD_CLASS_NONE;
        }
        xx->corrupt_current_frame[0] = FALSE;
        break;
    case T38_FIELD_CM_MESSAGE:
        if (len >= 1)
            span_log(&s->logging, SPAN_LOG_FLOW, "CM profile %d - %s\n", buf[0] - '0', t38_cm_profile_to_str(buf[0]));
        else
            span_log(&s->logging, SPAN_LOG_FLOW, "Bad length for CM message - %d\n", len);
        break;
    case T38_FIELD_JM_MESSAGE:
        if (len >= 2)
            span_log(&s->logging, SPAN_LOG_FLOW, "JM - %s\n", t38_jm_to_str(buf, len));
        else
            span_log(&s->logging, SPAN_LOG_FLOW, "Bad length for JM message - %d\n", len);
        break;
    case T38_FIELD_CI_MESSAGE:
        if (len >= 1)
            span_log(&s->logging, SPAN_LOG_FLOW, "CI 0x%X\n", buf[0]);
        else
            span_log(&s->logging, SPAN_LOG_FLOW, "Bad length for CI message - %d\n", len);
        break;
    case T38_FIELD_V34RATE:
        if (len >= 3)
        {
            xx->t38.v34_rate = t38_v34rate_to_bps(buf, len);
            span_log(&s->logging, SPAN_LOG_FLOW, "V.34 rate %d bps\n", xx->t38.v34_rate);
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Bad length for V34rate message - %d\n", len);
        }
        break;
    default:
        break;
    }

#if 0
    if (span_log_test(&s->logging, SPAN_LOG_FLOW))
    {
        int i;

        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Data: ");
            for (i = 0;  i < len;  i++)
                span_log(&s->logging, SPAN_LOG_FLOW | SPAN_LOG_SUPPRESS_LABELLING, " %02X", buf[i]);
        }
    }
    span_log(&s->logging, SPAN_LOG_FLOW | SPAN_LOG_SUPPRESS_LABELLING, "\n");
#endif
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void set_octets_per_data_packet(t38_gateway_state_t *s, int bit_rate)
{
    int octets;
    
    octets = MS_PER_TX_CHUNK*bit_rate/(8*1000);
    if (octets < 1)
        octets = 1;
    s->t38x.octets_per_data_packet = octets;
}
/*- End of function --------------------------------------------------------*/

static int set_slow_packetisation(t38_gateway_state_t *s)
{
    set_octets_per_data_packet(s, 300);
    s->t38x.current_tx_data_type = T38_DATA_V21;
    return T38_IND_V21_PREAMBLE;
}
/*- End of function --------------------------------------------------------*/

static int set_fast_packetisation(t38_gateway_state_t *s)
{
    int ind;

    ind = T38_IND_NO_SIGNAL;
    switch (s->fast_rx_active)
    {
    case T38_V17_RX:
        switch (s->fast_bit_rate)
        {
        case 7200:
            set_octets_per_data_packet(s, 7200);
            ind = (s->audio.short_train)  ?  T38_IND_V17_7200_SHORT_TRAINING  :  T38_IND_V17_7200_LONG_TRAINING;
            s->t38x.current_tx_data_type = T38_DATA_V17_7200;
            break;
        case 9600:
            set_octets_per_data_packet(s, 9600);
            ind = (s->audio.short_train)  ?  T38_IND_V17_9600_SHORT_TRAINING  :  T38_IND_V17_9600_LONG_TRAINING;
            s->t38x.current_tx_data_type = T38_DATA_V17_9600;
            break;
        case 12000:
            set_octets_per_data_packet(s, 12000);
            ind = (s->audio.short_train)  ?  T38_IND_V17_12000_SHORT_TRAINING  :  T38_IND_V17_12000_LONG_TRAINING;
            s->t38x.current_tx_data_type = T38_DATA_V17_12000;
            break;
        default:
        case 14400:
            set_octets_per_data_packet(s, 14400);
            ind = (s->audio.short_train)  ?  T38_IND_V17_14400_SHORT_TRAINING  :  T38_IND_V17_14400_LONG_TRAINING;
            s->t38x.current_tx_data_type = T38_DATA_V17_14400;
            break;
        }
        break;
    case T38_V27TER_RX:
        switch (s->fast_bit_rate)
        {
        case 2400:
            set_octets_per_data_packet(s, 2400);
            ind = T38_IND_V27TER_2400_TRAINING;
            s->t38x.current_tx_data_type = T38_DATA_V27TER_2400;
            break;
        default:
        case 4800:
            set_octets_per_data_packet(s, 4800);
            ind = T38_IND_V27TER_4800_TRAINING;
            s->t38x.current_tx_data_type = T38_DATA_V27TER_4800;
            break;
        }
        break;
    case T38_V29_RX:
        switch (s->fast_bit_rate)
        {
        case 7200:
            set_octets_per_data_packet(s, 7200);
            ind = T38_IND_V29_7200_TRAINING;
            s->t38x.current_tx_data_type = T38_DATA_V29_7200;
            break;
        default:
        case 9600:
            set_octets_per_data_packet(s, 9600);
            ind = T38_IND_V29_9600_TRAINING;
            s->t38x.current_tx_data_type = T38_DATA_V29_9600;
            break;
        }
        break;
    }
    return ind;
}
/*- End of function --------------------------------------------------------*/

static void announce_training(t38_gateway_state_t *s)
{
    t38_core_send_indicator(&s->t38x.t38, set_fast_packetisation(s), s->t38x.t38.indicator_tx_count);
}
/*- End of function --------------------------------------------------------*/

static void non_ecm_put_bit(void *user_data, int bit)
{
    t38_gateway_state_t *s;

    s = (t38_gateway_state_t *) user_data;
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_TRAINING_IN_PROGRESS:
            span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM carrier training in progress\n");
            if (s->tcf_mode_predictable_modem_start)
                s->tcf_mode_predictable_modem_start = 0;
            else
                announce_training(s);
            break;
        case PUTBIT_TRAINING_FAILED:
            span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM carrier training failed\n");
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            /* The modem is now trained */
            span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM carrier trained\n");
            s->audio.rx_signal_present = TRUE;
            s->audio.rx_trained = TRUE;
            s->to_t38.data_ptr = 0;
            break;
        case PUTBIT_CARRIER_UP:
            span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM carrier down\n");
            s->tcf_mode_predictable_modem_start = 0;
            switch (s->t38x.current_tx_data_type)
            {
            case T38_DATA_V17_7200:
            case T38_DATA_V17_9600:
            case T38_DATA_V17_12000:
            case T38_DATA_V17_14400:
            case T38_DATA_V27TER_2400:
            case T38_DATA_V27TER_4800:
            case T38_DATA_V29_7200:
            case T38_DATA_V29_9600:
                t38_core_send_data(&s->t38x.t38, s->t38x.current_tx_data_type, T38_FIELD_T4_NON_ECM_SIG_END, NULL, 0, s->t38x.t38.data_end_tx_count);
                t38_core_send_indicator(&s->t38x.t38, T38_IND_NO_SIGNAL, s->t38x.t38.indicator_tx_count);
                restart_rx_modem(s);
                break;
            }
            break;
        default:
            span_log(&s->logging, SPAN_LOG_WARNING, "Unexpected non-ECM special bit - %d!\n", bit);
            break;
        }
        return;
    }
    bit &= 1;
    if (s->t38x.t38.fill_bit_removal)
    {
        /* Drop any extra zero bits when we already have enough for an EOL symbol. */
        /* The snag here is that if we just look for 11 bits, a line ending with
           a code that has trailing zero bits will cause problems. The longest run of
           trailing zeros for any code is 3, so we need to look for at least 14 zeros
           if we don't want to actually analyse the compressed data in depth. This means
           we do not strip every fill bit, but we strip most of them. */
        if ((s->non_ecm_to_modem.rx_bit_stream & 0x3FFF) == 0  &&  bit == 0)
            return;
    }
    s->non_ecm_to_modem.rx_bit_stream = (s->non_ecm_to_modem.rx_bit_stream << 1) | bit;
    if (++s->non_ecm_to_modem.bit_no >= 8)
    {
        s->to_t38.data[s->to_t38.data_ptr++] = (uint8_t) s->non_ecm_to_modem.rx_bit_stream & 0xFF;
        if (s->to_t38.data_ptr >= s->t38x.octets_per_data_packet)
        {
            t38_core_send_data(&s->t38x.t38, s->t38x.current_tx_data_type, T38_FIELD_T4_NON_ECM_DATA, s->to_t38.data, s->to_t38.data_ptr, s->t38x.t38.data_tx_count);
            /* Since we delay transmission by 2 octets, we should now have sent the last of the data octets when
               we have just received the last of the CRC octets. */
            s->to_t38.data_ptr = 0;
        }
        s->non_ecm_to_modem.bit_no = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static int non_ecm_get_bit(void *user_data)
{
    t38_gateway_state_t *s;
    int bit;

    /* A rate adapting data stuffer for non-ECM image data */
    s = (t38_gateway_state_t *) user_data;
    if (s->non_ecm_to_modem.bit_no <= 0)
    {
        /* We need another byte */
        if (s->non_ecm_to_modem.tx_out_ptr != s->non_ecm_to_modem.tx_latest_eol_ptr)
        {
            s->non_ecm_to_modem.tx_octet = s->non_ecm_to_modem.tx_data[s->non_ecm_to_modem.tx_out_ptr];
            s->non_ecm_to_modem.tx_out_ptr = (s->non_ecm_to_modem.tx_out_ptr + 1) & (T38_NON_ECM_TX_BUF_LEN - 1);
        }
        else
        {
            if (s->non_ecm_to_modem.data_finished)
            {
                /* The queue is empty, and we have received the end of data signal. This must
                   really be the end to transmission. */
                s->non_ecm_to_modem.data_finished = FALSE;
                /* Reset the data pointers for next time. */
                s->non_ecm_to_modem.tx_out_ptr = 0;
                s->non_ecm_to_modem.tx_in_ptr = 0;
                s->non_ecm_to_modem.tx_latest_eol_ptr = 0;
                return PUTBIT_END_OF_DATA;
            }
            /* The queue is blocked, but this does not appear to be the end of the data. Idle with
               fill octets, which should be safe at this point. */
            s->non_ecm_to_modem.tx_octet = s->non_ecm_to_modem.flow_control_fill_octet;
            s->non_ecm_to_modem.flow_control_fill_octets++;
        }
        s->non_ecm_to_modem.out_octets++;
        s->non_ecm_to_modem.bit_no = 8;
    }
    s->non_ecm_to_modem.bit_no--;
    bit = (s->non_ecm_to_modem.tx_octet >> 7) & 1;
    s->non_ecm_to_modem.tx_octet <<= 1;
    return bit;
}
/*- End of function --------------------------------------------------------*/

static void add_to_non_ecm_modem_buffer(t38_gateway_state_t *s, const uint8_t *buf, int len)
{
    int i;
    int upper;

    /* A rate adapting data stuffer for non-ECM image data */
    i = 0;
    if (s->non_ecm_to_modem.at_initial_all_ones)
    {
        /* Dump initial 0xFF bytes. We will add enough of our own to makes things flow
           smoothly. If we don't strip these off we might end up delaying the start of
           forwarding by a large amount, as we could end up with a large block of 0xFF
           bytes before the real data begins. This is especially true with PC FAX
           systems. */
        for (  ;  i < len;  i++)
        {
            if (buf[i] != 0xFF)
            {
                s->non_ecm_to_modem.at_initial_all_ones = FALSE;
                break;
            }
        }
    }
    if (s->image_data_mode)
    {
        /* This is image data */
        for (  ;  i < len;  i++)
        {
            /* Check for EOLs, because at an EOL we can pause and pump out zeros while
               waiting for more incoming data. */
            if (buf[i])
            {
                /* There might be an EOL here. Look for at least 11 zeros, followed by a one, split
                   between two octets. Between those two octets we can insert numerous zero octets
                   as a means of flow control. */
                /* Or'ing with 0x800 here is simply to avoid zero words looking like they have -1
                   trailing zeros */
                upper = bottom_bit(s->non_ecm_to_modem.bit_stream | 0x800);
                if (upper - top_bit(buf[i]) > 3)
                {
                    s->non_ecm_to_modem.in_rows++;
                    s->non_ecm_to_modem.tx_latest_eol_ptr = s->non_ecm_to_modem.tx_in_ptr;
                    s->non_ecm_to_modem.flow_control_fill_octet = 0x00;
                }
            }
            s->non_ecm_to_modem.bit_stream = (s->non_ecm_to_modem.bit_stream << 8) | buf[i];
            s->non_ecm_to_modem.tx_data[s->non_ecm_to_modem.tx_in_ptr] = buf[i];
            /* TODO: We can't buffer overflow, since we wrap around. However, the tail could overwrite
                     itself if things fall badly behind. */
            s->non_ecm_to_modem.tx_in_ptr = (s->non_ecm_to_modem.tx_in_ptr + 1) & (T38_NON_ECM_TX_BUF_LEN - 1);
            s->non_ecm_to_modem.in_octets++;
        }
    }
    else
    {
        /* This is TCF data */
        for (  ;  i < len;  i++)
        {
            /* Check for zero bytes, as we can pause and pump out zeros while waiting
               for more incoming data. */
            if (buf[i] == 0x00)
            {
                s->non_ecm_to_modem.tx_latest_eol_ptr = s->non_ecm_to_modem.tx_in_ptr;
                s->non_ecm_to_modem.flow_control_fill_octet = 0x00;
            }
            s->non_ecm_to_modem.tx_data[s->non_ecm_to_modem.tx_in_ptr] = buf[i];
            /* TODO: We can't buffer overflow, since we wrap around. However, the tail could overwrite
                     itself if things fall badly behind. */
            s->non_ecm_to_modem.tx_in_ptr = (s->non_ecm_to_modem.tx_in_ptr + 1) & (T38_NON_ECM_TX_BUF_LEN - 1);
            s->non_ecm_to_modem.in_octets++;
        }
    }
}
/*- End of function --------------------------------------------------------*/

static void hdlc_rx_special_condition(hdlc_rx_state_t *t, int condition)
{
    t38_gateway_state_t *s;

    s = (t38_gateway_state_t *) t->user_data;
    switch (condition)
    {
    case PUTBIT_TRAINING_IN_PROGRESS:
        span_log(&s->logging, SPAN_LOG_FLOW, "HDLC carrier training in progress\n");
        announce_training(s);
        break;
    case PUTBIT_TRAINING_FAILED:
        span_log(&s->logging, SPAN_LOG_FLOW, "HDLC carrier training failed\n");
        break;
    case PUTBIT_TRAINING_SUCCEEDED:
        /* The modem is now trained. */
        span_log(&s->logging, SPAN_LOG_FLOW, "HDLC carrier trained\n");
        s->audio.rx_signal_present = TRUE;
        s->audio.rx_trained = TRUE;
        /* Behave like HDLC preamble has been announced. */
        t->framing_ok_announced = TRUE;
        s->to_t38.data_ptr = 0;
        break;
    case PUTBIT_CARRIER_UP:
        span_log(&s->logging, SPAN_LOG_FLOW, "HDLC carrier up\n");
        /* Reset the HDLC receiver. */
        t->raw_bit_stream = 0;
        t->len = 0;
        t->num_bits = 0;
        t->flags_seen = 0;
        t->framing_ok_announced = FALSE;
        s->to_t38.data_ptr = 0;
        break;
    case PUTBIT_CARRIER_DOWN:
        span_log(&s->logging, SPAN_LOG_FLOW, "HDLC carrier down\n");
        if (t->framing_ok_announced)
        {
            t38_core_send_data(&s->t38x.t38, s->t38x.current_tx_data_type, T38_FIELD_HDLC_SIG_END, NULL, 0, s->t38x.t38.data_end_tx_count);
            t38_core_send_indicator(&s->t38x.t38, T38_IND_NO_SIGNAL, s->t38x.t38.indicator_tx_count);
            t->framing_ok_announced = FALSE;
        }
        restart_rx_modem(s);
        if (s->tcf_mode_predictable_modem_start == 2)
        {
            /* If we are doing TCF, we need to announce the fast carrier training very
               quickly, to ensure it starts 75+-20ms after the HDLC carrier ends. Waiting until
               it trains will be too late. We need to announce the fast modem a fixed time after
               the end of the V.21 carrier, in anticipation of its arrival. */
            s->samples_to_timeout = ms_to_samples(75);
            s->tcf_mode_predictable_modem_start = 1;
        }
        break;
    default:
        span_log(&s->logging, SPAN_LOG_WARNING, "Unexpected HDLC special bit - %d!\n", condition);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void rx_flag_or_abort(hdlc_rx_state_t *t)
{
    t38_gateway_state_t *s;

    s = (t38_gateway_state_t *) t->user_data;
    if ((t->raw_bit_stream & 0x80))
    {
        /* Hit HDLC abort */
        t->rx_aborts++;
        if (t->flags_seen < t->framing_ok_threshold)
            t->flags_seen = 0;
        else
            t->flags_seen = t->framing_ok_threshold - 1;
    }
    else
    {
        /* Hit HDLC flag */
        if (t->flags_seen >= t->framing_ok_threshold)
        {
            if (t->len)
            {
                /* This is not back-to-back flags */
                if (t->len >= 2)
                {
                    if (s->to_t38.data_ptr)
                    {
                        bit_reverse(s->to_t38.data, t->buffer + t->len - 2 - s->to_t38.data_ptr, s->to_t38.data_ptr);
                        t38_core_send_data(&s->t38x.t38, s->t38x.current_tx_data_type, T38_FIELD_HDLC_DATA, s->to_t38.data, s->to_t38.data_ptr, s->t38x.t38.data_tx_count);
                    }
                    if (t->num_bits != 7)
                    {
                        t->rx_crc_errors++;
                        span_log(&s->logging, SPAN_LOG_FLOW, "HDLC frame type %s, misaligned terminating flag at %d\n", t30_frametype(t->buffer[2]), t->len);
                        /* It seems some boxes may not like us sending a _SIG_END here, and then another
                           when the carrier actually drops. Lets just send T38_FIELD_HDLC_FCS_OK here. */
                        if (t->len > 2)
                            t38_core_send_data(&s->t38x.t38, s->t38x.current_tx_data_type, T38_FIELD_HDLC_FCS_BAD, NULL, 0, s->t38x.t38.data_tx_count);
                    }
                    else if ((s->audio.crc & 0xFFFF) != 0xF0B8)
                    {
                        t->rx_crc_errors++;
                        span_log(&s->logging, SPAN_LOG_FLOW, "HDLC frame type %s, bad CRC at %d\n", t30_frametype(t->buffer[2]), t->len);
                        /* It seems some boxes may not like us sending a _SIG_END here, and then another
                           when the carrier actually drops. Lets just send T38_FIELD_HDLC_FCS_OK here. */
                        if (t->len > 2)
                            t38_core_send_data(&s->t38x.t38, s->t38x.current_tx_data_type, T38_FIELD_HDLC_FCS_BAD, NULL, 0, s->t38x.t38.data_tx_count);
                    }
                    else
                    {
                        t->rx_frames++;
                        t->rx_bytes += t->len - 2;
                        span_log(&s->logging, SPAN_LOG_FLOW, "HDLC frame type %s, CRC OK\n", t30_frametype(t->buffer[2]));
                        if (s->t38x.current_tx_data_type == T38_DATA_V21)
                        {
                            monitor_control_messages(s, TRUE, t->buffer, t->len - 2);
                            if (s->real_time_frame_handler)
                                s->real_time_frame_handler(s, s->real_time_frame_user_data, TRUE, t->buffer, t->len - 2);
                        }
                        else
                        {
                            /* Make sure we go back to short training if CTC/CTR has kicked us into
                               long training. Any successful HDLC frame received at a rate other than
                               V.21 is an adequate indication we should change. */
                            s->audio.short_train = TRUE;
                        }

                        /* It seems some boxes may not like us sending a _SIG_END here, and then another
                           when the carrier actually drops. Lets just send T38_FIELD_HDLC_FCS_OK here. */
                        t38_core_send_data(&s->t38x.t38, s->t38x.current_tx_data_type, T38_FIELD_HDLC_FCS_OK, NULL, 0, s->t38x.t38.data_tx_count);
                    }
                }
                else
                {
                    /* Frame too short */
                    t->rx_length_errors++;
                }
            }
        }
        else
        {
            /* Check the flags are back-to-back when testing for valid preamble. This
               greatly reduces the chances of false preamble detection, and anything
               which doesn't send them back-to-back is badly broken. */
            if (t->num_bits != 7)
                t->flags_seen = 0;
            if (++t->flags_seen >= t->framing_ok_threshold  &&  !t->framing_ok_announced)
            {
                if (s->t38x.current_tx_data_type == T38_DATA_V21)
                {
                    t38_core_send_indicator(&s->t38x.t38, set_slow_packetisation(s), s->t38x.t38.indicator_tx_count);
                    s->audio.rx_signal_present = TRUE;
                }
                if (s->t38x.in_progress_rx_indicator == T38_IND_CNG)
                    set_next_tx_type(s);
                t->framing_ok_announced = TRUE;
            }
        }
    }
    s->audio.crc = 0xFFFF;
    t->len = 0;
    t->num_bits = 0;
    s->to_t38.data_ptr = 0;
    s->t38x.corrupt_current_frame[1] = FALSE;
}
/*- End of function --------------------------------------------------------*/

static void t38_hdlc_rx_put_bit(hdlc_rx_state_t *t, int new_bit)
{
    t38_gateway_state_t *s;

    if (new_bit < 0)
    {
        hdlc_rx_special_condition(t, new_bit);
        return;
    }
    t->raw_bit_stream = (t->raw_bit_stream << 1) | (new_bit & 1);
    if ((t->raw_bit_stream & 0x3F) == 0x3E)
    {
        /* Its time to either skip a bit, for stuffing, or process a flag or abort */
        if ((t->raw_bit_stream & 0x40))
            rx_flag_or_abort(t);
        return;
    }
    t->num_bits++;
    if (!t->framing_ok_announced)
        return;
    t->byte_in_progress = (t->byte_in_progress >> 1) | ((t->raw_bit_stream & 0x01) << 7);
    if (t->num_bits != 8)
        return;
    t->num_bits = 0;
    if (t->len >= (int) sizeof(t->buffer))
    {
        /* This is too long. Abandon the frame, and wait for the next flag octet. */
        t->rx_length_errors++;
        t->flags_seen = t->framing_ok_threshold - 1;
        t->len = 0;
        return;
    }
    s = (t38_gateway_state_t *) t->user_data;
    t->buffer[t->len] = (uint8_t) t->byte_in_progress;
    /* Calculate the CRC progressively, before we start altering the frame */
    s->audio.crc = crc_itu16_calc(&t->buffer[t->len], 1, s->audio.crc);
    /* Make the transmission lag by two octets, so we do not send the CRC, and
       do not report the CRC result too late. */
    if (++t->len <= 2)
        return;
    if (s->t38x.current_tx_data_type == T38_DATA_V21)
    {
        /* The V.21 control messages need to be monitored, and possibly corrupted, to manage the
           man-in-the-middle role of T.38 */
        edit_control_messages(s, 1, t->buffer, t->len);
    }
    if (++s->to_t38.data_ptr >= s->t38x.octets_per_data_packet)
    {
        bit_reverse(s->to_t38.data, t->buffer + t->len - 2 - s->to_t38.data_ptr, s->to_t38.data_ptr);
        t38_core_send_data(&s->t38x.t38, s->t38x.current_tx_data_type, T38_FIELD_HDLC_DATA, s->to_t38.data, s->to_t38.data_ptr, s->t38x.t38.data_tx_count);
        /* Since we delay transmission by 2 octets, we should now have sent the last of the data octets when
           we have just received the last of the CRC octets. */
        s->to_t38.data_ptr = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static int restart_rx_modem(t38_gateway_state_t *s)
{
    put_bit_func_t put_bit_func;
    void *put_bit_user_data;

    span_log(&s->logging, SPAN_LOG_FLOW, "Restart rx modem - modem = %d, short train = %d, ECM = %d\n", s->fast_modem, s->audio.short_train, s->ecm_mode);

    hdlc_rx_init(&(s->audio.modems.hdlc_rx), FALSE, TRUE, 5, NULL, s);
    s->audio.crc = 0xFFFF;
    s->audio.rx_signal_present = FALSE;
    s->audio.rx_trained = FALSE;
    /* Default to the transmit data being V.21, unless a faster modem pops up trained. */
    s->t38x.current_tx_data_type = T38_DATA_V21;
    fsk_rx_init(&(s->audio.modems.v21_rx), &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) t38_hdlc_rx_put_bit, &(s->audio.modems.hdlc_rx));
#if 0
    fsk_rx_signal_cutoff(&(s->audio.modems.v21_rx), -45.5);
#endif
    if (s->image_data_mode  &&  s->ecm_mode)
    {
        put_bit_func = (put_bit_func_t) t38_hdlc_rx_put_bit;
        put_bit_user_data = (void *) &(s->audio.modems.hdlc_rx);
    }
    else
    {
        put_bit_func = non_ecm_put_bit;
        put_bit_user_data = (void *) s;
    }
    s->to_t38.data_ptr = 0;
    s->t38x.octets_per_data_packet = 1;
    switch (s->fast_modem)
    {
    case T38_V17_RX:
        v17_rx_restart(&(s->audio.modems.v17_rx), s->fast_bit_rate, s->audio.short_train);
        v17_rx_set_put_bit(&(s->audio.modems.v17_rx), put_bit_func, put_bit_user_data);
        set_rx_handler(s, (span_rx_handler_t *) &early_v17_rx, s);
        s->fast_rx_active = T38_V17_RX;
        break;
    case T38_V27TER_RX:
        v27ter_rx_restart(&(s->audio.modems.v27ter_rx), s->fast_bit_rate, FALSE);
        v27ter_rx_set_put_bit(&(s->audio.modems.v27ter_rx), put_bit_func, put_bit_user_data);
        set_rx_handler(s, (span_rx_handler_t *) &early_v27ter_rx, s);
        s->fast_rx_active = T38_V27TER_RX;
        break;
    case T38_V29_RX:
        v29_rx_restart(&(s->audio.modems.v29_rx), s->fast_bit_rate, FALSE);
        v29_rx_set_put_bit(&(s->audio.modems.v29_rx), put_bit_func, put_bit_user_data);
        set_rx_handler(s, (span_rx_handler_t *) &early_v29_rx, s);
        s->fast_rx_active = T38_V29_RX;
        break;
    default:
        set_rx_handler(s, (span_rx_handler_t *) &fsk_rx, &(s->audio.modems.v21_rx));
        s->fast_rx_active = T38_NONE;
        break;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int t38_gateway_rx(t38_gateway_state_t *s, int16_t amp[], int len)
{
    int i;

#if defined(LOG_FAX_AUDIO)
    if (s->audio.fax_audio_rx_log >= 0)
        write(s->audio.fax_audio_rx_log, amp, len*sizeof(int16_t));
#endif
    if (s->samples_to_timeout > 0)
    {
        if ((s->samples_to_timeout -= len) <= 0)
        {
            if (s->tcf_mode_predictable_modem_start == 1)
                announce_training(s);
        }
    }
    for (i = 0;  i < len;  i++)
        amp[i] = dc_restore(&(s->audio.modems.dc_restore), amp[i]);
    s->audio.immediate_rx_handler(s->audio.rx_user_data, amp, len);
    return  0;
}
/*- End of function --------------------------------------------------------*/

int t38_gateway_tx(t38_gateway_state_t *s, int16_t amp[], int max_len)
{
    int len;
#if defined(LOG_FAX_AUDIO)
    int required_len;
    
    required_len = max_len;
#endif
    if ((len = s->audio.tx_handler(s->audio.tx_user_data, amp, max_len)) < max_len)
    {
        if (set_next_tx_type(s))
        {
            /* Give the new handler a chance to file the remaining buffer space */
            len += s->audio.tx_handler(s->audio.tx_user_data, amp + len, max_len - len);
            if (len < max_len)
            {
                silence_gen_set(&(s->audio.modems.silence_gen), 0);
                set_next_tx_type(s);
            }
        }
    }
    if (s->audio.transmit_on_idle)
    {
        if (len < max_len)
        {
            /* Pad to the requested length with silence */
            memset(amp + len, 0, (max_len - len)*sizeof(int16_t));
            len = max_len;        
        }
    }
#if defined(LOG_FAX_AUDIO)
    if (s->audio.fax_audio_tx_log >= 0)
    {
        if (len < required_len)
            memset(amp + len, 0, (required_len - len)*sizeof(int16_t));
        write(s->audio.fax_audio_tx_log, amp, required_len*sizeof(int16_t));
    }
#endif
    return len;
}
/*- End of function --------------------------------------------------------*/

void t38_gateway_get_transfer_statistics(t38_gateway_state_t *s, t38_stats_t *t)
{
    memset(t, 0, sizeof(*t));
    t->bit_rate = s->fast_bit_rate;
    t->error_correcting_mode = s->ecm_mode;
    t->pages_transferred = s->pages_confirmed;
}
/*- End of function --------------------------------------------------------*/

void t38_gateway_set_ecm_capability(t38_gateway_state_t *s, int ecm_allowed)
{
    s->ecm_allowed = ecm_allowed;
}
/*- End of function --------------------------------------------------------*/

void t38_gateway_set_transmit_on_idle(t38_gateway_state_t *s, int transmit_on_idle)
{
    s->audio.transmit_on_idle = transmit_on_idle;
}
/*- End of function --------------------------------------------------------*/

void t38_gateway_set_supported_modems(t38_gateway_state_t *s, int supported_modems)
{
    s->supported_modems = supported_modems;
    if ((s->supported_modems & T30_SUPPORT_V17))
        s->t38x.t38.fastest_image_data_rate = 14400;
    else if ((s->supported_modems & T30_SUPPORT_V29))
        s->t38x.t38.fastest_image_data_rate = 9600;
    else
        s->t38x.t38.fastest_image_data_rate = 4800;
}
/*- End of function --------------------------------------------------------*/

void t38_gateway_set_nsx_suppression(t38_gateway_state_t *s,
                                     const uint8_t *from_t38,
                                     int from_t38_len,
                                     const uint8_t *from_modem,
                                     int from_modem_len)
{
    s->t38x.suppress_nsx_len[0] = (from_t38_len < 0  ||  from_t38_len < MAX_NSX_SUPPRESSION)  ?  (from_t38_len + 3)  :  0;
    s->t38x.suppress_nsx_len[1] = (from_modem_len < 0  ||  from_modem_len < MAX_NSX_SUPPRESSION)  ?  (from_modem_len + 3)  :  0;
}
/*- End of function --------------------------------------------------------*/

void t38_gateway_set_tep_mode(t38_gateway_state_t *s, int use_tep)
{
    s->audio.use_tep = use_tep;
}
/*- End of function --------------------------------------------------------*/

void t38_gateway_set_real_time_frame_handler(t38_gateway_state_t *s,
                                             t38_gateway_real_time_frame_handler_t *handler,
                                             void *user_data)
{
    s->real_time_frame_handler = handler;
    s->real_time_frame_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

static int t38_gateway_audio_init(t38_gateway_state_t *t)
{
    t38_gateway_audio_state_t *s;
    
    s = &t->audio;

    fsk_rx_init(&s->modems.v21_rx, &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) t38_hdlc_rx_put_bit, &s->modems.hdlc_rx);
#if 0
    fsk_rx_signal_cutoff(&s->modems.v21_rx, -45.5);
#endif
    fsk_tx_init(&s->modems.v21_tx, &preset_fsk_specs[FSK_V21CH2], (get_bit_func_t) hdlc_tx_get_bit, &s->modems.hdlc_tx);
    v17_rx_init(&s->modems.v17_rx, 14400, non_ecm_put_bit, t);
    v17_tx_init(&s->modems.v17_tx, 14400, s->use_tep, non_ecm_get_bit, t);
    v29_rx_init(&s->modems.v29_rx, 9600, non_ecm_put_bit, t);
#if 0
    v29_rx_signal_cutoff(&s->modems.v29_rx, -45.5);
#endif
    v29_tx_init(&s->modems.v29_tx, 9600, s->use_tep, non_ecm_get_bit, t);
    v27ter_rx_init(&s->modems.v27ter_rx, 4800, non_ecm_put_bit, t);
    v27ter_tx_init(&s->modems.v27ter_tx, 4800, s->use_tep, non_ecm_get_bit, t);
    silence_gen_init(&s->modems.silence_gen, 0);
    hdlc_rx_init(&s->modems.hdlc_rx, FALSE, TRUE, 5, NULL, t);
    hdlc_tx_init(&s->modems.hdlc_tx, FALSE, 2, TRUE, hdlc_underflow_handler, t);
    s->rx_signal_present = FALSE;
    s->tx_handler = (span_tx_handler_t *) &silence_gen;
    s->tx_user_data = &s->modems.silence_gen;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int t38_gateway_t38_init(t38_gateway_state_t *t,
                                t38_tx_packet_handler_t *tx_packet_handler,
                                void *tx_packet_user_data)
{
    t38_gateway_t38_state_t *s;
    
    s = &t->t38x;

    s->octets_per_data_packet = 1;
    t38_core_init(&s->t38,
                  process_rx_indicator,
                  process_rx_data,
                  process_rx_missing,
                  (void *) t,
                  tx_packet_handler,
                  tx_packet_user_data);
    s->t38.indicator_tx_count = INDICATOR_TX_COUNT;
    s->t38.data_tx_count = DATA_TX_COUNT;
    s->t38.data_end_tx_count = DATA_END_TX_COUNT;
    return 0;
}
/*- End of function --------------------------------------------------------*/

t38_gateway_state_t *t38_gateway_init(t38_gateway_state_t *s,
                                      t38_tx_packet_handler_t *tx_packet_handler,
                                      void *tx_packet_user_data)
{
    if (tx_packet_handler == NULL)
        return NULL;

    if (s == NULL)
    {
        if ((s = (t38_gateway_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.38G");

    t38_gateway_audio_init(s);
    t38_gateway_t38_init(s, tx_packet_handler, tx_packet_user_data);
    
    set_rx_active(s, TRUE);
    t38_gateway_set_supported_modems(s, T30_SUPPORT_V27TER | T30_SUPPORT_V29);
    t38_gateway_set_nsx_suppression(s, (const uint8_t *) "\x00\x00\x00", 3, (const uint8_t *) "\x00\x00\x00", 3);

    s->ecm_allowed = FALSE;
    restart_rx_modem(s);
#if defined(LOG_FAX_AUDIO)
    {
        char buf[100 + 1];
        struct tm *tm;
        time_t now;

        time(&now);
        tm = localtime(&now);
        sprintf(buf,
                "/tmp/t38-rx-audio-%p-%02d%02d%02d%02d%02d%02d",
                s,
                tm->tm_year%100,
                tm->tm_mon + 1,
                tm->tm_mday,
                tm->tm_hour,
                tm->tm_min,
                tm->tm_sec);
        s->audio.fax_audio_rx_log = open(buf, O_CREAT | O_TRUNC | O_WRONLY, 0666);
        sprintf(buf,
                "/tmp/t38-tx-audio-%p-%02d%02d%02d%02d%02d%02d",
                s,
                tm->tm_year%100,
                tm->tm_mon + 1,
                tm->tm_mday,
                tm->tm_hour,
                tm->tm_min,
                tm->tm_sec);
        s->audio.fax_audio_tx_log = open(buf, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    }
#endif
    return s;
}
/*- End of function --------------------------------------------------------*/

int t38_gateway_free(t38_gateway_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v8.h - V.8 modem negotiation processing.
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
 * $Id: v8.c,v 1.1 2004/07/24 11:46:55 steveu Exp $
 */
 
/*! \file */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "spandsp/telephony.h"
#include "spandsp/queue.h"
#include "spandsp/biquad.h"
#include "spandsp/ec_disable_detector.h"
#include "spandsp/power_meter.h"
#include "spandsp/fsk.h"
#include "spandsp/v8.h"

enum
{
    V8_WAIT_1S,
    V8_CI,
    V8_CI_ON,
    V8_CI_OFF,
    V8_HEARD_ANSAM,
    V8_CM_ON,
    V8_CJ_ON,
    V8_CM_WAIT,

    V8_SIGC,
    V8_WAIT_200MS,
    V8_JM_ON,
    V8_SIGA,

    V8_PARKED
} v8_states_e;

#define V8_CI_SYNC      1
#define V8_CM_SYNC      2

void log_supported_modulations(int modulation_schemes)
{
    if (modulation_schemes & V8_MOD_V17)
        fprintf(stderr, "V.17 supported\n");
    if (modulation_schemes & V8_MOD_V21)
        fprintf(stderr, "V.21 supported\n");
    if (modulation_schemes & V8_MOD_V22)
        fprintf(stderr, "V.22/V.22bis supported\n");
    if (modulation_schemes & V8_MOD_V23HALF)
        fprintf(stderr, "V.23 half-duplex supported\n");
    if (modulation_schemes & V8_MOD_V23)
        fprintf(stderr, "V.23 duplex supported\n");
    if (modulation_schemes & V8_MOD_V26BIS)
        fprintf(stderr, "V.26bis supported\n");
    if (modulation_schemes & V8_MOD_V26TER)
        fprintf(stderr, "V.26ter supported\n");
    if (modulation_schemes & V8_MOD_V27TER)
        fprintf(stderr, "V.27ter supported\n");
    if (modulation_schemes & V8_MOD_V29)
        fprintf(stderr, "V.29 half-duplex supported\n");
    if (modulation_schemes & V8_MOD_V32)
        fprintf(stderr, "V.32/V.32bis supported\n");
    if (modulation_schemes & V8_MOD_V34HALF)
        fprintf(stderr, "V.34 half-duplex supported\n");
    if (modulation_schemes & V8_MOD_V34)
        fprintf(stderr, "V.34 duplex supported\n");
    if (modulation_schemes & V8_MOD_V90)
        fprintf(stderr, "V.90/V.92 supported\n");
}

static void ci_decode(v8_state_t *s)
{
    if (s->rx_data[0] == 0xC1)
        fprintf(stderr, "CI: data call\n");
}

static void cm_jm_decode(v8_state_t *s)
{
    uint8_t *p;
    int c;
    int mask;

    if (s->got_cm_jm)
        return;

    /* We must receive two consecutive identical CM or JM sequences to accept it. */
    if (s->cm_jm_count <= 0
        ||
        s->cm_jm_count != s->rx_data_ptr
        ||
        memcmp(s->cm_jm_data, s->rx_data, s->rx_data_ptr))
    {
        /* Save the current CM or JM sequence */
        s->cm_jm_count = s->rx_data_ptr;
        memcpy(s->cm_jm_data, s->rx_data, s->rx_data_ptr);
        return;
    }
    /* We have a pair of matching CMs or JMs */
    s->got_cm_jm = TRUE;

printf("decoding\n");

    s->far_end_modulations = 0;
    p = s->cm_jm_data;

    /* Zero indicates the end */
    s->cm_jm_data[s->cm_jm_count] = 0;

    /* Call function octet */
    if ((*p & 0x1F) != 0x01) 
        return;

    if (*p == 0x01)
    {
        fprintf(stderr, "TBS\n");
    }
    else if (*p == 0x21)
    {
        fprintf(stderr, "H.324\n");
    }
    else if (*p == 0x41)
    {
        fprintf(stderr, "V.18\n");
    }
    else if (*p == 0x61)
    {
        fprintf(stderr, "T.101\n");
    }
    else if (*p == 0x81)
    {
        fprintf(stderr, "T.30 Tx\n");
    }
    else if (*p == 0xA1)
    {
        fprintf(stderr, "T.30 Rx\n");
    }
    else if (*p == 0xC1)
    {
        fprintf(stderr, "V series modem\n");
        /* Modulation octet */
        p++;
        if ((*p & 0x1F) != 0x05)
            return;

        if (*p & 0x80)
            s->far_end_modulations |= V8_MOD_V34HALF;
        if (*p & 0x40)
            s->far_end_modulations |= V8_MOD_V34;
        if (*p & 0x20)
            s->far_end_modulations |= V8_MOD_V90;

        if ((*++p & 0x38) == 0x10)
        {
            if (*p & 0x80)
                s->far_end_modulations |= V8_MOD_V27TER;
            if (*p & 0x40)
                s->far_end_modulations |= V8_MOD_V29;
            if (*p & 0x04)
                s->far_end_modulations |= V8_MOD_V17;
            if (*p & 0x02)
                s->far_end_modulations |= V8_MOD_V22;
            if (*p & 0x01)
                s->far_end_modulations |= V8_MOD_V32;
            if ((*++p & 0x38) == 0x10)
            {
                if (*p & 0x80)
                    s->far_end_modulations |= V8_MOD_V21;
                if (*p & 0x40)
                    s->far_end_modulations |= V8_MOD_V23HALF;
                if (*p & 0x04)
                    s->far_end_modulations |= V8_MOD_V23;
                if (*p & 0x02)
                    s->far_end_modulations |= V8_MOD_V26BIS;
                if (*p & 0x01)
                    s->far_end_modulations |= V8_MOD_V26TER;
                /* Skip any future extensions we do not understand */
                while  ((*++p & 0x38) == 0x10)
                    /* dummy loop */;
                log_supported_modulations(s->far_end_modulations);
            }
        }
    }
    else if (*p == 0xE1)
    {
        fprintf(stderr, "Call function is in extention octet\n");
    }
}

static void put_bit(void *user_data, int bit)
{
    v8_state_t *s;
    int new_preamble_type;
    int i;

    s = user_data;
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_CARRIER_UP:
        case PUTBIT_CARRIER_DOWN:
        case PUTBIT_TRAINING_SUCCEEDED:
        case PUTBIT_TRAINING_FAILED:
            break;
        default:
            break;
        }
        return;
    }
    /* Wait until we sync. */
    s->bit_stream = (s->bit_stream >> 1) | (bit << 19);
    if (s->bit_stream == 0x803FF)
        new_preamble_type = V8_CI_SYNC;
    else if (s->bit_stream == 0xF03FF)
        new_preamble_type = V8_CM_SYNC;
    else
        new_preamble_type = 0;
    if (new_preamble_type)
    {
        /* Debug */
        if (s->preamble_type == V8_CI_SYNC)
        {
            fprintf(stderr, "CI: ");
        }
        else if (s->preamble_type == V8_CM_SYNC)
        {
            if (s->caller)
                fprintf(stderr, "JM: ");
            else
                fprintf(stderr, "CM: ");
        }
        for (i = 0;  i < s->rx_data_ptr;  i++)
            fprintf(stderr, " %02x", s->rx_data[i]);
        fprintf(stderr, "\n");
        
        /* Decode previous sequence */
        switch (s->preamble_type)
        {
        case V8_CI_SYNC:
            ci_decode(s);
            break;
        case V8_CM_SYNC:
            cm_jm_decode(s);
            break;
        }
        s->preamble_type = new_preamble_type;
        s->bit_cnt = 0;
        s->rx_data_ptr = 0;
    }
    
    /* Parse octets with 1 bit start, 1 bit stop */
    if (s->preamble_type)
    {
        s->bit_cnt++;
        /* Start, stop? */
        if ((s->bit_stream & 0x80400) == 0x80000  &&  s->bit_cnt >= 10)
        {
            int data;

            /* Store the available data */
            data = (s->bit_stream >> 11) & 0xFF;
            /* CJ detection */
            if (data == 0)
            {
                if (++s->zero_byte_count == 3)
                    s->got_cj = TRUE;
            }
            else
            {
                s->zero_byte_count = 0;
            }

            if (s->rx_data_ptr < (sizeof(s->rx_data) - 1))
                s->rx_data[s->rx_data_ptr++] = data;
            s->bit_cnt = 0;
        }
    }
}

static void v8_decode_init(v8_state_t *s)
{
    if (s->caller)
        fsk_rx_init(&s->v21rx, &preset_fsk_specs[FSK_V21CH2], FALSE, put_bit, s);
    else
        fsk_rx_init(&s->v21rx, &preset_fsk_specs[FSK_V21CH1], FALSE, put_bit, s);
    s->preamble_type = 0;
    s->bit_stream = 0;
    s->cm_jm_count = 0;
    s->got_cm_jm = FALSE;
    s->got_cj = FALSE;
    s->zero_byte_count = 0;
    s->rx_data_ptr = 0;
}

static int get_bit(void *user_data)
{
    v8_state_t *s;
    uint8_t bit;

    s = user_data;
    if (queue_read(&s->tx_queue, &bit, 1) <= 0)
        bit = 1;
    return bit;
}

static void v8_put_byte(v8_state_t *s, int data)
{
    int i;
    uint8_t bits[10];

    /* Insert start & stop bits */
    bits[0] = 0;
    for (i = 1;  i < 9;  i++)
    {
        bits[i] = data & 1;
        data >>= 1;
    }
    bits[9] = 1;
    queue_write(&s->tx_queue, bits, 10);
}

static void send_cm_jm(v8_state_t *s, int mod_mask)
{
    int val;
    int i;
    static const uint8_t preamble[20] =
    {
        /* 10 1's (0x3FF), then 10 bits of CM sync (0x00F) */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1
    };

    /* Send a CM, or a JM as appropriate */
    queue_write(&s->tx_queue, preamble, 20);
    
    /* Data call */
    v8_put_byte(s, 0xC1);
    
    /* Supported modulations */
    val = 0x05;
    if (mod_mask & V8_MOD_V90)
        val |= 0x20;
    if (mod_mask & V8_MOD_V34)
        val |= 0x40;
    v8_put_byte(s, val);

    val = 0x10;
    if (mod_mask & V8_MOD_V32)
        val |= 0x01;
    if (mod_mask & V8_MOD_V22)
        val |= 0x02;
    if (mod_mask & V8_MOD_V17)
        val |= 0x04;
    if (mod_mask & V8_MOD_V29)
        val |= 0x40;
    if (mod_mask & V8_MOD_V27TER)
        val |= 0x80;
    v8_put_byte(s, val);

    val = 0x10;
    if (mod_mask & V8_MOD_V26TER)
        val |= 0x01;
    if (mod_mask & V8_MOD_V26BIS)
        val |= 0x02;
    if (mod_mask & V8_MOD_V23)
        val |= 0x04;
    if (mod_mask & V8_MOD_V23HALF)
        val |= 0x40;
    if (mod_mask & V8_MOD_V21)
        val |= 0x80;
    v8_put_byte(s, val);

    /* No LAPM right now */
    v8_put_byte(s, 0x2A);

    /* No cellular right now */    
    v8_put_byte(s, 0x0D);
}

static int select_modulation(int mask)
{
    if (mask & V8_MOD_V90)
        return V8_MOD_V90;
    if (mask & V8_MOD_V34)
        return V8_MOD_V34;
    if (mask & V8_MOD_V32)
        return V8_MOD_V32;
    if (mask & V8_MOD_V23)
        return V8_MOD_V23;
    if (mask & V8_MOD_V21)
        return V8_MOD_V21;
    return V8_MOD_FAILED;
}

int v8_tx(v8_state_t *s, int16_t *amp, int max_samples)
{
    int samples;

    //fprintf(stderr, "v8_tx state %d\n", s->state);
    samples = 0;
    switch (s->state)
    {
    case V8_CI_ON:
    case V8_CM_ON:
    case V8_JM_ON:
    case V8_CJ_ON:
        samples = fsk_tx(&s->v21tx, amp, max_samples);
        break;
    case V8_CM_WAIT:
        /* Send the ANSam tone */
        samples = echo_can_disable_tone_tx(&s->v8_tx, amp, max_samples);
        break;
    }
    return samples;
}

int v8_rx(v8_state_t *s, const int16_t *amp, int samples)
{
    int i;
    int residual_samples;
    static const uint8_t preamble[20] =
    {
        /* 10 1's (0x3FF), then 10 bits of CI sync (0x001) */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
    };

    //fprintf(stderr, "v8_rx state %d\n", s->state);
    residual_samples = 0;
    switch (s->state)
    {
    case V8_WAIT_1S:
        /* Wait 1 second before sending the first CI packet */
        if ((s->negotiation_timer -= samples) > 0)
            break;
        s->state = V8_CI;
        s->ci_count = 0;
        echo_can_disable_tone_rx_init(&s->v8_rx);
        fsk_tx_init(&s->v21tx, &preset_fsk_specs[FSK_V21CH1], get_bit, s);
        /* Fall through to the next state */
    case V8_CI:
        residual_samples = echo_can_disable_tone_rx(&s->v8_rx, amp, samples);
        /* Send 4 CI packets in a burst (the spec says at least 3) */
        for (i = 0;  i < 4;  i++)
        {
            /* 10 1's (0x3FF), then CI sync (0x001) */
            queue_write(&s->tx_queue, preamble, 20);
            v8_put_byte(s, 0xC1);
        }
        s->state = V8_CI_ON;
        break;
    case V8_CI_ON:
        residual_samples = echo_can_disable_tone_rx(&s->v8_rx, amp, samples);
        if (queue_empty(&s->tx_queue))
        {
            s->state = V8_CI_OFF;
            s->ci_timer = 500*8;
        }
        break;
    case V8_CI_OFF:
        residual_samples = echo_can_disable_tone_rx(&s->v8_rx, amp, samples);
        /* Check if an ANSam tone has been detected */
        if (s->v8_rx.hit)
        {
            /* Set the Te interval. The spec. says 500ms is the minimum,
               but gives reasons why 1 second is a better value. */
            s->ci_timer = 1000*8;
            s->state = V8_HEARD_ANSAM;
            break;
        }
        if ((s->ci_timer -= samples) <= 0)
        {
            if (++s->ci_count >= 10)
            {
                /* The spec says we should give up now. */
                s->state = V8_PARKED;
                if (s->result_handler)
                    s->result_handler(s->result_handler_user_data, V8_MOD_FAILED);
            }
            else
            {
                /* Try again */
                s->state = V8_CI;
            }
        }
        break;
    case V8_HEARD_ANSAM:
        /* We have heard the ANSam signal, but we still need to wait for the
           end of the Te timeout period to comply with the spec. */
        if ((s->ci_timer -= samples) <= 0)
        {
            v8_decode_init(s);
            s->state = V8_CM_ON;
            s->negotiation_timer = 5000*8;
            send_cm_jm(s, s->available_modulations);
	}
        break;
    case V8_CM_ON:
        residual_samples = fsk_rx(&s->v21rx, amp, samples);
        if (s->got_cm_jm)
        {
            /* Now JM has been detected we send CJ and wait for 75 ms
               before finishing the V.8 analysis. */
            s->selected_modulation = select_modulation(s->far_end_modulations);

            queue_flush(&s->tx_queue);
            v8_put_byte(s, 0);
            v8_put_byte(s, 0);
            v8_put_byte(s, 0);
            v8_put_byte(s, 0);
            v8_put_byte(s, 0);
            v8_put_byte(s, 0);
            v8_put_byte(s, 0);
            v8_put_byte(s, 0);
            v8_put_byte(s, 0);
            s->state = V8_CJ_ON;
            break;
        }
        if ((s->negotiation_timer -= samples) <= 0)
        {
            /* Timeout */
            s->state = V8_PARKED;
            if (s->result_handler)
                s->result_handler(s->result_handler_user_data, V8_MOD_FAILED);
        }
        if (queue_empty(&s->tx_queue))
        {
            /* Send CM again */
            send_cm_jm(s, s->available_modulations);
        }
        break;
    case V8_CJ_ON:
        residual_samples = fsk_rx(&s->v21rx, amp, samples);
        if (queue_empty(&s->tx_queue))
        {
            s->negotiation_timer = 75*8;
            s->state = V8_SIGC;
        }
        break;
    case V8_SIGC:
        if ((s->negotiation_timer -= samples) <= 0)
        {
            /* The V.8 negotiation has succeeded. */
            s->state = V8_PARKED;
            if (s->result_handler)
                s->result_handler(s->result_handler_user_data, s->selected_modulation);
        }
        break;
    case V8_WAIT_200MS:
        if ((s->negotiation_timer -= samples) <= 0)
        {
            /* Send the ANSam tone */
            echo_can_disable_tone_tx_init(&s->v8_tx, TRUE);
                
            v8_decode_init(s);
            s->state = V8_CM_WAIT;
            s->negotiation_timer = 5000*8;
        }
        break;
    case V8_CM_WAIT:
        residual_samples = fsk_rx(&s->v21rx, amp, samples);
        if (s->got_cm_jm)
        {
            /* Stop sending ANSam and send JM instead */
            fsk_tx_init(&s->v21tx, &preset_fsk_specs[FSK_V21CH2], get_bit, s);
            /* Set the timeout for JM */
            s->negotiation_timer = 5000*8; 
            s->state = V8_JM_ON;
            s->common_modulations = s->available_modulations & s->far_end_modulations;
            s->selected_modulation = select_modulation(s->common_modulations);
            send_cm_jm(s, s->common_modulations);
            break;
        }
        if ((s->negotiation_timer -= samples) <= 0)
        {
            /* Timeout */
            s->state = V8_PARKED;
            if (s->result_handler)
                s->result_handler(s->result_handler_user_data, V8_MOD_FAILED);
        }
        break;
    case V8_JM_ON:
        residual_samples = fsk_rx(&s->v21rx, amp, samples);
        if (s->got_cj)
        {
            /* Stop sending JM, and wait 75 ms */
            s->negotiation_timer = 75*8;
            s->state = V8_SIGA;
            break;
        }
        if ((s->negotiation_timer -= samples) <= 0)
        {
            /* Timeout */
            s->state = V8_PARKED;
            if (s->result_handler)
                s->result_handler(s->result_handler_user_data, V8_MOD_FAILED);
            break;
        }
        if (queue_empty(&s->tx_queue))
        {
            /* Send JM */
            send_cm_jm(s, s->common_modulations);
        }
        break;
    case V8_SIGA:
        if ((s->negotiation_timer -= samples) <= 0)
        {
            /* The V.8 negotiation has succeeded. */
            s->state = V8_PARKED;
            if (s->result_handler)
                s->result_handler(s->result_handler_user_data, s->selected_modulation);
        }
        break;
    case V8_PARKED:
        residual_samples = samples;
        break;
    }
    return residual_samples;
}

v8_state_t *v8_init(v8_state_t *s,
                    int caller,
                    int available_modulations,
                    v8_result_handler_t *result_handler,
                    void *user_data)
{
    memset(s, 0, sizeof(*s));
    s->caller = caller;
    s->available_modulations = available_modulations;
    s->result_handler = result_handler;
    s->result_handler_user_data = user_data;

    s->ci_timer = 0;
    if (s->caller)
    {
        s->state = V8_WAIT_1S;
        s->negotiation_timer = 1000*8;
    }
    else
    {
        s->state = V8_WAIT_200MS;
        s->negotiation_timer = 200*8;
    }
    if (queue_create(&s->tx_queue, 1024, 0))
        return NULL;
    return s;
}

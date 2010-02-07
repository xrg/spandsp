/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v42.c
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
 * $Id: v42.c,v 1.15 2005/12/25 17:33:37 steveu Exp $
 */

/* THIS IS A WORK IN PROGRESS. IT IS NOT FINISHED. */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/async.h"
#include "spandsp/hdlc.h"
#include "spandsp/schedule.h"
#include "spandsp/queue.h"
#include "spandsp/v42.h"

#define FALSE   0
#define TRUE    (!FALSE)

#define LAPM_FRAMETYPE_MASK         0x03

#define LAPM_FRAMETYPE_I            0x00
#define LAPM_FRAMETYPE_I_ALT        0x02
#define LAPM_FRAMETYPE_S            0x01
#define LAPM_FRAMETYPE_U            0x03

/* Timer values */

#define T_WAIT_MIN                  2000
#define T_WAIT_MAX                  10000
#define T_400                       750         /* Detection phase timer */
#define T_401                       1000        /* 1 second between SABME's */
#define T_403                       10000       /* 10 seconds with no packets */
#define N_400                       3           /* Max retries */
#define N_401                       128         /* Max octets in an information field */

#define LAPM_DLCI_DTE_TO_DTE        0
#define LAPM_DLCI_LAYER2_MANAGEMENT 63

static void t401_expired(sp_sched_state_t *s, void *user_data);
static void t403_expired(sp_sched_state_t *s, void *user_data);

void lapm_reset(lapm_state_t *s);
void lapm_restart(lapm_state_t *s);

static void lapm_link_down(lapm_state_t *s);

static __inline__ void lapm_init_header(uint8_t *frame, int command)
{
    /* Data link connection identifier (0) */
    /* Command/response (0 if answerer, 1 if originator) */
    /* Extended address (1) */
    frame[0] = (LAPM_DLCI_DTE_TO_DTE << 2) | ((command)  ?  0x02  :  0x00) | 0x01;
}
/*- End of function --------------------------------------------------------*/

static int lapm_tx_frame(lapm_state_t *s, uint8_t *frame, int len)
{
    int res;

    if ((s->debug & LAPM_DEBUG_LAPM_DUMP))
        lapm_dump(s, frame, len, s->debug & LAPM_DEBUG_LAPM_RAW, TRUE);
    /*endif*/
    hdlc_tx_frame(&s->hdlc_tx, frame, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void t400_expired(sp_sched_state_t *ss, void *user_data)
{
    v42_state_t *s;
    
    /* Give up trying to detect a V.42 capable peer. */
    s = (v42_state_t *) user_data;
    s->t400_timer = -1;
    s->lapm.state = LAPM_UNSUPPORTED;
    if (s->lapm.status_callback)
        s->lapm.status_callback(s->lapm.status_callback_user_data, s->lapm.state);
}
/*- End of function --------------------------------------------------------*/

static void lapm_send_ua(lapm_state_t *s, int pfbit)
{
    uint8_t frame[3];

    lapm_init_header(frame, !s->we_are_originator);
    frame[1] = 0x63 | (pfbit << 4);
    frame[2] = 0;
    if ((s->debug & LAPM_DEBUG_LAPM_STATE))
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending unnumbered acknowledgement\n");
    /*endif*/
    lapm_tx_frame(s, frame, 3);
}
/*- End of function --------------------------------------------------------*/

static void lapm_send_sabme(sp_sched_state_t *ss, void *user_data)
{
    lapm_state_t *s;
    uint8_t frame[3];

    s = (lapm_state_t *) user_data;
    s->t401_timer = sp_schedule_event(&s->sched, T_401, lapm_send_sabme, s);
    lapm_init_header(frame, s->we_are_originator);
    frame[1] = 0x7F;
    frame[2] = 0;
    if ((s->debug & LAPM_DEBUG_LAPM_STATE))
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending Set Asynchronous Balanced Mode Extended\n");
    /*endif*/
    lapm_tx_frame(s, frame, 3);
    s->state = LAPM_ESTABLISH;
}
/*- End of function --------------------------------------------------------*/

static int lapm_ack_packet(lapm_state_t *s, int num)
{
    lapm_frame_queue_t *f;
    lapm_frame_queue_t *prev;

    for (prev = NULL, f = s->txqueue;  f;  prev = f, f = f->next)
    {
        if ((f->frame[1] >> 1) == num)
        {
            /* Cancel each packet, as necessary */
            if (prev)
                prev->next = f->next;
            else
                s->txqueue = f->next;
            /*endif*/
            if ((s->debug & LAPM_DEBUG_LAPM_STATE))
            {
                span_log(&s->logging,
                         SPAN_LOG_FLOW,
                         "-- ACKing packet %d. New txqueue is %d (-1 means empty)\n",
                         (f->frame[1] >> 1),
                         (s->txqueue)  ?  (s->txqueue->frame[1] >> 1)  :  -1);
            }
            /*endif*/
            s->last_frame_peer_acknowledged = num;
            free(f);
            /* Reset retransmission count if we actually acked something */
            s->retransmissions = 0;
            return 1;
        }
        /*endif*/
    }
    /*endfor*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void lapm_ack_rx(lapm_state_t *s, int ack)
{
    int i;
    int cnt;

    /* This might not be acking anything new */
    if (s->last_frame_peer_acknowledged == ack)
        return;
    /*endif*/
    /* It should be acking something that is actually outstanding */
    if ((s->last_frame_peer_acknowledged < s->next_tx_frame  &&  (ack < s->last_frame_peer_acknowledged  ||  ack > s->next_tx_frame))
        ||
        (s->last_frame_peer_acknowledged > s->next_tx_frame  &&  (ack > s->last_frame_peer_acknowledged  ||  ack < s->next_tx_frame)))
    {
        /* ACK was outside our window --- ignore */
        span_log(&s->logging, SPAN_LOG_FLOW, "ACK received outside window, ignoring\n");
        return;
    }
    /*endif*/
    
    /* Cancel each packet, as necessary */
    if ((s->debug & LAPM_DEBUG_LAPM_STATE))
    {
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "-- ACKing all packets from %d to (but not including) %d\n",
                 s->last_frame_peer_acknowledged,
                 ack);
    }
    /*endif*/
    for (cnt = 0, i = s->last_frame_peer_acknowledged;  i != ack;  i = (i + 1) & 0x7F) 
        cnt += lapm_ack_packet(s, i);
    /*endfor*/
    s->last_frame_peer_acknowledged = ack;
    if (s->txqueue == NULL)
    {
        if ((s->debug & LAPM_DEBUG_LAPM_STATE))
            span_log(&s->logging, SPAN_LOG_FLOW, "-- Since there was nothing left, stopping timer T_401\n");
        /*endif*/
        /* Something was ACK'd.  Stop timer T_401. */
        sp_schedule_del(&s->sched, s->t401_timer);
        s->t401_timer = -1;
    }
    /*endif*/
    if (s->t403_timer >= 0)
    {
        if ((s->debug & LAPM_DEBUG_LAPM_STATE))
            span_log(&s->logging, SPAN_LOG_FLOW, "-- Stopping timer T_403, since we got an ACK\n");
        /*endif*/
        sp_schedule_del(&s->sched, s->t403_timer);
        s->t403_timer = -1;
    }
    /*endif*/
    if (s->txqueue)
    {
        /* Something left to transmit. Start timer T_401 again if it is stopped */
        if ((s->debug & LAPM_DEBUG_LAPM_STATE))
        {
            span_log(&s->logging,
                     SPAN_LOG_FLOW,
                     "-- Something left to transmit (%d). Restarting timer T_401\n",
                     s->txqueue->frame[1] >> 1);
        }
        /*endif*/
        if (s->t401_timer < 0)
            s->t401_timer = sp_schedule_event(&s->sched, T_401, t401_expired, s);
        /*endif*/
    }
    else
    {
        if ((s->debug & LAPM_DEBUG_LAPM_STATE))
            span_log(&s->logging, SPAN_LOG_FLOW, "-- Nothing left, starting timer T_403\n");
        /*endif*/
        /* Nothing to transmit. Start timer T_403. */
        s->t403_timer = sp_schedule_event(&s->sched, T_403, t403_expired, s);
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void lapm_reject(lapm_state_t *s)
{
    uint8_t frame[4];

    lapm_init_header(frame, !s->we_are_originator);
    frame[1] = 0x00 | 0x08 | LAPM_FRAMETYPE_S;
    frame[2] = (s->next_expected_frame << 1) | 0x01;    /* Where to start retransmission */
    if ((s->debug & LAPM_DEBUG_LAPM_STATE))
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending REJ (reject (%d)\n", s->next_expected_frame);
    /*endif*/
    lapm_tx_frame(s, frame, 4);
}
/*- End of function --------------------------------------------------------*/

static void lapm_rr(lapm_state_t *s, int pfbit)
{
    uint8_t frame[4];

    lapm_init_header(frame, !s->we_are_originator);
    frame[1] = 0x00 | 0x00 | LAPM_FRAMETYPE_S;
    frame[2] = (s->next_expected_frame << 1) | pfbit;
    /* Note that we have already ACKed this */
    s->last_frame_we_acknowledged = s->next_expected_frame;
    if ((s->debug & LAPM_DEBUG_LAPM_STATE))
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending RR (receiver ready) (%d)\n", s->next_expected_frame);
    /*endif*/
    lapm_tx_frame(s, frame, 4);
}
/*- End of function --------------------------------------------------------*/

static void t401_expired(sp_sched_state_t *ss, void *user_data)
{
    lapm_state_t *s;
    
    s = (lapm_state_t *) user_data;
    if (s->txqueue)
    {
        /* Retransmit first packet in the queue, setting the poll bit */
        if ((s->debug & LAPM_DEBUG_LAPM_STATE))
            span_log(&s->logging, SPAN_LOG_FLOW, "-- Timer T_401 expired, What to do...\n");
        /*endif*/
        /* Update N(R), and set the poll bit */
        s->txqueue->frame[2] = (s->next_expected_frame << 1) | 0x01;
        s->last_frame_we_acknowledged = s->next_expected_frame;
        s->solicit_f_bit = TRUE;
        if (++s->retransmissions <= N_400)
        {
            /* Reschedule timer T401 */
            if ((s->debug & LAPM_DEBUG_LAPM_STATE))
                span_log(&s->logging, SPAN_LOG_FLOW, "-- Retransmitting %d bytes\n", s->txqueue->len);
            /*endif*/
            lapm_tx_frame(s, s->txqueue->frame, s->txqueue->len);
            if ((s->debug & LAPM_DEBUG_LAPM_STATE))
                span_log(&s->logging, SPAN_LOG_FLOW, "-- Scheduling retransmission (%d)\n", s->retransmissions);
            /*endif*/
            s->t401_timer = sp_schedule_event(&s->sched, T_401, t401_expired, s);
        }
        else
        {
            if ((s->debug & LAPM_DEBUG_LAPM_STATE))
                span_log(&s->logging, SPAN_LOG_FLOW, "-- Timeout occured\n");
            /*endif*/
            s->state = LAPM_RELEASE;
            if (s->status_callback)
                s->status_callback(s->status_callback_user_data, s->state);
            s->t401_timer = -1;
            lapm_link_down(s);
            lapm_restart(s);
        }
        /*endif*/
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Timer T_401 expired. Nothing to send...\n");
        s->t401_timer = -1;
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

int lapm_tx(lapm_state_t *s, const void *buf, int len)
{
    return queue_write(&s->tx_queue, buf, len);
}
/*- End of function --------------------------------------------------------*/

int lapm_tx_iframe(lapm_state_t *s, const void *buf, int len, int cr)
{
    lapm_frame_queue_t *f;

    if ((f = malloc(sizeof(lapm_frame_queue_t) + len + 4)) == NULL)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Out of memory\n");
        return -1;
    }
    /*endif*/

    if (s->peer_is_originator)
        lapm_init_header(f->frame, cr);
    else
        lapm_init_header(f->frame, !cr);
    /*endif*/
    f->next = NULL;
    f->len = len + 4;
    f->frame[1] = s->next_tx_frame << 1;
    f->frame[2] = s->next_expected_frame << 1;
    memcpy(f->frame + 3, buf, len);
    s->next_tx_frame = (s->next_tx_frame + 1) & 0x7F;
    s->last_frame_we_acknowledged = s->next_expected_frame;
    /* Clear poll bit */
    f->frame[2] &= ~0x01;
    if (s->tx_last)
        s->tx_last->next = f;
    else
        s->txqueue = f;
    /*endif*/
    s->tx_last = f;
    /* Immediately transmit unless we're in a recovery state */
    if (s->retransmissions == 0)
        lapm_tx_frame(s, f->frame, f->len);
    /*endif*/
    if (s->t403_timer >= 0)
    {
        if ((s->debug & LAPM_DEBUG_LAPM_STATE))
            span_log(&s->logging, SPAN_LOG_FLOW, "Stopping T_403 timer\n");
        /*endif*/
        sp_schedule_del(&s->sched, s->t403_timer);
        s->t403_timer = -1;
    }
    /*endif*/
    if (s->t401_timer >= 0)
    {
        if ((s->debug & LAPM_DEBUG_LAPM_STATE))
            span_log(&s->logging, SPAN_LOG_FLOW, "Timer T_401 already running (%d)\n", s->t401_timer);
        /*endif*/
    }
    else
    {
        if ((s->debug & LAPM_DEBUG_LAPM_STATE))
            span_log(&s->logging, SPAN_LOG_FLOW, "Starting timer T_401\n");
        /*endif*/
        s->t401_timer = sp_schedule_event(&s->sched, T_401, t401_expired, s);
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void t403_expired(sp_sched_state_t *ss, void *user_data)
{
    lapm_state_t *s;

    s = (lapm_state_t *) user_data;
    if ((s->debug & LAPM_DEBUG_LAPM_STATE))
        span_log(&s->logging, SPAN_LOG_FLOW, "Timer T_403 expired. Sending RR and scheduling T_403 again\n");
    /*endif*/
    /* Solicit an F-bit in the other end's RR */
    s->solicit_f_bit = TRUE;
    lapm_rr(s, 1);
    /* Restart ourselves */
    span_log(&s->logging, SPAN_LOG_FLOW, "T403 expired at %lld\n", sp_schedule_time(&s->sched));
    s->t403_timer = sp_schedule_event(&s->sched, T_403, t403_expired, s);
}
/*- End of function --------------------------------------------------------*/

void lapm_dump(lapm_state_t *s, const uint8_t *frame, int len, int showraw, int txrx)
{
    int x;
    char *type;
    char direction_tag;
    
    direction_tag = txrx  ?  '>'  :  '<';
    if (showraw)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "\n%c [", direction_tag);
        for (x = 0;  x < len;  x++) 
            span_log(&s->logging, SPAN_LOG_FLOW, "%02x ", frame[x]);
        /*endfor*/
        span_log(&s->logging, SPAN_LOG_FLOW, "]");
    }
    /*endif*/

    switch ((frame[1] & LAPM_FRAMETYPE_MASK))
    {
    case LAPM_FRAMETYPE_I:
    case LAPM_FRAMETYPE_I_ALT:
        span_log(&s->logging, SPAN_LOG_FLOW, "\n%c Information frame:\n", direction_tag);
        break;
    case LAPM_FRAMETYPE_S:
        span_log(&s->logging, SPAN_LOG_FLOW, "\n%c Supervisory frame:\n", direction_tag);
        break;
    case LAPM_FRAMETYPE_U:
        span_log(&s->logging, SPAN_LOG_FLOW, "\n%c Unnumbered frame:\n", direction_tag);
        break;
    }
    /*endswitch*/
    
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "%c DLCI: %2d  C/R: %d  EA: %d\n",
             direction_tag,
             (frame[0] >> 2),
             (frame[0] & 0x02)  ?  1  :  0,
             (frame[0] & 0x01),
             direction_tag);
    switch ((frame[1] & LAPM_FRAMETYPE_MASK))
    {
    case LAPM_FRAMETYPE_I:
    case LAPM_FRAMETYPE_I_ALT:
        /* Information frame */
        span_log(&s->logging,
                 SPAN_LOG_FLOW, 
                 "%c N(S): %03d\n"
                 "%c N(R): %03d   P: %d\n"
                 "%c %d bytes of data\n",
                 direction_tag,
                 (frame[1] >> 1),
                 direction_tag,
                 (frame[2] >> 1),
                 (frame[2] & 0x01),
                 direction_tag,
                 len - 4);
        break;
    case LAPM_FRAMETYPE_S:
        /* Supervisory frame */
        switch (frame[1] & 0xEC)
        {
        case 0x00:
            type = "RR (receive ready)";
            break;
        case 0x04:
            type = "RNR (receive not ready)";
            break;
        case 0x08:
            type = "REJ (reject)";
            break;
        case 0x0C:
            type = "SREJ (selective reject)";
            break;
        default:
            type = "???";
            break;
        }
        /*endswitch*/
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "%c S: %03d [ %s ]\n"
                 "%c N(R): %03d P/F: %d\n"
                 "%c %d bytes of data\n",
                 direction_tag,
                 frame[1],
                 type,
                 direction_tag,
                 frame[2] >> 1,
                 frame[2] & 0x01, 
                 direction_tag,
                 len - 4);
        break;
    case LAPM_FRAMETYPE_U:
        /* Unnumbered frame */
        switch (frame[1] & 0xEC)
        {
        case 0x00:
            type = "UI (unnumbered information)";
            break;
        case 0x0C:
            type = "DM (disconnect mode)";
            break;
        case 0x40:
            type = "DISC (disconnect)";
            break;
        case 0x60:
            type = "UA (unnumbered acknowledgement)";
            break;
        case 0x6C:
            type = "SABME (set asynchronous balanced mode extended)";
            break;
        case 0x84:
            type = "FRMR (frame reject)";
            break;
        case 0xAC:
            type = "XID (exchange identification)";
            break;
        case 0xE0:
            type = "TEST (test)";
            break;
        default:
            type = "???";
            break;
        }
        /*endswitch*/
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "%c   M: %03d [ %s ]\n"
                 "%c %d bytes of data\n",
                 direction_tag,
                 frame[1],
                 type,
                 direction_tag,
                 len - 3);
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

static void lapm_link_up(lapm_state_t *s)
{
    uint8_t buf[1024];
    int len;

    lapm_reset(s);
    /* Go into connection established state */
    s->state = LAPM_DATA;
    if (s->status_callback)
        s->status_callback(s->status_callback_user_data, s->state);
    if (!queue_empty(&(s->tx_queue)))
    {
        if ((len = queue_read(&(s->tx_queue), buf, s->n401)) > 0)
            lapm_tx_iframe(s, buf, len, TRUE);
    }
    /* Start the T403 timer */
    s->t403_timer = sp_schedule_event(&s->sched, T_403, t403_expired, s);
}
/*- End of function --------------------------------------------------------*/

static void lapm_link_down(lapm_state_t *s)
{
    lapm_reset(s);

    if (s->status_callback)
        s->status_callback(s->status_callback_user_data, s->state);
}
/*- End of function --------------------------------------------------------*/

void lapm_reset(lapm_state_t *s)
{
    lapm_frame_queue_t *f;
    lapm_frame_queue_t *p;

    /* Having received a SABME, we need to reset our entire state */
    s->next_tx_frame = 0;
    s->last_frame_peer_acknowledged = 0;
    s->next_expected_frame = 0;
    s->last_frame_we_acknowledged = 0;
    s->window_size_k = 15;
    s->n401 = 128;
    if (s->t401_timer >= 0)
    {
        sp_schedule_del(&s->sched, s->t401_timer);
        s->t401_timer = -1;
    }
    /*endif*/
    if (s->t403_timer >= 0)
    {
        sp_schedule_del(&s->sched, s->t403_timer);
        s->t403_timer = -1;
    }
    /*endif*/
    s->busy = FALSE;
    s->solicit_f_bit = FALSE;
    s->state = LAPM_RELEASE;
    s->retransmissions = 0;
    /* Discard anything waiting to go out */
    for (f = s->txqueue;  f;  )
    {
        p = f;
        f = f->next;
        free(p);
    }
    /*endfor*/
    s->txqueue = NULL;
}
/*- End of function --------------------------------------------------------*/

void lapm_receive(void *user_data, int ok, const uint8_t *frame, int len)
{
    lapm_state_t *s;
    lapm_frame_queue_t *f;
    int sendnow;
    int octet;
    int s_field;
    int m_field;

    if (!ok  ||  len == 0)
        return;
    /*endif*/
    if (len < 0)
    {
        /* Special conditions */
        switch (len)
        {
        case PUTBIT_TRAINING_FAILED:
            span_log(&s->logging, SPAN_LOG_DEBUG, "Training failed\n");
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            span_log(&s->logging, SPAN_LOG_DEBUG, "Training succeeded\n");
            break;
        case PUTBIT_CARRIER_UP:
            span_log(&s->logging, SPAN_LOG_DEBUG, "Carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            span_log(&s->logging, SPAN_LOG_DEBUG, "Carrier down\n");
            break;
        case PUTBIT_FRAMING_OK:
            span_log(&s->logging, SPAN_LOG_DEBUG, "Framing OK\n");
            break;
        default:
            span_log(&s->logging, SPAN_LOG_DEBUG, "Eh!\n");
            break;
        }
        return;
    }
    s = (lapm_state_t *) user_data;

    if ((s->debug & LAPM_DEBUG_LAPM_DUMP))
        lapm_dump(s, frame, len, s->debug & LAPM_DEBUG_LAPM_RAW, FALSE);
    /*endif*/
    octet = 0;
    /* We do not expect extended addresses */
    if ((frame[octet] & 0x01) == 0)
        return;
    /*endif*/
    /* Check for DLCIs we do not recognise */
    if ((frame[octet] >> 2) != LAPM_DLCI_DTE_TO_DTE)
        return;
    /*endif*/
    octet++;
    switch (frame[octet] & LAPM_FRAMETYPE_MASK)
    {
    case LAPM_FRAMETYPE_I:
    case LAPM_FRAMETYPE_I_ALT:
        if (s->state != LAPM_DATA)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "!! Got an I-frame while link state is %d\n", s->state);
            break;
        }
        /*endif*/
        /* Information frame */
        if (len < 4)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "!! Received short I-frame (expected 4, got %d)\n", len);
            break;
        }
        /*endif*/
        /* Make sure this is a valid packet */
        if ((frame[1] >> 1) == s->next_expected_frame)
        {
            /* Increment next expected I-frame */
            s->next_expected_frame = (s->next_expected_frame + 1) & 0x7F;
            /* Handle their ACK */
            lapm_ack_rx(s, frame[2] >> 1);
            if ((frame[2] & 0x01))
            {
                /* If the Poll/Final bit is set, send the RR immediately */
                lapm_rr(s, 1);
            }
            /*endif*/
            s->iframe_receive(s->iframe_receive_user_data, frame + 3, len - 4);
            /* Send an RR if one wasn't sent already */
            if (s->last_frame_we_acknowledged != s->next_expected_frame) 
                lapm_rr(s, 0);
            /*endif*/
        }
        else
        {
            if (((s->next_expected_frame - (frame[1] >> 1)) & 127) < s->window_size_k)
            {
                /* It's within our window -- send back an RR */
                lapm_rr(s, 0);
            }
            else
            {
                lapm_reject(s);
            }
            /*endif*/
        }
        /*endif*/
        break;
    case LAPM_FRAMETYPE_S:
        if (s->state != LAPM_DATA)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "!! Got S-frame while link down\n");
            break;
        }
        /*endif*/
        if (len < 4)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "!! Received short S-frame (expected 4, got %d)\n", len);
            break;
        }
        /*endif*/
        s_field = frame[octet] & 0xEC;
        switch (s_field)
        {
        case 0x00:
            /* RR (receive ready) */
            s->busy = FALSE;
            /* Acknowledge frames as necessary */
            lapm_ack_rx(s, frame[2] >> 1);
            if ((frame[2] & 0x01))
            {
                /* If P/F is one, respond with an RR with the P/F bit set */
                if (s->solicit_f_bit)
                {
                    if ((s->debug & LAPM_DEBUG_LAPM_STATE))
                        span_log(&s->logging, SPAN_LOG_FLOW, "-- Got RR response to our frame\n");
                    /*endif*/
                }
                else
                {
                    if ((s->debug & LAPM_DEBUG_LAPM_STATE))
                        span_log(&s->logging, SPAN_LOG_FLOW, "-- Unsolicited RR with P/F bit, responding\n");
                    /*endif*/
                    lapm_rr(s, 1);
                }
                /*endif*/
                s->solicit_f_bit = FALSE;
            }
            /*endif*/
            break;
        case 0x04:
            /* RNR (receive not ready) */
            if ((s->debug & LAPM_DEBUG_LAPM_STATE))
                span_log(&s->logging, SPAN_LOG_FLOW, "-- Got receiver not ready\n");
            /*endif*/
            s->busy = TRUE;
            break;   
        case 0x08:
            /* REJ (reject) */
            /* Just retransmit */
            if ((s->debug & LAPM_DEBUG_LAPM_STATE))
                span_log(&s->logging, SPAN_LOG_FLOW, "-- Got reject requesting packet %d...  Retransmitting.\n", frame[2] >> 1);
            /*endif*/
            if ((frame[2] & 0x01))
            {
                /* If it has the poll bit set, send an appropriate supervisory response */
                lapm_rr(s, 1);
            }
            /*endif*/
            sendnow = FALSE;
            /* Resend the appropriate I-frame */
            for (f = s->txqueue;  f;  f = f->next)
            {
                if (sendnow  ||  (f->frame[1] >> 1) == (frame[2] >> 1))
                {
                    /* Matches the request, or follows in our window */
                    sendnow = TRUE;
                    span_log(&s->logging,
                             SPAN_LOG_FLOW,
                             "!! Got reject for frame %d, retransmitting frame %d now, updating n_r!\n",
                             frame[2] >> 1,
                             f->frame[1] >> 1);
                    f->frame[2] = s->next_expected_frame << 1;
                    lapm_tx_frame(s, f->frame, f->len);
                }
                /*endif*/
            }
            /*endfor*/
            if (!sendnow)
            {
                if (s->txqueue)
                {
                    /* This should never happen */
                    if ((frame[2] & 0x01) == 0  ||  (frame[2] >> 1))
                    {
                        span_log(&s->logging,
                                 SPAN_LOG_FLOW,
                                 "!! Got reject for frame %d, but we only have others!\n",
                                 frame[2] >> 1);
                    }
                    /*endif*/
                }
                else
                {
                    /* Hrm, we have nothing to send, but have been REJ'd.  Reset last_frame_peer_acknowledged, next_tx_frame, etc */
                    span_log(&s->logging, SPAN_LOG_FLOW, "!! Got reject for frame %d, but we have nothing -- resetting!\n", frame[2] >> 1);
                    s->last_frame_peer_acknowledged =
                    s->next_tx_frame = frame[2] >> 1;
                    /* Reset t401 timer if it was somehow going */
                    if (s->t401_timer >= 0)
                    {
                        sp_schedule_del(&s->sched, s->t401_timer);
                        s->t401_timer = -1;
                    }
                    /*endif*/
                    /* Reset and restart t403 timer */
                    if (s->t403_timer >= 0)
                        sp_schedule_del(&s->sched, s->t403_timer);
                    /*endif*/
                    s->t403_timer = sp_schedule_event(&s->sched, T_403, t403_expired, s);
                }
                /*endif*/
            }
            /*endif*/
            break;
        case 0x0C:
            /* SREJ (selective reject) */
            break;
        default:
            span_log(&s->logging,
                     SPAN_LOG_FLOW,
                     "!! XXX Unknown Supervisory frame sd=0x%02x,pf=%02xnr=%02x vs=%02x, va=%02x XXX\n",
                     s_field,
                     frame[2] & 0x01,
                     frame[2] >> 1,
                     s->next_tx_frame,
                     s->last_frame_peer_acknowledged);
            break;
        }
        /*endswitch*/
        break;
    case LAPM_FRAMETYPE_U:
        if (len < 3)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "!! Received short unnumbered frame\n");
            break;
        }
        /*endif*/
        m_field = frame[octet] & 0xEC;
        switch (m_field)
        {
        case 0x00:
            /* UI (unnumbered information) */
            span_log(&s->logging, SPAN_LOG_FLOW, "UI (unnumbered information) not implemented.\n");
            break;
        case 0x0C:
            /* DM (disconnect mode) */
            if ((frame[octet] & 0x10))
            {
                if ((s->debug & LAPM_DEBUG_LAPM_STATE))
                    span_log(&s->logging, SPAN_LOG_FLOW, "-- Got Unconnected Mode from peer.\n");
                /*endif*/
                /* Disconnected mode, try again */
                lapm_link_down(s);
                lapm_restart(s);
            }
            else
            {
                if ((s->debug & LAPM_DEBUG_LAPM_STATE))
                    span_log(&s->logging, SPAN_LOG_FLOW, "-- DM (disconnect mode) requesting SABME, starting.\n");
                /*endif*/
                /* Requesting that we start */
                lapm_restart(s);
            }
            /*endif*/
            break;
        case 0x40:
            /* DISC (disconnect) */
            if ((s->debug & LAPM_DEBUG_LAPM_STATE))
                span_log(&s->logging, SPAN_LOG_FLOW, "-- Got DISC (disconnect) from peer.\n");
            /*endif*/
            /* Acknowledge */
            lapm_send_ua(s, (frame[octet] & 0x10));
            lapm_link_down(s);
            break;
        case 0x60:
            /* UA (unnumbered acknowledgement) */
            if (s->state == LAPM_ESTABLISH)
            {
                if ((s->debug & LAPM_DEBUG_LAPM_STATE))
                    span_log(&s->logging, SPAN_LOG_FLOW, "-- Got UA (unnumbered acknowledgement) from %s peer. Link up.\n", (frame[0] & 0x02)  ?  "cpe"  :  "network");
                /*endif*/
                lapm_link_up(s);
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "!! Got a UA (unnumbered acknowledgement) in state %d\n", s->state);
            }
            /*endif*/
            break;
        case 0x6C:
            /* SABME (set asynchronous balanced mode extended) */
            if ((s->debug & LAPM_DEBUG_LAPM_STATE))
                span_log(&s->logging, SPAN_LOG_FLOW, "-- Got SABME (set asynchronous balanced mode extended) from %s peer.\n", (frame[0] & 0x02)  ?  "network"  :  "cpe");
            /*endif*/
            if ((frame[0] & 0x02))
            {
                s->peer_is_originator = TRUE;
                if (s->we_are_originator)
                {
                    /* We can't both be originators */
                    span_log(&s->logging, SPAN_LOG_FLOW, "We think we are the originator, but they think so too.");
                    break;
                }
                /*endif*/
            }
            else
            {
                s->peer_is_originator = FALSE;
                if (!s->we_are_originator)
                {
                    /* We can't both be answerers */
                    span_log(&s->logging, SPAN_LOG_FLOW, "We think we are the answerer, but they think so too.\n");
                    break;
                }
                /*endif*/
            }
            /*endif*/
            /* Send unnumbered acknowledgement */
            lapm_send_ua(s, (frame[octet] & 0x10));
            lapm_link_up(s);
            break;
        case 0x84:
            /* FRMR (frame reject) */
            span_log(&s->logging, SPAN_LOG_FLOW, "!! FRMR (frame reject).\n");
            break;
        case 0xAC:
            /* XID (exchange identification) */
            span_log(&s->logging, SPAN_LOG_FLOW, "!! XID (exchange identification) frames not supported\n");
            break;
        case 0xE0:
            /* TEST (test) */
            span_log(&s->logging, SPAN_LOG_FLOW, "!! TEST (test) frames not supported\n");
            break;
        default:
            span_log(&s->logging, SPAN_LOG_FLOW, "!! Don't know what to do with M=%X u-frames\n", m_field);
            break;
        }
        /*endswitch*/
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

void lapm_hdlc_underflow(void *user_data)
{
    lapm_state_t *s;
    uint8_t buf[1024];
    int len;

    s = (lapm_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "HDLC underflow\n");
    if (s->state == LAPM_DATA)
    {
        if (!queue_empty(&(s->tx_queue)))
        {
            if ((len = queue_read(&(s->tx_queue), buf, s->n401)) > 0)
                lapm_tx_iframe(s, buf, len, TRUE);
        }
    }
}
/*- End of function --------------------------------------------------------*/

void lapm_restart(lapm_state_t *s)
{
#if 0
    if (s->state != LAPM_RELEASE)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "!! lapm_restart: Not in 'Link Connection Released' state\n");
        return;
    }
    /*endif*/
#endif
    hdlc_tx_init(&s->hdlc_tx, FALSE, lapm_hdlc_underflow, s);
    hdlc_rx_init(&s->hdlc_rx, FALSE, FALSE, 1, lapm_receive, s);
    /* TODO: This is a bodge! */
    s->t401_timer = -1;
    s->t402_timer = -1;
    s->t403_timer = -1;
    lapm_reset(s);
    /* TODO: Maybe we should implement T_WAIT? */
    lapm_send_sabme(NULL, s);
}
/*- End of function --------------------------------------------------------*/

void lapm_init(lapm_state_t *s)
{
    lapm_restart(s);
}
/*- End of function --------------------------------------------------------*/

static void negotiation_rx_bit(v42_state_t *s, int new_bit)
{
    /* DC1 with even parity, 8-16 ones, DC1 with odd parity, 8-16 ones */
    //uint8_t odp = "0100010001 11111111 0100010011 11111111";
    /* V.42 OK E , 8-16 ones, C, 8-16 ones */
    //uint8_t adp_v42 = "0101000101  11111111  0110000101  11111111";
    /* V.42 disabled E, 8-16 ones, NULL, 8-16 ones */
    //uint8_t adp_nov42 = "0101000101  11111111  0000000001  11111111";

    uint32_t stream;

    /* There may be no negotiation, so we need to process this data through the
       HDLC receiver as well */
    if (new_bit < 0)
    {
        /* Special conditions */
        switch (new_bit)
        {
        case PUTBIT_CARRIER_UP:
            break;
        case PUTBIT_CARRIER_DOWN:
        case PUTBIT_TRAINING_SUCCEEDED:
        case PUTBIT_TRAINING_FAILED:
            break;
        default:
            //printf("Eh!\n");
            break;
        }
        return;
    }
    new_bit &= 1;
    s->rxstream = (s->rxstream << 1) | new_bit;
    switch (s->rx_negotiation_step)
    {
    case 0:
        /* Look for some ones */
        if (new_bit)
            break;
        s->rx_negotiation_step = 1;
        s->rxbits = 0;
        s->rxstream = ~1;
        s->rxoks = 0;
        break;
    case 1:
        /* Look for the first character */
        if (++s->rxbits < 9)
            break;
        s->rxstream &= 0x3FF;
        if (s->caller  &&  s->rxstream == 0x145)
        {
            s->rx_negotiation_step++;
        }
        else if (!s->caller  &&  s->rxstream == 0x111)
        {
            s->rx_negotiation_step++;
        }
        else
        {
            s->rx_negotiation_step = 0;
        }
        s->rxbits = 0;
        s->rxstream = ~0;
        break;
    case 2:
        /* Look for 8 to 16 ones */
        s->rxbits++;
        if (new_bit)
            break;
        if (s->rxbits >= 8  &&  s->rxbits <= 16)
        {
            s->rx_negotiation_step++;
        }
        else
        {
            s->rx_negotiation_step = 0;
        }
        s->rxbits = 0;
        s->rxstream = ~1;
        break;
    case 3:
        /* Look for the second character */
        if (++s->rxbits < 9)
            break;
        s->rxstream &= 0x3FF;
        if (s->caller  &&  s->rxstream == 0x185)
        {
            s->rx_negotiation_step++;
        }
        else if (s->caller  &&  s->rxstream == 0x001)
        {
            s->rx_negotiation_step++;
        }
        else if (!s->caller  &&  s->rxstream == 0x113)
        {
            s->rx_negotiation_step++;
        }
        else
        {
            s->rx_negotiation_step = 0;
        }
        s->rxbits = 0;
        s->rxstream = ~0;
        break;
    case 4:
        /* Look for 8 to 16 ones */
        s->rxbits++;
        if (new_bit)
            break;
        if (s->rxbits >= 8  &&  s->rxbits <= 16)
        {
            if (++s->rxoks >= 2)
            {
                /* HIT */
                s->rx_negotiation_step++;
                if (s->caller)
                {
                    s->lapm.state = LAPM_ESTABLISH;
                    if (s->t400_timer >= 0)
                    {
                        sp_schedule_del(&s->lapm.sched, s->t400_timer);
                        s->t400_timer = -1;
                    }
                    if (s->lapm.status_callback)
                        s->lapm.status_callback(s->lapm.status_callback_user_data, s->lapm.state);
                }
                else
                {
                    s->odp_seen = TRUE;
                }
                break;
            }
            s->rx_negotiation_step = 1;
            s->rxbits = 0;
            s->rxstream = ~1;
        }
        else
        {
            s->rx_negotiation_step = 0;
            s->rxbits = 0;
            s->rxstream = ~0;
        }
        break;
    case 5:
        /* Parked */
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static int negotiation_tx_bit(v42_state_t *s)
{
    int bit;

    if (s->caller)
    {
        if (s->txbits <= 0)
        {
            s->txstream = 0x3FE22;
            s->txbits = 36;
        }
        else if (s->txbits == 18)
        {
            s->txstream = 0x3FF22;
        }
        bit = s->txstream & 1;
        s->txstream >>= 1;
        s->txbits--;
    }
    else
    {
        if (s->odp_seen  &&  s->txadps < 10)
        {
            if (s->txbits <= 0)
            {
                if (++s->txadps >= 10)
                {
                    if (s->t400_timer >= 0)
                    {
                        sp_schedule_del(&s->lapm.sched, s->t400_timer);
                        s->t400_timer = -1;
                    }
                    s->lapm.state = LAPM_ESTABLISH;
                    if (s->lapm.status_callback)
                        s->lapm.status_callback(s->lapm.status_callback_user_data, s->lapm.state);
                    s->txstream = 1;
                }
                else
                {
                    s->txstream = 0x3FE8A;
                    s->txbits = 36;
                }
            }
            else if (s->txbits == 18)
            {
                s->txstream = 0x3FE86;
            }
            bit = s->txstream & 1;
            s->txstream >>= 1;
            s->txbits--;
        }
        else
        {
            bit = 1;
        }
    }
    return bit;
}
/*- End of function --------------------------------------------------------*/

void v42_rx_bit(void *user_data, int bit)
{
    v42_state_t *s;

    s = (v42_state_t *) user_data;
    if (s->lapm.state == LAPM_DETECT)
        negotiation_rx_bit(s, bit);
    else
        hdlc_rx_bit(&s->lapm.hdlc_rx, bit);
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

int v42_tx_bit(void *user_data)
{
    v42_state_t *s;
    int bit;

    s = (v42_state_t *) user_data;
    if (s->lapm.state == LAPM_DETECT)
        bit = negotiation_tx_bit(s);
    else
        bit = hdlc_tx_getbit(&s->lapm.hdlc_tx);
    /*endif*/
    return bit;
}
/*- End of function --------------------------------------------------------*/

void v42_set_status_callback(v42_state_t *s, v42_status_func_t callback, void *user_data)
{
    s->lapm.status_callback = callback;
    s->lapm.status_callback_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

void v42_restart(v42_state_t *s)
{
    sp_schedule_init(&s->lapm.sched);

    s->lapm.we_are_originator = s->caller;
    lapm_restart(&s->lapm);
    if (s->detect)
    {
        s->txstream = ~0;
        s->txbits = 0;
        s->rxstream = ~0;
        s->rxbits = 0;
        s->rxoks = 0;
        s->txadps = 0;
        s->rx_negotiation_step = 0;
        s->odp_seen = FALSE;
        s->t400_timer = sp_schedule_event(&s->lapm.sched, T_400, t400_expired, s);
        s->lapm.state = LAPM_DETECT;
    }
    else
    {
        s->lapm.state = LAPM_ESTABLISH;
    }
}
/*- End of function --------------------------------------------------------*/

void v42_init(v42_state_t *s, int caller, int detect, v42_frame_handler_t frame_handler, void *user_data)
{
    memset(s, 0, sizeof(*s));
    s->caller = caller;
    s->detect = detect;
    s->lapm.iframe_receive = frame_handler;
    s->lapm.iframe_receive_user_data = user_data;
    s->lapm.debug |= (LAPM_DEBUG_LAPM_RAW | LAPM_DEBUG_LAPM_DUMP | LAPM_DEBUG_LAPM_STATE);
    if (queue_create(&(s->lapm.tx_queue), 16384, 0) < 0)
        return;
    v42_restart(s);
}
/*- End of function --------------------------------------------------------*/

int v42_release(v42_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

#define LOG_FAX_AUDIO
/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t30.c - ITU T.30 FAX transfer processing
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
 * $Id: t30.c,v 1.26 2004/03/25 15:23:17 steveu Exp $
 */

/*! \file */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include <tiffiop.h>

#include "spandsp/telephony.h"
#include "spandsp/power_meter.h"
#include "spandsp/complex.h"
#include "spandsp/complex_filters.h"
#include "spandsp/tone_generate.h"
#include "spandsp/fsk.h"
#include "spandsp/v27ter_rx.h"
#include "spandsp/v27ter_tx.h"
#include "spandsp/v29rx.h"
#include "spandsp/v29tx.h"
#include "spandsp/hdlc.h"
#include "spandsp/t4.h"
#include "spandsp/t35.h"
#include "spandsp/t30_fcf.h"
#include "spandsp/t30.h"

#define MAXMESSAGE          (MAXFRAME + 4)  /* HDLC frame, including address, control, and CRC */

/* T.30 defines the following call phases:
   Phase A: Call set-up.
       Exchange of CNG, CED and the called terminal identification.
   Phase B: Pre-message procedure for identifying and selecting the required facilities.
       Capabilities negotiation, and training, up the the confirmation to receive.
   Phase C: Message transmission (includes phasing and synchronization where appropriate).
       Transfer of the message at high speed.
   Phase D: Post-message procedure, including end-of-message and confirmation and multi-document procedures.
       End of message and acknowledgement.
   Phase E: Call release
       Final call disconnect. */
enum
{
    T30_PHASE_IDLE,         /* Freshly initialised */
    T30_PHASE_A_CED,        /* Doing the CED (answer) sequence */
    T30_PHASE_A_CNG,        /* Doing the CNG (caller) sequence */
    T30_PHASE_BDE_RX,       /* Receiving control messages */
    T30_PHASE_BDE_TX,       /* Transmitting a control message */
    T30_PHASE_C_RX,         /* Receiving a document message */
    T30_PHASE_C_TX,         /* Transmitting a document message */
    T30_PHASE_E_DONE        /* Call completely finished */
};

/* These state names are modelled after places in the T.30 flow charts. */
enum
{
    T30_STATE_B = 1,
    T30_STATE_C,
    T30_STATE_D,
    T30_STATE_D_TCF,
    T30_STATE_F,
    T30_STATE_F_TCF,
    T30_STATE_F_MPS_MCF,
    T30_STATE_F_EOP_MCF,
    T30_STATE_R,
    T30_STATE_T,
    T30_STATE_I,
    T30_STATE_II,
    T30_STATE_II_MPS,
    T30_STATE_II_EOP,
    T30_STATE_II_EOM
};

enum
{
    T30_MODE_SEND_DOC = 1,
    T30_MODE_RECEIVE_DOC
};

#define DISBIT1     0x01
#define DISBIT2     0x02
#define DISBIT3     0x04
#define DISBIT4     0x08
#define DISBIT5     0x10
#define DISBIT6     0x20
#define DISBIT7     0x40
#define DISBIT8     0x80

#define DEFAULT_TIMER_T0        60
#define DEFAULT_TIMER_T1        35
#define DEFAULT_TIMER_T2        6
#define DEFAULT_TIMER_T3        10
#define DEFAULT_TIMER_T4        3
#define DEFAULT_TIMER_T5        60
#define DEFAULT_TIMER_SIG_ON    3

/* The simple fixed format messages */
static const uint8_t mps_frame[] = {T30_MPS};
static const uint8_t eom_frame[] = {T30_EOM};
static const uint8_t eop_frame[] = {T30_EOP};

static const uint8_t cfr_frame[] = {T30_CFR};
static const uint8_t ftt_frame[] = {T30_FTT};

static const uint8_t mcf_frame[] = {T30_MCF};

static const uint8_t dcn_frame[] = {T30_DCN};

/* Exact widths in PELs for the difference resolutions, and page widths:
    R4    864 pels/215mm for ISO A4, North American Letter and Legal
    R4   1024 pels/255mm for ISO B4
    R4   1216 pels/303mm for ISO A3
    R8   1728 pels/215mm for ISO A4, North American Letter and Legal
    R8   2048 pels/255mm for ISO B4
    R8   2432 pels/303mm for ISO A3
    R16  3456 pels/215mm for ISO A4, North American Letter and Legal
    R16  4096 pels/255mm for ISO B4
    R16  4864 pels/303mm for ISO A3
*/

#if defined(LOG_FAX_AUDIO)
static int fax_audio_rx_log;
static int fax_audio_tx_log;
#endif

static void set_phase(t30_state_t *s, int phase);
static void send_frame(t30_state_t *s, const uint8_t *fr, int frlen, int final);
static void disconnect(t30_state_t *s);
void decode_password(t30_state_t *s, char *msg, const uint8_t *pkt, int len);
void decode_20digit_msg(t30_state_t *s, char *msg, const uint8_t *pkt, int len);
void decode_url_msg(t30_state_t *s, char *msg, const uint8_t *pkt, int len);

static void fast_putbit(void *user_data, int bit)
{
    t30_state_t *s;

    s = (t30_state_t *) user_data;
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_TRAINING_FAILED:
            fprintf(stderr, "Fast carrier training failed\n");
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            /* The modem is now trained */
            fprintf(stderr, "Fast carrier trained\n");
            /* In case we are in trainability test mode... */
            /* A FAX machine is supposed to send 1.5s of training test
               data, but some send a little bit less. Lets just check
               the first 1s, and be safe. */
            s->training_current_zeros = 0;
            s->training_most_zeros = 0;
            s->rx_signal_present = TRUE;
            break;
        case PUTBIT_CARRIER_UP:
            fprintf(stderr, "Fast carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            fprintf(stderr, "Fast carrier down\n");
            switch (s->state)
            {
            case T30_STATE_F_TCF:
                /* Although T.30 says the training test should be 1.5s of all 0's, some FAX
                   machines send a burst of all 1's before the all 0's. Tolerate this. */
                if (s->training_current_zeros > s->training_most_zeros)
                    s->training_most_zeros = s->training_current_zeros;
                if (s->training_most_zeros < s->bit_rate)
                {
                    fprintf(stderr, "Trainability test failed - longest run of zeros was %d\n", s->training_most_zeros);
                    send_frame(s, ftt_frame, 1, TRUE);
                }
                else
                {
                    s->state = T30_STATE_F;
                    set_phase(s, T30_PHASE_BDE_TX);
                    if (!s->in_message  &&  t4_rx_init(&(s->t4)))
                    {
                        fprintf(stderr, "Cannot open target TIFF file\n");
                        send_frame(s, dcn_frame, 1, TRUE);
                    }
                    else
                    {
                        s->in_message = TRUE;
                        t4_rx_set_sub_address(&(s->t4), s->sub_address);
                        t4_rx_set_far_ident(&(s->t4), s->far_ident);
                        t4_rx_set_vendor(&(s->t4), s->vendor);
                        t4_rx_set_model(&(s->t4), s->model);
                        t4_rx_start_page(&(s->t4));
                        send_frame(s, cfr_frame, 1, TRUE);
                    }
                }
                break;
            default:
                /* We should be receiving a document right now */
                if (s->rx_signal_present)
                {
                    t4_rx_end_page(&(s->t4));
                    /* We should have changed phase already, but if we have, this
                       should be harmless. */
                    set_phase(s, T30_PHASE_BDE_RX);
                }
                break;
            }
            s->rx_signal_present = FALSE;
            break;
        default:
            fprintf(stderr, "Eh!\n");
            break;
        }
        return;
    }
    if (s->phase == T30_PHASE_C_RX)
    {
        /* Only process the data while we are in phase C, otherwise we might
           process some straggly bits at the end of a page, after the end of
           page mark has been recognised. Although we will turn off the fast
           modem when the end of page mark is seen, we will still process the
           remainder of the current block of audio, which might contain quite
           a few bits. */
        if (s->state == T30_STATE_F_TCF)
        {
            /* Trainability test */
            if (bit)
            {
                if (s->training_current_zeros > s->training_most_zeros)
                    s->training_most_zeros = s->training_current_zeros;
                s->training_current_zeros = 0;
            }
            else
            {
                s->training_current_zeros++;
            }
        }
        else
        {
            /* Document transfer */
            if (t4_rx_putbit(&(s->t4), bit))
                set_phase(s, T30_PHASE_BDE_RX);
        }
    }
}
/*- End of function --------------------------------------------------------*/

static int fast_getbit(void *user_data)
{
    int bit;
    t30_state_t *s;

    s = (t30_state_t *) user_data;
    switch (s->state)
    {
    case T30_STATE_D:
        /* Trainability test */
        if (s->training_test_bits-- < 0)
        {
            /* Finished sending training test. Listen for messages */
            set_phase(s, T30_PHASE_BDE_RX);
            s->timer_t4 = DEFAULT_TIMER_T4*SAMPLE_RATE;
            s->state = T30_STATE_D_TCF;
        }
        bit = 0;
        break;
    case T30_STATE_I:
        /* Transferring real data */
        bit = t4_tx_getbit(&(s->t4));
        if ((bit & 2))
        {
            /* Send the end of page message */
            /* TODO: This usually works, but if it occurs right at the end of
               a block it might get cut short. We should inject a few more 0 bits
               for safety. */
            set_phase(s, T30_PHASE_BDE_TX);
            if (t4_tx_start_page(&(s->t4)) == 0)
            {
                send_frame(s, mps_frame, 1, TRUE);
                s->state = T30_STATE_II_MPS;
            }
            else
            {
                send_frame(s, eop_frame, 1, TRUE);
                s->state = T30_STATE_II_EOP;
            }
            bit &= 1;
        }
        break;
    case T30_STATE_D_TCF:
    case T30_STATE_II_MPS:
    case T30_STATE_II_EOM:
    case T30_STATE_II_EOP:
        /* We should be padding out a block of samples if we are here */
        bit = 0;
        break;
    default:
        fprintf(stderr, "fast_getbit in bad state %d\n", s->state);
        break;
    }
    return bit;
}
/*- End of function --------------------------------------------------------*/

static void hdlc_tx_underflow(void *user_data)
{
    t30_state_t *s;
    
    s = (t30_state_t *) user_data;
    fprintf(stderr, "HDLC underflow in state %d\n", s->state);
    /* We have finished sending our messages, so move on to the next operation. */
    switch (s->state)
    {
    case T30_STATE_F:
        /* Send trainability response */
        fprintf(stderr, "Post trainability\n");
        set_phase(s, T30_PHASE_C_RX);
        break;
    case T30_STATE_F_MPS_MCF:
        s->state = T30_STATE_F;
        set_phase(s, T30_PHASE_C_RX);
        break;
    case T30_STATE_F_EOP_MCF:
    case T30_STATE_R:
        /* Wait for acknowledgement */
        set_phase(s, T30_PHASE_BDE_RX);
        s->timer_t4 = DEFAULT_TIMER_T4*SAMPLE_RATE;
        break;
    case T30_STATE_C:
        /* We just sent the disconnect message. Now it is time to disconnect */
        disconnect(s);
        break;
    case T30_STATE_D:
        /* Do the trainability test */
        set_phase(s, T30_PHASE_C_TX);
        break;
    case T30_STATE_II_MPS:
    case T30_STATE_II_EOM:
    case T30_STATE_II_EOP:
        /* Wait for acknowledgement */
        set_phase(s, T30_PHASE_BDE_RX);
        s->timer_t4 = DEFAULT_TIMER_T4*SAMPLE_RATE;
        break;
    default:
        fprintf(stderr, "Bad state in hdlc_tx_underflow - %d\n", s->state);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void print_frame(t30_state_t *s, const char *io, const uint8_t *fr, int frlen)
{
    int i;
    
    fprintf(stderr, "%s %s:", io, t30_frametype(fr[0]));
    for (i = 0;  i < frlen;  i++)
        fprintf(stderr, " %02x", fr[i]);
    fprintf(stderr, "\n");
}
/*- End of function --------------------------------------------------------*/

static void send_frame(t30_state_t *s, const uint8_t *fr, int frlen, int final)
{
    uint8_t message[MAXMESSAGE];
    
    print_frame(s, ">>>", fr, frlen);
    /* HDLC address field - always 0xFF for PSTN use */
    message[0] = 0xFF;
    /* HDLC control field - either 0x13 for the final frame in a procedure, or 0x03 */
    message[1] = final  ?  0x13  :  0x03;
    /* Now comes the actual fax control message */
    memcpy(&message[2], fr, frlen);
    hdlc_tx_preamble(&(s->hdlctx), 2);
    hdlc_tx_packet(&(s->hdlctx), message, frlen + 2);
    hdlc_tx_preamble(&(s->hdlctx), 1);
}
/*- End of function --------------------------------------------------------*/

static void send_ident_frame(t30_state_t *s, uint8_t cmd, int lastframe)
{
    int len;
    int p;
    uint8_t frame[123];

    fprintf(stderr, "Sending ident\n");
    len = strlen(s->local_ident);
    p = 0;
    frame[p++] = cmd;  /* T30_TSI or T30_CSI */
    while (len > 0)
        frame[p++] = s->local_ident[--len];
    while (p < 21)
        frame[p++] = ' ';
    send_frame(s, frame, 21, lastframe);
}
/*- End of function --------------------------------------------------------*/

static int build_dtc(t30_state_t *s)
{
    /* Tell the far end our capabilities. */
    s->dtc_frame[0] = T30_DTC;
    s->dtc_frame[1] = 0;
    /* 2D compression OK; fine resolution OK; V.29 and V.27ter OK */
    s->dtc_frame[2] = DISBIT8 | DISBIT7 | DISBIT4 | DISBIT3;
    if (s->t4.rx_file[0])
        s->dtc_frame[2] |= DISBIT2;
    if (s->t4.tx_file[0])
        s->dtc_frame[2] |= DISBIT1;
    /* No scan-line padding required; 215mm wide only; A4 long only. */
    s->dtc_frame[3] = DISBIT7 | DISBIT6 | DISBIT5;
    s->dtc_frame[4] = DISBIT8;
    s->dtc_frame[5] = DISBIT8;
    /* Super fine resolution OK */
    s->dtc_frame[6] = DISBIT1;
    s->dtc_len = 7;
    t30_decode_dis_dtc_dcs(s, s->dtc_frame, s->dtc_len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int build_dis(t30_state_t *s)
{
    /* Tell the far end our capabilities. */
    s->dis_frame[0] = T30_DIS;
    s->dis_frame[1] = 0;
    /* 2D compression OK; fine resolution OK; V.29 and V.27ter OK */
    s->dis_frame[2] = DISBIT8 | DISBIT7 | DISBIT4 | DISBIT3;
    if (s->t4.rx_file[0])
        s->dis_frame[2] |= DISBIT2;
    if (s->t4.tx_file[0])
        s->dis_frame[2] |= DISBIT1;
    /* No scan-line padding required; 215mm wide; A4 long. */
    s->dis_frame[3] = DISBIT8 | DISBIT7 | DISBIT6 | DISBIT5;
    s->dis_frame[4] = DISBIT8;
    s->dis_frame[5] = DISBIT8;
    /* Super fine resolution OK */
    s->dis_frame[6] = DISBIT1;
    s->dis_len = 7;
    t30_decode_dis_dtc_dcs(s, s->dis_frame, s->dis_len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int build_dcs(t30_state_t *s, uint8_t *dis_frame)
{
    static const uint8_t translate_min_scan_time[2][8] =
    {
        {0, 1, 2, 0, 4, 4, 2, 7}, /* normal */
        {0, 1, 2, 2, 4, 0, 2, 7}  /* fine */
    };
    static const int scanbitstab[4][8] =
    {
        /* Translate the minimum scan time to a minimum number of transmitted bits at 7200 and 9600bps */
        {144,  36,  72, -1,  288, -1, -1, 0},
        {192,  48,  96, -1,  384, -1, -1, 0},
        {288,  72, 144, -1,  576, -1, -1, 0},
        {576, 144, 288, -1, 1152, -1, -1, 0}
    };
    uint8_t spd;
    uint8_t n;
    
    /* Make a DCS frame from a received DIS frame. Negotiate the result
       based on what both parties can do. */
    s->dcs_frame[0] = T30_DCS;
    s->dcs_frame[1] = 0x00;
    if (!(dis_frame[2] & (DISBIT3 | DISBIT4)))
    {
        fprintf(stderr, "Remote does not support V.29 or V.27ter\n");
        /* We cannot talk to this machine! */
        return -1;
    }

    /* Set to required modem rate; standard resolution */
    /* Set the minimum scan time, in bits at the chosen bit rate */
    s->dcs_frame[2] = 0;
    switch (s->bit_rate)
    {
    case 9600:
        s->dcs_frame[2] |= DISBIT3;
        s->scanbits = scanbitstab[0][n];
        break;
    case 7200:
        s->dcs_frame[2] |= (DISBIT4 | DISBIT3);
        s->scanbits = scanbitstab[1][n];
        break;
    case 4800:
        s->dcs_frame[2] |= DISBIT4;
        s->scanbits = scanbitstab[2][n];
        break;
    case 2400:
        /* Speed bits are all zero for this */
        s->scanbits = scanbitstab[3][n];
        break;
    }
    /* If remote supports 2D compression, use it. */
    if ((dis_frame[2] & DISBIT8))
    {
        s->t4.remote_compression = 2;
        s->dcs_frame[2] |= DISBIT8;
    }
    else
    {
        s->t4.remote_compression = 1;
    }
    if (s->t4.rx_file[0])
        s->dcs_frame[2] |= DISBIT2;
    /* Set A4 1728 dots per scan line; set scantime bits; clear extend bit */
    n = (dis_frame[3] >> 4) & 7;     /* DIS bits 21-23 */
    switch (s->t4.resolution)
    {
    case 2:
        if ((dis_frame[6] & DISBIT1))
        {
            s->dcs_frame[6] |= DISBIT1;
            n = translate_min_scan_time[1][n];
            s->dcs_frame[3] = (n + 8) << 4;
            break;
        }
        s->t4.resolution = 1;
        fprintf(stderr, "Remote fax does not support super-fine resolution.\n");
        /* Fall through */
        break;
    case 1:
        if ((dis_frame[2] & DISBIT7))
        {
            s->dcs_frame[2] |= DISBIT7;
            n = translate_min_scan_time[1][n];
            s->dcs_frame[3] = (n + 8) << 4;
            break;
        }
        s->t4.resolution = 0;
        fprintf(stderr, "Remote fax does not support fine resolution.\n");
        /* Fall through */
    case 0:
        n = translate_min_scan_time[0][n];
        s->dcs_frame[3] = n << 4;
        break;
    }
    s->dcs_len = 4;
    t30_decode_dis_dtc_dcs(s, s->dcs_frame, s->dcs_len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int check_dcs(t30_state_t *s, uint8_t *dcs_frame, int len)
{
    static const int widths[3][4] =
    {
        { 864, 1024, 1216, -1},
        {1728, 2048, 2432, -1},
        {3456, 4096, 4864, -1}
    };
    int speed;

    /* Check DCS frame from remote */
    t30_decode_dis_dtc_dcs(s, dcs_frame, len);
    if (len < 4)
        fprintf(stderr, "Short DCS frame\n");
    if (len >= 7  &&  (dcs_frame[6] & DISBIT1))
        s->t4.resolution = 2;
    else if (dcs_frame[2] & DISBIT7)
        s->t4.resolution = 1;
    else
        s->t4.resolution = 0;
    s->t4.image_width = widths[1][dcs_frame[3] & (DISBIT2 | DISBIT1)];
    s->t4.remote_compression = (dcs_frame[2] & DISBIT8)  ?  2  :  1;
    if (!(dcs_frame[2] & DISBIT2))
        fprintf(stderr, "Remote cannot receive\n");
    if ((dcs_frame[2] & (DISBIT5 | DISBIT6)))
    {
        fprintf(stderr, "Remote has not specified an acceptable modem rate\n");
        /* We cannot talk to this machine! */
        return -1;
    }

    speed = dcs_frame[2] & (DISBIT6 | DISBIT5 | DISBIT4 | DISBIT3);
    switch (speed)
    {
    case DISBIT3:
        s->bit_rate = 9600;
        break;
    case (DISBIT4 | DISBIT3):
        s->bit_rate = 7200;
        break;
    case DISBIT4:
        s->bit_rate = 4800;
        break;
    case 0:
        s->bit_rate = 2400;
        break;
    default:
        fprintf(stderr, "Remote asked for a modem standard we do not support\n");
        return -1;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void send_dcn(t30_state_t *s)
{
    /* There will be no response to this command */
    s->timer_t4 = 0;
    set_phase(s, T30_PHASE_BDE_TX);
    s->state = T30_STATE_C;
    send_frame(s, dcn_frame, 1, TRUE);
}
/*- End of function --------------------------------------------------------*/

static void disconnect(t30_state_t *s)
{
    fprintf(stderr, "Disconnecting\n");
    s->timer_t1 = 0;
    s->timer_t2 = 0;
    s->timer_t3 = 0;
    s->timer_t4 = 0;
    set_phase(s, T30_PHASE_E_DONE);
    s->state = T30_STATE_B;
}
/*- End of function --------------------------------------------------------*/

static int start_sending_document(t30_state_t *s)
{
    if (s->t4.tx_file[0] == '\0')
    {
        /* There is nothing to send */
        return  FALSE;
    }
    fprintf(stderr, "Start sending document\n");
    if (t4_tx_init(&(s->t4)))
        return  FALSE;
    switch (s->t4.resolution)
    {
    case 0:
        s->dcs_frame[2] &= ~DISBIT7;
        s->dtc_frame[6] &= ~DISBIT1;
        break;
    case 1:
        s->dcs_frame[2] |= DISBIT7;
        s->dtc_frame[6] &= ~DISBIT1;
        break;
    case 2:
        s->dcs_frame[2] &= ~DISBIT7;
        s->dtc_frame[6] |= DISBIT1;
        break;
    }
    set_phase(s, T30_PHASE_BDE_TX);
    send_ident_frame(s, T30_TSI, FALSE);
    send_frame(s, s->dcs_frame, s->dcs_len, TRUE);
    /* Schedule training after the messages */
    s->state = T30_STATE_D;
    return  TRUE;
}
/*- End of function --------------------------------------------------------*/

static int start_receiving_document(t30_state_t *s)
{
    if (s->t4.rx_file[0] == '\0')
    {
        /* There is nothing to receive to */
        return  FALSE;
    }
    fprintf(stderr, "Start receiving document\n");
    set_phase(s, T30_PHASE_BDE_TX);
    send_ident_frame(s, T30_CSI, FALSE);
    build_dis(s);
    send_frame(s, s->dis_frame, s->dis_len, TRUE);
    s->state = T30_STATE_R;
    s->timer_t2 = SAMPLE_RATE*DEFAULT_TIMER_T2;
    return  TRUE;
}
/*- End of function --------------------------------------------------------*/

static void process_rx_dis(t30_state_t *s, uint8_t *msg, int len)
{
    int incompatible;
    
    /* Digital identification signal */
    switch (s->state)
    {
    case T30_STATE_R:
        s->timer_t4 = 0;
        /* Fall through */
    case T30_STATE_T:
    case T30_STATE_F:
        t30_decode_dis_dtc_dcs(s, &msg[2], len - 2);
        if (s->phase_b_handler)
            s->phase_b_handler(s, s->phase_d_user_data, T30_DIS);
        /* Try to send something */
        if ((incompatible = build_dcs(s, &msg[2]))
            ||
            !start_sending_document(s))
        {
            printf("DIS nothing to send [%d]\n", incompatible);
            /* ... then try to receive something */
            if ((incompatible = build_dis(s))
                ||
                !start_receiving_document(s))
            {
                printf("DIS nothing to receive [%d]\n", incompatible);
                /* There is nothing to do, or nothing we are able to do. */
                send_dcn(s);
            }
        }
        break;
    case T30_STATE_D_TCF:
        /* It appears they didn't see what we sent - retry */
        /* TODO: retry */
        break;
    default:
        fprintf(stderr, "Unexpected DIS received in state %d\n", s->state);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_rx_dtc(t30_state_t *s, uint8_t *msg, int len)
{
    int incompatible;

    /* Digital transmit signal */
    switch (s->state)
    {
    case T30_STATE_R:
    case T30_STATE_T:
    case T30_STATE_F:
        t30_decode_dis_dtc_dcs(s, &msg[2], len - 2);
        if (s->phase_b_handler)
            s->phase_b_handler(s, s->phase_d_user_data, T30_DTC);
        /* Try to send something */
        if ((incompatible = build_dcs(s, &msg[2]))
            ||
            !start_sending_document(s))
        {
            printf("DTC nothing to send [%d]\n", incompatible);
            /* ... then try to receive something */
            if ((incompatible = build_dis(s))
                ||
                !start_receiving_document(s))
            {
                printf("DTC nothing to receive [%d]\n", incompatible);
                /* There is nothing to do, or nothing we are able to do. */
                send_dcn(s);
            }
        }
        break;
    case T30_STATE_D_TCF:
        /* It appears they didn't see what we sent - retry */
        break;
    default:
        fprintf(stderr, "Unexpected DTC received in state %d\n", s->state);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_rx_dcs(t30_state_t *s, uint8_t *msg, int len)
{
    /* Digital command signal */
    switch (s->state)
    {
    case T30_STATE_R:
    case T30_STATE_F:
        /* (TSI) DCS */
        /* (PWD) (SUB) (TSI) DCS */
        s->timer_t4 = 0;
        check_dcs(s, &msg[2], len - 2);
        if (s->phase_b_handler)
            s->phase_b_handler(s, s->phase_d_user_data, T30_DCS);
        fprintf(stderr, "Get at %d\n", s->bit_rate);
        s->state = T30_STATE_F_TCF;
        set_phase(s, T30_PHASE_C_RX);
        break;
    default:
        fprintf(stderr, "Unexpected DCS received in state %d\n", s->state);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_rx_cfr(t30_state_t *s, uint8_t *msg, int len)
{
    /* Confirmation to receive */
    switch (s->state)
    {
    case T30_STATE_D_TCF:
        /* Trainability test succeeded. Send the document. */
        fprintf(stderr, "Trainability test succeeded\n");
        s->timer_t4 = 0;
        /* Send the first page */
        t4_tx_start_page(&(s->t4));
        s->state = T30_STATE_I;
        set_phase(s, T30_PHASE_C_TX);
        break;
    default:
        fprintf(stderr, "Unexpected CFR received in state %d\n", s->state);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_rx_ftt(t30_state_t *s, uint8_t *msg, int len)
{
    /* Failure to train */
    switch (s->state)
    {
    case T30_STATE_D_TCF:
        /* Trainability test failed. Try again. */
        fprintf(stderr, "Trainability test failed\n");
        /*TODO: Renegotiate for a different speed */
        /* Sending training after the messages */
        s->state = T30_STATE_D;
        set_phase(s, T30_PHASE_C_TX);
        break;
    default:
        fprintf(stderr, "Unexpected FTT received in state %d\n", s->state);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_rx_mps(t30_state_t *s, uint8_t *msg, int len)
{
    /* Multi-page signal */
    switch (s->state)
    {
    case T30_STATE_F:
        s->timer_t4 = 0;
        set_phase(s, T30_PHASE_BDE_TX);
        s->state = T30_STATE_F_MPS_MCF;
        if (s->phase_d_handler)
            s->phase_d_handler(s, s->phase_d_user_data, T30_MPS);
        t4_rx_set_sub_address(&(s->t4), s->sub_address);
        t4_rx_set_far_ident(&(s->t4), s->far_ident);
        t4_rx_set_vendor(&(s->t4), s->vendor);
        t4_rx_set_model(&(s->t4), s->model);
        t4_rx_start_page(&(s->t4));
        send_frame(s, mcf_frame, 1, TRUE);
        break;
    default:
        fprintf(stderr, "Unexpected MPS received in state %d\n", s->state);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_rx_eom(t30_state_t *s, uint8_t *msg, int len)
{
    /* End of message */
    switch (s->state)
    {
    case T30_STATE_F:
        s->timer_t4 = 0;
        set_phase(s, T30_PHASE_BDE_TX);
        s->state = T30_STATE_R;
        if (s->phase_d_handler)
            s->phase_d_handler(s, s->phase_d_user_data, T30_EOM);
        send_frame(s, mcf_frame, 1, TRUE);
        break;
    default:
        fprintf(stderr, "Unexpected EOM received in state %d\n", s->state);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_rx_eop(t30_state_t *s, uint8_t *msg, int len)
{
    /* End of procedure */
    switch (s->state)
    {
    case T30_STATE_F:
        set_phase(s, T30_PHASE_BDE_TX);
        s->state = T30_STATE_F_EOP_MCF;
        if (s->phase_d_handler)
            s->phase_d_handler(s, s->phase_d_user_data, T30_EOP);
        t4_rx_end(&(s->t4));
        send_frame(s, mcf_frame, 1, TRUE);
        break;
    default:
        fprintf(stderr, "Unexpected EOP received in state %d\n", s->state);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_rx_mcf(t30_state_t *s, uint8_t *msg, int len)
{
    /* Message confirmation */
    switch (s->state)
    {
    case T30_STATE_II_MPS:
        s->timer_t4 = 0;
        if (s->phase_d_handler)
            s->phase_d_handler(s, s->phase_d_user_data, T30_MCF);
        s->state = T30_STATE_I;
        set_phase(s, T30_PHASE_C_TX);
        break;
    case T30_STATE_II_EOM:
        s->timer_t4 = 0;
        if (s->phase_d_handler)
            s->phase_d_handler(s, s->phase_d_user_data, T30_MCF);
        s->state = T30_STATE_R;
        fprintf(stderr, "Success - delivered %d pages\n", s->t4.pages_transferred);
        break;
    case T30_STATE_II_EOP:
        if (s->phase_d_handler)
            s->phase_d_handler(s, s->phase_d_user_data, T30_MCF);
        send_dcn(s);
        fprintf(stderr, "Success - delivered %d pages\n", s->t4.pages_transferred);
        break;
    default:
        fprintf(stderr, "Unexpected MCF received in state %d\n", s->state);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_rx_rtp(t30_state_t *s, uint8_t *msg, int len)
{
    /* Retrain positive */
    switch (s->state)
    {
    case T30_STATE_II_MPS:
        s->timer_t4 = 0;
        if (s->phase_d_handler)
            s->phase_d_handler(s, s->phase_d_user_data, T30_RTP);
        s->state = T30_STATE_I;
        set_phase(s, T30_PHASE_C_TX);
        break;
    case T30_STATE_II_EOM:
        s->timer_t4 = 0;
        if (s->phase_d_handler)
            s->phase_d_handler(s, s->phase_d_user_data, T30_RTP);
        s->state = T30_STATE_R;
        break;
    case T30_STATE_II_EOP:
        if (s->phase_d_handler)
            s->phase_d_handler(s, s->phase_d_user_data, T30_RTP);
        send_dcn(s);
        break;
    default:
        fprintf(stderr, "Unexpected RTP received in state %d\n", s->state);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_rx_rtn(t30_state_t *s, uint8_t *msg, int len)
{
    /* Retrain negative */
    switch (s->state)
    {
    case T30_STATE_II_MPS:
        if (s->phase_d_handler)
            s->phase_d_handler(s, s->phase_d_user_data, T30_RTN);
        send_dcn(s);
        break;
    case T30_STATE_II_EOM:
        if (s->phase_d_handler)
            s->phase_d_handler(s, s->phase_d_user_data, T30_RTN);
        send_dcn(s);
        break;
    case T30_STATE_II_EOP:
        if (s->phase_d_handler)
            s->phase_d_handler(s, s->phase_d_user_data, T30_RTN);
        send_dcn(s);
        break;
    default:
        fprintf(stderr, "Unexpected RTN received in state %d\n", s->state);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_rx_pip(t30_state_t *s, uint8_t *msg, int len)
{
    /* Procedure interrupt positive */
    fprintf(stderr, "Unexpected PIP received in state %d\n", s->state);
}
/*- End of function --------------------------------------------------------*/

static void process_rx_pin(t30_state_t *s, uint8_t *msg, int len)
{
    /* Procedure interrupt negative */
    fprintf(stderr, "Unexpected PIN received in state %d\n", s->state);
}
/*- End of function --------------------------------------------------------*/

static void process_rx_crp(t30_state_t *s, uint8_t *msg, int len)
{
    /* Command repeat */
    fprintf(stderr, "Unexpected CRP received in state %d\n", s->state);
}
/*- End of function --------------------------------------------------------*/

static void hdlc_accept(void *user_data, uint8_t *msg, int len)
{
    t30_state_t *s;
    int final_frame;
    
    s = (t30_state_t *) user_data;

    if (len < 0)
    {
        /* Special conditions */
        switch (len)
        {
        case PUTBIT_CARRIER_UP:
            fprintf(stderr, "Slow carrier up\n");
            s->rx_signal_present = TRUE;
            s->timer_sig_on = SAMPLE_RATE*DEFAULT_TIMER_SIG_ON;
            break;
        case PUTBIT_CARRIER_DOWN:
            fprintf(stderr, "Slow carrier down\n");
            s->rx_signal_present = FALSE;
            break;
        default:
            fprintf(stderr, "Unexpected HDLC special length - %d!\n", len);
            break;
        }
        return;
    }
    
    /* Cancel the response timer */
    s->timer_t2 = 0;
    if (msg[0] != 0xFF  ||  !(msg[1] == 0x03  ||  msg[1] == 0x13))
    {
        fprintf(stderr, "Bad frame header - %02x %02x", msg[0], msg[1]);
        return;
    }
    print_frame(s, "<<<", &msg[2], len - 2);

    final_frame = msg[1] & 0x10;
    switch (s->phase)
    {
    case T30_PHASE_A_CED:
    case T30_PHASE_A_CNG:
    case T30_PHASE_BDE_RX:
        s->msgendtime = s->samplecount + 4*SAMPLE_RATE;  /* reset timeout counter (4 secs in future) */
        break;
    default:
        fprintf(stderr, "Unexpected HDLC frame received\n");
        break;
    }

    if (!final_frame)
    {
        fprintf(stderr, "%s without final frame tag\n", t30_frametype(msg[2]));
        /* The following handles all the message types we expect to get without
           a final frame tag. If we get one that T.30 says we should not expect
           in a particular context, its pretty harmless, so don't worry. */
        switch (msg[2])
        {
        case T30_CSI:
            /* OK in (NSF) (CSI) DIS */
            decode_20digit_msg(s, s->far_ident, &msg[2], len - 2);
            break;
        case T30_PWD:
            /* OK in (PWD) (SUB) (TSI) DCS */
            /* OK in (PWD) (SEP) (CIG) DTC */
            decode_password(s, s->password, &msg[2], len - 2);
            break;
        case T30_SEP:
            /* OK in (PWD) (SEP) (CIG) DTC */
            decode_20digit_msg(s, NULL, &msg[2], len - 2);
            break;
        case T30_TSI:
            /* OK in (TSI) DCS */
            /* OK in (PWD) (SUB) (TSI) DCS */
            decode_20digit_msg(s, s->far_ident, &msg[2], len - 2);
            break;
        case T30_SUB:
            /* OK in (PWD) (SUB) (TSI) DCS */
            decode_20digit_msg(s, s->sub_address, &msg[2], len - 2);
            break;
        case T30_SID:
            /* T.30 does not say where this is OK */
            decode_20digit_msg(s, NULL, &msg[2], len - 2);
            break;
        case T30_NSF:
            /* OK in (NSF) (CSI) DIS */
            if (t35_decode(&msg[3], len - 3, &s->vendor, &s->model))
            {
                if (s->vendor)
                    fprintf(stderr, "The remote is made by '%s'\n", s->vendor);
                if (s->model)
                    fprintf(stderr, "The remote is a '%s'\n", s->model);
            }
            break;
        case T30_NSC:
            /* OK in (NSC) (CIG) DTC */
            break;
        case T30_CIG:
            /* OK in (NSC) (CIG) DTC */
            /* OK in (PWD) (SEP) (CIG) DTC */
            decode_20digit_msg(s, NULL, &msg[2], len - 2);
            break;
        case T30_PSA:
            decode_20digit_msg(s, NULL, &msg[2], len - 2);
            break;
        case T30_CSA:
            decode_url_msg(s, NULL, &msg[2], len - 2);
            break;
        case T30_TSA:
            decode_url_msg(s, NULL, &msg[2], len - 2);
            break;
        case T30_CIA:
            decode_url_msg(s, NULL, &msg[2], len - 2);
            break;
        case T30_IRA:
            decode_url_msg(s, NULL, &msg[2], len - 2);
            break;
        case T30_ISP:
            decode_url_msg(s, NULL, &msg[2], len - 2);
            break;
        default:
            fprintf(stderr, "Unexpected %s frame\n", t30_frametype(msg[2]));
            break;
        }
    }
    else
    {
        fprintf(stderr, "%s with final frame tag\n", t30_frametype(msg[2]));
        /* Once we have any successful message from the far end, we
           cancel timer T1 */
        s->timer_t1 = 0;

        /* The following handles context sensitive message types, which should
           occur at the end of message sequences. They should, therefore have
           the final frame flag set. */
        fprintf(stderr, "In state %d\n", s->state);
        switch (msg[2])
        {
        case T30_DCS:
            process_rx_dcs(s, msg, len);
            break;
        case T30_DIS:
            process_rx_dis(s, msg, len);
            break;
        case T30_DTC:
            process_rx_dtc(s, msg, len);
            break;
        case T30_NSS:
            break;
        case T30_CFR:
            process_rx_cfr(s, msg, len);
            break;
        case T30_FTT:
            process_rx_ftt(s, msg, len);
            break;
        case T30_MPS:
            process_rx_mps(s, msg, len);
            break;
        case T30_EOM:
            process_rx_eom(s, msg, len);
            break;
        case T30_EOP:
            process_rx_eop(s, msg, len);
            break;
        case T30_PRI_EOM:
            break;
        case T30_PRI_MPS:
            break;
        case T30_PRI_EOP:
            break;
        case T30_MCF:
            process_rx_mcf(s, msg, len);
            break;
        case T30_RTP:
            process_rx_rtp(s, msg, len);
            break;
        case T30_RTN:
            process_rx_rtn(s, msg, len);
            break;
        case T30_PIP:
            process_rx_pip(s, msg, len);
            break;
        case T30_PIN:
            process_rx_pin(s, msg, len);
            break;
        case T30_FDM:
            break;
        case T30_DCN:
        case T30_XCN:
            /* Time to disconnect */
            disconnect(s);
            break;
        case T30_CRP:
            process_rx_crp(s, msg, len);
            break;
        case T30_FNV:
            break;
        }
    }
}
/*- End of function --------------------------------------------------------*/

static void set_phase(t30_state_t *s, int phase)
{
    tone_gen_descriptor_t tone_desc;

    if (phase != s->phase)
    {
        switch (phase)
        {
        case T30_PHASE_A_CED:
            /* 0.2s of silence, then 2.6s to 4s of 2100Hz tone, then 75ms of
               silence. */
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
            if (s->t30_flush_handler)
                s->t30_flush_handler(s, s->t30_flush_user_data, 3);
            hdlc_rx_init(&(s->hdlcrx), hdlc_accept, s);
            fsk_rx_init(&(s->v21rx), &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) hdlc_rx_bit, &(s->hdlcrx));
            break;
        case T30_PHASE_A_CNG:
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
            if (s->t30_flush_handler)
                s->t30_flush_handler(s, s->t30_flush_user_data, 3);
            hdlc_rx_init(&(s->hdlcrx), hdlc_accept, s);
            fsk_rx_init(&(s->v21rx), &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) hdlc_rx_bit, &(s->hdlcrx));
            break;
        case T30_PHASE_BDE_RX:
            if (s->t30_flush_handler)
                s->t30_flush_handler(s, s->t30_flush_user_data, 3);
            hdlc_rx_init(&(s->hdlcrx), hdlc_accept, s);
            fsk_rx_init(&(s->v21rx), &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) hdlc_rx_bit, &(s->hdlcrx));
            break;
        case T30_PHASE_BDE_TX:
            hdlc_tx_init(&(s->hdlctx), hdlc_tx_underflow, s);
            fsk_tx_init(&(s->v21tx), &preset_fsk_specs[FSK_V21CH2], (get_bit_func_t) hdlc_tx_getbit, &(s->hdlctx));
            if (s->phase == T30_PHASE_C_TX)
            {
                /* Pause before switching from phase C, as per T.30. If we omit this, the receiver
                   might not see the carrier fall between the high speed and low speed sections. */
                s->silent_samples += SAMPLE_RATE*0.075;
            }
            hdlc_tx_preamble(&(s->hdlctx), 40);
            break;
        case T30_PHASE_C_RX:
            /* Old state is tx_ctl or rx_ctl */
            if (s->phase == T30_PHASE_BDE_TX)
            {
                if (s->t30_flush_handler)
                    s->t30_flush_handler(s, s->t30_flush_user_data, 3);
            }
            s->rx_signal_present = FALSE;
            if (s->bit_rate == 4800  ||  s->bit_rate == 2400)
                v27ter_rx_restart(&(s->v27ter_rx), s->bit_rate);
            else
                v29_rx_restart(&(s->v29rx), s->bit_rate);
            break;
        case T30_PHASE_C_TX:
            /* Pause before switching from anything to phase C */
            s->training_test_bits = (3*s->bit_rate)/2;
            s->silent_samples += SAMPLE_RATE*0.075;
            if (s->bit_rate == 4800  ||  s->bit_rate == 2400)
                v27ter_tx_restart(&(s->v27ter_tx), s->bit_rate);
            else
                v29_tx_restart(&(s->v29tx), s->bit_rate);
            break;
        case T30_PHASE_E_DONE:
            /* Send a little silence before ending things, to ensure the
               buffers are all flushed through, and the far end has seen
               the last message we sent. */
            s->silent_samples += SAMPLE_RATE*0.2;
            s->training_current_zeros = 0;
            s->training_most_zeros = 0;
            break;
        }
        fprintf(stderr, "Changed from phase %d to %d\n", s->phase, phase);
        s->phase = phase;
    }
}
/*- End of function --------------------------------------------------------*/

void timer_t1_expired(t30_state_t *s)
{
    fprintf(stderr, "T1 timeout in state %d\n", s->state);
    /* The initial connection establishment has timeout out. In other words, we
       have been unable to communicate successfully with a remote machine.
       It is time to abandon the call. */
    switch (s->state)
    {
    case T30_STATE_T:
        /* Just end the call */
        disconnect(s);
        break;
    case T30_STATE_R:
        /* Send and disconnect, and then end the call. Since we have not
           successfully contacted the far end, it is unclear why we should
           send a disconnect message at this point. However, it is what T.30
           says we should do. */
        send_dcn(s);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

void timer_t2_expired(t30_state_t *s)
{
    fprintf(stderr, "T2 timeout\n");
    start_receiving_document(s);
}
/*- End of function --------------------------------------------------------*/

void timer_t3_expired(t30_state_t *s)
{
    fprintf(stderr, "T3 timeout\n");
}
/*- End of function --------------------------------------------------------*/

void timer_t4_expired(t30_state_t *s)
{
    /* There was no response (or only a corrupt response) to a command */
    fprintf(stderr, "T4 timeout in state %d\n", s->state);
    switch (s->state)
    {
    case T30_STATE_F_EOP_MCF:
        set_phase(s, T30_PHASE_BDE_TX);
        send_frame(s, mcf_frame, 1, TRUE);
        break;
    case T30_STATE_R:
        set_phase(s, T30_PHASE_BDE_TX);
        send_ident_frame(s, T30_CSI, FALSE);
        build_dis(s);
        send_frame(s, s->dis_frame, s->dis_len, TRUE);
        break;
    case T30_STATE_II_MPS:
        set_phase(s, T30_PHASE_BDE_TX);
        send_frame(s, mps_frame, 1, TRUE);
        break;
    case T30_STATE_II_EOM:
        set_phase(s, T30_PHASE_BDE_TX);
        send_frame(s, eom_frame, 1, TRUE);
        break;
    case T30_STATE_II_EOP:
        set_phase(s, T30_PHASE_BDE_TX);
        send_frame(s, eop_frame, 1, TRUE);
        break;
    case T30_STATE_D:
        break;
    }
}
/*- End of function --------------------------------------------------------*/

char *t30_frametype(uint8_t x)
{
    switch (x)
    {
    case T30_DIS:
        return "DIS";
    case T30_CSI:
        return "CSI";
    case T30_NSF:
        return "NSF";
    case T30_DTC:
        return "DTC";
    case T30_CIG:
        return "CIG";
    case T30_NSC:
        return "NSC";
    case T30_PWD:
        return "PWD";
    case T30_SEP:
        return "SEP";
    case T30_PSA:
        return "PSA";
    case T30_CIA:
        return "CIA";
    case T30_ISP:
        return "ISP";
    case T30_DCS:
        return "DCS";
    case T30_TSI:
        return "TSI";
    case T30_NSS:
        return "NSS";
    case T30_SUB:
        return "SUB";
    case T30_SID:
        return "SID";
    case T30_TSA:
        return "TSA";
    case T30_IRA:
        return "IRA";
    case T30_CFR:
        return "CFR";
    case T30_FTT:
        return "FTT";
    case T30_CSA:
        return "CSA";
    case T30_EOM:
        return "EOM";
    case T30_MPS:
        return "MPS";
    case T30_EOP:
        return "EOP";
    case T30_PRI_EOM:
        return "PRI_EOM";
    case T30_PRI_MPS:
        return "PRI_MPS";
    case T30_PRI_EOP:
        return "PRI_EOP";
    case T30_MCF:
        return "MCF";
    case T30_RTP:
        return "RTP";
    case T30_RTN:
        return "RTN";
    case T30_PIP:
        return "PIP";
    case T30_PIN:
        return "PIN";
    case T30_FDM:
        return "FDM";
    case T30_DCN:
        return "DCN";
    case T30_XCN:
        return "XCN";
    case T30_CRP:
        return "CRP";
    case T30_FNV:
        return "FNV";
    default:
        return "???";
    }
}
/*- End of function --------------------------------------------------------*/

void decode_20digit_msg(t30_state_t *s, char *msg, const uint8_t *pkt, int len)
{
    int p;
    int k;
    char text[20 + 1];

    if (msg == NULL)
        msg = text;
    if (len > 21)
    {
        fprintf(stderr, "Bad %s frame length - %d\n", t30_frametype(pkt[0]), len);
        msg[0] = '\0';
        return;
    }
    p = len;
    /* Strip trailing spaces */
    while (p > 1  &&  pkt[p - 1] == ' ')
        p--;
    /* The string is actually backwards in the message */
    k = 0;
    while (p > 1)
        msg[k++] = pkt[--p];
    msg[k] = '\0';
    fprintf(stderr, "Remote fax gave %s as: \"%s\"\n", t30_frametype(pkt[0]), msg);
}
/*- End of function --------------------------------------------------------*/

void decode_password(t30_state_t *s, char *msg, const uint8_t *pkt, int len)
{
    int p;
    int k;
    char text[20 + 1];

    if (msg == NULL)
        msg = text;
    if (len > 21)
    {
        fprintf(stderr, "Bad password frame length - %d\n", len);
        msg[0] = '\0';
        return;
    }
    p = len;
    /* Strip trailing spaces */
    while (p > 1  &&  pkt[p - 1] == ' ')
        p--;
    /* The string is actually backwards in the message */
    k = 0;
    while (p > 1)
        msg[k++] = pkt[--p];
    msg[k] = '\0';
    fprintf(stderr, "Remote fax gave the password as: \"%s\"\n", msg);
}
/*- End of function --------------------------------------------------------*/

void decode_url_msg(t30_state_t *s, char *msg, const uint8_t *pkt, int len)
{
    int p;
    int k;
    char text[77 + 1];

    if (msg == NULL)
        msg = text;
    if (len < 3  ||  len > 77 + 3  ||  len != pkt[2] + 3)
    {
        fprintf(stderr, "Bad %s frame length - %d\n", t30_frametype(pkt[0]), len);
        msg[0] = '\0';
        return;
    }
    memcpy(msg, &pkt[3], len - 3);
    msg[len - 3] = '\0';
    fprintf(stderr, "Remote fax gave %s as: %d, %d, \"%s\"\n", t30_frametype(pkt[0]), pkt[0], pkt[1], msg);
}
/*- End of function --------------------------------------------------------*/

void t30_decode_dis_dtc_dcs(t30_state_t *s, uint8_t *pkt, int len)
{
    fprintf(stderr, "%s:\n", t30_frametype(pkt[0]));
    
    if ((pkt[1] & DISBIT1))
        fprintf(stderr, "Store and forward Internet fax\n");
    if ((pkt[1] & DISBIT3))
        fprintf(stderr, "Real-time Internet fax\n");
    if (pkt[0] == T30_DCS)
    {
        if ((pkt[1] & DISBIT6))
            fprintf(stderr, "Invalid: 1\n");
        if ((pkt[1] & DISBIT7))
            fprintf(stderr, "Invalid: 1\n");
    }
    else
    {
        if ((pkt[1] & DISBIT6))
            fprintf(stderr, "V.8 capable\n");
        fprintf(stderr, "Preferred octets: %d\n", (pkt[1] & DISBIT7)  ?  64  :  256);
    }
    if ((pkt[1] & (DISBIT2 | DISBIT4 | DISBIT5 | DISBIT8)))
        fprintf(stderr, "Reserved: 0x%X\n", (pkt[1] & (DISBIT2 | DISBIT4 | DISBIT5 | DISBIT8)));

    if (pkt[0] == T30_DCS)
    {
        if ((pkt[2] & DISBIT1))
            fprintf(stderr, "Set to \"0\": 1\n");
    }
    else
    {
        if ((pkt[2] & DISBIT1))
            fprintf(stderr, "Ready to transmit a fax document (polling)\n");
    }
    if ((pkt[2] & DISBIT2))
        fprintf(stderr, "Can receive fax\n");
    if (pkt[0] == T30_DCS)
    {
        fprintf(stderr, "Selected data signalling rate: ");
        switch (pkt[2] & (DISBIT6 | DISBIT5 | DISBIT4 | DISBIT3))
        {
        case 0:
            fprintf(stderr, "V.27ter, 2400bps\n");
            break;
        case DISBIT4:
            fprintf(stderr, "V.27ter, 4800bps\n");
            break;
        case DISBIT3:
            fprintf(stderr, "V.29, 9600bps\n");
            break;
        case (DISBIT4 | DISBIT3):
            fprintf(stderr, "V.29, 7200bps\n");
            break;
        case DISBIT6:
            fprintf(stderr, "V17, 14400bps\n");
            break;
        case (DISBIT6 | DISBIT4):
            fprintf(stderr, "V17, 12000bps\n");
            break;
        case (DISBIT6 | DISBIT3):
            fprintf(stderr, "V17, 9600bps\n");
            break;
        case (DISBIT6 | DISBIT4 | DISBIT3):
            fprintf(stderr, "V17, 7200bps\n");
            break;
        case (DISBIT5 | DISBIT3):
        case (DISBIT5 | DISBIT4 | DISBIT3):
        case (DISBIT6 | DISBIT5):
        case (DISBIT6 | DISBIT5 | DISBIT3):
        case (DISBIT6 | DISBIT5 | DISBIT4):
        case (DISBIT6 | DISBIT5 | DISBIT4 | DISBIT3):
            fprintf(stderr, "Reserved\n");
            break;
        default:
            fprintf(stderr, "Not used\n");
            break;
        }
    }
    else
    {
        fprintf(stderr, "Supported data signalling rates: ");
        switch (pkt[2] & (DISBIT6 | DISBIT5 | DISBIT4 | DISBIT3))
        {
        case 0:
            fprintf(stderr, "V.27ter fallback mode\n");
            break;
        case DISBIT4:
            fprintf(stderr, "V.27ter\n");
            break;
        case DISBIT3:
            fprintf(stderr, "V.29\n");
            break;
        case (DISBIT4 | DISBIT3):
            fprintf(stderr, "V.27ter and V.29\n");
            break;
        case (DISBIT6 | DISBIT4 | DISBIT3):
            fprintf(stderr, "V.27ter, V.29 and V.17\n");
            break;
        case (DISBIT5 | DISBIT4):
        case (DISBIT6 | DISBIT4):
        case (DISBIT6 | DISBIT5 | DISBIT4):
        case (DISBIT6 | DISBIT5 | DISBIT4 | DISBIT3):
            fprintf(stderr, "Reserved\n");
            break;
        default:
            fprintf(stderr, "Not used\n");
            break;
        }
    }
    if ((pkt[2] & DISBIT7))
        fprintf(stderr, "R8x7.7lines/mm and/or 200x200pels/25.4mm OK\n");
    if ((pkt[2] & DISBIT8))
        fprintf(stderr, "2D coding OK\n");

    if (pkt[0] == T30_DCS)
    {
        fprintf(stderr, "Scan line length: ");
        switch (pkt[3] & (DISBIT2 | DISBIT1))
        {
        case 0:
            fprintf(stderr, "215mm\n");
            break;
        case DISBIT2:
            fprintf(stderr, "303mm\n");
            break;
        case DISBIT1:
            fprintf(stderr, "255mm\n");
            break;
        default:
            fprintf(stderr, "Invalid\n");
            break;
        }
        fprintf(stderr, "Recording length: ");
        switch (pkt[3] & (DISBIT4 | DISBIT3))
        {
        case 0:
            fprintf(stderr, "A4 (297mm)\n");
            break;
        case DISBIT3:
            fprintf(stderr, "Unlimited\n");
            break;
        case DISBIT4:
            fprintf(stderr, "B4 (364mm)\n");
            break;
        case (DISBIT4 | DISBIT3):
            fprintf(stderr, "Invalid\n");
            break;
        }
        fprintf(stderr, "Minimum scan line time: ");
        switch (pkt[3] & (DISBIT7 | DISBIT6 | DISBIT5))
        {
        case 0:
            fprintf(stderr, "20ms\n");
            break;
        case DISBIT7:
            fprintf(stderr, "40ms\n");
            break;
        case DISBIT6:
            fprintf(stderr, "10ms\n");
            break;
        case DISBIT5:
            fprintf(stderr, "5ms\n");
            break;
        case (DISBIT7 | DISBIT6 | DISBIT5):
            fprintf(stderr, "0ms\n");
            break;
        default:
            fprintf(stderr, "Invalid\n");
            break;
        }
    }
    else
    {
        fprintf(stderr, "Scan line length: ");
        switch (pkt[3] & (DISBIT2 | DISBIT1))
        {
        case 0:
            fprintf(stderr, "215mm\n");
            break;
        case DISBIT2:
            fprintf(stderr, "215mm, 255mm or 303mm\n");
            break;
        case DISBIT1:
            fprintf(stderr, "215mm or 255mm\n");
            break;
        default:
            fprintf(stderr, "Invalid\n");
            break;
        }
        fprintf(stderr, "Recording length: ");
        switch (pkt[3] & (DISBIT4 | DISBIT3))
        {
        case 0:
            fprintf(stderr, "A4 (297mm)\n");
            break;
        case DISBIT3:
            fprintf(stderr, "Unlimited\n");
            break;
        case DISBIT4:
            fprintf(stderr, "A4 (297mm) and B4 (364mm)\n");
            break;
        case (DISBIT4 | DISBIT3):
            fprintf(stderr, "Invalid\n");
            break;
        }
        fprintf(stderr, "Receiver's minimum scan line time: ");
        switch (pkt[3] & (DISBIT7 | DISBIT6 | DISBIT5))
        {
        case 0:
            fprintf(stderr, "20ms at 3.85 l/mm: T7.7 = T3.85\n");
            break;
        case DISBIT7:
            fprintf(stderr, "40ms at 3.85 l/mm: T7.7 = T3.85\n");
            break;
        case DISBIT6:
            fprintf(stderr, "10ms at 3.85 l/mm: T7.7 = T3.85\n");
            break;
        case DISBIT5:
            fprintf(stderr, "5ms at 3.85 l/mm: T7.7 = T3.85\n");
            break;
        case (DISBIT7 | DISBIT6):
            fprintf(stderr, "10ms at 3.85 l/mm: T7.7 = 1/2 T3.85\n");
            break;
        case (DISBIT6 | DISBIT5):
            fprintf(stderr, "20ms at 3.85 l/mm: T7.7 = 1/2 T3.85\n");
            break;
        case (DISBIT7 | DISBIT5):
            fprintf(stderr, "40ms at 3.85 l/mm: T7.7 = 1/2 T3.85\n");
            break;
        case (DISBIT7 | DISBIT6 | DISBIT5):
            fprintf(stderr, "0ms at 3.85 l/mm: T7.7 = T3.85\n");
            break;
        }
    }
    if (!(pkt[3] & DISBIT8))
        return;

    if ((pkt[4] & DISBIT2))
        fprintf(stderr, "Uncompressed mode\n");
    if ((pkt[4] & DISBIT3))
        fprintf(stderr, "Error correction mode\n");
    if (pkt[2] == T30_DCS)
    {
        fprintf(stderr, "Frame size: %s\n", (pkt[4] & DISBIT4)  ?  "256 octets"  :  "64 octets");
    }
    else
    {
        if ((pkt[4] & DISBIT4))
            fprintf(stderr, "Set to \"0\": 0x%X\n", (pkt[4] & DISBIT4));
    }
    if ((pkt[4] & DISBIT7))
        fprintf(stderr, "T.6 coding\n");
    if ((pkt[4] & (DISBIT1 | DISBIT5 | DISBIT6)))
        fprintf(stderr, "Reserved: 0x%X\n", (pkt[4] & (DISBIT1 | DISBIT5 | DISBIT6)));
    if (!(pkt[4] & DISBIT8))
        return;

    if ((pkt[5] & DISBIT1))
        fprintf(stderr, "\"Field not valid\" supported\n");
    if (pkt[2] == T30_DCS)
    {
        if ((pkt[5] & DISBIT2))
            fprintf(stderr, "Set to \"0\": 1\n");
        if ((pkt[5] & DISBIT3))
            fprintf(stderr, "Set to \"0\": 1\n");
    }
    else
    {
        if ((pkt[5] & DISBIT2))
            fprintf(stderr, "Multiple selective polling\n");
        if ((pkt[5] & DISBIT3))
            fprintf(stderr, "Polled Subaddress\n");
    }
    if ((pkt[5] & DISBIT4))
        fprintf(stderr, "T.43 coding OK\n");
    if ((pkt[5] & DISBIT5))
        fprintf(stderr, "Plane interleave OK\n");
    if ((pkt[5] & DISBIT6))
        fprintf(stderr, "Voice coding with 32kbit/s ADPCM (Rec. G.726) OK\n");
    if ((pkt[5] & DISBIT7))
        fprintf(stderr, "Reserved for the use of extended voice coding set\n");
    if (!(pkt[5] & DISBIT8))
        return;

    if ((pkt[6] & DISBIT1))
        fprintf(stderr, "R8x15.4lines/mm OK\n");
    if ((pkt[6] & DISBIT2))
        fprintf(stderr, "300x300pels/25.4mm OK\n");
    if ((pkt[6] & DISBIT3))
        fprintf(stderr, "R16x15.4lines/mm and/or 400x400pels/25.4 mm OK\n");
    if (pkt[2] == T30_DCS)
    {
        fprintf(stderr, "Resolution type selection: %s\n", (pkt[6] & DISBIT4)  ?  "inch-based"  :  "metric-based");
        if ((pkt[6] & DISBIT5))
            fprintf(stderr, "Don't care: 1\n");
        if ((pkt[6] & DISBIT6))
            fprintf(stderr, "Don't care: 1\n");
    }
    else
    {
        if ((pkt[6] & DISBIT4))
            fprintf(stderr, "Inch-based resolution preferred\n");
        if ((pkt[6] & DISBIT5))
            fprintf(stderr, "Metric-based resolution preferred\n");
        fprintf(stderr, "Minimum scan line time for higher resolutions: %s\n", (pkt[6] & DISBIT6)  ?  "T15.4 = 1/2 T7.7"  :  "T15.4 = T7.7");
    }
    if (pkt[2] == T30_DCS)
    {
        if ((pkt[6] & DISBIT7))
            fprintf(stderr, "Set to \"0\": 1\n");
    }
    else
    {
        if ((pkt[6] & DISBIT7))
            fprintf(stderr, "Selective polling OK\n");
    }
    if (!(pkt[6] & DISBIT8))
        return;

    if ((pkt[7] & DISBIT1))
        fprintf(stderr, "Subaddressing OK\n");
    if ((pkt[7] & DISBIT2))
        fprintf(stderr, "Password OK\n");
    if (pkt[2] == T30_DCS)
    {
        if ((pkt[7] & DISBIT3))
            fprintf(stderr, "Set to \"0\": 1\n");
    }
    else
    {
        if ((pkt[7] & DISBIT3))
            fprintf(stderr, "Ready to transmit a data file (polling)\n");
    }
    if ((pkt[7] & DISBIT5))
        fprintf(stderr, "Binary file transfer (BFT) OK\n");
    if ((pkt[7] & DISBIT6))
        fprintf(stderr, "Document transfer mode (DTM) OK\n");
    if ((pkt[7] & DISBIT7))
        fprintf(stderr, "Electronic data interchange (EDI) OK\n");
    if ((pkt[7] & DISBIT4))
        fprintf(stderr, "Reserved: 1\n");
    if (!(pkt[7] & DISBIT8))
        return;

    if ((pkt[8] & DISBIT1))
        fprintf(stderr, "Basic transfer mode (BTM) OK\n");
    if (pkt[2] == T30_DCS)
    {
        if ((pkt[8] & DISBIT3))
            fprintf(stderr, "Set to \"0\": 1\n");
    }
    else
    {
        if ((pkt[8] & DISBIT3))
            fprintf(stderr, "Ready to transfer a character or mixed mode document (polling) OK\n");
    }
    if ((pkt[8] & DISBIT4))
        fprintf(stderr, "Character mode\n");
    if ((pkt[8] & DISBIT6))
        fprintf(stderr, "Mixed mode (Annex E/T.4)\n");
    if ((pkt[8] & (DISBIT2 | DISBIT5 | DISBIT7)))
        fprintf(stderr, "Reserved: 0x%X\n", (pkt[8] & (DISBIT2 | DISBIT5 | DISBIT7)));
    if (!(pkt[8] & DISBIT8))
        return;

    if ((pkt[9] & DISBIT1))
        fprintf(stderr, "Processable mode 26 (Rec. T.505)\n");
    if ((pkt[9] & DISBIT2))
        fprintf(stderr, "Digital network\n");
    if (pkt[2] == T30_DCS)
    {
        if ((pkt[9] & DISBIT3))
            fprintf(stderr, "Duplex or half-duplex\n");
        if ((pkt[9] & DISBIT4))
            fprintf(stderr, "Full colour mode\n");
    }
    else
    {
        if ((pkt[9] & DISBIT3))
            fprintf(stderr, "Duplex\n");
        if ((pkt[9] & DISBIT4))
            fprintf(stderr, "JPEG coding\n");
    }
    if ((pkt[9] & DISBIT5))
        fprintf(stderr, "Full colour mode\n");
    if (pkt[2] == T30_DCS)
    {
        if ((pkt[9] & DISBIT6))
            fprintf(stderr, "Preferred Huffman tables\n");
    }
    else
    {
        if ((pkt[9] & DISBIT6))
            fprintf(stderr, "Set to \"0\": 1\n");
    }
    if ((pkt[9] & DISBIT7))
        fprintf(stderr, "12bits/pel component\n");
    if (!(pkt[9] & DISBIT8))
        return;

    if ((pkt[10] & DISBIT1))
        fprintf(stderr, "No subsampling (1:1:1)\n");
    if ((pkt[10] & DISBIT2))
        fprintf(stderr, "Custom illuminant\n");
    if ((pkt[10] & DISBIT3))
        fprintf(stderr, "Custom gamut range\n");
    if ((pkt[10] & DISBIT4))
        fprintf(stderr, "North American Letter (215.9mm x 279.4mm)\n");
    if ((pkt[10] & DISBIT5))
        fprintf(stderr, "North American Legal (215.9mm x 355.6mm)\n");
    if ((pkt[10] & DISBIT6))
        fprintf(stderr, "Single-progression sequential coding (Rec. T.85) basic\n");
    if ((pkt[10] & DISBIT7))
        fprintf(stderr, "Single-progression sequential coding (Rec. T.85) optional L0\n");
    if (!(pkt[10] & DISBIT8))
        return;

    if ((pkt[11] & DISBIT1))
        fprintf(stderr, "HKM key management\n");
    if ((pkt[11] & DISBIT2))
        fprintf(stderr, "RSA key management\n");
    if ((pkt[11] & DISBIT3))
        fprintf(stderr, "Override\n");
    if ((pkt[11] & DISBIT4))
        fprintf(stderr, "HFX40 cipher\n");
    if ((pkt[11] & DISBIT5))
        fprintf(stderr, "Alternative cipher number 2\n");
    if ((pkt[11] & DISBIT6))
        fprintf(stderr, "Alternative cipher number 3\n");
    if ((pkt[11] & DISBIT7))
        fprintf(stderr, "HFX40-I hashing\n");
    if (!(pkt[11] & DISBIT8))
        return;

    if ((pkt[12] & DISBIT1))
        fprintf(stderr, "Alternative hashing system 2\n");
    if ((pkt[12] & DISBIT2))
        fprintf(stderr, "Alternative hashing system 3\n");
    if ((pkt[12] & DISBIT3))
        fprintf(stderr, "Reserved for future security features\n");
    if ((pkt[12] & (DISBIT4 | DISBIT5 | DISBIT6)))
        fprintf(stderr, "T.44 (Mixed Raster Content): 0x%X\n", (pkt[12] & (DISBIT4 | DISBIT5 | DISBIT6)));
    if ((pkt[12] & DISBIT6))
        fprintf(stderr, "Page length maximum stripe size for T.44 (Mixed Raster Content)\n");
    if (!(pkt[12] & DISBIT8))
        return;

    if ((pkt[13] & DISBIT1))
        fprintf(stderr, "Colour/gray-scale 300pels/25.4mm x 300lines/25.4mm or 400pels/25.4mm x 400lines/25.4mm resolution\n");
    if ((pkt[13] & DISBIT2))
        fprintf(stderr, "100pels/25.4mm x 100lines/25.4mm for colour/gray scale\n");
    if ((pkt[13] & DISBIT3))
        fprintf(stderr, "Simple phase C BFT negotiations\n");
    if (pkt[2] == T30_DCS)
    {
        if ((pkt[13] & DISBIT4))
            fprintf(stderr, "Set to \"0\": 1\n");
        if ((pkt[13] & DISBIT5))
            fprintf(stderr, "Set to \"0\": 1\n");
    }
    else
    {
        if ((pkt[13] & DISBIT4))
            fprintf(stderr, "Reserved for Extended BFT Negotiations capable\n");
        if ((pkt[13] & DISBIT5))
            fprintf(stderr, "Internet Selective Polling address (ISP)\n");
    }
    if ((pkt[13] & DISBIT6))
        fprintf(stderr, "Internet Routing Address (IRA)\n");
    if ((pkt[13] & DISBIT7))
        fprintf(stderr, "Reserved: 1\n");
    if (!(pkt[13] & DISBIT8))
        return;

    if ((pkt[14] & DISBIT1))
        fprintf(stderr, "600pels/25.4mm x 600lines/25.4mm\n");
    if ((pkt[14] & DISBIT2))
        fprintf(stderr, "1200pels/25.4mm x 1200lines/25.4mm\n");
    if ((pkt[14] & DISBIT3))
        fprintf(stderr, "300pels/25.4mm x 600lines/25.4mm\n");
    if ((pkt[14] & DISBIT4))
        fprintf(stderr, "400pels/25.4mm x 800lines/25.4mm\n");
    if ((pkt[14] & DISBIT5))
        fprintf(stderr, "600pels/25.4mm x 1200lines/25.4mm\n");
    if ((pkt[14] & (DISBIT6 | DISBIT7)))
        fprintf(stderr, "Reserved: 0x%X\n", (pkt[14] & (DISBIT6 | DISBIT7)));
    if (!(pkt[14] & DISBIT8))
        return;
    fprintf(stderr, "Extended beyond the current T.30 specification!\n");
}
/*- End of function --------------------------------------------------------*/

int fax_init(t30_state_t *s, int calling_party, void *user_data)
{
    memset(s, 0, sizeof(*s));
    s->bit_rate = 9600;
    s->phase = T30_PHASE_IDLE;
    v27ter_rx_init(&(s->v27ter_rx), s->bit_rate, fast_putbit, s);
    v27ter_tx_init(&(s->v27ter_tx), s->bit_rate, fast_getbit, s);
    v29_rx_init(&(s->v29rx), s->bit_rate, fast_putbit, s);
    v29_tx_init(&(s->v29tx), s->bit_rate, fast_getbit, s);
    s->rx_signal_present = FALSE;
    if (calling_party)
    {
        s->state = T30_STATE_T;
        set_phase(s, T30_PHASE_A_CNG);
        s->timer_t1 = DEFAULT_TIMER_T1*SAMPLE_RATE;
    }
    else
    {
        s->state = T30_STATE_R;
        set_phase(s, T30_PHASE_A_CED);
    }
#if defined(LOG_FAX_AUDIO)
    {
        char buf[100 + 1];
        struct tm *tm;
        time_t now;

        time(&now);
        tm = localtime(&now);
        sprintf(buf,
                "/tmp/fax-rx-audio-%02d%02d%02d%02d%02d%02d",
                tm->tm_year%100,
                tm->tm_mon + 1,
                tm->tm_mday,
                tm->tm_hour,
                tm->tm_min,
                tm->tm_sec);
        fax_audio_rx_log = open(buf, O_CREAT | O_TRUNC | O_WRONLY, 0666);
        sprintf(buf,
                "/tmp/fax-tx-audio-%02d%02d%02d%02d%02d%02d",
                tm->tm_year%100,
                tm->tm_mon + 1,
                tm->tm_mday,
                tm->tm_hour,
                tm->tm_min,
                tm->tm_sec);
        fax_audio_tx_log = open(buf, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    }
#endif
    return  0;
}
/*- End of function --------------------------------------------------------*/

int fax_rx_process(t30_state_t *s, int16_t *buf, int len)
{
#if defined(LOG_FAX_AUDIO)
    if (fax_audio_rx_log >= 0)
        write(fax_audio_rx_log, buf, len*sizeof(int16_t));
#endif
    switch (s->phase)
    {
    case T30_PHASE_A_CED:
    case T30_PHASE_A_CNG:
    case T30_PHASE_BDE_RX:
        fsk_rx(&(s->v21rx), buf, len);
        break;
    case T30_PHASE_C_RX:
        if (s->bit_rate == 4800  ||  s->bit_rate == 2400)
            v27ter_rx(&(s->v27ter_rx), buf, len);
        else
            v29_rx(&(s->v29rx), buf, len);
        break;
    default:
        /* Ignore */
        break;
    }
    if (s->timer_t1 > 0)
    {
        s->timer_t1 -= len;
        if (s->timer_t1 <= 0)
            timer_t1_expired(s);
    }
    if (s->timer_t2 > 0)
    {
        s->timer_t2 -= len;
        if (s->timer_t2 <= 0)
            timer_t2_expired(s);
    }
    if (s->timer_t3 > 0)
    {
        s->timer_t3 -= len;
        if (s->timer_t3 <= 0)
            timer_t3_expired(s);
    }
    if (s->timer_t4 > 0)
    {
        s->timer_t4 -= len;
        if (s->timer_t4 <= 0)
            timer_t4_expired(s);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/

int fax_tx_process(t30_state_t *s, int16_t *buf, int max_len)
{
    int len;
    int lenx;

    len = 0;
    if (s->silent_samples)
    {
        len = s->silent_samples;
        if (len > max_len)
            len = max_len;
        s->silent_samples -= len;
        max_len -= len;
        memset(buf, 0, len*sizeof(int16_t));
        if (s->silent_samples <= 0)
        {
            if (s->phase == T30_PHASE_E_DONE)
            {
                /* We have now allowed time for the last message to flush
                   through the system, so it is safe to report the end of the
                   call. */
                if (s->phase_e_handler)
                    s->phase_e_handler(s, s->phase_e_user_data, TRUE);
            }
        }
    }
    if (max_len > 0)
    {
        switch (s->phase)
        {
        case T30_PHASE_A_CED:
        case T30_PHASE_A_CNG:
            lenx = tone_gen(&(s->tone_gen), buf + len, max_len);
            len += lenx;
            if (lenx <= 0)
                start_receiving_document(s);
            break;
        case T30_PHASE_BDE_TX:
            len += fsk_tx(&(s->v21tx), buf + len, max_len);
            break;
        case T30_PHASE_C_TX:
            if (s->bit_rate == 4800  ||  s->bit_rate == 2400)
                len += v27ter_tx(&(s->v27ter_tx), buf + len, max_len);
            else
                len += v29_tx(&(s->v29tx), buf + len, max_len);
            break;
        default:
            /* Send silence */
            memset(buf + len, 0, max_len*sizeof(int16_t));
            break;
        }
    }
#if defined(LOG_FAX_AUDIO)
    if (fax_audio_tx_log >= 0)
        write(fax_audio_tx_log, buf, len*sizeof(int16_t));
#endif
    return len;
}
/*- End of function --------------------------------------------------------*/

int fax_set_local_ident(t30_state_t *s, const char *id)
{
    if (id == NULL)
    {
        s->local_ident[0] = '\0';
        return 0;
    }
    if (strlen(id) > 20)
        return -1;
    strcpy(s->local_ident, id);
    return 0;
}
/*- End of function --------------------------------------------------------*/

int fax_set_sub_address(t30_state_t *s, const char *sub_address)
{
    if (sub_address == NULL)
    {
        s->sub_address[0] = '\0';
        return 0;
    }
    if (strlen(sub_address) > 20)
        return -1;
    strcpy(s->sub_address, sub_address);
    return 0;
}
/*- End of function --------------------------------------------------------*/

int fax_get_far_ident(t30_state_t *s, char *id)
{
    strcpy(id, s->far_ident);
    return strlen(id);
}
/*- End of function --------------------------------------------------------*/

void fax_set_phase_b_handler(t30_state_t *s, phase_b_handler_t *handler, void *user_data)
{
    s->phase_b_handler = handler;
    s->phase_b_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

void fax_set_phase_d_handler(t30_state_t *s, phase_d_handler_t *handler, void *user_data)
{
    s->phase_d_handler = handler;
    s->phase_d_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

void fax_set_phase_e_handler(t30_state_t *s, phase_e_handler_t *handler, void *user_data)
{
    s->phase_e_handler = handler;
    s->phase_e_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

void fax_set_flush_handler(t30_state_t *s, t30_flush_handler_t *handler, void *user_data)
{
    s->t30_flush_handler = handler;
    s->t30_flush_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

void fax_set_rx_file(t30_state_t *s, const char *file)
{
    strncpy(s->t4.rx_file, file, sizeof(s->t4.rx_file));
    s->t4.rx_file[sizeof(s->t4.rx_file) - 1] = 0;
}
/*- End of function --------------------------------------------------------*/

void fax_set_tx_file(t30_state_t *s, const char *file)
{
    strncpy(s->t4.tx_file, file, sizeof(s->t4.tx_file));
    s->t4.tx_file[sizeof(s->t4.tx_file) - 1] = 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

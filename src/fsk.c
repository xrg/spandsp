/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fsk.c - FSK modem transmit and receive parts
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
 * $Id: fsk.c,v 1.11 2004/09/19 08:47:11 steveu Exp $
 */

/*! \file */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/complex.h"
#include "spandsp/vector.h"
#include "spandsp/dds.h"
#include "spandsp/power_meter.h"
#include "spandsp/fsk.h"

fsk_spec_t preset_fsk_specs[] =
{
    {
        "V21 ch 1",
        1080 + 100,
        1080 - 100,
        -10,
        -30,
        300
    },
    {
        "V21 ch2",
        1750 + 100,
        1750 - 100,
        -10,
        -30,
        300
    },
    {
        "V23 ch 1",
        2100,
        1300,
        -10,
        -30,
        1200
    },
    {
        "V23 ch 2",
        450,
        390,
        -10,
        -30,
        75
    },
    {
        "Bell103 ch 1",
        2125 - 100,
        2125 + 100,
        -10,
        -30,
        300
    },
    {
        "Bell103 ch 2",
        1170 - 100,
        1170 + 100,
        -10,
        -30,
        300
    },
    {
        "Bell202",
        2200,
        1200,
        -10,
        -30,
        1200
    },
    {
        "Weitbrecht",   /* Used for TDD (Telecomc Device for the Deaf) */
        1800,
        1400,
        -10,
        -30,
         45             /* Actually 45.45 */
    }
};

void async_rx_init(async_rx_state_t *s,
                   int data_bits,
                   int parity,
                   int stop_bits,
                   int use_v14,
                   put_byte_func_t put_byte,
                   void *user_data)
{
    s->data_bits = data_bits;
    s->parity = parity;
    s->stop_bits = stop_bits;
    s->use_v14 = use_v14;

    s->put_byte = put_byte;
    s->user_data = user_data;

    s->byte_in_progress = 0;
    s->bitpos = 0;
    s->parity_bit = 0;

    s->parity_errors = 0;
    s->framing_errors = 0;
}
/*- End of function --------------------------------------------------------*/

void async_rx_bit(void *user_data, int bit)
{
    async_rx_state_t *s;

    s = (async_rx_state_t *) user_data;
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_CARRIER_UP:
        case PUTBIT_CARRIER_DOWN:
        case PUTBIT_TRAINING_SUCCEEDED:
        case PUTBIT_TRAINING_FAILED:
            s->put_byte(s->user_data, bit);
            s->bitpos = 0;
            s->byte_in_progress = 0;
            break;
        default:
            //printf("Eh!\n");
            break;
        }
        return;
    }
    if (s->bitpos == 0)
    {
        if (bit == 0)
        {
            /* Start bit */
            s->bitpos++;
            s->parity_bit = 0;
        }
    }
    else if (s->bitpos <= s->data_bits)
    {
        s->byte_in_progress >>= 1;
        if (bit)
            s->byte_in_progress |= 0x80;
        s->parity_bit ^= bit;
        s->bitpos++;
    }
    else if (s->parity  &&  s->bitpos == s->data_bits + 1)
    {
        if (s->parity == ASYNC_PARITY_ODD)
            s->parity_bit ^= 1;

        if (s->parity_bit != bit)
            s->parity_errors++;
        s->bitpos++;
    }
    else
    {
        /* Stop bit */
        if (bit == 1)
        {
            /* Align the received value */
            if (s->data_bits < 8)
                s->byte_in_progress >>= (8 - s->data_bits);
            s->put_byte(s->user_data, s->byte_in_progress);
            s->bitpos = 0;
        }
        else
        {
            if (s->use_v14)
            {
                /* This is actually the start bit for the next character, and
                   the stop bit has been dropped from the stream. This is the
                   rate adaption specified in V.14 */
                /* Align the received value */
                if (s->data_bits < 8)
                    s->byte_in_progress >>= (8 - s->data_bits);
                s->put_byte(s->user_data, s->byte_in_progress);
                s->bitpos = 1;
            }
            else
            {
                if (bit != 1)
                    s->framing_errors++;
                s->bitpos = 0;
            }
        }
        s->parity_bit = 0;
        s->byte_in_progress = 0;
    }
}
/*- End of function --------------------------------------------------------*/

void async_tx_init(async_tx_state_t *s,
                   int data_bits,
                   int parity,
                   int stop_bits,
                   int use_v14,
                   get_byte_func_t get_byte,
                   void *user_data)
{
    s->data_bits = data_bits;
    s->parity = parity;
    s->stop_bits = stop_bits;
    if (parity != ASYNC_PARITY_NONE)
        s->stop_bits++;
        
    s->get_byte = get_byte;
    s->user_data = user_data;

    s->byte_in_progress = 0;
    s->bitpos = 0;
    s->parity_bit = 0;
}
/*- End of function --------------------------------------------------------*/

int async_tx_bit(void *user_data)
{
    async_tx_state_t *s;
    int bit;
    
    s = (async_tx_state_t *) user_data;
    if (s->bitpos == 0)
    {
        /* Start bit */
        bit = 0;
        s->byte_in_progress = s->get_byte(s->user_data);
        s->parity_bit = 0;
        s->bitpos++;
    }
    else if (s->bitpos <= s->data_bits)
    {
        bit = s->byte_in_progress & 1;
        s->parity_bit ^= bit;
        s->byte_in_progress >>= 1;
        s->bitpos++;
    }
    else if (s->parity  &&  s->bitpos == s->data_bits + 1)
    {
        if (s->parity == ASYNC_PARITY_ODD)
            s->parity_bit ^= 1;
        bit = s->parity_bit;
        s->bitpos++;
    }
    else
    {
        /* Stop bit(s) */
        bit = 1;
        s->bitpos++;
        if (s->bitpos > s->data_bits + s->stop_bits)
            s->bitpos = 0;
    }
    return bit;
}
/*- End of function --------------------------------------------------------*/

void fsk_tx_init(fsk_tx_state_t *s,
                 fsk_spec_t *spec,
                 get_bit_func_t get_bit,
                 void *user_data)
{
    s->baud_rate = spec->baud_rate;
    s->get_bit = get_bit;
    s->user_data = user_data;

    s->phase_rates[0] = dds_phase_step(spec->freq_zero);
    s->phase_rates[1] = dds_phase_step(spec->freq_one);
    s->scaling = dds_scaling(spec->tx_level);
    /* Initialise fractional sample baud generation. */
    s->phase_acc = 0;
    s->baud_inc = (s->baud_rate*0x10000)/SAMPLE_RATE;
    s->baud_frac = 0;
    s->current_phase_rate = s->phase_rates[1];
    
    s->shutdown = FALSE;
}
/*- End of function --------------------------------------------------------*/

int fsk_tx(fsk_tx_state_t *s, int16_t *amp, int len)
{
    int sample;
    int bit;

    if (s->shutdown)
        return 0;
    /* Make the transitions between 0 and 1 phase coherent, but instantaneous
       jumps. There is currently no interpolation for bauds that end mid-sample.
       Mainstream users will not care. Some specialist users might have a problem
       with they, if they care about accurate transition timing. */
    for (sample = 0;  sample < len;  sample++)
    {
        if ((s->baud_frac += s->baud_inc) >= 0x10000)
        {
            s->baud_frac -= 0x10000;
            bit = s->get_bit(s->user_data);
            if (bit == 2)
            {
                s->shutdown = TRUE;
                break;
            }
            s->current_phase_rate = s->phase_rates[bit & 1];
        }
        amp[sample] = dds_mod(&(s->phase_acc), s->current_phase_rate, s->scaling, 0);
    }
    return sample;
}
/*- End of function --------------------------------------------------------*/

void fsk_tx_power(fsk_tx_state_t *s, float power)
{
    s->scaling = dds_scaling(power);
}
/*- End of function --------------------------------------------------------*/

void fsk_rx_init(fsk_rx_state_t *s,
                 fsk_spec_t *spec,
                 int sync_mode,
                 put_bit_func_t put_bit,
                 void *user_data)
{
    int chop;

    memset(s, 0, sizeof(*s));
    s->baud_rate = spec->baud_rate;
    s->sync_mode = sync_mode;
    s->min_power = power_meter_level(spec->min_level);
    s->put_bit = put_bit;
    s->user_data = user_data;

    /* Detect by correlating against the tones we want, over a period
       of one baud. The correlation must be quadrature. */
    
    /* First we need the quadrature tone generators to correlate
       against. */
    s->phase_rate[0] = dds_phase_step(spec->freq_zero);
    s->phase_rate[1] = dds_phase_step(spec->freq_one);
    s->phase_acc[0] = 0;
    s->phase_acc[1] = 0;

    /* The correlation should be over one baud. */
    s->correlation_span = SAMPLE_RATE/spec->baud_rate;
    /* But limit it for very slow baud rates, so we do not overflow our
       buffer. */
    if (s->correlation_span > FSK_MAX_WINDOW_LEN)
        s->correlation_span = FSK_MAX_WINDOW_LEN;

    /* We need to scale, to avoid overflow in the correlation. */
    s->scaling_shift = 0;
    chop = s->correlation_span;
    while (chop != 0)
    {
        s->scaling_shift++;
        chop >>= 1;
    }

    /* Initialise the baud/bit rate tracking. */
    s->baud_inc = (s->baud_rate*0x10000)/SAMPLE_RATE;
    s->baud_pll = 0;
    
    /* Initialise a power detector, so sense when a signal is present. */
    power_meter_init(&(s->power), 4);
    s->carrier_present = FALSE;
}
/*- End of function --------------------------------------------------------*/

int fsk_rx(fsk_rx_state_t *s, const int16_t *amp, int len)
{
    int buf_ptr;
    int baudstate;
    int sample;
    int j;
    int32_t dot;
    int32_t sum;
    icomplex_t ph;

    buf_ptr = s->buf_ptr;

    for (sample = 0;  sample < len;  sample++)
    {
        /* If there isn't much signal, don't demodulate - it will only produce
           useless junk results. */
        /* TODO: The carrier signal has no hysteresis! */
        if (power_meter_update(&(s->power), amp[sample]) < s->min_power)
        {
            if (s->carrier_present)
            {
                s->put_bit(s->user_data, PUTBIT_CARRIER_DOWN);
                s->carrier_present = FALSE;
            }
            continue;
        }
        if (!s->carrier_present)
        {
            s->put_bit(s->user_data, PUTBIT_CARRIER_UP);
            s->carrier_present = TRUE;
        }
        /* Non-coherent FSK demodulation by correlation with the target tones
           over a one baud interval. The slow V.xx specs. are too open ended
           to allow anything fancier to be used. The dot products are calculated
           using a sliding window approach, so the compute load is not that great. */
        /* The *totally* asynchronous character to character behaviour of these
           modems, when carrying async. data, seems to force a sample by sample
           approach. */
        for (j = 0;  j < 2;  j++)
        {
            s->dot_i[j] -= s->window_i[j][buf_ptr];
            s->dot_q[j] -= s->window_q[j][buf_ptr];

            ph = dds_complex(&(s->phase_acc[j]), s->phase_rate[j]);
            s->window_i[j][buf_ptr] = (ph.re*amp[sample]) >> s->scaling_shift;
            s->window_q[j][buf_ptr] = (ph.im*amp[sample]) >> s->scaling_shift;

            s->dot_i[j] += s->window_i[j][buf_ptr];
            s->dot_q[j] += s->window_q[j][buf_ptr];
        }
        dot = s->dot_i[0] >> 15;
        sum = dot*dot;
        dot = s->dot_q[0] >> 15;
        sum += dot*dot;
        dot = s->dot_i[1] >> 15;
        sum -= dot*dot;
        dot = s->dot_q[1] >> 15;
        sum -= dot*dot;
        baudstate = (sum < 0);

        if (s->lastbit != baudstate)
        {
            s->lastbit = baudstate;
            if (s->sync_mode)
            {
                /* For synchronous use (e.g. HDLC channels in FAX modems), nudge
                   the baud phase gently, trying to keep it centred on the bauds. */
                if (s->baud_pll < 0x8000)
                    s->baud_pll += (s->baud_inc >> 3);
                else
                    s->baud_pll -= (s->baud_inc >> 3);
            }
            else
            {
                /* For async. operation, believe transitions completely, and
                   sample appropriately. This allows instant start on the first
                   transition. */
                /* We must now be about half way to a sampling point. We do not do
                   any fractional sample estimation of the transitions, so this is
                   the most accurate baud alignment we can do. */
                s->baud_pll = 0x8000;
            }

        }
        if ((s->baud_pll += s->baud_inc) >= 0x10000)
        {
            /* We should be in the middle of a baud now, so report the current
               state as the next bit */
            s->baud_pll -= 0x10000;
            s->put_bit(s->user_data, baudstate);
        }
        if (++buf_ptr >= s->correlation_span)
            buf_ptr = 0;
    }
    s->buf_ptr = buf_ptr;
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

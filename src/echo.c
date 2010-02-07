/*
 * SpanDSP - a series of DSP components for telephony
 *
 * echo.c - An echo cancellor, suitable for electrical and acoustic
 *          cancellation. This code does not currently comply with
 *          any relevant standards (e.g. G.164/5/7/8). One day....
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001, 2003 Steve Underwood
 *
 * Based on a bit from here, a bit from there, eye of toad,
 * ear of bat, etc - plus, of course, my own 2 cents.
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
 * $Id: echo.c,v 1.6 2005/03/03 14:18:58 steveu Exp $
 */

/*! \file */

/* TODO:
   Finish the echo suppressor option, however nasty suppression may be
   Add an option to reintroduce side tone at -24dB under appropriate conditions.
   Improve double talk detector (iterative!)
*/

/* We need to differentiate between transmitted energy which will train the echo
   canceller well (voice, white noise, and other broadband sources) and energy
   which will train it badly (supervisory tones, DTMF, whistles, and other
   narrowband sources). There are many ways this might be done. This canceller uses
   a method based on the autocorrelation qualities of the transmitted signal. A rather
   peaky autocorrelation function is a clear sign of a narrowband signal. We only need
   perform the autocorrelation at well spaced intervals, so the compute load is not too
   great. Multiple successive autocorrelation functions with a similar peaky shape are a
   clear indication of a stationary narrowband signal. */

/* The FIR taps must be adapted as 32 bit values, to get the necessary finesse
   in the adaption process. However, they are applied as 16 bit values (bits 30-15
   of the 32 bit values) in the FIR. For the working 16 bit values, we need 4 sets.
   
   3 of the 16 bit sets are used on a rotating basis. Normally the canceller steps
   round these 3 sets at regular intervals. Any time we detect double talk, we can go
   back to the set from two steps ago with reasonable assurance it is a well adapted
   set. We cannot just go back one step, as we may have rotated the sets just before
   double talk or tone was detected, and that set may already be somewhat corrupted.
   
   When narrowband energy is detected we need to continue adapting to it, to echo
   cancel it. However, the adaption will almost certainly be going astray. Broadband
   (or even complex sequences of narrowband) energy will normally lead to a well
   trained cancellor, with taps matching the impulse response of the channel.
   For stationary narrowband energy, there is usually has an infinite number of
   alternative tap sets which will cancel it well. A previously well trained set of
   taps will tend to drift amongst the alternatives. When broadband energy resumes, the
   taps may be a total mismatch for the signal, and could even amplify rather than
   attenuate the echo. The solution is to use a fourth set of 16 bit taps. When we first
   detect the narrowband energy we save the oldest of the group of three sets, but do
   not change back to an older set. We let the canceller cancel, and it adaption drift
   while the narrowband energy is present. When we detect the narrowband energy has ceased,
   we switch to using the fourth set of taps which was saved.

   When we revert to an older set of taps, we must replace both the 16 bit and 32 bit
   working tap sets. The saved 16 bit values are good enough to also be used as a replacement
   for the 32 bit values. */

#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "spandsp/alaw_ulaw.h"
#include "spandsp/echo.h"
#include "spandsp/mmx.h"

#if !defined(NULL)
#define NULL (void *) 0
#endif
#define FALSE 0
#define TRUE (!FALSE)

#define MIN_TX_POWER_FOR_ADAPTION   64*64
#define MIN_RX_POWER_FOR_ADAPTION   64*64

static void autocorrelation(int16_t s[], int len, int32_t acf[], int alen)
{
    register int k;
    register int i;
    register float *sf;
    register float *sfl;
    register float temp;
    float scale;
    float s_f[128];
    float f_acf[128];

    sf = s_f;
    for (i = 0;  i < len;  i++)
        sf[i] = s[i];
    for (k = 0;  k < alen;  k++)
    {
        sfl = sf - k;
        temp = 0;
        for (i = k;  i < len;  i++)
            temp += sf[i]*sfl[i];
        f_acf[k] = temp;
    }
    scale = 0x1FFFFFFF/f_acf[0];
    for (k = 0;  k < alen;  k++)
        acf[k] = f_acf[k]*scale;
}

#if 0
void reflection_coefficients(int32_t acf[], int16_t r[]);

void reflection_coefficients(int32_t acf[], int16_t r[])
{
    int i;
    int m;
    int n;
    register int32_t temp;
    int xacf[9];
    int p[9];
    int k[9];

    /* Schur recursion with 16 bit arithmetic. */
    if (acf[0] == 0)
    {
        for (i = 8;  i--;  *r++ = 0)
            ;
        return;
    }

    temp = gsm_norm(acf[0]);

    for (i = 0;  i <= 8;  i++)
        xacf[i] = acf[i] >> (16 - temp);

    for (i = 1;  i <= 7;  i++)
        k[i] = xacf[i];
    for (i = 0;  i <= 8;  i++)
        p[i] = xacf[i];

    /* Compute the reflection coefficients */
    for (n = 1;  n <= 8;  n++, r++)
    {
	temp = abs(p[1]);
	if (p[0] < temp)
        {
	    for (i = n;  i <= 8;  i++)
                *r++ = 0;
	    return;
	}

	*r = temp/p[0];

	if (p[1] > 0)
            *r = -*r;

	if (n == 8)
            return; 

	/* Schur recursion */
	p[0] += p[1]*(*r);

	for (m = 1;  m <= 8 - n;  m++)
        {
	    p[m] = p[m + 1] + k[m]*(*r);
	    k[m] += p[m + 1]*(*r);
	}
    }
}
#endif

static inline void lms_adapt(echo_can_state_t *ec, int factor)
{
    int i;

#if 0
    mmx_t *mmx_taps;
    mmx_t *mmx_coeffs;
    mmx_t *mmx_hist;
    mmx_t mmx;

    mmx.w[0] =
    mmx.w[1] =
    mmx.w[2] =
    mmx.w[3] = factor;
    mmx_hist = (mmx_t *) &fir->history[fir->curr_pos];
    mmx_taps = (mmx_t *) &fir->taps;
    mmx_coeffs = (mmx_t *) fir->coeffs;
    i = fir->taps;
    movq_m2r(mmx, mm0);
    while (i > 0)
    {
        movq_m2r(mmx_hist[0], mm1);
        movq_m2r(mmx_taps[0], mm0);
        movq_m2r(mmx_taps[1], mm1);
        movq_r2r(mm1, mm2);
        pmulhw(mm0, mm1);
        pmullw(mm0, mm2);

        pmaddwd_r2r(mm1, mm0);
        pmaddwd_r2r(mm3, mm2);
        paddd_r2r(mm0, mm4);
        paddd_r2r(mm2, mm4);
        movq_r2m(mm0, mmx_taps[0]);
        movq_r2m(mm1, mmx_taps[0]);
        movq_r2m(mm2, mmx_coeffs[0]);
        mmx_taps += 2;
        mmx_coeffs += 1;
        mmx_hist += 1;
        i -= 4;
    )
    emms();
#elif 1
    /* Update the FIR taps */
    for (i = ec->taps - 1;  i >= 0;  i--)
    {
        /* Leak to avoid the coefficients drifting beyond the ability of the
           adaption process to bring them back under control. */
        ec->fir_taps32[i] -= (ec->fir_taps32[i] >> 23);
        ec->fir_taps32[i] += (ec->fir_state.history[i + ec->curr_pos]*factor);
        ec->latest_correction = (ec->fir_state.history[i + ec->curr_pos]*factor);
        //ec->fir_state.coeffs[i] = ec->fir_taps32[i] >> 15;
    }
#else
    int offset1;
    int offset2;

    /* Update the FIR taps */
    offset2 = ec->curr_pos;
    offset1 = ec->taps - offset2;
    for (i = ec->taps - 1;  i >= offset1;  i--)
    {
        ec->fir_taps32[i] += (ec->fir_state.history[i - offset1]*factor);
        ec->fir_state.coeffs[i] = ec->fir_taps32[i] >> 15;
    }
    for (  ;  i >= 0;  i--)
    {
        ec->fir_taps32[i] += (ec->fir_state.history[i + offset2]*factor);
        ec->fir_state.coeffs[i] = ec->fir_taps32[i] >> 15;
    }
#endif
}
/*- End of function --------------------------------------------------------*/

echo_can_state_t *echo_can_create(int len, int adaption_mode)
{
    echo_can_state_t *ec;
    int i;
    int j;

    ec = (echo_can_state_t *) malloc(sizeof(*ec));
    if (ec == NULL)
        return  NULL;
    memset(ec, 0, sizeof(*ec));
    ec->taps = len;
    ec->curr_pos = ec->taps - 1;
    ec->tap_mask = ec->taps - 1;
    if ((ec->fir_taps32 = (int32_t *) malloc(ec->taps*sizeof(int32_t))) == NULL)
    {
        free(ec);
        return  NULL;
    }
    memset(ec->fir_taps32, 0, ec->taps*sizeof(int32_t));
    for (i = 0;  i < 4;  i++)
    {
        if ((ec->fir_taps16[i] = (int16_t *) malloc(ec->taps*sizeof(int16_t))) == NULL)
        {
            for (j = 0;  j < i;  j++)
                free(ec->fir_taps16[j]);
            free(ec->fir_taps32);
            free(ec);
            return  NULL;
        }
        memset(ec->fir_taps16[i], 0, ec->taps*sizeof(int16_t));
    }
    fir16_create(&ec->fir_state,
                 ec->fir_taps16[0],
                 ec->taps);
    ec->rx_power_threshold = 10000000;
    ec->adaption_mode = adaption_mode;
    ec->geigel_max = 0;
    ec->geigel_lag = 0;
    ec->dtd_onset = FALSE;
    ec->tap_set = 0;
    ec->tap_rotate_counter = 1600;
    ec->cng_level = 1000;
    return  ec;
}
/*- End of function --------------------------------------------------------*/

void echo_can_free(echo_can_state_t *ec)
{
    fir16_free(&ec->fir_state);
    free(ec->fir_taps32);
    free(ec->fir_taps16[0]);
    free(ec->fir_taps16[1]);
    free(ec->fir_taps16[2]);
    free(ec->fir_taps16[3]);
    free(ec);
}
/*- End of function --------------------------------------------------------*/

void echo_can_adaption_mode(echo_can_state_t *ec, int adaption_mode)
{
    ec->adaption_mode = adaption_mode;
}
/*- End of function --------------------------------------------------------*/

void echo_can_flush(echo_can_state_t *ec)
{
    int i;

    ec->tx_power[3] = 0;
    ec->tx_power[2] = 0;
    ec->tx_power[1] = 0;
    ec->tx_power[0] = 0;
    ec->rx_power[2] = 0;
    ec->rx_power[1] = 0;
    ec->rx_power[0] = 0;
    ec->clean_rx_power = 0;
    ec->nonupdate_dwell = 0;

    fir16_flush(&ec->fir_state);
    ec->fir_state.curr_pos = ec->taps - 1;
    memset(ec->fir_taps32, 0, ec->taps*sizeof(int32_t));
    for (i = 0;  i < 4;  i++)
        memset(ec->fir_taps16[i], 0, ec->taps*sizeof(int16_t));

    ec->curr_pos = ec->taps - 1;

    ec->supp_test1 = 0;
    ec->supp_test2 = 0;
    ec->supp1 = 0;
    ec->supp2 = 0;
    ec->vad = 0;
    ec->cng_level = 1000;
    ec->cng_filter = 0;

    ec->geigel_max = 0;
    ec->geigel_lag = 0;
    ec->dtd_onset = FALSE;
    ec->tap_set = 0;
    ec->tap_rotate_counter = 1600;

    ec->latest_correction = 0;

    memset(ec->last_acf, 0, sizeof(ec->last_acf));
    memset(ec->acf, 0, sizeof(ec->acf));
    ec->acf_count = 0;
    ec->narrowband_score = 0;
}
/*- End of function --------------------------------------------------------*/

int sample_no = 0;

int16_t echo_can_update(echo_can_state_t *ec, int16_t tx, int16_t rx)
{
    int32_t echo_value;
    int clean_rx;
    int nsuppr;
    int i;

sample_no++;
    ec->latest_correction = 0;
    /* Evaluate the echo - i.e. apply the FIR filter */
    /* Assume the gain of the FIR does not exceed unity. Exceeding unity
       would seem like a rather poor thing for an echo cancellor to do :)
       This means we can compute the result with a total disregard for
       overflows. 16bits x 16bits -> 31bits, so no overflow can occur in
       any multiply. While accumulating we may overflow and underflow the
       32 bit scale often. However, if the gain does not exceed unity,
       everything should work itself out, and the final result will be
       OK, without any saturation logic. */
    /* Overflow is very much possible here, and we do nothing about it because
       of the compute costs */
    /* 16 bit coeffs for the LMS give lousy results (maths good, actual sound
       bad!), but 32 bit coeffs require some shifting. On balance 32 bit seems
       best */
    echo_value = fir16(&ec->fir_state, tx);

    /* And the answer is..... */
    clean_rx = rx - echo_value;

    /* That was the easy part. Now we need to adapt! */
    if (ec->nonupdate_dwell > 0)
        ec->nonupdate_dwell--;

    /* Calculate short term power levels using very simple single pole IIRs */
    /* TODO: Is the nasty modulus approach the fastest, or would a real
             tx*tx power calculation actually be faster? Using the squares
             makes the numbers grow a lot! */
    ec->tx_power[3] += ((abs(tx) - ec->tx_power[3]) >> 5);
    ec->tx_power[2] += ((tx*tx - ec->tx_power[2]) >> 8);
    ec->tx_power[1] += ((tx*tx - ec->tx_power[1]) >> 5);
    ec->tx_power[0] += ((tx*tx - ec->tx_power[0]) >> 3);
    ec->rx_power[1] += ((rx*rx - ec->rx_power[1]) >> 6);
    ec->rx_power[0] += ((rx*rx - ec->rx_power[0]) >> 3);
    ec->clean_rx_power += ((clean_rx*clean_rx - ec->clean_rx_power) >> 6);

    /* If there is very little being transmitted, any attempt to train is
       futile. We would either be training on the far end's noise or signal,
       the channel's own noise, or our noise. Either way, this is hardly good
       training, so don't do it (avoid trouble). */
    if (ec->tx_power[0] > MIN_TX_POWER_FOR_ADAPTION)
    {
        /* If the received power is very low, either we are sending very little or
           we are already well adapted. There is little point in trying to improve
           the adaption under these circumstances, so don't do it (reduce the
           compute load). */
        if (ec->tx_power[1] > ec->rx_power[0])
        {
            /* There is no (or little) far-end speech. */
            if (ec->nonupdate_dwell == 0)
            {
                if ((ec->acf_count++)%160 == 0)
                {
                    autocorrelation(&ec->fir_state.history[ec->curr_pos], 64, ec->acf, 9);
                    {
                        int i;
                        int score;

                        score = 0;
                        for (i = 0;  i < 9;  i++)
                        {
                            if (ec->last_acf[i] >= 0  &&  ec->acf[i] >= 0)
                            {
                                if ((ec->last_acf[i] >> 1) < ec->acf[i]  &&  ec->acf[i] < (ec->last_acf[i] << 1))
                                    score++;
                            }
                            else if (ec->last_acf[i] < 0  &&  ec->acf[i] < 0)
                            {
                                if ((ec->last_acf[i] >> 1) > ec->acf[i]  &&  ec->acf[i] > (ec->last_acf[i] << 1))
                                    score++;
                            }
                        }
                        //for (i = 0;  i < 9;  i++)
                        //    printf("%12d ", ec->acf[i]);
                        //printf("%12d\n", score);
                        if (score > 6)
                        {
                            if (ec->narrowband_score == 0)
                                memcpy(ec->fir_taps16[3], ec->fir_taps16[(ec->tap_set + 1)%3], ec->taps*sizeof(int16_t));
                            ec->narrowband_score += score;
                        }
                        else
                        {
                            if (ec->narrowband_score > 200)
                            {
//printf("Revert to %d\n", (ec->tap_set + 1)%3);
                                memcpy(ec->fir_taps16[ec->tap_set], ec->fir_taps16[3], ec->taps*sizeof(int16_t));
                                memcpy(ec->fir_taps16[(ec->tap_set - 1)%3], ec->fir_taps16[3], ec->taps*sizeof(int16_t));
                                for (i = 0;  i < ec->taps;  i++)
                                    ec->fir_taps32[i] = ec->fir_taps16[3][i] << 15;
                                ec->tap_rotate_counter = 1600;
                            }
                            ec->narrowband_score = 0;
                        }
                        memcpy(ec->last_acf, ec->acf, sizeof(ec->acf));
                    }
                }
                ec->dtd_onset = FALSE;
                if (--ec->tap_rotate_counter <= 0)
                {
//printf("Rotate to %d\n", ec->tap_set);
                    ec->tap_rotate_counter = 1600;
                    ec->tap_set++;
                    if (ec->tap_set > 2)
                        ec->tap_set = 0;
                    ec->fir_state.coeffs = ec->fir_taps16[ec->tap_set];
                }
                /* ... and we are not in the dwell time from previous speech. */
                if (!(ec->adaption_mode & ECHO_CAN_FREEZE_ADAPTION))
                {
                    //nsuppr = saturate((clean_rx << 16)/ec->tx_power[1]);
                    //nsuppr = clean_rx/ec->tx_power[1];
                    /* If a sudden surge in signal level (e.g. the onset of a tone
                       burst) cause an abnormally high instantaneous to average
                       signal power ratio, we could kick the adaption badly in the
                       wrong direction. This is because the tx_power takes too long
                       to react and rise. We need to stop too rapid adaption to the
                       new signal. We normalise to a value derived from the
                       instantaneous signal if it exceeds the peak by too much. */
                    nsuppr = clean_rx;
                    /* Divide isn't very quick, but the "where is the top bit" and shift
                       instructions are single cycle. */
                    if (tx > 4*ec->tx_power[3])
                        i = top_bit(tx) - 8;
                    else
                        i = top_bit(ec->tx_power[3]) - 8;
                    if (i > 0)
                        nsuppr >>= i;
                    lms_adapt(ec, nsuppr);
                }
            }
            //printf("%10d %10d %10d %10d %10d\n", rx, clean_rx, nsuppr, ec->tx_power[1], ec->rx_power[1]);
            //printf("%.4f\n", (float) ec->rx_power[1]/(float) ec->clean_rx_power);
        }
        else
        {
            if (!ec->dtd_onset)
            {
//printf("Revert to %d\n", (ec->tap_set + 1)%3);
                memcpy(ec->fir_taps16[ec->tap_set], ec->fir_taps16[(ec->tap_set + 1)%3], ec->taps*sizeof(int16_t));
                memcpy(ec->fir_taps16[(ec->tap_set - 1)%3], ec->fir_taps16[(ec->tap_set + 1)%3], ec->taps*sizeof(int16_t));
                for (i = 0;  i < ec->taps;  i++)
                    ec->fir_taps32[i] = ec->fir_taps16[(ec->tap_set + 1)%3][i] << 15;
                ec->tap_rotate_counter = 1600;
                ec->dtd_onset = TRUE;
            }
            ec->nonupdate_dwell = NONUPDATE_DWELL_TIME;
        }
    }

    if (ec->rx_power[1])
        ec->vad = (8000*ec->clean_rx_power)/ec->rx_power[1];
    else
        ec->vad = 0;
    if (ec->rx_power[1] > 2048*2048  &&  ec->clean_rx_power > 4*ec->rx_power[1])
    {
        /* The EC seems to be making things worse, instead of better. Zap it! */
        memset(ec->fir_taps32, 0, ec->taps*sizeof(int32_t));
        for (i = 0;  i < 4;  i++)
            memset(ec->fir_taps16[i], 0, ec->taps*sizeof(int16_t));
    }

#if defined(XYZZY)
    if ((ec->adaption_mode & ECHO_CAN_USE_SUPPRESSOR))
    {
        ec->supp_test1 += (ec->fir_state.history[ec->curr_pos] - ec->fir_state.history[(ec->curr_pos - 7) & ec->tap_mask]);
        ec->supp_test2 += (ec->fir_state.history[(ec->curr_pos - 24) & ec->tap_mask] - ec->fir_state.history[(ec->curr_pos - 31) & ec->tap_mask]);
        if (ec->supp_test1 > 42  &&  ec->supp_test2 > 42)
            supp_change = 25;
        else
            supp_change = 50;
        supp = supp_change + k1*ec->supp1 + k2*ec->supp2;
        ec->supp2 = ec->supp1;
        ec->supp1 = supp;
        clean_rx *= (1 - supp);
    }
#endif

    if ((ec->adaption_mode & ECHO_CAN_USE_NLP))
    {
        /* Non-linear processor - a fancy way to say "zap small signals, to avoid
           residual echo due to (uLaw/ALaw) non-linearity in the channel.". */
        if (ec->rx_power[1] < 30000000)
        {
            if (!ec->cng)
            {
                ec->cng_level = ec->clean_rx_power;
                ec->cng = TRUE;
            }
            if ((ec->adaption_mode & ECHO_CAN_USE_CNG))
            {
                /* Very elementary comfort noise generation */
                /* Just random numbers rolled off very vaguely Hoth-like */
                ec->cng_filter += ((int16_t) (rand() & 0xFFFF) - ec->cng_filter) >> 2;
                clean_rx = (ec->cng_filter*ec->cng_level) >> 17;
                /* TODO: A better CNG, with more accurate (tracking) spectral shaping! */
            }
            else
            {
                clean_rx = 0;
            }
//clean_rx = -16000;
        }
        else
        {
            ec->cng = FALSE;
        }
    }
    else
    {
        ec->cng = FALSE;
    }

    /* Roll around the rolling buffer */
    if (ec->curr_pos <= 0)
        ec->curr_pos = ec->taps;
    ec->curr_pos--;
    return  clean_rx;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

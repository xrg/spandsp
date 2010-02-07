/*
 * SpanDSP - a series of DSP components for telephony
 *
 * echo_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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
 * $Id: echo_tests.c,v 1.8 2005/09/01 17:06:45 steveu Exp $
 */

/*! \page echo_can_tests_page Echo cancellation tests

\section echo_can_tests_page_sec_1 What does it do?
Currently the echo cancellation tests only provide simple exercising of the
cancellor in the way it might be used for line echo cancellation. The test code
is in echotests.c. 

The goal is to test the echo cancellor again the G.16X specs. Clearly, that also
means the goal for the cancellor itself is to comply with those specs. Right
now, the only aspect of these tests implemented is the line impulse response
models in g168tests.c. 

\section echo_can_tests_page_sec_2 How does it work?
The current test consists of feeding a wave file of real speech to the echo
cancellor as the transmit signal. A very simple model of a telephone line is
used to simulate a simple echo from the transmit signal. A second wave file of
real speech is also used to simulate a signal received form the far end of the
line. This is gated so it is only placed for one second every 10 seconds,
simulating the double talk condition. The resulting echo cancelled signal can
either be store in a file for further analysis, or played back as the data is
processed. 

A number of modified versions of this test have been performed. The signal level
of the two speech sources has been varied. Several simple models of the
telephone line have been used. Although the current cancellor design has known
limitations, it seems stable for all these test conditions. No instability has
been observed in the current version due to arithmetic overflow when the speech
is very loud (with earlier versions, well, ....:) ). The lack of saturating
arithmetic in general purpose CPUs is a huge disadvantage here, as software
saturation logic would cause a major slow down. Floating point would be good,
but is not usable in the Linux kernel. Anyway, the bottom line seems to be the
current design is genuinely useful, if imperfect. 

\section echo_can_tests_page_sec_2 How do I use it?

Build the tests with the command "./build". Currently there is no proper make
setup, or way to build individual tests. "./build" will built all the tests
which currently exist for the DSP functions. The echo cancellation test assumes
there are two wave files containing mono, 16 bit signed PCM speech data, sampled
at 8kHz. These should be called local_sound.wav and far_sound.wav. A third wave
file will be produced. This very crudely starts with the first 256 bytes from
the local_sound.wav file, followed by the results of the echo cancellation. The
resulting audio is also played to the /dev/dsp device. A printf near the end of
echo_tests.c is commented out with a #if. If this is enabled, detailed
information about the results of the echo cancellation will be written to
stdout. By saving this into a file, Grace (recommended), GnuPlot, or some other
plotting package may be used to graphically display the functioning of the
cancellor.  
*/

#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <audiofile.h>
#include <tiffio.h>

#define GEN_CONST
#include <math.h>

#include "spandsp.h"
#include "spandsp/g168models.h"

//#define PERFORM_TEST_2A
//#define PERFORM_TEST_2B
//#define PERFORM_TEST_2C
//#define PERFORM_TEST_3A
#define PERFORM_TEST_3B
//#define PERFORM_TEST_3C
//#define PERFORM_TEST_4
//#define PERFORM_TEST_5
//#define PERFORM_TEST_6
//#define PERFORM_TEST_7
//#define PERFORM_TEST_8
//#define PERFORM_TEST_9
//#define PERFORM_TEST_10A
//#define PERFORM_TEST_10B
//#define PERFORM_TEST_10C
//#define PERFORM_TEST_11
//#define PERFORM_TEST_12
//#define PERFORM_TEST_13
//#define PERFORM_TEST_14
//#define PERFORM_TEST_15

#if !defined(NULL)
#define NULL (void *) 0
#endif

typedef struct
{
    char *name;
    int max;
    int cur;
    AFfilehandle handle;
    int16_t signal[8000];
} signal_source_t;

typedef struct
{
    int type;
    fir_float_state_t *fir;
    float history[35*8];
    int pos;
    float factor; 
    float power;
} level_measurement_device_t;

signal_source_t local_css;
signal_source_t far_css;

fir32_state_t line_model;

AFfilehandle residuehandle;
int16_t residue_sound[8000];
int residue_cur = 0;

static inline void put_residue(int16_t amp)
{
    int outframes;

    residue_sound[residue_cur++] = amp;
    if (residue_cur >= 8000)
    {
        outframes = afWriteFrames(residuehandle,
                                  AF_DEFAULT_TRACK,
                                  residue_sound,
                                  residue_cur);
        if (outframes != residue_cur)
        {
            fprintf(stderr, "    Error writing residue sound\n");
            exit(2);
        }
        residue_cur = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static void signal_load(signal_source_t *sig, char *name)
{
    float x;

    sig->name = name;
    sig->handle = afOpenFile(sig->name, "r", 0);
    if (sig->handle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open sound file '%s'\n", sig->name);
        exit(2);
    }
    x = afGetFrameSize(sig->handle, AF_DEFAULT_TRACK, 1);
    if (x != 2.0)
    {
        fprintf(stderr, "    Unexpected frame size in sound file '%s'\n", sig->name);
        exit(2);
    }
    sig->max = afReadFrames(sig->handle, AF_DEFAULT_TRACK, sig->signal, 8000);
    if (sig->max < 0)
    {
        fprintf(stderr, "    Error reading sound file '%s'\n", sig->name);
        exit(2);
    }
}
/*- End of function --------------------------------------------------------*/

static void signal_free(signal_source_t *sig)
{
    if (afCloseFile(sig->handle) != 0)
    {
        fprintf(stderr, "    Cannot close sound file '%s'\n", sig->name);
        exit(2);
    }
}
/*- End of function --------------------------------------------------------*/

static void signal_restart(signal_source_t *sig)
{
    sig->cur = 0;
}
/*- End of function --------------------------------------------------------*/

static int16_t signal_amp(signal_source_t *sig)
{
    int16_t tx;

    tx = sig->signal[sig->cur++];
    if (sig->cur >= sig->max)
        sig->cur = 0;
    return tx;
}
/*- End of function --------------------------------------------------------*/

static inline int16_t alaw_munge(int16_t amp)
{
    return alaw_to_linear(linear_to_alaw(amp));
}
/*- End of function --------------------------------------------------------*/

static void channel_model_create(int model)
{
    static const int32_t *line_models[] =
    {
        line_model_d2_coeffs,
        line_model_d3_coeffs,
        line_model_d4_coeffs,
        line_model_d5_coeffs,
        line_model_d6_coeffs,
        line_model_d7_coeffs,
        line_model_d8_coeffs,
        line_model_d9_coeffs
    };

    static int line_model_sizes[] =
    {
        sizeof(line_model_d2_coeffs)/sizeof(int32_t),
        sizeof(line_model_d3_coeffs)/sizeof(int32_t),
        sizeof(line_model_d4_coeffs)/sizeof(int32_t),
        sizeof(line_model_d5_coeffs)/sizeof(int32_t),
        sizeof(line_model_d6_coeffs)/sizeof(int32_t),
        sizeof(line_model_d7_coeffs)/sizeof(int32_t),
        sizeof(line_model_d8_coeffs)/sizeof(int32_t),
        sizeof(line_model_d9_coeffs)/sizeof(int32_t)
    };

    fir32_create(&line_model, line_models[model], line_model_sizes[model]);
}
/*- End of function --------------------------------------------------------*/

static int16_t channel_model(int16_t local, int16_t far)
{
    int16_t echo;
    int16_t rx;

    /* Channel modelling is merely simulating the effects of A-law distortion
       and using one of the echo models from G.168 */

    /* The local tx signal will have gone through an A-law munging before
       it reached the line's analogue area where the echo occurs. */
    echo = fir32(&line_model, alaw_munge(local/8));
    /* The far end signal will have been through an A-law munging, although
       this should not affect things. */
    rx = echo + alaw_munge(far);
    /* This mixed echo and far end signal will have been through an A-law munging when it came back into
       the digital network. */
    rx = alaw_munge(rx);
    return  rx;
}
/*- End of function --------------------------------------------------------*/

static level_measurement_device_t *level_measurement_device_create(int type)
{
    level_measurement_device_t *dev;
    int i;

    dev = (level_measurement_device_t *) malloc(sizeof(level_measurement_device_t));
    dev->fir = (fir_float_state_t *) malloc(sizeof(fir_float_state_t));
    fir_float_create(dev->fir,
                     level_measurement_bp_coeffs,
                     sizeof(level_measurement_bp_coeffs)/sizeof(float));
    for (i = 0;  i < 35*8;  i++)
        dev->history[i] = 0.0;
    dev->pos = 0;
    dev->factor = exp(-1.0/((float) SAMPLE_RATE*0.035));
    dev->power = 0;
    dev->type = type;
    return  dev;
}
/*- End of function --------------------------------------------------------*/

static float level_measurement_device(level_measurement_device_t *dev, int16_t amp)
{
    float signal;

    signal = fir_float(dev->fir, amp);
    signal *= signal;
    if (dev->type == 0)
    {
        dev->power = dev->power*dev->factor + signal*(1.0 - dev->factor);
        signal = sqrt(dev->power);
    }
    else
    {
        dev->power -= dev->history[dev->pos];
        dev->power += signal;
        dev->history[dev->pos++] = signal;
        signal = sqrt(dev->power/(35.8*8.0));
    }
    return signal;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    echo_can_state_t *ctx;
    awgn_state_t local_noise_source;
    awgn_state_t far_noise_source;
    int i;
    int clean;
    int16_t rx;
    int16_t tx;
#if 0
    int j;
    int k;
    tone_gen_descriptor_t tone_desc;
    tone_gen_state_t tone_state;
    int16_t noise;
    int16_t local_sound[40000];
    int local_max;
#endif
    int local_cur;
    //int16_t far_sound[8000];
    //int16_t result_sound[64000];
    //int32_t coeffs[200][128];
    //int coeff_index;
    int far_cur;
    int result_cur;
    //int far_tx;
    AFfilehandle resulthandle;
    AFfilesetup filesetup;
    AFfilesetup filesetup2;
    //int outframes;
    time_t now;
    int tone_burst_step;
    level_measurement_device_t *power_meter_1;
    level_measurement_device_t *power_meter_2;

    time(&now);
    tone_burst_step = 0;
    ctx = echo_can_create(256, 0);
    awgn_init(&far_noise_source, 7162534, -50);

    signal_load(&local_css, "sound_c1_8k.wav");
    signal_load(&far_css, "sound_c3_8k.wav");

    filesetup = afNewFileSetup();
    if (filesetup == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 6);

    filesetup2 = afNewFileSetup();
    if (filesetup2 == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup2, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup2, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup2, AF_FILE_WAVE);
    afInitChannels(filesetup2, AF_DEFAULT_TRACK, 1);

    resulthandle = afOpenFile("result_sound.wav", "w", filesetup);
    if (resulthandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Failed to open result file\n");
        exit(2);
    }

    residuehandle = afOpenFile("residue_sound.wav", "w", filesetup2);
    if (residuehandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Failed to open residue file\n");
        exit(2);
    }
    local_cur = 0;
    far_cur = 0;
    result_cur = 0;
    channel_model_create(0);
    power_meter_1 = level_measurement_device_create(0);
    power_meter_2 = level_measurement_device_create(0);

    level_measurement_device(power_meter_1, 0);
    
    
#if 0
    echo_can_flush(ctx);
    /* Converge the canceller */
    signal_restart(&local_css);
    for (i = 0;  i < 800*2;  i++)
    {
        clean = echo_can_update(ctx, 0, 0);
        put_residue(clean);
    }
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    echo_can_adaption_mode(ctx, ECHO_CAN_USE_NLP | ECHO_CAN_USE_CNG);
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    echo_can_adaption_mode(ctx, 0);
    
    for (i = 0;  i < 8000*10;  i++)
    {
        tx = signal_amp(&local_css);
#if 0
        if ((i/10000)%10 == 9)
        {
            /* Inject a burst of far sound */
            if (far_cur >= far_max)
            {
                far_max = afReadFrames(farhandle, AF_DEFAULT_TRACK, far_sound, 8000);
                if (far_max < 0)
                {
                    fprintf(stderr, "    Error reading far sound\n");
                    exit(2);
                }
                if (far_max == 0)
                    break;
                far_cur = 0;
            }
            far_tx = far_sound[far_cur++];
        }
        else
        {
            far_tx = 0;
        }
#else
        far_sound[0] = 0;
        far_tx = 0;
#endif
        rx = channel_model(tx, far_tx);
        //rx += awgn(&far_noise_source);
        //tx += awgn(&far_noise_source);
        tx = alaw_munge(tx);

        clean = echo_can_update(ctx, tx, rx);

#if defined(XYZZY)
        if (i%8000 == 0)
        {
            if (coeff_index < 200)
            {
                for (j = 0;  j < ctx->taps;  j++)
                    coeffs[coeff_index][j] = ctx->fir_taps32[j];
                coeff_index++;
            }
        }
#endif
        result_sound[result_cur++] = tx;
        result_sound[result_cur++] = rx;
        result_sound[result_cur++] = clean - far_tx;
        //result_sound[result_cur++] = ctx->tx_power[2];
        //result_sound[result_cur++] = ctx->tx_power[1];
        result_sound[result_cur++] = (ctx->tx_power[1] > 64)  ?  8000  :  -8000;
        //result_sound[result_cur++] = ctx->tap_set*8000;
        //result_sound[result_cur++] = (ctx->nonupdate_dwell > 0)  ?  8000  :  -8000;
        //result_sound[result_cur++] = ctx->latest_correction >> 8;
        //result_sound[result_cur++] = level_measurement_device(tx)/(16.0*65536.0);
        //result_sound[result_cur++] = level_measurement_device(tx)/4096.0;
        result_sound[result_cur++] = (ctx->tx_power[1] > ctx->rx_power[0])  ?  8000  :  -8000;
        //result_sound[result_cur++] = (ctx->tx_power[1] > ctx->rx_power[0])  ?  8000  :  -8000;
        //result_sound[result_cur++] = (ctx->narrowband_score)*5; //  ?  8000  :  -8000;
        //result_sound[result_cur++] = ctx->tap_rotate_counter*10;
        result_sound[result_cur++] = ctx->vad;
        
        put_residue(clean - far_tx);
        if (result_cur >= 48000)
        {
            outframes = afWriteFrames(resulthandle,
                                      AF_DEFAULT_TRACK,
                                      result_sound,
                                      result_cur/6);
            if (outframes != result_cur/6)
            {
                fprintf(stderr, "    Error writing result sound\n");
                exit(2);
            }
            result_cur = 0;
        }
    }
    if (result_cur > 0)
    {
        outframes = afWriteFrames(resulthandle,
                                  AF_DEFAULT_TRACK,
                                  result_sound,
                                  result_cur/6);
        if (outframes != result_cur/4)
        {
            fprintf(stderr, "    Error writing result sound\n");
            exit(2);
        }
    }
#endif

    /* Test 1 - Steady state residual and returned echo level test */
    /* This functionality has been merged with test 2 in newer versions of G.168,
       so test 1 no longer exists. */

    /* Test 2 - Convergence and steady state residual and returned echo level test */
#if defined(PERFORM_TEST_2A)
    /* Test 2A - Convergence with NLP enabled */
    echo_can_flush(ctx);
    /* Converge the canceller */
    signal_restart(&local_css);
    for (i = 0;  i < 800*2;  i++)
    {
        clean = echo_can_update(ctx, 0, 0);
        put_residue(clean);
    }
    for (i = 0;  i < 8000*50;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        clean = 100.0*level_measurement_device(power_meter_1, rx)/level_measurement_device(power_meter_2, clean);
        put_residue(clean);
    }
#endif

#if defined(PERFORM_TEST_2B)
    /* Test 2B - Convergence with NLP disabled */
    echo_can_flush(ctx);
    /* Converge a canceller */
    signal_restart(&local_css);
    for (i = 0;  i < 800*2;  i++)
    {
        clean = echo_can_update(ctx, 0, 0);
        put_residue(clean);
    }
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
#endif

#if defined(PERFORM_TEST_2C)
    /* Test 2C - Convergence with background noise present */
    echo_can_flush(ctx);
    /* Converge a canceller */
    signal_restart(&local_css);
    for (i = 0;  i < 800*2;  i++)
    {
        clean = echo_can_update(ctx, 0, 0);
        put_residue(clean);
    }
    /* TODO: This uses a crude approx. to Hoth noise. We need the real thing. */
    awgn_init(&far_noise_source, 7162534, -40);
    noise = 0;
    for (i = 0;  i < 8000*5;  i++)
    {
        noise = noise*0.675 + awgn(&far_noise_source)*0.375;
        tx = signal_amp(&local_css);
        rx = channel_model(tx, noise);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    /* Now freeze adaption, and measure the echo. */
    echo_can_adaption_mode(ctx, ECHO_CAN_FREEZE_ADAPTION);
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }

    echo_can_adaption_mode(ctx, 0);
#endif

    /* Test 3 - Performance under double talk conditions */
#if defined(PERFORM_TEST_3A)
    /* Test 3A - Double talk test with low cancelled-end levels */
    echo_can_flush(ctx);
    signal_restart(&local_css);
    signal_restart(&far_css);
    /* Apply double talk, with a weak far end signal */
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = signal_amp(&far_css)/20;
        channel_model(tx, rx);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean - rx);
    }
    /* Now freeze adaption. */
    echo_can_adaption_mode(ctx, ECHO_CAN_FREEZE_ADAPTION);
    for (i = 0;  i < 800*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    /* Now measure the echo */
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    echo_can_adaption_mode(ctx, 0);
#endif

#if defined(PERFORM_TEST_3B)
    /* Test 3B - Double talk test with high cancelled-end levels */
    echo_can_flush(ctx);
    signal_restart(&local_css);
    signal_restart(&far_css);
    /* Converge it */
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    /* Apply double talk */
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = signal_amp(&far_css);
        rx = channel_model(tx, rx);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean - rx);
    }
    /* Now freeze adaption. */
    echo_can_adaption_mode(ctx, ECHO_CAN_FREEZE_ADAPTION);
    for (i = 0;  i < 800*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    /* Now measure the echo */
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    echo_can_adaption_mode(ctx, 0);
#endif

#if defined(PERFORM_TEST_3C)
    /* Test 3C - Double talk test with simulated conversation */
    echo_can_flush(ctx);
    signal_restart(&local_css);
    signal_restart(&far_css);
    /* Apply double talk */
    for (i = 0;  i < 800*56;  i++)
    {
        tx = signal_amp(&local_css);
        rx = signal_amp(&far_css);
        rx = channel_model(tx, rx);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean - rx);
    }
    /* Stop the far signal */
    for (i = 0;  i < 800*14;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    /* Continue measuring the resulting echo */
    for (i = 0;  i < 800*50;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    /* Reapply double talk */
    signal_restart(&far_css);
    for (i = 0;  i < 800*56;  i++)
    {
        tx = signal_amp(&local_css);
        rx = signal_amp(&far_css);
        rx = channel_model(tx, rx);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean - rx);
    }
    /* Now the far signal only */
    for (i = 0;  i < 800*56;  i++)
    {
        tx = 0;
        rx = signal_amp(&far_css);
        rx = channel_model(0, rx);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean - rx);
    }
#endif

#if defined(PERFORM_TEST_4)
    /* Test 4 - Leak rate test */
    echo_can_flush(ctx);
    /* Converge a canceller */
    signal_restart(&local_css);
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    /* Put 2 minutes of silence through it */
    for (i = 0;  i < 8000*120;  i++)
    {
        clean = echo_can_update(ctx, 0, 0);
        put_residue(clean);
    }
    /* Now freeze it, and check if it is still well adapted. */
    echo_can_adaption_mode(ctx, ECHO_CAN_FREEZE_ADAPTION);
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    echo_can_adaption_mode(ctx, 0);
#endif

#if defined(PERFORM_TEST_5)
    /* Test 5 - Infinite return loss convergence test */
    echo_can_flush(ctx);
    /* Converge the canceller */
    signal_restart(&local_css);
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    /* Now stop echoing, and see we don't do anything unpleasant as the
       echo path is open looped. */
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = signal_amp(&local_css);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, 0);
        put_residue(clean);
    }
#endif

#if defined(PERFORM_TEST_6)
    /* Test 6 - Non-divergence on narrow-band signals */
    echo_can_flush(ctx);
    /* Converge the canceller */
    signal_restart(&local_css);
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    /* Now put 5s bursts of a list of tones through the converged canceller, and check
       that bothing unpleasant happens. */
    for (k = 0;  tones_6_4_2_7[k][0];  k++)
    {
        make_tone_gen_descriptor(&tone_desc,
                                 tones_6_4_2_7[k][0],
                                 -11,
                                 tones_6_4_2_7[k][1],
                                 -9,
                                 1,
                                 0,
                                 0,
                                 0,
                                 1);
        tone_gen_init(&tone_state, &tone_desc);
        j = 0;
        for (i = 0;  i < 5;  i++)
        {
            local_max = tone_gen(&tone_state, local_sound, 8000);
            for (j = 0;  j < 8000;  j++)
            {
                tx = local_sound[j];
                rx = channel_model(tx, 0);
                tx = alaw_munge(tx);
                clean = echo_can_update(ctx, tx, rx);
                put_residue(clean);
            }
        }
    }
#endif

#if defined(PERFORM_TEST_7)
    /* Test 7 - Stability */
    /* Put tones through an unconverged canceller, and check nothing unpleasant
       happens. */
    echo_can_flush(ctx);
    make_tone_gen_descriptor(&tone_desc,
                             tones_6_4_2_7[0][0],
                             -11,
                             tones_6_4_2_7[0][1],
                             -9,
                             1,
                             0,
                             0,
                             0,
                             1);
    tone_gen_init(&tone_state, &tone_desc);
    j = 0;
    for (i = 0;  i < 120;  i++)
    {
        local_max = tone_gen(&tone_state, local_sound, 8000);
        for (j = 0;  j < 8000;  j++)
        {
            tx = local_sound[j];
            rx = channel_model(tx, 0);
            tx = alaw_munge(tx);
            clean = echo_can_update(ctx, tx, rx);
            put_residue(clean);
        }
    }
#endif

#if defined(PERFORM_TEST_8)
    /* Test 8 - Non-convergence on No 5, 6, and 7 in-band signalling */
#endif

#if defined(PERFORM_TEST_9)
    /* Test 9 - Comfort noise test */
    /* Test 9 part 1 - matching */
    echo_can_flush(ctx);
    /* Converge the canceller */
    signal_restart(&local_css);
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    echo_can_adaption_mode(ctx, ECHO_CAN_USE_NLP | ECHO_CAN_USE_CNG);
    awgn_init(&far_noise_source, 7162534, -45);
    for (i = 0;  i < 8000*30;  i++)
    {
        tx = 0;
        rx = awgn(&far_noise_source);
        rx = channel_model(tx, rx);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    awgn_init(&local_noise_source, 1234567, -10);
    for (i = 0;  i < 8000*2;  i++)
    {
        tx = awgn(&local_noise_source);
        rx = awgn(&far_noise_source);
        rx = channel_model(tx, rx);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }

    /* Test 9 part 2 - adjust down */
    awgn_init(&far_noise_source, 7162534, -55);
    for (i = 0;  i < 8000*10;  i++)
    {
        tx = 0;
        rx = awgn(&far_noise_source);
        rx = channel_model(tx, rx);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
    for (i = 0;  i < 8000*2;  i++)
    {
        tx = awgn(&local_noise_source);
        rx = awgn(&far_noise_source);
        rx = channel_model(tx, rx);
        tx = alaw_munge(tx);
        clean = echo_can_update(ctx, tx, rx);
        put_residue(clean);
    }
#endif

    /* Test 10 - FAX test during call establishment phase */
#if defined(PERFORM_TEST_10A)
    /* Test 10A - Canceller operation on the calling station side */
#endif

#if defined(PERFORM_TEST_10B)
    /* Test 10B - Canceller operation on the called station side */
#endif

#if defined(PERFORM_TEST_10C)
    /* Test 10C - Canceller operation on the calling station side during page
                  transmission and page breaks (for further study) */
#endif

#if defined(PERFORM_TEST_11)
    /* Test 11 ­ Tandem echo canceller test (for further study) */
#endif

#if defined(PERFORM_TEST_12)
    /* Test 12 ­ Residual acoustic echo test (for further study) */
#endif

#if defined(PERFORM_TEST_13)
    /* Test 13 ­ Performance with ITU-T low-bit rate coders in echo path
                 (Optional, under study) */
#endif

#if defined(PERFORM_TEST_14)
    /* Test 14 ­ Performance with V-series low-speed data modems */
#endif

#if defined(PERFORM_TEST_15)
    /* Test 15 ­ PCM offset test (Optional) */
#endif

    echo_can_free(ctx);

    signal_free(&local_css);
    signal_free(&far_css);

    if (afCloseFile(resulthandle) != 0)
    {
        fprintf(stderr, "    Cannot close speech file '%s'\n", "result_sound.wav");
        exit(2);
    }
    if (afCloseFile(residuehandle) != 0)
    {
        fprintf(stderr, "    Cannot close speech file '%s'\n", "residue_sound.wav");
        exit(2);
    }
#if defined(XYZZY)
    for (j = 0;  j < ctx->taps;  j++)
    {
        for (i = 0;  i < coeff_index;  i++)
            fprintf(stderr, "%d ", coeffs[i][j]);
        fprintf(stderr, "\n");
    }
#endif
    printf("Run time %lds\n", time(NULL) - now);
    
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

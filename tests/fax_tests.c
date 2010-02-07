/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fax_tests.c
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
 * $Id: fax_tests.c,v 1.11 2004/03/23 00:48:29 steveu Exp $
 */

//#define _ISOC9X_SOURCE	1
//#define _ISOC99_SOURCE	1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <audiofile.h>
#include <tiffio.h>
#include <pthread.h>

#include "spandsp.h"

#define SAMPLES_PER_CHUNK 160

void phase_b_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    
    i = (int) user_data;
    printf("Phase B handler on channel %d - 0x%X\n", i, result);
}
/*- End of function --------------------------------------------------------*/

void phase_d_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    
    i = (int) user_data;
    printf("Phase D handler on channel %d - 0x%X\n", i, result);
}
/*- End of function --------------------------------------------------------*/

void phase_e_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    
    i = (int) user_data;
    printf("Phase E handler on channel %d\n", i);
}
/*- End of function --------------------------------------------------------*/

#define MACHINES    6
struct machine_s
{
    int chan;
    pthread_t thread;
    pthread_mutex_t mutex;
    AFfilehandle handle;
    int16_t amp[SAMPLES_PER_CHUNK];
    int len;
    t30_state_t fax;
    int block;
    int used_block;
} machines[MACHINES];

void *channel(void *arg)
{
    AFfilesetup filesetup;
    struct machine_s *mc;
    int i;
    int outframes;
    char buf[128 + 1];
    int next_block;
    
    mc = (struct machine_s *) arg;
    filesetup = afNewFileSetup();
    if (filesetup == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    sprintf(buf, "/tmp/fax%d.wav", mc->chan + 1);
    mc->handle = afOpenFile(buf, "w", filesetup);
    if (mc->handle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", buf);
        exit(2);
    }
    i = mc->chan + 1;
    sprintf(buf, "%d%d%d%d%d%d%d%d", i, i, i, i, i, i, i, i);
    fax_init(&mc->fax, (mc->chan & 1)  ?  FALSE  :  TRUE, NULL);
    fax_set_local_ident(&mc->fax, buf);
    if ((mc->chan & 1))
    {
        sprintf(buf, "rx%d.tif", (mc->chan + 1)/2);
        fax_set_rx_file(&mc->fax, buf);
    }
    else
    {
        fax_set_tx_file(&mc->fax, "itutests.tif");
    }
    fax_set_phase_b_handler(&mc->fax, phase_b_handler, (void *) mc->chan);
    fax_set_phase_d_handler(&mc->fax, phase_d_handler, (void *) mc->chan);
    fax_set_phase_e_handler(&mc->fax, phase_e_handler, (void *) mc->chan);

    outframes = 0;
    for (;;)
    {
        pthread_mutex_lock(&mc->mutex);
        if (mc->used_block == mc->block)
        {
            mc->len = fax_tx_process(&mc->fax, mc->amp, SAMPLES_PER_CHUNK);
            /* The receive side always expects a full block of samples, but the
               transmit side may not be sending any when it doesn't need to. We
               may need to pad with some silence. */
            if (mc->len < SAMPLES_PER_CHUNK)
            {
                memset(mc->amp + mc->len, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - mc->len));
                mc->len = SAMPLES_PER_CHUNK;
            }
            mc->block++;
#if 1
            outframes = afWriteFrames(mc->handle, AF_DEFAULT_TRACK, mc->amp, mc->len);
            if (outframes != mc->len)
                break;
#endif
        }
        pthread_mutex_unlock(&mc->mutex);
        /* Cross connect pairs */
        i = mc->chan ^ 1;
        pthread_mutex_lock(&machines[i].mutex);
        if (machines[i].used_block != machines[i].block)
        {
#if 0
            outframes = afWriteFrames(mc->handle, AF_DEFAULT_TRACK, machines[i].amp, machines[i].len);
            if (outframes != mc->len)
                break;
#endif
            fax_rx_process(&mc->fax, machines[i].amp, machines[i].len);
            machines[i].used_block++;
        }
        pthread_mutex_unlock(&machines[i].mutex);
    }

    if (afCloseFile(mc->handle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file for channel %d\n", mc->chan);
        exit(2);
    }
}

int main(int argc, char *argv[])
{
    int i;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    for (i = 0;  i < MACHINES;  i++)
    {
        machines[i].chan = i;
        machines[i].block = 0;
        machines[i].used_block = 0;
        pthread_mutex_init(&machines[i].mutex, NULL);
        if (pthread_create(&machines[i].thread, &attr, channel, &machines[i]))
            exit(2);
        /*endif*/
    }
    sleep(60);
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

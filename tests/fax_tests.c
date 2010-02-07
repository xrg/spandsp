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
 * $Id: fax_tests.c,v 1.17 2004/12/08 14:00:36 steveu Exp $
 */

/*! \page fax_tests_page FAX tests
\section fax_tests_page_sec_1 What does it do?
\section fax_tests_page_sec_2 How does it work?
*/

//#define _ISOC9X_SOURCE	1
//#define _ISOC99_SOURCE	1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"

#define SAMPLES_PER_CHUNK 160

void phase_b_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    
    i = (intptr_t) user_data;
    printf("Phase B handler on channel %d - 0x%X\n", i, result);
}
/*- End of function --------------------------------------------------------*/

void phase_d_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    t30_stats_t t;
    char ident[21];

    i = (intptr_t) user_data;
    printf("Phase D handler on channel %d - 0x%X\n", i, result);
    fax_get_transfer_statistics(s, &t);
    printf("Phase D: bit rate %d\n", t.bit_rate);
    printf("Phase D: pages transferred %d\n", t.pages_transferred);
    printf("Phase D: image size %d x %d\n", t.columns, t.rows);
    printf("Phase D: image resolution %d x %d\n", t.column_resolution, t.row_resolution);
    printf("Phase D: bad rows %d\n", t.bad_rows);
    printf("Phase D: longest bad row run %d\n", t.longest_bad_row_run);
    printf("Phase D: compression type %d\n", t.encoding);
    printf("Phase D: image size %d\n", t.image_size);
    fax_get_local_ident(s, ident);
    printf("Phase D: local ident '%s'\n", ident);
    fax_get_far_ident(s, ident);
    printf("Phase D: remote ident '%s'\n", ident);
}
/*- End of function --------------------------------------------------------*/

void phase_e_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    
    i = (intptr_t) user_data;
    printf("Phase E handler on channel %d, result %d\n", i, result);
}
/*- End of function --------------------------------------------------------*/

#define MACHINES    2
struct machine_s
{
    int chan;
    AFfilehandle handle;
    int16_t amp[SAMPLES_PER_CHUNK];
    int len;
    t30_state_t fax;
} machines[MACHINES];

int main(int argc, char *argv[])
{
    AFfilesetup filesetup;
    int i;
    int j;
    struct machine_s *mc;
    int outframes;
    char buf[128 + 1];
    int16_t silence[SAMPLES_PER_CHUNK];

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

    memset(silence, 0, sizeof(silence));
    for (j = 0;  j < MACHINES;  j++)
    {
        machines[j].chan = j;
        mc = &machines[j];

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
        fax_set_phase_b_handler(&mc->fax, phase_b_handler, (void *) (intptr_t) mc->chan);
        fax_set_phase_d_handler(&mc->fax, phase_d_handler, (void *) (intptr_t) mc->chan);
        fax_set_phase_e_handler(&mc->fax, phase_e_handler, (void *) (intptr_t) mc->chan);
        mc->fax.verbose = 1;
        memset(mc->amp, 0, sizeof(mc->amp));
    }
    for (;;)
    {
        for (j = 0;  j < MACHINES;  j++)
        {
            mc = &machines[j];

            mc->len = fax_tx_process(&mc->fax, mc->amp, SAMPLES_PER_CHUNK);
            /* The receive side always expects a full block of samples, but the
               transmit side may not be sending any when it doesn't need to. We
               may need to pad with some silence. */
            if (mc->len < SAMPLES_PER_CHUNK)
            {
                memset(mc->amp + mc->len, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - mc->len));
                mc->len = SAMPLES_PER_CHUNK;
            }
#if 1
            outframes = afWriteFrames(mc->handle, AF_DEFAULT_TRACK, mc->amp, mc->len);
            if (outframes != mc->len)
                break;
#endif
            if (machines[j ^ 1].len < SAMPLES_PER_CHUNK)
                memset(machines[j ^ 1].amp + machines[j ^ 1].len, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - machines[j ^ 1].len));
            if (fax_rx_process(&mc->fax, machines[j ^ 1].amp, SAMPLES_PER_CHUNK))
                break;
            //if (fax_rx_process(&mc->fax, silence, SAMPLES_PER_CHUNK))
            //    break;
        }
        if (j < MACHINES)
            break;
    }
    for (j = 0;  j < MACHINES;  j++)
    {
        mc = &machines[j];
        if (afCloseFile(mc->handle) != 0)
        {
            fprintf(stderr, "    Cannot close wave file for channel %d\n", mc->chan);
            exit(2);
        }
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

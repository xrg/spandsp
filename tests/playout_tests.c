/*
 * SpanDSP - a series of DSP components for telephony
 *
 * playout_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
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
 * $Id: playout_tests.c,v 1.6 2005/09/01 17:06:45 steveu Exp $
 */

/*! \page playout_tests_page Playout (jitter buffering) tests
\section playout_tests_page_sec_1 What does it do?
These tests simulate timing jitter and packet loss in an audio stream, and see
how well the playout module copes.
*/

//#define _ISOC9X_SOURCE  1
//#define _ISOC99_SOURCE  1

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <tgmath.h>
#include <tiffio.h>

#include <audiofile.h>

#include "spandsp.h"

void dynamic_buffer_tests(void)
{
    playout_state_t *s;
    playout_frame_t frame;
    playout_frame_t *p;
    plc_state_t plc;
    time_scale_t ts;
    int16_t *amp;
    int16_t fill[160];
    int16_t buf[20*160];
    int16_t out[10*160];
    timestamp_t time_stamp;
    timestamp_t next_actual_receive;
    timestamp_t next_scheduled_receive;
    int near_far_time_offset;
    int rng;
    int i;
    int j;
    int ret;
    int len;
    int inframes;
    int outframes;
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;

    filesetup = afNewFileSetup();
    if (filesetup == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 2);

    inhandle = afOpenFile("playout_in.wav", "r", NULL);
    if (inhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Failed to open in file\n");
        exit(2);
    }
    outhandle = afOpenFile("playout_out.wav", "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Failed to open out file\n");
        exit(2);
    }

    near_far_time_offset = 54321;
    time_stamp = 12345;
    next_actual_receive = time_stamp + near_far_time_offset;
    next_scheduled_receive = 0;
    for (i = 0;  i < 160;  i++)
        fill[i] = 32767;

	if ((s = playout_new(2*160, 15*160)) == NULL)
        return;
    plc_init(&plc);
    time_scale_init(&ts, 1.0);
    for (i = 0;  i < 1000000;  i++)
    {
        if (i >= next_actual_receive)
        {
            amp = malloc(160*sizeof(int16_t));
            inframes = afReadFrames(inhandle,
                                    AF_DEFAULT_TRACK,
                                    amp,
                                    160);
            if (inframes < 160)
                break;
            ret = playout_put(s,
                              amp,
                              PLAYOUT_TYPE_SPEECH,
                              inframes,
                              time_stamp,
                              next_actual_receive);
#if 0
            switch (ret)
            {
            case PLAYOUT_OK:
                printf("<< Record\n");
                break;
            case PLAYOUT_ERROR:
                printf("<< Error\n");
                break;
            default:
                printf("<< Eh?\n");
                break;
            }
#endif
            rng = rand() & 0xFF;
            if (i < 100000)
                rng = (rng*rng) >> 7;
            else if (i < 200000)
                rng = (rng*rng) >> 6;
            else if (i < 300000)
                rng = (rng*rng) >> 5;
            else if (i < 400000)
                rng = (rng*rng) >> 7;
            time_stamp += 160;
            next_actual_receive = time_stamp + near_far_time_offset + rng;
        }
        if (i >= next_scheduled_receive)
        {
            do
            {
                ret = playout_get(s, &frame, next_scheduled_receive);
                if (ret == PLAYOUT_DROP)
                    printf(">> Drop %d\n", next_scheduled_receive);
            }
            while (ret == PLAYOUT_DROP);
            switch (ret)
            {
            case PLAYOUT_OK:
                printf(">> Play %d\n", next_scheduled_receive);
                plc_rx(&plc, frame.data, frame.sender_len);
                len = time_scale(&ts, out, ((int16_t *) frame.data), frame.sender_len);
printf("len = %d\n", len);
                for (j = 0;  j < len;  j++)
                {
                    buf[2*j] = out[j];
                    buf[2*j + 1] = 10*playout_current_length(s);
                }
                outframes = afWriteFrames(outhandle, AF_DEFAULT_TRACK, buf, len);
                if (outframes != len)
                {
                    fprintf(stderr, "    Error writing out sound\n");
                    exit(2);
                }
                free(frame.data);
                next_scheduled_receive += 160;
                break;
            case PLAYOUT_FILLIN:
                printf(">> Fill %d\n", next_scheduled_receive);
                plc_fillin(&plc, fill, 160);
                time_scale_rate(&ts, 0.5);
                len = time_scale(&ts, out, fill, 160);
                time_scale_rate(&ts, 1.0);
printf("len = %d\n", len);
                for (j = 0;  j < len;  j++)
                {
                    buf[2*j] = out[j];
                    buf[2*j + 1] = 10*playout_current_length(s);
                }
                outframes = afWriteFrames(outhandle, AF_DEFAULT_TRACK, buf, len);
                if (outframes != len)
                {
                    fprintf(stderr, "    Error writing out sound\n");
                    exit(2);
                }
                next_scheduled_receive += 160;
                break;
            case PLAYOUT_DROP:
                printf(">> Drop %d\n", next_scheduled_receive);
                break;
            case PLAYOUT_NOFRAME:
                printf(">> No frame %d %d %d %d\n", next_scheduled_receive, playout_next_due(s), s->last_speech_sender_stamp, s->last_speech_sender_len);
                next_scheduled_receive += 160;
                break;
            case PLAYOUT_EMPTY:
                printf(">> Empty %d\n", next_scheduled_receive);
                next_scheduled_receive += 160;
                break;
            case PLAYOUT_ERROR:
                printf(">> Error %d\n", next_scheduled_receive);
                next_scheduled_receive += 160;
                break;
            default:
                printf(">> Eh? %d\n", next_scheduled_receive);
                break;
            }
        }
    }
    if (afCloseFile(inhandle) != 0)
    {
        fprintf(stderr, "    Cannot close speech file '%s'\n", "plc_in.wav");
        exit(2);
    }
    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close speech file '%s'\n", "plc_out.wav");
        exit(2);
    }

    printf("%10d %10d %10d\n", s->state_just_in_time, s->state_late, playout_current_length(s));

    /* Clear everything from the queue */
    while ((p = playout_get_unconditional(s)))
		/*free(p->data)*/;
    /* Now free the context itself */
    playout_free(s);
}
/*- End of function --------------------------------------------------------*/

void static_buffer_tests(void)
{
    playout_state_t *s;
    playout_frame_t frame;
    playout_frame_t *p;
    int type;
    uint8_t fr[160];
    timestamp_t next_scheduled_send;
    int transit_time;
    timestamp_t next_actual_receive;
    timestamp_t next_scheduled_receive;
    int len;
    int i;
    int ret;

    next_scheduled_send = 0;
    transit_time = 320;
    next_actual_receive = next_scheduled_send + transit_time;
    next_scheduled_receive = 960;

    memset(fr, 0, sizeof(fr));
    type = PLAYOUT_TYPE_SPEECH;
    len = 160;

    if ((s = playout_new(2*160, 2*160)) == NULL)
        return;
    for (i = 0;  i < 1000000;  i++)
    {
        if (i >= next_actual_receive)
        {
            ret = playout_put(s,
                              fr,
                              type,
                              len,
                              next_scheduled_send, 
                              next_actual_receive);
            switch (ret)
            {
            case PLAYOUT_OK:
                printf("<< Record\n");
                break;
            case PLAYOUT_ERROR:
                printf("<< Error\n");
                break;
            default:
                printf("<< Eh?\n");
                break;
            }
            next_scheduled_send += 160;
            ret = rand() & 0xFF;
            ret = (ret*ret) >> 7;
            transit_time = 320 + ret;
            next_actual_receive = next_scheduled_send + transit_time;
        }
        if (i >= next_scheduled_receive)
        {
            do
            {
                ret = playout_get(s, &frame, next_scheduled_receive);
            }
            while (ret == PLAYOUT_DROP);
            switch (ret)
            {
            case PLAYOUT_OK:
                printf(">> Play\n");
                next_scheduled_receive += 160;
                break;
            case PLAYOUT_FILLIN:
                printf(">> Fill\n");
                next_scheduled_receive += 160;
                break;
            case PLAYOUT_DROP:
                printf(">> Drop\n");
                break;
            case PLAYOUT_NOFRAME:
                printf(">> No frame\n");
                next_scheduled_receive += 160;
                break;
            case PLAYOUT_EMPTY:
                printf(">> Empty\n");
                next_scheduled_receive += 160;
                break;
            case PLAYOUT_ERROR:
                printf(">> Error\n");
                next_scheduled_receive += 160;
                break;
            default:
                printf(">> Eh?\n");
                break;
            }
        }
    }
    /* Clear everything from the queue */
    while ((p = playout_get_unconditional(s)))
		/*free(p->data)*/;
    /* Now free the context itself */
    playout_free(s);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    printf("Dynamic buffering tests\n");
    dynamic_buffer_tests();
    printf("Static buffering tests\n");
    static_buffer_tests();
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

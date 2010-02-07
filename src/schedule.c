/*
 * SpanDSP - a series of DSP components for telephony
 *
 * schedule.c
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
 * $Id: schedule.c,v 1.2 2004/12/31 15:23:01 steveu Exp $
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#include "spandsp/telephony.h"
#include "spandsp/schedule.h"

#define FALSE 0
#define TRUE (!FALSE)

int sp_schedule_event(sp_sched_state_t *s, int ms, sp_sched_callback_func_t function, void *user_data)
{
    int i;

    for (i = 0;  i < s->max_to_date;  i++)
    {
        if (s->sched[i].callback == NULL)
            break;
        /*endif*/
    }
    /*endfor*/
    if (i >= s->allocated)
    {
        s->allocated += 5;
        s->sched = (sp_sched_t *) realloc(s->sched, sizeof(sp_sched_t)*s->allocated);
    }
    /*endif*/
    if (i >= s->max_to_date)
        s->max_to_date = i + 1;
    /*endif*/
    s->sched[i].when = s->ticker + ms*SAMPLE_RATE/1000;
    s->sched[i].callback = function;
    s->sched[i].user_data = user_data;
    return i;
}
/*- End of function --------------------------------------------------------*/

uint64_t sp_schedule_next(sp_sched_state_t *s)
{
    int i;
    uint64_t earliest;

    earliest = ~0;
    for (i = 0;  i < s->max_to_date;  i++)
    {
        if (s->sched[i].callback  &&  earliest > s->sched[i].when)
            earliest = s->sched[i].when;
        /*endif*/
    }
    /*endfor*/
    return earliest;
}
/*- End of function --------------------------------------------------------*/

uint64_t sp_schedule_time(sp_sched_state_t *s)
{
    return s->ticker;
}
/*- End of function --------------------------------------------------------*/

void sp_schedule_update(sp_sched_state_t *s, int samples)
{
    int i;
    sp_sched_callback_func_t callback;
    void *user_data;

    s->ticker += samples;
    for (i = 0;  i < s->max_to_date;  i++)
    {
        if (s->sched[i].callback  &&  s->sched[i].when <= s->ticker)
        {
            callback = s->sched[i].callback;
            user_data = s->sched[i].user_data;
            s->sched[i].callback = NULL;
            s->sched[i].user_data = NULL;
            callback(s, user_data);
        }
        /*endif*/
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

void sp_schedule_del(sp_sched_state_t *s, int i)
{
    if (i >= s->max_to_date
        ||
        i < 0
        ||
        s->sched[i].callback == NULL)
    {
        fprintf(stderr, "Asked to delete scheduled ID %d???\n", i);
        return;
    }
    /*endif*/
    s->sched[i].callback = NULL;
}
/*- End of function --------------------------------------------------------*/

void sp_schedule_init(sp_sched_state_t *s)
{
    memset(s, 0, sizeof(*s));
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

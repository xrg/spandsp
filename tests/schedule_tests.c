/*
 * SpanDSP - a series of DSP components for telephony
 *
 * schedule_tests.c
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
 * $Id: schedule_tests.c,v 1.1 2004/12/29 15:04:59 steveu Exp $
 */

//#define _ISOC9X_SOURCE  1
//#define _ISOC99_SOURCE  1

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>

#include "spandsp.h"

void callback(sp_sched_state_t *s, void *user_data)
{
    uint64_t when;

    printf("Callback at %f\n", (float) s->ticker/8000.0);
    sp_schedule_event(s, 500, callback, NULL);
    when = sp_schedule_next(s);
    printf("Earliest is %llX\n", when);
}

int main(int argc, char *argv[])
{
    int i;
    int id;
    sp_sched_state_t sched;
    
    sp_schedule_init(&sched);

    id = sp_schedule_event(&sched, 500, callback, NULL);
    
    //sp_schedule_del(&sched, i);
    
    for (i = 0;  i < SAMPLE_RATE*100;  i += 160)
    {
        sp_schedule_update(&sched, 160);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

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
 * $Id: schedule_tests.c,v 1.6 2005/12/29 12:46:21 steveu Exp $
 */

/*! \page schedule_tests_page Event scheduler tests
\section schedule_tests_page_sec_1 What does it do?
???.

\section schedule_tests_page_sec_2 How does it work?
???.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <tgmath.h>
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
    sp_schedule_release(&sched);
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

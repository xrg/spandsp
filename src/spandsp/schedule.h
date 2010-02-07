/*
 * SpanDSP - a series of DSP components for telephony
 *
 * schedule.h
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
 * $Id: schedule.h,v 1.2 2004/12/31 15:23:01 steveu Exp $
 */

/*! \file */

#if !defined(_SCHEDULE_H_)
#define _SCHEDULE_H_

typedef struct sp_sched_state_s sp_sched_state_t;

typedef void (*sp_sched_callback_func_t)(sp_sched_state_t *s, void *user_data);

typedef struct
{
    uint64_t when;
    sp_sched_callback_func_t callback;
    void *user_data;
} sp_sched_t;

struct sp_sched_state_s
{
    uint64_t ticker;
    int allocated;
    int max_to_date;
    sp_sched_t *sched;
};

#ifdef __cplusplus
extern "C" {
#endif

uint64_t sp_schedule_next(sp_sched_state_t *s);
uint64_t sp_schedule_time(sp_sched_state_t *s);
void sp_schedule_run(sp_sched_state_t *s);

int sp_schedule_event(sp_sched_state_t *s, int ms, void (*function)(sp_sched_state_t *s, void *data), void *user_data);
void sp_schedule_update(sp_sched_state_t *s, int samples);
void sp_schedule_del(sp_sched_state_t *s, int id);

void sp_schedule_init(sp_sched_state_t *s);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

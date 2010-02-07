/*
 * SpanDSP - a series of DSP components for telephony
 *
 * echo_monitor.h - Display echo canceller status, using the FLTK toolkit.
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
 * $Id: echo_monitor.h,v 1.1 2004/12/16 15:33:55 steveu Exp $
 */

#if !defined(_ECHO_MONITOR_H_)
#define _ECHO_MONITOR_H_

#ifdef __cplusplus
extern "C" {
#endif

    int start_echo_can_monitor(int len);
    int echo_can_monitor_update(const int16_t *coeffs, int len);
    void echo_can_monitor_wait_to_end(void);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

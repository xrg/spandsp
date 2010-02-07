/*
 * SpanDSP - a series of DSP components for telephony
 *
 * oss.h - OSS interface routines for testing stuff
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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
 * $Id: oss.h,v 1.2 2004/03/30 14:29:41 steveu Exp $
 */

#if !defined(_OSS_H_)
#define _OSS_H_

#ifdef __cplusplus
extern "C" {
#endif

int oss_init(int mode);
int oss_get(int16_t amp[], int samples);
int oss_put(int16_t amp[], int samples);
int oss_release(void);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

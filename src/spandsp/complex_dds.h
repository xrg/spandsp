/*
 * SpanDSP - a series of DSP components for telephony
 *
 * complex_dds.h
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
 * $Id: complex_dds.h,v 1.1 2004/03/12 16:27:24 steveu Exp $
 */

int32_t complex_dds_phase_step(double frequency);
float real_dds(uint32_t *phase_acc, int32_t phase_rate);
complex_t complex_dds(uint32_t *phase_acc, int32_t phase_rate);
complex_t complex_dds_mod(uint32_t *phase_acc, int32_t phase_rate, float scale, int32_t phase);

/*- End of file ------------------------------------------------------------*/

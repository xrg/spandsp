/*
 * SpanDSP - a series of DSP components for telephony
 *
 * constel.h - Display QAM constellations, using the FLTK toolkit.
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
 * $Id: modem_monitor.h,v 1.3 2005/12/08 16:51:00 steveu Exp $
 */

/*! \page constel_page Modem performance monitoring
\section constel_page_sec_1 What does it do?
This code controls a GUI window, which provides monitoring of the internal status
of a modem. It shows, graphically:

    - the constellation, for PSK, QAM and other similar modulations.
    - the equalizer status, for modems with adaptive equalizers.
    - the carrier frequency.
    - the symbol timing correction.

\section constel_page_sec_2 How does it work?
This code uses the FLTK cross platform GUI toolkit. It works on X11 and Windows platforms.
In addition to the basic FLTK toolkit, fltk_cartesian is also required.
*/

#if !defined(_CONSTEL_H_)
#define _CONSTEL_H_

#ifdef __cplusplus
extern "C" {
#endif

int start_qam_monitor(float constel_width);
int update_qam_monitor(const complex_t *pt);
int update_qam_equalizer_monitor(const complex_t *coeffs, int len);
int update_qam_symbol_tracking(int total_correction);
int update_qam_carrier_tracking(float carrier);
void qam_wait_to_end(void);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

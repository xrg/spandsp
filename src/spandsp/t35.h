/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t35.h - ITU T.35 FAX non-standard facility processing.
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
 * $Id: t35.h,v 1.4 2004/11/05 14:48:41 steveu Exp $
 */

/*! \file */

#if !defined(_T35_H_)
#define _T35_H_

extern const char *t35_country_codes[256];

#ifdef __cplusplus
extern "C" {
#endif

/*! Decode an NSF field to try to determine the make and model of the
    remote machine.
    \brief Decode an NSF field.
    \param msg The NSF message.
    \param len The length of the NSF message.
    \param vendor A pointer which will be pointed to the identified vendor.
           If a NULL pointer is given, the vendor ID will not be returned.
           If the vendor is not identified, NULL will be returned.
    \param model A pointer which will be pointed to the identified model.
           If a NULL pointer is given, the model will not be returned.
           If the model is not identified, NULL will be returned.
    \return TRUE if the machine was identified, otherwise FALSE.
*/
int t35_decode(const uint8_t *msg, int len, const char **vendor, const char **model);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

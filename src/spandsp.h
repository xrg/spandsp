/*
 * SpanDSP - a series of DSP components for telephony
 *
 * spandsp.h - The head guy amongst the headers
 *
 * Written by Steve Underwood <spandsp/steveu@coppice.org>
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
 * $Id: spandsp.h,v 1.4 2004/03/15 13:17:35 steveu Exp $
 */

/*! \file */

#if !defined(_SPANDSP_H_)
#define _SPANDSP_H_

#include <spandsp/telephony.h>
#include <spandsp/alaw_ulaw.h>
#include <spandsp/timing.h>
#include <spandsp/vector.h>
#include <spandsp/complex.h>
#include <spandsp/mmx.h>
#include <spandsp/arctan2.h>
#include <spandsp/biquad.h>
#include <spandsp/fir.h>
#include <spandsp/awgn.h>
#include <spandsp/power_meter.h>
#include <spandsp/complex_dds.h>
#include <spandsp/complex_filters.h>
#include <spandsp/dc_restore.h>
#include <spandsp/dds.h>
#include <spandsp/echo.h>
#include <spandsp/fsk.h>
#include <spandsp/hdlc.h>
#include <spandsp/oss.h>
#include <spandsp/tone_detect.h>
#include <spandsp/tone_generate.h>
#include <spandsp/super_tone_detect.h>
#include <spandsp/super_tone_generate.h>
#include <spandsp/v27ter_rx.h>
#include <spandsp/v27ter_tx.h>
#include <spandsp/v29rx.h>
#include <spandsp/v29tx.h>
#include <spandsp/t4.h>
#include <spandsp/t30.h>
#include <spandsp/t30_fcf.h>
#include <spandsp/t35.h>
#include <spandsp/adsi.h>
#include <spandsp/oki_adpcm.h>
#include <spandsp/ec_disable_detector.h>

#endif
/*- End of file ------------------------------------------------------------*/

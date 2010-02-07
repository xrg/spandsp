/*
 * SpanDSP - a series of DSP components for telephony
 *
 * expose.h - Expose the internal structures of spandsp, for users who
 *            really need that.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: expose.h,v 1.1 2008/10/18 05:32:48 steveu Exp $
 */

/*! \file */

/* TRY TO ONLY INCLUDE THIS IF YOU REAKKY REALLY HAVE TO */

#if !defined(_SPANDSP_EXPOSE_H_)
#define _SPANDSP_EXPOSE_H_

#include <spandsp/private/bell_r2_mf.h>
#include <spandsp/private/dtmf.h>
#include <spandsp/private/g722.h>
#include <spandsp/private/g726.h>
#include <spandsp/private/fsk.h>
#include <spandsp/private/v29rx.h>
#include <spandsp/private/v29tx.h>
#include <spandsp/private/v17rx.h>
#include <spandsp/private/v17tx.h>
#include <spandsp/private/v27ter_rx.h>
#include <spandsp/private/v27ter_tx.h>
#include <spandsp/private/modem_connect_tones.h>
#include <spandsp/private/fax_modems.h>
#include <spandsp/private/t31.h>
#include <spandsp/private/t4.h>
#include <spandsp/private/t30.h>
#include <spandsp/private/fax.h>
#include <spandsp/private/t38_non_ecm_buffer.h>
#include <spandsp/private/t38_gateway.h>
#include <spandsp/private/t38_terminal.h>
#include <spandsp/private/t31.h>
#include <spandsp/private/v8.h>
#include <spandsp/private/adsi.h>

#endif
/*- End of file ------------------------------------------------------------*/

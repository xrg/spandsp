/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fcf.h - ITU T.30 fax control field definitions
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
 * $Id: t30_fcf.h,v 1.1 2004/03/12 16:27:25 steveu Exp $
 */

/*! \file */

#if !defined(_T30_FCF_H_)
#define _T30_FCF_H_

/* Initial identification */
/* From the called to the calling terminal. */
#define T30_DIS     0x80        /* Digital identification signal */
#define T30_CSI     0x40        /* Called subscriber identification */
#define T30_NSF     0x20        /* Non-standard facilities */

/* Command to send */
/* From a calling terminal wishing to be a receiver to a called terminal
   which is capable of transmitting. */
#define T30_DTC     0x81        /* Digital transmit command */
#define T30_CIG     0x41        /* Calling subscriber identification */
#define T30_NSC     0x21        /* Non-standard facilities command */
#define T30_PWD     0xC1        /* Password */
#define T30_SEP     0xA1        /* Selective polling */
#define T30_PSA     0x61        /* Polled subaddress */
#define T30_CIA     0xE1        /* Calling subscriber internet address */
#define T30_ISP     0x11        /* Internet selective polling address */

/* Command to receive */
#define T30_DCS     0x83        /* Digital command signal */
#define T30_TSI     0x43        /* Transmitting subscriber information */
#define T30_NSS     0x23        /* Non-standard facilities set-up */
#define T30_SUB     0xC3        /* Subaddress */
#define T30_SID     0xA3        /* Sender identification */
#define T30_TSA     0x63        /* Transmitting subscriber internet address */
#define T30_IRA     0xE3        /* Internet routing address */

/* Pre-message response signals */
/* From the receiver to the transmitter. */
#define T30_CFR     0x84        /* Confirmation to receive */
#define T30_FTT     0x44        /* Failure to train    */
#define T30_CSA     0x24        /* Called subscriber internet address */

/* Post-message commands */
#define T30_EOM     0x8F        /* End of message */
#define T30_MPS     0x4F        /* Multipage signal */
#define T30_EOP     0x2F        /* End of procedure */
#define T30_PRI_EOM 0x9F        /* Procedure interrupt - end of procedure */
#define T30_PRI_MPS 0x5F        /* Procedure interrupt - multipage signal */
#define T30_PRI_EOP 0x3F        /* Procedure interrupt - end of procedure */

/* Post-message responses */
#define T30_MCF     0x8C        /* Message confirmation */
#define T30_RTP     0xCC        /* Retrain positive */
#define T30_RTN     0x4C        /* Retrain negative */
#define T30_PIP     0xAC        /* Procedure interrupt positive */
#define T30_PIN     0x2C        /* Procedure interrupt negative */
#define T30_FDM     0xFC        /* File diagnostics message */

/* Other line control signals */
#define T30_DCN     0xFB        /* Disconnect (sender to receiver) */
#define T30_XCN     0xFA        /* Disconnect (receiver to sender) */
#define T30_CRP     0x1B        /* Command repeat */
#define T30_FNV     0xCB        /* Field not valid */

#endif
/*- End of file ------------------------------------------------------------*/

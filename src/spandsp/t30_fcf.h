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
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
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
 * $Id: t30_fcf.h,v 1.11 2007/12/13 11:31:33 steveu Exp $
 */

/*! \file */

#if !defined(_SPANDSP_T30_FCF_H_)
#define _SPANDSP_T30_FCF_H_

enum
{
    /*! Initial identification messages */
    /*! From the called to the calling terminal. */
    T30_DIS = 0x80,         /*! Digital identification signal */
    T30_CSI = 0x40,         /*! Called subscriber identification */
    T30_NSF = 0x20,         /*! Non-standard facilities */

    /*! Commands to send */
    /*! From a calling terminal wishing to be a receiver, to a called terminal
        which is capable of transmitting. */
    T30_DTC = 0x81,         /*! Digital transmit command */
    T30_CIG = 0x41,         /*! Calling subscriber identification */
    T30_NSC = 0x21,         /*! Non-standard facilities command */
    T30_PWD = 0xC1,         /*! Password */
    T30_SEP = 0xA1,         /*! Selective polling */
    T30_PSA = 0x61,         /*! Polled subaddress */
    T30_CIA = 0xE1,         /*! Calling subscriber internet address */
    T30_ISP = 0x11,         /*! Internet selective polling address */

    /*! Commands to receive */
    /*! From a calling terminal wishing to be a transmitter, to a called terminal
        which is capable of receiving. */
    T30_DCS = 0x82,         /*! Digital command signal */
    T30_TSI = 0x42,         /*! Transmitting subscriber information */
    T30_NSS = 0x22,         /*! Non-standard facilities set-up */
    T30_SUB = 0xC2,         /*! Subaddress */
    T30_SID = 0xA2,         /*! Sender identification */
    /*! T30_TCF - Training check is a burst of 1.5s of zeros sent using the image modem */
    T30_CTC = 0x12,         /*! Continue to correct */
    T30_TSA = 0x62,         /*! Transmitting subscriber internet address */
    T30_IRA = 0xE2,         /*! Internet routing address */

    /*! Pre-message response signals */
    /*! From the receiver to the transmitter. */
    T30_CFR = 0x84,         /*! Confirmation to receive */
    T30_FTT = 0x44,         /*! Failure to train */
    T30_CTR = 0xC4,         /*! Response for continue to correct */
    T30_CSA = 0x24,         /*! Called subscriber internet address */

    /*! Post-message commands */
    T30_EOM = 0x8E,         /*! End of message */
    T30_MPS = 0x4E,         /*! Multipage signal */
    T30_EOP = 0x2E,         /*! End of procedure */
    T30_PRI_EOM = 0x9E,     /*! Procedure interrupt - end of procedure */
    T30_PRI_MPS = 0x5E,     /*! Procedure interrupt - multipage signal */
    T30_PRI_EOP = 0x3E,     /*! Procedure interrupt - end of procedure */
    T30_EOS = 0x1E,         /*! End of selection */
    T30_PPS = 0xBE,         /*! Partial page signal */
    T30_EOR = 0xCE,         /*! End of retransmission */
    T30_RR = 0x6E,          /*! Receiver ready */

    /*! Post-message responses */
    T30_MCF = 0x8C,         /*! Message confirmation */
    T30_RTP = 0xCC,         /*! Retrain positive */
    T30_RTN = 0x4C,         /*! Retrain negative */
    T30_PIP = 0xAC,         /*! Procedure interrupt positive */
    T30_PIN = 0x2C,         /*! Procedure interrupt negative */
    T30_PPR = 0xBC,         /*! Partial page request */
    T30_RNR = 0xEC,         /*! Receive not ready */
    T30_ERR = 0x1C,         /*! Response for end of retransmission */
    T30_FDM = 0xFC,         /*! File diagnostics message */

    /*! Other line control signals */
    T30_DCN = 0xFA,         /*! Disconnect */
    T30_CRP = 0x1A,         /*! Command repeat */
    T30_FNV = 0xCA,         /*! Field not valid */
    T30_TNR = 0xEA,         /*! Transmit not ready */
    T30_TR = 0x6A,          /*! Transmit ready */
    T30_PID = 0x6C,         /*! Procedure interrupt disconnect */

    /*! Something only use as a secondary value in error correcting mode */
    T30_NULL = 0x00,        /*! Nothing to say */

    /*! Information frame types used for error correction mode, in T.4 */
    T4_FCD = 0x06,          /*! Facsimile coded data */
    T4_RCP = 0x86           /*! Return to control for partial page */
};

#endif
/*- End of file ------------------------------------------------------------*/

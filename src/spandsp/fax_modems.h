/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fax_modems.h - definitions for the analogue modem set for fax processing
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
 * $Id: fax_modems.h,v 1.1 2008/07/25 13:56:54 steveu Exp $
 */

/*! \file */

#if !defined(_SPANDSP_FAX_MODEMS_H_)
#define _SPANDSP_FAX_MODEMS_H_

/*!
    The set of modems needed for FAX, plus the auxilliary stuff, like tone generation.
*/
typedef struct
{
    /*! \brief An HDLC context used when transmitting HDLC messages. */
    hdlc_tx_state_t hdlc_tx;
    /*! \brief An HDLC context used when receiving HDLC messages. */
    hdlc_rx_state_t hdlc_rx;
    /*! \brief A V.21 FSK modem context used when transmitting HDLC over V.21
               messages. */
    fsk_tx_state_t v21_tx;
    /*! \brief A V.21 FSK modem context used when receiving HDLC over V.21
               messages. */
    fsk_rx_state_t v21_rx;
    /*! \brief A V.17 modem context used when sending FAXes at 7200bps, 9600bps
               12000bps or 14400bps */
    v17_tx_state_t v17_tx;
    /*! \brief A V.29 modem context used when receiving FAXes at 7200bps, 9600bps
               12000bps or 14400bps */
    v17_rx_state_t v17_rx;
    /*! \brief A V.29 modem context used when sending FAXes at 7200bps or
               9600bps */
    v29_tx_state_t v29_tx;
    /*! \brief A V.29 modem context used when receiving FAXes at 7200bps or
               9600bps */
    v29_rx_state_t v29_rx;
    /*! \brief A V.27ter modem context used when sending FAXes at 2400bps or
               4800bps */
    v27ter_tx_state_t v27ter_tx;
    /*! \brief A V.27ter modem context used when receiving FAXes at 2400bps or
               4800bps */
    v27ter_rx_state_t v27ter_rx;
    /*! \brief A tone generator context used to generate supervisory tones during
               FAX handling. */
    tone_gen_state_t tone_gen;
    /*! \brief Used to insert timed silences. */
    silence_gen_state_t silence_gen;
    /*! \brief */
    dc_restore_state_t dc_restore;
} fax_modems_t;

#if defined(__cplusplus)
extern "C"
{
#endif

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

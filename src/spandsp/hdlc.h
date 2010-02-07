/*
 * SpanDSP - a series of DSP components for telephony
 *
 * hdlc.h
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
 * $Id: hdlc.h,v 1.1 2004/03/12 16:27:25 steveu Exp $
 */

/*! \file */

#if !defined(_HDLC_H_)
#define _HDLC_H_

/*! 
    HDLC_MAXFRAME_LEN is the maximum length of a stuffed HDLC packet, excluding the CRC.
*/
#define HDLC_MAXFRAME_LEN       400	

typedef void (hdlc_packet_handler_t)(void *user_data, uint8_t *pkt, int len);
typedef void (hdlc_underflow_handler_t)(void *user_data);

/*!
    HDLC receive descriptor. This contains all the state information for an HDLC receiver.
 */
typedef struct
{
    /*! \brief 0 = sync hunt, !0 = receiving */
    int rx_state;	
    unsigned int bitbuf;
    unsigned int byteinprogress;
    int numbits;
	
    /*! \brief Buffer for a packet in progress. */
    uint8_t buffer[HDLC_MAXFRAME_LEN + 2];
    /*! \brief Length of a packet in progress. */
    int len;

    /*! \brief The number of good packets received. */
    unsigned long int rx_packets;
    /*! \brief The number of packets with CRC errors received. */
    unsigned long int rx_crc_errors;
    /*! \brief The number of HDLC aborts received. */
    unsigned long int rx_aborts;

    /*! \brief The callback routine called to process each good received packet. */
    hdlc_packet_handler_t *packet_handler;
    /*! \brief An opaque parameter passed to the callback routine. */
    void *user_data;
} hdlc_rx_state_t;

/*!
    HDLC transmit descriptor. This contains all the state information for an
    HDLC transmitter.
 */
typedef struct
{
    /*
     * 0 = send flags
     * 1 = send txtail (flags)
     * 2 = send packet
     */
    int tx_state;	
    int numbits;
    int idle_byte;

    int len;
    uint8_t buffer[HDLC_MAXFRAME_LEN + 2];
    int pos;

    int byte;
    int bits;

    int underflow_reported;
    
    unsigned long int tx_packets;
    unsigned long int tx_errors;

    hdlc_underflow_handler_t *underflow_handler;
    void *user_data;
} hdlc_tx_state_t;

/*! \brief Append an ITU/CCITT CRC-16 value to a packet.
    \param buf The buffer containing the packet. This must be at least 2 bytes longer than
               the packet it contains, to allow room for the CRC value.
    \param len The length of the packet.
    \return The new length of the packet.
*/
int append_crc_itu16(uint8_t *buf, int len);

/*! \brief Check the ITU/CCITT CRC-16 value in a packet.
    \param buf The buffer containing the packet.
    \param len The length of the packet.
    \return TRUE if the CRC is OK, else FALSE.
*/
int check_crc_itu16(const uint8_t *buf, int len);

/*! \brief Initialise an HDLC receiver context.
    \param s A pointer to an HDLC receiver context.
    \param handler The function to be called when a good HDLC packet is received.
    \param user_data An opaque parameter for the callback routine.
    \return A pointer to the HDLC receiver context.
*/
hdlc_rx_state_t *hdlc_rx_init(hdlc_rx_state_t *s,
                              hdlc_packet_handler_t *handler,
                              void *user_data);

/* Use either the bit-by-bit or byte-by-byte routines. Do not mix them is a
   single instance of HDLC */
void hdlc_rx_bit(hdlc_rx_state_t *s, int new_bit);
void hdlc_rx_byte(hdlc_rx_state_t *s, int new_byte);

/*! \brief Initialise an HDLC transmitter context.
    \param s A pointer to an HDLC transmitter context.
    \param handler The callback function called when the HDLC transmitter underflows.
    \param user_data An opaque parameter for the callback routine.
    \return A pointer to the HDLC transmitter context.
*/
hdlc_tx_state_t *hdlc_tx_init(hdlc_tx_state_t *s,
                              hdlc_underflow_handler_t *handler,
                              void *user_data);
void hdlc_tx_packet(hdlc_tx_state_t *s, uint8_t *packet, int len);
void hdlc_tx_preamble(hdlc_tx_state_t *s, int len);
int hdlc_tx_getbit(hdlc_tx_state_t *s);
int hdlc_tx_getbyte(hdlc_tx_state_t *s);

#endif
/*- End of file ------------------------------------------------------------*/

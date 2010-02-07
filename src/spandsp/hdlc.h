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
 * $Id: hdlc.h,v 1.6 2004/12/31 15:23:01 steveu Exp $
 */

/*! \file */

#if !defined(_HDLC_H_)
#define _HDLC_H_

/*! 
    HDLC_MAXFRAME_LEN is the maximum length of a stuffed HDLC frame, excluding the CRC.
*/
#define HDLC_MAXFRAME_LEN       400	

typedef void (*hdlc_frame_handler_t)(void *user_data, int ok, const uint8_t *pkt, int len);
typedef void (*hdlc_underflow_handler_t)(void *user_data);

/*!
    HDLC receive descriptor. This contains all the state information for an HDLC receiver.
 */
typedef struct
{
    /*! 2 for CRC-16, 4 for CRC-32 */
    int crc_bytes;
    /*! \brief The callback routine called to process each good received frame. */
    hdlc_frame_handler_t frame_handler;
    /*! \brief An opaque parameter passed to the callback routine. */
    void *user_data;
    int report_bad_frames;

    /*! \brief 0 = sync hunt, !0 = receiving */
    int rx_state;	
    unsigned int bitbuf;
    unsigned int byteinprogress;
    int numbits;
	
    /*! \brief Buffer for a frame in progress. */
    uint8_t buffer[HDLC_MAXFRAME_LEN + 2];
    /*! \brief Length of a frame in progress. */
    int len;

    /*! \brief The number of bytes of good frames received (CRC not included). */
    unsigned long int rx_bytes;
    /*! \brief The number of good frames received. */
    unsigned long int rx_frames;
    /*! \brief The number of frames with CRC errors received. */
    unsigned long int rx_crc_errors;
    /*! \brief The number of too short and too long frames received. */
    unsigned long int rx_length_errors;
    /*! \brief The number of HDLC aborts received. */
    unsigned long int rx_aborts;
} hdlc_rx_state_t;

typedef struct
{
    /*! \brief The number of bytes of good frames received (CRC not included). */
    unsigned long int bytes;
    /*! \brief The number of good frames received. */
    unsigned long int good_frames;
    /*! \brief The number of frames with CRC errors received. */
    unsigned long int crc_errors;
    /*! \brief The number of too short and too long frames received. */
    unsigned long int length_errors;
    /*! \brief The number of HDLC aborts received. */
    unsigned long int aborts;
} hdlc_rx_stats_t;

/*!
    HDLC transmit descriptor. This contains all the state information for an
    HDLC transmitter.
 */
typedef struct
{
    /*! 2 for CRC-16, 4 for CRC-32 */
    int crc_bytes;
    /*! \brief The callback routine called to indicate transmit underflow. */
    hdlc_underflow_handler_t underflow_handler;
    /*! \brief An opaque parameter passed to the callback routine. */
    void *user_data;

    int numbits;
    int idle_byte;

    int len;
    uint8_t buffer[HDLC_MAXFRAME_LEN + 2];
    int pos;

    int byte;
    int bits;

    int underflow_reported;
} hdlc_tx_state_t;

#ifdef __cplusplus
extern "C" {
#endif

uint32_t crc_itu32_calc(uint8_t *buf, int len);

/*! \brief Append an ITU/CCITT CRC-32 value to a frame.
    \param buf The buffer containing the frame. This must be at least 2 bytes longer than
               the frame it contains, to allow room for the CRC value.
    \param len The length of the frame.
    \return The new length of the frame.
*/
int crc_itu32_append(uint8_t *buf, int len);

/*! \brief Check the ITU/CCITT CRC-32 value in a frame.
    \param buf The buffer containing the frame.
    \param len The length of the frame.
    \return TRUE if the CRC is OK, else FALSE.
*/
int crc_itu32_check(const uint8_t *buf, int len);

uint16_t crc_itu16_calc(uint8_t *buf, int len);

/*! \brief Append an ITU/CCITT CRC-16 value to a frame.
    \param buf The buffer containing the frame. This must be at least 2 bytes longer than
               the frame it contains, to allow room for the CRC value.
    \param len The length of the frame.
    \return The new length of the frame.
*/
int crc_itu16_append(uint8_t *buf, int len);

/*! \brief Check the ITU/CCITT CRC-16 value in a frame.
    \param buf The buffer containing the frame.
    \param len The length of the frame.
    \return TRUE if the CRC is OK, else FALSE.
*/
int crc_itu16_check(const uint8_t *buf, int len);

/*! \brief Initialise an HDLC receiver context.
    \param s A pointer to an HDLC receiver context.
    \param handler The function to be called when a good HDLC frame is received.
    \param user_data An opaque parameter for the callback routine.
    \return A pointer to the HDLC receiver context.
*/
hdlc_rx_state_t *hdlc_rx_init(hdlc_rx_state_t *s,
                              int crc32,
                              hdlc_frame_handler_t handler,
                              void *user_data);

/*! \brief Control whether bad frames are reported.
    \param s A pointer to an HDLC receiver context.
    \param report TRUE if bad frames should be reported.
*/
void hdlc_rx_bad_frame_control(hdlc_rx_state_t *s,
                               int report);
                                                        
/*! \brief Get the current receive statistics.
    \param s A pointer to an HDLC receiver context.
    \param t A pointer to the buffer for the statistics.
    \return 0 for OK, else -1.
*/
int hdlc_rx_get_stats(hdlc_rx_state_t *s,
                      hdlc_rx_stats_t *t);

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
                              int crc32,
                              hdlc_underflow_handler_t handler,
                              void *user_data);
void hdlc_tx_frame(hdlc_tx_state_t *s, uint8_t *frame, int len);
void hdlc_tx_preamble(hdlc_tx_state_t *s, int len);
int hdlc_tx_getbit(hdlc_tx_state_t *s);
int hdlc_tx_getbyte(hdlc_tx_state_t *s);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

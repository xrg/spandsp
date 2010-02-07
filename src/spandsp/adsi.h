/*
 * SpanDSP - a series of DSP components for telephony
 *
 * adsi.h - Analogue display services interface and other call ID related handling.
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
 * $Id: adsi.h,v 1.4 2004/03/18 13:24:46 steveu Exp $
 */

/*! \file */

#if !defined(_ADSI_H_)
#define _ADSI_H_

#define ADSI_STANDARD_CLASS     1
#define ADSI_STANDARD_CLIP      2
#define ADSI_STANDARD_ACLIP     3
#define ADSI_STANDARD_JCLIP     4
#define ADSI_STANDARD_CLIP_DTMF 5

/* In some of the messages code characters are used, as follows:
        'C' for public callbox
        'L' for long distance
        'O' for overseas
        'P' for private
        'S' for service conflict */

/* Definitions for generic caller ID message type IDs */
#define CLIDINFO_CMPLT          0x100   /* Complete caller ID message */
#define CLIDINFO_GENERAL        0x101   /* Date, time, phone #, name */
#define CLIDINFO_CALLID         0x102   /* Caller ID */
#define CLIDINFO_FRAMETYPE      0x103   /* See frame type equates */

/* Definitions for CLASS (Custom Local Area Signaling Services) */
#define CLASS_SDMF_CALLERID     0x04    /* Single data message caller ID */
#define CLASS_MDMF_CALLERID     0x80    /* Multiple data message caller ID */
#define CLASS_SDMF_MSG_WAITING  0x06    /* Single data message message waiting */
#define CLASS_MDMF_MSG_WAITING  0x82    /* Multiple data message message waiting */

/* CLASS MDMF message IDs */
#define MCLASS_DATETIME         0x01    /* Date and time (MMDDHHMM) */
#define MCLASS_CALLER_NUMBER    0x02    /* Caller number */
#define MCLASS_DIALED_NUMBER    0x03    /* Dialed number */
#define MCLASS_ABSENCE1         0x04    /* Caller number absent: 'O' or 'P' */
#define MCLASS_REDIRECT         0x05    /* Call forward: universal ('0'), on busy ('1'), or on unanswered ('2') */
#define MCLASS_QUALIFIER        0x06    /* Long distance: 'L' */
#define MCLASS_CALLER_NAME      0x07    /* Caller's name */
#define MCLASS_ABSENCE2         0x08    /* Caller's name absent: 'O' or 'P' */

/* CLASS MDMF message waiting message IDs */
#define MCLASS_VISUAL_INDICATOR 0x0B    /* Message waiting */

/* Definitions for CLIP (Calling Line Identity Presentation) */
#define CLIP_MDMF_CALLERID      0x80    /* Multiple data message caller ID */
#define CLIP_MDMF_MSG_WAITING   0x82
#define CLIP_MDMF_CHARGE_INFO   0x86
#define CLIP_MDMF_SMS           0x89

/* CLIP message IDs */
#define CLIP_DATETIME           0x01    /* Date and time (MMDDHHMM) */
#define CLIP_CALLER_NUMBER      0x02    /* Caller number */
#define CLIP_DIALED_NUMBER      0x03    /* Dialed number */
#define CLIP_ABSENCE1           0x04    /* Caller number absent: 'O' or 'P' */
#define CLIP_CALLER_NAME        0x07    /* Caller's name */
#define CLIP_ABSENCE2           0x08    /* Caller's name absent: 'O' or 'P' */
#define CLIP_VISUAL_INDICATOR   0x0B    /* Visual indicator */
#define CLIP_MESSAGE_ID         0x0D    /* Message ID */
#define CLIP_CALLTYPE           0x11    /* Voice call, ring-back-when-free call, or msg waiting call */
#define CLIP_NUM_MSG            0x13    /* Number of messages */
#define CLIP_REDIR_NUMBER       0x03    /* Redirecting number */
#define CLIP_CHARGE             0x20    /* Charge */
#define CLIP_DURATION           0x23    /* Duration of the call */
#define CLIP_ADD_CHARGE         0x21    /* Additional charge */
#define CLIP_DISPLAY_INFO       0x50    /* Display information */
#define CLIP_SERVICE_INFO       0x55    /* Service information */

/* Definitions for A-CLIP (Analog Calling Line Identity Presentation) */
#define ACLIP_SDMF_CALLERID     0x04    /* Single data message caller ID frame   */
#define ACLIP_MDMF_CALLERID     0x80    /* Multiple data message caller ID frame */

/* A-CLIP MDM message IDs */
#define ACLIP_DATETIME          0x01    /* Date and time (MMDDHHMM) */
#define ACLIP_CALLER_NUMBER     0x02    /* Caller number */
#define ACLIP_DIALED_NUMBER     0x03    /* Dialed number */
#define ACLIP_ABSENCE1          0x04    /* Caller number absent: 'O' or 'P' */
#define ACLIP_REDIRECT          0x05    /* Call forward: universal, on busy, or on unanswered */
#define ACLIP_QUALIFIER         0x06    /* Long distance call: 'L' */
#define ACLIP_CALLER_NAME       0x07    /* Caller's name */
#define ACLIP_ABSENCE2          0x08    /* Caller's name absent: 'O' or 'P' */

/* Definitions for J-CLIP (Japan Calling Line Identity Presentation) */
#define JCLIP_MDMF_CALLERID     0x40    /* Multiple data message caller ID frame */

/* J-CLIP MDM message IDs */
#define JCLIP_CALLER_NUMBER     0x02    /* Caller number */
#define JCLIP_CALLER_NUM_DES    0x21    /* Caller number data extension signal */
#define JCLIP_DIALED_NUMBER     0x09    /* Dialed number */
#define JCLIP_DIALED_NUM_DES    0x22    /* Dialed number data extension signal */
#define JCLIP_ABSENCE           0x04    /* Caller number absent: 'C', 'O', 'P' or 'S' */

/* Definitions for CLIP-DTMF */
#define CLIP_DTMF_CALLER_NUMBER 'A'     /* Caller number */
#define CLIP_DTMF_ABSENCE1      'D'     /* Caller number absent: private (1), overseas (2) or not available (3) */

/*!
    ADSI transmitter descriptor. This contains all the state information for an ADSI
    (caller ID, CLASS, CLIP, ACLIP) transmit channel.
 */
typedef struct
{
    int standard;

    tone_gen_descriptor_t alert_tone_desc;
    tone_gen_state_t alert_tone_gen;
    fsk_tx_state_t fsktx;
    dtmf_tx_state_t dtmftx;

    int fsk_on;
    
    int byteno;
    int bitpos;
    int bitno;
    uint8_t msg[256];
    int msg_len;
    int ones_len;
} adsi_tx_state_t;

/*!
    ADSI receiver descriptor. This contains all the state information for an ADSI
    (caller ID, CLASS, CLIP, ACLIP) receive channel.
 */
typedef struct
{
    put_msg_func_t put_msg;
    void *user_data;
    int standard;

    fsk_rx_state_t fskrx;
    dtmf_rx_state_t dtmfrx;
    
    int consecutive_ones;
    int bitpos;
    int in_progress;
    uint8_t msg[256];
    int msg_len;
    
    int framing_errors;
} adsi_rx_state_t;

/*! \brief Initialise an ADSI receive context.
    \param s The ADSI receive context.
    \param standard The code for the ADSI standard to be used.
    \param put_msg A callback routine called to deliver the received messages
           to the application.
    \param user_data An opaque pointer for the callback routine.
*/
void adsi_rx_init(adsi_rx_state_t *s, int standard, put_msg_func_t put_msg, void *user_data);
void adsi_rx(adsi_rx_state_t *s, const int16_t *amp, int len);

/*! \brief Initialise an ADSI transmit context.
    \param s The ADSI transmit context.
    \param standard The code for the ADSI standard to be used.
*/
void adsi_tx_init(adsi_tx_state_t *s, int standard);
int adsi_tx(adsi_tx_state_t *s, int16_t *amp, int max_len);
void adsi_send_alert_tone(adsi_tx_state_t *s);

/*! \brief Put a message into the input buffer of an ADSI transmit context.
    \param s The ADSI transmit context.
    \param msg The message.
    \param len The length of the message.
    \return The length actually added. If a message is already in progress
            in the transmitter, this function will return zero, as it will
            not successfully add the message to the buffer.
*/
int adsi_put_message(adsi_tx_state_t *s, uint8_t *msg, int len);

/*! \brief Get a field from an ADSI message.
    \param s The ADSI receive context.
    \param msg The message buffer.
    \param msg_len The length of the message.
    \param pos Current position within the message. Set to -1 when starting a message.
    \param field_type The type code for the field.
    \param field_body Pointer to the body of the field.
    \param field_len The length of the field.
*/
int adsi_next_field(adsi_rx_state_t *s, const uint8_t *msg, int msg_len, int pos, uint8_t *field_type, uint8_t const **field_body, int *field_len);

/*! \brief Insert the header or a field into an ADSI message.
    \param s The ADSI transmit context.
    \param msg The message buffer.
    \param len The current length of the message.
    \param field_type The type code for the new field.
    \param field_body Pointer to the body of the new field.
    \param field_len The length of the new field.
*/
int adsi_add_field(adsi_tx_state_t *s, uint8_t *msg, int len, uint8_t field_type, uint8_t const *field_body, int field_len);

#endif
/*- End of file ------------------------------------------------------------*/

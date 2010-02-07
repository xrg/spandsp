/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4.h - definitions for T.4 fax processing
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
 * $Id: t4.h,v 1.2 2004/03/21 13:02:00 steveu Exp $
 */

/*! \file */

#if !defined(_T4_H_)
#define _T4_H_

/*!
    T.4 FAX compression/decompression descriptor. This defines the working state
    for a single instance of a T.4 FAX compression or decompression channel.
*/
typedef struct
{
    int pages_transferred;

    int scan_lines;
    int options;
    int remote_compression;
    int min_scan_line_bits;

    char rx_file[256];
    char tx_file[256];

    /* The TIFF stuff */
    float x_resolution;
    float y_resolution;

    int output_compression;
    int output_t4_options;
    int output_zero_padding;

    char header[132 + 1];
    
    int verbose;

    TIFF *faxTIFF;
    TIFF *inTIFF;
    TIFF *outTIFF;

    int resolution;
    int image_width;
    int row;
    int rows;
    int badrun;
    uint16_t badfaxrun;
    uint32_t badfaxlines;

    char *rowbuf;
    char *refbuf;
    
    int bit_pos;
    int bit_ptr;
    int bits;
    uint32_t bits_to_date;
    int consecutive_eols;
    
    const char *vendor;
    const char *model;
    const char *far_ident;
    const char *sub_address;
} t4_state_t;

/*! \brief Prepare for reception of a document.
    \param s The T.4 context.
    \return 0 for success, otherwise -1.
*/
int t4_rx_init(t4_state_t *s);

/*! \brief Prepare to receive the next page of the current document.
    \param s The T.4 context.
    \return zero for success, -1 for failure.
*/
int t4_rx_start_page(t4_state_t *s);

/*! \brief Put a bit of the current document page.
    \param s The T.4 context.
    \param bit The data bit.
    \return TRUE when the bit ends the document page, otherwise FALSE.
*/
int t4_rx_putbit(t4_state_t *s, int bit);

/*! \brief Complete for reception of a page.
    \param s The T.4 receive context.
    \return 0 for success, otherwise -1.
*/
int t4_rx_end_page(t4_state_t *s);

/*! \brief End reception of a document. Tidy up and close the file.
    \param s The T.4 context.
    \return 0 for success, otherwise -1.
*/
int t4_rx_end(t4_state_t *s);

/*! \brief Set the sub-address of the fax, for inclusion in the file.
    \param s The T.4 context.
    \param sub_address The sub-address string.
*/
void t4_rx_set_sub_address(t4_state_t *s, const char *sub_address);

/*! \brief Set the identity of the remote machine, for inclusion in the file.
    \param s The T.4 context.
    \param ident The identity string.
*/
void t4_rx_set_far_ident(t4_state_t *s, const char *ident);

/*! \brief Set the vendor of the remote machine, for inclusion in the file.
    \param s The T.4 context.
    \param vendor The vendor string, or NULL.
*/
void t4_rx_set_make(t4_state_t *s, const char *vendor);

/*! \brief Set the model of the remote machine, for inclusion in the file.
    \param s The T.4 context.
    \param model The model string, or NULL.
*/
void t4_rx_set_model(t4_state_t *s, const char *model);

/*! \brief Prepare for transmission of a document.
    \param s The T.4 context.
    \return 0 for success, otherwise -1.
*/
int t4_tx_init(t4_state_t *s);

/*! \brief Prepare to send the next page of the current document.
    \param s The T.4 context.
    \return zero for success, -1 for failure.
*/
int t4_tx_start_page(t4_state_t *s);

/*! \brief Get the next bit of the current document page. The document will
           be padded for the current minimum scan line time. If the
           file does not contain an RTC (return to control) code at
           the end of the page, one will be added.
    \param s The T.4 context.
    \return The next bit (i.e. 0 or 1). For the last bit of data, bit 1 is
            set (i.e. the returned value is 2 or 3).
*/
int t4_tx_getbit(t4_state_t *s);

/*! \brief End the transmission of a document. Tidy up and
           close the file.
    \param s The T.4 context.
    \return 0 for success, otherwise -1.
*/
int t4_tx_end(t4_state_t *s);

#endif
/*- End of file ------------------------------------------------------------*/

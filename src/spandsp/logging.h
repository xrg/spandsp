/*
 * SpanDSP - a series of DSP components for telephony
 *
 * logging.h - definitions for error and debug logging.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
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
 * $Id$
 */

/*! \file */

#if !defined(_LOGGING_H_)
#define _LOGGING_H_

/* Logging elements */
#define SPAN_LOG_SEVERITY_MASK          0x00FF
#define SPAN_LOG_SHOW_DATE              0x0100
#define SPAN_LOG_SHOW_SEVERITY          0x0200
#define SPAN_LOG_SHOW_PROTOCOL          0x0400
#define SPAN_LOG_SHOW_VARIANT           0x0800
#define SPAN_LOG_SHOW_TAG               0x1000

/* Logging severity levels */
enum
{
    SPAN_LOG_NONE                       = 0,
    SPAN_LOG_ERROR                      = 1,
    SPAN_LOG_WARNING                    = 2,
    SPAN_LOG_PROTOCOL_ERROR             = 3,
    SPAN_LOG_PROTOCOL_WARNING           = 4,
    SPAN_LOG_FLOW                       = 5,
    SPAN_LOG_FLOW_2                     = 6,
    SPAN_LOG_FLOW_3                     = 7,
    SPAN_LOG_DEBUG                      = 8,
    SPAN_LOG_DEBUG_2                    = 9,
    SPAN_LOG_DEBUG_3                    = 10
};

typedef struct
{
    int level;
    const char *tag;
    const char *protocol;
} logging_state_t;

#ifdef __cplusplus
extern "C" {
#endif

int span_log(logging_state_t *s, int level, const char *format, ...);

int span_log_buf(logging_state_t *s, int level, const char *tag, const uint8_t *buf, int len);

int span_log_init(logging_state_t *s, int level, const char *tag);

int span_log_set_protocol(logging_state_t *s, const char *protocol);

void span_set_error_handler(void (*func)(const char *text));

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

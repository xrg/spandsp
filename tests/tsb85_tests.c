/*
 * SpanDSP - a series of DSP components for telephony
 *
 * faxtester_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2005, 2006 Steve Underwood
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
 * $Id: tsb85_tests.c,v 1.1 2008/07/15 14:28:20 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "floating_fudge.h"
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <audiofile.h>

#if defined(HAVE_LIBXML_XMLMEMORY_H)
#include <libxml/xmlmemory.h>
#endif
#if defined(HAVE_LIBXML_PARSER_H)
#include <libxml/parser.h>
#endif
#if defined(HAVE_LIBXML_XINCLUDE_H)
#include <libxml/xinclude.h>
#endif

#include "spandsp.h"
#include "fax_tester.h"

#define INPUT_TIFF_FILE_NAME    "../test-data/itu/fax/itutests.tif"
#define OUTPUT_TIFF_FILE_NAME   "tsb85.tif"

#define OUT_FILE_NAME           "tsb85.wav"

#define SAMPLES_PER_CHUNK       160

AFfilehandle outhandle;
AFfilesetup filesetup;

int use_receiver_not_ready = FALSE;
int test_local_interrupt = FALSE;

static int phase_b_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    const char *u;
    
    i = (intptr_t) user_data;
    if ((u = t30_get_rx_ident(s)))
        printf("%d: Phase B: remote ident '%s'\n", i, u);
    if ((u = t30_get_rx_sub_address(s)))
        printf("%d: Phase B: remote sub-address '%s'\n", i, u);
    if ((u = t30_get_rx_polled_sub_address(s)))
        printf("%d: Phase B: remote polled sub-address '%s'\n", i, u);
    if ((u = t30_get_rx_selective_polling_address(s)))
        printf("%d: Phase B: remote selective polling address '%s'\n", i, u);
    if ((u = t30_get_rx_sender_ident(s)))
        printf("%d: Phase B: remote sender ident '%s'\n", i, u);
    if ((u = t30_get_rx_password(s)))
        printf("%d: Phase B: remote password '%s'\n", i, u);
    printf("%d: Phase B handler on channel %d - (0x%X) %s\n", i, i, result, t30_frametype(result));
    return T30_ERR_OK;
}
/*- End of function --------------------------------------------------------*/

static int phase_d_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    t30_stats_t t;
    const char *u;

    i = (intptr_t) user_data;
    t30_get_transfer_statistics(s, &t);

    printf("%d: Phase D handler on channel %d - (0x%X) %s\n", i, i, result, t30_frametype(result));
    printf("%d: Phase D: bit rate %d\n", i, t.bit_rate);
    printf("%d: Phase D: ECM %s\n", i, (t.error_correcting_mode)  ?  "on"  :  "off");
    printf("%d: Phase D: pages transferred %d\n", i, t.pages_transferred);
    printf("%d: Phase D: pages in the file %d\n", i, t.pages_in_file);
    printf("%d: Phase D: image size %d x %d\n", i, t.width, t.length);
    printf("%d: Phase D: image resolution %d x %d\n", i, t.x_resolution, t.y_resolution);
    printf("%d: Phase D: bad rows %d\n", i, t.bad_rows);
    printf("%d: Phase D: longest bad row run %d\n", i, t.longest_bad_row_run);
    printf("%d: Phase D: compression type %d\n", i, t.encoding);
    printf("%d: Phase D: image size %d bytes\n", i, t.image_size);
    if ((u = t30_get_tx_ident(s)))
        printf("%d: Phase D: local ident '%s'\n", i, u);
    if ((u = t30_get_rx_ident(s)))
        printf("%d: Phase D: remote ident '%s'\n", i, u);
    printf("%d: Phase D: bits per row - min %d, max %d\n", i, s->t4.min_row_bits, s->t4.max_row_bits);

    if (use_receiver_not_ready)
        t30_set_receiver_not_ready(s, 3);

    if (test_local_interrupt)
    {
        if (i == 0)
        {
            printf("%d: Initiating interrupt request\n", i);
            t30_local_interrupt_request(s, TRUE);
        }
        else
        {
            switch (result)
            {
            case T30_PIP:
            case T30_PRI_MPS:
            case T30_PRI_EOM:
            case T30_PRI_EOP:
                printf("%d: Accepting interrupt request\n", i);
                t30_local_interrupt_request(s, TRUE);
                break;
            case T30_PIN:
                break;
            }
        }
    }
    return T30_ERR_OK;
}
/*- End of function --------------------------------------------------------*/

static void phase_e_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    t30_stats_t t;
    const char *u;
    
    i = (intptr_t) user_data;
    printf("%d: Phase E handler on channel %d - (%d) %s\n", i, i, result, t30_completion_code_to_str(result));    
    t30_get_transfer_statistics(s, &t);
    printf("%d: Phase E: bit rate %d\n", i, t.bit_rate);
    printf("%d: Phase E: ECM %s\n", i, (t.error_correcting_mode)  ?  "on"  :  "off");
    printf("%d: Phase E: pages transferred %d\n", i, t.pages_transferred);
    printf("%d: Phase E: pages in the file %d\n", i, t.pages_in_file);
    printf("%d: Phase E: image size %d x %d\n", i, t.width, t.length);
    printf("%d: Phase E: image resolution %d x %d\n", i, t.x_resolution, t.y_resolution);
    printf("%d: Phase E: bad rows %d\n", i, t.bad_rows);
    printf("%d: Phase E: longest bad row run %d\n", i, t.longest_bad_row_run);
    printf("%d: Phase E: coding method %s\n", i, t4_encoding_to_str(t.encoding));
    printf("%d: Phase E: image size %d bytes\n", i, t.image_size);
    //printf("%d: Phase E: local ident '%s'\n", i, info->ident);
    if ((u = t30_get_rx_ident(s)))
        printf("%d: Phase E: remote ident '%s'\n", i, u);
    if ((u = t30_get_rx_country(s)))
        printf("%d: Phase E: Remote was made in '%s'\n", i, u);
    if ((u = t30_get_rx_vendor(s)))
        printf("%d: Phase E: Remote was made by '%s'\n", i, u);
    if ((u = t30_get_rx_model(s)))
        printf("%d: Phase E: Remote is model '%s'\n", i, u);
}
/*- End of function --------------------------------------------------------*/

static void real_time_frame_handler(t30_state_t *s,
                                    void *user_data,
                                    int direction,
                                    const uint8_t *msg,
                                    int len)
{
    int i;
    
    i = (intptr_t) user_data;
    printf("%d: Real time frame handler on channel %d - %s, %s, length = %d\n",
           i,
           i,
           (direction)  ?  "line->T.30"  : "T.30->line",
           t30_frametype(msg[2]),
           len);
}
/*- End of function --------------------------------------------------------*/

static int document_handler(t30_state_t *s, void *user_data, int event)
{
    int i;
    
    i = (intptr_t) user_data;
    printf("%d: Document handler on channel %d - event %d\n", i, i, event);
    return FALSE;
}
/*- End of function --------------------------------------------------------*/

static int next_step(faxtester_state_t *s)
{
    int delay;
    int flags;
    xmlChar *dir;
    xmlChar *type;
    xmlChar *value;

    if (s->cur == NULL)
    {
        /* Finished */
        exit(0);
    }
    while (s->cur  &&  xmlStrcmp(s->cur->name, (const xmlChar *) "step") != 0)
        s->cur = s->cur->next;
    if (s->cur == NULL)
    {
        /* Finished */
        exit(0);
    }

    dir = xmlGetProp(s->cur, (const xmlChar *) "dir");
    type = xmlGetProp(s->cur, (const xmlChar *) "type");
    value = xmlGetProp(s->cur, (const xmlChar *) "value");

    s->cur = s->cur->next;

    printf("Dir - %s, type - %s, value - %s\n", (dir)  ?  (const char *) dir  :  "", (type)  ?  (const char *) type  :  "", (value)  ?  (const char *) value  :  "");
    if (type == NULL)
        return 1;
    /*endif*/
    if (dir  &&  strcasecmp((const char *) dir, "R") == 0)
    {
        return 0;
    }
    else
    {
        if (strcasecmp((const char *) type, "CALL") == 0)
        {
            return 0;
        }
        else if (strcasecmp((const char *) type, "ANSWER") == 0)
        {
            return 0;
        }
        else if (strcasecmp((const char *) type, "CNG") == 0)
        {
            faxtester_set_rx_type(s, T30_MODEM_NONE, FALSE, FALSE);
            faxtester_set_tx_type(s, T30_MODEM_CNG, FALSE, FALSE);
        }
        else if (strcasecmp((const char *) type, "CED") == 0)
        {
            faxtester_set_rx_type(s, T30_MODEM_NONE, FALSE, FALSE);
            faxtester_set_tx_type(s, T30_MODEM_CED, FALSE, FALSE);
        }
        else if (strcasecmp((const char *) type, "WAIT") == 0)
        {
            if (value)
                delay = atoi(value);
            else
                delay = 1;
            faxtester_set_rx_type(s, T30_MODEM_NONE, FALSE, FALSE);
            faxtester_set_tx_type(s, T30_MODEM_PAUSE, delay, FALSE);
        }
        else if (strcasecmp((const char *) type, "SLOW_PREAMBLE") == 0)
        {
            if (value)
                flags = atoi(value);
            else
                flags = 37;
            faxtester_set_rx_type(s, T30_MODEM_NONE, FALSE, FALSE);
            faxtester_set_tx_type(s, T30_MODEM_V21, FALSE, TRUE);
            hdlc_tx_flags(&(s->hdlctx), flags);
        }
        else if (strcasecmp((const char *) type, "SLOW-POSTAMBLE") == 0)
        {
            if (value)
                flags = atoi(value);
            else
                flags = 5;
            hdlc_tx_flags(&(s->hdlctx), flags);
        }
        else if (strcasecmp((const char *) type, "MPS") == 0)
        {
        }
        else if (strcasecmp((const char *) type, "EOM") == 0)
        {
        }
        else if (strcasecmp((const char *) type, "EOP") == 0)
        {
        }
        /*endif*/
    }
    /*endif*/
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void exchange(faxtester_state_t *s)
{
    int16_t amp[SAMPLES_PER_CHUNK];
    int len;
    fax_state_t fax;
    int total_audio_time;
    const char *input_tiff_file_name;
    const char *output_tiff_file_name;

    input_tiff_file_name = INPUT_TIFF_FILE_NAME;
    output_tiff_file_name = OUTPUT_TIFF_FILE_NAME;

    total_audio_time = 0;
    fax_init(&fax, FALSE);
    fax_set_transmit_on_idle(&fax, TRUE);
    fax_set_tep_mode(&fax, TRUE);
    t30_set_tx_ident(&fax.t30_state, "1234567890");
    t30_set_tx_sub_address(&fax.t30_state, "Sub-address");
    t30_set_tx_sender_ident(&fax.t30_state, "Sender ID");
    t30_set_tx_password(&fax.t30_state, "Password");
    t30_set_tx_polled_sub_address(&fax.t30_state, "Polled sub-address");
    t30_set_tx_selective_polling_address(&fax.t30_state, "Sel polling address");
    t30_set_tx_nsf(&fax.t30_state, (const uint8_t *) "\x50\x00\x00\x00Spandsp\x00", 12);
    t30_set_ecm_capability(&fax.t30_state, TRUE);
    t30_set_supported_t30_features(&fax.t30_state,
                                   T30_SUPPORT_IDENTIFICATION
                                 | T30_SUPPORT_SELECTIVE_POLLING
                                 | T30_SUPPORT_SUB_ADDRESSING);
    t30_set_supported_image_sizes(&fax.t30_state,
                                  T30_SUPPORT_US_LETTER_LENGTH
                                | T30_SUPPORT_US_LEGAL_LENGTH
                                | T30_SUPPORT_UNLIMITED_LENGTH
                                | T30_SUPPORT_215MM_WIDTH
                                | T30_SUPPORT_255MM_WIDTH
                                | T30_SUPPORT_303MM_WIDTH);
    t30_set_supported_resolutions(&fax.t30_state,
                                  T30_SUPPORT_STANDARD_RESOLUTION
                                | T30_SUPPORT_FINE_RESOLUTION
                                | T30_SUPPORT_SUPERFINE_RESOLUTION
                                | T30_SUPPORT_R8_RESOLUTION
                                | T30_SUPPORT_R16_RESOLUTION
                                | T30_SUPPORT_300_300_RESOLUTION
                                | T30_SUPPORT_400_400_RESOLUTION
                                | T30_SUPPORT_600_600_RESOLUTION
                                | T30_SUPPORT_1200_1200_RESOLUTION
                                | T30_SUPPORT_300_600_RESOLUTION
                                | T30_SUPPORT_400_800_RESOLUTION
                                | T30_SUPPORT_600_1200_RESOLUTION);
    t30_set_supported_modems(&fax.t30_state, T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17);
    t30_set_supported_compressions(&fax.t30_state, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION | T30_SUPPORT_T6_COMPRESSION);
    t30_set_phase_b_handler(&fax.t30_state, phase_b_handler, (void *) (intptr_t) 1);
    t30_set_phase_d_handler(&fax.t30_state, phase_d_handler, (void *) (intptr_t) 1);
    t30_set_phase_e_handler(&fax.t30_state, phase_e_handler, (void *) (intptr_t) 1);
    t30_set_real_time_frame_handler(&fax.t30_state, real_time_frame_handler, (void *) (intptr_t) 1);
    t30_set_document_handler(&fax.t30_state, document_handler, (void *) (intptr_t) 1);
    t30_set_rx_file(&fax.t30_state, output_tiff_file_name, -1);
    //t30_set_tx_file(&fax.t30_state, input_tiff_file_name, -1, -1);

    span_log_set_level(&fax.t30_state.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(&fax.t30_state.logging, "A");
    span_log_set_level(&fax.v29_rx.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(&fax.v29_rx.logging, "A");
    span_log_set_level(&fax.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(&fax.logging, "A");

    while (next_step(s) == 0)
        ;
    for (;;)
    {
        len = fax_tx(&fax, amp, SAMPLES_PER_CHUNK);
        total_audio_time += SAMPLES_PER_CHUNK;
        span_log_bump_samples(&fax.t30_state.logging, len);
        span_log_bump_samples(&fax.v29_rx.logging, len);
        span_log_bump_samples(&fax.logging, len);
        faxtester_rx(s, amp, len);
                
        len = faxtester_tx(s, amp, 160);
        if (fax_rx(&fax, amp, len))
            break;
    }
}
/*- End of function --------------------------------------------------------*/

static int parse_test_group(faxtester_state_t *s, xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr cur, const char *test)
{
    xmlChar *x;

    while (cur)
    {
        if (xmlStrcmp(cur->name, (const xmlChar *) "test") == 0)
        {
            if ((x = xmlGetProp(cur, (const xmlChar *) "name")))
            {
                if (xmlStrcmp(x, (const xmlChar *) test) == 0)
                {
                    printf("Found '%s'\n", (char *) x);
                    s->cur = cur->xmlChildrenNode;
                    return 0;
                }
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
    return -1;
}
/*- End of function --------------------------------------------------------*/

static int get_test_set(faxtester_state_t *s, const char *test_file, const char *test)
{
    xmlDocPtr doc;
    xmlNsPtr ns;
    xmlNodePtr cur;
#if 0
    xmlValidCtxt valid;
#endif

    ns = NULL;    
    xmlKeepBlanksDefault(0);
    xmlCleanupParser();
    doc = xmlParseFile(test_file);
    if (doc == NULL)
    {
        fprintf(stderr, "No document\n");
        exit(2);
    }
    /*endif*/
    xmlXIncludeProcess(doc);
#if 0
    if (!xmlValidateDocument(&valid, doc))
    {
        fprintf(stderr, "Invalid document\n");
        exit(2);
    }
    /*endif*/
#endif
    /* Check the document is of the right kind */
    if ((cur = xmlDocGetRootElement(doc)) == NULL)
    {
        fprintf(stderr, "Empty document\n");
        xmlFreeDoc(doc);
        exit(2);
    }
    /*endif*/
    if (xmlStrcmp(cur->name, (const xmlChar *) "fax-tests"))
    {
        fprintf(stderr, "Document of the wrong type, root node != fax-tests");
        xmlFreeDoc(doc);
        exit(2);
    }
    /*endif*/
    cur = cur->xmlChildrenNode;
    while (cur  &&  xmlIsBlankNode(cur))
        cur = cur->next;
    /*endwhile*/
    if (cur == NULL)
        exit(2);
    /*endif*/
    while (cur)
    {
        if (xmlStrcmp(cur->name, (const xmlChar *) "test-group") == 0)
        {
            if (parse_test_group(s, doc, ns, cur->xmlChildrenNode, test) == 0)
            {
                /* We found the test we want, so run it. */
                exchange(s);
                break;
            }
            /*endif*/
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
    xmlFreeDoc(doc);
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    faxtester_state_t state;

    if ((filesetup = afNewFileSetup ()) == AF_NULL_FILESETUP)
    {
    	fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, 8000.0);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    if ((outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    faxtester_init(&state, TRUE);
#if defined(HAVE_LIBXML2)
    get_test_set(&state, "../spandsp/tsb85.xml", "MRGN01");
#endif
    if (afCloseFile (outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    afFreeFileSetup(filesetup);
    printf("Done\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

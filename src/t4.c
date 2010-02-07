/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4.c - ITU T.4 FAX image processing
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
 * $Id: t4.c,v 1.14 2004/03/21 13:02:00 steveu Exp $
 */

/*! \file */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>

#include <tiffiop.h>

#include "spandsp/telephony.h"
#include "spandsp/t4.h"

static tsize_t fakeread(thandle_t handle, tdata_t data, tsize_t len)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static tsize_t fakewrite(thandle_t handle, tdata_t data, tsize_t len)
{
    return len;
}
/*- End of function --------------------------------------------------------*/

static toff_t fakeseek(thandle_t handle, toff_t off, int whence)
{
    return off;
}
/*- End of function --------------------------------------------------------*/

static int fakeclose(thandle_t handle)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static toff_t fakesize(thandle_t handle)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int fakemap(thandle_t handle, tdata_t *data, toff_t *off)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void fakeunmap(thandle_t handle, tdata_t data, toff_t off)
{
}
/*- End of function --------------------------------------------------------*/

int t4_rx_init(t4_state_t *s)
{
    uint32_t input_t4_options;
    int input_compression;
    int output_coding;

    fprintf(stderr, "Start rx document - compression %d\n", s->remote_compression);
    if (s->rx_file[0] == '\0')
        return -1;
    s->outTIFF = TIFFOpen(s->rx_file, "w");
    if (s->outTIFF == NULL)
        return -1;

    output_coding = 2;
    /* Smuggle a descriptor out of the library */
    s->faxTIFF = TIFFClientOpen("(FakeInput)",
                                "w",
                                (thandle_t) -1,
                                fakeread,
                                fakewrite,
                                fakeseek,
                                fakeclose,
                                fakesize,
                                fakemap,
                                fakeunmap);
    if (s->faxTIFF == NULL)
        return -1;
    s->faxTIFF->tif_mode = O_RDONLY;

    s->x_resolution = 77.0;
    switch (s->resolution)
    {
    case 0:
        s->y_resolution = 38.5;
        break;
    case 1:
        s->y_resolution = 77.0;
        break;
    case 2:
        s->y_resolution = 154.0;
        break;
    }
    TIFFSetField(s->faxTIFF, TIFFTAG_IMAGEWIDTH, s->image_width);
    TIFFSetField(s->faxTIFF, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(s->faxTIFF, TIFFTAG_BITSPERSAMPLE, 1);
    TIFFSetField(s->faxTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(s->faxTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
    TIFFSetField(s->faxTIFF, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    TIFFSetField(s->faxTIFF, TIFFTAG_RESOLUTIONUNIT, RESUNIT_CENTIMETER);
    TIFFSetField(s->faxTIFF, TIFFTAG_XRESOLUTION, s->x_resolution);
    TIFFSetField(s->faxTIFF, TIFFTAG_YRESOLUTION, s->y_resolution);
    input_t4_options = 0;
    input_compression = COMPRESSION_CCITT_T4;
    if (s->remote_compression == 1)
    {
        /* Generate 1D-encoded output */
    }
    else if (s->remote_compression == 2)
    {
        /* Generate 2D-encoded output */
        input_t4_options |= GROUP3OPT_2DENCODING;
    }
    else if (s->remote_compression == 4)
    {
        /* Generate G4-encoded output */
        input_t4_options |= GROUP3OPT_2DENCODING;
        input_compression = COMPRESSION_CCITT_T6;
    }
    TIFFSetField(s->faxTIFF, TIFFTAG_COMPRESSION, input_compression);
    if (input_compression == COMPRESSION_CCITT_T4)
        TIFFSetField(s->faxTIFF, TIFFTAG_T4OPTIONS, input_t4_options);

    TIFFSetField(s->faxTIFF, TIFFTAG_FAXMODE, FAXMODE_CLASSIC);

    s->output_t4_options = GROUP3OPT_FILLBITS | GROUP3OPT_2DENCODING;
    s->output_compression = COMPRESSION_CCITT_T4;
    if (output_coding == 1)
    {
        /* Generate 1D-encoded output */
    }
    else if (output_coding == 2)
    {
        /* Generate 2D-encoded output */
        s->output_t4_options |= GROUP3OPT_2DENCODING;
    }
    else if (output_coding == 4)
    {
        /* Generate G4-encoded output */
        s->output_t4_options |= GROUP3OPT_2DENCODING;
        s->output_compression = COMPRESSION_CCITT_T6;
    }
    if (s->output_zero_padding)
    {
        /* zero pad output scanline EOLs */
        s->output_t4_options &= ~GROUP3OPT_FILLBITS;
    }
    s->pages_transferred = 0;

    s->rowbuf = malloc(TIFFhowmany(s->image_width, 8));
    s->refbuf = malloc(TIFFhowmany(s->image_width, 8));
    return 0;
}
/*- End of function --------------------------------------------------------*/

int t4_rx_start_page(t4_state_t *s)
{
    float res;
    uint16_t resunit;
    time_t now;
    struct tm *tm;
    char buf[256 + 1];

    fprintf(stderr, "Start rx page\n");
    s->x_resolution = 77.0;
    switch (s->resolution)
    {
    case 0:
        s->y_resolution = 38.5;
        break;
    case 1:
        s->y_resolution = 77.0;
        break;
    case 2:
        s->y_resolution = 154.0;
        break;
    }
    TIFFSetField(s->faxTIFF, TIFFTAG_XRESOLUTION, s->x_resolution);
    TIFFSetField(s->faxTIFF, TIFFTAG_YRESOLUTION, s->y_resolution);

    TIFFSetField(s->outTIFF, TIFFTAG_COMPRESSION, s->output_compression);
    if (s->output_compression == COMPRESSION_CCITT_T4)
    {
        TIFFSetField(s->outTIFF, TIFFTAG_T4OPTIONS, s->output_t4_options);
        TIFFSetField(s->outTIFF, TIFFTAG_FAXMODE, FAXMODE_CLASSF);
    }
    TIFFSetField(s->outTIFF, TIFFTAG_IMAGEWIDTH, s->image_width);
    TIFFSetField(s->outTIFF, TIFFTAG_BITSPERSAMPLE, 1);
    TIFFSetField(s->outTIFF, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(s->outTIFF, TIFFTAG_SAMPLESPERPIXEL, 1);
    if (s->output_compression == COMPRESSION_CCITT_T4
        ||
        s->output_compression == COMPRESSION_CCITT_T6)
    {
        TIFFSetField(s->outTIFF, TIFFTAG_ROWSPERSTRIP, -1L);
    }
    else
    {
        TIFFSetField(s->outTIFF,
                     TIFFTAG_ROWSPERSTRIP,
                     TIFFDefaultStripSize(s->outTIFF, 0));
    }
    TIFFSetField(s->outTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(s->outTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
    TIFFSetField(s->outTIFF, TIFFTAG_FILLORDER, FILLORDER_LSB2MSB);

    TIFFGetField(s->faxTIFF, TIFFTAG_XRESOLUTION, &res);
    TIFFSetField(s->outTIFF, TIFFTAG_XRESOLUTION, res);
    TIFFGetField(s->faxTIFF, TIFFTAG_YRESOLUTION, &res);
    TIFFSetField(s->outTIFF, TIFFTAG_YRESOLUTION, res);
    TIFFGetField(s->faxTIFF, TIFFTAG_RESOLUTIONUNIT, &resunit);
    TIFFSetField(s->outTIFF, TIFFTAG_RESOLUTIONUNIT, resunit);

    /* TODO: add the version of spandsp */
    TIFFSetField(s->outTIFF, TIFFTAG_SOFTWARE, "spandsp");
    if (gethostname(buf, sizeof(buf)) == 0)
        TIFFSetField(s->outTIFF, TIFFTAG_HOSTCOMPUTER, buf);
    time(&now);
    tm = localtime(&now);
    sprintf(buf,
    	    "%4d/%02d/%02d %02d:%02d:%02d",
            tm->tm_year + 1900,
            tm->tm_mon + 1,
            tm->tm_mday,
            tm->tm_hour,
            tm->tm_min,
            tm->tm_sec);
    TIFFSetField(s->outTIFF, TIFFTAG_DATETIME, buf);
    TIFFSetField(s->outTIFF, TIFFTAG_FAXRECVTIME, buf);

    //TIFFSetField(s->outTIFF, TIFFTAG_FAXRECVPARAMS, ???);
    //TIFFSetField(s->outTIFF, TIFFTAG_FAXMODE, ???);
    if (s->sub_address)
        TIFFSetField(s->outTIFF, TIFFTAG_FAXSUBADDRESS, s->sub_address);
    if (s->far_ident)
        TIFFSetField(s->outTIFF, TIFFTAG_IMAGEDESCRIPTION, s->far_ident);
    if (s->vendor)
        TIFFSetField(s->outTIFF, TIFFTAG_MAKE, s->vendor);
    if (s->model)
        TIFFSetField(s->outTIFF, TIFFTAG_MODEL, s->model);

    s->bits = 0;
    s->bits_to_date = 0;
    s->consecutive_eols = 0;

    s->faxTIFF->tif_rawcc = 0;
    s->faxTIFF->tif_rawdatasize = 200000;
    if ((s->faxTIFF->tif_rawdata = _TIFFrealloc(s->faxTIFF->tif_rawdata, s->faxTIFF->tif_rawdatasize)) == NULL)
    {
        fprintf(stderr, "Oh dear!\n");
        return -1;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int t4_rx_putbit(t4_state_t *s, int bit)
{
    tdata_t ptr;
    
    s->bits_to_date = (s->bits_to_date << 1) | (bit & 1);
    if (++s->bits >= 8)
    {
        s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = s->bits_to_date & 0xFF;
        if (s->faxTIFF->tif_rawcc >= s->faxTIFF->tif_rawdatasize)
        {
            s->faxTIFF->tif_rawdatasize += 200000;
            if ((ptr = _TIFFrealloc(s->faxTIFF->tif_rawdata, s->faxTIFF->tif_rawdatasize)) == NULL)
                fprintf(stderr, "Oh dear!\n");
            else
                s->faxTIFF->tif_rawdata = ptr;
        }
        s->bits = 0;
    }
    if ((s->remote_compression == 1  &&  (s->bits_to_date & 0xFFF) == 0x001)
        ||
        (s->remote_compression == 2  &&  (s->bits_to_date & 0x1FFF) == 0x0003))
    {
        if ((s->remote_compression == 1  &&  (s->bits_to_date & 0xFFFFFF) == 0x001001)
            ||
            (s->remote_compression == 2  &&  (s->bits_to_date & 0x3FFFFFF) == 0x0006003))
        {
            if (++s->consecutive_eols >= 5)
            {
                /* We have found 6 EOLs in a row, so we are at the end of a page. */
                /* Back up to remove the EOLs - i.e. 72 or 78 bits. The TIFF file does
                   not have any EOL at the end of a page. */
                if (s->remote_compression == 1)
                    s->bits = 72 - s->bits;
                else
                    s->bits = 78 - s->bits;
                s->faxTIFF->tif_rawcc -= (s->bits >> 3);
                /* We should now clear out the remaining fraction of a byte. However, this
                   will always be zero, since an EOL starts with 11 zeros. Therefore, we
                   can skip this. */
                t4_rx_end_page(s);
                fprintf(stderr, "Rx page end detected\n");
                return TRUE;
            }
        }
        else
        {
            s->consecutive_eols = 0;
        }
    }
    return FALSE;
}
/*- End of function --------------------------------------------------------*/

int t4_rx_end_page(t4_state_t *s)
{
    int ok;

    if (s->faxTIFF->tif_rawcc == 0)
        return -1;

    s->faxTIFF->tif_rawcp = s->faxTIFF->tif_rawdata;

    (*s->faxTIFF->tif_setupdecode)(s->faxTIFF);
    (*s->faxTIFF->tif_predecode)(s->faxTIFF, (tsample_t) 0);
    s->faxTIFF->tif_row = 0;
    s->badfaxlines = 0;
    s->badfaxrun = 0;

    _TIFFmemset(s->refbuf, 0, sizeof (s->refbuf));
    s->row = 0;
    s->badrun = 0;        /* current run of bad lines */

    while (s->faxTIFF->tif_rawcc > 0)
    {
        ok = (*s->faxTIFF->tif_decoderow)(s->faxTIFF,
                                          (tdata_t) s->rowbuf, 
                                          TIFFhowmany(s->image_width, 8),
                                          (tsample_t) 0);
        if (!ok)
        {
            s->badfaxlines++;
            s->badrun++;
            /* Fill in the bad line, by copying the last good line */
            _TIFFmemcpy(s->rowbuf, s->refbuf, TIFFhowmany(s->image_width, 8));
        }
        else
        {
            if (s->badrun > s->badfaxrun)
                s->badfaxrun = s->badrun;
            s->badrun = 0;
            _TIFFmemcpy(s->refbuf, s->rowbuf, TIFFhowmany(s->image_width, 8));
        }
        s->faxTIFF->tif_row++;

        if (TIFFWriteScanline(s->outTIFF, s->rowbuf, s->row, 0) < 0)
        {
            fprintf(stderr,
                    "%s: Write error at row %ld.\n",
                    s->outTIFF->tif_name,
                    (long) s->row);
            break;
        }
        s->row++;
    }

    if (s->badrun > s->badfaxrun)
        s->badfaxrun = s->badrun;
    TIFFSetField(s->outTIFF, TIFFTAG_IMAGELENGTH, s->row);
    /* Set the total pages to 1. For any one page document we will get this
       right. For multi-page documents we will need to come back and fill in
       the right answer when we know it. */
    TIFFSetField(s->outTIFF, TIFFTAG_PAGENUMBER, s->pages_transferred, 1);
    s->pages_transferred++;
    if (s->output_compression == COMPRESSION_CCITT_T4)
    {
        TIFFSetField(s->outTIFF, TIFFTAG_BADFAXLINES, s->badfaxlines);
        TIFFSetField(s->outTIFF,
                     TIFFTAG_CLEANFAXDATA,
                     s->badfaxlines  ?  CLEANFAXDATA_REGENERATED  :  CLEANFAXDATA_CLEAN);
        TIFFSetField(s->outTIFF, TIFFTAG_CONSECUTIVEBADFAXLINES, s->badfaxrun);
    }
    TIFFWriteDirectory(s->outTIFF);
    s->bits = 0;
    s->bits_to_date = 0;
    s->faxTIFF->tif_rawcc = 0;
    s->consecutive_eols = 0;
    if (1)//s->verbose)
    {
        fprintf(stderr, "Page %d of %s:\n", s->pages_transferred, s->outTIFF->tif_name);
        fprintf(stderr, "%d rows received\n", s->row);
        fprintf(stderr, "%ld total bad rows\n", (long) s->badfaxlines);
        fprintf(stderr, "%d max consecutive bad rows\n", s->badfaxrun);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int t4_rx_end(t4_state_t *s)
{
    int i;

    if (s->pages_transferred > 1)
    {
        /* We need to edit the TIFF directories. Until now we did not know
           the total page count, so the TIFF file currently says one. Now we
           need to set the correct total page count associated with each page. */
        for (i = 0;  i < s->pages_transferred;  i++)
        {
            TIFFSetDirectory(s->outTIFF, i);
            TIFFSetField(s->outTIFF, TIFFTAG_PAGENUMBER, i, s->pages_transferred);
            TIFFWriteDirectory(s->outTIFF);
        }
    }
    if (s->faxTIFF->tif_rawdata)
    {
        _TIFFfree(s->faxTIFF->tif_rawdata);
        s->faxTIFF->tif_rawdata = NULL;
    }
    TIFFClose(s->faxTIFF);
    TIFFClose(s->outTIFF);
    free(s->rowbuf);
    free(s->refbuf);
    return 0;
}
/*- End of function --------------------------------------------------------*/

void t4_rx_set_sub_address(t4_state_t *s, const char *sub_address)
{
    s->sub_address = (sub_address  &&  sub_address[0])  ?  sub_address  :  NULL;
}
/*- End of function --------------------------------------------------------*/

void t4_rx_set_far_ident(t4_state_t *s, const char *ident)
{
    s->far_ident = (ident  &&  ident[0])  ?  ident  :  NULL;
}
/*- End of function --------------------------------------------------------*/

void t4_rx_set_vendor(t4_state_t *s, const char *vendor)
{
    s->vendor = vendor;
}
/*- End of function --------------------------------------------------------*/

void t4_rx_set_model(t4_state_t *s, const char *model)
{
    s->model = model;
}
/*- End of function --------------------------------------------------------*/

int t4_tx_init(t4_state_t *s)
{
    float x_resolution;
    float y_resolution;
    uint16_t res_unit;
    uint32_t parm;
    uint32_t output_t4_options;
    int output_compression;

    fprintf(stderr, "Start tx document - compression %d\n", s->remote_compression);
    if (s->tx_file[0] == '\0')
        return -1;
    s->inTIFF = TIFFOpen(s->tx_file, "r");
    if (s->inTIFF == NULL)
        return -1;

    /* Smuggle a descriptor out of the library */
    s->faxTIFF = TIFFClientOpen("(FakeOutput)",
                                "w",
                                (thandle_t) -1,
                                fakeread,
                                fakewrite,
                                fakeseek,
                                fakeclose,
                                fakesize,
                                fakemap,
                                fakeunmap);
    if (s->faxTIFF == NULL)
        return -1;

    output_t4_options = GROUP3OPT_FILLBITS;
    output_compression = COMPRESSION_CCITT_T4;
    if (s->remote_compression == 1)
    {
        /* Generate 1D-encoded output */
    }
    else if (s->remote_compression == 2)
    {
        /* Generate 2D-encoded output */
        output_t4_options |= GROUP3OPT_2DENCODING;
    }
    else if (s->remote_compression == 4)
    {
        /* Generate G4-encoded output */
        output_t4_options |= GROUP3OPT_2DENCODING;
        output_compression = COMPRESSION_CCITT_T6;
    }

    TIFFSetField(s->faxTIFF, TIFFTAG_COMPRESSION, output_compression);
    if (output_compression == COMPRESSION_CCITT_T4)
    {
        TIFFSetField(s->faxTIFF, TIFFTAG_T4OPTIONS, output_t4_options);
        TIFFSetField(s->faxTIFF, TIFFTAG_FAXMODE, FAXMODE_CLASSIC);
    }
    if (output_compression == COMPRESSION_CCITT_T4
        ||
        output_compression == COMPRESSION_CCITT_T6)
    {
        TIFFSetField(s->faxTIFF, TIFFTAG_ROWSPERSTRIP, -1L);
    }
    else
    {
        TIFFSetField(s->faxTIFF,
                     TIFFTAG_ROWSPERSTRIP,
                     TIFFDefaultStripSize(s->faxTIFF, 0));
    }

    TIFFSetField(s->faxTIFF, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(s->faxTIFF, TIFFTAG_BITSPERSAMPLE, 1);
    TIFFSetField(s->faxTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(s->faxTIFF, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    TIFFSetField(s->faxTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
    TIFFSetField(s->faxTIFF, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);

    TIFFGetField(s->inTIFF, TIFFTAG_IMAGEWIDTH, &parm);
    TIFFSetField(s->faxTIFF, TIFFTAG_IMAGEWIDTH, parm);
    s->image_width = parm;
    TIFFGetField(s->inTIFF, TIFFTAG_YRESOLUTION, &x_resolution);
    TIFFSetField(s->faxTIFF, TIFFTAG_XRESOLUTION, x_resolution);
    TIFFGetField(s->inTIFF, TIFFTAG_YRESOLUTION, &y_resolution);
    TIFFSetField(s->faxTIFF, TIFFTAG_YRESOLUTION, y_resolution);
    TIFFGetField(s->inTIFF, TIFFTAG_RESOLUTIONUNIT, &res_unit);
    TIFFSetField(s->faxTIFF, TIFFTAG_RESOLUTIONUNIT, res_unit);

    if ((res_unit == RESUNIT_CENTIMETER  &&  y_resolution == 154.0)
        ||
        (res_unit == RESUNIT_INCH  &&  y_resolution == 392.0))
    {
        s->resolution = 2;
        fprintf(stderr, "Super-fine mode\n");
    }
    else if ((res_unit == RESUNIT_CENTIMETER  &&  y_resolution == 77.0)
             ||
             (res_unit == RESUNIT_INCH  &&  y_resolution == 196.0))
    {
        s->resolution = 1;
        fprintf(stderr, "Fine mode\n");
    }
    else
    {
        s->resolution = 0;
        fprintf(stderr, "Standard mode\n");
    }
    s->rowbuf = malloc(TIFFhowmany(s->image_width, 8));

    s->pages_transferred = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

int t4_tx_start_page(t4_state_t *s)
{
    uint32 row;
    int ok;
    int i;

    fprintf(stderr, "Start tx page\n");
    if (s->pages_transferred > 0)
    {
        if (!TIFFReadDirectory(s->inTIFF))
            return -1;
    }
    s->pages_transferred++;
    s->faxTIFF->tif_rawdatasize = TIFFStripSize(s->inTIFF) + 1000;
    s->faxTIFF->tif_rawcc = 0;

    if ((s->faxTIFF->tif_rawdata = _TIFFrealloc(s->faxTIFF->tif_rawdata, s->faxTIFF->tif_rawdatasize)) == NULL)
    {
        fprintf(stderr, "Oh dear!\n");
        return -1;
    }
    s->faxTIFF->tif_rawcp = s->faxTIFF->tif_rawdata;

    (*s->faxTIFF->tif_setupencode)(s->faxTIFF);
    (*s->faxTIFF->tif_preencode)(s->faxTIFF, (tsample_t) 0);
    if (s->header[0])
    {
        /* 27-SEP-2003  19:31    STEVE AND CONNIE           852 2666 0542         P.01 */
        for (row = 0;  row < 15;  row++)
        {
            if ((ok = (*s->faxTIFF->tif_encoderow)(s->faxTIFF,
                                                   (tdata_t) s->rowbuf, 
                                                   sizeof(s->rowbuf),
                                                   (tsample_t) 0)) <= 0)
            {
                fprintf(stderr,
                        "%s: Encode error at row %ld.\n",
                        s->faxTIFF->tif_name,
                        (long) row);
                break;
            }
        }
    }
    TIFFGetField(s->inTIFF, TIFFTAG_IMAGELENGTH, &s->rows);
    for (row = 0;  row < s->rows;  row++)
    {
        if ((ok = TIFFReadScanline(s->inTIFF, s->rowbuf, row, 0)) <= 0)
        {
            fprintf(stderr,
                    "%s: Write error at row %ld.\n",
                    s->inTIFF->tif_name,
                    (long) row);
            break;
        }
        if ((ok = (*s->faxTIFF->tif_encoderow)(s->faxTIFF,
                                               (tdata_t) s->rowbuf, 
                                               sizeof(s->rowbuf),
                                               (tsample_t) 0)) <= 0)
        {
            fprintf(stderr,
                    "%s: Encode error at row %ld.\n",
                    s->faxTIFF->tif_name,
                    (long) row);
            break;
        }
    }
    (*s->faxTIFF->tif_postencode)(s->faxTIFF);

    if (s->remote_compression == 1)
    {
        /* Now attach a return to control (RTC == 6 x EOLs) to the end of the page */
        for (i = 0;  i < 3;  i++)
        {
            s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x00;
            s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x10;
            s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x01;
        }
        /* And a few zero bits to make things terminate nice and cleanly. */
        for (i = 0;  i < 50;  i++)
            s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x00;
    }
    else
    {
        /* Now attach a return to control (RTC == 6 x (EOL + 1)s) to the end of the page */
        s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x00;
        s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x18;
        s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x00;
        s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0xC0;
        s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x06;
        s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x00;
        s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x30;
        s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x01;
        s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x80;
        s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x0C;
        s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x00;
        s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x60;
        /* And a few zero bits to make things terminate nice and cleanly. */
        for (i = 0;  i < 50;  i++)
            s->faxTIFF->tif_rawdata[s->faxTIFF->tif_rawcc++] = 0x00;
    }
    if (1)//s->verbose)
    {
        fprintf(stderr, "Page %d of %s\n", s->pages_transferred, s->inTIFF->tif_name);
        fprintf(stderr, "%d rows/%d bytes to send\n", s->rows, (int) s->faxTIFF->tif_rawcc);
    }
    s->bit_pos = 0;
    s->bit_ptr = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

int t4_tx_getbit(t4_state_t *s)
{
    int bit;
    
    bit = (s->faxTIFF->tif_rawdata[s->bit_ptr] >> (7 - s->bit_pos)) & 1;
    if (++s->bit_pos >= 8)
    {
        s->bit_pos = 0;
        if (++s->bit_ptr >= s->faxTIFF->tif_rawcc)
            bit |= 2;
    }
    return bit;
}
/*- End of function --------------------------------------------------------*/

int t4_tx_end(t4_state_t *s)
{
    if (s->faxTIFF->tif_rawdata)
    {
        _TIFFfree(s->faxTIFF->tif_rawdata);
        s->faxTIFF->tif_rawdata = NULL;
    }
    TIFFClose(s->inTIFF);
    TIFFClose(s->faxTIFF);
    free(s->rowbuf);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

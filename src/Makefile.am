## Makefile.am -- Process this file with automake to produce Makefile.in
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
##
## $Id: Makefile.am,v 1.24 2005/03/03 14:18:58 steveu Exp $

MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I$(top_builddir)

lib_LTLIBRARIES = libspandsp.la

libspandsp_la_SOURCES = adsi.c \
                        awgn.c \
                        bert.c \
                        complex_dds.c \
                        complex_filters.c \
                        dds.c \
                        ec_disable_detector.c \
                        echo.c \
                        fsk.c \
                        g722_encode.c \
                        g722_decode.c \
                        hdlc.c \
                        ima_adpcm.c \
                        logging.c \
                        modem_echo.c \
                        noise.c \
                        oki_adpcm.c \
                        oss.c \
                        playout.c \
                        plc.c \
                        power_meter.c \
                        queue.c \
                        schedule.c \
                        sig_tone.c \
                        super_tone_rx.c \
                        super_tone_tx.c \
                        t4.c \
                        t30.c \
                        t31.c \
                        t35.c \
                        testcpuid.c \
                        time_scale.c \
                        tone_detect.c \
                        tone_generate.c \
                        v17rx.c \
                        v17tx.c \
                        v22bis_rx.c \
                        v22bis_tx.c \
                        v27ter_rx.c \
                        v27ter_tx.c \
                        v29rx.c \
                        v29tx.c \
                        v42.c \
                        v42bis.c \
                        v8.c
libspandsp_la_LDFLAGS = -version-info @SPANDSP_LT_CURRENT@:@SPANDSP_LT_REVISION@:@SPANDSP_LT_AGE@

nobase_include_HEADERS = spandsp/adsi.h \
                        spandsp/alaw_ulaw.h \
                        spandsp/arctan2.h \
                        spandsp/awgn.h \
                        spandsp/bert.h \
                        spandsp/biquad.h \
                        spandsp/complex.h \
                        spandsp/complex_filters.h \
                        spandsp/dc_restore.h \
                        spandsp/dds.h \
                        spandsp/ec_disable_detector.h \
                        spandsp/echo.h \
                        spandsp/fir.h \
                        spandsp/fsk.h \
                        spandsp/g168models.h \
                        spandsp/g722.h \
                        spandsp/hdlc.h \
                        spandsp/ima_adpcm.h \
                        spandsp/logging.h \
                        spandsp/mmx.h \
                        spandsp/modem_echo.h \
                        spandsp/noise.h \
                        spandsp/oss.h \
                        spandsp/oki_adpcm.h \
                        spandsp/playout.h \
                        spandsp/plc.h \
                        spandsp/power_meter.h \
                        spandsp/queue.h \
                        spandsp/schedule.h \
                        spandsp/sig_tone.h \
                        spandsp/super_tone_rx.h \
                        spandsp/super_tone_tx.h \
                        spandsp/t4.h \
                        spandsp/t30.h \
                        spandsp/t30_fcf.h \
                        spandsp/t31.h \
                        spandsp/t35.h \
                        spandsp/telephony.h \
                        spandsp/time_scale.h \
                        spandsp/timing.h \
                        spandsp/tone_detect.h \
                        spandsp/tone_generate.h \
                        spandsp/v17rx.h \
                        spandsp/v17tx.h \
                        spandsp/v22bis.h \
                        spandsp/v27ter_rx.h \
                        spandsp/v27ter_tx.h \
                        spandsp/v29rx.h \
                        spandsp/v29tx.h \
                        spandsp/v42.h \
                        spandsp/v42bis.h \
                        spandsp/v8.h \
                        spandsp/vector.h \
                        spandsp.h

noinst_HEADERS =        t4states.h \
			faxfont.h


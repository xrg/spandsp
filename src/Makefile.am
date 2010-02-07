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
## $Id: Makefile.am,v 1.7 2004/03/12 16:38:47 steveu Exp $

MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I$(top_buildir)

lib_LTLIBRARIES = libspandsp.la

libspandsp_la_SOURCES = adsi.c \
                        awgn.c \
                        complex_dds.c \
                        complex_filters.c \
                        dds.c \
                        ec_disable_detector.c \
                        echo.c \
                        fsk.c \
                        hdlc.c \
                        oki_adpcm.c \
                        oss.c \
                        power_meter.c \
                        super_tone_detect.c \
                        super_tone_generate.c \
                        t4.c \
                        t30.c \
                        t35.c \
                        testcpuid.c \
                        tone_detect.c \
                        tone_generate.c \
                        v27ter_rx.c \
                        v27ter_tx.c \
                        v29rx.c \
                        v29tx.c

nobase_include_HEADERS =spandsp/adsi.h \
                        spandsp/alaw_ulaw.h \
                        spandsp/arctan2.h \
                        spandsp/awgn.h \
                        spandsp/biquad.h \
                        spandsp/complex.h \
                        spandsp/complex_dds.h \
                        spandsp/complex_filters.h \
                        spandsp/dc_restore.h \
                        spandsp/dds.h \
                        spandsp/ec_disable_detector.h \
                        spandsp/echo.h \
                        spandsp/fir.h \
                        spandsp/fsk.h \
                        spandsp/g168models.h \
                        spandsp/hdlc.h \
                        spandsp/mmx.h \
                        spandsp/oss.h \
                        spandsp/oki_adpcm.h \
                        spandsp/power_meter.h \
                        spandsp/super_tone_detect.h \
                        spandsp/super_tone_generate.h \
                        spandsp/t4.h \
                        spandsp/t30.h \
                        spandsp/t30_fcf.h \
                        spandsp/t35.h \
                        spandsp/telephony.h \
                        spandsp/timing.h \
                        spandsp/tone_detect.h \
                        spandsp/tone_generate.h \
                        spandsp/v27ter_rx.h \
                        spandsp/v27ter_tx.h \
                        spandsp/v29rx.h \
                        spandsp/v29tx.h \
                        spandsp/vector.h \
                        spandsp.h

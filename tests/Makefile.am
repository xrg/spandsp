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
## $Id: Makefile.am,v 1.9 2004/03/28 12:46:42 steveu Exp $

MAINTAINERCLEANFILES = Makefile.in

LIBS = -laudiofile -lfftw -lxml2 -ltiff -lunicall -lmfcr2 -lfx -lpthread

INCLUDES = -I$(top_builddir)/src -I/usr/include/libxml2

noinst_PROGRAMS =   adsi_tests \
                    awgn_tests \
                    bell_mf_tests \
                    dc_restore_tests \
                    dds_tests \
                    dtmf_tests \
                    dtmf_gen_tests \
                    ec_disable_detector_tests \
                    echo_tests \
                    fax_tests \
                    fsk_tests \
                    g168_tests \
                    hdlc_tests \
                    makecss \
                    power_meter_tests \
                    r2_mf_tests \
                    super_tone_detect_tests \
                    super_tone_generate_tests \
                    t4_tests \
                    tone_generate_tests \
                    v27ter_tests \
                    v29_tests \
                    testadsi \
                    testfax

adsi_tests_SOURCES = adsi_tests.c
adsi_tests_LDADD = -L$(top_builddir)/src -lspandsp

awgn_tests_SOURCES = awgn_tests.c
awgn_tests_LDADD = -L$(top_builddir)/src -lspandsp

bell_mf_tests_SOURCES = bell_mf_tests.c
bell_mf_tests_LDADD = -L$(top_builddir)/src -lspandsp

dc_restore_tests_SOURCES = dc_restore_tests.c
dc_restore_tests_LDADD = -L$(top_builddir)/src -lspandsp

dds_tests_SOURCES = dds_tests.c
dds_tests_LDADD = -L$(top_builddir)/src -lspandsp

dtmf_tests_SOURCES = dtmf_tests.c
dtmf_tests_LDADD = -L$(top_builddir)/src -lspandsp

dtmf_gen_tests_SOURCES = dtmf_gen_tests.c
dtmf_gen_tests_LDADD = -L$(top_builddir)/src -lspandsp

ec_disable_detector_tests_SOURCES = ec_disable_detector_tests.c
ec_disable_detector_tests_LDADD = -L$(top_builddir)/src -lspandsp

echo_tests_SOURCES = echo_tests.c
echo_tests_LDADD = -L$(top_builddir)/src -lspandsp

fax_tests_SOURCES = fax_tests.c
fax_tests_LDADD = -L$(top_builddir)/src -lspandsp

fsk_tests_SOURCES = fsk_tests.c
fsk_tests_LDADD = -L$(top_builddir)/src -lspandsp

g168_tests_SOURCES = g168_tests.c
g168_tests_LDADD = -L$(top_builddir)/src -lspandsp

hdlc_tests_SOURCES = hdlc_tests.c
hdlc_tests_LDADD = -L$(top_builddir)/src -lspandsp

makecss_SOURCES = makecss.c
makecss_LDADD = -L$(top_builddir)/src -lspandsp

power_meter_tests_SOURCES = power_meter_tests.c
power_meter_tests_LDADD = -L$(top_builddir)/src -lspandsp

r2_mf_tests_SOURCES = r2_mf_tests.c
r2_mf_tests_LDADD = -L$(top_builddir)/src -lspandsp

super_tone_detect_tests_SOURCES = super_tone_detect_tests.c
super_tone_detect_tests_LDADD = -L$(top_builddir)/src -lspandsp

super_tone_generate_tests_SOURCES = super_tone_generate_tests.c
super_tone_generate_tests_LDADD = -L$(top_builddir)/src -lspandsp

t4_tests_SOURCES = t4_tests.c
t4_tests_LDADD = -L$(top_builddir)/src -lspandsp

tone_generate_tests_SOURCES = tone_generate_tests.c
tone_generate_tests_LDADD = -L$(top_builddir)/src -lspandsp

v27ter_tests_SOURCES = v27ter_tests.c
v27ter_tests_LDADD = -L$(top_builddir)/src -lspandsp

v29_tests_SOURCES = v29_tests.c constel.cpp
v29_tests_LDFLAGS = `fltk-config --ldflags`
v29_tests_LDADD = -L$(top_builddir)/src -lspandsp -lfltk_cartesian -lm

testadsi_SOURCES = testadsi.c
testadsi_LDADD = -L$(top_builddir)/src -lspandsp

testfax_SOURCES = testfax.c
testfax_LDADD = -L$(top_builddir)/src -lspandsp

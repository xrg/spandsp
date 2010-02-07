# configure.ac -- Process this file with autoconf to produce configure
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# $Id: configure.ac,v 1.1 2005/09/03 10:37:56 steveu Exp $

# @start 1

AC_INIT

SPANDSP_MAJOR_VERSION=0
SPANDSP_MINOR_VERSION=0
SPANDSP_MICRO_VERSION=2

SPANDSP_LT_CURRENT=0
SPANDSP_LT_REVISION=1
SPANDSP_LT_AGE=0

VERSION=$SPANDSP_MAJOR_VERSION.$SPANDSP_MINOR_VERSION.$SPANDSP_MICRO_VERSION
PACKAGE=spandsp

AC_SUBST(SPANDSP_LT_CURRENT)
AC_SUBST(SPANDSP_LT_REVISION)
AC_SUBST(SPANDSP_LT_AGE)

AC_CONFIG_SRCDIR([src/tone_generate.c])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADERS([src/config.h:config-h.in])
AM_INIT_AUTOMAKE($PACKAGE, $VERSION)

AC_PROG_CC
AC_PROG_CXX
AC_PROG_GCC_TRADITIONAL
AC_PROG_LIBTOOL

AC_LANG([C])

AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE

AC_TYPE_SIGNAL

AC_ARG_ENABLE(doc,     [  --enable-doc      Build the documentation])
AC_ARG_ENABLE(tests,   [  --enable-tests    Build the test programs])
AC_ARG_ENABLE(itutests,[  --enable-itutests Build TIFF test files for some ITU test images])

AC_FUNC_ERROR_AT_LINE
AC_FUNC_VPRINTF
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES

AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([pow])
AC_CHECK_FUNCS([sqrt])
AC_CHECK_FUNCS([rint])
AC_CHECK_FUNCS([select])
AC_CHECK_FUNCS([strcasecmp])
AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strerror])
AC_CHECK_FUNCS([strstr])
AC_CHECK_FUNCS([strtol])
AC_CHECK_FUNCS([gettimeofday])

AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_TIME

# Check for header files.
AC_CHECK_HEADERS([socket.h])
AC_CHECK_HEADERS([inttypes.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([strings.h])
AC_CHECK_HEADERS([malloc.h])
AC_CHECK_HEADERS([float.h])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([sys/select.h])
AC_CHECK_HEADERS([sys/ioctl.h])
AC_CHECK_HEADERS([sys/fcntl.h])
AC_CHECK_HEADERS([sys/soundcard.h])
AC_CHECK_HEADERS([audiofile.h])
AC_CHECK_HEADERS([fftw.h])
AC_CHECK_HEADERS([fftw3.h])
AC_CHECK_HEADERS([tiffio.h])
AC_CHECK_HEADERS([pthread.h])
AC_CHECK_HEADERS([unicall.h])
AC_CHECK_HEADERS([X11/X.h])

AC_LANG([C++])
AC_CHECK_HEADERS([FL/Fl.H])
AC_CHECK_HEADERS([FL/Fl_Overlay_Window.H])
AC_CHECK_HEADERS([FL/Fl_Light_Button.H])
AC_CHECK_HEADERS([FL/fl_draw.H])
AC_CHECK_HEADERS([FL/Fl_Cartesian.H], [TESTLIBS="$TESTLIBS -lfltk_cartesian -lfltk -lsupc++"], [], [],[[#include <FL/Fl.H>
]])

AC_LANG([C])
# X86_64 Linux machines have both 64 bit and 32 bit libraries. We need to choose the right set
AC_CHECK_FILE([/usr/X11R6/lib64], [TESTLIBS="$TESTLIBS -L/usr/X11R6/lib64"], AC_CHECK_FILE([/usr/X11R6/lib], [TESTLIBS="$TESTLIBS -L/usr/X11R6/lib"]))

# Checks for libraries.
AC_CHECK_LIB([Xft], [XftFontOpen], TESTLIBS="$TESTLIBS -lXft",, $TESTLIBS)
AC_CHECK_LIB([Xext], [XextCreateExtension], TESTLIBS="$TESTLIBS -lXext",, $TESTLIBS)
AC_CHECK_LIB([X11], [XOpenDisplay], TESTLIBS="$TESTLIBS -lX11",, $TESTLIBS)
AC_CHECK_LIB([tiff], [TIFFOpen],,echo "Can't build without libtiff"; exit 1, -lm)
AC_CHECK_LIB([m], [cos])
AC_CHECK_LIB([xml2], [xmlParseFile], TESTLIBS="$TESTLIBS -lxml2")
if test -n "$enable_tests" ; then
    AC_CHECK_LIB([audiofile], [afOpenFile], TESTLIBS="$TESTLIBS -laudiofile",echo "Can't make tests without libaudiofile"; exit 1)
    AC_CHECK_LIB([fftw3], [fftw_plan_dft_1d], TESTLIBS="$TESTLIBS -lfftw3")
    AC_CHECK_LIB([fftw], [fftw_create_plan], TESTLIBS="$TESTLIBS -lfftw")
    AC_CHECK_LIB([pthread], [pthread_attr_init], TESTLIBS="$TESTLIBS -lpthread")
    AC_CHECK_LIB([dl], [dlopen], TESTLIBS="$TESTLIBS -ldl")
    AC_CHECK_LIB([unicall], [uc_start], TESTLIBS="$TESTLIBS -lunicall",, -ltiff -ldl)
fi

AM_CONDITIONAL([COND_DOC], [test "$enable_doc" = yes])
AM_CONDITIONAL([COND_TESTS], [test "$enable_tests" = yes])
AM_CONDITIONAL([COND_ITUTESTS], [test "$enable_itutests" = yes])

AC_SUBST(TESTLIBS, $TESTLIBS)

AC_CONFIG_FILES([Makefile
                 doc/Makefile
                 itutests/Makefile
                 itutests/fax/Makefile
                 src/Makefile
                 tests/Makefile
                 spandsp.spec])

AC_OUTPUT

# @end 1

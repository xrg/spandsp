##
## SpanDSP - a series of DSP components for telephony
##
## Makefile.am -- Process this file with automake to produce Makefile.in
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License version 2, as
## published by the Free Software Foundation.
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
## $Id: Makefile.am,v 1.7 2007/12/20 15:06:22 steveu Exp $

MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = css.css \
             t38-gateway.dia \
             t38-terminal.dia \
             t38_manual.xml \
             wrapper.xsl \
             t38_manual/css.css

all: doxydocs t38_manual/index.html

doxydocs:
	doxygen libspandsp-doxygen

t38_manual/index.html: t38_manual.xml
	cd t38_manual ; xsltproc ../wrapper.xsl ../t38_manual.xml

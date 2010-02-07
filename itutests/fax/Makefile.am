#
# Makefile for building TIFF files from the ITU test pages
#
# $Id: Makefile.am,v 1.1 2005/12/28 04:25:28 steveu Exp $
#

PBM2G3 = pbmtog3
FAX2TIFF = fax2tiff

EXTRA_DIST = itu1.pbm itu2.pbm itu3.pbm itu4.pbm itu5.pbm itu6.pbm itu7.pbm itu8.pbm \
               test1.pbm test2.pbm test3.pbm test4.pbm

all: itu1.tif itu2.tif itu3.tif itu4.tif itu5.tif itu6.tif itu7.tif itu8.tif \
     test1.tif test2.tif test3.tif test4.tif

clean:
	rm -f *.tif *.g3

itu1.tif: itu1.g3
	${FAX2TIFF} -M -o itu1.tif itu1.g3

itu1.g3: itu1.pbm
	${PBM2G3} itu1.pbm >itu1.g3

itu2.tif: itu2.g3
	${FAX2TIFF} -M -o itu2.tif itu2.g3

itu2.g3: itu2.pbm
	${PBM2G3} itu2.pbm >itu2.g3

itu3.tif: itu3.g3
	${FAX2TIFF} -M -o itu3.tif itu3.g3

itu3.g3: itu3.pbm
	${PBM2G3} itu3.pbm >itu3.g3

itu4.tif: itu4.g3
	${FAX2TIFF} -M -o itu4.tif itu4.g3

itu4.g3: itu4.pbm
	${PBM2G3} itu4.pbm >itu4.g3

itu5.tif: itu5.g3
	${FAX2TIFF} -M -o itu5.tif itu5.g3

itu5.g3: itu5.pbm
	${PBM2G3} itu5.pbm >itu5.g3

itu6.tif: itu6.g3
	${FAX2TIFF} -M -o itu6.tif itu6.g3

itu6.g3: itu6.pbm
	${PBM2G3} itu6.pbm >itu6.g3

itu7.tif: itu7.g3
	${FAX2TIFF} -M -o itu7.tif itu7.g3

itu7.g3: itu7.pbm
	${PBM2G3} itu7.pbm >itu7.g3

itu8.tif: itu8.g3
	${FAX2TIFF} -M -o itu8.tif itu8.g3

itu8.g3: itu8.pbm
	${PBM2G3} itu8.pbm >itu8.g3

test1.tif: test1.g3
	${FAX2TIFF} -M -o test1.tif test1.g3

test1.g3: test1.pbm
	${PBM2G3} test1.pbm >test1.g3

test2.tif: test2.g3
	${FAX2TIFF} -M -o test2.tif test2.g3

test2.g3: test2.pbm
	${PBM2G3} test2.pbm >test2.g3

test3.tif: test3.g3
	${FAX2TIFF} -M -o test3.tif test3.g3

test3.g3: test3.pbm
	${PBM2G3} test3.pbm >test3.g3

test4.tif: test4.g3
	${FAX2TIFF} -M -o test4.tif test4.g3

test4.g3: test4.pbm
	${PBM2G3} test4.pbm >test4.g3

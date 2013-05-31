lib_LTLIBRARIES = libjpeg.la
libjpeg_la_LDFLAGS = -version-info ${LIBTOOL_CURRENT}:${SO_MINOR_VERSION}:${SO_AGE} -no-undefined
include_HEADERS = jerror.h jmorecfg.h jpeglib.h

if WITH_TURBOJPEG
lib_LTLIBRARIES += libturbojpeg.la
libturbojpeg_la_LDFLAGS = -version-info 0:0 -no-undefined
include_HEADERS += turbojpeg.h
endif

nodist_include_HEADERS = jconfig.h


HDRS = jchuff.h jdct.h jdhuff.h jerror.h jinclude.h jmemsys.h jmorecfg.h \
	jpegint.h jpeglib.h jversion.h jsimd.h jsimddct.h jpegcomp.h

libjpeg_la_SOURCES = $(HDRS) jcapimin.c jcapistd.c jccoefct.c jccolor.c \
	jcdctmgr.c jchuff.c jcinit.c jcmainct.c jcmarker.c jcmaster.c \
	jcomapi.c jcparam.c jcphuff.c jcprepct.c jcsample.c jctrans.c \
	jdapimin.c jdapistd.c jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c \
	jddctmgr.c jdhuff.c jdinput.c jdmainct.c jdmarker.c jdmaster.c \
	jdmerge.c jdphuff.c jdpostct.c jdsample.c jdtrans.c jerror.c \
	jfdctflt.c jfdctfst.c jfdctint.c jidctflt.c jidctfst.c jidctint.c \
	jidctred.c jquant1.c jquant2.c jutils.c jmemmgr.c jmemnobs.c

if WITH_ARITH
libjpeg_la_SOURCES += jaricom.c
endif

if WITH_ARITH_ENC
libjpeg_la_SOURCES += jcarith.c
endif

if WITH_ARITH_DEC
libjpeg_la_SOURCES += jdarith.c
endif


SUBDIRS = java


if WITH_TURBOJPEG

libturbojpeg_la_SOURCES = $(libjpeg_la_SOURCES) turbojpeg.c turbojpeg.h \
	transupp.c transupp.h jdatadst-tj.c jdatasrc-tj.c

if WITH_JAVA

libturbojpeg_la_SOURCES += turbojpeg-jni.c
libturbojpeg_la_CFLAGS = ${JNI_CFLAGS}
TJMAPFILE = turbojpeg-mapfile.jni

else

TJMAPFILE = turbojpeg-mapfile

endif

libturbojpeg_la_SOURCES += $(TJMAPFILE)

if VERSION_SCRIPT
libturbojpeg_la_LDFLAGS += $(VERSION_SCRIPT_FLAG)$(srcdir)/$(TJMAPFILE)
endif

endif


if VERSION_SCRIPT
libjpeg_la_LDFLAGS += $(VERSION_SCRIPT_FLAG)libjpeg.map
endif


if WITH_SIMD

SUBDIRS += simd
libjpeg_la_LIBADD = simd/libsimd.la
libturbojpeg_la_LIBADD = simd/libsimd.la

else

libjpeg_la_SOURCES += jsimd_none.c

endif


bin_PROGRAMS = cjpeg djpeg jpegtran rdjpgcom wrjpgcom
noinst_PROGRAMS = jcstest


if WITH_TURBOJPEG

bin_PROGRAMS += tjbench

noinst_PROGRAMS += tjunittest

tjbench_SOURCES = tjbench.c bmp.h bmp.c tjutil.h tjutil.c rdbmp.c rdppm.c \
	wrbmp.c wrppm.c

tjbench_LDADD = libturbojpeg.la libjpeg.la -lm

tjbench_CFLAGS = -DBMP_SUPPORTED -DPPM_SUPPORTED

tjunittest_SOURCES = tjunittest.c tjutil.h tjutil.c

tjunittest_LDADD = libturbojpeg.la

endif


cjpeg_SOURCES = cdjpeg.h cderror.h cdjpeg.c cjpeg.c rdbmp.c rdgif.c \
	rdppm.c rdswitch.c rdtarga.c 

cjpeg_LDADD = libjpeg.la

cjpeg_CFLAGS = -DBMP_SUPPORTED -DGIF_SUPPORTED -DPPM_SUPPORTED \
	-DTARGA_SUPPORTED

djpeg_SOURCES = cdjpeg.h cderror.h cdjpeg.c djpeg.c rdcolmap.c rdswitch.c \
	wrbmp.c wrgif.c wrppm.c wrtarga.c

djpeg_LDADD = libjpeg.la

djpeg_CFLAGS = -DBMP_SUPPORTED -DGIF_SUPPORTED -DPPM_SUPPORTED \
	-DTARGA_SUPPORTED

jpegtran_SOURCES = jpegtran.c rdswitch.c cdjpeg.c transupp.c transupp.h

jpegtran_LDADD = libjpeg.la

rdjpgcom_SOURCES = rdjpgcom.c

rdjpgcom_LDADD = libjpeg.la

wrjpgcom_SOURCES = wrjpgcom.c

wrjpgcom_LDADD = libjpeg.la

jcstest_SOURCES = jcstest.c

jcstest_LDADD = libjpeg.la

dist_man1_MANS = cjpeg.1 djpeg.1 jpegtran.1 rdjpgcom.1 wrjpgcom.1

DOCS= coderules.txt jconfig.txt change.log rdrle.c wrrle.c BUILDING.txt \
	ChangeLog.txt

docdir = $(datadir)/doc
dist_doc_DATA = README README-turbo.txt libjpeg.txt structure.txt usage.txt \
	wizard.txt 

exampledir = $(datadir)/doc
dist_example_DATA = example.c


EXTRA_DIST = win release $(DOCS) testimages CMakeLists.txt \
	sharedlib/CMakeLists.txt cmakescripts libjpeg.map.in doc doxygen.config \
	jccolext.c jdcolext.c jdmrgext.c

dist-hook:
	rm -rf `find $(distdir) -name .svn`


SUBDIRS += md5

MD5_JPEG_INT = 9a68f56bc76e466aa7e52f415d0f4a5f
MD5_JPEG_FAST = 0e1502e7fa421835e376a314fac2a39f
MD5_JPEG_FAST_100 = 7bf72a8e741d64eecb960c97323af77c
MD5_JPEG_FLOAT = d1623885ffafcd40c684af09e3d65cd5
MD5_JPEG_FLOAT_NOSIMD = fb4884c35f8273f498cb32879de5c455
MD5_JPEG_INT_GRAY = 72b51f894b8f4a10b3ee3066770aa38d
MD5_PPM_INT = d1ed0d11f076b842525271647716aeb8
MD5_PPM_FAST = 048298a2d2410261c0533cb97bcfef23
MD5_PPM_FLOAT = 7f5b446ee36b2630e06785b8d42af15f
MD5_PPM_FLOAT_NOSIMD = 64072f1dbdc5b3a187777788604971a5
MD5_PPM_INT_2_1 = 9f9de8c0612f8d06869b960b05abf9c9
MD5_PPM_INT_15_8 = b6875bc070720b899566cc06459b63b7
MD5_PPM_INT_7_4 = 06a177eae05f164fac57f7a2c346ee87
MD5_PPM_INT_13_8 = bc3452573c8152f6ae552939ee19f82f
MD5_PPM_INT_3_2 = f5a8b88a8a7f96016f04d259cf82ed67
MD5_PPM_INT_11_8 = d8cc73c0aaacd4556569b59437ba00a5
MD5_PPM_INT_5_4 = 32775dd9ad2ab90f4c5b219b53e0c86c
MD5_PPM_INT_9_8 = d25e61bc7eac0002f5b393aa223747b6
MD5_PPM_INT_7_8 = ddb564b7c74a09494016d6cd7502a946
MD5_PPM_INT_3_4 = 8ed8e68808c3fbc4ea764fc9d2968646
MD5_PPM_INT_5_8 = a3363274999da2366a024efae6d16c9b
MD5_PPM_INT_1_2 = e692a315cea26b988c8e8b29a5dbcd81
MD5_PPM_INT_3_8 = 79eca9175652ced755155c90e785a996
MD5_PPM_INT_1_4 = 79cd778f8bf1a117690052cacdd54eca
MD5_PPM_INT_1_8 = 391b3d4aca640c8567d6f8745eb2142f
MD5_PPM_FAST_1_2 = f30bcf6d32ccd44cbdd9aeaacbd9454f
MD5_BMP_256 = 4980185e3776e89bd931736e1cddeee6
MD5_JPEG_ARI = e986fb0a637a8d833d96e8a6d6d84ea1
MD5_PPM_ARI = 72b59a99bcf1de24c5b27d151bde2437
MD5_JPEG_PROG = 1c4afddc05c0a43489ee54438a482d92
MD5_JPEG_CROP = b4197f377e621c4e9b1d20471432610d

test: testclean all
if WITH_TURBOJPEG
if WITH_JAVA
	$(JAVA) -cp java/turbojpeg.jar -Djava.library.path=.libs TJUnitTest
	$(JAVA) -cp java/turbojpeg.jar -Djava.library.path=.libs TJUnitTest -bi
	$(JAVA) -cp java/turbojpeg.jar -Djava.library.path=.libs TJUnitTest -yuv
	$(JAVA) -cp java/turbojpeg.jar -Djava.library.path=.libs TJUnitTest -yuv -bi
endif
	./tjunittest
	./tjunittest -alloc
	./tjunittest -yuv
endif
	./cjpeg -dct int -outfile testoutint.jpg $(srcdir)/testimages/testorig.ppm
	md5/md5cmp $(MD5_JPEG_INT) testoutint.jpg
	./cjpeg -dct fast -opt -outfile testoutfst.jpg $(srcdir)/testimages/testorig.ppm
	md5/md5cmp $(MD5_JPEG_FAST) testoutfst.jpg
	./cjpeg -dct fast -quality 100 -opt -outfile testoutfst100.jpg $(srcdir)/testimages/testorig.ppm
	md5/md5cmp $(MD5_JPEG_FAST_100) testoutfst100.jpg
	./cjpeg -dct float -outfile testoutflt.jpg $(srcdir)/testimages/testorig.ppm
if WITH_SSE_FLOAT_DCT
	md5/md5cmp $(MD5_JPEG_FLOAT) testoutflt.jpg
else
	md5/md5cmp $(MD5_JPEG_FLOAT_NOSIMD) testoutflt.jpg
endif
	./cjpeg -dct int -grayscale -outfile testoutgray.jpg $(srcdir)/testimages/testorig.ppm
	md5/md5cmp $(MD5_JPEG_INT_GRAY) testoutgray.jpg
	./djpeg -dct int -fast -ppm -outfile testoutint.ppm $(srcdir)/testimages/testorig.jpg
	md5/md5cmp $(MD5_PPM_INT) testoutint.ppm
	./djpeg -dct fast -ppm -outfile testoutfst.ppm $(srcdir)/testimages/testorig.jpg
	md5/md5cmp $(MD5_PPM_FAST) testoutfst.ppm
	./djpeg -dct float -ppm -outfile testoutflt.ppm $(srcdir)/testimages/testorig.jpg
if WITH_SSE_FLOAT_DCT
	md5/md5cmp $(MD5_PPM_FLOAT) testoutflt.ppm
else
	md5/md5cmp $(MD5_PPM_FLOAT_NOSIMD) testoutflt.ppm
endif
	./djpeg -dct int -nosmooth -scale 2/1 -ppm -outfile testoutint2_1.ppm $(srcdir)/testimages/testorig.jpg;
	md5/md5cmp $(MD5_PPM_INT_2_1) testoutint2_1.ppm;
	./djpeg -dct int -nosmooth -scale 15/8 -ppm -outfile testoutint15_8.ppm $(srcdir)/testimages/testorig.jpg;
	md5/md5cmp $(MD5_PPM_INT_15_8) testoutint15_8.ppm;
	./djpeg -dct int -nosmooth -scale 7/4 -ppm -outfile testoutint7_4.ppm $(srcdir)/testimages/testorig.jpg;
	md5/md5cmp $(MD5_PPM_INT_7_4) testoutint7_4.ppm;
	./djpeg -dct int -nosmooth -scale 13/8 -ppm -outfile testoutint13_8.ppm $(srcdir)/testimages/testorig.jpg;
	md5/md5cmp $(MD5_PPM_INT_13_8) testoutint13_8.ppm;
	./djpeg -dct int -nosmooth -scale 3/2 -ppm -outfile testoutint3_2.ppm $(srcdir)/testimages/testorig.jpg;
	md5/md5cmp $(MD5_PPM_INT_3_2) testoutint3_2.ppm;
	./djpeg -dct int -nosmooth -scale 11/8 -ppm -outfile testoutint11_8.ppm $(srcdir)/testimages/testorig.jpg;
	md5/md5cmp $(MD5_PPM_INT_11_8) testoutint11_8.ppm;
	./djpeg -dct int -nosmooth -scale 5/4 -ppm -outfile testoutint5_4.ppm $(srcdir)/testimages/testorig.jpg;
	md5/md5cmp $(MD5_PPM_INT_5_4) testoutint5_4.ppm;
	./djpeg -dct int -nosmooth -scale 9/8 -ppm -outfile testoutint9_8.ppm $(srcdir)/testimages/testorig.jpg;
	md5/md5cmp $(MD5_PPM_INT_9_8) testoutint9_8.ppm;
	./djpeg -dct int -nosmooth -scale 7/8 -ppm -outfile testoutint7_8.ppm $(srcdir)/testimages/testorig.jpg;
	md5/md5cmp $(MD5_PPM_INT_7_8) testoutint7_8.ppm;
	./djpeg -dct int -nosmooth -scale 3/4 -ppm -outfile testoutint3_4.ppm $(srcdir)/testimages/testorig.jpg;
	md5/md5cmp $(MD5_PPM_INT_3_4) testoutint3_4.ppm;
	./djpeg -dct int -nosmooth -scale 5/8 -ppm -outfile testoutint5_8.ppm $(srcdir)/testimages/testorig.jpg;
	md5/md5cmp $(MD5_PPM_INT_5_8) testoutint5_8.ppm;
	./djpeg -dct int -nosmooth -scale 1/2 -ppm -outfile testoutint1_2.ppm $(srcdir)/testimages/testorig.jpg;
	md5/md5cmp $(MD5_PPM_INT_1_2) testoutint1_2.ppm;
	./djpeg -dct int -nosmooth -scale 3/8 -ppm -outfile testoutint3_8.ppm $(srcdir)/testimages/testorig.jpg;
	md5/md5cmp $(MD5_PPM_INT_3_8) testoutint3_8.ppm;
	./djpeg -dct int -nosmooth -scale 1/4 -ppm -outfile testoutint1_4.ppm $(srcdir)/testimages/testorig.jpg;
	md5/md5cmp $(MD5_PPM_INT_1_4) testoutint1_4.ppm;
	./djpeg -dct int -nosmooth -scale 1/8 -ppm -outfile testoutint1_8.ppm $(srcdir)/testimages/testorig.jpg;
	md5/md5cmp $(MD5_PPM_INT_1_8) testoutint1_8.ppm;
	./djpeg -dct fast -scale 1/2 -ppm -outfile testoutfst1_2.ppm $(srcdir)/testimages/testorig.jpg
	md5/md5cmp $(MD5_PPM_FAST_1_2) testoutfst1_2.ppm
	./djpeg -dct int -bmp -colors 256 -outfile testout.bmp $(srcdir)/testimages/testorig.jpg
	md5/md5cmp $(MD5_BMP_256) testout.bmp
if WITH_ARITH_ENC
	./cjpeg -dct int -arithmetic -outfile testoutari.jpg $(srcdir)/testimages/testorig.ppm
	md5/md5cmp $(MD5_JPEG_ARI) testoutari.jpg
	./jpegtran -arithmetic -outfile testouta.jpg $(srcdir)/testimages/testimgint.jpg
	md5/md5cmp $(MD5_JPEG_ARI) testouta.jpg
endif
if WITH_ARITH_DEC
	./djpeg -dct int -fast -ppm -outfile testoutari.ppm $(srcdir)/testimages/testimgari.jpg
	md5/md5cmp $(MD5_PPM_ARI) testoutari.ppm
	./jpegtran -outfile testouta.jpg $(srcdir)/testimages/testimgari.jpg
	md5/md5cmp $(MD5_JPEG_INT) testouta.jpg
endif
	./cjpeg -dct int -progressive -outfile testoutp.jpg $(srcdir)/testimages/testorig.ppm
	md5/md5cmp $(MD5_JPEG_PROG) testoutp.jpg
	./jpegtran -outfile testoutt.jpg testoutp.jpg
	md5/md5cmp $(MD5_JPEG_INT) testoutt.jpg
	./jpegtran -crop 120x90+20+50 -transpose -perfect -outfile testoutcrop.jpg $(srcdir)/testimages/testorig.jpg
	md5/md5cmp $(MD5_JPEG_CROP) testoutcrop.jpg


testclean:
	rm -f testout*
	rm -f *_GRAY_*.bmp
	rm -f *_GRAY_*.png
	rm -f *_GRAY_*.ppm
	rm -f *_GRAY_*.jpg
	rm -f *_GRAY.yuv
	rm -f *_420_*.bmp
	rm -f *_420_*.png
	rm -f *_420_*.ppm
	rm -f *_420_*.jpg
	rm -f *_420.yuv
	rm -f *_422_*.bmp
	rm -f *_422_*.png
	rm -f *_422_*.ppm
	rm -f *_422_*.jpg
	rm -f *_422.yuv
	rm -f *_444_*.bmp
	rm -f *_444_*.png
	rm -f *_444_*.ppm
	rm -f *_444_*.jpg
	rm -f *_444.yuv
	rm -f *_440_*.bmp
	rm -f *_440_*.png
	rm -f *_440_*.ppm
	rm -f *_440_*.jpg
	rm -f *_440.yuv


tjtest:
	sh ./tjbenchtest
if WITH_JAVA
	sh ./tjbenchtest.java
endif


pkgscripts/libjpeg-turbo.spec: pkgscripts/libjpeg-turbo.spec.tmpl
	cat pkgscripts/libjpeg-turbo.spec.tmpl | sed s@%{__prefix}@$(prefix)@g | \
		sed s@%{__bindir}@$(bindir)@g | sed s@%{__datadir}@$(datadir)@g | \
		sed s@%{__docdir}@$(docdir)@g | sed s@%{__includedir}@$(includedir)@g | \
		sed s@%{__libdir}@$(libdir)@g | sed s@%{__mandir}@$(mandir)@g \
		> pkgscripts/libjpeg-turbo.spec

rpm: all pkgscripts/libjpeg-turbo.spec
	TMPDIR=`mktemp -d /tmp/${PACKAGE_NAME}-build.XXXXXX`; \
	mkdir -p $$TMPDIR/RPMS; \
	ln -fs `pwd` $$TMPDIR/BUILD; \
	rm -f ${PKGNAME}-${VERSION}.${RPMARCH}.rpm; \
	rpmbuild -bb --define "_blddir $$TMPDIR/buildroot"  \
		--define "_topdir $$TMPDIR" \
		--target ${RPMARCH} pkgscripts/libjpeg-turbo.spec; \
	cp $$TMPDIR/RPMS/${RPMARCH}/${PKGNAME}-${VERSION}-${BUILD}.${RPMARCH}.rpm \
		${PKGNAME}-${VERSION}.${RPMARCH}.rpm; \
	rm -rf $$TMPDIR

srpm: dist-gzip pkgscripts/libjpeg-turbo.spec
	TMPDIR=`mktemp -d /tmp/${PACKAGE_NAME}-build.XXXXXX`; \
	mkdir -p $$TMPDIR/RPMS; \
	mkdir -p $$TMPDIR/SRPMS; \
	mkdir -p $$TMPDIR/BUILD; \
	mkdir -p $$TMPDIR/SOURCES; \
	mkdir -p $$TMPDIR/SPECS; \
	rm -f ${PKGNAME}-${VERSION}.src.rpm; \
	cp ${PACKAGE_NAME}-${VERSION}.tar.gz $$TMPDIR/SOURCES; \
	cat pkgscripts/libjpeg-turbo.spec | sed s/%{_blddir}/%{_tmppath}/g \
		| sed s/#--\>//g \
		> $$TMPDIR/SPECS/libjpeg-turbo.spec; \
	rpmbuild -bs --define "_topdir $$TMPDIR" $$TMPDIR/SPECS/libjpeg-turbo.spec; \
	cp $$TMPDIR/SRPMS/${PKGNAME}-${VERSION}-${BUILD}.src.rpm \
		${PKGNAME}-${VERSION}.src.rpm; \
	rm -rf $$TMPDIR

pkgscripts/makedpkg: pkgscripts/makedpkg.tmpl
	cat pkgscripts/makedpkg.tmpl | sed s@%{__prefix}@$(prefix)@g | \
		sed s@%{__docdir}@$(docdir)@g | sed s@%{__libdir}@$(libdir)@g \
		> pkgscripts/makedpkg

deb: all pkgscripts/makedpkg
	sh pkgscripts/makedpkg

pkgscripts/uninstall: pkgscripts/uninstall.tmpl
	cat pkgscripts/uninstall.tmpl | sed s@%{__prefix}@$(prefix)@g | \
		sed s@%{__bindir}@$(bindir)@g | sed s@%{__datadir}@$(datadir)@g | \
		sed s@%{__includedir}@$(includedir)@g | sed s@%{__libdir}@$(libdir)@g | \
		sed s@%{__mandir}@$(mandir)@g > pkgscripts/uninstall

pkgscripts/makemacpkg: pkgscripts/makemacpkg.tmpl
	cat pkgscripts/makemacpkg.tmpl | sed s@%{__prefix}@$(prefix)@g | \
		sed s@%{__bindir}@$(bindir)@g | sed s@%{__docdir}@$(docdir)@g | \
		sed s@%{__libdir}@$(libdir)@g > pkgscripts/makemacpkg

if X86_64

udmg: all pkgscripts/makemacpkg pkgscripts/uninstall
	sh pkgscripts/makemacpkg -build32 ${BUILDDIR32}

iosdmg: all pkgscripts/makemacpkg pkgscripts/uninstall
	sh pkgscripts/makemacpkg -build32 ${BUILDDIR32} -buildarmv6 ${BUILDDIRARMV6} -buildarmv7 ${BUILDDIRARMV7} -buildarmv7s ${BUILDDIRARMV7S}

else

iosdmg: all pkgscripts/makemacpkg pkgscripts/uninstall
	sh pkgscripts/makemacpkg -buildarmv6 ${BUILDDIRARMV6} -buildarmv7 ${BUILDDIRARMV7} -buildarmv7s ${BUILDDIRARMV7S}

endif

dmg: all pkgscripts/makemacpkg pkgscripts/uninstall
	sh pkgscripts/makemacpkg

pkgscripts/makecygwinpkg: pkgscripts/makecygwinpkg.tmpl
	cat pkgscripts/makecygwinpkg.tmpl | sed s@%{__prefix}@$(prefix)@g | \
		sed s@%{__docdir}@$(docdir)@g | sed s@%{__libdir}@$(libdir)@g \
		> pkgscripts/makecygwinpkg

cygwinpkg: all pkgscripts/makecygwinpkg
	sh pkgscripts/makecygwinpkg

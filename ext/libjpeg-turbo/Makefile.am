lib_LTLIBRARIES = libjpeg.la libturbojpeg.la
libjpeg_la_LDFLAGS = -version-info ${SO_MAJOR_VERSION}:${SO_MINOR_VERSION} -no-undefined
libturbojpeg_la_LDFLAGS = -avoid-version -no-undefined
include_HEADERS = jerror.h jmorecfg.h jpeglib.h turbojpeg.h
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

libturbojpeg_la_SOURCES = $(libjpeg_la_SOURCES) turbojpeg.c turbojpeg.h \
	transupp.c transupp.h jdatadst-tj.c jdatasrc-tj.c

SUBDIRS = java

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
libjpeg_la_LDFLAGS += $(VERSION_SCRIPT_FLAG)libjpeg.map

endif

if WITH_SIMD

SUBDIRS += simd
libjpeg_la_LIBADD = simd/libsimd.la
libturbojpeg_la_LIBADD = simd/libsimd.la

else

libjpeg_la_SOURCES += jsimd_none.c

endif

bin_PROGRAMS = cjpeg djpeg jpegtran rdjpgcom wrjpgcom tjbench
noinst_PROGRAMS = tjunittest jcstest

tjbench_SOURCES = tjbench.c bmp.h bmp.c tjutil.h tjutil.c rdbmp.c rdppm.c \
	wrbmp.c wrppm.c

tjbench_LDADD = libturbojpeg.la libjpeg.la -lm

tjbench_CFLAGS = -DBMP_SUPPORTED -DPPM_SUPPORTED

tjunittest_SOURCES = tjunittest.c tjutil.h tjutil.c

tjunittest_LDADD = libturbojpeg.la

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

DOCS= install.txt coderules.txt filelist.txt jconfig.txt change.log \
	rdrle.c wrrle.c BUILDING.txt ChangeLog.txt

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


test: testclean all
if WITH_JAVA
	$(JAVA) -cp java/turbojpeg.jar -Djava.library.path=.libs TJUnitTest
	$(JAVA) -cp java/turbojpeg.jar -Djava.library.path=.libs TJUnitTest -bi
	$(JAVA) -cp java/turbojpeg.jar -Djava.library.path=.libs TJUnitTest -yuv
	$(JAVA) -cp java/turbojpeg.jar -Djava.library.path=.libs TJUnitTest -yuv -bi
endif
	./tjunittest
	./tjunittest -alloc
	./tjunittest -yuv
	./cjpeg -dct int -outfile testoutint.jpg $(srcdir)/testimages/testorig.ppm
	cmp $(srcdir)/testimages/testimgint.jpg testoutint.jpg
	./cjpeg -dct fast -opt -outfile testoutfst.jpg $(srcdir)/testimages/testorig.ppm
	cmp $(srcdir)/testimages/testimgfst.jpg testoutfst.jpg
	./cjpeg -dct fast -quality 100 -opt -outfile testoutfst100.jpg $(srcdir)/testimages/testorig.ppm
	cmp $(srcdir)/testimages/testimgfst100.jpg testoutfst100.jpg
	./cjpeg -dct float -outfile testoutflt.jpg $(srcdir)/testimages/testorig.ppm
if WITH_SSE_FLOAT_DCT
	cmp $(srcdir)/testimages/testimgflt.jpg testoutflt.jpg
else
	cmp $(srcdir)/testimages/testimgflt-nosimd.jpg testoutflt.jpg
endif
	./cjpeg -dct int -grayscale -outfile testoutgray.jpg $(srcdir)/testimages/testorig.ppm
	cmp $(srcdir)/testimages/testimggray.jpg testoutgray.jpg
	./djpeg -dct int -fast -ppm -outfile testoutint.ppm $(srcdir)/testimages/testorig.jpg
	cmp $(srcdir)/testimages/testimgint.ppm testoutint.ppm
	./djpeg -dct fast -ppm -outfile testoutfst.ppm $(srcdir)/testimages/testorig.jpg
	cmp $(srcdir)/testimages/testimgfst.ppm testoutfst.ppm
	./djpeg -dct float -ppm -outfile testoutflt.ppm $(srcdir)/testimages/testorig.jpg
if WITH_SSE_FLOAT_DCT
	cmp $(srcdir)/testimages/testimgflt.ppm testoutflt.ppm
else
	cmp $(srcdir)/testimages/testorig.ppm testoutflt.ppm
endif
	./djpeg -dct int -scale 1/2 -ppm -outfile testoutint1_2.ppm $(srcdir)/testimages/testorig.jpg
	cmp $(srcdir)/testimages/testimgint1_2.ppm testoutint1_2.ppm
	./djpeg -dct fast -scale 1/2 -ppm -outfile testoutfst1_2.ppm $(srcdir)/testimages/testorig.jpg
	cmp $(srcdir)/testimages/testimgfst1_2.ppm testoutfst1_2.ppm
	./djpeg -dct int -scale 1/4 -ppm -outfile testoutint1_4.ppm $(srcdir)/testimages/testorig.jpg
	cmp $(srcdir)/testimages/testimgint1_4.ppm testoutint1_4.ppm
	./djpeg -dct fast -scale 1/4 -ppm -outfile testoutfst1_4.ppm $(srcdir)/testimages/testorig.jpg
	cmp $(srcdir)/testimages/testimgfst1_4.ppm testoutfst1_4.ppm
	./djpeg -dct int -scale 1/8 -ppm -outfile testoutint1_8.ppm $(srcdir)/testimages/testorig.jpg
	cmp $(srcdir)/testimages/testimgint1_8.ppm testoutint1_8.ppm
	./djpeg -dct fast -scale 1/8 -ppm -outfile testoutfst1_8.ppm $(srcdir)/testimages/testorig.jpg
	cmp $(srcdir)/testimages/testimgfst1_8.ppm testoutfst1_8.ppm
	./djpeg -dct int -bmp -colors 256 -outfile testout.bmp  $(srcdir)/testimages/testorig.jpg
	cmp $(srcdir)/testimages/testimg.bmp testout.bmp
if WITH_ARITH_ENC
	./cjpeg -dct int -arithmetic -outfile testoutari.jpg $(srcdir)/testimages/testorig.ppm
	cmp $(srcdir)/testimages/testimgari.jpg testoutari.jpg
	./jpegtran -arithmetic -outfile testouta.jpg $(srcdir)/testimages/testimgint.jpg
	cmp $(srcdir)/testimages/testimgari.jpg testouta.jpg
endif
if WITH_ARITH_DEC
	./djpeg -dct int -fast -ppm -outfile testoutari.ppm $(srcdir)/testimages/testimgari.jpg
	cmp $(srcdir)/testimages/testimgari.ppm testoutari.ppm
	./jpegtran -outfile testouta.jpg $(srcdir)/testimages/testimgari.jpg
	cmp $(srcdir)/testimages/testimgint.jpg testouta.jpg
endif
	./cjpeg -dct int -progressive -outfile testoutp.jpg $(srcdir)/testimages/testorig.ppm
	cmp $(srcdir)/testimages/testimgp.jpg testoutp.jpg
	./jpegtran -outfile testoutt.jpg testoutp.jpg
	cmp $(srcdir)/testimages/testimgint.jpg testoutt.jpg
	./jpegtran -crop 120x90+20+50 -transpose -perfect -outfile testoutcrop.jpg $(srcdir)/testimages/testorig.jpg
	cmp $(srcdir)/testimages/testimgcrop.jpg testoutcrop.jpg


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
	sh ./tjexampletest
endif


if X86_64

install-exec-hook:
	__PREFIX=`echo ${prefix} | sed -e 's@\/*$$@@'`;  \
	if [ "$$__PREFIX" = "/opt/libjpeg-turbo" ]; then  \
		cd $(DESTDIR)/${prefix};  \
		if [ -d lib -a ! -d lib64 -a ! -h lib64 ]; then  \
			$(LN_S) lib lib64;  \
		fi  \
	fi

else

install-exec-hook:
	__PREFIX=`echo ${prefix} | sed -e 's@\/*$$@@'`;  \
	if [ "$$__PREFIX" = "/opt/libjpeg-turbo" ]; then  \
		cd $(DESTDIR)/${prefix};  \
		if [ -d lib -a ! -d lib32 -a ! -h lib32 ]; then  \
			$(LN_S) lib lib32;  \
		fi  \
	fi

endif

rpm: all
	TMPDIR=`mktemp -d /tmp/${PACKAGE_NAME}-build.XXXXXX`; \
	mkdir -p $$TMPDIR/RPMS; \
	ln -fs `pwd` $$TMPDIR/BUILD; \
	rm -f ${PACKAGE_NAME}-${VERSION}.${RPMARCH}.rpm; \
	rpmbuild -bb --define "_blddir $$TMPDIR/buildroot"  \
		--define "_topdir $$TMPDIR" \
		--target ${RPMARCH} pkgscripts/libjpeg-turbo.spec; \
	cp $$TMPDIR/RPMS/${RPMARCH}/${PACKAGE_NAME}-${VERSION}-${BUILD}.${RPMARCH}.rpm \
		${PACKAGE_NAME}-${VERSION}.${RPMARCH}.rpm; \
	rm -rf $$TMPDIR

srpm: dist-gzip
	TMPDIR=`mktemp -d /tmp/${PACKAGE_NAME}-build.XXXXXX`; \
	mkdir -p $$TMPDIR/RPMS; \
	mkdir -p $$TMPDIR/SRPMS; \
	mkdir -p $$TMPDIR/BUILD; \
	mkdir -p $$TMPDIR/SOURCES; \
	mkdir -p $$TMPDIR/SPECS; \
	rm -f ${PACKAGE_NAME}-${VERSION}.src.rpm; \
	cp ${PACKAGE_NAME}-${VERSION}.tar.gz $$TMPDIR/SOURCES; \
	cat pkgscripts/libjpeg-turbo.spec | sed s/%{_blddir}/%{_tmppath}/g \
		| sed s/#--\>//g \
		> $$TMPDIR/SPECS/libjpeg-turbo.spec; \
	rpmbuild -bs --define "_topdir $$TMPDIR" $$TMPDIR/SPECS/libjpeg-turbo.spec; \
	cp $$TMPDIR/SRPMS/${PACKAGE_NAME}-${VERSION}-${BUILD}.src.rpm \
		${PACKAGE_NAME}-${VERSION}.src.rpm; \
	rm -rf $$TMPDIR

deb: all
	sh pkgscripts/makedpkg

if X86_64

udmg: all
	sh pkgscripts/makemacpkg -build32 ${BUILDDIR32}

iosdmg: all
	sh pkgscripts/makemacpkg -build32 ${BUILDDIR32} -buildarmv6 ${BUILDDIRARMV6} -buildarmv7 ${BUILDDIRARMV7}

else

iosdmg: all
	sh pkgscripts/makemacpkg -buildarmv6 ${BUILDDIRARMV6} -buildarmv7 ${BUILDDIRARMV7}

endif

dmg: all
	sh pkgscripts/makemacpkg

if X86_64

csunpkg: all
	sh pkgscripts/makesunpkg combined ${BUILDDIR32}

endif

sunpkg: all
	sh pkgscripts/makesunpkg

cygwinpkg: all
	sh pkgscripts/makecygwinpkg

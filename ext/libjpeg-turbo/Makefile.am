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

libturbojpeg_la_SOURCES = $(libjpeg_la_SOURCES) turbojpegl.c turbojpeg.h \
	turbojpeg-mapfile

if VERSION_SCRIPT

libturbojpeg_la_LDFLAGS += $(VERSION_SCRIPT_FLAG)$(srcdir)/turbojpeg-mapfile
libjpeg_la_LDFLAGS += $(VERSION_SCRIPT_FLAG)libjpeg.map

endif

if WITH_SIMD

SUBDIRS = simd
libjpeg_la_LIBADD = simd/libsimd.la
libturbojpeg_la_LIBADD = simd/libsimd.la

else

libjpeg_la_SOURCES += jsimd_none.c

endif

TSTHDRS = rrutil.h rrtimer.h

bin_PROGRAMS = cjpeg djpeg jpegtran rdjpgcom wrjpgcom jpgtest
noinst_PROGRAMS = jpegut

jpgtest_SOURCES = $(TSTHDRS) jpgtest.c bmp.h bmp.c

jpgtest_LDADD = libturbojpeg.la -lm

jpegut_SOURCES = $(TSTHDRS) jpegut.c bmp.h bmp.c

jpegut_LDADD = libturbojpeg.la

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


dist_man1_MANS = cjpeg.1 djpeg.1 jpegtran.1 rdjpgcom.1 wrjpgcom.1

DOCS= README install.txt usage.txt wizard.txt example.c libjpeg.txt \
	structure.txt coderules.txt filelist.txt jconfig.txt change.log \
	README-turbo.txt rdrle.c wrrle.c LICENSE.txt LGPL.txt BUILDING.txt \
	ChangeLog.txt

TESTFILES= testorig.jpg testorig.ppm testimg.bmp testimgflt.jpg \
	testimgfst.jpg testimgint.jpg testimgp.jpg testimgflt.ppm testimgfst.ppm \
	testimgint.ppm testimgflt-nosimd.jpg testimgcrop.jpg testimgari.jpg \
	testimgari.ppm testimgfst100.jpg

EXTRA_DIST = win release $(DOCS) $(TESTFILES) CMakeLists.txt \
	sharedlib/CMakeLists.txt cmakescripts libjpeg.map.in

dist-hook:
	rm -rf `find $(distdir) -name .svn`


test: testclean all
	./jpegut
	./jpegut -yuv
	./cjpeg -dct int -outfile testoutint.jpg $(srcdir)/testorig.ppm
	cmp $(srcdir)/testimgint.jpg testoutint.jpg
	./cjpeg -dct fast -opt -outfile testoutfst.jpg $(srcdir)/testorig.ppm
	cmp $(srcdir)/testimgfst.jpg testoutfst.jpg
	./cjpeg -dct fast -quality 100 -opt -outfile testoutfst100.jpg $(srcdir)/testorig.ppm
	cmp $(srcdir)/testimgfst100.jpg testoutfst100.jpg
	./cjpeg -dct float -outfile testoutflt.jpg $(srcdir)/testorig.ppm
if WITH_SIMD
	cmp $(srcdir)/testimgflt.jpg testoutflt.jpg
else
	cmp $(srcdir)/testimgflt-nosimd.jpg testoutflt.jpg
endif
	./djpeg -dct int -fast -ppm -outfile testoutint.ppm $(srcdir)/testorig.jpg
	cmp $(srcdir)/testimgint.ppm testoutint.ppm
	./djpeg -dct fast -ppm -outfile testoutfst.ppm $(srcdir)/testorig.jpg
	cmp $(srcdir)/testimgfst.ppm testoutfst.ppm
	./djpeg -dct float -ppm -outfile testoutflt.ppm $(srcdir)/testorig.jpg
if WITH_SIMD
	cmp $(srcdir)/testimgflt.ppm testoutflt.ppm
else
	cmp $(srcdir)/testorig.ppm testoutflt.ppm
endif
	./djpeg -dct int -bmp -colors 256 -outfile testout.bmp  $(srcdir)/testorig.jpg
	cmp $(srcdir)/testimg.bmp testout.bmp
if WITH_ARITH_ENC
	./cjpeg -dct int -arithmetic -outfile testoutari.jpg $(srcdir)/testorig.ppm
	cmp $(srcdir)/testimgari.jpg testoutari.jpg
	./jpegtran -arithmetic -outfile testouta.jpg $(srcdir)/testimgint.jpg
	cmp $(srcdir)/testimgari.jpg testouta.jpg
endif
if WITH_ARITH_DEC
	./djpeg -dct int -fast -ppm -outfile testoutari.ppm $(srcdir)/testimgari.jpg
	cmp $(srcdir)/testimgari.ppm testoutari.ppm
	./jpegtran -outfile testouta.jpg $(srcdir)/testimgari.jpg
	cmp $(srcdir)/testimgint.jpg testouta.jpg
endif
	./cjpeg -dct int -progressive -outfile testoutp.jpg $(srcdir)/testorig.ppm
	cmp $(srcdir)/testimgp.jpg testoutp.jpg
	./jpegtran -outfile testoutt.jpg testoutp.jpg
	cmp $(srcdir)/testimgint.jpg testoutt.jpg
	./jpegtran -crop 120x90+20+50 -transpose -perfect -outfile testoutcrop.jpg $(srcdir)/testorig.jpg
	cmp $(srcdir)/testimgcrop.jpg testoutcrop.jpg


testclean:
	rm -f testout*
	rm -f *_GRAYQ[0-9]*.bmp
	rm -f *_GRAYQ[0-9]*.ppm
	rm -f *_GRAYQ[0-9]*.jpg
	rm -f *_420Q[0-9]*.bmp
	rm -f *_420Q[0-9]*.ppm
	rm -f *_420Q[0-9]*.jpg
	rm -f *_422Q[0-9]*.bmp
	rm -f *_422Q[0-9]*.ppm
	rm -f *_422Q[0-9]*.jpg
	rm -f *_444Q[0-9]*.bmp
	rm -f *_444Q[0-9]*.ppm
	rm -f *_444Q[0-9]*.jpg

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
	rm -f ${PACKAGE_NAME}.${RPMARCH}.rpm; \
	rpmbuild -bb --define "_blddir $$TMPDIR/buildroot"  \
		--define "_topdir $$TMPDIR" --define "_srcdir ${srcdir}" \
		--target ${RPMARCH} libjpeg-turbo.spec; \
	cp $$TMPDIR/RPMS/${RPMARCH}/${PACKAGE_NAME}-${VERSION}-${BUILD}.${RPMARCH}.rpm ${PACKAGE_NAME}.${RPMARCH}.rpm; \
	rm -rf $$TMPDIR

srpm: dist-gzip
	TMPDIR=`mktemp -d /tmp/${PACKAGE_NAME}-build.XXXXXX`; \
	mkdir -p $$TMPDIR/RPMS; \
	mkdir -p $$TMPDIR/SRPMS; \
	mkdir -p $$TMPDIR/BUILD; \
	mkdir -p $$TMPDIR/SOURCES; \
	mkdir -p $$TMPDIR/SPECS; \
	rm -f ${PACKAGE_NAME}.src.rpm; \
	cp ${PACKAGE_NAME}-${VERSION}.tar.gz $$TMPDIR/SOURCES; \
	cat libjpeg-turbo.spec | sed s/%{_blddir}/%{_tmppath}/g \
		| sed s@%{_srcdir}/@@g | sed s/#--\>//g \
		>$$TMPDIR/SPECS/libjpeg-turbo.spec; \
	rpmbuild -bs --define "_topdir $$TMPDIR" $$TMPDIR/SPECS/libjpeg-turbo.spec; \
	cp $$TMPDIR/SRPMS/${PACKAGE_NAME}-${VERSION}-${BUILD}.src.rpm ${PACKAGE_NAME}.src.rpm; \
	rm -rf $$TMPDIR

deb: all
	sh $(srcdir)/release/makedpkg ${PACKAGE_NAME} ${VERSION} ${BUILD} \
		${DEBARCH} ${srcdir}

if X86_64

udmg: all
	sh makemacpkg universal ${BUILDDIR32}

endif

dmg: all
	sh makemacpkg

if X86_64

csunpkg: all
	sh makesunpkg combined ${BUILDDIR32}

endif

sunpkg: all
	sh makesunpkg

cygwinpkg: all
	sh $(srcdir)/release/makecygwinpkg ${PACKAGE_NAME} ${VERSION} ${srcdir}

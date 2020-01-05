lib_LTLIBRARIES = libdjvulibre.la

includesdir = $(includedir)/libdjvu
includes_HEADERS = ddjvuapi.h miniexp.h

libdjvulibre_la_SOURCES = Arrays.cpp BSByteStream.cpp			\
 BSEncodeByteStream.cpp ByteStream.cpp DataPool.cpp DjVmDir.cpp		\
 DjVmDir0.cpp DjVmDoc.cpp DjVmNav.cpp DjVuAnno.cpp DjVuDocEditor.cpp	\
 DjVuDocument.cpp DjVuDumpHelper.cpp DjVuErrorList.cpp DjVuFile.cpp	\
 DjVuFileCache.cpp DjVuGlobal.cpp DjVuGlobalMemory.cpp DjVuImage.cpp	\
 DjVuInfo.cpp DjVuMessage.cpp DjVuMessageLite.cpp DjVuNavDir.cpp	\
 DjVuPalette.cpp DjVuPort.cpp DjVuText.cpp DjVuToPS.cpp GBitmap.cpp	\
 GContainer.cpp GException.cpp GIFFManager.cpp GMapAreas.cpp GOS.cpp	\
 GPixmap.cpp GRect.cpp GScaler.cpp GSmartPointer.cpp GString.cpp	\
 GThreads.cpp GURL.cpp GUnicode.cpp IFFByteStream.cpp			\
 IW44EncodeCodec.cpp IW44Image.cpp JB2EncodeCodec.cpp JB2Image.cpp	\
 JPEGDecoder.cpp MMRDecoder.cpp MMX.cpp UnicodeByteStream.cpp		\
 XMLParser.cpp XMLTags.cpp ZPCodec.cpp atomic.cpp ddjvuapi.cpp		\
 debug.cpp miniexp.cpp Arrays.h BSByteStream.h ByteStream.h		\
 DataPool.h DjVmDir.h DjVmDir0.h DjVmDoc.h DjVmNav.h DjVuAnno.h		\
 DjVuDocEditor.h DjVuDocument.h DjVuDumpHelper.h DjVuErrorList.h	\
 DjVuFile.h DjVuFileCache.h DjVuGlobal.h DjVuImage.h DjVuInfo.h		\
 DjVuMessage.h DjVuMessageLite.h DjVuNavDir.h DjVuPalette.h		\
 DjVuPort.h DjVuText.h DjVuToPS.h GBitmap.h GContainer.h GException.h	\
 GIFFManager.h GMapAreas.h GOS.h GPixmap.h GRect.h GScaler.h		\
 GSmartPointer.h GString.h GThreads.h GURL.h IFFByteStream.h		\
 IW44Image.h JB2Image.h JPEGDecoder.h MMRDecoder.h MMX.h Template.h	\
 UnicodeByteStream.h XMLParser.h XMLTags.h ZPCodec.h atomic.h debug.h

libdjvulibre_la_CPPFLAGS = -DDIR_DATADIR=\"$(datadir)\"
libdjvulibre_la_CXXFLAGS = $(JPEG_CFLAGS) $(PTHREAD_CFLAGS)
libdjvulibre_la_LIBADD = $(JPEG_LIBS) $(PTHREAD_LIBS)
libdjvulibre_la_LDFLAGS = -no-undefined -version-info $(version_info)

if HAVE_OS_WIN32
libdjvulibre_la_CPPFLAGS += -DDJVUAPI_EXPORT 
libdjvulibre_la_CPPFLAGS += -DDDJVUAPI_EXPORT -DMINILISPAPI_EXPORT
libdjvulibre_la_LDFLAGS += -Wl,--export-all-symbols
endif

if HAVE_OS_APPLE
libdjvulibre_la_LDFLAGS += -framework CoreFoundation
endif

sed_process = $(SED) \
 -e 's,@PACKAGE_VERSION\@,$(PACKAGE_VERSION),g' \
 -e 's,@prefix\@,$(prefix),g' \
 -e 's,@exec_prefix\@,$(exec_prefix),g' \
 -e 's,@libdir\@,$(libdir),g' \
 -e 's,@includedir\@,$(includedir),g' \
 -e 's,@Libs\@,-L$(libdir) -ldjvulibre,g' \
 -e 's,@Libsprivate\@,$(JPEG_LIBS) $(PTHREAD_LIBS) $(LIBS),g' \
 -e 's,@Cflags\@,-I$(includedir) $(JPEG_CFLAGS) $(PTHREAD_CFLAGS),g' \
 < $< > $@ || (rm $@; exit 1)

pc_verbose = $(pc_verbose_@AM_V@)
pc_verbose_ = $(pc_verbose_@AM_DEFAULT_V@)
pc_verbose_0 = @echo "  PC      " $@;

ddjvuapi.pc: ddjvuapi.pc.in Makefile
	$(AM_V_at)rm -f $@
	$(pc_verbose)$(sed_process)

pkgconfig_DATA = ddjvuapi.pc
EXTRA_DIST = ddjvuapi.pc.in
MOSTLYCLEANFILES = ddjvuapi.pc

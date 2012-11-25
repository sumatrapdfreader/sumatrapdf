LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_ROOT := ../..

OPENJPEG := openjpeg
JPEG := jpeg
ZLIB := zlib
FREETYPE := freetype

LOCAL_C_INCLUDES := \
	../thirdparty/jbig2dec \
	../thirdparty/$(OPENJPEG)/libopenjpeg \
	../thirdparty/$(JPEG) \
	../thirdparty/$(ZLIB) \
	../thirdparty/$(FREETYPE)/include \
	../scripts

LOCAL_CFLAGS := \
	-DFT2_BUILD_LIBRARY -DDARWIN_NO_CARBON -DHAVE_STDINT_H \
	'-DFT_CONFIG_MODULES_H="slimftmodules.h"' \
	'-DFT_CONFIG_OPTIONS_H="slimftoptions.h"'
ifdef NDK_PROFILER
LOCAL_CFLAGS += -pg -DNDK_PROFILER -O2
endif

LOCAL_MODULE := mupdfthirdparty
LOCAL_SRC_FILES := \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_arith.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_arith_iaid.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_arith_int.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_generic.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_halftone.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_huffman.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_image.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_metadata.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_mmr.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_page.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_refinement.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_segment.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_symbol_dict.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_text.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/bio.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/cidx_manager.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/cio.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/dwt.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/event.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/image.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/j2k.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/j2k_lib.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/jp2.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/jpt.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/mct.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/mqc.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/openjpeg.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/phix_manager.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/pi.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/ppix_manager.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/raw.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/t1.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/t2.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/tcd.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/tgt.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/thix_manager.c \
	$(MY_ROOT)/thirdparty/$(OPENJPEG)/libopenjpeg/tpix_manager.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jaricom.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jcomapi.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdapimin.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdapistd.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdarith.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdatadst.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdatasrc.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdcoefct.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdcolor.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jddctmgr.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdhuff.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdinput.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdmainct.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdmarker.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdmaster.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdmerge.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdpostct.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdsample.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jdtrans.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jerror.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jfdctflt.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jfdctfst.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jfdctint.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jidctflt.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jidctfst.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jidctint.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jmemmgr.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jmemnobs.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jquant1.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jquant2.c \
	$(MY_ROOT)/thirdparty/$(JPEG)/jutils.c \
	$(MY_ROOT)/thirdparty/$(ZLIB)/adler32.c \
	$(MY_ROOT)/thirdparty/$(ZLIB)/compress.c \
	$(MY_ROOT)/thirdparty/$(ZLIB)/crc32.c \
	$(MY_ROOT)/thirdparty/$(ZLIB)/deflate.c \
	$(MY_ROOT)/thirdparty/$(ZLIB)/inffast.c \
	$(MY_ROOT)/thirdparty/$(ZLIB)/inflate.c \
	$(MY_ROOT)/thirdparty/$(ZLIB)/inftrees.c \
	$(MY_ROOT)/thirdparty/$(ZLIB)/trees.c \
	$(MY_ROOT)/thirdparty/$(ZLIB)/uncompr.c \
	$(MY_ROOT)/thirdparty/$(ZLIB)/zutil.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/base/ftbase.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/base/ftbbox.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/base/ftbitmap.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/base/ftgasp.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/base/ftglyph.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/base/ftinit.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/base/ftstroke.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/base/ftsynth.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/base/ftsystem.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/base/fttype1.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/base/ftxf86.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/cff/cff.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/cid/type1cid.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/psaux/psaux.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/pshinter/pshinter.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/psnames/psnames.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/raster/raster.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/smooth/smooth.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/sfnt/sfnt.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/truetype/truetype.c \
	$(MY_ROOT)/thirdparty/$(FREETYPE)/src/type1/type1.c

include $(BUILD_STATIC_LIBRARY)

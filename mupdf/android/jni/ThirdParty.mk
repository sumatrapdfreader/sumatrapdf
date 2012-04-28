LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_ROOT := ../..

LOCAL_C_INCLUDES := \
	../thirdparty/jbig2dec \
	../thirdparty/openjpeg-1.5.0/libopenjpeg \
	../thirdparty/jpeg-8d \
	../thirdparty/zlib-1.2.5 \
	../thirdparty/freetype-2.4.9/include \
	../scripts

LOCAL_CFLAGS := \
	-DFT2_BUILD_LIBRARY -DDARWIN_NO_CARBON -DHAVE_STDINT_H \
	'-DFT_CONFIG_MODULES_H="slimftmodules.h"' \
	'-DFT_CONFIG_OPTIONS_H="slimftoptions.h"'

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
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/bio.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/cidx_manager.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/cio.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/dwt.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/event.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/image.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/j2k.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/j2k_lib.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/jp2.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/jpt.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/mct.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/mqc.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/openjpeg.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/phix_manager.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/pi.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/ppix_manager.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/raw.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/t1.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/t2.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/tcd.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/tgt.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/thix_manager.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.5.0/libopenjpeg/tpix_manager.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jaricom.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jcomapi.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdapimin.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdapistd.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdarith.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdatadst.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdatasrc.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdcoefct.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdcolor.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jddctmgr.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdhuff.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdinput.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdmainct.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdmarker.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdmaster.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdmerge.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdpostct.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdsample.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdtrans.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jerror.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jfdctflt.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jfdctfst.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jfdctint.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jidctflt.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jidctfst.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jidctint.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jmemmgr.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jmemnobs.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jquant1.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jquant2.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jutils.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/adler32.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/compress.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/crc32.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/deflate.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/inffast.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/inflate.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/inftrees.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/trees.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/uncompr.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/zutil.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/base/ftbase.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/base/ftbbox.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/base/ftbitmap.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/base/ftgasp.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/base/ftglyph.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/base/ftinit.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/base/ftstroke.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/base/ftsynth.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/base/ftsystem.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/base/fttype1.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/base/ftxf86.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/cff/cff.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/cid/type1cid.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/psaux/psaux.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/pshinter/pshinter.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/psnames/psnames.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/raster/raster.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/smooth/smooth.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/sfnt/sfnt.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/truetype/truetype.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.9/src/type1/type1.c

include $(BUILD_STATIC_LIBRARY)

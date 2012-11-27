LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_ROOT := ../..

OPENJPEG := openjpeg
JPEG := jpeg
ZLIB := zlib
FREETYPE := freetype
V8 := v8-3.9

LOCAL_CFLAGS += -DARCH_ARM -DARCH_THUMB -DARCH_ARM_CAN_LOAD_UNALIGNED -DAA_BITS=8
ifdef NDK_PROFILER
LOCAL_CFLAGS += -pg -DNDK_PROFILER -O2
endif

LOCAL_C_INCLUDES := \
	../thirdparty/jbig2dec \
	../thirdparty/$(OPENJPEG)/libopenjpeg \
	../thirdparty/$(JPEG) \
	../thirdparty/$(ZLIB) \
	../thirdparty/$(FREETYPE)/include \
	../draw \
	../fitz \
	../pdf \
	../xps \
	../cbz \
	../scripts \
	..
ifdef V8_BUILD
LOCAL_C_INCLUDES += ../thirdparty/$(V8)/include
endif

LOCAL_MODULE    := mupdfcore
LOCAL_SRC_FILES := \
	$(MY_ROOT)/fitz/base_context.c \
	$(MY_ROOT)/fitz/base_error.c \
	$(MY_ROOT)/fitz/base_geometry.c \
	$(MY_ROOT)/fitz/base_getopt.c \
	$(MY_ROOT)/fitz/base_hash.c \
	$(MY_ROOT)/fitz/base_memory.c \
	$(MY_ROOT)/fitz/base_string.c \
	$(MY_ROOT)/fitz/base_time.c \
	$(MY_ROOT)/fitz/base_xml.c \
	$(MY_ROOT)/fitz/crypt_aes.c \
	$(MY_ROOT)/fitz/crypt_arc4.c \
	$(MY_ROOT)/fitz/crypt_md5.c \
	$(MY_ROOT)/fitz/crypt_sha2.c \
	$(MY_ROOT)/fitz/dev_bbox.c \
	$(MY_ROOT)/fitz/dev_list.c \
	$(MY_ROOT)/fitz/dev_null.c \
	$(MY_ROOT)/fitz/dev_text.c \
	$(MY_ROOT)/fitz/dev_trace.c \
	$(MY_ROOT)/fitz/doc_document.c \
	$(MY_ROOT)/fitz/doc_interactive.c \
	$(MY_ROOT)/fitz/doc_link.c \
	$(MY_ROOT)/fitz/doc_outline.c \
	$(MY_ROOT)/fitz/doc_search.c \
	$(MY_ROOT)/fitz/filt_basic.c \
	$(MY_ROOT)/fitz/filt_dctd.c \
	$(MY_ROOT)/fitz/filt_faxd.c \
	$(MY_ROOT)/fitz/filt_flate.c \
	$(MY_ROOT)/fitz/filt_jbig2d.c \
	$(MY_ROOT)/fitz/filt_lzwd.c \
	$(MY_ROOT)/fitz/filt_predict.c \
	$(MY_ROOT)/fitz/image_jpx.c \
	$(MY_ROOT)/fitz/image_jpeg.c \
	$(MY_ROOT)/fitz/image_png.c \
	$(MY_ROOT)/fitz/image_tiff.c \
	$(MY_ROOT)/fitz/res_colorspace.c \
	$(MY_ROOT)/fitz/res_font.c \
	$(MY_ROOT)/fitz/res_path.c \
	$(MY_ROOT)/fitz/res_pixmap.c \
	$(MY_ROOT)/fitz/res_store.c \
	$(MY_ROOT)/fitz/res_text.c \
	$(MY_ROOT)/fitz/stm_buffer.c \
	$(MY_ROOT)/fitz/stm_comp_buf.c \
	$(MY_ROOT)/fitz/stm_open.c \
	$(MY_ROOT)/fitz/stm_read.c \
	$(MY_ROOT)/draw/draw_affine.c \
	$(MY_ROOT)/draw/draw_blend.c \
	$(MY_ROOT)/draw/draw_device.c \
	$(MY_ROOT)/draw/draw_edge.c \
	$(MY_ROOT)/draw/draw_glyph.c \
	$(MY_ROOT)/draw/draw_mesh.c \
	$(MY_ROOT)/draw/draw_paint.c \
	$(MY_ROOT)/draw/draw_path.c \
	$(MY_ROOT)/draw/draw_simple_scale.c \
	$(MY_ROOT)/draw/draw_unpack.c \
	$(MY_ROOT)/pdf/pdf_annot.c \
	$(MY_ROOT)/pdf/pdf_cmap.c \
	$(MY_ROOT)/pdf/pdf_cmap_load.c \
	$(MY_ROOT)/pdf/pdf_cmap_parse.c \
	$(MY_ROOT)/pdf/pdf_cmap_table.c \
	$(MY_ROOT)/pdf/pdf_colorspace.c \
	$(MY_ROOT)/pdf/pdf_crypt.c \
	$(MY_ROOT)/pdf/pdf_encoding.c \
	$(MY_ROOT)/pdf/pdf_event.c \
	$(MY_ROOT)/pdf/pdf_font.c \
	$(MY_ROOT)/pdf/pdf_fontfile.c \
	$(MY_ROOT)/pdf/pdf_form.c \
	$(MY_ROOT)/pdf/pdf_function.c \
	$(MY_ROOT)/pdf/pdf_image.c \
	$(MY_ROOT)/pdf/pdf_interpret.c \
	$(MY_ROOT)/pdf/pdf_lex.c \
	$(MY_ROOT)/pdf/pdf_metrics.c \
	$(MY_ROOT)/pdf/pdf_nametree.c \
	$(MY_ROOT)/pdf/pdf_object.c \
	$(MY_ROOT)/pdf/pdf_outline.c \
	$(MY_ROOT)/pdf/pdf_page.c \
	$(MY_ROOT)/pdf/pdf_parse.c \
	$(MY_ROOT)/pdf/pdf_pattern.c \
	$(MY_ROOT)/pdf/pdf_repair.c \
	$(MY_ROOT)/pdf/pdf_shade.c \
	$(MY_ROOT)/pdf/pdf_stream.c \
	$(MY_ROOT)/pdf/pdf_store.c \
	$(MY_ROOT)/pdf/pdf_type3.c \
	$(MY_ROOT)/pdf/pdf_unicode.c \
	$(MY_ROOT)/pdf/pdf_write.c \
	$(MY_ROOT)/pdf/pdf_xobject.c \
	$(MY_ROOT)/pdf/pdf_xref.c \
	$(MY_ROOT)/pdf/pdf_xref_aux.c \
	$(MY_ROOT)/xps/xps_common.c \
	$(MY_ROOT)/xps/xps_doc.c \
	$(MY_ROOT)/xps/xps_glyphs.c \
	$(MY_ROOT)/xps/xps_gradient.c \
	$(MY_ROOT)/xps/xps_image.c \
	$(MY_ROOT)/xps/xps_outline.c \
	$(MY_ROOT)/xps/xps_path.c \
	$(MY_ROOT)/xps/xps_resource.c \
	$(MY_ROOT)/xps/xps_tile.c \
	$(MY_ROOT)/xps/xps_util.c \
	$(MY_ROOT)/xps/xps_zip.c \
	$(MY_ROOT)/cbz/mucbz.c
ifdef V8_BUILD
LOCAL_SRC_FILES += \
	$(MY_ROOT)/pdf/pdf_js.c \
	$(MY_ROOT)/pdf/pdf_jsimp_cpp.c \
	$(MY_ROOT)/pdf/pdf_jsimp_v8.cpp
else
LOCAL_SRC_FILES += \
	$(MY_ROOT)/pdf/pdf_js_none.c
endif

LOCAL_LDLIBS    := -lm -llog -ljnigraphics

include $(BUILD_STATIC_LIBRARY)

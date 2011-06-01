LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_ROOT := ../..

LOCAL_C_INCLUDES := \
	../thirdparty/jbig2dec \
	../thirdparty/openjpeg-1.4/libopenjpeg \
	../thirdparty/jpeg-8c \
	../thirdparty/zlib-1.2.5 \
	../thirdparty/freetype-2.4.4/include \
	../draw \
	../fitz \
	../pdf \
	../scripts \
	..

LOCAL_MODULE    := mupdfcore
LOCAL_SRC_FILES := \
	$(MY_ROOT)/fitz/base_error.c \
	$(MY_ROOT)/fitz/base_geometry.c \
	$(MY_ROOT)/fitz/base_getopt.c \
	$(MY_ROOT)/fitz/base_hash.c \
	$(MY_ROOT)/fitz/base_memory.c \
	$(MY_ROOT)/fitz/base_object.c \
	$(MY_ROOT)/fitz/base_string.c \
	$(MY_ROOT)/fitz/base_time.c \
	$(MY_ROOT)/fitz/crypt_aes.c \
	$(MY_ROOT)/fitz/crypt_arc4.c \
	$(MY_ROOT)/fitz/crypt_md5.c \
	$(MY_ROOT)/fitz/crypt_sha2.c \
	$(MY_ROOT)/fitz/dev_bbox.c \
	$(MY_ROOT)/fitz/dev_list.c \
	$(MY_ROOT)/fitz/dev_null.c \
	$(MY_ROOT)/fitz/dev_text.c \
	$(MY_ROOT)/fitz/dev_trace.c \
	$(MY_ROOT)/fitz/filt_basic.c \
	$(MY_ROOT)/fitz/filt_dctd.c \
	$(MY_ROOT)/fitz/filt_faxd.c \
	$(MY_ROOT)/fitz/filt_flate.c \
	$(MY_ROOT)/fitz/filt_jbig2d.c \
	$(MY_ROOT)/fitz/filt_jpxd.c \
	$(MY_ROOT)/fitz/filt_lzwd.c \
	$(MY_ROOT)/fitz/filt_predict.c \
	$(MY_ROOT)/fitz/obj_print.c \
	$(MY_ROOT)/fitz/res_colorspace.c \
	$(MY_ROOT)/fitz/res_font.c \
	$(MY_ROOT)/fitz/res_path.c \
	$(MY_ROOT)/fitz/res_pixmap.c \
	$(MY_ROOT)/fitz/res_shade.c \
	$(MY_ROOT)/fitz/res_text.c \
	$(MY_ROOT)/fitz/stm_buffer.c \
	$(MY_ROOT)/fitz/stm_open.c \
	$(MY_ROOT)/fitz/stm_read.c \
	$(MY_ROOT)/draw/arch_arm.c \
	$(MY_ROOT)/draw/arch_port.c \
	$(MY_ROOT)/draw/draw_affine.c \
	$(MY_ROOT)/draw/draw_blend.c \
	$(MY_ROOT)/draw/draw_device.c \
	$(MY_ROOT)/draw/draw_edge.c \
	$(MY_ROOT)/draw/draw_glyph.c \
	$(MY_ROOT)/draw/draw_mesh.c \
	$(MY_ROOT)/draw/draw_paint.c \
	$(MY_ROOT)/draw/draw_path.c \
	$(MY_ROOT)/draw/draw_scale.c \
	$(MY_ROOT)/draw/draw_unpack.c \
	$(MY_ROOT)/pdf/pdf_annot.c \
	$(MY_ROOT)/pdf/pdf_cmap.c \
	$(MY_ROOT)/pdf/pdf_cmap_load.c \
	$(MY_ROOT)/pdf/pdf_cmap_parse.c \
	$(MY_ROOT)/pdf/pdf_cmap_table.c \
	$(MY_ROOT)/pdf/pdf_colorspace.c \
	$(MY_ROOT)/pdf/pdf_crypt.c \
	$(MY_ROOT)/pdf/pdf_encoding.c \
	$(MY_ROOT)/pdf/pdf_font.c \
	$(MY_ROOT)/pdf/pdf_fontfile.c \
	$(MY_ROOT)/pdf/pdf_function.c \
	$(MY_ROOT)/pdf/pdf_image.c \
	$(MY_ROOT)/pdf/pdf_interpret.c \
	$(MY_ROOT)/pdf/pdf_lex.c \
	$(MY_ROOT)/pdf/pdf_metrics.c \
	$(MY_ROOT)/pdf/pdf_nametree.c \
	$(MY_ROOT)/pdf/pdf_outline.c \
	$(MY_ROOT)/pdf/pdf_page.c \
	$(MY_ROOT)/pdf/pdf_parse.c \
	$(MY_ROOT)/pdf/pdf_pattern.c \
	$(MY_ROOT)/pdf/pdf_repair.c \
	$(MY_ROOT)/pdf/pdf_shade.c \
	$(MY_ROOT)/pdf/pdf_store.c \
	$(MY_ROOT)/pdf/pdf_stream.c \
	$(MY_ROOT)/pdf/pdf_type3.c \
	$(MY_ROOT)/pdf/pdf_unicode.c \
	$(MY_ROOT)/pdf/pdf_xobject.c \
	$(MY_ROOT)/pdf/pdf_xref.c

LOCAL_LDLIBS    := -lm -llog -ljnigraphics

include $(BUILD_STATIC_LIBRARY)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_ROOT := ../..

LOCAL_CFLAGS := \
	-I$(LOCAL_PATH)/$(MY_ROOT)/thirdparty/jbig2dec \
	-I$(LOCAL_PATH)/$(MY_ROOT)/thirdparty/openjpeg/libopenjpeg \
	-I$(LOCAL_PATH)/$(MY_ROOT)/thirdparty/jpeg \
	-I$(LOCAL_PATH)/$(MY_ROOT)/thirdparty/zlib \
	-I$(LOCAL_PATH)/$(MY_ROOT)/thirdparty/freetype/include \
	-I$(LOCAL_PATH)/$(MY_ROOT)/draw \
	-I$(LOCAL_PATH)/$(MY_ROOT)/fitz \
	-I$(LOCAL_PATH)/$(MY_ROOT)/mupdf

LOCAL_MODULE    := mupdfcore
LOCAL_SRC_FILES := \
	$(MY_ROOT)/fitz/base_error.c \
	$(MY_ROOT)/fitz/base_geometry.c \
	$(MY_ROOT)/fitz/base_getopt.c \
	$(MY_ROOT)/fitz/base_hash.c \
	$(MY_ROOT)/fitz/base_memory.c \
	$(MY_ROOT)/fitz/base_string.c \
	$(MY_ROOT)/fitz/base_time.c \
	$(MY_ROOT)/fitz/crypt_aes.c \
	$(MY_ROOT)/fitz/crypt_arc4.c \
	$(MY_ROOT)/fitz/crypt_md5.c \
	$(MY_ROOT)/fitz/dev_bbox.c \
	$(MY_ROOT)/fitz/dev_draw.c \
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
	$(MY_ROOT)/fitz/obj_array.c \
	$(MY_ROOT)/fitz/obj_dict.c \
	$(MY_ROOT)/fitz/obj_print.c \
	$(MY_ROOT)/fitz/obj_simple.c \
	$(MY_ROOT)/fitz/res_colorspace.c \
	$(MY_ROOT)/fitz/res_font.c \
	$(MY_ROOT)/fitz/res_path.c \
	$(MY_ROOT)/fitz/res_pixmap.c \
	$(MY_ROOT)/fitz/res_shade.c \
	$(MY_ROOT)/fitz/res_text.c \
	$(MY_ROOT)/fitz/stm_buffer.c \
	$(MY_ROOT)/fitz/stm_open.c \
	$(MY_ROOT)/fitz/stm_read.c \
	$(MY_ROOT)/draw/archport.c \
	$(MY_ROOT)/draw/blendmodes.c \
	$(MY_ROOT)/draw/glyphcache.c \
	$(MY_ROOT)/draw/imagedraw.c \
	$(MY_ROOT)/draw/imagescale.c \
	$(MY_ROOT)/draw/imagesmooth.c \
	$(MY_ROOT)/draw/imageunpack.c \
	$(MY_ROOT)/draw/meshdraw.c \
	$(MY_ROOT)/draw/pathfill.c \
	$(MY_ROOT)/draw/pathscan.c \
	$(MY_ROOT)/draw/pathstroke.c \
	$(MY_ROOT)/draw/porterduff.c \
	$(MY_ROOT)/mupdf/pdf_annot.c \
	$(MY_ROOT)/mupdf/pdf_build.c \
	$(MY_ROOT)/mupdf/pdf_cmap.c \
	$(MY_ROOT)/mupdf/pdf_cmap_load.c \
	$(MY_ROOT)/mupdf/pdf_cmap_parse.c \
	$(MY_ROOT)/mupdf/pdf_cmap_table.c \
	$(MY_ROOT)/mupdf/pdf_colorspace.c \
	$(MY_ROOT)/mupdf/pdf_crypt.c \
	$(MY_ROOT)/mupdf/pdf_debug.c \
	$(MY_ROOT)/mupdf/pdf_font.c \
	$(MY_ROOT)/mupdf/pdf_fontagl.c \
	$(MY_ROOT)/mupdf/pdf_fontenc.c \
	$(MY_ROOT)/mupdf/pdf_fontfile.c \
	$(MY_ROOT)/mupdf/pdf_fontmtx.c \
	$(MY_ROOT)/mupdf/pdf_function.c \
	$(MY_ROOT)/mupdf/pdf_image.c \
	$(MY_ROOT)/mupdf/pdf_interpret.c \
	$(MY_ROOT)/mupdf/pdf_lex.c \
	$(MY_ROOT)/mupdf/pdf_nametree.c \
	$(MY_ROOT)/mupdf/pdf_outline.c \
	$(MY_ROOT)/mupdf/pdf_page.c \
	$(MY_ROOT)/mupdf/pdf_pagetree.c \
	$(MY_ROOT)/mupdf/pdf_parse.c \
	$(MY_ROOT)/mupdf/pdf_pattern.c \
	$(MY_ROOT)/mupdf/pdf_repair.c \
	$(MY_ROOT)/mupdf/pdf_shade.c \
	$(MY_ROOT)/mupdf/pdf_store.c \
	$(MY_ROOT)/mupdf/pdf_stream.c \
	$(MY_ROOT)/mupdf/pdf_type3.c \
	$(MY_ROOT)/mupdf/pdf_unicode.c \
	$(MY_ROOT)/mupdf/pdf_xobject.c \
	$(MY_ROOT)/mupdf/pdf_xref.c \
	$(MY_ROOT)/pregen/cmap_unicode.c \
	$(MY_ROOT)/pregen/cmap_cns.c \
	$(MY_ROOT)/pregen/cmap_gb.c \
	$(MY_ROOT)/pregen/cmap_japan.c \
	$(MY_ROOT)/pregen/cmap_korea.c \
	$(MY_ROOT)/pregen/font_base14.c \
	$(MY_ROOT)/pregen/font_cjk.c

LOCAL_LDLIBS    := -lm -llog -ljnigraphics

include $(BUILD_STATIC_LIBRARY)

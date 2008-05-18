# Makefile for Fitz library

BUILD = release

CC = gcc
CPPFLAGS =
CFLAGS =
AR = ar
INC_DIR =

-include config.mk

# TODO: precompiled headers?
# TODO: output into specific build directory depending on configuration

INC_DIR += fitz/include

ifeq ($(BUILD), debug)
    CFLAGS += -O0
endif
ifeq ($(BUILD), release)
    CPPFLAGS += -DNDEBUG
    CFLAGS += -O2
endif
ifneq ($(BUILD), debug)
    ifneq ($(BUILD), release)
        $(error BUILD should be "debug" or "release")
    endif
endif

CPPFLAGS += $(addprefix -I ,$(INC_DIR))
CFLAGS += -Wall
CFLAGS += -g
CPPFLAGS += -DNEED_STRLCAT -DNEED_STRLCPY -DNEED_STRSEP
CFLAGS += -std=gnu99
CPPFLAGS += -DHAVE_C99

all: libfitz.a
.PHONY: all

# TODO: automatic header dependancies stuff

SOURCES += fitz/base/base_cpudep.c
SOURCES += fitz/base/base_error.c
SOURCES += fitz/base/base_hash.c
SOURCES += fitz/base/base_matrix.c
SOURCES += fitz/base/base_memory.c
SOURCES += fitz/base/base_rect.c
SOURCES += fitz/base/base_rune.c
SOURCES += fitz/stream/crypt_arc4.c
SOURCES += fitz/stream/crypt_crc32.c
SOURCES += fitz/stream/crypt_md5.c
SOURCES += fitz/stream/filt_a85d.c
SOURCES += fitz/stream/filt_a85e.c
SOURCES += fitz/stream/filt_ahxd.c
SOURCES += fitz/stream/filt_ahxe.c
SOURCES += fitz/stream/filt_arc4.c
SOURCES += fitz/stream/filt_dctd.c
SOURCES += fitz/stream/filt_dcte.c
SOURCES += fitz/stream/filt_faxd.c
SOURCES += fitz/stream/filt_faxdtab.c
SOURCES += fitz/stream/filt_faxe.c
SOURCES += fitz/stream/filt_faxetab.c
SOURCES += fitz/stream/filt_flate.c
SOURCES += fitz/stream/filt_lzwd.c
SOURCES += fitz/stream/filt_lzwe.c
SOURCES += fitz/stream/filt_null.c
SOURCES += fitz/stream/filt_pipeline.c
SOURCES += fitz/stream/filt_predict.c
SOURCES += fitz/stream/filt_rld.c
SOURCES += fitz/stream/filt_rle.c
SOURCES += fitz/raster/glyphcache.c
SOURCES += fitz/raster/imagedraw.c
SOURCES += fitz/raster/imagescale.c
SOURCES += fitz/raster/imageunpack.c
SOURCES += fitz/raster/meshdraw.c
SOURCES += fitz/world/node_misc1.c
SOURCES += fitz/world/node_misc2.c
SOURCES += fitz/world/node_optimize.c
SOURCES += fitz/world/node_path.c
SOURCES += fitz/world/node_text.c
SOURCES += fitz/world/node_tolisp.c
SOURCES += fitz/world/node_tree.c
SOURCES += fitz/stream/obj_array.c
SOURCES += fitz/stream/obj_dict.c
SOURCES += fitz/stream/obj_parse.c
SOURCES += fitz/stream/obj_print.c
SOURCES += fitz/stream/obj_simple.c
SOURCES += fitz/raster/pathfill.c
SOURCES += fitz/raster/pathscan.c
SOURCES += fitz/raster/pathstroke.c
SOURCES += fitz/mupdf/pdf_annot.c
SOURCES += fitz/mupdf/pdf_build.c
SOURCES += fitz/mupdf/pdf_cmap.c
SOURCES += fitz/mupdf/pdf_colorspace1.c
SOURCES += fitz/mupdf/pdf_colorspace2.c
SOURCES += fitz/mupdf/pdf_crypt.c
SOURCES += fitz/mupdf/pdf_debug.c
SOURCES += fitz/mupdf/pdf_doctor.c
SOURCES += fitz/mupdf/pdf_font.c
SOURCES += fitz/mupdf/pdf_fontagl.c
SOURCES += fitz/mupdf/pdf_fontenc.c
SOURCES += fitz/mupdf/pdf_fontfile.c
SOURCES += fitz/mupdf/pdf_function.c
SOURCES += fitz/mupdf/pdf_image.c
SOURCES += fitz/mupdf/pdf_interpret.c
SOURCES += fitz/mupdf/pdf_lex.c
SOURCES += fitz/mupdf/pdf_nametree.c
SOURCES += fitz/mupdf/pdf_open.c
SOURCES += fitz/mupdf/pdf_outline.c
SOURCES += fitz/mupdf/pdf_page.c
SOURCES += fitz/mupdf/pdf_pagetree.c
SOURCES += fitz/mupdf/pdf_parse.c
SOURCES += fitz/mupdf/pdf_pattern.c
SOURCES += fitz/mupdf/pdf_repair.c
SOURCES += fitz/mupdf/pdf_resources.c
SOURCES += fitz/mupdf/pdf_save.c
SOURCES += fitz/mupdf/pdf_shade.c
SOURCES += fitz/mupdf/pdf_shade1.c
SOURCES += fitz/mupdf/pdf_shade4.c
SOURCES += fitz/mupdf/pdf_store.c
SOURCES += fitz/mupdf/pdf_stream.c
SOURCES += fitz/mupdf/pdf_type3.c
SOURCES += fitz/mupdf/pdf_unicode.c
SOURCES += fitz/mupdf/pdf_xobject.c
SOURCES += fitz/mupdf/pdf_xref.c
SOURCES += fitz/raster/pixmap.c
SOURCES += fitz/raster/porterduff.c
SOURCES += fitz/raster/render.c
SOURCES += fitz/world/res_colorspace.c
SOURCES += fitz/world/res_font.c
SOURCES += fitz/world/res_image.c
SOURCES += fitz/world/res_shade.c
SOURCES += fitz/stream/stm_buffer.c
SOURCES += fitz/stream/stm_filter.c
SOURCES += fitz/stream/stm_misc.c
SOURCES += fitz/stream/stm_open.c
SOURCES += fitz/stream/stm_read.c
SOURCES += fitz/stream/stm_write.c

# Replacements for common functions:
SOURCES += fitz/base/util_strlcat.c
SOURCES += fitz/base/util_strlcpy.c
SOURCES += fitz/base/util_strsep.c

HEADERS += fitz/include/mupdf/annot.h
HEADERS += fitz/include/mupdf/base14.h
HEADERS += fitz/include/fitz/base_cpudep.h
HEADERS += fitz/include/fitz/base_geom.h
HEADERS += fitz/include/fitz/base_hash.h
HEADERS += fitz/include/fitz/base_math.h
HEADERS += fitz/include/fitz/base_pixmap.h
HEADERS += fitz/include/fitz/base_runtime.h
HEADERS += fitz/include/fitz/base_sysdep.h
HEADERS += fitz/include/mupdf/content.h
HEADERS += fitz/include/fitz/draw_misc.h
HEADERS += fitz/include/fitz/draw_path.h
HEADERS += fitz/include/fitz-base.h
HEADERS += fitz/include/fitz-draw.h
HEADERS += fitz/include/fitz-stream.h
HEADERS += fitz/include/fitz-world.h
HEADERS += fitz/include/fitz.h
HEADERS += fitz/include/samus/fixdoc.h
HEADERS += fitz/include/samus/misc.h
HEADERS += fitz/include/mupdf.h
HEADERS += fitz/include/samus/names.h
HEADERS += fitz/include/samus/pack.h
HEADERS += fitz/include/mupdf/page.h
HEADERS += fitz/include/mupdf/rsrc.h
HEADERS += fitz/include/samus.h
HEADERS += fitz/include/fitz/stm_buffer.h
HEADERS += fitz/include/fitz/stm_crypt.h
HEADERS += fitz/include/fitz/stm_filter.h
HEADERS += fitz/include/fitz/stm_object.h
HEADERS += fitz/include/fitz/stm_stream.h
HEADERS += fitz/include/mupdf/syntax.h
HEADERS += fitz/include/win_os.h
HEADERS += fitz/include/fitz/wld_color.h
HEADERS += fitz/include/fitz/wld_font.h
HEADERS += fitz/include/fitz/wld_image.h
HEADERS += fitz/include/fitz/wld_path.h
HEADERS += fitz/include/fitz/wld_shade.h
HEADERS += fitz/include/fitz/wld_text.h
HEADERS += fitz/include/fitz/wld_tree.h
HEADERS += fitz/include/samus/xml.h
HEADERS += fitz/include/mupdf/xref.h
HEADERS += fitz/include/samus/zip.h

FONTS += fitz/fonts/Dingbats.cff.c
FONTS += fitz/fonts/NimbusMonL-Bold.cff.c
FONTS += fitz/fonts/NimbusMonL-BoldObli.cff.c
FONTS += fitz/fonts/NimbusMonL-Regu.cff.c
FONTS += fitz/fonts/NimbusMonL-ReguObli.cff.c
FONTS += fitz/fonts/NimbusRomNo9L-Medi.cff.c
FONTS += fitz/fonts/NimbusRomNo9L-MediItal.cff.c
FONTS += fitz/fonts/NimbusRomNo9L-Regu.cff.c
FONTS += fitz/fonts/NimbusRomNo9L-ReguItal.cff.c
FONTS += fitz/fonts/NimbusSanL-Bold.cff.c
FONTS += fitz/fonts/NimbusSanL-BoldItal.cff.c
FONTS += fitz/fonts/NimbusSanL-Regu.cff.c
FONTS += fitz/fonts/NimbusSanL-ReguItal.cff.c
FONTS += fitz/fonts/StandardSymL.cff.c
FONTS += fitz/fonts/URWChanceryL-MediItal.cff.c

OBJECTS = $(addsuffix .o,$(SOURCES) $(FONTS))

.SUFFIXES:

libfitz.a: $(OBJECTS)
	@echo AR r $@
	@$(AR) r $@ $?

$(OBJECTS): %.o: %
	@echo CC $(CPPFLAGS_$<) $(CFLAGS_$<) -c $<
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(CPPFLAGS_$<) $(CFLAGS_$<) -c $< -o $@

clean:
	@echo RM libfitz.a OBJECTS
	@rm -f libfitz.a $(OBJECTS)
.PHONY: clean

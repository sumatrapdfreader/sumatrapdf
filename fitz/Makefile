# Makefile for building mupdf and related stuff
# Valid option to make:
# CFG=[rel|dbg] - dbg if not given
# JBIG2_DIR=
# JASPER_DIR=

# Symbolic names for HOST variable
HOST_LINUX := Linux
HOST_MAC := Darwin
HOST_CYGWIN := CYGWIN_NT-6.0

# HOST can be: Linux, Darwin, CYGWIN_NT-6.0
HOST := $(shell uname -s)

VPATH=base:raster:world:stream:mupdf:apps:$(JBIG2_DIR)
ifneq ($(JASPER_DIR),)
VPATH+=$(JASPER_DIR)/base:$(JASPER_DIR)/jp2:$(JASPER_DIR)/pgx:$(JASPER_DIR)/jpc
endif

# make dbg default target if none provided
ifeq ($(CFG),)
CFG=dbg
endif

INCS = -I include -I cmaps

ifneq ($(JBIG2_DIR),)
INCS += -I $(JBIG2_DIR)
CFLAGS += -DHAVE_JBIG2DEC
endif

ifneq ($(JASPER_DIR),)
INCS += -I $(JASPER_DIR)/include
CFLAGS += -DHAVE_JASPER -DJAS_CONFIGURE
endif

FREETYPE_CFLAGS  = `freetype-config --cflags`
FREETYPE_LDFLAGS = `freetype-config --libs`

FONTCONFIG_CFLAGS  = `pkg-config fontconfig --cflags`
FONTCONFIG_LDFLAGS = `pkg-config fontconfig --libs`

# cc-option
# Usage: OP_CFLAGS+=$(call cc-option, -falign-functions=0, -malign-functions=0)

cc-option = $(shell if $(CC) $(OP_CFLAGS) $(1) -S -o /dev/null -xc /dev/null \
              > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

CFLAGS += -g -Wall
CFLAGS += $(call cc-option, -Wno-pointer-sign, "")

ifeq ($(CFG),dbg)
CFLAGS += -O0  ${INCS}
else
CFLAGS += -O2 ${INCS} -DNDEBUG
endif

ifeq ($(HOST),$(HOST_LINUX))
CFLAGS += -std=gnu99 -DHAVE_C99
endif

ifeq ($(HOST),$(HOST_MAC))
CFLAGS += -std=gnu99 -DHAVE_C99
endif

JBIG2_CFLAGS = $(CFLAGS) -DHAVE_STDINT_H
JASPER_CFLAGS = $(CFLAGS) -DEXCLUDE_MIF_SUPPORT -DEXCLUDE_PNM_SUPPORT -DEXCLUDE_BMP_SUPPORT -DEXCLUDE_RAS_SUPPORT -DEXCLUDE_JPG_SUPPORT

#-DHAVE_CONFIG_H

CFLAGS += ${FREETYPE_CFLAGS} ${FONTCONFIG_CFLAGS}

LDFLAGS += ${FREETYPE_LDFLAGS} ${FONTCONFIG_LDFLAGS} -lm -ljpeg

CFLAGS += -DUSE_STATIC_CMAPS
CFLAGS += -DDUMP_STATIC_CMAPS

OUTDIR=obj-$(CFG)

JBIG2_SRC = \
	jbig2.c jbig2_arith.c jbig2_arith_int.c jbig2_arith_iaid.c \
	jbig2_huffman.c jbig2_segment.c jbig2_page.c \
	jbig2_symbol_dict.c jbig2_text.c \
	jbig2_generic.c jbig2_refinement.c jbig2_mmr.c \
	jbig2_image.c jbig2_metadata.c

JASPER_SRC = \
	jas_cm.c jas_debug.c jas_icc.c \
	jas_iccdata.c jas_image.c jas_init.c jas_malloc.c \
	jas_seq.c jas_stream.c jas_string.c jas_tvp.c \
	jas_version.c \
	jp2_cod.c jp2_dec.c jp2_enc.c \
	jpc_bs.c jpc_cs.c jpc_dec.c jpc_enc.c \
	jpc_math.c jpc_mct.c jpc_mqcod.c jpc_mqdec.c \
	jpc_mqenc.c jpc_qmfb.c jpc_t1cod.c jpc_t1dec.c \
	jpc_t1enc.c jpc_t2cod.c jpc_t2dec.c jpc_t2enc.c \
	jpc_tagtree.c jpc_tsfb.c jpc_util.c \
	pgx_cod.c pgx_dec.c pgx_enc.c

BASE_SRC = \
	base_memory.c \
	base_error.c \
	base_hash.c \
	base_matrix.c \
	base_rect.c \
	base_rune.c \

ifeq ($(HOST),$(HOST_LINUX))
BASE_SRC += \
	util_strlcat.c \
	util_strlcpy.c

CFLAGS += -DNEED_STRLCPY
endif

#./base/base_cleanname.c
#./base/base_cpudep.c

STREAM_SRC = \
	crypt_aes.c \
	crypt_arc4.c \
	crypt_crc32.c \
	crypt_md5.c \
	filt_a85d.c \
	filt_a85e.c \
	filt_aes.c \
	filt_ahxd.c \
	filt_ahxe.c \
	filt_arc4.c \
	filt_faxd.c \
	filt_faxdtab.c \
	filt_faxe.c \
	filt_faxetab.c \
	filt_flate.c \
	filt_lzwd.c \
	filt_lzwe.c \
	filt_null.c \
	filt_pipeline.c \
	filt_predict.c \
	filt_rld.c \
	filt_rle.c \
	obj_array.c \
	obj_dict.c \
	obj_parse.c \
	obj_print.c \
	obj_simple.c \
	stm_buffer.c \
	stm_filter.c \
	stm_misc.c \
	stm_open.c \
	stm_read.c \
	stm_write.c \
	filt_dctd.c \
	filt_dcte.c \

ifneq ($(JASPER_DIR),)
STREAM_SRC += filt_jpxd.c
endif

ifneq ($(JBIG2_DIR),)
STREAM_SRC += filt_jbig2d.c
endif

RASTER_SRC = \
	archx86.c \
	blendmodes.c \
	imagescale.c \
	pathfill.c \
	pixmap.c \
	glyphcache.c \
	imageunpack.c \
	pathscan.c \
	porterduff.c \
	imagedraw.c \
	meshdraw.c \
	pathstroke.c \
	render.c \

WORLD_SRC = \
	node_misc1.c \
	node_misc2.c \
	node_optimize.c \
	node_path.c \
	node_text.c \
	node_toxml.c \
	node_tree.c \
	res_colorspace.c \
	res_font.c \
	res_image.c \
	res_shade.c \

MUPDF_SRC = \
	pdf_annot.c \
	pdf_build.c \
	pdf_cmap.c \
	pdf_colorspace1.c \
	pdf_colorspace2.c \
	pdf_crypt.c \
	pdf_debug.c \
	pdf_doctor.c \
	pdf_font.c \
	pdf_fontagl.c \
	pdf_fontenc.c \
	pdf_function.c \
	pdf_image.c \
	pdf_interpret.c \
	pdf_lex.c \
	pdf_nametree.c \
	pdf_open.c \
	pdf_outline.c \
	pdf_page.c \
	pdf_pagetree.c \
	pdf_parse.c \
	pdf_pattern.c \
	pdf_repair.c \
	pdf_resources.c \
	pdf_save.c \
	pdf_shade.c \
	pdf_shade1.c \
	pdf_shade4.c \
	pdf_store.c \
	pdf_stream.c \
	pdf_type3.c \
	pdf_unicode.c \
	pdf_xobject.c \
	pdf_xref.c \
	pdf_fontfilefc.c \

LIBS_SRC = \
	${BASE_SRC} \
	${STREAM_SRC} \
	${RASTER_SRC} \
	${WORLD_SRC} \
	${MUPDF_SRC}

JASPER_OBJ = $(patsubst %.c, $(OUTDIR)/JASPER_%.o, ${JASPER_SRC})
JASPER_DEP = $(patsubst %.o, %.d, $(JASPER_OBJ))

JBIG2_OBJ = $(patsubst %.c, $(OUTDIR)/JBIG_%.o, ${JBIG2_SRC})
JBIG2_DEP = $(patsubst %.o, %.d, $(JBIG2_OBJ))

LIBS_OBJ = $(patsubst %.c, $(OUTDIR)/FITZ_%.o, ${LIBS_SRC})
LIBS_DEP = $(patsubst %.o, %.d, $(LIBS_OBJ))

PDFTOOL_SRC = pdftool.c
PDFTOOL_OBJ = $(patsubst %.c, $(OUTDIR)/FITZ_%.o, ${PDFTOOL_SRC})
PDFTOOL_DEP = $(patsubst %.o, %.d, $(PDFTOOL_OBJ))

PDFBENCH_SRC = pdfbench.c
PDFBENCH_OBJ = $(patsubst %.c, $(OUTDIR)/FITZ_%.o, ${PDFBENCH_SRC})
PDFBENCH_DEP = $(patsubst %.o, %.d, $(PDFBENCH_OBJ))

ifneq ($(JASPER_DIR),)
PDFTOOL_OBJ += $(JASPER_OBJ)
PDFBENCH_OBJ += $(JASPER_OBJ)
endif

ifneq ($(JBIG2_DIR),)
PDFTOOL_OBJ += $(JBIG2_OBJ)
PDFBENCH_OBJ += $(JBIG2_OBJ)
endif

PDFTOOL_APP = ${OUTDIR}/pdftool

PDFBENCH_APP = ${OUTDIR}/pdfbench

all: inform ${OUTDIR} ${PDFTOOL_APP} ${PDFBENCH_APP}

$(OUTDIR):
	@mkdir -p $(OUTDIR)

$(PDFTOOL_APP): ${LIBS_OBJ} ${PDFTOOL_OBJ}
	$(CC) -g -o $@ $^ ${LDFLAGS}

$(PDFBENCH_APP): ${LIBS_OBJ} ${PDFBENCH_OBJ}
	$(CC) -g -o $@ $^ ${LDFLAGS}

$(OUTDIR)/FITZ_%.o: %.c
	$(CC) -MD -c $(CFLAGS) -o $@ $<

$(OUTDIR)/JBIG_%.o: %.c
	$(CC) -MD -c $(JBIG2_CFLAGS) -o $@ $<

$(OUTDIR)/JASPER_%.o: %.c
	$(CC) -MD -c $(JASPER_CFLAGS) -o $@ $<

-include $(LIBS_DEP)
-include $(JASPER_DEP)
-include $(JBIG2_DEP)
-include $(PDFTOOL_DEP)
-include $(PDFBENCH_DEP)

inform:
ifneq ($(CFG),rel)
ifneq ($(CFG),dbg)
	@echo "Invalid configuration: '"$(CFG)"'"
	@echo "Valid configurations: rel, dbg (e.g. make CFG=dbg)"
	@exit 1
endif
endif

clean: inform
	rm -rf obj-$(CFG)

cleanall:
	rm -rf obj-*

#./apps/common/pdfapp.c
#./apps/mozilla/moz_main.c
#./apps/mozilla/npunix.c
#./apps/unix/x11pdf.c
#./apps/unix/ximage.c

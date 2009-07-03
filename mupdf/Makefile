# Makefile for building mupdf and related stuff
# Valid option to make:
# CFG=[rel|dbg|rel64|dbg64] - dbg if not given
# ZLIB_DIR=
# JPEG_DIR=
# JBIG2_DIR=
# JASPER_DIR=
# OPENJPEG_DIR=

# Symbolic names for HOST variable
HOST_LINUX := Linux
HOST_MAC := Darwin
HOST_CYGWIN_6 := CYGWIN_NT-6.0
HOST_CYGWIN_5 := CYGWIN_NT-5.1
HOST_CYGWIN := Cygwin

# HOST can be: Linux, Darwin, CYGWIN_NT-6.0
HOST := $(shell uname -s)

ifeq ($(HOST),$(HOST_CYGWIN_6))
HOST = $(HOST_CYGWIN)
endif

ifeq ($(HOST),$(HOST_CYGWIN_5))
HOST = $(HOST_CYGWIN)
endif

VPATH=fitzdraw:fitz:mupdf:fonts:cmaps:apps:$(JBIG2_DIR)

ifneq ($(ZLIB_DIR),)
VPATH+=$(ZLIB_DIR)
endif

ifneq ($(JPEG_DIR),)
VPATH+=$(JPEG_DIR)
endif

ifneq ($(JASPER_DIR),)
VPATH+=$(JASPER_DIR)/base:$(JASPER_DIR)/jp2:$(JASPER_DIR)/pgx:$(JASPER_DIR)/jpc
endif

ifneq ($(OPENJPEG_DIR),)
VPATH+=$(OPENJPEG_DIR)
endif

# make dbg default target if none provided
ifeq ($(CFG),)
CFG=dbg
endif

INCS = -I fitz -I mupdf -I cmaps

ifneq ($(JPEG_DIR),)
INCS += -I $(JPEG_DIR)
endif

ifneq ($(ZLIB_DIR),)
INCS += -I $(ZLIB_DIR)
endif

ifneq ($(JBIG2_DIR),)
INCS += -I $(JBIG2_DIR)
CFLAGS += -DHAVE_JBIG2DEC
endif

ifneq ($(JASPER_DIR),)
INCS += -I $(JASPER_DIR)/include
CFLAGS += -DHAVE_JASPER -DJAS_CONFIGURE
endif

ifneq ($(OPENJPEG_DIR),)
INCS += -I $(OPENJPEG_DIR)
CFLAGS += -DHAVE_OPENJPEG
endif

FREETYPE_CFLAGS  = `freetype-config --cflags`
FREETYPE_LDFLAGS = `freetype-config --libs`

# cc-option
# Usage: OP_CFLAGS+=$(call cc-option, -falign-functions=0, -malign-functions=0)

cc-option = $(shell if $(CC) $(OP_CFLAGS) $(1) -S -o /dev/null -xc /dev/null \
              > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

CFLAGS += -g -Wall
CFLAGS += $(call cc-option, -Wno-pointer-sign, "")

ifeq ($(CFG),dbg)
CFLAGS += -O0 -fno-inline ${INCS}
endif

ifeq ($(CFG),dbg64)
CFLAGS += -m64 -O0 -fno-inline ${INCS}
endif

ifeq ($(CFG),rel)
CFLAGS += -O3 ${INCS} -DNDEBUG
endif

ifeq ($(CFG),rel64)
CFLAGS += -m64 -O3 ${INCS} -DNDEBUG
endif

CFLAGS += -std=c99 -DHAVE_C99

ZLIB_CFLAGS = $(CFLAGS)
JPEG_CFLAGS = $(CFLAGS)
JBIG2_CFLAGS = $(CFLAGS) -DHAVE_STDINT_H
JASPER_CFLAGS = $(CFLAGS) -DEXCLUDE_MIF_SUPPORT -DEXCLUDE_PNM_SUPPORT -DEXCLUDE_BMP_SUPPORT \
	-DEXCLUDE_RAS_SUPPORT -DEXCLUDE_JPG_SUPPORT
OPENJPEG_CFLAGS = $(CFLAGS)

#-DHAVE_CONFIG_H

CFLAGS += ${FREETYPE_CFLAGS}

LDFLAGS += ${FREETYPE_LDFLAGS} -lm

#ifeq ($(JPEG_DIR),)
#LDFLAGS += -ljpeg
#endif

ifeq ($(CFG), rel64)
LDFLAGS += -m64
endif

ifeq ($(CFG), dbg64)
LDFLAGS += -m64
endif

ifeq ($(ZLIB_DIR),)
LDFLAGS += -lz
endif

OUTDIR=obj-$(CFG)

VPATH+=$(OUTDIR)

ZLIB_SRC = \
	adler32.c crc32.c inflate.c zutil.c \
	inffast.c inftrees.c deflate.c compress.c \
	trees.c \
	gzio.c

JPEG_SRC = \
	jcapimin.c jcapistd.c jccoefct.c jccolor.c jcdctmgr.c jchuff.c \
	jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c jcparam.c \
	jcphuff.c jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c \
	jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c \
	jdinput.c jdmainct.c jdmarker.c jdmaster.c jdmerge.c jdphuff.c \
	jdpostct.c jdsample.c jdtrans.c jerror.c jfdctflt.c jfdctfst.c \
	jfdctint.c jidctflt.c jidctfst.c jidctint.c jidctred.c jquant1.c \
	jquant2.c jutils.c jmemmgr.c jmemnobs.c

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

OPENJPEG_SRC = \
	bio.c cio.c dwt.c event.c image.c \
	j2k.c j2k_lib.c jp2.c jpt.c mct.c \
	mqc.c openjpeg.c pi.c raw.c t1.c \
	t2.c tcd.c tgt.c

FITZ_DRAW_SRC = \
	archx86.c blendmodes.c imagescale.c pathfill.c pixmap.c \
	glyphcache.c imageunpack.c pathscan.c porterduff.c imagedraw.c \
	meshdraw.c pathstroke.c render.c

FITZ_SRC = \
	base_memory.c base_error.c base_hash.c base_matrix.c base_rect.c \
	base_unicode.c base_string.c node_misc1.c node_misc2.c node_optimize.c node_path.c \
	node_text.c node_toxml.c node_tree.c res_colorspace.c res_font.c \
	res_image.c res_shade.c crypt_aes.c crypt_arc4.c crypt_crc32.c \
	crypt_md5.c filt_a85d.c filt_a85e.c filt_aes.c filt_ahxd.c filt_ahxe.c \
	filt_arc4.c filt_faxd.c filt_faxdtab.c filt_faxe.c filt_faxetab.c \
	filt_flate.c filt_lzwd.c filt_lzwe.c filt_null.c filt_pipeline.c \
	filt_predict.c filt_rld.c filt_rle.c obj_array.c obj_dict.c \
	obj_parse.c obj_print.c obj_simple.c stm_buffer.c stm_filter.c \
	stm_misc.c stm_open.c stm_read.c filt_dctd.c filt_dcte.c util_getopt.c

ifneq ($(JASPER_DIR),)
FITZ_SRC += filt_jpxd_jas.c
endif

ifneq ($(OPENJPEG_DIR),)
FITZ_SRC += filt_jpxd_opj.c
endif

ifneq ($(JBIG2_DIR),)
FITZ_SRC += filt_jbig2d.c
endif

MUPDF_SRC = \
	pdf_annot.c pdf_build.c pdf_cmap.c pdf_cmap_load.c pdf_cmap_parse.c \
	pdf_cmap_table.c pdf_colorspace1.c pdf_colorspace2.c pdf_crypt.c \
	pdf_debug.c pdf_font.c pdf_fontagl.c pdf_fontenc.c pdf_fontfile.c \
	pdf_fontmtx.c pdf_function.c pdf_image.c pdf_interpret.c pdf_lex.c \
	pdf_nametree.c pdf_open.c pdf_outline.c pdf_page.c pdf_pagetree.c \
	pdf_parse.c pdf_pattern.c pdf_repair.c pdf_resources.c pdf_shade.c \
	pdf_shade1.c pdf_shade4.c pdf_store.c pdf_stream.c pdf_type3.c \
	pdf_unicode.c pdf_xobject.c pdf_xref.c

FONTS_SRC = font_misc.c font_mono.c font_serif.c font_sans.c font_cjk.c

CMAPS_SRC = cmap_chinese_s.c cmap_chinese_t.c cmap_korean.c cmap_japanese.c

LIBS_SRC = \
	${CMAPS_SRC} \
	${FONTS_SRC} \
	${FITZ_DRAW_SRC} \
	${FITZ_SRC} \
	${MUPDF_SRC}

JPEG_OBJ = $(patsubst %.c, $(OUTDIR)/JPEG_%.o, ${JPEG_SRC})
JPEG_DEP = $(patsubst %.o, %.d, $(JPEG_OBJ))

ZLIB_OBJ = $(patsubst %.c, $(OUTDIR)/ZLIB_%.o, ${ZLIB_SRC})
ZLIB_DEP = $(patsubst %.o, %.d, $(ZLIB_OBJ))

JASPER_OBJ = $(patsubst %.c, $(OUTDIR)/JASPER_%.o, ${JASPER_SRC})
JASPER_DEP = $(patsubst %.o, %.d, $(JASPER_OBJ))

OPENJPEG_OBJ = $(patsubst %.c, $(OUTDIR)/OPENJPEG_%.o, ${OPENJPEG_SRC})
OPENJPEG_DEP = $(patsubst %.o, %.d, $(OPENJPEG_OBJ))

JBIG2_OBJ = $(patsubst %.c, $(OUTDIR)/JBIG_%.o, ${JBIG2_SRC})
JBIG2_DEP = $(patsubst %.o, %.d, $(JBIG2_OBJ))

LIBS_OBJ = $(patsubst %.c, $(OUTDIR)/FITZ_%.o, ${LIBS_SRC})
LIBS_DEP = $(patsubst %.o, %.d, $(LIBS_OBJ))

PDFBENCH_SRC = pdfbench.c
PDFBENCH_OBJ = $(patsubst %.c, $(OUTDIR)/FITZ_%.o, ${PDFBENCH_SRC})
PDFBENCH_DEP = $(patsubst %.o, %.d, $(PDFBENCH_OBJ))

PDFCLEAN_SRC = pdfclean.c
PDFCLEAN_OBJ = $(patsubst %.c, $(OUTDIR)/FITZ_%.o, ${PDFCLEAN_SRC})
PDFCLEAN_DEP = $(patsubst %.o, %.d, $(PDFCLEAN_OBJ))

PDFDRAW_SRC = pdfdraw.c
PDFDRAW_OBJ = $(patsubst %.c, $(OUTDIR)/FITZ_%.o, ${PDFDRAW_SRC})
PDFDRAW_DEP = $(patsubst %.o, %.d, $(PDFDRAW_OBJ))

PDFINFO_SRC = pdfinfo.c
PDFINFO_OBJ = $(patsubst %.c, $(OUTDIR)/FITZ_%.o, ${PDFINFO_SRC})
PDFINFO_DEP = $(patsubst %.o, %.d, $(PDFINFO_OBJ))

PDFSHOW_SRC = pdfshow.c
PDFSHOW_OBJ = $(patsubst %.c, $(OUTDIR)/FITZ_%.o, ${PDFSHOW_SRC})
PDFSHOW_DEP = $(patsubst %.o, %.d, $(PDFSHOW_OBJ))

FONTDUMP_SRC = fontdump.c
FONTDUMP_OBJ = $(patsubst %.c, $(OUTDIR)/FITZ_%.o, ${FONTDUMP_SRC})

CMAPDUMP_SRC = cmapdump.c base_error.c base_memory.c \
	stm_read.c stm_buffer.c stm_filter.c \
	stm_open.c base_string.c

CMAPDUMP_OBJ = $(patsubst %.c, $(OUTDIR)/FITZ_%.o, ${CMAPDUMP_SRC})

ifneq ($(ZLIB_DIR),)
PDFBENCH_OBJ += $(ZLIB_OBJ)
PDFCLEAN_OBJ += $(ZLIB_OBJ)
PDFDRAW_OBJ  += $(ZLIB_OBJ)
PDFINFO_OBJ  += $(ZLIB_OBJ)
PDFSHOW_OBJ  += $(ZLIB_OBJ)
endif

ifneq ($(JPEG_DIR),)
PDFBENCH_OBJ += $(JPEG_OBJ)
PDFCLEAN_OBJ += $(JPEG_OBJ)
PDFDRAW_OBJ  += $(JPEG_OBJ)
PDFINFO_OBJ  += $(JPEG_OBJ)
PDFSHOW_OBJ  += $(JPEG_OBJ)
endif

ifneq ($(JASPER_DIR),)
PDFBENCH_OBJ += $(JASPER_OBJ)
PDFCLEAN_OBJ += $(JASPER_OBJ)
PDFDRAW_OBJ  += $(JASPER_OBJ)
PDFINFO_OBJ  += $(JASPER_OBJ)
PDFSHOW_OBJ  += $(JASPER_OBJ)
endif

ifneq ($(OPENJPEG_DIR),)
PDFBENCH_OBJ += $(OPENJPEG_OBJ)
PDFCLEAN_OBJ += $(OPENJPEG_OBJ)
PDFDRAW_OBJ  += $(OPENJPEG_OBJ)
PDFINFO_OBJ  += $(OPENJPEG_OBJ)
PDFSHOW_OBJ  += $(OPENJPEG_OBJ)
endif

ifneq ($(JBIG2_DIR),)
PDFBENCH_OBJ += $(JBIG2_OBJ)
PDFCLEAN_OBJ += $(JBIG2_OBJ)
PDFDRAW_OBJ  += $(JBIG2_OBJ)
PDFINFO_OBJ  += $(JBIG2_OBJ)
PDFSHOW_OBJ  += $(JBIG2_OBJ)
endif

PDFBENCH_APP = ${OUTDIR}/pdfbench
PDFCLEAN_APP = ${OUTDIR}/pdfclean
PDFDRAW_APP  = ${OUTDIR}/pdfdraw
PDFINFO_APP  = ${OUTDIR}/pdfinfo
PDFSHOW_APP  = ${OUTDIR}/pdfshow
FONTDUMP_APP = ${OUTDIR}/fontdump
CMAPDUMP_APP = ${OUTDIR}/cmapdump

all: inform ${OUTDIR} ${CMAPDUMP_APP} ${PDFBENCH_APP} ${PDFCLEAN_APP} ${PDFDRAW_APP} ${PDFINFO_APP} ${PDFSHOW_APP}

pdfbench: inform ${OUTDIR} ${CMAPDUMP_APP} ${PDFBENCH_APP}

$(OUTDIR):
	@mkdir -p $(OUTDIR)

$(PDFBENCH_APP): ${LIBS_OBJ} ${PDFBENCH_OBJ}
	$(CC) -g -o $@ $^ ${LDFLAGS}

$(PDFCLEAN_APP): ${LIBS_OBJ} ${PDFCLEAN_OBJ}
	$(CC) -g -o $@ $^ ${LDFLAGS}

$(PDFDRAW_APP): ${LIBS_OBJ} ${PDFDRAW_OBJ}
	$(CC) -g -o $@ $^ ${LDFLAGS}

$(PDFINFO_APP): ${LIBS_OBJ} ${PDFINFO_OBJ}
	$(CC) -g -o $@ $^ ${LDFLAGS}

$(PDFSHOW_APP): ${LIBS_OBJ} ${PDFSHOW_OBJ}
	$(CC) -g -o $@ $^ ${LDFLAGS}

$(FONTDUMP_APP): ${FONTDUMP_OBJ}
	$(CC) -g -o $@ $^ ${LDFLAGS}

$(CMAPDUMP_APP): ${CMAPDUMP_OBJ}
	$(CC) -g -o $@ $^ ${LDFLAGS}

$(OUTDIR)/FITZ_%.o: %.c
	$(CC) -MD -c $(CFLAGS) -o $@ $<

$(OUTDIR)/ZLIB_%.o: %.c
	$(CC) -MD -c $(ZLIB_CFLAGS) -o $@ $<

$(OUTDIR)/JPEG_%.o: %.c
	$(CC) -MD -c $(JPEG_CFLAGS) -o $@ $<

$(OUTDIR)/JBIG_%.o: %.c
	$(CC) -MD -c $(JBIG2_CFLAGS) -o $@ $<

$(OUTDIR)/JASPER_%.o: %.c
	$(CC) -MD -c $(JASPER_CFLAGS) -o $@ $<

$(OUTDIR)/OPENJPEG_%.o: %.c
	$(CC) -MD -c $(OPENJPEG_CFLAGS) -o $@ $<

$(OUTDIR)/font_misc.c: $(FONTDUMP_APP)
	$(FONTDUMP_APP) $@ fonts/Dingbats.cff fonts/StandardSymL.cff fonts/URWChanceryL-MediItal.cff

$(OUTDIR)/font_mono.c: $(FONTDUMP_APP)
	$(FONTDUMP_APP) $@ fonts/NimbusMonL-Regu.cff fonts/NimbusMonL-ReguObli.cff fonts/NimbusMonL-Bold.cff fonts/NimbusMonL-BoldObli.cff

$(OUTDIR)/font_serif.c: $(FONTDUMP_APP)
	$(FONTDUMP_APP) $@ fonts/NimbusRomNo9L-Regu.cff fonts/NimbusRomNo9L-ReguItal.cff fonts/NimbusRomNo9L-Medi.cff fonts/NimbusRomNo9L-MediItal.cff

$(OUTDIR)/font_sans.c: $(FONTDUMP_APP)
	$(FONTDUMP_APP) $@ fonts/NimbusSanL-Bold.cff fonts/NimbusSanL-BoldItal.cff fonts/NimbusSanL-Regu.cff fonts/NimbusSanL-ReguItal.cff

$(OUTDIR)/font_cjk.c: $(FONTDUMP_APP)
	$(FONTDUMP_APP) $@ fonts/droid/DroidSansFallback.ttf

$(OUTDIR)/cmap_chinese_s.c: $(CMAPDUMP_APP)
	$(CMAPDUMP_APP) $(OUTDIR)/cmap_chinese_s.c cmaps/chinese_s/Adobe-GB1-4 \
	cmaps/chinese_s/Adobe-GB1-UCS2 cmaps/chinese_s/GB-EUC-H \
	cmaps/chinese_s/GB-EUC-V cmaps/chinese_s/GBK-EUC-H \
	cmaps/chinese_s/GBK-EUC-UCS2 cmaps/chinese_s/GBK-EUC-V \
	cmaps/chinese_s/GBK2K-H cmaps/chinese_s/GBK2K-V cmaps/chinese_s/GBKp-EUC-H \
	cmaps/chinese_s/GBKp-EUC-V cmaps/chinese_s/GBT-EUC-H cmaps/chinese_s/GBT-EUC-V \
	cmaps/chinese_s/GBpc-EUC-H cmaps/chinese_s/GBpc-EUC-UCS2 \
	cmaps/chinese_s/GBpc-EUC-UCS2C cmaps/chinese_s/GBpc-EUC-V \
	cmaps/chinese_s/UniGB-UCS2-H cmaps/chinese_s/UniGB-UCS2-V

$(OUTDIR)/cmap_chinese_t.c: $(CMAPDUMP_APP)
	$(CMAPDUMP_APP) $(OUTDIR)/cmap_chinese_t.c cmaps/chinese_t/Adobe-CNS1-3 \
	cmaps/chinese_t/Adobe-CNS1-UCS2 cmaps/chinese_t/B5pc-H \
	cmaps/chinese_t/B5pc-UCS2 cmaps/chinese_t/B5pc-UCS2C \
	cmaps/chinese_t/B5pc-V cmaps/chinese_t/CNS-EUC-H \
	cmaps/chinese_t/CNS-EUC-V cmaps/chinese_t/ETen-B5-H \
	cmaps/chinese_t/ETen-B5-UCS2 cmaps/chinese_t/ETen-B5-V \
	cmaps/chinese_t/ETenms-B5-H cmaps/chinese_t/ETenms-B5-V \
	cmaps/chinese_t/HKscs-B5-H cmaps/chinese_t/HKscs-B5-V \
	cmaps/chinese_t/UniCNS-UCS2-H cmaps/chinese_t/UniCNS-UCS2-V

$(OUTDIR)/cmap_japanese.c: $(CMAPDUMP_APP)
	$(CMAPDUMP_APP) $(OUTDIR)/cmap_japanese.c cmaps/japanese/83pv-RKSJ-H \
	cmaps/japanese/90ms-RKSJ-H cmaps/japanese/90ms-RKSJ-UCS2 \
	cmaps/japanese/90ms-RKSJ-V cmaps/japanese/90msp-RKSJ-H \
	cmaps/japanese/90msp-RKSJ-V cmaps/japanese/90pv-RKSJ-H \
	cmaps/japanese/90pv-RKSJ-UCS2 cmaps/japanese/90pv-RKSJ-UCS2C \
	cmaps/japanese/Add-RKSJ-H cmaps/japanese/Add-RKSJ-V \
	cmaps/japanese/Adobe-Japan1-4 cmaps/japanese/Adobe-Japan1-UCS2 \
	cmaps/japanese/EUC-H cmaps/japanese/EUC-V \
	cmaps/japanese/Ext-RKSJ-H cmaps/japanese/Ext-RKSJ-V \
	cmaps/japanese/H cmaps/japanese/UniJIS-UCS2-H \
	cmaps/japanese/UniJIS-UCS2-HW-H cmaps/japanese/UniJIS-UCS2-HW-V \
	cmaps/japanese/UniJIS-UCS2-V cmaps/japanese/V

$(OUTDIR)/cmap_korean.c: $(CMAPDUMP_APP)
	$(CMAPDUMP_APP) $(OUTDIR)/cmap_korean.c cmaps/korean/Adobe-Korea1-2 \
	cmaps/korean/Adobe-Korea1-UCS2 cmaps/korean/KSC-EUC-H \
	cmaps/korean/KSC-EUC-V cmaps/korean/KSCms-UHC-H \
	cmaps/korean/KSCms-UHC-HW-H cmaps/korean/KSCms-UHC-HW-V \
	cmaps/korean/KSCms-UHC-UCS2 cmaps/korean/KSCms-UHC-V \
	cmaps/korean/KSCpc-EUC-H cmaps/korean/KSCpc-EUC-UCS2 \
	cmaps/korean/KSCpc-EUC-UCS2C cmaps/korean/UniKS-UCS2-H \
	cmaps/korean/UniKS-UCS2-V

fontsources: $(OUTDIR)/font_misc.c $(OUTDIR)/font_mono.c $(OUTDIR)/font_serif.c $(OUTDIR)/font_sans.c $(OUTDIR)/font_cjk.c
	@echo "building font sources"

ifneq ($(ZLIB_DIR),)
-include $(ZLIB_DEP)
endif

ifneq ($(JPEG_DIR),)
-include $(JPEG_DEP)
endif

ifneq ($(JASPER_DIR),)
-include $(JASPER_DEP)
endif

ifneq ($(OPENJPEG_DIR),)
-include $(OPENJPEG_DEP)
endif

ifneq ($(JBIG2_DIR),)
-include $(JBIG2_DEP)
endif

-include $(PDFCLEAN_DEP)
-include $(PDFBENCH_DEP)

inform:
ifneq ($(CFG),rel)
ifneq ($(CFG),dbg)
ifneq ($(CFG),rel64)
ifneq ($(CFG),dbg64)
	@echo "Invalid configuration: '"$(CFG)"'"
	@echo "Valid configurations: rel, dbg (e.g. make CFG=dbg)"
	@exit 1
endif
endif
endif
endif

clean: inform
	rm -rf obj-$(CFG)

cleanall:
	rm -rf obj-*

force:

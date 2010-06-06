# GNU Makefile for MuPDF
#
# make build=release prefix=/usr/local install
#

default: all

build ?= debug
prefix ?= /usr/local

OBJDIR := build/$(build)
GENDIR := build/generated

# If no pregen directory is supplied, then generate (dump) the
# font and cmap .c files as part of the build.
# If it is supplied, then just use the files from that directory.

pregen := $(wildcard pregen)

ifneq "$(pregen)" ""
GENDIR := $(pregen)
endif

# Include makefile for third party libraries.
# They will only be built if the thirdparty directory exists.

-include Makethird

# Compiler flags and configuration options are kept in a separate file.
# This file includes some tests based on variables set by Makethird.

-include Makerules

#
# Build commands
#

GENFILE_CMD = @ echo GENFILE $@ && $(firstword $^) $@ $(wordlist 2, 999, $^)
CC_CMD = @ echo CC $@ && $(CC) -o $@ -c $< $(CFLAGS)
LD_CMD = @ echo LD $@ && $(LD) -o $@ $^ $(LDFLAGS) $(LIBS)
AR_CMD = @ echo AR $@ && $(AR) cru $@ $^

#
# Directories
#

DIRS = $(OBJDIR) $(GENDIR)

$(DIRS):
	mkdir -p $@

#
# Code generation tools
#

FONTDUMP_EXE=$(OBJDIR)/fontdump
$(FONTDUMP_EXE): $(OBJDIR)/fontdump.o
	$(LD_CMD)

CMAPDUMP_EXE=$(OBJDIR)/cmapdump
$(CMAPDUMP_EXE): $(OBJDIR)/cmapdump.o
	$(LD_CMD)

#
# Sources
#

FITZ_HDR=fitz/fitz.h fitz/fitz_base.h fitz/fitz_draw.h fitz/fitz_stream.h
FITZ_SRC=$(addprefix fitz/, \
	base_cpudep.c \
	base_error.c base_memory.c base_string.c base_unicode.c \
	base_hash.c base_matrix.c base_rect.c \
	crypt_aes.c crypt_arc4.c crypt_md5.c \
	filt_aesd.c filt_arc4.c filt_basic.c \
	filt_dctd.c filt_faxd.c filt_faxdtab.c filt_flate.c \
	filt_jbig2d.c filt_jpxd.c filt_lzwd.c filt_pipeline.c filt_predict.c \
	dev_null.c dev_text.c dev_draw.c dev_bbox.c dev_list.c dev_trace.c \
	obj_array.c obj_dict.c obj_print.c obj_simple.c \
	res_colorspace.c res_font.c res_shade.c res_pixmap.c \
	res_path.c res_text.c \
	stm_buffer.c stm_filter.c stm_misc.c stm_open.c stm_read.c \
	util_getopt.c util_gettimeofday.c )
FITZ_OBJ=$(FITZ_SRC:fitz/%.c=$(OBJDIR)/%.o)
$(FITZ_OBJ): $(FITZ_HDR)

DRAW_SRC=$(addprefix draw/, $(ARCH_SRC) \
	archport.c blendmodes.c glyphcache.c \
	imagedraw.c imagescale.c imageunpack.c meshdraw.c \
	pathfill.c pathscan.c pathstroke.c porterduff.c )
DRAW_OBJ=$(DRAW_SRC:draw/%.c=$(OBJDIR)/%.o)
$(DRAW_OBJ): $(FITZ_HDR)

MUPDF_HDR=$(FITZ_HDR) mupdf/mupdf.h
MUPDF_SRC=$(addprefix mupdf/, \
	pdf_annot.c pdf_build.c pdf_cmap.c pdf_cmap_load.c pdf_cmap_parse.c \
	pdf_cmap_table.c pdf_colorspace.c pdf_crypt.c pdf_debug.c \
	pdf_font.c pdf_fontagl.c pdf_fontenc.c pdf_fontfile.c pdf_fontmtx.c \
	pdf_function.c pdf_image.c pdf_interpret.c pdf_lex.c pdf_nametree.c \
	pdf_open.c pdf_outline.c pdf_page.c pdf_pagetree.c pdf_parse.c \
	pdf_pattern.c pdf_repair.c pdf_shade.c pdf_store.c pdf_stream.c \
	pdf_type3.c pdf_unicode.c pdf_xobject.c pdf_xref.c )
MUPDF_OBJ=$(MUPDF_SRC:mupdf/%.c=$(OBJDIR)/%.o)
$(MUPDF_OBJ): $(MUPDF_HDR)

$(OBJDIR)/%.o: fitz/%.c
	$(CC_CMD)
$(OBJDIR)/%.o: draw/%.c
	$(CC_CMD)
$(OBJDIR)/%.o: mupdf/%.c
	$(CC_CMD)
$(OBJDIR)/%.o: $(GENDIR)/%.c
	$(CC_CMD)

#
# Generated font file dumps
#

BASEFONT_FILES=$(addprefix fonts/, \
	Dingbats.cff \
	NimbusMonL-Bold.cff NimbusMonL-BoldObli.cff \
	NimbusMonL-Regu.cff NimbusMonL-ReguObli.cff \
	NimbusRomNo9L-Medi.cff NimbusRomNo9L-MediItal.cff \
	NimbusRomNo9L-Regu.cff NimbusRomNo9L-ReguItal.cff \
	NimbusSanL-Bold.cff NimbusSanL-BoldItal.cff \
	NimbusSanL-Regu.cff NimbusSanL-ReguItal.cff \
	StandardSymL.cff URWChanceryL-MediItal.cff )

CJKFONT_FILES=fonts/droid/DroidSansFallback.ttf

ifeq "$(pregen)" ""

$(GENDIR)/font_base14.c: $(FONTDUMP_EXE) $(BASEFONT_FILES)
	$(GENFILE_CMD)
$(GENDIR)/font_cjk.c: $(FONTDUMP_EXE) $(CJKFONT_FILES)
	$(GENFILE_CMD)

endif

FONT_SRC=\
	$(GENDIR)/font_base14.c \
	$(GENDIR)/font_cjk.c

FONT_OBJ=$(FONT_SRC:$(GENDIR)/%.c=$(OBJDIR)/%.o)

#
# Generated CMap file dumps
#

CMAP_UNICODE_FILES=$(addprefix cmaps/, \
	Adobe-CNS1-UCS2 Adobe-GB1-UCS2 \
	Adobe-Japan1-UCS2 Adobe-Korea1-UCS2 )

CMAP_CNS_FILES=$(addprefix cmaps/, \
	Adobe-CNS1-0 Adobe-CNS1-1 Adobe-CNS1-2 Adobe-CNS1-3 \
	Adobe-CNS1-4 Adobe-CNS1-5 Adobe-CNS1-6 B5-H B5-V B5pc-H B5pc-V \
	CNS-EUC-H CNS-EUC-V CNS1-H CNS1-V CNS2-H CNS2-V ETen-B5-H \
	ETen-B5-V ETenms-B5-H ETenms-B5-V ETHK-B5-H ETHK-B5-V \
	HKdla-B5-H HKdla-B5-V HKdlb-B5-H HKdlb-B5-V HKgccs-B5-H \
	HKgccs-B5-V HKm314-B5-H HKm314-B5-V HKm471-B5-H HKm471-B5-V \
	HKscs-B5-H HKscs-B5-V UniCNS-UCS2-H UniCNS-UCS2-V \
	UniCNS-UTF16-H UniCNS-UTF16-V )

CMAP_GB_FILES=$(addprefix cmaps/, \
	Adobe-GB1-0 Adobe-GB1-1 Adobe-GB1-2 Adobe-GB1-3 Adobe-GB1-4 \
	Adobe-GB1-5 GB-EUC-H GB-EUC-V GB-H GB-V GBK-EUC-H GBK-EUC-V \
	GBK2K-H GBK2K-V GBKp-EUC-H GBKp-EUC-V GBpc-EUC-H GBpc-EUC-V \
	GBT-EUC-H GBT-EUC-V GBT-H GBT-V GBTpc-EUC-H GBTpc-EUC-V \
	UniGB-UCS2-H UniGB-UCS2-V UniGB-UTF16-H UniGB-UTF16-V )

CMAP_JAPAN_FILES=$(addprefix cmaps/, \
	78-EUC-H 78-EUC-V 78-H 78-RKSJ-H 78-RKSJ-V 78-V 78ms-RKSJ-H \
	78ms-RKSJ-V 83pv-RKSJ-H 90ms-RKSJ-H 90ms-RKSJ-V 90msp-RKSJ-H \
	90msp-RKSJ-V 90pv-RKSJ-H 90pv-RKSJ-V Add-H Add-RKSJ-H \
	Add-RKSJ-V Add-V Adobe-Japan1-0 Adobe-Japan1-1 Adobe-Japan1-2 \
	Adobe-Japan1-3 Adobe-Japan1-4 Adobe-Japan1-5 Adobe-Japan1-6 \
	EUC-H EUC-V Ext-H Ext-RKSJ-H Ext-RKSJ-V Ext-V H Hankaku \
	Hiragana Katakana NWP-H NWP-V RKSJ-H RKSJ-V Roman \
	UniJIS-UCS2-H UniJIS-UCS2-HW-H UniJIS-UCS2-HW-V UniJIS-UCS2-V \
	UniJISPro-UCS2-HW-V UniJISPro-UCS2-V V WP-Symbol \
	Adobe-Japan2-0 Hojo-EUC-H Hojo-EUC-V Hojo-H Hojo-V \
	UniHojo-UCS2-H UniHojo-UCS2-V UniHojo-UTF16-H UniHojo-UTF16-V \
	UniJIS-UTF16-H UniJIS-UTF16-V )

CMAP_KOREA_FILES=$(addprefix cmaps/, \
	Adobe-Korea1-0 Adobe-Korea1-1 Adobe-Korea1-2 KSC-EUC-H \
	KSC-EUC-V KSC-H KSC-Johab-H KSC-Johab-V KSC-V KSCms-UHC-H \
	KSCms-UHC-HW-H KSCms-UHC-HW-V KSCms-UHC-V KSCpc-EUC-H \
	KSCpc-EUC-V UniKS-UCS2-H UniKS-UCS2-V UniKS-UTF16-H UniKS-UTF16-V )

ifeq "$(pregen)" ""

$(GENDIR)/cmap_unicode.c: $(CMAPDUMP_EXE) $(CMAP_UNICODE_FILES)
	$(GENFILE_CMD)
$(GENDIR)/cmap_cns.c: $(CMAPDUMP_EXE) $(CMAP_CNS_FILES)
	$(GENFILE_CMD)
$(GENDIR)/cmap_gb.c: $(CMAPDUMP_EXE) $(CMAP_GB_FILES)
	$(GENFILE_CMD)
$(GENDIR)/cmap_japan.c: $(CMAPDUMP_EXE) $(CMAP_JAPAN_FILES)
	$(GENFILE_CMD)
$(GENDIR)/cmap_korea.c: $(CMAPDUMP_EXE) $(CMAP_KOREA_FILES)
	$(GENFILE_CMD)

endif

CMAP_SRC=\
	$(GENDIR)/cmap_unicode.c \
	$(GENDIR)/cmap_cns.c \
	$(GENDIR)/cmap_gb.c \
	$(GENDIR)/cmap_japan.c \
	$(GENDIR)/cmap_korea.c

CMAP_OBJ=$(CMAP_SRC:$(GENDIR)/%.c=$(OBJDIR)/%.o)

#
# Library
#

MUPDF_LIB=$(OBJDIR)/libmupdf.a
$(MUPDF_LIB): $(FITZ_OBJ) $(DRAW_OBJ) $(MUPDF_OBJ) $(CMAP_OBJ) $(FONT_OBJ)
	 $(AR_CMD)

#
# Applications
#

APPS = $(PDFSHOW_EXE) $(PDFCLEAN_EXE) $(PDFDRAW_EXE) $(PDFEXTRACT_EXE) $(PDFINFO_EXE) $(PDFVIEW_EXE)

PDFAPP_HDR = apps/pdfapp.h
PDFTOOL_HDR = apps/pdftool.h

$(OBJDIR)/%.o: apps/%.c
	$(CC_CMD)

PDFSHOW_SRC=apps/pdfshow.c apps/pdftool.c
PDFSHOW_OBJ=$(PDFSHOW_SRC:apps/%.c=$(OBJDIR)/%.o)
PDFSHOW_EXE=$(OBJDIR)/pdfshow

$(PDFSHOW_OBJ): $(MUPDF_HDR) $(PDFTOOL_HDR)
$(PDFSHOW_EXE): $(PDFSHOW_OBJ) $(MUPDF_LIB) $(THIRD_LIBS)
	$(LD_CMD)

PDFCLEAN_SRC=apps/pdfclean.c apps/pdftool.c
PDFCLEAN_OBJ=$(PDFCLEAN_SRC:apps/%.c=$(OBJDIR)/%.o)
PDFCLEAN_EXE=$(OBJDIR)/pdfclean

$(PDFCLEAN_OBJ): $(MUPDF_HDR) $(PDFTOOL_HDR)
$(PDFCLEAN_EXE): $(PDFCLEAN_OBJ) $(MUPDF_LIB) $(THIRD_LIBS)
	$(LD_CMD)

PDFDRAW_SRC=apps/pdfdraw.c apps/pdftool.c
PDFDRAW_OBJ=$(PDFDRAW_SRC:apps/%.c=$(OBJDIR)/%.o)
PDFDRAW_EXE=$(OBJDIR)/pdfdraw

$(PDFDRAW_OBJ): $(MUPDF_HDR) $(PDFTOOL_HDR)
$(PDFDRAW_EXE): $(PDFDRAW_OBJ) $(MUPDF_LIB) $(THIRD_LIBS)
	$(LD_CMD)

PDFEXTRACT_SRC=apps/pdfextract.c apps/pdftool.c
PDFEXTRACT_OBJ=$(PDFEXTRACT_SRC:apps/%.c=$(OBJDIR)/%.o)
PDFEXTRACT_EXE=$(OBJDIR)/pdfextract

$(PDFEXTRACT_OBJ): $(MUPDF_HDR) $(PDFTOOL_HDR)
$(PDFEXTRACT_EXE): $(PDFEXTRACT_OBJ) $(MUPDF_LIB) $(THIRD_LIBS)
	$(LD_CMD)

PDFINFO_SRC=apps/pdfinfo.c apps/pdftool.c
PDFINFO_OBJ=$(PDFINFO_SRC:apps/%.c=$(OBJDIR)/%.o)
PDFINFO_EXE=$(OBJDIR)/pdfinfo

$(PDFINFO_OBJ): $(MUPDF_HDR) $(PDFTOOL_HDR)
$(PDFINFO_EXE): $(PDFINFO_OBJ) $(MUPDF_LIB) $(THIRD_LIBS)
	$(LD_CMD)

X11VIEW_SRC=apps/x11_main.c apps/x11_image.c apps/pdfapp.c
X11VIEW_OBJ=$(X11VIEW_SRC:apps/%.c=$(OBJDIR)/%.o)
X11VIEW_EXE=$(OBJDIR)/mupdf

$(X11VIEW_OBJ): $(MUPDF_HDR) $(PDFAPP_HDR)
$(X11VIEW_EXE): $(X11VIEW_OBJ) $(MUPDF_LIB) $(THIRD_LIBS)
	$(LD_CMD) $(X11LIBS)

WINVIEW_SRC=apps/win_main.c apps/pdfapp.c
WINVIEW_RES=apps/win_res.rc
WINVIEW_OBJ=$(WINVIEW_SRC:apps/%.c=$(OBJDIR)/%.o) $(WINVIEW_RES:apps/%.rc=$(OBJDIR)/%.o)
WINVIEW_EXE=$(OBJDIR)/mupdf.exe

$(OBJDIR)/%.o: apps/%.rc
	windres -i $< -o $@ --include-dir=apps

$(WINVIEW_OBJ): $(MUPDF_HDR) $(PDFAPP_HDR)
$(WINVIEW_EXE): $(WINVIEW_OBJ) $(MUPDF_LIB) $(THIRD_LIBS)
	$(LD_CMD) $(W32LIBS)

#
# Installation and tarball packaging
#

dist: $(DIRS) $(APPS)
	mkdir -p mupdf-dist
	cp README COPYING $(APPS) mupdf-dist
	tar cvf mupdf-dist.tar mupdf-dist
	rm -rf mupdf-dist

#
# Default rules
#

all: $(DIRS) $(THIRD_LIBS) $(MUPDF_LIB) $(APPS)

clean:
	rm -rf $(OBJDIR)/*

nuke:
	rm -rf build

install: $(DIRS) $(APPS) $(MUPDF_LIB)
	install -d $(prefix)/bin $(prefix)/lib $(prefix)/include
	install $(APPS) $(prefix)/bin
	install $(MUPDF_LIB) $(prefix)/lib
	install $(MUPDF_HDR) $(prefix)/include


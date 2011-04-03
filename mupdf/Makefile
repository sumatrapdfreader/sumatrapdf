# GNU Makefile for MuPDF
#
# make build=release prefix=/usr/local verbose=true install
#

default: all

build ?= debug
prefix ?= /usr/local

OBJDIR := build/$(build)
GENDIR := build/generated

$(OBJDIR):
	mkdir -p $@
$(GENDIR):
	mkdir -p $@

# If no pregen directory is supplied, then generate (dump) the
# font and cmap .c files as part of the build.
# If it is supplied, then just use the files from that directory.

pregen := $(wildcard pregen)

ifneq "$(pregen)" ""
GENDIR := $(pregen)
endif

# Compiler flags and configuration options are kept in Makerules.
# Thirdparty libs will be built by Makethird if the thirdparty
# directory exists.

LIBS := -lfreetype -ljbig2dec -lopenjpeg -ljpeg -lz -lm

include Makerules
include Makethird

CFLAGS += $(THIRD_INCS) $(SYS_FREETYPE_INC)

#
# Build commands
#

ifneq "$(verbose)" ""

GENFILE_CMD = $(firstword $^) $@ $(wordlist 2, 999, $^)
CC_CMD = $(CC) -o $@ -c $< $(CFLAGS)
LD_CMD = $(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
AR_CMD = rm -f $@ && $(AR) cru $@ $^

else

GENFILE_CMD = @ echo GENFILE $@ && $(firstword $^) $@ $(wordlist 2, 999, $^)
CC_CMD = @ echo CC $@ && $(CC) -o $@ -c $< $(CFLAGS)
LD_CMD = @ echo LD $@ && $(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
AR_CMD = @ echo AR $@ && rm -f $@ && $(AR) cru $@ $^

endif

#
# Code generation tools
#

FONTDUMP_EXE := $(OBJDIR)/fontdump
$(FONTDUMP_EXE): $(OBJDIR)/fontdump.o
	$(LD_CMD)

CMAPDUMP_EXE := $(OBJDIR)/cmapdump
$(CMAPDUMP_EXE): $(OBJDIR)/cmapdump.o
	$(LD_CMD)

#
# Sources
#

FITZ_HDR := fitz/fitz.h
FITZ_SRC := \
	fitz/base_error.c \
	fitz/base_geometry.c \
	fitz/base_getopt.c \
	fitz/base_hash.c \
	fitz/base_memory.c \
	fitz/base_string.c \
	fitz/base_time.c \
	fitz/crypt_aes.c \
	fitz/crypt_arc4.c \
	fitz/crypt_md5.c \
	fitz/crypt_sha2.c \
	fitz/dev_bbox.c \
	fitz/dev_draw.c \
	fitz/dev_list.c \
	fitz/dev_null.c \
	fitz/dev_text.c \
	fitz/dev_trace.c \
	fitz/filt_basic.c \
	fitz/filt_dctd.c \
	fitz/filt_faxd.c \
	fitz/filt_flate.c \
	fitz/filt_jbig2d.c \
	fitz/filt_jpxd.c \
	fitz/filt_lzwd.c \
	fitz/filt_predict.c \
	fitz/obj_array.c \
	fitz/obj_dict.c \
	fitz/obj_print.c \
	fitz/obj_simple.c \
	fitz/res_colorspace.c \
	fitz/res_font.c \
	fitz/res_path.c \
	fitz/res_pixmap.c \
	fitz/res_shade.c \
	fitz/res_text.c \
	fitz/stm_buffer.c \
	fitz/stm_open.c \
	fitz/stm_read.c
FITZ_OBJ := $(FITZ_SRC:fitz/%.c=$(OBJDIR)/%.o)
$(FITZ_OBJ): $(FITZ_HDR)

DRAW_SRC := $(DRAW_ARCH_SRC) \
	draw/archport.c \
	draw/blendmodes.c \
	draw/glyphcache.c \
	draw/imagedraw.c \
	draw/imagescale.c \
	draw/imagesmooth.c \
	draw/imageunpack.c \
	draw/meshdraw.c \
	draw/pathfill.c \
	draw/pathscan.c \
	draw/pathstroke.c \
	draw/porterduff.c
DRAW_OBJ := $(DRAW_SRC:draw/%.c=$(OBJDIR)/%.o)
DRAW_OBJ := $(DRAW_OBJ:draw/%.s=$(OBJDIR)/%.o)
$(DRAW_OBJ): $(FITZ_HDR)

MUPDF_HDR := $(FITZ_HDR) mupdf/mupdf.h
MUPDF_SRC := \
	mupdf/pdf_annot.c \
	mupdf/pdf_build.c \
	mupdf/pdf_cmap.c \
	mupdf/pdf_cmap_load.c \
	mupdf/pdf_cmap_parse.c \
	mupdf/pdf_cmap_table.c \
	mupdf/pdf_colorspace.c \
	mupdf/pdf_crypt.c \
	mupdf/pdf_debug.c \
	mupdf/pdf_font.c \
	mupdf/pdf_fontagl.c \
	mupdf/pdf_fontenc.c \
	mupdf/pdf_fontfile.c \
	mupdf/pdf_fontmtx.c \
	mupdf/pdf_function.c \
	mupdf/pdf_image.c \
	mupdf/pdf_interpret.c \
	mupdf/pdf_lex.c \
	mupdf/pdf_nametree.c \
	mupdf/pdf_outline.c \
	mupdf/pdf_page.c \
	mupdf/pdf_pagetree.c \
	mupdf/pdf_parse.c \
	mupdf/pdf_pattern.c \
	mupdf/pdf_repair.c \
	mupdf/pdf_shade.c \
	mupdf/pdf_store.c \
	mupdf/pdf_stream.c \
	mupdf/pdf_type3.c \
	mupdf/pdf_unicode.c \
	mupdf/pdf_xobject.c \
	mupdf/pdf_xref.c
MUPDF_OBJ := $(MUPDF_SRC:mupdf/%.c=$(OBJDIR)/%.o)
$(MUPDF_OBJ): $(MUPDF_HDR)

MUXPS_HDR := $(FITZ_HDR) xps/muxps.h
MUXPS_SRC := \
	xps/xps_common.c \
	xps/xps_doc.c \
	xps/xps_glyphs.c \
	xps/xps_gradient.c \
	xps/xps_hash.c \
	xps/xps_image.c \
	xps/xps_jpeg.c \
	xps/xps_path.c \
	xps/xps_png.c \
	xps/xps_resource.c \
	xps/xps_tiff.c \
	xps/xps_tile.c \
	xps/xps_util.c \
	xps/xps_xml.c \
	xps/xps_zip.c
MUXPS_OBJ := $(MUXPS_SRC:xps/%.c=$(OBJDIR)/%.o)
$(MUXPS_OBJ): $(MUXPS_HDR)

$(OBJDIR)/%.o: fitz/%.c
	$(CC_CMD)
$(OBJDIR)/%.o: draw/%.c
	$(CC_CMD)
$(OBJDIR)/%.o: draw/%.s
	$(CC_CMD)
$(OBJDIR)/%.o: mupdf/%.c
	$(CC_CMD)
$(OBJDIR)/%.o: xps/%.c
	$(CC_CMD)
$(OBJDIR)/%.o: $(GENDIR)/%.c
	$(CC_CMD)

#
# Generated font file dumps
#

BASEFONT_FILES := \
	fonts/Dingbats.cff \
	fonts/NimbusMonL-Bold.cff \
	fonts/NimbusMonL-BoldObli.cff \
	fonts/NimbusMonL-Regu.cff \
	fonts/NimbusMonL-ReguObli.cff \
	fonts/NimbusRomNo9L-Medi.cff \
	fonts/NimbusRomNo9L-MediItal.cff \
	fonts/NimbusRomNo9L-Regu.cff \
	fonts/NimbusRomNo9L-ReguItal.cff \
	fonts/NimbusSanL-Bold.cff \
	fonts/NimbusSanL-BoldItal.cff \
	fonts/NimbusSanL-Regu.cff \
	fonts/NimbusSanL-ReguItal.cff \
	fonts/StandardSymL.cff \
	fonts/URWChanceryL-MediItal.cff

CJKFONT_FILES := \
	fonts/droid/DroidSansFallback.ttf

ifeq "$(pregen)" ""

$(GENDIR)/font_base14.c: $(FONTDUMP_EXE) $(BASEFONT_FILES)
	$(GENFILE_CMD)
$(GENDIR)/font_cjk.c: $(FONTDUMP_EXE) $(CJKFONT_FILES)
	$(GENFILE_CMD)

endif

FONT_SRC := \
	$(GENDIR)/font_base14.c \
	$(GENDIR)/font_cjk.c

FONT_OBJ := $(FONT_SRC:$(GENDIR)/%.c=$(OBJDIR)/%.o)


#
# Generated CMap file dumps
#

CMAP_UNICODE_FILES := $(addprefix cmaps/, \
	Adobe-CNS1-UCS2 Adobe-GB1-UCS2 \
	Adobe-Japan1-UCS2 Adobe-Korea1-UCS2 )

CMAP_CNS_FILES := $(addprefix cmaps/, \
	Adobe-CNS1-0 Adobe-CNS1-1 Adobe-CNS1-2 Adobe-CNS1-3 \
	Adobe-CNS1-4 Adobe-CNS1-5 Adobe-CNS1-6 B5-H B5-V B5pc-H B5pc-V \
	CNS-EUC-H CNS-EUC-V CNS1-H CNS1-V CNS2-H CNS2-V ETen-B5-H \
	ETen-B5-V ETenms-B5-H ETenms-B5-V ETHK-B5-H ETHK-B5-V \
	HKdla-B5-H HKdla-B5-V HKdlb-B5-H HKdlb-B5-V HKgccs-B5-H \
	HKgccs-B5-V HKm314-B5-H HKm314-B5-V HKm471-B5-H HKm471-B5-V \
	HKscs-B5-H HKscs-B5-V UniCNS-UCS2-H UniCNS-UCS2-V \
	UniCNS-UTF16-H UniCNS-UTF16-V )

CMAP_GB_FILES := $(addprefix cmaps/, \
	Adobe-GB1-0 Adobe-GB1-1 Adobe-GB1-2 Adobe-GB1-3 Adobe-GB1-4 \
	Adobe-GB1-5 GB-EUC-H GB-EUC-V GB-H GB-V GBK-EUC-H GBK-EUC-V \
	GBK2K-H GBK2K-V GBKp-EUC-H GBKp-EUC-V GBpc-EUC-H GBpc-EUC-V \
	GBT-EUC-H GBT-EUC-V GBT-H GBT-V GBTpc-EUC-H GBTpc-EUC-V \
	UniGB-UCS2-H UniGB-UCS2-V UniGB-UTF16-H UniGB-UTF16-V )

CMAP_JAPAN_FILES := $(addprefix cmaps/, \
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

CMAP_KOREA_FILES := $(addprefix cmaps/, \
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

CMAP_SRC := \
	$(GENDIR)/cmap_unicode.c \
	$(GENDIR)/cmap_cns.c \
	$(GENDIR)/cmap_gb.c \
	$(GENDIR)/cmap_japan.c \
	$(GENDIR)/cmap_korea.c

CMAP_OBJ := $(CMAP_SRC:$(GENDIR)/%.c=$(OBJDIR)/%.o)

#
# Library
#

MUPDF_LIB = $(OBJDIR)/libmupdf.a
$(MUPDF_LIB): $(FITZ_OBJ) $(DRAW_OBJ) $(MUPDF_OBJ) $(CMAP_OBJ) $(FONT_OBJ)
	 $(AR_CMD)

MUXPS_LIB = $(OBJDIR)/libmuxps.a
$(MUXPS_LIB): $(FITZ_OBJ) $(DRAW_OBJ) $(MUXPS_OBJ)
	 $(AR_CMD)

#
# Applications
#

APPS = $(PDFSHOW_EXE) $(PDFCLEAN_EXE) $(PDFDRAW_EXE) $(PDFEXTRACT_EXE) $(PDFINFO_EXE) $(PDFVIEW_EXE) $(XPSDRAW_EXE)

APPS_MAN = \
	apps/man/mupdf.1 \
	apps/man/pdfclean.1 \
	apps/man/pdfdraw.1 \
	apps/man/pdfshow.1

$(OBJDIR)/%.o: apps/%.c
	$(CC_CMD)

PDFSHOW_SRC=apps/pdfshow.c
PDFSHOW_OBJ=$(PDFSHOW_SRC:apps/%.c=$(OBJDIR)/%.o)
PDFSHOW_EXE=$(OBJDIR)/pdfshow
$(PDFSHOW_OBJ): $(MUPDF_HDR)
$(PDFSHOW_EXE): $(PDFSHOW_OBJ) $(MUPDF_LIB) $(THIRD_LIBS)
	$(LD_CMD)

PDFCLEAN_SRC=apps/pdfclean.c
PDFCLEAN_OBJ=$(PDFCLEAN_SRC:apps/%.c=$(OBJDIR)/%.o)
PDFCLEAN_EXE=$(OBJDIR)/pdfclean
$(PDFCLEAN_OBJ): $(MUPDF_HDR)
$(PDFCLEAN_EXE): $(PDFCLEAN_OBJ) $(MUPDF_LIB) $(THIRD_LIBS)
	$(LD_CMD)

PDFDRAW_SRC=apps/pdfdraw.c
PDFDRAW_OBJ=$(PDFDRAW_SRC:apps/%.c=$(OBJDIR)/%.o)
PDFDRAW_EXE=$(OBJDIR)/pdfdraw
$(PDFDRAW_OBJ): $(MUPDF_HDR)
$(PDFDRAW_EXE): $(PDFDRAW_OBJ) $(MUPDF_LIB) $(THIRD_LIBS)
	$(LD_CMD)

PDFEXTRACT_SRC=apps/pdfextract.c
PDFEXTRACT_OBJ=$(PDFEXTRACT_SRC:apps/%.c=$(OBJDIR)/%.o)
PDFEXTRACT_EXE=$(OBJDIR)/pdfextract
$(PDFEXTRACT_OBJ): $(MUPDF_HDR)
$(PDFEXTRACT_EXE): $(PDFEXTRACT_OBJ) $(MUPDF_LIB) $(THIRD_LIBS)
	$(LD_CMD)

PDFINFO_SRC=apps/pdfinfo.c
PDFINFO_OBJ=$(PDFINFO_SRC:apps/%.c=$(OBJDIR)/%.o)
PDFINFO_EXE=$(OBJDIR)/pdfinfo
$(PDFINFO_OBJ): $(MUPDF_HDR)
$(PDFINFO_EXE): $(PDFINFO_OBJ) $(MUPDF_LIB) $(THIRD_LIBS)
	$(LD_CMD)

XPSDRAW_SRC=apps/xpsdraw.c
XPSDRAW_OBJ=$(XPSDRAW_SRC:apps/%.c=$(OBJDIR)/%.o)
XPSDRAW_EXE=$(OBJDIR)/xpsdraw
$(XPSDRAW_OBJ): $(MUXPS_HDR)
$(XPSDRAW_EXE): $(XPSDRAW_OBJ) $(MUXPS_LIB) $(THIRD_LIBS)
	$(LD_CMD)

PDFAPP_HDR = apps/pdfapp.h

X11VIEW_SRC=apps/x11_main.c apps/x11_image.c apps/pdfapp.c
X11VIEW_OBJ=$(X11VIEW_SRC:apps/%.c=$(OBJDIR)/%.o)
X11VIEW_EXE=$(OBJDIR)/mupdf

$(X11VIEW_OBJ): $(MUPDF_HDR) $(PDFAPP_HDR)
$(X11VIEW_EXE): $(X11VIEW_OBJ) $(MUPDF_LIB) $(MUXPS_LIB) $(THIRD_LIBS)
	$(LD_CMD) $(X11LIBS)

WINVIEW_SRC=apps/win_main.c apps/pdfapp.c
WINVIEW_RES=apps/win_res.rc
WINVIEW_OBJ=$(WINVIEW_SRC:apps/%.c=$(OBJDIR)/%.o) $(WINVIEW_RES:apps/%.rc=$(OBJDIR)/%.o)
WINVIEW_EXE=$(OBJDIR)/mupdf.exe

$(OBJDIR)/%.o: apps/%.rc
	$(WINDRES) -i $< -o $@ --include-dir=apps

$(WINVIEW_OBJ): $(MUPDF_HDR) $(PDFAPP_HDR)
$(WINVIEW_EXE): $(WINVIEW_OBJ) $(MUPDF_LIB) $(MUXPS_LIB) $(THIRD_LIBS)
	$(LD_CMD) $(W32LIBS)

#
# Default rules
#

.PHONY: default all pregen clean nuke install

all: $(OBJDIR) $(GENDIR) $(THIRD_LIBS) $(MUPDF_LIB) $(APPS)

clean:
	rm -rf $(OBJDIR)/*

nuke:
	rm -rf build

BINDIR ?= $(prefix)/bin
LIBDIR ?= $(prefix)/lib
INCDIR ?= $(prefix)/include
MANDIR ?= $(prefix)/share/man/man1

install: $(OBJDIR) $(GENDIR) $(MUPDF_LIB) $(APPS)
	install -d $(BINDIR) $(LIBDIR) $(INCDIR) $(MANDIR)
	install $(APPS) $(BINDIR)
	install $(APPS_MAN) $(MANDIR)
	install $(MUPDF_LIB) $(LIBDIR)
	install $(MUPDF_HDR) $(INCDIR)

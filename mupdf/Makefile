# GNU Makefile

build ?= debug

OUT := build/$(build)
GEN := generated

default: all

# --- Configuration ---

# Do not specify CFLAGS or LIBS on the make invocation line - specify
# XCFLAGS or XLIBS instead. Make ignores any lines in the makefile that
# set a variable that was set on the command line.
CFLAGS += $(XCFLAGS) -Iinclude -I$(GEN)
LIBS += $(XLIBS) -lm

include Makerules
include Makethird

THIRD_LIBS += $(FREETYPE_LIB)
THIRD_LIBS += $(JBIG2DEC_LIB)
THIRD_LIBS += $(JPEG_LIB)
THIRD_LIBS += $(OPENJPEG_LIB)
THIRD_LIBS += $(OPENSSL_LIB)
THIRD_LIBS += $(ZLIB_LIB)

LIBS += $(FREETYPE_LIBS)
LIBS += $(JBIG2DEC_LIBS)
LIBS += $(JPEG_LIBS)
LIBS += $(OPENJPEG_LIBS)
LIBS += $(OPENSSL_LIBS)
LIBS += $(ZLIB_LIBS)

CFLAGS += $(FREETYPE_CFLAGS)
CFLAGS += $(JBIG2DEC_CFLAGS)
CFLAGS += $(JPEG_CFLAGS)
CFLAGS += $(OPENJPEG_CFLAGS)
CFLAGS += $(OPENSSL_CFLAGS)
CFLAGS += $(ZLIB_CFLAGS)

# --- Commands ---

ifneq "$(verbose)" "yes"
QUIET_AR = @ echo ' ' ' ' AR $@ ;
QUIET_CC = @ echo ' ' ' ' CC $@ ;
QUIET_CXX = @ echo ' ' ' ' CXX $@ ;
QUIET_GEN = @ echo ' ' ' ' GEN $@ ;
QUIET_LINK = @ echo ' ' ' ' LINK $@ ;
QUIET_MKDIR = @ echo ' ' ' ' MKDIR $@ ;
QUIET_RM = @ echo ' ' ' ' RM $@ ;
endif

CC_CMD = $(QUIET_CC) $(CC) $(CFLAGS) -o $@ -c $<
CXX_CMD = $(QUIET_CXX) $(CXX) $(CFLAGS) -o $@ -c $<
AR_CMD = $(QUIET_AR) $(AR) cr $@ $^
LINK_CMD = $(QUIET_LINK) $(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
MKDIR_CMD = $(QUIET_MKDIR) mkdir -p $@
RM_CMD = $(QUIET_RM) rm -f $@

# --- File lists ---

ALL_DIR := $(OUT)/fitz
ALL_DIR += $(OUT)/pdf $(OUT)/pdf/js
ALL_DIR += $(OUT)/xps
ALL_DIR += $(OUT)/cbz
ALL_DIR += $(OUT)/img
ALL_DIR += $(OUT)/tiff
ALL_DIR += $(OUT)/tools
ALL_DIR += $(OUT)/platform/x11
ALL_DIR += $(OUT)/platform/x11/curl

FITZ_HDR := include/mupdf/fitz.h $(wildcard include/mupdf/fitz/*.h)
PDF_HDR := include/mupdf/pdf.h $(wildcard include/mupdf/pdf/*.h)
XPS_HDR := include/mupdf/xps.h
CBZ_HDR := include/mupdf/cbz.h
IMG_HDR := include/mupdf/img.h

FITZ_SRC := $(wildcard source/fitz/*.c)
PDF_SRC := $(wildcard source/pdf/*.c)
XPS_SRC := $(wildcard source/xps/*.c)
CBZ_SRC := $(wildcard source/cbz/*.c)
IMG_SRC := $(wildcard source/img/*.c)
TIFF_SRC := $(wildcard source/tiff/*.c)

FITZ_SRC_HDR := $(wildcard source/fitz/*.h)
PDF_SRC_HDR := $(wildcard source/pdf/*.h)
XPS_SRC_HDR := $(wildcard source/xps/*.h)
CBZ_SRC_HDR := $(wildcard source/cbz/*.h)
IMG_SRC_HDR := $(wildcard source/img/*.h)
TIFF_SRC_HDR := $(wildcard source/tiff/*.h)

FITZ_OBJ := $(subst source/, $(OUT)/, $(addsuffix .o, $(basename $(FITZ_SRC))))
PDF_OBJ := $(subst source/, $(OUT)/, $(addsuffix .o, $(basename $(PDF_SRC))))
XPS_OBJ := $(subst source/, $(OUT)/, $(addsuffix .o, $(basename $(XPS_SRC))))
CBZ_OBJ := $(subst source/, $(OUT)/, $(addsuffix .o, $(basename $(CBZ_SRC))))
IMG_OBJ := $(subst source/, $(OUT)/, $(addsuffix .o, $(basename $(IMG_SRC))))
TIFF_OBJ := $(subst source/, $(OUT)/, $(addsuffix .o, $(basename $(TIFF_SRC))))

# --- Choice of Javascript library ---

ifeq "$(HAVE_MUJS)" "yes"
PDF_OBJ += $(OUT)/pdf/js/pdf-js.o
PDF_OBJ += $(OUT)/pdf/js/pdf-jsimp-mu.o
THIRD_LIBS += $(MUJS_LIB)
LIBS += $(MUJS_LIBS)
CFLAGS += $(MUJS_CFLAGS)
else ifeq "$(HAVE_JSCORE)" "yes"
PDF_OBJ += $(OUT)/pdf/js/pdf-js.o
PDF_OBJ += $(OUT)/pdf/js/pdf-jsimp-jscore.o
LIBS += $(JSCORE_LIBS)
CFLAGS += $(JSCORE_CFLAGS)
else ifeq "$(HAVE_V8)" "yes"
PDF_OBJ += $(OUT)/pdf/js/pdf-js.o
PDF_OBJ += $(OUT)/pdf/js/pdf-jsimp-cpp.o $(OUT)/pdf/js/pdf-jsimp-v8.o
LIBS += $(V8_LIBS)
CFLAGS += $(V8_CFLAGS)
else
PDF_OBJ += $(OUT)/pdf/js/pdf-js-none.o
endif

$(FITZ_OBJ) : $(FITZ_HDR) $(FITZ_SRC_HDR)
$(PDF_OBJ) : $(FITZ_HDR) $(PDF_HDR) $(PDF_SRC_HDR)
$(XPS_OBJ) : $(FITZ_HDR) $(XPS_HDR) $(XPS_SRC_HDR)
$(CBZ_OBJ) : $(FITZ_HDR) $(CBZ_HDR) $(CBZ_SRC_HDR)
$(IMG_OBJ) : $(FITZ_HDR) $(IMG_HDR) $(IMG_SRC_HDR)
$(TIFF_OBJ) : $(FITZ_HDR) $(IMG_HDR) $(TIFF_SRC_HDR)

# --- Library ---

MUPDF_LIB := $(OUT)/libmupdf.a

$(MUPDF_LIB) : $(FITZ_OBJ) $(PDF_OBJ) $(XPS_OBJ) $(CBZ_OBJ) $(IMG_OBJ) $(TIFF_OBJ)

INSTALL_LIBS := $(MUPDF_LIB)

# --- Rules ---

$(ALL_DIR) $(OUT) $(GEN) :
	$(MKDIR_CMD)

$(OUT)/%.a :
	$(RM_CMD)
	$(AR_CMD)
	$(RANLIB_CMD)

$(OUT)/%: $(OUT)/%.o
	$(LINK_CMD)

$(OUT)/%.o : source/%.c | $(ALL_DIR)
	$(CC_CMD)

$(OUT)/%.o : source/%.cpp | $(ALL_DIR)
	$(CXX_CMD)

$(OUT)/%.o : scripts/%.c | $(OUT)
	$(CC_CMD)

$(OUT)/platform/x11/%.o : platform/x11/%.c | $(ALL_DIR)
	$(CC_CMD) $(X11_CFLAGS)

$(OUT)/platform/x11/curl/%.o : platform/x11/%.c | $(ALL_DIR)
	$(CC_CMD) $(X11_CFLAGS) $(CURL_CFLAGS) -DHAVE_CURL

.PRECIOUS : $(OUT)/%.o # Keep intermediates from chained rules

# --- Generated CMAP, FONT and JAVASCRIPT files ---

CMAPDUMP := $(OUT)/cmapdump
FONTDUMP := $(OUT)/fontdump
CQUOTE := $(OUT)/cquote
BIN2HEX := $(OUT)/bin2hex

CMAP_CNS_SRC := $(wildcard resources/cmaps/cns/*)
CMAP_GB_SRC := $(wildcard resources/cmaps/gb/*)
CMAP_JAPAN_SRC := $(wildcard resources/cmaps/japan/*)
CMAP_KOREA_SRC := $(wildcard resources/cmaps/korea/*)

FONT_BASE14_SRC := $(wildcard resources/fonts/urw/*.cff)
FONT_DROID_SRC := resources/fonts/droid/DroidSans.ttf resources/fonts/droid/DroidSansMono.ttf
FONT_CJK_SRC := resources/fonts/droid/DroidSansFallback.ttc
FONT_CJK_FULL_SRC := resources/fonts/droid/DroidSansFallbackFull.ttc

$(GEN)/gen_cmap_cns.h : $(CMAP_CNS_SRC)
	$(QUIET_GEN) $(CMAPDUMP) $@ $(CMAP_CNS_SRC)
$(GEN)/gen_cmap_gb.h : $(CMAP_GB_SRC)
	$(QUIET_GEN) $(CMAPDUMP) $@ $(CMAP_GB_SRC)
$(GEN)/gen_cmap_japan.h : $(CMAP_JAPAN_SRC)
	$(QUIET_GEN) $(CMAPDUMP) $@ $(CMAP_JAPAN_SRC)
$(GEN)/gen_cmap_korea.h : $(CMAP_KOREA_SRC)
	$(QUIET_GEN) $(CMAPDUMP) $@ $(CMAP_KOREA_SRC)

CMAP_GEN := $(addprefix $(GEN)/, gen_cmap_cns.h gen_cmap_gb.h gen_cmap_japan.h gen_cmap_korea.h)

$(GEN)/gen_font_base14.h : $(FONT_BASE14_SRC)
	$(QUIET_GEN) $(FONTDUMP) $@ $(FONT_BASE14_SRC)
$(GEN)/gen_font_droid.h : $(FONT_DROID_SRC)
	$(QUIET_GEN) $(FONTDUMP) $@ $(FONT_DROID_SRC)
$(GEN)/gen_font_cjk.h : $(FONT_CJK_SRC)
	$(QUIET_GEN) $(FONTDUMP) $@ $(FONT_CJK_SRC)
$(GEN)/gen_font_cjk_full.h : $(FONT_CJK_FULL_SRC)
	$(QUIET_GEN) $(FONTDUMP) $@ $(FONT_CJK_FULL_SRC)

FONT_GEN := $(GEN)/gen_font_base14.h $(GEN)/gen_font_droid.h $(GEN)/gen_font_cjk.h $(GEN)/gen_font_cjk_full.h

JAVASCRIPT_SRC := source/pdf/js/pdf-util.js
JAVASCRIPT_GEN := $(GEN)/gen_js_util.h
$(JAVASCRIPT_GEN) : $(JAVASCRIPT_SRC)
	$(QUIET_GEN) $(CQUOTE) $@ $(JAVASCRIPT_SRC)

ADOBECA_SRC := resources/certs/AdobeCA.p7c
ADOBECA_GEN := $(GEN)/gen_adobe_ca.h
$(ADOBECA_GEN) : $(ADOBECA_SRC)
	$(QUIET_GEN) $(BIN2HEX) $@ $(ADOBECA_SRC)

ifneq "$(CROSSCOMPILE)" "yes"
$(CMAP_GEN) : $(CMAPDUMP) | $(GEN)
$(FONT_GEN) : $(FONTDUMP) | $(GEN)
$(JAVASCRIPT_GEN) : $(CQUOTE) | $(GEN)
$(ADOBECA_GEN) : $(BIN2HEX) | $(GEN)
endif

generate: $(CMAP_GEN) $(FONT_GEN) $(JAVASCRIPT_GEN) $(ADOBECA_GEN)

$(OUT)/pdf/pdf-cmap-table.o : $(CMAP_GEN)
$(OUT)/pdf/pdf-fontfile.o : $(FONT_GEN)
$(OUT)/pdf/pdf-pkcs7.o : $(ADOBECA_GEN)
$(OUT)/pdf/js/pdf-js.o : $(JAVASCRIPT_GEN)
$(OUT)/cmapdump.o : include/mupdf/pdf/cmap.h source/pdf/pdf-cmap.c source/pdf/pdf-cmap-parse.c

# --- Tools and Apps ---

MUDRAW := $(addprefix $(OUT)/, mudraw)
MUDRAW_OBJ := $(addprefix $(OUT)/tools/, mudraw.o)
$(MUDRAW_OBJ) : $(FITZ_HDR)
$(MUDRAW) : $(MUPDF_LIB) $(THIRD_LIBS)
$(MUDRAW) : $(MUDRAW_OBJ)
	$(LINK_CMD)

MUTOOL := $(addprefix $(OUT)/, mutool)
MUTOOL_OBJ := $(addprefix $(OUT)/tools/, mutool.o pdfclean.o pdfextract.o pdfinfo.o pdfposter.o pdfshow.o)
$(MUTOOL_OBJ): $(FITZ_HDR) $(PDF_HDR)
$(MUTOOL) : $(MUPDF_LIB) $(THIRD_LIBS)
$(MUTOOL) : $(MUTOOL_OBJ)
	$(LINK_CMD)

MJSGEN := $(OUT)/mjsgen
$(MJSGEN) : $(MUPDF_LIB) $(THIRD_LIBS)
$(MJSGEN) : $(addprefix $(OUT)/tools/, mjsgen.o)
	$(LINK_CMD)

MUJSTEST := $(OUT)/mujstest
$(MUJSTEST) : $(MUPDF_LIB) $(THIRD_LIBS)
$(MUJSTEST) : $(addprefix $(OUT)/platform/x11/, jstest_main.o pdfapp.o)
	$(LINK_CMD)

ifeq "$(HAVE_X11)" "yes"
MUVIEW_X11 := $(OUT)/mupdf-x11
MUVIEW_X11_OBJ := $(addprefix $(OUT)/platform/x11/, x11_main.o x11_image.o pdfapp.o)
$(MUVIEW_X11_OBJ) : $(FITZ_HDR) $(PDF_HDR)
$(MUVIEW_X11) : $(MUPDF_LIB) $(THIRD_LIBS)
$(MUVIEW_X11) : $(MUVIEW_X11_OBJ)
	$(LINK_CMD) $(X11_LIBS)

ifeq "$(HAVE_CURL)" "yes"
MUVIEW_X11_CURL := $(OUT)/mupdf-x11-curl
MUVIEW_X11_CURL_OBJ := $(addprefix $(OUT)/platform/x11/curl/, x11_main.o x11_image.o pdfapp.o curl_stream.o)
$(MUVIEW_X11_CURL_OBJ) : $(FITZ_HDR) $(PDF_HDR)
$(MUVIEW_X11_CURL) : $(MUPDF_LIB) $(THIRD_LIBS) $(CURL_LIB)
$(MUVIEW_X11_CURL) : $(MUVIEW_X11_CURL_OBJ)
	$(LINK_CMD) $(X11_LIBS) $(CURL_LIBS) $(SYS_CURL_DEPS)
endif
endif

MUVIEW := $(MUVIEW_X11)
MUVIEW_CURL := $(MUVIEW_X11_CURL)

INSTALL_APPS := $(MUDRAW) $(MUTOOL) $(MUVIEW) $(MUJSTEST) $(MUVIEW_CURL)

# --- Update version string header ---

VERSION = $(shell git describe --tags)

version:
	sed -i~ -e '/FZ_VERSION /s/".*"/"'$(VERSION)'"/' include/mupdf/fitz/version.h

# --- Format man pages ---

%.txt: %.1
	nroff -man $< | col -b | expand > $@

MAN_FILES := $(wildcard docs/man/*.1)
TXT_FILES := $(MAN_FILES:%.1=%.txt)

catman: $(TXT_FILES)

# --- Install ---

prefix ?= /usr/local
bindir ?= $(prefix)/bin
libdir ?= $(prefix)/lib
incdir ?= $(prefix)/include
mandir ?= $(prefix)/share/man
docdir ?= $(prefix)/share/doc/mupdf

third: $(THIRD_LIBS) $(CURL_LIB)
libs: $(INSTALL_LIBS)
apps: $(INSTALL_APPS)

install: libs apps
	install -d $(DESTDIR)$(incdir)/mupdf
	install -d $(DESTDIR)$(incdir)/mupdf/fitz
	install -d $(DESTDIR)$(incdir)/mupdf/pdf
	install include/mupdf/*.h $(DESTDIR)$(incdir)/mupdf
	install include/mupdf/fitz/*.h $(DESTDIR)$(incdir)/mupdf/fitz
	install include/mupdf/pdf/*.h $(DESTDIR)$(incdir)/mupdf/pdf

	install -d $(DESTDIR)$(libdir)
	install $(INSTALL_LIBS) $(DESTDIR)$(libdir)

	install -d $(DESTDIR)$(bindir)
	install $(INSTALL_APPS) $(DESTDIR)$(bindir)

	install -d $(DESTDIR)$(mandir)/man1
	install docs/man/*.1 $(DESTDIR)$(mandir)/man1

	install -d $(DESTDIR)$(docdir)
	install README COPYING CHANGES docs/*.txt $(DESTDIR)$(docdir)

tarball:
	bash scripts/archive.sh

# --- Clean and Default ---

tags: $(shell find include source platform -name '*.[ch]')
	ctags $^

cscope.files: $(shell find include source platform -name '*.[ch]')
	@ echo $^ | tr ' ' '\n' > $@

cscope.out: cscope.files
	cscope -b

all: libs apps

clean:
	rm -rf $(OUT)
nuke:
	rm -rf build/* $(GEN)

.PHONY: all clean nuke install third libs apps generate

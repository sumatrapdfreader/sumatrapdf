# GNU Makefile

build ?= debug

OUT := build/$(build)
GEN := generated

default: all

# --- Configuration ---

# Do not specify CFLAGS or LIBS on the make invocation line - specify
# XCFLAGS or XLIBS instead. Make ignores any lines in the makefile that
# set a variable that was set on the command line.
CFLAGS += $(XCFLAGS) -Iinclude -Iscripts -I$(GEN)
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

ifeq "$(verbose)" ""
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
ALL_DIR += $(OUT)/tools
ALL_DIR += $(OUT)/platform/x11

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

FITZ_SRC_HDR := $(wildcard source/fitz/*.h)
PDF_SRC_HDR := $(wildcard source/pdf/*.h)
XPS_SRC_HDR := $(wildcard source/xps/*.h)
CBZ_SRC_HDR := $(wildcard source/cbz/*.h)
IMG_SRC_HDR := $(wildcard source/img/*.h)

FITZ_OBJ := $(subst source/, $(OUT)/, $(addsuffix .o, $(basename $(FITZ_SRC))))
PDF_OBJ := $(subst source/, $(OUT)/, $(addsuffix .o, $(basename $(PDF_SRC))))
XPS_OBJ := $(subst source/, $(OUT)/, $(addsuffix .o, $(basename $(XPS_SRC))))
CBZ_OBJ := $(subst source/, $(OUT)/, $(addsuffix .o, $(basename $(CBZ_SRC))))
IMG_OBJ := $(subst source/, $(OUT)/, $(addsuffix .o, $(basename $(IMG_SRC))))

PDF_JS_V8_OBJ := $(OUT)/pdf/js/pdf-js.o $(OUT)/pdf/js/pdf-jsimp-cpp.o $(OUT)/pdf/js/pdf-jsimp-v8.o
PDF_JS_NONE_OBJ := $(OUT)/pdf/js/pdf-js-none.o

$(FITZ_OBJ) : $(FITZ_HDR) $(FITZ_SRC_HDR)
$(PDF_OBJ) : $(FITZ_HDR) $(PDF_HDR) $(PDF_SRC_HDR)
$(XPS_OBJ) : $(FITZ_HDR) $(XPS_HDR) $(XPS_SRC_HDR)
$(CBZ_OBJ) : $(FITZ_HDR) $(CBZ_HDR) $(CBZ_SRC_HDR)
$(IMG_OBJ) : $(FITZ_HDR) $(IMG_HDR) $(IMG_SRC_HDR)

$(PDF_JS_V8_OBJ) : $(FITZ_HDR) $(PDF_HDR) $(PDF_SRC_HDR)
$(PDF_JS_NONE_OBJ) :=  $(FITZ_HDR) $(PDF_HDR) $(PDF_SRC_HDR)

# --- Library ---

MUPDF_LIB := $(OUT)/libmupdf.a
MUPDF_JS_NONE_LIB := $(OUT)/libmupdf-js-none.a

$(MUPDF_LIB) : $(FITZ_OBJ) $(PDF_OBJ) $(XPS_OBJ) $(CBZ_OBJ) $(IMG_OBJ)
$(MUPDF_JS_NONE_LIB) : $(PDF_JS_NONE_OBJ)

ifeq "$(V8_PRESENT)" "yes"
MUPDF_JS_V8_LIB := $(OUT)/libmupdf-js-v8.a
$(MUPDF_JS_V8_LIB) : $(PDF_JS_V8_OBJ)
endif

INSTALL_LIBS := $(MUPDF_LIB) $(MUPDF_JS_NONE_LIB) $(MUPDF_JS_V8_LIB)

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
	$(CC_CMD) $(X11_CFLAGS) $(CURL_CFLAGS)

$(OUT)/platform/x11/curl/%.o : platform/x11/%.c | $(ALL_DIR)
	mkdir -p $(OUT)/platform/x11/curl
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

FONT_BASE14_SRC := $(wildcard resources/fonts/*.cff)
FONT_DROID_SRC := resources/fonts/droid/DroidSans.ttf resources/fonts/droid/DroidSansMono.ttf
FONT_CJK_SRC := resources/fonts/droid/DroidSansFallback.ttf
FONT_CJK_FULL_SRC := resources/fonts/droid/DroidSansFallbackFull.ttf

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

ifeq "$(CROSSCOMPILE)" ""
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
$(OUT)/cmapdump.o : source/pdf/pdf-cmap.c source/pdf/pdf-cmap-parse.c

# --- Tools and Apps ---

MUDRAW := $(addprefix $(OUT)/, mudraw)
$(MUDRAW) : $(MUPDF_LIB) $(MUPDF_JS_NONE_LIB) $(THIRD_LIBS)
$(MUDRAW) : $(addprefix $(OUT)/tools/, mudraw.o)
	$(LINK_CMD)

MUTOOL := $(addprefix $(OUT)/, mutool)
$(MUTOOL) : $(MUPDF_LIB) $(MUPDF_JS_NONE_LIB) $(THIRD_LIBS)
$(MUTOOL) : $(addprefix $(OUT)/tools/, mutool.o pdfclean.o pdfextract.o pdfinfo.o pdfposter.o pdfshow.o)
	$(LINK_CMD)

ifeq "$(V8_PRESENT)" "yes"
MUJSTEST_V8 := $(OUT)/mujstest-v8
$(MUJSTEST_V8) : $(MUPDF_LIB) $(MUPDF_JS_V8_LIB) $(THIRD_LIBS)
$(MUJSTEST_V8) : $(addprefix $(OUT)/platform/x11/, jstest_main.o pdfapp.o)
	$(LINK_CMD) $(V8_LIBS)
endif

ifeq "$(NOX11)" ""
MUVIEW_X11 := $(OUT)/mupdf-x11
$(MUVIEW_X11) : $(MUPDF_LIB) $(MUPDF_JS_NONE_LIB) $(THIRD_LIBS)
$(MUVIEW_X11) : $(addprefix $(OUT)/platform/x11/, x11_main.o x11_image.o pdfapp.o)
	$(LINK_CMD) $(X11_LIBS)

ifeq "$(NOCURL)" ""
MUVIEW_X11_CURL := $(OUT)/mupdf-x11-curl
$(MUVIEW_X11_CURL) : $(MUPDF_LIB) $(MUPDF_JS_NONE_LIB) $(THIRD_LIBS) $(CURL_LIB)
$(MUVIEW_X11_CURL) : $(addprefix $(OUT)/platform/x11/curl/, x11_main.o x11_image.o pdfapp.o curl_stream.o)
	$(LINK_CMD) $(X11_LIBS) $(CURL_LIBS)
endif
endif

ifeq "$(V8_PRESENT)" "yes"
ifeq "$(NOX11)" ""
MUVIEW_X11_V8 := $(OUT)/mupdf-x11-v8
$(MUVIEW_X11_V8) : $(MUPDF_LIB) $(MUPDF_JS_V8_LIB) $(THIRD_LIBS)
$(MUVIEW_X11_V8) : $(addprefix $(OUT)/platform/x11/, x11_main.o x11_image.o pdfapp.o)
	$(LINK_CMD) $(X11_LIBS) $(V8_LIBS)
endif
endif

MUVIEW := $(MUVIEW_X11)
MUVIEW_V8 := $(MUVIEW_X11_V8)
MUVIEW_CURL := $(MUVIEW_X11_CURL)

INSTALL_APPS := $(MUDRAW) $(MUTOOL) $(MUVIEW) $(MUJSTEST_V8) $(MUVIEW_V8) $(MUVIEW_CURL)

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

third: $(THIRD_LIBS)
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

tags: $(shell find include source -name '*.[ch]')
	ctags $^

all: libs apps

all-nojs:
	$(MAKE) V8_PRESENT=no

clean:
	rm -rf $(OUT)
nuke:
	rm -rf build/* $(GEN)

.PHONY: all clean nuke install third libs apps generate

# GNU Makefile

build ?= debug

OUT ?= build/$(build)
GEN := generated

# --- Variables, Commands, etc... ---

default: all

# Do not specify CFLAGS or LIBS on the make invocation line - specify
# XCFLAGS or XLIBS instead. Make ignores any lines in the makefile that
# set a variable that was set on the command line.
CFLAGS += $(XCFLAGS) -Ifitz -Ipdf -Ixps -Icbz -Iscripts
LIBS += $(XLIBS) -lfreetype -ljbig2dec -ljpeg -lopenjpeg -lz -lm
LIBS_V8 = $(LIBS) $(V8LIBS)

include Makerules
include Makethird

THIRD_LIBS := $(FREETYPE_LIB)
THIRD_LIBS += $(JBIG2DEC_LIB)
THIRD_LIBS += $(JPEG_LIB)
THIRD_LIBS += $(OPENJPEG_LIB)
THIRD_LIBS += $(ZLIB_LIB)

ifeq "$(verbose)" ""
QUIET_AR = @ echo ' ' ' ' AR $@ ;
QUIET_CC = @ echo ' ' ' ' CC $@ ;
QUIET_CXX = @ echo ' ' ' ' CXX $@ ;
QUIET_GEN = @ echo ' ' ' ' GEN $@ ;
QUIET_LINK = @ echo ' ' ' ' LINK $@ ;
QUIET_MKDIR = @ echo ' ' ' ' MKDIR $@ ;
endif

CC_CMD = $(QUIET_CC) $(CC) $(CFLAGS) -o $@ -c $<
CXX_CMD = $(QUIET_CXX) $(CXX) $(CFLAGS) -o $@ -c $<
AR_CMD = $(QUIET_AR) $(AR) cr $@ $^
LINK_CMD = $(QUIET_LINK) $(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
LINK_V8_CMD = $(QUIET_LINK) $(CXX) $(LDFLAGS) -o $@ $^ $(LIBS_V8)
MKDIR_CMD = $(QUIET_MKDIR) mkdir -p $@

# --- Rules ---

FITZ_HDR := fitz/fitz.h fitz/fitz-internal.h
MUPDF_HDR := $(FITZ_HDR) pdf/mupdf.h pdf/mupdf-internal.h
MUXPS_HDR := $(FITZ_HDR) xps/muxps.h xps/muxps-internal.h
MUCBZ_HDR := $(FITZ_HDR) cbz/mucbz.h

$(OUT) $(GEN) :
	$(MKDIR_CMD)

$(OUT)/%.a :
	$(AR_CMD)
	$(RANLIB_CMD)

$(OUT)/% : $(OUT)/%.o
	$(LINK_CMD)

$(OUT)/%.o : fitz/%.c $(FITZ_HDR) | $(OUT)
	$(CC_CMD)
$(OUT)/%.o : draw/%.c $(FITZ_HDR) | $(OUT)
	$(CC_CMD)
$(OUT)/%.o : pdf/%.c $(MUPDF_HDR) | $(OUT)
	$(CC_CMD)
$(OUT)/%.o : pdf/%.cpp $(MUPDF_HDR) | $(OUT)
	$(CXX_CMD)
$(OUT)/%.o : xps/%.c $(MUXPS_HDR) | $(OUT)
	$(CC_CMD)
$(OUT)/%.o : cbz/%.c $(MUCBZ_HDR) | $(OUT)
	$(CC_CMD)
$(OUT)/%.o : apps/%.c fitz/fitz.h pdf/mupdf.h xps/muxps.h cbz/mucbz.h | $(OUT)
	$(CC_CMD)
$(OUT)/%.o : scripts/%.c | $(OUT)
	$(CC_CMD)

.PRECIOUS : $(OUT)/%.o # Keep intermediates from chained rules

# --- Fitz, MuPDF, MuXPS and MuCBZ library ---

FITZ_LIB := $(OUT)/libfitz.a
FITZ_V8_LIB := $(OUT)/libfitzv8.a

FITZ_SRC := $(notdir $(wildcard fitz/*.c draw/*.c))
FITZ_SRC := $(filter-out draw_simple_scale.c, $(FITZ_SRC))
MUPDF_ALL_SRC := $(notdir $(wildcard pdf/*.c))
MUPDF_SRC := $(filter-out pdf_js.c pdf_jsimp_cpp.c, $(MUPDF_ALL_SRC))
MUPDF_V8_SRC := $(filter-out pdf_js_none.c, $(MUPDF_ALL_SRC))
MUPDF_V8_CPP_SRC := $(notdir $(wildcard pdf/*.cpp))
MUXPS_SRC := $(notdir $(wildcard xps/*.c))
MUCBZ_SRC := $(notdir $(wildcard cbz/*.c))

$(FITZ_LIB) : $(addprefix $(OUT)/, $(FITZ_SRC:%.c=%.o))
$(FITZ_LIB) : $(addprefix $(OUT)/, $(MUPDF_SRC:%.c=%.o))
$(FITZ_LIB) : $(addprefix $(OUT)/, $(MUXPS_SRC:%.c=%.o))
$(FITZ_LIB) : $(addprefix $(OUT)/, $(MUCBZ_SRC:%.c=%.o))

$(FITZ_V8_LIB) : $(addprefix $(OUT)/, $(FITZ_SRC:%.c=%.o))
$(FITZ_V8_LIB) : $(addprefix $(OUT)/, $(MUPDF_V8_SRC:%.c=%.o))
$(FITZ_V8_LIB) : $(addprefix $(OUT)/, $(MUPDF_V8_CPP_SRC:%.cpp=%.o))
$(FITZ_V8_LIB) : $(addprefix $(OUT)/, $(MUXPS_SRC:%.c=%.o))
$(FITZ_V8_LIB) : $(addprefix $(OUT)/, $(MUCBZ_SRC:%.c=%.o))

libs: $(FITZ_LIB) $(THIRD_LIBS)
libs_v8: libs $(FITZ_V8_LIB)

# --- Generated CMAP, FONT and JAVASCRIPT files ---

CMAPDUMP := $(OUT)/cmapdump
FONTDUMP := $(OUT)/fontdump
CQUOTE := $(OUT)/cquote

CMAP_CNS_SRC := $(wildcard cmaps/cns/*)
CMAP_GB_SRC := $(wildcard cmaps/gb/*)
CMAP_JAPAN_SRC := $(wildcard cmaps/japan/*)
CMAP_KOREA_SRC := $(wildcard cmaps/korea/*)
FONT_BASE14_SRC := $(wildcard fonts/*.cff)
FONT_DROID_SRC := fonts/droid/DroidSans.ttf fonts/droid/DroidSansMono.ttf
FONT_CJK_SRC := fonts/droid/DroidSansFallback.ttf
FONT_CJK_FULL_SRC := fonts/droid/DroidSansFallbackFull.ttf
JAVASCRIPT_SRC := pdf/pdf_util.js

$(GEN)/cmap_cns.h : $(CMAP_CNS_SRC)
	$(QUIET_GEN) ./$(CMAPDUMP) $@ $(CMAP_CNS_SRC)
$(GEN)/cmap_gb.h : $(CMAP_GB_SRC)
	$(QUIET_GEN) ./$(CMAPDUMP) $@ $(CMAP_GB_SRC)
$(GEN)/cmap_japan.h : $(CMAP_JAPAN_SRC)
	$(QUIET_GEN) ./$(CMAPDUMP) $@ $(CMAP_JAPAN_SRC)
$(GEN)/cmap_korea.h : $(CMAP_KOREA_SRC)
	$(QUIET_GEN) ./$(CMAPDUMP) $@ $(CMAP_KOREA_SRC)

$(GEN)/font_base14.h : $(FONT_BASE14_SRC)
	$(QUIET_GEN) ./$(FONTDUMP) $@ $(FONT_BASE14_SRC)
$(GEN)/font_droid.h : $(FONT_DROID_SRC)
	$(QUIET_GEN) ./$(FONTDUMP) $@ $(FONT_DROID_SRC)
$(GEN)/font_cjk.h : $(FONT_CJK_SRC)
	$(QUIET_GEN) ./$(FONTDUMP) $@ $(FONT_CJK_SRC)
$(GEN)/font_cjk_full.h : $(FONT_CJK_FULL_SRC)
	$(QUIET_GEN) ./$(FONTDUMP) $@ $(FONT_CJK_FULL_SRC)

$(GEN)/js_util.h : $(JAVASCRIPT_SRC)
	$(QUIET_GEN) ./$(CQUOTE) $@ $(JAVASCRIPT_SRC)

CMAP_HDR := $(addprefix $(GEN)/, cmap_cns.h cmap_gb.h cmap_japan.h cmap_korea.h)
FONT_HDR := $(GEN)/font_base14.h $(GEN)/font_droid.h $(GEN)/font_cjk.h $(GEN)/font_cjk_full.h
JAVASCRIPT_HDR := $(GEN)/js_util.h

ifeq "$(CROSSCOMPILE)" ""
$(CMAP_HDR) : $(CMAPDUMP) | $(GEN)
$(FONT_HDR) : $(FONTDUMP) | $(GEN)
$(JAVASCRIPT_HDR) : $(CQUOTE) | $(GEN)
endif

generate: $(CMAP_HDR) $(FONT_HDR) $(JAVASCRIPT_HDR)

$(OUT)/pdf_cmap_table.o : $(CMAP_HDR)
$(OUT)/pdf_fontfile.o : $(FONT_HDR)
$(OUT)/pdf_js.o : $(JAVASCRIPT_HDR)
$(OUT)/cmapdump.o : pdf/pdf_cmap.c pdf/pdf_cmap_parse.c

# --- Tools and Apps ---

MUDRAW := $(addprefix $(OUT)/, mudraw)
$(MUDRAW) : $(FITZ_LIB) $(THIRD_LIBS)

MUTOOL := $(addprefix $(OUT)/, mutool)
$(MUTOOL) : $(addprefix $(OUT)/, pdfclean.o pdfextract.o pdfinfo.o pdfposter.o pdfshow.o) $(FITZ_LIB) $(THIRD_LIBS)

ifeq "$(NOX11)" ""
MUVIEW := $(OUT)/mupdf
$(MUVIEW) : $(FITZ_LIB) $(THIRD_LIBS)
$(MUVIEW) : $(addprefix $(OUT)/, x11_main.o x11_image.o pdfapp.o)
	$(LINK_CMD) $(X11_LIBS)

MUVIEW_V8 := $(OUT)/mupdf-v8
$(MUVIEW_V8) : $(FITZ_V8_LIB) $(THIRD_LIBS)
$(MUVIEW_V8) : $(addprefix $(OUT)/, x11_main.o x11_image.o pdfapp.o)
	$(LINK_V8_CMD) $(X11_LIBS)
endif

MUJSTEST_V8 := $(OUT)/mujstest-v8
$(MUJSTEST_V8) : $(FITZ_V8_LIB) $(THIRD_LIBS)
$(MUJSTEST_V8) : $(addprefix $(OUT)/, jstest_main.o pdfapp.o)
	$(LINK_V8_CMD)

ifeq "$(V8_PRESENT)" "1"
JSTARGETS := $(MUJSTEST_V8) $(FITZ_V8_LIB) $(MUVIEW_V8)
else
JSTARGETS :=
endif

# --- Format man pages ---

%.txt: %.1
	nroff -man $< | col -b | expand > $@

MAN_FILES := $(wildcard apps/man/*.1)
TXT_FILES := $(MAN_FILES:%.1=%.txt)

catman: $(TXT_FILES)

# --- Install ---

prefix ?= /usr/local
bindir ?= $(prefix)/bin
libdir ?= $(prefix)/lib
incdir ?= $(prefix)/include
mandir ?= $(prefix)/share/man

install: $(FITZ_LIB) $(MUVIEW) $(MUDRAW) $(MUTOOL)
	install -d $(DESTDIR)$(bindir) $(DESTDIR)$(libdir) $(DESTDIR)$(incdir) $(DESTDIR)$(mandir)/man1
	install $(FITZ_LIB) $(DESTDIR)$(libdir)
	install fitz/memento.h fitz/fitz.h pdf/mupdf.h xps/muxps.h cbz/mucbz.h $(DESTDIR)$(incdir)
	install $(MUVIEW) $(MUDRAW) $(MUBUSY) $(DESTDIR)$(bindir)
	install $(wildcard apps/man/*.1) $(DESTDIR)$(mandir)/man1

# --- Clean and Default ---

all: all-nojs $(JSTARGETS)

all-nojs: $(THIRD_LIBS) $(FITZ_LIB) $(MUVIEW) $(MUDRAW) $(MUTOOL)

third: $(THIRD_LIBS)

clean:
	rm -rf $(OUT)
nuke:
	rm -rf build/* $(GEN)

.PHONY: all clean nuke install

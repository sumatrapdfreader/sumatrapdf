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
QUIET_GEN = @ echo ' ' ' ' GEN $@ ;
QUIET_LINK = @ echo ' ' ' ' LINK $@ ;
QUIET_MKDIR = @ echo ' ' ' ' MKDIR $@ ;
endif

CC_CMD = $(QUIET_CC) $(CC) $(CFLAGS) -o $@ -c $<
AR_CMD = $(QUIET_AR) $(AR) cr $@ $^
LINK_CMD = $(QUIET_LINK) $(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
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

FITZ_SRC := $(notdir $(wildcard fitz/*.c draw/*.c))
FITZ_SRC := $(filter-out draw_simple_scale.c, $(FITZ_SRC))
MUPDF_SRC := $(notdir $(wildcard pdf/*.c))
MUXPS_SRC := $(notdir $(wildcard xps/*.c))
MUCBZ_SRC := $(notdir $(wildcard cbz/*.c))

$(FITZ_LIB) : $(addprefix $(OUT)/, $(FITZ_SRC:%.c=%.o))
$(FITZ_LIB) : $(addprefix $(OUT)/, $(MUPDF_SRC:%.c=%.o))
$(FITZ_LIB) : $(addprefix $(OUT)/, $(MUXPS_SRC:%.c=%.o))
$(FITZ_LIB) : $(addprefix $(OUT)/, $(MUCBZ_SRC:%.c=%.o))

libs: $(FITZ_LIB) $(THIRD_LIBS)

# --- Generated CMAP and FONT files ---

CMAPDUMP := $(OUT)/cmapdump
FONTDUMP := $(OUT)/fontdump

CMAP_CNS_SRC := $(wildcard cmaps/cns/*)
CMAP_GB_SRC := $(wildcard cmaps/gb/*)
CMAP_JAPAN_SRC := $(wildcard cmaps/japan/*)
CMAP_KOREA_SRC := $(wildcard cmaps/korea/*)
FONT_BASE14_SRC := $(wildcard fonts/*.cff)
FONT_DROID_SRC := fonts/droid/DroidSans.ttf fonts/droid/DroidSansMono.ttf
FONT_CJK_SRC := fonts/droid/DroidSansFallback.ttf

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

CMAP_HDR := $(addprefix $(GEN)/, cmap_cns.h cmap_gb.h cmap_japan.h cmap_korea.h)
FONT_HDR := $(GEN)/font_base14.h $(GEN)/font_droid.h $(GEN)/font_cjk.h

ifeq "$(CROSSCOMPILE)" ""
$(CMAP_HDR) : $(CMAPDUMP) | $(GEN)
$(FONT_HDR) : $(FONTDUMP) | $(GEN)
endif

generate: $(CMAP_HDR) $(FONT_HDR)

$(OUT)/pdf_cmap_table.o : $(CMAP_HDR)
$(OUT)/pdf_fontfile.o : $(FONT_HDR)
$(OUT)/cmapdump.o : pdf/pdf_cmap.c pdf/pdf_cmap_parse.c

# --- Tools and Apps ---

MUDRAW := $(addprefix $(OUT)/, mudraw)
$(MUDRAW) : $(FITZ_LIB) $(THIRD_LIBS)

MUBUSY := $(addprefix $(OUT)/, mubusy)
$(MUBUSY) : $(addprefix $(OUT)/, mupdfclean.o mupdfextract.o mupdfinfo.o mupdfposter.o mupdfshow.o) $(FITZ_LIB) $(THIRD_LIBS)

ifeq "$(NOX11)" ""
MUVIEW := $(OUT)/mupdf
$(MUVIEW) : $(FITZ_LIB) $(THIRD_LIBS)
$(MUVIEW) : $(addprefix $(OUT)/, x11_main.o x11_image.o pdfapp.o)
	$(LINK_CMD) $(X11_LIBS)
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

install: $(FITZ_LIB) $(MUVIEW) $(MUDRAW) $(MUBUSY)
	install -d $(bindir) $(libdir) $(incdir) $(mandir)/man1
	install $(FITZ_LIB) $(libdir)
	install fitz/memento.h fitz/fitz.h pdf/mupdf.h xps/muxps.h cbz/mucbz.h $(incdir)
	install $(MUVIEW) $(MUDRAW) $(MUBUSY) $(bindir)
	install $(wildcard apps/man/*.1) $(mandir)/man1

# --- Clean and Default ---

all: all-nojs

all-nojs: $(THIRD_LIBS) $(FITZ_LIB) $(MUVIEW) $(MUDRAW) $(MUBUSY)

third: $(THIRD_LIBS)

clean:
	rm -rf $(OUT)
nuke:
	rm -rf build/* $(GEN)

.PHONY: all clean nuke install

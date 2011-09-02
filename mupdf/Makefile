# GNU Makefile

build ?= debug

OUT := build/$(build)
GEN := generated

# --- Variables, Commands, etc... ---

default: all

CFLAGS += -Ifitz -Ipdf -Ixps -Iscripts
LIBS += -lfreetype -ljbig2dec -ljpeg -lopenjpeg -lz -lm

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
AR_CMD = $(QUIET_AR) $(AR) cru $@ $^
LINK_CMD = $(QUIET_LINK) $(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
MKDIR_CMD = $(QUIET_MKDIR) mkdir -p $@

# --- Rules ---

$(OUT) $(GEN) :
	$(MKDIR_CMD)

$(OUT)/%.a :
	$(AR_CMD)

$(OUT)/% : $(OUT)/%.o
	$(LINK_CMD)

$(OUT)/%.o : fitz/%.c fitz/fitz.h | $(OUT)
	$(CC_CMD)
$(OUT)/%.o : draw/%.c fitz/fitz.h | $(OUT)
	$(CC_CMD)
$(OUT)/%.o : pdf/%.c fitz/fitz.h pdf/mupdf.h | $(OUT)
	$(CC_CMD)
$(OUT)/%.o : xps/%.c fitz/fitz.h xps/muxps.h | $(OUT)
	$(CC_CMD)
$(OUT)/%.o : apps/%.c fitz/fitz.h pdf/mupdf.h xps/muxps.h | $(OUT)
	$(CC_CMD)
$(OUT)/%.o : scripts/%.c | $(OUT)
	$(CC_CMD)

.PRECIOUS : $(OUT)/%.o # Keep intermediates from chained rules

# --- Fitz, MuPDF and MuXPS libraries ---

FITZ_LIB := $(OUT)/libfitz.a
MUPDF_LIB := $(OUT)/libmupdf.a
MUXPS_LIB := $(OUT)/libmuxps.a

FITZ_SRC := $(notdir $(wildcard fitz/*.c draw/*.c))
MUPDF_SRC := $(notdir $(wildcard pdf/*.c))
MUXPS_SRC := $(notdir $(wildcard xps/*.c))

$(FITZ_LIB) : $(addprefix $(OUT)/, $(FITZ_SRC:%.c=%.o))
$(MUPDF_LIB) : $(addprefix $(OUT)/, $(MUPDF_SRC:%.c=%.o))
$(MUXPS_LIB) : $(addprefix $(OUT)/, $(MUXPS_SRC:%.c=%.o))

libs: $(MUXPS_LIB) $(MUPDF_LIB) $(FITZ_LIB) $(THIRD_LIBS)
	@ echo MuPDF/XPS and underlying libraries built

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

PDF_APPS := $(addprefix $(OUT)/, pdfdraw pdfclean pdfextract pdfinfo pdfshow)
XPS_APPS := $(addprefix $(OUT)/, xpsdraw)

$(PDF_APPS) : $(MUPDF_LIB) $(FITZ_LIB) $(THIRD_LIBS)
$(XPS_APPS) : $(MUXPS_LIB) $(FITZ_LIB) $(THIRD_LIBS)

MUPDF := $(OUT)/mupdf
$(MUPDF) : $(MUXPS_LIB) $(MUPDF_LIB) $(FITZ_LIB) $(THIRD_LIBS)
ifeq "$(NOX11)" ""
$(MUPDF) : $(addprefix $(OUT)/, x11_main.o x11_image.o pdfapp.o)
	$(LINK_CMD) $(X11_LIBS)
endif

# --- Install ---

prefix ?= /usr/local
bindir ?= $(prefix)/bin
libdir ?= $(prefix)/lib
incdir ?= $(prefix)/include
mandir ?= $(prefix)/share/man

install: $(MUXPS_LIB) $(MUPDF_LIB) $(FITZ_LIB) $(PDF_APPS) $(XPS_APPS) $(MUPDF)
	install -d $(bindir) $(libdir) $(incdir) $(mandir)/man1
	install $(MUXPS_LIB) $(MUPDF_LIB) $(FITZ_LIB) $(libdir)
	install fitz/fitz.h pdf/mupdf.h xps/muxps.h $(incdir)
	install $(PDF_APPS) $(XPS_APPS) $(MUPDF) $(bindir)
	install $(wildcard apps/man/*.1) $(mandir)/man1

# --- Clean and Default ---

all: $(THIRD_LIBS) $(FITZ_LIB) $(PDF_APPS) $(XPS_APPS) $(MUPDF)

clean:
	rm -rf $(OUT)
nuke:
	rm -rf build/* $(GEN)

.PHONY: all clean nuke install

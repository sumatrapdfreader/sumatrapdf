# GNU Makefile

-include user.make

ifndef build
  build := release
endif

ifndef OUT
  OUT := build/$(build)
endif

default: all

# --- Configuration ---

include Makerules
include Makethird

# Do not specify CFLAGS or LIBS on the make invocation line - specify
# XCFLAGS or XLIBS instead. Make ignores any lines in the makefile that
# set a variable that was set on the command line.
CFLAGS += $(XCFLAGS) -Iinclude
LIBS += $(XLIBS) -lm

ifneq ($(threading),no)
  ifeq ($(HAVE_PTHREAD),yes)
	THREADING_CFLAGS := $(PTHREAD_CFLAGS) -DHAVE_PTHREAD
	THREADING_LIBS := $(PTHREAD_LIBS)
  endif
endif

ifeq ($(HAVE_WIN32),yes)
  WIN32_LIBS := -lcomdlg32 -lgdi32
  WIN32_LDFLAGS := -Wl,-subsystem,windows
endif

# --- Commands ---

ifneq ($(verbose),yes)
  QUIET_AR = @ echo "    AR $@" ;
  QUIET_RANLIB = @ echo "    RANLIB $@" ;
  QUIET_CC = @ echo "    CC $@" ;
  QUIET_CXX = @ echo "    CXX $@" ;
  QUIET_GEN = @ echo "    GEN $@" ;
  QUIET_LINK = @ echo "    LINK $@" ;
  QUIET_RM = @ echo "    RM $@" ;
  QUIET_TAGS = @ echo "    TAGS $@" ;
  QUIET_WINDRES = @ echo "    WINDRES $@" ;
  QUIET_OBJCOPY = @ echo "    OBJCOPY $@" ;
endif

MKTGTDIR = mkdir -p $(dir $@)
CC_CMD = $(QUIET_CC) $(MKTGTDIR) ; $(CC) $(CFLAGS) -MMD -MP -o $@ -c $<
CXX_CMD = $(QUIET_CXX) $(MKTGTDIR) ; $(CXX) $(CFLAGS) -MMD -MP -o $@ -c $<
AR_CMD = $(QUIET_AR) $(MKTGTDIR) ; $(AR) cr $@ $^
ifdef RANLIB
  RANLIB_CMD = $(QUIET_RANLIB) $(RANLIB) $@
endif
LINK_CMD = $(QUIET_LINK) $(MKTGTDIR) ; $(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
TAGS_CMD = $(QUIET_TAGS) ctags -R
WINDRES_CMD = $(QUIET_WINDRES) $(MKTGTDIR) ; $(WINDRES) $< $@
OBJCOPY_CMD = $(QUIET_OBJCOPY) $(MKTGTDIR) ; $(LD) -r -b binary -o $@ $<

# --- Rules ---

$(OUT)/%.a :
	$(AR_CMD)
	$(RANLIB_CMD)

$(OUT)/%.exe: %.c
	$(LINK_CMD)

$(OUT)/source/helpers/mu-threads/%.o : source/helpers/mu-threads/%.c
	$(CC_CMD) $(THREADING_CFLAGS)

$(OUT)/source/helpers/pkcs7/%.o : source/helpers/pkcs7/%.c
	$(CC_CMD) $(LIBCRYPTO_CFLAGS)

$(OUT)/source/tools/%.o : source/tools/%.c
	$(CC_CMD) -Wall $(THIRD_CFLAGS) $(THREADING_CFLAGS)

$(OUT)/generated/%.o : generated/%.c
	$(CC_CMD) -O0

$(OUT)/platform/x11/%.o : platform/x11/%.c
	$(CC_CMD) -Wall $(X11_CFLAGS)

$(OUT)/platform/x11/curl/%.o : platform/x11/%.c
	$(CC_CMD) -Wall $(X11_CFLAGS) $(CURL_CFLAGS) -DHAVE_CURL

$(OUT)/platform/gl/%.o : platform/gl/%.c
	$(CC_CMD) -Wall $(THIRD_CFLAGS) $(GLUT_CFLAGS)

ifeq ($(HAVE_OBJCOPY),yes)
  $(OUT)/source/fitz/noto.o : source/fitz/noto.c
	$(CC_CMD) -Wall -Wdeclaration-after-statement -DHAVE_OBJCOPY $(THIRD_CFLAGS)
endif

$(OUT)/source/%.o : source/%.c
	$(CC_CMD) -Wall -Wdeclaration-after-statement $(THIRD_CFLAGS)

$(OUT)/platform/%.o : platform/%.c
	$(CC_CMD) -Wall

$(OUT)/%.o: %.rc
	$(WINDRES_CMD)

.PRECIOUS : $(OUT)/%.o # Keep intermediates from chained rules
.PRECIOUS : $(OUT)/%.exe # Keep intermediates from chained rules

# --- File lists ---

THIRD_OBJ := $(THIRD_SRC:%.c=$(OUT)/%.o)
THIRD_OBJ := $(THIRD_OBJ:%.cc=$(OUT)/%.o)

MUPDF_SRC := $(sort $(wildcard source/fitz/*.c))
MUPDF_SRC += $(sort $(wildcard source/pdf/*.c))
MUPDF_SRC += $(sort $(wildcard source/xps/*.c))
MUPDF_SRC += $(sort $(wildcard source/svg/*.c))
MUPDF_SRC += $(sort $(wildcard source/html/*.c))
MUPDF_SRC += $(sort $(wildcard source/cbz/*.c))

MUPDF_OBJ := $(MUPDF_SRC:%.c=$(OUT)/%.o)

THREAD_SRC := source/helpers/mu-threads/mu-threads.c
THREAD_OBJ := $(THREAD_SRC:%.c=$(OUT)/%.o)

PKCS7_SRC := source/helpers/pkcs7/pkcs7-check.c
PKCS7_SRC += source/helpers/pkcs7/pkcs7-openssl.c
PKCS7_OBJ := $(PKCS7_SRC:%.c=$(OUT)/%.o)

# --- Generated embedded font files ---

HEXDUMP_EXE := $(OUT)/scripts/hexdump.exe

FONT_BIN := $(sort $(wildcard resources/fonts/urw/*.cff))
FONT_BIN += $(sort $(wildcard resources/fonts/han/*.ttc))
FONT_BIN += $(sort $(wildcard resources/fonts/droid/*.ttf))
FONT_BIN += $(sort $(wildcard resources/fonts/noto/*.otf))
FONT_BIN += $(sort $(wildcard resources/fonts/noto/*.ttf))
FONT_BIN += $(sort $(wildcard resources/fonts/sil/*.cff))

FONT_GEN := $(FONT_BIN:%=generated/%.c)

generated/%.cff.c : %.cff $(HEXDUMP_EXE) ; $(QUIET_GEN) $(MKTGTDIR) ; $(HEXDUMP_EXE) -s $@ $<
generated/%.otf.c : %.otf $(HEXDUMP_EXE) ; $(QUIET_GEN) $(MKTGTDIR) ; $(HEXDUMP_EXE) -s $@ $<
generated/%.ttf.c : %.ttf $(HEXDUMP_EXE) ; $(QUIET_GEN) $(MKTGTDIR) ; $(HEXDUMP_EXE) -s $@ $<
generated/%.ttc.c : %.ttc $(HEXDUMP_EXE) ; $(QUIET_GEN) $(MKTGTDIR) ; $(HEXDUMP_EXE) -s $@ $<

ifeq ($(HAVE_OBJCOPY),yes)
  MUPDF_OBJ += $(FONT_BIN:%=$(OUT)/%.o)
  $(OUT)/%.cff.o : %.cff ; $(OBJCOPY_CMD)
  $(OUT)/%.otf.o : %.otf ; $(OBJCOPY_CMD)
  $(OUT)/%.ttf.o : %.ttf ; $(OBJCOPY_CMD)
  $(OUT)/%.ttc.o : %.ttc ; $(OBJCOPY_CMD)
else
  MUPDF_OBJ += $(FONT_GEN:%.c=$(OUT)/%.o)
endif

generate: $(FONT_GEN)

# --- Generated ICC profiles ---

source/fitz/icc/%.icc.h: resources/icc/%.icc
	$(QUIET_GEN) xxd -i $< | sed 's/unsigned/static const unsigned/' > $@

generate: source/fitz/icc/gray.icc.h
generate: source/fitz/icc/rgb.icc.h
generate: source/fitz/icc/cmyk.icc.h
generate: source/fitz/icc/lab.icc.h

# --- Generated CMap files ---

CMAP_GEN := $(notdir $(sort $(wildcard resources/cmaps/*)))
CMAP_GEN := $(CMAP_GEN:%=source/pdf/cmaps/%.h)

source/pdf/cmaps/%.h: resources/cmaps/% scripts/cmapdump.py
	$(QUIET_GEN) python3 scripts/cmapdump.py > $@ $<

generate: $(CMAP_GEN)

# --- Generated embedded javascript files ---

source/pdf/js/%.js.h: source/pdf/js/%.js scripts/jsdump.sed
	$(QUIET_GEN) sed -f scripts/jsdump.sed < $< > $@

generate: source/pdf/js/util.js.h

# --- Library ---

MUPDF_LIB = $(OUT)/libmupdf.a
THIRD_LIB = $(OUT)/libmupdf-third.a
THREAD_LIB = $(OUT)/libmupdf-threads.a
PKCS7_LIB = $(OUT)/libmupdf-pkcs7.a

$(MUPDF_LIB) : $(MUPDF_OBJ)
$(THIRD_LIB) : $(THIRD_OBJ)
$(THREAD_LIB) : $(THREAD_OBJ)
$(PKCS7_LIB) : $(PKCS7_OBJ)

INSTALL_LIBS := $(MUPDF_LIB) $(THIRD_LIB)

# --- Main tools and viewers ---

MUTOOL_SRC := source/tools/mutool.c
MUTOOL_SRC += source/tools/muconvert.c
MUTOOL_SRC += source/tools/mudraw.c
MUTOOL_SRC += source/tools/murun.c
MUTOOL_SRC += source/tools/mutrace.c
MUTOOL_SRC += source/tools/cmapdump.c
MUTOOL_SRC += $(sort $(wildcard source/tools/pdf*.c))
MUTOOL_OBJ := $(MUTOOL_SRC:%.c=$(OUT)/%.o)
MUTOOL_EXE := $(OUT)/mutool
$(MUTOOL_EXE) : $(MUTOOL_OBJ) $(MUPDF_LIB) $(THIRD_LIB) $(PKCS7_LIB) $(THREAD_LIB)
	$(LINK_CMD) $(THIRD_LIBS) $(THREADING_LIBS) $(LIBCRYPTO_LIBS)
TOOL_APPS += $(MUTOOL_EXE)

MURASTER_OBJ := $(OUT)/source/tools/muraster.o
MURASTER_EXE := $(OUT)/muraster
$(MURASTER_EXE) : $(MURASTER_OBJ) $(MUPDF_LIB) $(THIRD_LIB) $(THREAD_LIB)
	$(LINK_CMD) $(THIRD_LIBS) $(THREADING_LIBS)
TOOL_APPS += $(MURASTER_EXE)

ifeq ($(HAVE_GLUT),yes)
  MUVIEW_GLUT_SRC += $(sort $(wildcard platform/gl/*.c))
  MUVIEW_GLUT_OBJ := $(MUVIEW_GLUT_SRC:%.c=$(OUT)/%.o)
  MUVIEW_GLUT_EXE := $(OUT)/mupdf-gl
  $(MUVIEW_GLUT_EXE) : $(MUVIEW_GLUT_OBJ) $(MUPDF_LIB) $(THIRD_LIB) $(PKCS7_LIB) $(GLUT_LIB)
	$(LINK_CMD) $(THIRD_LIBS) $(LIBCRYPTO_LIBS) $(WIN32_LDFLAGS) $(GLUT_LIBS)
  VIEW_APPS += $(MUVIEW_GLUT_EXE)
endif

ifeq ($(HAVE_X11),yes)
  MUVIEW_X11_EXE := $(OUT)/mupdf-x11
  MUVIEW_X11_OBJ += $(OUT)/platform/x11/pdfapp.o
  MUVIEW_X11_OBJ += $(OUT)/platform/x11/x11_main.o
  MUVIEW_X11_OBJ += $(OUT)/platform/x11/x11_image.o
  $(MUVIEW_X11_EXE) : $(MUVIEW_X11_OBJ) $(MUPDF_LIB) $(THIRD_LIB) $(PKCS7_LIB)
	$(LINK_CMD) $(THIRD_LIBS) $(X11_LIBS) $(LIBCRYPTO_LIBS)
  VIEW_APPS += $(MUVIEW_X11_EXE)
endif

ifeq ($(HAVE_WIN32),yes)
  MUVIEW_WIN32_EXE := $(OUT)/mupdf-w32
  MUVIEW_WIN32_OBJ += $(OUT)/platform/x11/pdfapp.o
  MUVIEW_WIN32_OBJ += $(OUT)/platform/x11/win_main.o
  MUVIEW_WIN32_OBJ += $(OUT)/platform/x11/win_res.o
  $(MUVIEW_WIN32_EXE) : $(MUVIEW_WIN32_OBJ) $(MUPDF_LIB) $(THIRD_LIB) $(PKCS7_LIB)
	$(LINK_CMD) $(THIRD_LIBS) $(WIN32_LDFLAGS) $(WIN32_LIBS) $(LIBCRYPTO_LIBS)
  VIEW_APPS += $(MUVIEW_WIN32_EXE)
endif

ifeq ($(HAVE_X11),yes)
ifeq ($(HAVE_CURL),yes)
ifeq ($(HAVE_PTHREAD),yes)
  MUVIEW_X11_CURL_EXE := $(OUT)/mupdf-x11-curl
  MUVIEW_X11_CURL_OBJ += $(OUT)/platform/x11/curl/pdfapp.o
  MUVIEW_X11_CURL_OBJ += $(OUT)/platform/x11/curl/x11_main.o
  MUVIEW_X11_CURL_OBJ += $(OUT)/platform/x11/curl/x11_image.o
  MUVIEW_X11_CURL_OBJ += $(OUT)/platform/x11/curl/curl_stream.o
  MUVIEW_X11_CURL_OBJ += $(OUT)/platform/x11/curl/prog_stream.o
  $(MUVIEW_X11_CURL_EXE) : $(MUVIEW_X11_CURL_OBJ) $(MUPDF_LIB) $(THIRD_LIB) $(PKCS7_LIB) $(CURL_LIB)
	$(LINK_CMD) $(THIRD_LIBS) $(X11_LIBS) $(LIBCRYPTO_LIBS) $(CURL_LIBS) $(PTHREAD_LIBS)
  VIEW_APPS += $(MUVIEW_X11_CURL_EXE)
endif
endif
endif

# --- Generated dependencies ---

-include $(MUPDF_OBJ:%.o=%.d)
-include $(PKCS7_OBJ:%.o=%.d)
-include $(THREAD_OBJ:%.o=%.d)
-include $(THIRD_OBJ:%.o=%.d)
-include $(GLUT_OBJ:%.o=%.d)

-include $(MUTOOL_OBJ:%.o=%.d)
-include $(MUVIEW_GLUT_OBJ:%.o=%.d)
-include $(MUVIEW_X11_OBJ:%.o=%.d)
-include $(MUVIEW_WIN32_OBJ:%.o=%.d)

-include $(MURASTER_OBJ:%.o=%.d)
-include $(MUVIEW_X11_CURL_OBJ:%.o=%.d)

# --- Examples ---

$(OUT)/example: docs/examples/example.c $(MUPDF_LIB) $(THIRD_LIB)
	$(LINK_CMD) $(CFLAGS) $(THIRD_LIBS)
$(OUT)/multi-threaded: docs/examples/multi-threaded.c $(MUPDF_LIB) $(THIRD_LIB)
	$(LINK_CMD) $(CFLAGS) $(THIRD_LIBS) -lpthread

# --- Update version string header ---

VERSION = $(shell git describe --tags)

version:
	sed -i~ -e '/FZ_VERSION /s/".*"/"'$(VERSION)'"/' include/mupdf/fitz/version.h

# --- Format man pages ---

%.txt: %.1
	nroff -man $< | col -b | expand > $@

MAN_FILES := $(sort $(wildcard docs/man/*.1))
TXT_FILES := $(MAN_FILES:%.1=%.txt)

catman: $(TXT_FILES)

# --- Install ---

prefix ?= /usr/local
bindir ?= $(prefix)/bin
libdir ?= $(prefix)/lib
incdir ?= $(prefix)/include
mandir ?= $(prefix)/share/man
docdir ?= $(prefix)/share/doc/mupdf

third: $(THIRD_LIB)
extra-libs: $(GLUT_LIB)
libs: $(INSTALL_LIBS)
tools: $(TOOL_APPS)
apps: $(TOOL_APPS) $(VIEW_APPS)

install: libs apps
	install -d $(DESTDIR)$(incdir)/mupdf
	install -d $(DESTDIR)$(incdir)/mupdf/fitz
	install -d $(DESTDIR)$(incdir)/mupdf/pdf
	install -m 644 include/mupdf/*.h $(DESTDIR)$(incdir)/mupdf
	install -m 644 include/mupdf/fitz/*.h $(DESTDIR)$(incdir)/mupdf/fitz
	install -m 644 include/mupdf/pdf/*.h $(DESTDIR)$(incdir)/mupdf/pdf

	install -d $(DESTDIR)$(libdir)
	install -m 644 $(INSTALL_LIBS) $(DESTDIR)$(libdir)

	install -d $(DESTDIR)$(bindir)
	install -m 755 $(TOOL_APPS) $(VIEW_APPS) $(DESTDIR)$(bindir)

	install -d $(DESTDIR)$(mandir)/man1
	install -m 644 docs/man/*.1 $(DESTDIR)$(mandir)/man1

	install -d $(DESTDIR)$(docdir)
	install -d $(DESTDIR)$(docdir)/examples
	install -m 644 README COPYING CHANGES $(DESTDIR)$(docdir)
	install -m 644 docs/*.html docs/*.css docs/*.png $(DESTDIR)$(docdir)
	install -m 644 docs/examples/* $(DESTDIR)$(docdir)/examples

tarball:
	bash scripts/archive.sh

# --- Clean and Default ---

WATCH_SRCS = $(shell find include source platform -type f -name '*.[ch]')
watch:
	@ inotifywait -q -e modify $(WATCH_SRCS)

watch-recompile:
	@ while ! inotifywait -q -e modify $(WATCH_SRCS) ; do time -p $(MAKE) ; done

java:
	$(MAKE) -C platform/java

wasm:
	$(MAKE) -C platform/wasm

tags:
	$(TAGS_CMD)

cscope.files: $(shell find include source platform -name '*.[ch]')
	@ echo $^ | tr ' ' '\n' > $@

cscope.out: cscope.files
	cscope -b

all: libs apps

clean:
	rm -rf $(OUT)
nuke:
	rm -rf build/* generated

release:
	$(MAKE) build=release
debug:
	$(MAKE) build=debug
sanitize:
	$(MAKE) build=sanitize

android: generate
	ndk-build -j8 \
		APP_BUILD_SCRIPT=platform/java/Android.mk \
		APP_PROJECT_PATH=build/android \
		APP_PLATFORM=android-16 \
		APP_OPTIM=$(build)

.PHONY: all clean nuke install third libs apps generate tags wasm

# GNU Makefile

-include user.make

ifndef build
  build := release
endif

default: all

include Makerules

ifndef OUT
  OUT := build/$(build_prefix)$(build)$(build_suffix)
endif

include Makethird

# --- Configuration ---

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

VERSION_MAJOR = $(shell grep "define FZ_VERSION_MAJOR" include/mupdf/fitz/version.h | cut -d ' ' -f 3)
VERSION_MINOR = $(shell grep "define FZ_VERSION_MINOR" include/mupdf/fitz/version.h | cut -d ' ' -f 3)
VERSION_PATCH = $(shell grep "define FZ_VERSION_PATCH" include/mupdf/fitz/version.h | cut -d ' ' -f 3)

ifeq ($(LINUX_OR_OPENBSD),yes)
  SO_VERSION = .$(VERSION_MINOR).$(VERSION_PATCH)
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
  QUIET_DLLTOOL = @ echo "    DLLTOOL $@" ;
  QUIET_GENDEF = @ echo "    GENDEF $@" ;
endif

MKTGTDIR = mkdir -p $(dir $@)
CC_CMD = $(QUIET_CC) $(MKTGTDIR) ; $(CC) $(CFLAGS) -MMD -MP -o $@ -c $<
CXX_CMD = $(QUIET_CXX) $(MKTGTDIR) ; $(CXX) $(CFLAGS) $(XCXXFLAGS) -MMD -MP -o $@ -c $<
AR_CMD = $(QUIET_AR) $(MKTGTDIR) ; $(AR) cr $@ $^
ifdef RANLIB
  RANLIB_CMD = $(QUIET_RANLIB) $(RANLIB) $@
endif
LINK_CMD = $(QUIET_LINK) $(MKTGTDIR) ; $(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
TAGS_CMD = $(QUIET_TAGS) ctags
WINDRES_CMD = $(QUIET_WINDRES) $(MKTGTDIR) ; $(WINDRES) $< $@
OBJCOPY_CMD = $(QUIET_OBJCOPY) $(MKTGTDIR) ; $(LD) -r -b binary -z noexecstack -o $@ $<
GENDEF_CMD = $(QUIET_GENDEF) gendef - $< > $@
DLLTOOL_CMD = $(QUIET_DLLTOOL) dlltool -d $< -D $(notdir $(^:%.def=%.dll)) -l $@

ifeq ($(shared),yes)
LINK_CMD = $(QUIET_LINK) $(MKTGTDIR) ; $(CC) $(LDFLAGS) -o $@ \
	$(filter-out %.$(SO)$(SO_VERSION),$^) \
	$(sort $(patsubst %,-L%,$(dir $(filter %.$(SO)$(SO_VERSION),$^)))) \
	$(patsubst lib%.$(SO)$(SO_VERSION),-l%,$(notdir $(filter %.$(SO)$(SO_VERSION),$^))) \
	$(LIBS)
endif

# --- Rules ---

$(OUT)/%.a :
	$(AR_CMD)
	$(RANLIB_CMD)

$(OUT)/%.exe: %.c
	$(LINK_CMD)

$(OUT)/%.$(SO)$(SO_VERSION):
	$(LINK_CMD) $(LIB_LDFLAGS) $(THIRD_LIBS) $(LIBCRYPTO_LIBS)
ifneq ($(SO_VERSION),)
	ln -sf $(notdir $@) $(patsubst %$(SO_VERSION), %, $@)
endif

$(OUT)/%.def: $(OUT)/%.$(SO)$(SO_VERSION)
	$(GENDEF_CMD)

$(OUT)/%_$(SO)$(SO_VERSION).a: $(OUT)/%.def
	$(DLLTOOL_CMD)

$(OUT)/source/helpers/mu-threads/%.o : source/helpers/mu-threads/%.c
	$(CC_CMD) $(WARNING_CFLAGS) $(LIB_CFLAGS) $(THREADING_CFLAGS)

$(OUT)/source/helpers/pkcs7/%.o : source/helpers/pkcs7/%.c
	$(CC_CMD) $(WARNING_CFLAGS) $(LIB_CFLAGS) $(LIBCRYPTO_CFLAGS)

$(OUT)/source/tools/%.o : source/tools/%.c
	$(CC_CMD) $(LIB_CFLAGS) $(WARNING_CFLAGS) $(THIRD_CFLAGS) $(THREADING_CFLAGS)

$(OUT)/generated/%.o : generated/%.c
	$(CC_CMD) $(WARNING_CFLAGS) $(LIB_CFLAGS) -O0

$(OUT)/platform/x11/%.o : platform/x11/%.c
	$(CC_CMD) $(WARNING_CFLAGS) $(X11_CFLAGS)

$(OUT)/platform/x11/curl/%.o : platform/x11/%.c
	$(CC_CMD) $(WARNING_CFLAGS) $(X11_CFLAGS) $(CURL_CFLAGS)

$(OUT)/platform/gl/%.o : platform/gl/%.c
	$(CC_CMD) $(WARNING_CFLAGS) $(THIRD_CFLAGS) $(THIRD_GLUT_CFLAGS)

ifeq ($(HAVE_OBJCOPY),yes)
  $(OUT)/source/fitz/noto.o : source/fitz/noto.c
	$(CC_CMD) $(WARNING_CFLAGS) -Wdeclaration-after-statement -DHAVE_OBJCOPY $(LIB_CFLAGS) $(THIRD_CFLAGS)
endif

$(OUT)/source/fitz/memento.o : source/fitz/memento.c
	$(CC_CMD) $(WARNING_CFLAGS) $(LIB_CFLAGS) $(THIRD_CFLAGS) -DMEMENTO_MUPDF_HACKS

$(OUT)/source/%.o : source/%.c
	$(CC_CMD) $(WARNING_CFLAGS) -Wdeclaration-after-statement $(LIB_CFLAGS) $(THIRD_CFLAGS)

$(OUT)/source/%.o : source/%.cpp
	$(CXX_CMD) $(WARNING_CFLAGS) $(LIB_CFLAGS) $(THIRD_CFLAGS)

ifeq ($(HAVE_TESSERACT),yes)
$(OUT)/source/fitz/tessocr.o : source/fitz/tessocr.cpp
	$(CXX_CMD) $(WARNING_CFLAGS) $(LIB_CFLAGS) $(THIRD_CFLAGS) $(TESSERACT_CFLAGS) $(TESSERACT_DEFINES) $(TESSERACT_LANGFLAGS)
endif

ifeq ($(HAVE_LEPTONICA),yes)
$(OUT)/source/fitz/leptonica-wrap.o : source/fitz/leptonica-wrap.c
	$(CC_CMD) $(WARNING_CFLAGS) $(LIB_CFLAGS) $(THIRD_CFLAGS) $(LEPTONICA_CFLAGS) $(LEPTONICA_DEFINES) $(LEPTONICA_BUILD_CFLAGS)
endif

$(OUT)/platform/%.o : platform/%.c
	$(CC_CMD) $(WARNING_CFLAGS)

$(OUT)/%.o: %.rc
	$(WINDRES_CMD)

.PRECIOUS : $(OUT)/%.o # Keep intermediates from chained rules
.PRECIOUS : $(OUT)/%.exe # Keep intermediates from chained rules

# --- File lists ---

THIRD_OBJ := $(THIRD_SRC:%.c=$(OUT)/%.o)
THIRD_OBJ := $(THIRD_OBJ:%.cc=$(OUT)/%.o)
THIRD_OBJ := $(THIRD_OBJ:%.cpp=$(OUT)/%.o)

THIRD_GLUT_OBJ := $(THIRD_GLUT_SRC:%.c=$(OUT)/%.o)

MUPDF_SRC := $(sort $(wildcard source/fitz/*.c))
MUPDF_SRC += $(sort $(wildcard source/fitz/*.cpp))
MUPDF_SRC += $(sort $(wildcard source/pdf/*.c))
MUPDF_SRC += $(sort $(wildcard source/xps/*.c))
MUPDF_SRC += $(sort $(wildcard source/svg/*.c))
MUPDF_SRC += $(sort $(wildcard source/html/*.c))
MUPDF_SRC += $(sort $(wildcard source/reflow/*.c))
MUPDF_SRC += $(sort $(wildcard source/cbz/*.c))

MUPDF_OBJ := $(MUPDF_SRC:%.c=$(OUT)/%.o)
MUPDF_OBJ := $(MUPDF_OBJ:%.cpp=$(OUT)/%.o)

THREAD_SRC := source/helpers/mu-threads/mu-threads.c
THREAD_OBJ := $(THREAD_SRC:%.c=$(OUT)/%.o)

PKCS7_SRC += source/helpers/pkcs7/pkcs7-openssl.c
PKCS7_OBJ := $(PKCS7_SRC:%.c=$(OUT)/%.o)

# --- Generated embedded font files ---

HEXDUMP_SH := scripts/hexdump.sh

FONT_BIN := $(sort $(wildcard resources/fonts/urw/*.cff))
FONT_BIN += $(sort $(wildcard resources/fonts/han/*.ttc))
FONT_BIN += $(sort $(wildcard resources/fonts/droid/DroidSansFallbackFull.ttf))
FONT_BIN += $(sort $(wildcard resources/fonts/droid/DroidSansFallback.ttf))
FONT_BIN += $(sort $(wildcard resources/fonts/noto/*.otf))
FONT_BIN += $(sort $(wildcard resources/fonts/noto/*.ttf))
FONT_BIN += $(sort $(wildcard resources/fonts/sil/*.cff))

# Note: The tests here must match the equivalent tests in noto.c

ifneq ($(filter -DTOFU_CJK,$(XCFLAGS)),)
  FONT_BIN := $(filter-out resources/fonts/han/%.ttc, $(FONT_BIN))
  FONT_BIN := $(filter-out resources/fonts/droid/DroidSansFallbackFull.ttf, $(FONT_BIN))
  FONT_BIN := $(filter-out resources/fonts/droid/DroidSansFallback.ttf, $(FONT_BIN))
endif

ifneq ($(filter -DTOFU_CJK_EXT,$(XCFLAGS)),)
  FONT_BIN := $(filter-out resources/fonts/han/%.ttc, $(FONT_BIN))
  FONT_BIN := $(filter-out resources/fonts/droid/DroidSansFallbackFull.ttf, $(FONT_BIN))
endif

ifneq ($(filter -DTOFU_CJK_LANG,$(XCFLAGS)),)
  FONT_BIN := $(filter-out resources/fonts/han/%.ttc, $(FONT_BIN))
endif

ifneq ($(filter -DTOFU,$(XCFLAGS)),)
  FONT_BIN := $(filter-out resources/fonts/noto/%.otf,$(FONT_BIN))
  FONT_BIN := $(filter-out resources/fonts/noto/%.ttf,$(FONT_BIN))
  FONT_BIN := $(filter-out resources/fonts/sil/%.cff,$(FONT_BIN))
endif

ifneq ($(filter -DTOFU_NOTO,$(XCFLAGS)),)
  FONT_BIN := $(filter-out resources/fonts/noto/%.otf,$(FONT_BIN))
  FONT_BIN := $(filter-out resources/fonts/noto/%.ttf,$(FONT_BIN))
endif

ifneq ($(filter -DTOFU_SIL,$(XCFLAGS)),)
  FONT_BIN := $(filter-out resources/fonts/sil/%.cff,$(FONT_BIN))
endif

FONT_GEN := $(FONT_BIN:%=generated/%.c)

generated/%.cff.c : %.cff $(HEXDUMP_SH) ; $(QUIET_GEN) $(MKTGTDIR) ; bash $(HEXDUMP_SH) > $@ $<
generated/%.otf.c : %.otf $(HEXDUMP_SH) ; $(QUIET_GEN) $(MKTGTDIR) ; bash $(HEXDUMP_SH) > $@ $<
generated/%.ttf.c : %.ttf $(HEXDUMP_SH) ; $(QUIET_GEN) $(MKTGTDIR) ; bash $(HEXDUMP_SH) > $@ $<
generated/%.ttc.c : %.ttc $(HEXDUMP_SH) ; $(QUIET_GEN) $(MKTGTDIR) ; bash $(HEXDUMP_SH) > $@ $<

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
	$(QUIET_GEN) xxd -i $< | \
		sed 's/unsigned/static const unsigned/' | \
		sed '1i// This is an automatically generated file. Do not edit.' \
		> $@

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

# --- Generated perfect hash source files ---

source/html/css-properties.h: source/html/css-properties.gperf
	$(QUIET_GEN) gperf > $@ $<

generate: source/html/css-properties.h

# --- Library ---

ifeq ($(shared),yes)
MUPDF_LIB = $(OUT)/libmupdf.$(SO)$(SO_VERSION)
ifeq ($(SO),dll)
MUPDF_LIB_IMPORT = $(OUT)/libmupdf_$(SO).a
LIBS_TO_INSTALL_IN_BIN = $(MUPDF_LIB)
LIBS_TO_INSTALL_IN_LIB = $(MUPDF_LIB_IMPORT)
else
LIBS_TO_INSTALL_IN_LIB = $(MUPDF_LIB)
endif
ifneq ($(USE_SYSTEM_GLUT),yes)
THIRD_GLUT_LIB = $(OUT)/libmupdf-glut.a
endif
THREAD_LIB = $(OUT)/libmupdf-threads.a
PKCS7_LIB = $(OUT)/libmupdf-pkcs7.a

$(MUPDF_LIB) : $(MUPDF_OBJ) $(THIRD_OBJ)
$(THIRD_GLUT_LIB) : $(THIRD_GLUT_OBJ)
$(THREAD_LIB) : $(THREAD_OBJ)
$(PKCS7_LIB) : $(PKCS7_OBJ)
else
MUPDF_LIB = $(OUT)/libmupdf.a
LIBS_TO_INSTALL_IN_LIB = $(MUPDF_LIB) $(THIRD_LIB)
THIRD_LIB = $(OUT)/libmupdf-third.a
ifneq ($(USE_SYSTEM_GLUT),yes)
THIRD_GLUT_LIB = $(OUT)/libmupdf-glut.a
endif
THREAD_LIB = $(OUT)/libmupdf-threads.a
PKCS7_LIB = $(OUT)/libmupdf-pkcs7.a

$(MUPDF_LIB) : $(MUPDF_OBJ)
$(THIRD_LIB) : $(THIRD_OBJ)
$(THIRD_GLUT_LIB) : $(THIRD_GLUT_OBJ)
$(THREAD_LIB) : $(THREAD_OBJ)
$(PKCS7_LIB) : $(PKCS7_OBJ)
endif

$(MUPDF_LIB) : $(MUPDF_OBJ)
$(THIRD_LIB) : $(THIRD_OBJ)
$(THREAD_LIB) : $(THREAD_OBJ)
$(PKCS7_LIB) : $(PKCS7_OBJ)

# --- Main tools and viewers ---

MUTOOL_SRC := source/tools/mutool.c
MUTOOL_SRC += source/tools/muconvert.c
MUTOOL_SRC += source/tools/mudraw.c
MUTOOL_SRC += source/tools/murun.c
MUTOOL_SRC += source/tools/mutrace.c
MUTOOL_SRC += source/tools/cmapdump.c
MUTOOL_SRC += $(sort $(wildcard source/tools/pdf*.c))
MUTOOL_OBJ := $(MUTOOL_SRC:%.c=$(OUT)/%.o)
MUTOOL_EXE := $(OUT)/mutool$(EXE)
$(MUTOOL_EXE) : $(MUTOOL_OBJ) $(MUPDF_LIB) $(THIRD_LIB) $(PKCS7_LIB) $(THREAD_LIB)
	$(LINK_CMD) $(EXE_LDFLAGS) $(THIRD_LIBS) $(THREADING_LIBS) $(LIBCRYPTO_LIBS)
TOOL_APPS += $(MUTOOL_EXE)

MURASTER_OBJ := $(OUT)/source/tools/muraster.o
MURASTER_EXE := $(OUT)/muraster$(EXE)
$(MURASTER_EXE) : $(MURASTER_OBJ) $(MUPDF_LIB) $(THIRD_LIB) $(PKCS7_LIB) $(THREAD_LIB)
	$(LINK_CMD) $(EXE_LDFLAGS) $(THIRD_LIBS) $(THREADING_LIBS) $(LIBCRYPTO_LIBS)
TOOL_APPS += $(MURASTER_EXE)

ifeq ($(HAVE_GLUT),yes)
  MUVIEW_GLUT_SRC += $(sort $(wildcard platform/gl/*.c))
  MUVIEW_GLUT_OBJ := $(MUVIEW_GLUT_SRC:%.c=$(OUT)/%.o)
ifeq ($(HAVE_WIN32),yes)
  MUVIEW_GLUT_OBJ += $(OUT)/platform/gl/gl-winres.o
endif
  MUVIEW_GLUT_EXE := $(OUT)/mupdf-gl$(EXE)
  $(MUVIEW_GLUT_EXE) : $(MUVIEW_GLUT_OBJ) $(MUPDF_LIB) $(THIRD_LIB) $(THIRD_GLUT_LIB) $(PKCS7_LIB)
	$(LINK_CMD) $(EXE_LDFLAGS) $(THIRD_LIBS) $(LIBCRYPTO_LIBS) $(WIN32_LDFLAGS) $(THIRD_GLUT_LIBS)
  VIEW_APPS += $(MUVIEW_GLUT_EXE)
endif

ifeq ($(HAVE_X11),yes)
  MUVIEW_X11_EXE := $(OUT)/mupdf-x11$(EXE)
  MUVIEW_X11_OBJ += $(OUT)/platform/x11/pdfapp.o
  MUVIEW_X11_OBJ += $(OUT)/platform/x11/x11_main.o
  MUVIEW_X11_OBJ += $(OUT)/platform/x11/x11_image.o
  $(MUVIEW_X11_EXE) : $(MUVIEW_X11_OBJ) $(MUPDF_LIB) $(THIRD_LIB) $(PKCS7_LIB)
	$(LINK_CMD) $(THIRD_LIBS) $(X11_LIBS) $(LIBCRYPTO_LIBS)
  VIEW_APPS += $(MUVIEW_X11_EXE)
endif

ifeq ($(HAVE_WIN32),yes)
  MUVIEW_WIN32_EXE := $(OUT)/mupdf-w32$(EXE)
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
  MUVIEW_X11_CURL_EXE := $(OUT)/mupdf-x11-curl$(EXE)
  MUVIEW_X11_CURL_OBJ += $(OUT)/platform/x11/curl/pdfapp.o
  MUVIEW_X11_CURL_OBJ += $(OUT)/platform/x11/curl/x11_main.o
  MUVIEW_X11_CURL_OBJ += $(OUT)/platform/x11/curl/x11_image.o
  MUVIEW_X11_CURL_OBJ += $(OUT)/platform/x11/curl/curl_stream.o
  MUVIEW_X11_CURL_OBJ += $(OUT)/platform/x11/curl/prog_stream.o
  $(MUVIEW_X11_CURL_EXE) : $(MUVIEW_X11_CURL_OBJ) $(MUPDF_LIB) $(THIRD_LIB) $(PKCS7_LIB) $(CURL_LIB)
	$(LINK_CMD) $(EXE_LDFLAGS) $(THIRD_LIBS) $(X11_LIBS) $(LIBCRYPTO_LIBS) $(CURL_LIBS) $(PTHREAD_LIBS)
  VIEW_APPS += $(MUVIEW_X11_CURL_EXE)
endif
endif
endif

# --- Generated dependencies ---

-include $(MUPDF_OBJ:%.o=%.d)
-include $(PKCS7_OBJ:%.o=%.d)
-include $(THREAD_OBJ:%.o=%.d)
-include $(THIRD_OBJ:%.o=%.d)
-include $(THIRD_GLUT_OBJ:%.o=%.d)

-include $(MUTOOL_OBJ:%.o=%.d)
-include $(MUVIEW_GLUT_OBJ:%.o=%.d)
-include $(MUVIEW_X11_OBJ:%.o=%.d)
-include $(MUVIEW_WIN32_OBJ:%.o=%.d)

-include $(MURASTER_OBJ:%.o=%.d)
-include $(MUVIEW_X11_CURL_OBJ:%.o=%.d)

# --- Examples ---

examples: $(OUT)/example $(OUT)/multi-threaded $(OUT)/storytest

$(OUT)/example: docs/examples/example.c $(MUPDF_LIB) $(THIRD_LIB)
	$(LINK_CMD) $(CFLAGS) $(THIRD_LIBS)
$(OUT)/multi-threaded: docs/examples/multi-threaded.c $(MUPDF_LIB) $(THIRD_LIB)
	$(LINK_CMD) $(CFLAGS) $(THIRD_LIBS) -lpthread
$(OUT)/storytest: docs/examples/storytest.c $(MUPDF_LIB) $(THIRD_LIB)
	$(LINK_CMD) $(CFLAGS) $(THIRD_LIBS)

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
pydir ?= $(shell python3 -c "import sysconfig; print(sysconfig.get_path('platlib'))")

third: $(THIRD_LIB)
extra-libs: $(THIRD_GLUT_LIB)
libs: $(LIBS_TO_INSTALL_IN_BIN) $(LIBS_TO_INSTALL_IN_LIB)
tools: $(TOOL_APPS)
apps: $(TOOL_APPS) $(VIEW_APPS)

install-headers:
	install -d $(DESTDIR)$(incdir)/mupdf
	install -d $(DESTDIR)$(incdir)/mupdf/fitz
	install -d $(DESTDIR)$(incdir)/mupdf/pdf
	install -m 644 include/mupdf/*.h $(DESTDIR)$(incdir)/mupdf
	install -m 644 include/mupdf/fitz/*.h $(DESTDIR)$(incdir)/mupdf/fitz
	install -m 644 include/mupdf/pdf/*.h $(DESTDIR)$(incdir)/mupdf/pdf

install-libs: libs install-headers
ifneq ($(LIBS_TO_INSTALL_IN_LIB),)
	install -d $(DESTDIR)$(libdir)
	install -m 644 $(LIBS_TO_INSTALL_IN_LIB) $(DESTDIR)$(libdir)
endif

install-apps: apps
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(LIBS_TO_INSTALL_IN_BIN) $(TOOL_APPS) $(VIEW_APPS) $(DESTDIR)$(bindir)

install-docs:
	install -d $(DESTDIR)$(mandir)/man1
	install -m 644 docs/man/*.1 $(DESTDIR)$(mandir)/man1

	install -d $(DESTDIR)$(docdir)
	install -d $(DESTDIR)$(docdir)/examples
	install -m 644 README COPYING CHANGES $(DESTDIR)$(docdir)
	install -m 644 docs/examples/* $(DESTDIR)$(docdir)/examples

install: install-libs install-apps install-docs

install-docs-html:
	python3 scripts/build-docs.py
	install -d $(DESTDIR)$(docdir)
	install -d $(DESTDIR)$(docdir)/_images
	install -d $(DESTDIR)$(docdir)/_static
	install -d $(DESTDIR)$(docdir)/_static/js
	install -d $(DESTDIR)$(docdir)/_static/css
	install -d $(DESTDIR)$(docdir)/_static/css/fonts
	install -m 644 build/docs/html/*.html $(DESTDIR)$(docdir)
	install -m 644 build/docs/html/*.inv $(DESTDIR)$(docdir)
	install -m 644 build/docs/html/*.js $(DESTDIR)$(docdir)
	install -m 644 build/docs/html/_images/* $(DESTDIR)$(docdir)/_images
	install -m 644 build/docs/html/_static/*.css $(DESTDIR)$(docdir)/_static
	install -m 644 build/docs/html/_static/*.ico $(DESTDIR)$(docdir)/_static
	install -m 644 build/docs/html/_static/*.js $(DESTDIR)$(docdir)/_static
	install -m 644 build/docs/html/_static/*.png $(DESTDIR)$(docdir)/_static
	install -m 644 build/docs/html/_static/*.svg $(DESTDIR)$(docdir)/_static
	install -m 644 build/docs/html/_static/js/* $(DESTDIR)$(docdir)/_static/js
	install -m 644 build/docs/html/_static/css/*.css $(DESTDIR)$(docdir)/_static/css
	install -m 644 build/docs/html/_static/css/fonts/* $(DESTDIR)$(docdir)/_static/css/fonts

tarball:
	bash scripts/archive.sh

# --- Clean and Default ---

WATCH_SRCS = $(shell find include source platform -type f -name '*.[ch]')
watch:
	@ inotifywait -q -e modify $(WATCH_SRCS)

watch-recompile:
	@ while ! inotifywait -q -e modify $(WATCH_SRCS) ; do time -p $(MAKE) ; done

wasm:
	$(MAKE) -C platform/wasm

java:
	$(MAKE) -C platform/java build=$(build)

java-clean:
	$(MAKE) -C platform/java build=$(build) clean

extract-test:
	$(MAKE) debug
	$(MAKE) -C thirdparty/extract mutool=../../build/debug/mutool test-mutool

TAG_HDR_FILES=$(shell git ls-files | grep -v '^\(docs\|scripts\|generated\)' | grep '\.h$$')
TAG_SRC_FILES=$(shell git ls-files | grep -v '^\(docs\|scripts\|generated\)' | grep -v '\.h$$')

tags:
	$(TAGS_CMD) --sort=no --c-kinds=+p-t $(TAG_SRC_FILES)
	$(TAGS_CMD) -a --sort=no --c-kinds=+p-t $(TAG_HDR_FILES)
	$(TAGS_CMD) -a --sort=no --c-kinds=t $(TAG_SRC_FILES) $(TAG_HDR_FILES)

find-try-return:
	@ bash scripts/find-try-return.sh

cscope.files: $(shell find include source platform -name '*.[ch]')
	@ echo $^ | tr ' ' '\n' > $@

cscope.out: cscope.files
	cscope -b

all: libs apps

clean:
	rm -rf $(OUT)
nuke:
	rm -rf build/*
	rm -rf generated/resources/fonts/droid
	rm -rf generated/resources/fonts/han
	rm -rf generated/resources/fonts/noto
	rm -rf generated/resources/fonts/sil

release:
	$(MAKE) build=release
debug:
	$(MAKE) build=debug
sanitize:
	$(MAKE) build=sanitize

shared: shared-$(build)

shared-release:
	$(MAKE) shared=yes build=release
shared-debug:
	$(MAKE) shared=yes build=debug
shared-clean:
	rm -rf build/shared-*

android: generate
	ndk-build -j8 \
		APP_BUILD_SCRIPT=platform/java/Android.mk \
		APP_PROJECT_PATH=build/android \
		APP_PLATFORM=android-16 \
		APP_OPTIM=$(build)

# --- C++, Python and C#, and system installation ---

c++: c++-$(build)
python: python-$(build)
csharp: csharp-$(build)

c++-clean:
	rm -rf platform/c++
python-clean:
	rm -rf platform/python
csharp-clean:
	rm -rf platform/csharp

# $(OUT) only contains the `shared-` infix if shared=yes and targets that
# require shared-libraries only work if shared=yes. So if this is not the case,
# we re-run ourselves with `$(MAKE) shared=yes $@`.

ifeq ($(shared),yes)

# We can build targets that require shared libraries and use $(OUT).

# Assert that $(OUT) contains `shared`.
ifeq ($(findstring shared, $(OUT)),)
$(error OUT=$(OUT) does not contain shared)
endif

# C++, Python and C# shared libraries.
c++-%: shared-%
	./scripts/mupdfwrap.py --venv -d $(OUT) -b 01
python-%: c++-%
	./scripts/mupdfwrap.py --venv -d $(OUT) -b 23
csharp-%: c++-%
	./scripts/mupdfwrap.py --venv -d $(OUT) -b --csharp 23


# Installs of C, C++, Python and C# shared libraries
#
# We only allow install of shared libraries if we are not using any libraries
# in thirdparty/.
install-shared-check:
ifneq ($(USE_SYSTEM_LIBS),yes)
	echo "install-shared-* requires that USE_SYSTEM_LIBS=yes."
	false
endif

install-shared-c: install-shared-check shared install-headers
	install -d $(DESTDIR)$(libdir)
	install -m 644 $(OUT)/libmupdf.$(SO)$(SO_VERSION) $(DESTDIR)$(libdir)/
ifneq ($(OS),OpenBSD)
	ln -s libmupdf.$(SO)$(SO_VERSION) $(DESTDIR)$(libdir)/libmupdf.$(SO)
endif

install-shared-c++: install-shared-c c++
	install -m 644 platform/c++/include/mupdf/*.h $(DESTDIR)$(incdir)/mupdf
	install -m 644 $(OUT)/libmupdfcpp.$(SO)$(SO_VERSION) $(DESTDIR)$(libdir)/
ifneq ($(OS),OpenBSD)
	ln -s libmupdfcpp.$(SO)$(SO_VERSION) $(DESTDIR)$(libdir)/libmupdfcpp.$(SO)
endif

install-shared-python: install-shared-c++ python
	install -d $(DESTDIR)$(pydir)/mupdf
	install -m 644 $(OUT)/_mupdf.$(SO) $(DESTDIR)$(pydir)/mupdf
	install -m 644 $(OUT)/mupdf.py $(DESTDIR)$(pydir)/mupdf/__init__.py

else

# $(shared) != yes. For all targets that require a shared-library build and use
# $(OUT), we need to re-run ourselves with shared=yes.
install-% c++-% python-% csharp-%:
	# Running: $(MAKE) shared=yes $@
	$(MAKE) shared=yes $@

endif

.PHONY: all clean nuke install third libs apps generate tags
.PHONY: shared shared-debug shared-clean
.PHONY: c++-% python-% csharp-%
.PHONY: c++-clean python-clean csharp-clean

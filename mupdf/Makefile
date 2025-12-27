# GNU Makefile

-include user.make

ifndef build
  build := release
endif

default: apps libs

include Makerules

ifndef OUT
  OUT := build/$(build_prefix)$(build)$(build_suffix)
endif

include Makethird

# --- Configuration ---

# Do not specify CFLAGS, LDFLAGS, LIB_LDFLAGS, EXE_LDFLAGS or LIBS on the make
# invocation line - specify XCFLAGS, XLDFLAGS, XLIB_LDFLAGS, XEXE_LDFLAGS or
# XLIBS instead. Make ignores any lines in the makefile that set a variable
# that was set on the command line.
CFLAGS += $(XCFLAGS) -Iinclude
LIBS += $(XLIBS) -lm

LDFLAGS += $(XLDFLAGS)
LIB_LDFLAGS += $(XLIB_LDFLAGS)
EXE_LDFLAGS += $(XEXE_LDFLAGS)

ifneq ($(threading),no)
  ifeq ($(HAVE_PTHREAD),yes)
    THREADING_CFLAGS := $(PTHREAD_CFLAGS) -DHAVE_PTHREAD
    THREADING_LIBS := $(PTHREAD_LIBS)
  endif
endif

VERSION_MAJOR = $(shell grep "define FZ_VERSION_MAJOR" include/mupdf/fitz/version.h | cut -d ' ' -f 3)
VERSION_MINOR = $(shell grep "define FZ_VERSION_MINOR" include/mupdf/fitz/version.h | cut -d ' ' -f 3)
VERSION_PATCH = $(shell grep "define FZ_VERSION_PATCH" include/mupdf/fitz/version.h | cut -d ' ' -f 3)

ifneq ($(OS),Darwin)
  ifneq ($(USE_SONAME),no)
    SO_VERSION_MAJOR = .$(VERSION_MINOR)
    SO_VERSION = .$(VERSION_MINOR).$(VERSION_PATCH)
  endif
endif

# workaround argument list too long errors
ifeq (4.0,$(firstword $(sort $(MAKE_VERSION) 4.0)))
  USE_ARGUMENT_FILE=yes
else
  USE_ARGUMENT_FILE=no
endif

# --- Commands ---

ifneq ($(verbose),yes)
  QUIET_AR = @ echo "    AR $@" ;
  QUIET_RANLIB = @ echo "    RANLIB $@" ;
  QUIET_CC = @ echo "    CC $@" ;
  QUIET_CXX = @ echo "    CXX $@" ;
  QUIET_GEN = @ echo "    GEN $@" ;
  QUIET_LINK = @ echo "    LINK $@" ;
  QUIET_LINK_SO = @ echo "    LINK_SO $@" ;
  QUIET_RM = @ echo "    RM $@" ;
  QUIET_TAGS = @ echo "    TAGS $@" ;
  QUIET_OBJCOPY = @ echo "    OBJCOPY $@" ;
  QUIET_SYMLINK = @ echo "    SYMLINK $@" ;
endif

MKTGTDIR = mkdir -p $(dir $@)
CC_CMD = $(QUIET_CC) $(MKTGTDIR) ; $(CC) $(CFLAGS) -MMD -MP -o $@ -c $<
CXX_CMD = $(QUIET_CXX) $(MKTGTDIR) ; $(CXX) $(CFLAGS) $(XCXXFLAGS) -MMD -MP -o $@ -c $<
ifeq ($(USE_ARGUMENT_FILE),yes)
  AR_CMD = $(QUIET_AR) $(MKTGTDIR) ; $(AR) cr $@ $(file > $@.in,$^) @$@.in
else
  AR_CMD = $(QUIET_AR) $(MKTGTDIR) ; $(AR) cr $@ $^
endif
ifdef RANLIB
  RANLIB_CMD = $(QUIET_RANLIB) $(RANLIB) $@
endif
LINK_CMD = $(QUIET_LINK) $(MKTGTDIR) ; $(CC) $(EXE_LDFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
TAGS_CMD = $(QUIET_TAGS) ctags
OBJCOPY_CMD = $(QUIET_OBJCOPY) $(MKTGTDIR) ; $(LD) -r -b binary -z noexecstack -o $@ $<
SYMLINK_CMD = $(QUIET_SYMLINK) $(MKTGTDIR) ; ln -sf

ifeq ($(shared),yes)
  ifeq ($(USE_ARGUMENT_FILE),yes)
    LINK_SO_CMD = $(QUIET_LINK_SO) $(MKTGTDIR) ; $(CC) $(LIB_LDFLAGS) $(LDFLAGS) -o $@ $(file > $@.in,$^) @$@.in
  else
    LINK_SO_CMD = $(QUIET_LINK_SO) $(MKTGTDIR) ; $(CC) $(LIB_LDFLAGS) $(LDFLAGS) -o $@ $^
  endif
  ifeq ($(OS),OpenBSD)
    # OpenBSD linker magic doesn't use soname; so fake it by using -L$(OUT) and -lmupdf.
    LINK_CMD = $(QUIET_LINK) $(MKTGTDIR) ; $(CC) $(EXE_LDFLAGS) $(LDFLAGS) -o $@ -L$(OUT) \
	$(subst $(OUT)/libmupdf.$(SO)$(SO_VERSION),-lmupdf,$^) \
	$(LIBS)
  endif
endif

# --- Rules ---

$(OUT)/%.a :
	$(AR_CMD)
	$(RANLIB_CMD)

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
  $(OUT)/source/fitz/hyphen.o : source/fitz/hyphen.c
	$(CC_CMD) $(WARNING_CFLAGS) -Wdeclaration-after-statement -DHAVE_OBJCOPY $(LIB_CFLAGS) $(THIRD_CFLAGS)
endif

$(OUT)/source/fitz/memento.o : source/fitz/memento.c
	$(CC_CMD) $(WARNING_CFLAGS) $(LIB_CFLAGS) $(THIRD_CFLAGS) -DMEMENTO_MUPDF_HACKS

$(OUT)/source/%.o : source/%.c
	$(CC_CMD) $(WARNING_CFLAGS) -Wdeclaration-after-statement $(LIB_CFLAGS) $(THIRD_CFLAGS)

$(OUT)/thirdparty/so/source/%.o : thirdparty/so/source/%.c
	$(CC_CMD) $(WARNING_CFLAGS) -Wdeclaration-after-statement $(LIB_CFLAGS) $(THIRD_CFLAGS)

$(OUT)/source/%.o : source/%.cpp
	$(CXX_CMD) $(WARNING_CFLAGS) $(LIB_CFLAGS) $(THIRD_CFLAGS)

ifeq ($(HAVE_TESSERACT),yes)
$(OUT)/source/fitz/tessocr.o : source/fitz/tessocr.cpp
	$(CXX_CMD) $(WARNING_CFLAGS) $(LIB_CFLAGS) $(THIRD_CFLAGS) $(TESSERACT_CFLAGS) $(TESSERACT_LANGFLAGS) $(LEPTONICA_CFLAGS)
endif

ifeq ($(HAVE_LEPTONICA),yes)
$(OUT)/source/fitz/leptonica-wrap.o : source/fitz/leptonica-wrap.c
	$(CC_CMD) $(WARNING_CFLAGS) $(LIB_CFLAGS) $(THIRD_CFLAGS) $(LEPTONICA_CFLAGS)
endif

ifeq ($(HAVE_ZXINGCPP),yes)
$(OUT)/source/fitz/zxingbarcode.o : source/fitz/zxingbarcode.cpp
	$(CXX_CMD) $(WARNING_CFLAGS) $(LIB_CFLAGS) $(THIRD_CFLAGS) $(ZXINGCPP_CFLAGS) $(ZXINGCPP_LANGFLAGS)
endif

$(OUT)/platform/%.o : platform/%.c
	$(CC_CMD) $(WARNING_CFLAGS)

.PRECIOUS : $(OUT)/%.o # Keep intermediates from chained rules

# --- File lists ---

THIRD_OBJ := $(THIRD_SRC:%.c=$(OUT)/%.o)
THIRD_OBJ := $(THIRD_OBJ:%.cc=$(OUT)/%.o)
THIRD_OBJ := $(THIRD_OBJ:%.cpp=$(OUT)/%.o)

THIRD_GLUT_OBJ := $(THIRD_GLUT_SRC:%.c=$(OUT)/%.o)

MUPDF_SRC := $(sort $(wildcard source/fitz/*.c))
MUPDF_SRC += $(sort $(wildcard source/fitz/*.cpp))
MUPDF_SRC += $(sort $(wildcard source/pdf/*.c))
ifneq ($(xps),no)
MUPDF_SRC += $(sort $(wildcard source/xps/*.c))
endif
ifneq ($(svg),no)
MUPDF_SRC += $(sort $(wildcard source/svg/*.c))
endif
ifneq ($(html),no)
MUPDF_SRC += $(sort $(wildcard source/html/*.c))
endif
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

ifneq ($(filter -DTOFU_CJK,$(CFLAGS)),)
  FONT_BIN := $(filter-out resources/fonts/han/%.ttc, $(FONT_BIN))
  FONT_BIN := $(filter-out resources/fonts/droid/DroidSansFallbackFull.ttf, $(FONT_BIN))
  FONT_BIN := $(filter-out resources/fonts/droid/DroidSansFallback.ttf, $(FONT_BIN))
endif

ifneq ($(filter -DTOFU_CJK_EXT,$(CFLAGS)),)
  FONT_BIN := $(filter-out resources/fonts/han/%.ttc, $(FONT_BIN))
  FONT_BIN := $(filter-out resources/fonts/droid/DroidSansFallbackFull.ttf, $(FONT_BIN))
endif

ifneq ($(filter -DTOFU_CJK_LANG,$(CFLAGS)),)
  FONT_BIN := $(filter-out resources/fonts/han/%.ttc, $(FONT_BIN))
endif

ifneq ($(filter -DTOFU,$(CFLAGS)),)
  FONT_BIN := $(filter-out resources/fonts/noto/%.otf,$(FONT_BIN))
  FONT_BIN := $(filter-out resources/fonts/noto/%.ttf,$(FONT_BIN))
  FONT_BIN := $(filter-out resources/fonts/sil/%.cff,$(FONT_BIN))
endif

ifneq ($(filter -DTOFU_NOTO,$(CFLAGS)),)
  FONT_BIN := $(filter-out resources/fonts/noto/%.otf,$(FONT_BIN))
  FONT_BIN := $(filter-out resources/fonts/noto/%.ttf,$(FONT_BIN))
endif

ifneq ($(filter -DTOFU_SIL,$(CFLAGS)),)
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

# --- Generated hyphenation patterns ---

ifeq (,$(findstring -DFZ_ENABLE_HYPHEN=0,$(CFLAGS)))
  ifeq (,$(findstring -DFZ_ENABLE_HYPHEN_ALL=0,$(CFLAGS)))
    HYPH_BIN = resources/hyphen/hyph-all.zip
  else
    HYPH_BIN = resources/hyphen/hyph-std.zip
  endif
endif

HYPH_GEN := $(HYPH_BIN:%=generated/%.c)

generated/%.zip.c : %.zip $(HEXDUMP_SH) ; $(QUIET_GEN) $(MKTGTDIR) ; bash $(HEXDUMP_SH) > $@ $<

ifeq ($(HAVE_OBJCOPY),yes)
  MUPDF_OBJ += $(HYPH_BIN:%.zip=$(OUT)/%.zip.o)
  $(OUT)/%.zip.o : %.zip ; $(OBJCOPY_CMD)
else
  MUPDF_OBJ += $(HYPH_GEN:%.c=$(OUT)/%.o)
endif

# NOTE: Run scripts/runhyphen.sh to generate new hyphenation zip files.
generate: $(HYPH_GEN)

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
generate: source/fitz/icc/ps_gray.icc.h
generate: source/fitz/icc/ps_rgb.icc.h
generate: source/fitz/icc/ps_cmyk.icc.h

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
  $(OUT)/libmupdf.$(SO)$(SO_VERSION): $(MUPDF_OBJ) $(THIRD_OBJ)
	$(LINK_SO_CMD) $(THIRD_LIBS) $(LIBCRYPTO_LIBS) $(LIBS)
  ifeq ($(OS),OpenBSD)
    # should never create symlink
    MUPDF_LIB = $(OUT)/libmupdf.$(SO)$(SO_VERSION)
  else
    MUPDF_LIB = $(OUT)/libmupdf.$(SO)
    ifneq ($(SO_VERSION),)
      # create symlink with soname if needed
      $(OUT)/libmupdf.$(SO): $(OUT)/libmupdf.$(SO)$(SO_VERSION)
	$(SYMLINK_CMD) $(notdir $<) $@
    endif
  endif
else
  MUPDF_LIB = $(OUT)/libmupdf.a
  ifneq ($(THIRD_OBJ),)
    # omit libmupdf-third if there is nothing in it
  THIRD_LIB = $(OUT)/libmupdf-third.a
  endif
  $(MUPDF_LIB) : $(MUPDF_OBJ)
  $(THIRD_LIB) : $(THIRD_OBJ)
endif

ifneq ($(USE_SYSTEM_GLUT),yes)
  THIRD_GLUT_LIB = $(OUT)/libmupdf-glut.a
  $(THIRD_GLUT_LIB) : $(THIRD_GLUT_OBJ)
endif

THREAD_LIB = $(OUT)/libmupdf-threads.a
$(THREAD_LIB) : $(THREAD_OBJ)

PKCS7_LIB = $(OUT)/libmupdf-pkcs7.a
$(PKCS7_LIB) : $(PKCS7_OBJ)

# --- Main tools and viewers ---

MUTOOL_SRC := source/tools/mutool.c
MUTOOL_SRC += source/tools/muconvert.c
MUTOOL_SRC += source/tools/mudraw.c
MUTOOL_SRC += source/tools/murun.c
MUTOOL_SRC += source/tools/mutrace.c
MUTOOL_SRC += source/tools/mugrep.c
MUTOOL_SRC += source/tools/mubar.c
MUTOOL_SRC += source/tools/cmapdump.c
MUTOOL_SRC += $(sort $(wildcard source/tools/pdf*.c))
MUTOOL_OBJ := $(MUTOOL_SRC:%.c=$(OUT)/%.o)
MUTOOL_EXE := $(OUT)/mutool
$(MUTOOL_EXE) : $(MUTOOL_OBJ) $(MUPDF_LIB) $(THIRD_LIB) $(PKCS7_LIB) $(THREAD_LIB)
	$(LINK_CMD) $(THIRD_LIBS) $(THREADING_LIBS) $(LIBCRYPTO_LIBS)
TOOL_APPS += $(MUTOOL_EXE)

MURASTER_OBJ := $(OUT)/source/tools/muraster.o
MURASTER_EXE := $(OUT)/muraster
$(MURASTER_EXE) : $(MURASTER_OBJ) $(MUPDF_LIB) $(THIRD_LIB) $(PKCS7_LIB) $(THREAD_LIB)
	$(LINK_CMD) $(THIRD_LIBS) $(THREADING_LIBS) $(LIBCRYPTO_LIBS)
EXTRA_TOOL_APPS += $(MURASTER_EXE)

ifeq ($(HAVE_GLUT),yes)
  MUVIEW_GLUT_SRC += $(sort $(wildcard platform/gl/*.c))
  MUVIEW_GLUT_OBJ := $(MUVIEW_GLUT_SRC:%.c=$(OUT)/%.o)
  MUVIEW_GLUT_EXE := $(OUT)/mupdf-gl
  $(MUVIEW_GLUT_EXE) : $(MUVIEW_GLUT_OBJ) $(MUPDF_LIB) $(THIRD_LIB) $(THIRD_GLUT_LIB) $(PKCS7_LIB)
	$(LINK_CMD) $(THIRD_LIBS) $(LIBCRYPTO_LIBS) $(THIRD_GLUT_LIBS)
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
  EXTRA_VIEW_APPS += $(MUVIEW_X11_CURL_EXE)
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

-include $(MURASTER_OBJ:%.o=%.d)
-include $(MUVIEW_X11_CURL_OBJ:%.o=%.d)

# --- Examples ---

examples: $(OUT)/example $(OUT)/multi-threaded $(OUT)/storytest $(OUT)/searchtest

$(OUT)/example: docs/examples/example.c $(MUPDF_LIB) $(THIRD_LIB)
	$(LINK_CMD) $(CFLAGS) $(THIRD_LIBS)
$(OUT)/multi-threaded: docs/examples/multi-threaded.c $(MUPDF_LIB) $(THIRD_LIB)
	$(LINK_CMD) $(CFLAGS) $(THIRD_LIBS) -lpthread
$(OUT)/storytest: docs/examples/storytest.c $(MUPDF_LIB) $(THIRD_LIB)
	$(LINK_CMD) $(CFLAGS) $(THIRD_LIBS)
$(OUT)/searchtest: docs/examples/searchtest.c $(MUPDF_LIB) $(THIRD_LIB)
	$(LINK_CMD) $(CFLAGS) $(THIRD_LIBS)

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
SO_INSTALL_MODE ?= 644

third: $(THIRD_LIB)
extra-libs: $(THIRD_GLUT_LIB)
libs: $(MUPDF_LIB) $(THIRD_LIB) $(COMMERCIAL_LIBS)
commercial-libs: $(COMMERCIAL_LIBS)
tools: $(TOOL_APPS)
apps: $(TOOL_APPS) $(VIEW_APPS)
extra-apps: $(EXTRA_TOOL_APPS) $(EXTRA_VIEW_APPS)
libmupdf-threads: $(THREAD_LIB)

install-headers:
	install -d $(DESTDIR)$(incdir)/mupdf
	install -d $(DESTDIR)$(incdir)/mupdf/fitz
	install -d $(DESTDIR)$(incdir)/mupdf/pdf
	install -m 644 include/mupdf/*.h $(DESTDIR)$(incdir)/mupdf
	install -m 644 include/mupdf/fitz/*.h $(DESTDIR)$(incdir)/mupdf/fitz
	install -m 644 include/mupdf/pdf/*.h $(DESTDIR)$(incdir)/mupdf/pdf

install-libs: libs install-headers
	install -d $(DESTDIR)$(libdir)
ifeq ($(shared),yes)
	install -m $(SO_INSTALL_MODE) $(OUT)/libmupdf.$(SO)$(SO_VERSION) $(DESTDIR)$(libdir)/libmupdf.$(SO)$(SO_VERSION)
  ifneq ($(OS),OpenBSD)
	ln -sf libmupdf.$(SO)$(SO_VERSION) $(DESTDIR)$(libdir)/libmupdf.$(SO)$(SO_VERSION_MAJOR)
	ln -sf libmupdf.$(SO)$(SO_VERSION) $(DESTDIR)$(libdir)/libmupdf.$(SO)
  endif
else
	install -m 644 $(MUPDF_LIB) $(DESTDIR)$(libdir)
	install -m 644 $(THIRD_LIB) $(DESTDIR)$(libdir)
endif

install-tools: tools
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(TOOL_APPS) $(DESTDIR)$(bindir)

install-apps: apps
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(TOOL_APPS) $(VIEW_APPS) $(DESTDIR)$(bindir)

install-extra-apps: install-apps extra-apps
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXTRA_TOOL_APPS) $(EXTRA_VIEW_APPS) $(DESTDIR)$(bindir)

install-docs:
	install -d $(DESTDIR)$(mandir)/man1
	install -m 644 docs/man/*.1 $(DESTDIR)$(mandir)/man1
	install -d $(DESTDIR)$(docdir)
	install -d $(DESTDIR)$(docdir)/examples
	install -m 644 README CHANGES $(DESTDIR)$(docdir)
	install -m 644 $(wildcard COPYING LICENSE) $(DESTDIR)$(docdir)
	install -m 644 docs/examples/* $(DESTDIR)$(docdir)/examples

install: install-libs install-apps install-docs

docs:
	bash scripts/build-docs.sh

docs-live:
	bash scripts/build-docs-live.sh

docs-markdown:
	bash scripts/build-docs-markdown.sh

docs-clean:
	rm -rf build/docs
	rm -rf build/venv-docs
	rm -rf build/.doctrees

install-docs-html: docs
	mkdir -p $(DESTDIR)$(docdir)
	cp -r build/docs/* $(DESTDIR)$(docdir)

tarball:
	bash scripts/archive.sh

# --- Clean and Default ---

WATCH_SRCS = $(shell find include source platform -type f -name '*.[ch]')
watch:
	@ inotifywait -q -e modify $(WATCH_SRCS)

watch-recompile:
	@ while ! inotifywait -q -e modify $(WATCH_SRCS) ; do time -p $(MAKE) ; done

java:
	$(MAKE) -C platform/java build=$(build)

java-clean:
	$(MAKE) -C platform/java build=$(build) clean

wasm:
	$(MAKE) -C platform/wasm

wasm-clean:
	$(MAKE) -C platform/wasm clean

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

all: libs apps extra-apps

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
#
# To disable automatic use of a venv, use `make VENV_FLAG= ...` or `VENV_FLAG=
# make ...`.
#
VENV_FLAG ?= --venv
c++-%: shared-%
	./scripts/mupdfwrap.py $(VENV_FLAG) -d $(OUT) -b 01
python-%: c++-%
	./scripts/mupdfwrap.py $(VENV_FLAG) -d $(OUT) -b 23
csharp-%: c++-%
	./scripts/mupdfwrap.py $(VENV_FLAG) -d $(OUT) -b --csharp 23

# Installs of C, C++, Python and C# shared libraries
#
# We only allow install of shared libraries if we are not using any libraries
# in thirdparty/.
install-shared-check:
ifneq ($(shared),yes)
	@ echo "install-shared-* requires that shared=yes."
	@ false
endif
ifneq ($(USE_SYSTEM_LIBS),yes)
	@ echo "install-shared-* requires that USE_SYSTEM_LIBS=yes."
	@ false
endif

install-shared-c: install-shared-check install-libs install-headers

install-shared-c++: install-shared-c c++
	install -m 644 platform/c++/include/mupdf/*.h $(DESTDIR)$(incdir)/mupdf
	install -m $(SO_INSTALL_MODE) $(OUT)/libmupdfcpp.$(SO)$(SO_VERSION) $(DESTDIR)$(libdir)/
ifneq ($(OS),OpenBSD)
	ln -sf libmupdfcpp.$(SO)$(SO_VERSION) $(DESTDIR)$(libdir)/libmupdfcpp.$(SO)
	ln -sf libmupdfcpp.$(SO)$(SO_VERSION) $(DESTDIR)$(libdir)/libmupdfcpp.$(SO)$(SO_VERSION_MAJOR)
endif

install-shared-python: install-shared-c++ python
	install -d $(DESTDIR)$(pydir)/mupdf
	install -m $(SO_INSTALL_MODE) $(OUT)/_mupdf.$(SO) $(DESTDIR)$(pydir)/mupdf
	install -m 644 $(OUT)/mupdf.py $(DESTDIR)$(pydir)/mupdf/__init__.py

else

# $(shared) != yes. For all targets that require a shared-library build and use
# $(OUT), we need to re-run ourselves with shared=yes.
install-% c++-% python-% csharp-%:
	# Running: $(MAKE) shared=yes $@
	$(MAKE) shared=yes $@

endif

.PHONY: all clean nuke install third libs apps generate tags tarball
.PHONY: docs docs-live docs-clean java java-clean wasm wasm-clean
.PHONY: watch watch-recompile
.PHONY: shared shared-debug shared-clean
.PHONY: c++-% python-% csharp-%
.PHONY: c++-clean python-clean csharp-clean

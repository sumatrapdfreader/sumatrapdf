# Makefile for Poppler library

BUILD = release

CC = gcc
CPPFLAGS =
CFLAGS =
AR = ar
INC_DIR =

-include config.mk

# TODO: precompiled headers?
# TODO: output into specific build directory depending on configuration

INC_DIR += baseutils src
INC_DIR += poppler poppler/goo
INC_DIR += poppler/poppler poppler/splash

ifeq ($(BUILD), debug)
    CPPFLAGS += -D_DEBUG -DDEBUG
    CFLAGS += -O0
else
    ifeq ($(BUILD), release)
        CPPFLAGS += -DNDEBUG
        CFLAGS += -Os -O2
    else
        $(error Build mode should be "debug" or "release")
    endif
endif

CPPFLAGS += $(addprefix -I , $(INC_DIR))
CFLAGS += -Wall
CFLAGS += -g
CPPFLAGS += -D_WIN32 -DWIN32 -D_WINDOWS
CPPFLAGS += -DUSE_OWN_GET_AUTH_DATA
CPPFLAGS += -D'POPPLER_DATADIR=""'
CPPFLAGS += -DENABLE_ZLIB=1 -DENABLE_LIBJPEG=1

# MinGW <shlobj.h> missing SHGFP_TYPE_CURRENT unless _WIN32_IE >= 0x0500
CPPFLAGS += -D_WIN32_IE=0x0500

all: libpoppler.a
.PHONY: all

# TODO: automatic header dependancies stuff

SOURCES += poppler/poppler/Annot.cc
SOURCES += poppler/poppler/Array.cc
SOURCES += poppler/poppler/BuiltinFont.cc
SOURCES += poppler/poppler/BuiltinFontTables.cc
SOURCES += poppler/poppler/Catalog.cc
SOURCES += poppler/poppler/CharCodeToUnicode.cc
SOURCES += poppler/poppler/CMap.cc
SOURCES += poppler/poppler/DCTStream.cc
SOURCES += poppler/poppler/Decrypt.cc
SOURCES += poppler/poppler/Dict.cc
SOURCES += poppler/goo/FastAlloc.cc
SOURCES += poppler/goo/FastFixedAllocator.cc
SOURCES += poppler/goo/FixedPoint.cc
SOURCES += poppler/poppler/FlateStream.cc
SOURCES += poppler/fofi/FoFiBase.cc
SOURCES += poppler/fofi/FoFiEncodings.cc
SOURCES += poppler/fofi/FoFiTrueType.cc
SOURCES += poppler/fofi/FoFiType1.cc
SOURCES += poppler/fofi/FoFiType1C.cc
SOURCES += poppler/poppler/FontEncodingTables.cc
SOURCES += poppler/poppler/Function.cc
SOURCES += poppler/goo/gfile.cc
SOURCES += poppler/poppler/Gfx.cc
SOURCES += poppler/poppler/GfxFont.cc
SOURCES += poppler/poppler/GfxState.cc
SOURCES += poppler/poppler/GlobalParams.cc
SOURCES += poppler/poppler/GlobalParamsWin.cc
SOURCES += poppler/goo/gmem.c
SOURCES += poppler/goo/gmempp.cc
SOURCES += poppler/goo/GooHash.cc
SOURCES += poppler/goo/GooList.cc
SOURCES += poppler/goo/GooString.cc
SOURCES += poppler/goo/GooTimer.cc
SOURCES += poppler/poppler/JArithmeticDecoder.cc
SOURCES += poppler/poppler/JBIG2Stream.cc
SOURCES += poppler/poppler/JPXStream.cc
SOURCES += poppler/poppler/Lexer.cc
SOURCES += poppler/poppler/Link.cc
SOURCES += poppler/poppler/NameToCharCode.cc
SOURCES += poppler/poppler/Object.cc
SOURCES += poppler/poppler/Outline.cc
SOURCES += poppler/poppler/OutputDev.cc
SOURCES += poppler/poppler/Page.cc
SOURCES += poppler/poppler/PageLabelInfo.cc
SOURCES += poppler/poppler/Parser.cc
SOURCES += poppler/poppler/PDFDoc.cc
SOURCES += poppler/poppler/PDFDocEncoding.cc
SOURCES += poppler/poppler/PSTokenizer.cc
SOURCES += poppler/poppler/SecurityHandler.cc
SOURCES += poppler/poppler/Sound.cc
SOURCES += poppler/splash/Splash.cc
SOURCES += poppler/splash/SplashBitmap.cc
SOURCES += poppler/splash/SplashClip.cc
SOURCES += poppler/splash/SplashFont.cc
SOURCES += poppler/splash/SplashFontEngine.cc
SOURCES += poppler/splash/SplashFontFile.cc
SOURCES += poppler/splash/SplashFontFileID.cc
SOURCES += poppler/splash/SplashFTFont.cc
SOURCES += poppler/splash/SplashFTFontEngine.cc
SOURCES += poppler/splash/SplashFTFontFile.cc
SOURCES += poppler/poppler/SplashOutputDev.cc
SOURCES += poppler/splash/SplashPath.cc
SOURCES += poppler/splash/SplashPattern.cc
SOURCES += poppler/splash/SplashScreen.cc
SOURCES += poppler/splash/SplashState.cc
SOURCES += poppler/splash/SplashXPath.cc
SOURCES += poppler/splash/SplashXPathScanner.cc
SOURCES += poppler/poppler/Stream.cc
SOURCES += poppler/poppler/TextOutputDev.cc
SOURCES += poppler/poppler/UGooString.cc
SOURCES += poppler/poppler/UnicodeMap.cc
SOURCES += poppler/poppler/UnicodeTypeTable.cc
SOURCES += poppler/poppler/XpdfPluginAPI.cc
SOURCES += poppler/poppler/XRef.cc

HEADERS += poppler/poppler/Annot.h
HEADERS += poppler/poppler/Array.h
HEADERS += poppler/poppler/BuiltinFont.h
HEADERS += poppler/poppler/BuiltinFontTables.h
HEADERS += poppler/poppler/Catalog.h
HEADERS += poppler/poppler/CharCodeToUnicode.h
HEADERS += poppler/poppler/CharTypes.h
HEADERS += poppler/poppler/CMap.h
HEADERS += poppler/poppler/CompactFontTables.h
HEADERS += src/config.h
HEADERS += poppler/poppler/DCTStream.h
HEADERS += poppler/poppler/Decrypt.h
HEADERS += poppler/poppler/Dict.h
HEADERS += poppler/poppler/Error.h
HEADERS += poppler/poppler/ErrorCodes.h
HEADERS += poppler/goo/FastAlloc.h
HEADERS += poppler/goo/FastFixedAllocator.h
HEADERS += poppler/goo/FixedPoint.h
HEADERS += poppler/fofi/FoFiBase.h
HEADERS += poppler/fofi/FoFiEncodings.h
HEADERS += poppler/fofi/FoFiTrueType.h
HEADERS += poppler/fofi/FoFiType1.h
HEADERS += poppler/fofi/FoFiType1C.h
HEADERS += poppler/poppler/FontEncodingTables.h
HEADERS += poppler/poppler/Function.h
HEADERS += poppler/goo/gfile.h
HEADERS += poppler/poppler/Gfx.h
HEADERS += poppler/poppler/GfxFont.h
HEADERS += poppler/poppler/GfxState.h
HEADERS += poppler/poppler/GlobalParams.h
HEADERS += poppler/goo/gmem.h
HEADERS += poppler/goo/GooHash.h
HEADERS += poppler/goo/GooList.h
HEADERS += poppler/goo/GooMutex.h
HEADERS += poppler/goo/GooString.h
HEADERS += poppler/goo/GooTimer.h
HEADERS += poppler/goo/GooVector.h
HEADERS += poppler/goo/gtypes.h
HEADERS += poppler/poppler/JArithmeticDecoder.h
HEADERS += poppler/poppler/JBIG2Stream.h
HEADERS += poppler/poppler/JPXStream.h
HEADERS += poppler/poppler/Lexer.h
HEADERS += poppler/poppler/Link.h
HEADERS += poppler/poppler/NameToCharCode.h
HEADERS += poppler/poppler/NameToUnicodeTable.h
HEADERS += poppler/poppler/Object.h
HEADERS += poppler/poppler/Outline.h
HEADERS += poppler/poppler/OutputDev.h
HEADERS += poppler/poppler/Page.h
HEADERS += poppler/poppler/PageLabelInfo.h
HEADERS += poppler/poppler/Parser.h
HEADERS += poppler/poppler/PDFDoc.h
HEADERS += poppler/poppler/PDFDocEncoding.h
HEADERS += src/poppler-config.h
HEADERS += poppler/poppler/PSTokenizer.h
HEADERS += src/Resource.h
HEADERS += poppler/poppler/SecurityHandler.h
HEADERS += poppler/poppler/Sound.h
HEADERS += poppler/splash/Splash.h
HEADERS += poppler/splash/SplashBitmap.h
HEADERS += poppler/splash/SplashClip.h
HEADERS += poppler/splash/SplashErrorCodes.h
HEADERS += poppler/splash/SplashFont.h
HEADERS += poppler/splash/SplashFontEngine.h
HEADERS += poppler/splash/SplashFontFile.h
HEADERS += poppler/splash/SplashFontFileID.h
HEADERS += poppler/splash/SplashFTFont.h
HEADERS += poppler/splash/SplashFTFontEngine.h
HEADERS += poppler/splash/SplashFTFontFile.h
HEADERS += poppler/splash/SplashGlyphBitmap.h
HEADERS += poppler/splash/SplashMath.h
HEADERS += poppler/poppler/SplashOutputDev.h
HEADERS += poppler/splash/SplashPath.h
HEADERS += poppler/splash/SplashPattern.h
HEADERS += poppler/splash/SplashScreen.h
HEADERS += poppler/splash/SplashState.h
HEADERS += poppler/splash/SplashTypes.h
HEADERS += poppler/splash/SplashXPath.h
HEADERS += poppler/splash/SplashXPathScanner.h
HEADERS += poppler/poppler/Stream-CCITT.h
HEADERS += poppler/poppler/Stream.h
HEADERS += poppler/poppler/TextOutputDev.h
HEADERS += poppler/poppler/UGooString.h
HEADERS += poppler/poppler/UnicodeCClassTables.h
HEADERS += poppler/poppler/UnicodeCompTables.h
HEADERS += poppler/poppler/UnicodeDecompTables.h
HEADERS += poppler/poppler/UnicodeMap.h
HEADERS += poppler/poppler/UnicodeMapTables.h
HEADERS += poppler/poppler/UnicodeTypeTable.h
HEADERS += poppler/poppler/UTF8.h
HEADERS += poppler/poppler/XpdfPluginAPI.h
HEADERS += poppler/poppler/XRef.h

OBJECTS = $(addsuffix .o, $(SOURCES))

.SUFFIXES:

libpoppler.a: $(OBJECTS)
	@echo AR r $@
	@$(AR) r $@ $?

$(OBJECTS): %.o: %
	@echo CC $(CPPFLAGS_$<) $(CFLAGS_$<) -c $<
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(CPPFLAGS_$<) $(CFLAGS_$<) -c $< -o $@

clean:
	@echo RM libpoppler.a OBJECTS
	@rm -f libpoppler.a $(OBJECTS)
.PHONY: clean

# Android makefile to be used with ndk-build.
#
# Run ndk-build with the following arguments:
#	APP_BUILD_SCRIPT=platform/java/Android.mk (this file)
#	APP_PROJECT_DIR=build/android (where you want the output)
#	APP_PLATFORM=android-16
#	APP_OPTIM=release (or debug)
#	APP_ABI=all (or armeabi, armeabi-v7a, arm64-v8a, x86, x86_64, mips, mips64)
#
# The top-level Makefile will invoke ndk-build with appropriate arguments
# if you run 'make android'.
#
# Use the MUPDF_EXTRA_CFLAGS, MUPDF_EXTRA_CPPFLAGS, MUPDF_EXTRA_LDFLAGS,
# and MUPDF_EXTRA_LDLIBS variables to add more compiler flags.
#
# LOCAL_C_INCLUDES paths are relative to the NDK root directory.
# LOCAL_SRC_FILES paths are relative to LOCAL_PATH.
#
# We make sure to use absolute paths everywhere, so this makefile works
# regardless of where it is called from.

LOCAL_PATH := $(call my-dir)
MUPDF_PATH := $(realpath $(LOCAL_PATH)/../..)

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
HAVE_NEON := yes
endif

ifeq ($(TARGET_ARCH_ABI),x86_64)
HAVE_AVX := yes
HAVE_AVX2 := yes
HAVE_FMA := yes
HAVE_SSE4_1 := yes
endif

include $(MUPDF_PATH)/Makelists

ifeq ($(USE_TESSERACT),yes)
ifeq ($(USE_LEPTONICA),)
USE_LEPTONICA := yes
endif
endif

# --- Build a local static library for core mupdf ---

include $(CLEAR_VARS)

LOCAL_MODULE := mupdf_core

LOCAL_C_INCLUDES := $(MUPDF_PATH)/include

LOCAL_CFLAGS += -ffunction-sections -fdata-sections
LOCAL_CFLAGS += -D_FILE_OFFSET_BITS=32
LOCAL_CFLAGS += -DTOFU_NOTO
LOCAL_CFLAGS += -DTOFU_CJK
LOCAL_CFLAGS += -DTOFU_SIL
LOCAL_CFLAGS += -DAA_BITS=8

LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(FREETYPE_CFLAGS)))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(GUMBO_CFLAGS)))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(HARFBUZZ_CFLAGS)))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(JBIG2DEC_CFLAGS)))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(LCMS2_CFLAGS)))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(LIBJPEG_CFLAGS)))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(MUJS_CFLAGS)))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(OPENJPEG_CFLAGS)))

ifdef USE_TESSERACT
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(TESSERACT_CFLAGS)))
endif
ifdef USE_LEPTONICA
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(LEPTONICA_CFLAGS)))
endif

LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(EXTRACT_CFLAGS)))

LOCAL_CFLAGS += $(filter-out -I%,$(FREETYPE_CFLAGS))
LOCAL_CFLAGS += $(filter-out -I%,$(GUMBO_CFLAGS))
LOCAL_CFLAGS += $(filter-out -I%,$(HARFBUZZ_CFLAGS))
LOCAL_CFLAGS += $(filter-out -I%,$(JBIG2DEC_CFLAGS))
LOCAL_CFLAGS += $(filter-out -I%,$(LCMS2_CFLAGS))
LOCAL_CFLAGS += $(filter-out -I%,$(LIBJPEG_CFLAGS))
LOCAL_CFLAGS += $(filter-out -I%,$(MUJS_CFLAGS))
LOCAL_CFLAGS += $(filter-out -I%,$(OPENJPEG_CFLAGS))

ifdef USE_TESSERACT
LOCAL_CFLAGS += -DHAVE_TESSERACT
LOCAL_CFLAGS += $(filter-out -I%,$(TESSERACT_CFLAGS))
endif
ifdef USE_LEPTONICA
LOCAL_CFLAGS += -DHAVE_LEPTONICA
LOCAL_CFLAGS += $(filter-out -I%,$(LEPTONICA_CFLAGS))
endif

LOCAL_CFLAGS += $(filter-out -I%,$(EXTRACT_CFLAGS))

LOCAL_SRC_FILES += $(wildcard $(MUPDF_PATH)/source/fitz/*.c)
LOCAL_SRC_FILES += $(wildcard $(MUPDF_PATH)/source/fitz/*.cpp)
LOCAL_SRC_FILES += $(wildcard $(MUPDF_PATH)/source/pdf/*.c)
LOCAL_SRC_FILES += $(wildcard $(MUPDF_PATH)/source/xps/*.c)
LOCAL_SRC_FILES += $(wildcard $(MUPDF_PATH)/source/svg/*.c)
LOCAL_SRC_FILES += $(wildcard $(MUPDF_PATH)/source/cbz/*.c)
LOCAL_SRC_FILES += $(wildcard $(MUPDF_PATH)/source/html/*.c)
LOCAL_SRC_FILES += $(wildcard $(MUPDF_PATH)/source/helpers/pkcs7/*.c)

LOCAL_SRC_FILES += $(wildcard $(MUPDF_PATH)/generated/resources/fonts/urw/*.c)

LOCAL_CFLAGS += $(MUPDF_EXTRA_CFLAGS)

include $(BUILD_STATIC_LIBRARY)

# --- Build local static libraries for thirdparty libraries ---

include $(CLEAR_VARS)
LOCAL_MODULE += mupdf_thirdparty_freetype
LOCAL_SRC_FILES += $(patsubst %,$(MUPDF_PATH)/%,$(FREETYPE_SRC))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(FREETYPE_CFLAGS) $(FREETYPE_BUILD_CFLAGS)))
LOCAL_CFLAGS += $(filter-out -I%,$(FREETYPE_CFLAGS) $(FREETYPE_BUILD_CFLAGS))
LOCAL_CFLAGS += $(MUPDF_EXTRA_CFLAGS)
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE += mupdf_thirdparty_gumbo
LOCAL_SRC_FILES += $(patsubst %,$(MUPDF_PATH)/%,$(GUMBO_SRC))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(GUMBO_CFLAGS) $(GUMBO_BUILD_CFLAGS)))
LOCAL_CFLAGS += $(filter-out -I%,$(GUMBO_CFLAGS) $(GUMBO_BUILD_CFLAGS))
LOCAL_CFLAGS += $(MUPDF_EXTRA_CFLAGS)
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE += mupdf_thirdparty_jbig2dec
LOCAL_SRC_FILES += $(patsubst %,$(MUPDF_PATH)/%,$(JBIG2DEC_SRC))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(JBIG2DEC_CFLAGS) $(JBIG2DEC_BUILD_CFLAGS)))
LOCAL_CFLAGS += $(filter-out -I%,$(JBIG2DEC_CFLAGS) $(JBIG2DEC_BUILD_CFLAGS))
LOCAL_CFLAGS += $(MUPDF_EXTRA_CFLAGS)
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE += mupdf_thirdparty_harfbuzz
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES += $(patsubst %,$(MUPDF_PATH)/%,$(HARFBUZZ_SRC))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(HARFBUZZ_CFLAGS) $(HARFBUZZ_BUILD_CFLAGS)))
LOCAL_CPPFLAGS += $(filter-out -I%,$(HARFBUZZ_CFLAGS) $(HARFBUZZ_BUILD_CFLAGS))
LOCAL_CPPFLAGS += $(MUPDF_EXTRA_CPPFLAGS)
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE += mupdf_thirdparty_lcms2
LOCAL_CFLAGS += $(MUPDF_EXTRA_CFLAGS)
LOCAL_SRC_FILES += $(patsubst %,$(MUPDF_PATH)/%,$(LCMS2_SRC))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(LCMS2_CFLAGS) $(LCMS2_BUILD_CFLAGS)))
LOCAL_CFLAGS += $(filter-out -I%,$(LCMS2_CFLAGS) $(LCMS2_BUILD_CFLAGS))
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE += mupdf_thirdparty_libjpeg
LOCAL_SRC_FILES += $(patsubst %,$(MUPDF_PATH)/%,$(LIBJPEG_SRC))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(LIBJPEG_CFLAGS) $(LIBJPEG_BUILD_CFLAGS)))
LOCAL_CFLAGS += $(filter-out -I%,$(LIBJPEG_CFLAGS) $(LIBJPEG_BUILD_CFLAGS))
LOCAL_CFLAGS += $(MUPDF_EXTRA_CFLAGS)
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE += mupdf_thirdparty_mujs
LOCAL_SRC_FILES += $(patsubst %,$(MUPDF_PATH)/%,$(MUJS_SRC))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(MUJS_CFLAGS) $(MUJS_BUILD_CFLAGS)))
LOCAL_CFLAGS += $(filter-out -I%,$(MUJS_CFLAGS) $(MUJS_BUILD_CFLAGS))
LOCAL_CFLAGS += $(MUPDF_EXTRA_CFLAGS)
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE += mupdf_thirdparty_openjpeg
LOCAL_SRC_FILES += $(patsubst %,$(MUPDF_PATH)/%,$(OPENJPEG_SRC))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(OPENJPEG_CFLAGS) $(OPENJPEG_BUILD_CFLAGS)))
LOCAL_CFLAGS += $(filter-out -I%,$(OPENJPEG_CFLAGS) $(OPENJPEG_BUILD_CFLAGS))
LOCAL_CFLAGS += $(MUPDF_EXTRA_CFLAGS)
include $(BUILD_STATIC_LIBRARY)

ifdef USE_TESSERACT
# --- Build local static library for tesseract ---

include $(CLEAR_VARS)
LOCAL_MODULE += mupdf_thirdparty_tesseract
LOCAL_C_INCLUDES := $(MUPDF_PATH)/include
LOCAL_SRC_FILES += $(patsubst %,$(MUPDF_PATH)/%,$(TESSERACT_SRC))
LOCAL_SRC_FILES += $(MUPDF_PATH)/source/fitz/tessocr.cpp
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(TESSERACT_CFLAGS) $(TESSERACT_BUILD_CFLAGS)))
LOCAL_CFLAGS += $(filter-out -I%,$(TESSERACT_CFLAGS) $(TESSERACT_BUILD_CFLAGS))
LOCAL_CFLAGS += -Wno-sign-compare
LOCAL_CFLAGS += $(MUPDF_EXTRA_CFLAGS)
LOCAL_CPP_FEATURES := exceptions
include $(BUILD_STATIC_LIBRARY)

endif

ifdef USE_LEPTONICA
# --- Build local static library for leptonica ---

include $(CLEAR_VARS)
LOCAL_MODULE += mupdf_thirdparty_leptonica
LOCAL_SRC_FILES += $(patsubst %,$(MUPDF_PATH)/%,$(LEPTONICA_SRC))
LOCAL_SRC_FILES += $(MUPDF_PATH)/source/fitz/tessocr.cpp
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(LEPTONICA_CFLAGS) $(LEPTONICA_BUILD_CFLAGS)))
LOCAL_CFLAGS += $(filter-out -I%,$(LEPTONICA_CFLAGS) $(LEPTONICA_BUILD_CFLAGS))
LOCAL_CFLAGS += -Wno-sign-compare -DANDROID_BUILD
LOCAL_CFLAGS += $(MUPDF_EXTRA_CFLAGS)
include $(BUILD_STATIC_LIBRARY)

endif  #  USE_TESSERACT

include $(CLEAR_VARS)
LOCAL_MODULE += mupdf_thirdparty_extract
LOCAL_SRC_FILES += $(patsubst %,$(MUPDF_PATH)/%,$(EXTRACT_SRC))
LOCAL_C_INCLUDES += $(patsubst -I%,$(MUPDF_PATH)/%,$(filter -I%,$(EXTRACT_CFLAGS) $(EXTRACT_BUILD_CFLAGS)))
LOCAL_CFLAGS += $(filter-out -I%,$(EXTRACT_CFLAGS) $(EXTRACT_BUILD_CFLAGS))
LOCAL_CFLAGS += $(MUPDF_EXTRA_CFLAGS)
include $(BUILD_STATIC_LIBRARY)

# --- Build the final JNI shared library ---

include $(CLEAR_VARS)

LOCAL_MODULE := mupdf_java

LOCAL_C_INCLUDES += $(MUPDF_PATH)/include

LOCAL_CFLAGS += -DHAVE_ANDROID
LOCAL_CFLAGS += $(MUPDF_EXTRA_CFLAGS)

LOCAL_SRC_FILES += $(MUPDF_PATH)/platform/java/mupdf_native.c

ifdef ADD_SOURCE_FILES
LOCAL_SRC_FILES += $(ADD_SOURCE_FILES)
LOCAL_CFLAGS += $(ADD_C_FLAGS)
LOCAL_C_INCLUDES += $(ADD_C_INCLUDES)
endif

LOCAL_STATIC_LIBRARIES += mupdf_core
LOCAL_STATIC_LIBRARIES += mupdf_thirdparty_freetype
LOCAL_STATIC_LIBRARIES += mupdf_thirdparty_gumbo
LOCAL_STATIC_LIBRARIES += mupdf_thirdparty_harfbuzz
LOCAL_STATIC_LIBRARIES += mupdf_thirdparty_jbig2dec
LOCAL_STATIC_LIBRARIES += mupdf_thirdparty_lcms2
LOCAL_STATIC_LIBRARIES += mupdf_thirdparty_libjpeg
LOCAL_STATIC_LIBRARIES += mupdf_thirdparty_mujs
LOCAL_STATIC_LIBRARIES += mupdf_thirdparty_openjpeg

ifdef USE_TESSERACT
LOCAL_STATIC_LIBRARIES += mupdf_thirdparty_leptonica
endif
ifdef USE_LEPTONICA
LOCAL_STATIC_LIBRARIES += mupdf_thirdparty_tesseract
endif

LOCAL_STATIC_LIBRARIES += mupdf_thirdparty_extract

LOCAL_LDLIBS += $(MUPDF_EXTRA_LDLIBS)
LOCAL_LDLIBS += -ljnigraphics
LOCAL_LDLIBS += -llog
LOCAL_LDLIBS += -lz
LOCAL_LDLIBS += -lm

LOCAL_LDFLAGS := -Wl,--gc-sections
LOCAL_LDFLAGS += $(MUPDF_EXTRA_LDFLAGS)

include $(BUILD_SHARED_LIBRARY)

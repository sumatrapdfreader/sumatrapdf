LOCAL_PATH := $(call my-dir)
TOP_LOCAL_PATH := $(LOCAL_PATH)

MUPDF_ROOT := ..

ifdef NDK_PROFILER
include android-ndk-profiler.mk
endif

include $(TOP_LOCAL_PATH)/Core2.mk
include $(TOP_LOCAL_PATH)/Core.mk
include $(TOP_LOCAL_PATH)/ThirdParty.mk

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
	$(MUPDF_ROOT)/draw \
	$(MUPDF_ROOT)/fitz \
	$(MUPDF_ROOT)/pdf
LOCAL_CFLAGS :=
LOCAL_MODULE    := mupdf
LOCAL_SRC_FILES := mupdf.c
LOCAL_STATIC_LIBRARIES := mupdfcore mupdfcore2 mupdfthirdparty
ifdef NDK_PROFILER
LOCAL_CFLAGS += -pg -DNDK_PROFILER
LOCAL_STATIC_LIBRARIES += andprof
else
endif

LOCAL_LDLIBS    := -lm -llog -ljnigraphics
ifdef V8_BUILD
LOCAL_LDLIBS	+= -L$(MUPDF_ROOT)/thirdparty/v8-3.9/android -lv8_base -lv8_snapshot
endif

include $(BUILD_SHARED_LIBRARY)

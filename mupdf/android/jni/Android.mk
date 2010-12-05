TOP_LOCAL_PATH := $(call my-dir)

MUPDF_ROOT := ../..

include $(TOP_LOCAL_PATH)/Core.mk
include $(TOP_LOCAL_PATH)/ThirdParty.mk

LOCAL_PATH = $(TOP_LOCAL_PATH)
include $(CLEAR_VARS)

LOCAL_CFLAGS := \
	-I$(LOCAL_PATH)/$(MY_ROOT)/draw \
	-I$(LOCAL_PATH)/$(MY_ROOT)/fitz \
	-I$(LOCAL_PATH)/$(MY_ROOT)/mupdf
LOCAL_MODULE    := mupdf
LOCAL_SRC_FILES := mupdf.c
LOCAL_STATIC_LIBRARIES := mupdfcore mupdfthirdparty

LOCAL_LDLIBS    := -lm -llog -ljnigraphics

include $(BUILD_SHARED_LIBRARY)


LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := iconv
LOCAL_CFLAGS := \
  -Wno-multichar \
  -DANDROID \
  -DLIBDIR=\"\" \
  -DBUILDING_LIBICONV \
  -DIN_LIBRARY

LOCAL_SRC_FILES := \
  ../libiconv/libcharset/lib/localcharset.c \
  ../libiconv/lib/iconv.c \
  ../libiconv/lib/relocatable.c

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/../libiconv/include \
  $(LOCAL_PATH)/../libiconv/libcharset \
  $(LOCAL_PATH)/../libiconv/lib \
  $(LOCAL_PATH)/../libiconv/libcharset/include \
  $(LOCAL_PATH)/../libiconv/srclib
 
LOCAL_EXPORT_C_INCLUDES       := $(LOCAL_PATH)/../libiconv/include
include $(BUILD_STATIC_LIBRARY)

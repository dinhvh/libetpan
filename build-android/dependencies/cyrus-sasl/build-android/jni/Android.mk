LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

src_files = \
lib/auxprop.c \
lib/canonusr.c \
lib/checkpw.c \
lib/client.c \
lib/common.c \
lib/config.c \
lib/dlopen.c \
lib/external.c \
lib/getsubopt.c \
lib/md5.c \
lib/saslutil.c \
lib/server.c \
lib/seterror.c \
lib/snprintf.c \
plugins/anonymous.c \
plugins/cram.c \
plugins/crammd5_init.c \
plugins/digestmd5.c \
plugins/digestmd5_init.c \
plugins/login.c \
plugins/login_init.c \
plugins/ntlm.c \
plugins/ntlm_init.c \
plugins/otp.c \
plugins/otp_init.c \
plugins/passdss.c \
plugins/passdss_init.c \
plugins/plain.c \
plugins/plain_init.c \
plugins/scram.c \
plugins/scram_init.c \
plugins/srp.c \
plugins/srp_init.c \
common/plugin_common.c \
common/crypto-compat.h

src_dir := $(LOCAL_PATH)/../..

LOCAL_MODULE := sasl2

ifeq ($(OPENSSL_PATH),)
$(error OPENSSL_PATH must be set)
endif

NDK_TOOLCHAIN_VERSION := clang
LOCAL_C_INCLUDES += $(src_dir) $(src_dir)/include $(src_dir)/plugins \
   $(src_dir)/build-android/include $(OPENSSL_PATH)/include
LOCAL_SRC_FILES := $(addprefix $(src_dir)/, $(src_files))

include $(BUILD_STATIC_LIBRARY)

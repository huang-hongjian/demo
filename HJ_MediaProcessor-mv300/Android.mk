LOCAL_PATH := $(call my-dir)
#
# libCMCC_MediaProcessor
#
include $(CLEAR_VARS)
#ifeq ($(PRODUCT_TARGET),shcmcc) #rongxinjing,only shcmcc build.
#$(info PRODUCT_TARGET==shcmcc build libCMCC_MediaProcessor.so)
include $(SDK_DIR)/Android.def
LOCAL_CFLAGS += -DLINUX -DANDROID -fPIC -std=c++11

ifeq ($(PLATFORM_VERSION), 4.2.2)
LOCAL_CFLAGS += -DANDROID_API_LEVEL=17
endif

ifeq ($(PLATFORM_VERSION), 4.4.2)
LOCAL_CFLAGS += -DANDROID_API_LEVEL=19
endif

LOCAL_SRC_FILES := \
    CMCC_MediaProcessorImpl.cpp \
	hjplayer_adp.cpp \

LOCAL_C_INCLUDES := $(COMMON_UNF_INCLUDE)
LOCAL_C_INCLUDES += $(COMMON_DRV_INCLUDE)
LOCAL_C_INCLUDES += $(COMMON_API_INCLUDE)
LOCAL_C_INCLUDES += $(MSP_UNF_INCLUDE)
LOCAL_C_INCLUDES += $(MSP_DRV_INCLUDE)
LOCAL_C_INCLUDES += $(MSP_API_INCLUDE)
LOCAL_C_INCLUDES += $(SAMPLE_DIR)/common
LOCAL_C_INCLUDES += $(COMPONENT_DIR)/ha_codec/include
LOCAL_C_INCLUDES += $(SDK_DIR)/pub/include/
LOCAL_C_INCLUDES += $(HISI_PLATFORM_PATH)/frameworks/hidisplaymanager/libs/
LOCAL_C_INCLUDES += $(HISI_PLATFORM_PATH)/frameworks/hidisplaymanager/hal/
LOCAL_C_INCLUDES += $(HISI_PLATFORM_PATH)/frameworks/himediaplayer/hal/
LOCAL_C_INCLUDES += $(HISI_PLATFORM_PATH)/frameworks/base/include

LOCAL_CFLAGS += $(CFG_HI_CFLAGS) $(CFG_HI_BOARD_CONFIGS)

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libbinder \
    liblog \
    libui \
    libgui \
    libdl \
    libm \
    libhi_common \
    libhi_msp \
    libhidisplayclient \
    libmedia \
    libsurfaceflinger \
    libandroid_runtime \

LOCAL_MODULE := libHJ_MediaProcessor
ALL_DEFAULT_INSTALLED_MODULES += $(LOCAL_MODULE)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
#endif

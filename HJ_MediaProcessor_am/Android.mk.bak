LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

GRALLOC_DIR := hardware/amlogic/gralloc

ifeq ($(ANDROID_MAJOR_VER), 4.2)
LOCAL_CFLAGS += -DANDROID_API_LEVEL=17
endif

#ifeq ($(ANDROID_MAJOR_VER), 4.4)
LOCAL_CFLAGS += -DANDROID_API_LEVEL=19
#endif

LOCAL_SRC_FILES := \
    CMCC_MediaProcessorImpl.cpp \
	media_processor/CMCC_Player.cpp \
	media_processor/CMCC_MediaControl.cpp \
	hjplayer_adp.cpp

	
LIBPLAYER_PATH := $(TOP)/packages/amlogic/LibPlayer
LOCAL_C_INCLUDES += \
	$(ANDDROID_PLATFORM)/frameworks/native/include \
	$(LIBPLAYER_PATH)/amplayer/player/include \
	$(LIBPLAYER_PATH)/amplayer/control/include \
	$(LIBPLAYER_PATH)/amffmpeg \
	$(LIBPLAYER_PATH)/amcodec/include \
	$(LIBPLAYER_PATH)/amadec/include \
	$(LIBPLAYER_PATH)/amavutils/include \
	$(JNI_H_INCLUDE) \
	$(LOCAL_PATH)/include \
	$(GRALLOC_DIR) \

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libbinder \
    liblog \
    libui \
    libgui \
    libdl \
    libmedia \
    libsurfaceflinger \
    libandroid_runtime \

LOCAL_STATIC_LIBRARIES += libamcodec libamadec 

LOCAL_SHARED_LIBRARIES += libamplayer libz libamavutils libamsubdec

LOCAL_MODULE := libHJ_MediaProcessor

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

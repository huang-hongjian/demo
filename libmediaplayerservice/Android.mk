LOCAL_PATH:= $(call my-dir)

#
# libmediaplayerservice
#

include $(CLEAR_VARS)

ifeq ($(PLATFORM_VERSION), 4.2.1)
LOCAL_CFLAGS += -DAndroid_JB
endif
ifeq ($(PLATFORM_VERSION), 4.4.2)
LOCAL_CFLAGS += -DAndroid_KitKate
endif
ifeq ($(PLATFORM_VERSION), 4.4)
LOCAL_CFLAGS += -DAndroid_KitKate
endif

$(warning "VERSION--")
$(warning $(PLATFORM_VERSION))
$(warning $(LOCAL_CFLAGS))

LOCAL_SRC_FILES := \
   CMCCMediaPlayer/CMCCMediaPlayerManager.cpp \

LOCAL_SRC_FILES +=               \
    ActivityManager.cpp         \
    Crypto.cpp                  \
    Drm.cpp                     \
    HDCP.cpp                    \
    MediaPlayerFactory.cpp      \
    MediaPlayerService.cpp      \
    MediaRecorderClient.cpp     \
    MetadataRetrieverClient.cpp \
    MidiFile.cpp                \
    MidiMetadataRetriever.cpp   \
    RemoteDisplay.cpp           \
    SharedLibrary.cpp           \
    StagefrightPlayer.cpp       \
    StagefrightRecorder.cpp     \
    TestPlayerStub.cpp          \
    HiMetadataRetriever.cpp		\

LOCAL_SHARED_LIBRARIES :=       \
    libbinder                   \
    libcamera_client            \
    libcutils                   \
    liblog                      \
    libdl                       \
    libgui                      \
    libmedia                    \
    libsonivox                  \
    libstagefright              \
    libstagefright_foundation   \
    libstagefright_httplive     \
    libstagefright_omx          \
    libstagefright_wfd          \
    libutils                    \
    libvorbisidec               \
    libhi_common                \
    libhi_msp                   \
    libhisysmanagerclient       \
    libCMCCPlayer \


LOCAL_STATIC_LIBRARIES :=       \
    libstagefright_nuplayer     \
    libstagefright_rtsp         \
    libcurl                     \

LOCAL_C_INCLUDES :=                                                 \
    $(call include-path-for, graphics corecg)                       \
    $(TOP)/frameworks/av/media/libstagefright/include               \
    $(TOP)/frameworks/av/media/libstagefright/rtsp                  \
    $(TOP)/frameworks/av/media/libstagefright/wifi-display          \
    $(TOP)/frameworks/native/include/media/openmax                  \
    $(TOP)/external/tremolo/Tremolo                                 \
	$(TOP)/device/hisilicon/bigfish/frameworks/hisysmanager/libs    \

HIMEDIAPLAYER_ENABLE := true
ifeq ($(HIMEDIAPLAYER_ENABLE),true)
LOCAL_CFLAGS += -DHIMEDIAPLAYER_ENABLE

ifeq ($(PRODUCT_TARGET),shcmcc)
LOCAL_CFLAGS += -DPRODUCT_STB_MOBILE
endif
LOCAL_SRC_FILES +=  \
    FMediaPlayer/FMediaPlayer.cpp \
    FMediaPlayer/FMediaPlayerManager.cpp \
LOCAL_SRC_FILES +=  \
    HiMediaPlayer/HiMediaPlayerManage.cpp \

LOCAL_SHARED_LIBRARIES +=   \
    libhiplayer_adapter     \

LOCAL_C_INCLUDES +=                                             \
    $(SDK_DIR)/source/common/include                                \
    $(SDK_DIR)/source/component/ha_codec/include                \
    $(SDK_DIR)/source/component/subtoutput/include              \
    $(SDK_DIR)/source/msp/include                               \
    $(SDK_DIR)/source/msp/api/include                           \
    $(HISI_PLATFORM_PATH)/hidolphin/component/player/include    \
    $(HISI_PLATFORM_PATH)/hidolphin/component/charset/include   \
    $(HISI_PLATFORM_PATH)/frameworks/himediaplayer/hal          \
    $(TOP)/external/freetype/include                            \
    $(TOP)/external/harfbuzz_ng/src                             \
    $(TOP)/external/libxml2/include                             \
    $(TOP)/external/icu4c/common                                \
    $(TOP)/frameworks/native/include                            \
    $(TOP)/frameworks/av/include/media                          \
    $(TOP)/frameworks/av/media/libmedia                         \
    $(TOP)/device/hal/include                                   \
    $(TOP)/device/hisilicon/bigfish/external/curl/include/     \
    $(TOP)/device/hisilicon/bigfish/hidolphin/external/curl/include/ \
    $(TOP)/hardware/libhardware/include/hardware

##  HiMediaRecoder
HIMEDIARECORDER_ENABLE := false
ifeq ($(HIMEDIARECORDER_ENABLE),true)
LOCAL_CFLAGS += -DHIMEDIARECORDER_ENABLE
LOCAL_SRC_FILES +=                  \
    HiMediaRecorder/HiMediaRecorder.cpp  \
    HiMediaRecorder/transcodesource.cpp  \

LOCAL_C_INCLUDES +=         \
         $(HISI_PLATFORM_PATH)/hidolphin/component/hitranscoder/source/include \
         $(HISI_PLATFORM_PATH)/hidolphin/component/hitranscoder/include

LOCAL_SHARED_LIBRARIES +=   \
    libhi_transcoder       \
    libhi_muxer
## end HiMediaRecoder
endif

endif

LOCAL_C_INCLUDES +=                                             \
    $(SDK_DIR)/source/common/include                                \
    $(SDK_DIR)/source/component/ha_codec/include                \
    $(SDK_DIR)/source/component/subtoutput/include              \
    $(SDK_DIR)/source/msp/include                               \
    $(SDK_DIR)/source/msp/api/include                           \
    $(HISI_PLATFORM_PATH)/hidolphin/component/player/include    \
    $(HISI_PLATFORM_PATH)/hidolphin/component/charset/include   \
    $(HISI_PLATFORM_PATH)/frameworks/himediaplayer/hal          \
    $(TOP)/external/freetype/include                            \
    $(TOP)/external/harfbuzz_ng/src                             \
    $(TOP)/external/harfbuzz_ng/src/hb-old/                     \
    $(TOP)/external/libxml2/include                             \
    $(TOP)/external/icu4c/common                                \
    $(TOP)/frameworks/native/include                            \
    $(TOP)/frameworks/av/include/media                          \
    $(TOP)/frameworks/av/media/libmedia                         \
    $(TOP)/device/hal/include                                   \
    $(TOP)/device/hisilicon/bigfish/hidolphin/external/curl/include/ \
    $(TOP)/device/hisilicon/bigfish/external/curl/include/     \
    $(TOP)/hardware/libhardware/include/hardware




LOCAL_MODULE:= libmediaplayerservice

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))

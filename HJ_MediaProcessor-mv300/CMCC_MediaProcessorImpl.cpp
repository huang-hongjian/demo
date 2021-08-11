/**
 * @brief:解码器实现
 **/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <dlfcn.h>
#include <sys/times.h>
#include <hi_mpi_win.h>

#include <assert.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef ANDROID
#define LOG_NDEBUG 0
#define LOG_NDDEBUG 0
#define LOG_TAG "CMCC_MediaProcessorImpl"

#include <utils/Log.h>
#include <utils/Vector.h>
#include <binder/Parcel.h>
#include <cutils/properties.h>
#endif

#include "hi_common.h"
#include "hi_unf_common.h"
#include "hi_unf_ecs.h"
#include "hi_unf_avplay.h"
#include "hi_unf_sound.h"
#include "hi_unf_disp.h"
#include "hi_unf_vo.h"
#include "hi_unf_demux.h"
#include "hi_mpi_demux.h"
#include "hi_video_codec.h"

#include "HiMediaDefine.h"
#include "DisplayClient.h"
#include "hidisplay.h"

#include "HA.AUDIO.AAC.decode.h"
#include "HA.AUDIO.DTSHD.decode.h"
#include "HA.AUDIO.PCM.decode.h"
#include "HA.AUDIO.MP2.decode.h"
#include "HA.AUDIO.MP3.decode.h"
#include "HA.AUDIO.WMA9STD.decode.h"
#include "HA.AUDIO.COOK.decode.h"
#include "HA.AUDIO.DRA.decode.h"
#include "HA.AUDIO.AMRNB.codec.h"
#include "HA.AUDIO.AMRWB.codec.h"
#include "HA.AUDIO.FFMPEG_DECODE.decode.h"
#include "HA.AUDIO.DOLBYPLUS.decode.h"
#include "HA.AUDIO.TRUEHDPASSTHROUGH.decode.h"
#include "HA.AUDIO.AC3PASSTHROUGH.decode.h"
#include "HA.AUDIO.BLURAYLPCM.decode.h"

#include "CMCC_MediaProcessorImpl.h"

using namespace android;

#define CMCC_MEDIA_PROCESSOR_VERSION    1
#define CMCC_MEDIA_PROCESSOR_RELEASE    ("Build Time:["__DATE__", "__TIME__"]")

#define PROP_VALUE_MAX 92

#if ANDROID_API_LEVEL == 19
#define VIDEO_SIZE_MAX_X    4096
#define VIDEO_SIZE_MAX_Y    2160
#define VIDEO_SIZE_MAX_W    4096
#define VIDEO_SIZE_MAX_H    2160
#else
#define VIDEO_SIZE_MAX_X    1920
#define VIDEO_SIZE_MAX_Y    1080
#define VIDEO_SIZE_MAX_W    1920
#define VIDEO_SIZE_MAX_H    1080
#endif

#define PLAYER_MAX_NUM                  9
#define TS_BUF_SIZE                     0x200000
#define VID_BUF_SIZE                    (16*1024*1024)
#define AUD_BUF_SIZE                    (1 *1024 * 1024)
#define INVALID_TS_PID                  0x1fff
#define ES_STREAM_PID                   0xffff
#define INVALID_PTS                     0xFFFFFFFFU
#define EOS_TIMEOUT                     1000
#define SYSTIME_MAX                     0xFFFFFFFFU
#define TIME_BASE                       90

#define SVR_DEC_ERR_RECOVER_THREHOLD 5
#define SVR_DEC_RECOVER_WIN_DURATION 1000

#define SYNC_MODE_SLOW               1
#define SYNC_MODE_TIMEWAIT           2

#define SYNC_REPORT_TIME             2000
#define SYNC_REPORT_RANGE            1000

typedef struct tagSVR_BLURREDSCREEN_DETECT_INFO_S
{
    HI_BOOL bIsStated;            /**< HI_TRUE:sent BLURREDSCREEN_START, HI_FALSE:sent BLURREDSCREEN_END,and no dec err occur*/
    HI_U32  u32TotalNum;          /**< total Frame Num from BLURREDSCREEN_START*/
    HI_U32  u32ErrNum;            /**< total Err Frame Num from BLURREDSCREEN_START*/
    HI_S64  s64RecovWinStartTime; /**< start calculate no err frame time*/
    HI_U32  u32RecovWinTotalNum;  /**< total Frame Num from s64RecovWinStartTime*/
}SVR_BLURREDSCREEN_DETECT_INFO_S;

typedef struct player_event_para
{
    HI_HANDLE hAvplay;

    //first frame
    HI_BOOL bIsFirVidFrm;

    //blurred screen
    HI_U64 u64NewFrame;
    HI_U64 u64ErrFrame;
    HI_BOOL bBlsStart;
    HI_U32 u32BlsTime;
    //

    //skip frame
    HI_BOOL bSkipFrm;
    HI_U64 u64PtsBeforeSkip;
    HI_U64 u64CountSkipFrm;
    HI_U64 u64NowPts;
    HI_U64 u64LastSkipFrm;
    //

    //sync out
    HI_BOOL bSyncTimeOut;
    HI_U64 u64SyncTimeOut;
}PLAYER_EVENT_PARA;

SVR_BLURREDSCREEN_DETECT_INFO_S blsDetInfo;


static void             *g_eventHandler;
static PLAYER_EVENT_CB  g_callBackFunc;
//static HI_BOOL          g_bBlurredscreen = HI_FALSE;
CMCC_MediaProcessor*    CMCC_MediaProcessor_Handle = HI_NULL;
static PLAYER_EVENT_PARA       g_playerEventPara[PLAYER_MAX_NUM];

Parcel*    g_Parameter = HI_NULL;

static pthread_mutex_t g_PlayerMutex = PTHREAD_MUTEX_INITIALIZER;

/*player mutex */
#define PLAYER_LOCK() \
    do \
{ \
PLAYER_LOGE("%s %d PLAYER_LOCK\n",__FUNCTION__,__LINE__);\
    pthread_mutex_lock(&g_PlayerMutex); \
} while (0) /*lint -restore */

#define PLAYER_UNLOCK() \
    do \
{ \
    pthread_mutex_unlock(&g_PlayerMutex); \
	PLAYER_LOGE("%s %d PLAYER_UNLOCK\n",__FUNCTION__,__LINE__);\
} while (0) /*lint -restore */

#define PLAYER_MEMSET(ptr, val, size) \
    do \
    { \
        if (NULL != (ptr)) \
        { \
            memset((ptr), (val), (size)); \
        } \
} while (0)

#define PLAYER_MEMCPY(ptrdst, ptrsrc, size) \
    do \
{ \
    if (NULL != (ptrdst) && NULL != (ptrsrc)) \
    { \
        memcpy((ptrdst), (ptrsrc), (size)); \
    } \
} while (0)

#define PLAYER_CHK_PRINTF(val, ret, printstr)  \
    do \
{ \
    if ((val)) \
    { \
        ALOGE("[%s:%d], %s, ret 0x%x \n", __FILE__, __LINE__, printstr, (HI_S32)ret); \
    } \
} while (0)

#define MAX_LOG_LEN           (1024)

HI_VOID IPTV_ADAPTER_PLAYER_LOG(HI_CHAR *pFuncName, HI_U32 u32LineNum, const HI_CHAR *format, ...);

#define PLAYER_LOGE(fmt...) \
do \
{\
    IPTV_ADAPTER_PLAYER_LOG((HI_CHAR *)__FUNCTION__, (HI_U32)__LINE__, fmt);\
}while(0)

#define PLAYER_LOGI(fmt...) \
do \
{\
    IPTV_ADAPTER_PLAYER_LOG((HI_CHAR *)__FUNCTION__, (HI_U32)__LINE__, fmt);\
}while(0)

HI_VOID IPTV_ADAPTER_PLAYER_LOG(HI_CHAR *pFuncName, HI_U32 u32LineNum, const HI_CHAR *format, ...)
{
    HI_CHAR     LogStr[MAX_LOG_LEN] = {0};
    va_list     args;
    HI_S32      LogLen;

    va_start(args, format);
    LogLen = vsnprintf(LogStr, MAX_LOG_LEN, format, args);
    va_end(args);
    LogStr[MAX_LOG_LEN-1] = '\0';

    ALOGE("%s[%d]:%s", pFuncName, u32LineNum, LogStr);

    return ;
}

static bool vFormat2HiType(VIDEO_FORMAT_E vformat, int *hiVformart)
{
    HI_S32     Ret = 0;

    PLAYER_LOGI("Input video codec ID : %d \n", vformat);

    if (VIDEO_FORMAT_MPEG1 == vformat)
    {
        *hiVformart = HI_UNF_VCODEC_TYPE_MPEG2;
    }
    else if (VIDEO_FORMAT_MPEG2 == vformat)
    {
        *hiVformart = HI_UNF_VCODEC_TYPE_MPEG2;
    }
    else if (VIDEO_FORMAT_MPEG4 == vformat)
    {
        *hiVformart = HI_UNF_VCODEC_TYPE_MPEG4;
    }
    else if (VIDEO_FORMAT_H264 == vformat)
    {
        *hiVformart = HI_UNF_VCODEC_TYPE_H264;
    }
    else if (VIDEO_FORMAT_H265 == vformat)
    {
        *hiVformart = HI_UNF_VCODEC_TYPE_HEVC;
    }
    else
    {
        PLAYER_LOGE("unsupport input video codec ID: %d !\n", vformat);

        *hiVformart = VIDEO_FORMAT_MAX;
        return false;
    }

    PLAYER_LOGI("Switch to Hisi video codec ID: %d \n", *hiVformart);

    return true;
}


static bool aFormat2HiType(AUDIO_FORMAT_E afmt, HA_FORMAT_E *haFormat, HI_BOOL *bIsBigEndian)
{
    HI_S32    Ret = HI_SUCCESS;

    PLAYER_LOGI("Input audio codec ID: %d \n", afmt);

    switch(afmt)
    {
        case AUDIO_FORMAT_MPEG:
            *haFormat = FORMAT_MP3;
            break;
        case AUDIO_FORMAT_AAC:
            *haFormat = FORMAT_AAC;
            break;
        case AUDIO_FORMAT_AC3:
        case AUDIO_FORMAT_AC3PLUS:
            *haFormat = FORMAT_AC3;
            break;

            break;

        default:
            PLAYER_LOGE("unsupport input audio codec ID: %d !\n", afmt);
            Ret = HI_FAILURE;
            break;
    }

    if(HI_SUCCESS == Ret)
    {
        PLAYER_LOGI("Switch to Hisi audio codec format: 0x%d \n", *haFormat);
    }

    return Ret;
}

static bool AspectCvrs2HiType(PLAYER_CONTENTMODE_E contentMode, HI_UNF_VO_ASPECT_CVRS_E *hiContentMode)
{
    HI_S32 Ret = true;

    switch (contentMode)
    {
        case PLAYER_CONTENTMODE_LETTERBOX:
            *hiContentMode = HI_UNF_VO_ASPECT_CVRS_LETTERBOX;
            break;
        case PLAYER_CONTENTMODE_FULL:
            *hiContentMode = HI_UNF_VO_ASPECT_CVRS_IGNORE;
            break;
        default:
            PLAYER_LOGE("Unsupport input contentMode: %d !\n", contentMode);
            Ret = false;
            break;
    }
    if (true == Ret)
    {
        PLAYER_LOGI("Switch to Hisi Aspect Cvrs ID: 0x%x \n", *hiContentMode);
    }

    return true;
}

static HI_U32 Player_GetSysTime(HI_VOID)
{
    HI_U32      Ticks;
    struct tms  buf;

    /* a non-NULL value is required here */
    Ticks = (HI_U32)times(&buf);

    return Ticks * 10;
}

static inline HI_S64 SVR_CurrentTime()
{
    struct timespec t;

    if (0 != clock_gettime(CLOCK_MONOTONIC ,&t))
    {
        t.tv_sec = t.tv_nsec = 0;
    }

    return (HI_S64)(t.tv_sec) * 1000LL + t.tv_nsec / 1000000LL;
}

HI_VOID updateBlsEvent(HI_U32 u32CurrTime, HI_BOOL bGetBls, HI_S32 s32IndexOfAvplay)
{
    HI_U64 u64TotalFrame = g_playerEventPara[s32IndexOfAvplay].u64NewFrame + g_playerEventPara[s32IndexOfAvplay].u64ErrFrame;
    if (u64TotalFrame  == 0)
        return ;
    HI_U32 u32ErrRate =  (HI_U32)(g_playerEventPara[s32IndexOfAvplay].u64ErrFrame * 100 / u64TotalFrame);
    if (u32ErrRate >=  SVR_DEC_ERR_RECOVER_THREHOLD && g_playerEventPara[s32IndexOfAvplay].bBlsStart == HI_FALSE)
    {
        g_playerEventPara[s32IndexOfAvplay].bBlsStart = HI_TRUE;
        g_playerEventPara[s32IndexOfAvplay].u32BlsTime = u32CurrTime;
        PLAYER_LOGE("Event: PLAYER_EVENT_BLURREDSCREEN_START\n");
        g_callBackFunc(g_eventHandler, PLAYER_EVENT_BLURREDSCREEN_START, 0, 0);
    }
    else if (g_playerEventPara[s32IndexOfAvplay].bBlsStart == HI_TRUE)
    {
        if (bGetBls == HI_TRUE)
        {
            g_playerEventPara[s32IndexOfAvplay].u32BlsTime = u32CurrTime;
        }
        else if (bGetBls == HI_FALSE && u32CurrTime - g_playerEventPara[s32IndexOfAvplay].u32BlsTime > 2000)
        {
            g_playerEventPara[s32IndexOfAvplay].bBlsStart = HI_FALSE;
            PLAYER_LOGE("Event: PLAYER_EVENT_BLURREDSCREEN_END %u\n",u32ErrRate);
            g_callBackFunc(g_eventHandler, PLAYER_EVENT_BLURREDSCREEN_END, 0, u32ErrRate);
            g_playerEventPara[s32IndexOfAvplay].u64NewFrame = 0;
            g_playerEventPara[s32IndexOfAvplay].u64ErrFrame = 0;
        }
    }
    return;
}

static HI_U32 getCurrentTime()
{
    struct timeval tv;

    if (0 == gettimeofday(&tv, NULL))
    {
        return (tv.tv_sec % 10000000 * 1000 + tv.tv_usec / 1000);
    }

    return -1;
}

HI_S32 FindIndexByhAvplay(HI_HANDLE handle)
{
    for (HI_S32 i = 0; i < PLAYER_MAX_NUM; i++)
    {
        if (g_playerEventPara[i].hAvplay == handle)
        {
            return i;
        }
    }
    return -1;
}

HI_S32 FindIdleIndex()
{
    for (HI_S32 i = 0; i < PLAYER_MAX_NUM; i++)
    {
        if (NULL == g_playerEventPara[i].hAvplay)
        {
            return i;
        }
    }
    return -1;
}

HI_S32 Player_EvnetHandler(HI_HANDLE handle, HI_UNF_AVPLAY_EVENT_E enEvent, HI_U32 para)
{
    HI_S32 s32IndexOfAvplay = -1;
    if (NULL != handle)
    {
        s32IndexOfAvplay = FindIndexByhAvplay(handle);
    }

    if (HI_NULL != g_callBackFunc && -1 != s32IndexOfAvplay)
    {
        switch (enEvent)
        {
        case HI_UNF_AVPLAY_EVENT_NEW_VID_FRAME:
        {
            //PLAYER_LOGE("Player_EventHandler HI_UNF_AVPLAY_EVENT_NEW_VID_FRAME\n");
            g_playerEventPara[s32IndexOfAvplay].u64NewFrame++;
            HI_UNF_VIDEO_FRAME_INFO_S* pstFrmInfo = (HI_UNF_VIDEO_FRAME_INFO_S*)para;
            g_playerEventPara[s32IndexOfAvplay].u64NowPts = pstFrmInfo->u32Pts;
            if (HI_NULL == pstFrmInfo)
            {
                PLAYER_LOGE("Player_EventHandler para is null\n");
            }
			
			//PLAYER_LOGE("Player_EvnetHandler g_playerEventPara s32IndexOfAvplay=%d\n",s32IndexOfAvplay);
            if (HI_FALSE == g_playerEventPara[s32IndexOfAvplay].bIsFirVidFrm)
            {
                g_playerEventPara[s32IndexOfAvplay].bIsFirVidFrm = HI_TRUE;
                PLAYER_LOGE("Event: first video frame!\n");
                g_callBackFunc(g_eventHandler, PLAYER_EVENT_FIRST_PTS, 0, 0);
            }
            updateBlsEvent(getCurrentTime(),HI_FALSE,s32IndexOfAvplay);
            break;
        }
        case HI_UNF_AVPLAY_EVENT_IFRAME_ERR:
        {
            g_playerEventPara[s32IndexOfAvplay].u64NewFrame++;
            PLAYER_LOGE("Event: I frame error!\n");
            g_callBackFunc(g_eventHandler, PLAYER_EVENT_ERROR, 0, 0);
            g_playerEventPara[s32IndexOfAvplay].u64ErrFrame++;
            updateBlsEvent(getCurrentTime(),HI_TRUE,s32IndexOfAvplay);
            break;
            }
        case HI_UNF_AVPLAY_EVENT_VID_ERR_RATIO:
        {
            g_playerEventPara[s32IndexOfAvplay].u64ErrFrame++;
            PLAYER_LOGE("HI_UNF_AVPLAY_EVENT_VID_ERR_RATIO %d\n",para);
            updateBlsEvent(getCurrentTime(),HI_TRUE,s32IndexOfAvplay);

            HI_UNF_VCODEC_ATTR_S        VdecAttr;
            HI_S32 Ret;
            Ret = HI_UNF_AVPLAY_GetAttr(handle, HI_UNF_AVPLAY_ATTR_ID_VDEC, &VdecAttr);
            if (HI_SUCCESS != Ret)
            {
                PLAYER_LOGE("HI_UNF_AVPLAY_GetAttr failed:%#x\n",Ret);
                return Ret;
            }
            if (VdecAttr.u32ErrCover <= para)
            {
                g_playerEventPara[s32IndexOfAvplay].u64CountSkipFrm++;
                PLAYER_LOGE("skip this frame\n",Ret);
                if (g_playerEventPara[s32IndexOfAvplay].bSkipFrm == HI_FALSE)
                {
                    g_playerEventPara[s32IndexOfAvplay].bSkipFrm = HI_TRUE;
                    g_playerEventPara[s32IndexOfAvplay].u64PtsBeforeSkip = g_playerEventPara[s32IndexOfAvplay].u64NowPts;
                }
            }
            else
            {
                if (g_playerEventPara[s32IndexOfAvplay].bSkipFrm == HI_TRUE)
                {
                    g_playerEventPara[s32IndexOfAvplay].bSkipFrm = HI_FALSE;
                    PLAYER_LOGE("PLAYER_EVENT_FRAMESKIP skip frame %lld, start pts %lld\n",
                        g_playerEventPara[s32IndexOfAvplay].u64CountSkipFrm, g_playerEventPara[s32IndexOfAvplay].u64PtsBeforeSkip);
                    g_callBackFunc(g_eventHandler, PLAYER_EVENT_FIRST_PTS,
                        g_playerEventPara[s32IndexOfAvplay].u64PtsBeforeSkip, g_playerEventPara[s32IndexOfAvplay].u64CountSkipFrm);
                    g_playerEventPara[s32IndexOfAvplay].u64CountSkipFrm = 0;
                }
            }
            break;
        }
        case HI_UNF_AVPLAY_EVENT_SYNC_STAT_CHANGE:
        {
            HI_UNF_SYNC_STAT_PARAM_S *pStatParam = (HI_UNF_SYNC_STAT_PARAM_S *)para;
            HI_UNF_SYNC_ATTR_S stSync;
            HI_S32 Ret;

            Ret = HI_UNF_AVPLAY_GetAttr(handle, HI_UNF_AVPLAY_ATTR_ID_SYNC, &stSync);
            if(Ret != HI_SUCCESS)
            {
                PLAYER_LOGE("Get HI_UNF_AVPLAY_ATTR_ID_SYNC faile, Ret:%#x\n", Ret);
                return Ret;
            }

            if (pStatParam->s32VidAudDiff >= SYNC_REPORT_RANGE || pStatParam->s32VidAudDiff <= (-1)*SYNC_REPORT_RANGE)
            {
                if (g_playerEventPara[s32IndexOfAvplay].bSyncTimeOut == HI_FALSE)
                {
                    g_playerEventPara[s32IndexOfAvplay].u64SyncTimeOut = getCurrentTime();
                    g_playerEventPara[s32IndexOfAvplay].bSyncTimeOut = HI_TRUE;
                    PLAYER_LOGI("PLAYER_EVENT_AUDIO_AND_VIDEO_OUTSYNC IN,diff is %d\n", pStatParam->s32VidAudDiff);
                    g_callBackFunc(g_eventHandler, PLAYER_EVENT_AUDIO_AND_VIDEO_OUTSYNC, 0, 0);
                }
                else
                {
                    if (getCurrentTime() - g_playerEventPara[s32IndexOfAvplay].u64SyncTimeOut >= SYNC_REPORT_TIME)
                    {
                        PLAYER_LOGI("PLAYER_EVENT_AUDIO_AND_VIDEO_OUTSYNC 2s ,diff is %d\n", pStatParam->s32VidAudDiff);
                        g_callBackFunc(g_eventHandler, PLAYER_EVENT_AUDIO_AND_VIDEO_OUTSYNC, 0, getCurrentTime() - g_playerEventPara[s32IndexOfAvplay].u64SyncTimeOut);
                    }
                }
            }
            else
            {
                g_playerEventPara[s32IndexOfAvplay].bSyncTimeOut = HI_FALSE;
            }

            if(((pStatParam->s32VidAudDiff >= stSync.stSyncStartRegion.s32VidPlusTime) && (stSync.enSyncRef == HI_UNF_SYNC_REF_PCR))
                || pStatParam->s32VidAudDiff <= stSync.stSyncStartRegion.s32VidNegativeTime)
            {
                if (g_playerEventPara[s32IndexOfAvplay].bSkipFrm == HI_FALSE)
                {
                    g_playerEventPara[s32IndexOfAvplay].bSkipFrm = HI_TRUE;
                }
            }
            else
            {
                if (g_playerEventPara[s32IndexOfAvplay].bSkipFrm == HI_TRUE)
                {
                    g_playerEventPara[s32IndexOfAvplay].bSkipFrm = HI_FALSE;
                    HI_UNF_AVPLAY_STATUS_INFO_S     stStatusInfo;
                    Ret = HI_UNF_AVPLAY_GetStatusInfo(handle, &stStatusInfo);
                    if (HI_SUCCESS == Ret)
                    {
                        g_playerEventPara[s32IndexOfAvplay].u64CountSkipFrm = (stStatusInfo.u32VidFrameDropCount - g_playerEventPara[s32IndexOfAvplay].u64LastSkipFrm);

                        g_playerEventPara[s32IndexOfAvplay].u64PtsBeforeSkip = stStatusInfo.stSyncStatus.u32LastVidPts;
                        PLAYER_LOGE("PLAYER_EVENT_FRAMESKIP skip frame %lld, start pts %lld\n",
                            g_playerEventPara[s32IndexOfAvplay].u64CountSkipFrm, g_playerEventPara[s32IndexOfAvplay].u64PtsBeforeSkip);
                        g_callBackFunc(g_eventHandler, PLAYER_EVENT_FIRST_PTS,
                            g_playerEventPara[s32IndexOfAvplay].u64PtsBeforeSkip, g_playerEventPara[s32IndexOfAvplay].u64CountSkipFrm);
                        g_playerEventPara[s32IndexOfAvplay].u64LastSkipFrm = stStatusInfo.u32VidFrameDropCount;
                        g_playerEventPara[s32IndexOfAvplay].u64CountSkipFrm = 0;
                    }
                }
            }

            break;
        }
        default:
            break;
        }
    }
    return HI_SUCCESS;
}

int getIntfromString8(String8 &s,const char*pre)
{
    int off;
    int val=0;
    if((off=s.find(pre,0))>=0){
        sscanf(s.string()+off+strlen(pre),"%d",&val);
    }
    return val;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_VO_Init(HI_UNF_VO_DEV_MODE_E enDevMode)
{
    HI_S32             Ret;

    Ret = HI_UNF_VO_Init(enDevMode);
    if (Ret != HI_SUCCESS)
    {
        PLAYER_LOGE("call HI_UNF_VO_Init failed.\n");
        return Ret;
    }

    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_VO_DeInit()
{
    HI_S32                Ret;

    Ret = HI_UNF_VO_DeInit();
    if (Ret != HI_SUCCESS)
    {
        PLAYER_LOGE("call HI_UNF_VO_DeInit failed.\n");
        return Ret;
    }

    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_Disp_Init()
{
#if 0
    HI_S32                      Ret;
    HI_UNF_DISP_BG_COLOR_S      BgColor;
    PLAYER_LOGE("HIADP_Disp_Init. uid=%d, gid=%d\n", getuid(), getgid());

    Ret = HI_UNF_DISP_Init();
    if (Ret != HI_SUCCESS)
    {
        PLAYER_LOGE("call HI_UNF_DISP_Init failed.\n");
        return Ret;
    }

    BgColor.u8Red = 0;
    BgColor.u8Green = 0;
    BgColor.u8Blue = 0;

    HI_UNF_DISP_SetBgColor(HI_UNF_DISPLAY1, &BgColor);

    Ret = HI_UNF_DISP_Open(HI_UNF_DISPLAY1);
    if (Ret != HI_SUCCESS)
    {
        PLAYER_LOGE("call HI_UNF_DISP_Open failed.\n");
        HI_UNF_DISP_DeInit();
        return Ret;
    }

    Ret = HI_UNF_DISP_Open(HI_UNF_DISPLAY0);
    if (Ret != HI_SUCCESS)
    {
        PLAYER_LOGE("call HI_UNF_DISP_Open SD failed.\n");
        HI_UNF_DISP_Close(HI_UNF_DISPLAY1);
        HI_UNF_DISP_DeInit();
        return Ret;
    }
#else
    HI_S32 s32Ret = HI_SUCCESS;
    DisplayClient DspClient;

    s32Ret = DspClient.GetEncFmt();
#if ANDROID_API_LEVEL == 19
    s32Ret = DspClient.SetStereoOutMode(OUTPUT_MODE_2D, 0);
#else
    s32Ret = DspClient.SetStereoOutMode(OUTPUT_MODE_2D);
#endif
#endif
    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_Disp_DeInit(HI_VOID)
{
#if 0
    HI_S32                      Ret;

    Ret = HI_UNF_DISP_Close(HI_UNF_DISPLAY0);
    if (Ret != HI_SUCCESS)
    {
        PLAYER_LOGE("call HI_UNF_DISP_Close failed.\n");
        return Ret;
    }

    Ret = HI_UNF_DISP_Close(HI_UNF_DISPLAY1);
    if (Ret != HI_SUCCESS)
    {
        PLAYER_LOGE("call HI_UNF_DISP_Close failed.\n");
        return Ret;
    }

    Ret = HI_UNF_DISP_DeInit();
    if (Ret != HI_SUCCESS)
    {
        PLAYER_LOGE("call HI_UNF_DISP_DeInit failed.\n");
        return Ret;
    }
#endif
    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_Snd_Init(HI_VOID)
{
    HI_S32                  Ret;
    HI_UNF_SND_ATTR_S       stSndAttr;

    Ret = HI_UNF_SND_Init();
    if (Ret != HI_SUCCESS)
    {
        PLAYER_LOGE("call HI_UNF_SND_Init failed.\n");
        return Ret;
    }

    Ret = HI_UNF_SND_GetDefaultOpenAttr(HI_UNF_SND_0, &stSndAttr);
    if (Ret != HI_SUCCESS)
    {
        PLAYER_LOGE("call HI_UNF_SND_GetOpenAttr failed.");
        return Ret;
    }

    stSndAttr.u32SlaveOutputBufSize = 1024 * 64; /* in order to increase the reaction of stop/start, the buf cannot too big*/

    Ret = HI_UNF_SND_Open(HI_UNF_SND_0, &stSndAttr);
    if (Ret != HI_SUCCESS)
    {
        PLAYER_LOGE("call HI_UNF_SND_Open failed.\n");
        return Ret;
    }
    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_Snd_DeInit(HI_VOID)
{
    HI_S32                  Ret;

    Ret = HI_UNF_SND_Close(HI_UNF_SND_0);
    if (Ret != HI_SUCCESS )
    {
        PLAYER_LOGE("call HI_UNF_SND_Close failed.\n");
        return Ret;
    }

    Ret = HI_UNF_SND_DeInit();
    if (Ret != HI_SUCCESS )
    {
        PLAYER_LOGE("call HI_UNF_SND_DeInit failed.\n");
        return Ret;
    }

    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_DMX_AttachTSPort(HI_VOID)
{
    HI_S32                  Ret;
    HI_U32                  i;
    HI_UNF_DMX_PORT_E       DmxPort = HI_UNF_DMX_PORT_RAM_0;
    HI_UNF_DMX_CAPABILITY_S stCap;

    bIsDmxReady = HI_FALSE;

    Ret = HI_UNF_DMX_GetCapability(&stCap);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HI_UNF_DMX_GetCapability fail, Ret=0x%x\n", Ret);
        return HI_FAILURE;
    }

    if ((stCap.u32RamPortNum == 0) || (stCap.u32RamPortNum > 8))
    {
        PLAYER_LOGE("Dmx ram port num err, %d\n", stCap.u32RamPortNum);
        return HI_FAILURE;
    }

    for (i = 0; i < stCap.u32RamPortNum; i++)
    {
        DmxPort = (HI_UNF_DMX_PORT_E)(i + HI_UNF_DMX_PORT_RAM_0);

        Ret = HI_UNF_DMX_CreateTSBuffer(DmxPort, TS_BUF_SIZE, &hTsBuf);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGI("DMX_PORT_RAM_%d is busy, Ret=0x%x\n", i, Ret);
        }
        else
        {
            PLAYER_LOGI("DMX_PORT_RAM_%d is idle\n", i);
            break;
        }
    }

    if (i == stCap.u32RamPortNum)
    {
        PLAYER_LOGE("Not Found idle DMX_PORT_RAM!!!\n");
        return HI_FAILURE;
    }

    switch (DmxPort)
    {
        case HI_UNF_DMX_PORT_RAM_0:
        case HI_UNF_DMX_PORT_RAM_1:
        case HI_UNF_DMX_PORT_RAM_2:
        case HI_UNF_DMX_PORT_RAM_3:
        case HI_UNF_DMX_PORT_RAM_4:
        case HI_UNF_DMX_PORT_RAM_5:
        case HI_UNF_DMX_PORT_RAM_6:
        case HI_UNF_DMX_PORT_RAM_7:
            hDmxId = DmxPort - HI_UNF_DMX_PORT_RAM_0;
            PLAYER_LOGE("hDmxID = %d\n", hDmxId);
            break;
        default:
            PLAYER_LOGE("Not Found idle hDmxId!!!\n");
            Ret = HI_UNF_DMX_DestroyTSBuffer(hTsBuf);
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_UNF_DMX_DestroyTSBuffer failed");
            return HI_FAILURE;
    }

    if (hDmxId > stCap.u32DmxNum)
    {
        PLAYER_LOGE("Dmx %d id err, total=%d\n", hDmxId, stCap.u32DmxNum);
        Ret = HI_UNF_DMX_DestroyTSBuffer(hTsBuf);
        PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_UNF_DMX_DestroyTSBuffer failed");
        return HI_FAILURE;
    }

    Ret = HI_UNF_DMX_AttachTSPort(hDmxId, DmxPort);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HI_UNF_DMX_AttachTSPort fail.\n");
        Ret = HI_UNF_DMX_DestroyTSBuffer(hTsBuf);
        PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_UNF_DMX_DestroyTSBuffer failed");
        return HI_FAILURE;
    }

    bIsDmxReady = HI_TRUE;
    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_DMX_DetachTSPort(HI_VOID)
{
    HI_S32                      Ret;

    Ret = HI_UNF_DMX_DetachTSPort(hDmxId);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_DMX_DetachTSPort failed, Ret:%#x\n", Ret);
        return HI_FAILURE;
    }

    Ret = HI_UNF_DMX_DestroyTSBuffer(hTsBuf);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_DMX_DestroyTSBuffer failed, Ret:%#x\n", Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_AVPLAY_SetVdecAttr(HI_HANDLE hAvplay,HI_UNF_VCODEC_TYPE_E enType,HI_UNF_VCODEC_MODE_E enMode)
{
    HI_S32                      Ret;
    HI_UNF_VCODEC_ATTR_S        VdecAttr;

    Ret = HI_UNF_AVPLAY_GetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, &VdecAttr);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("HI_UNF_AVPLAY_GetAttr failed:%#x\n",Ret);
        return Ret;
    }

    VdecAttr.enType = enType;
    VdecAttr.enMode = enMode;
    VdecAttr.u32ErrCover = 0;//http 0 udp 100
    VdecAttr.u32Priority = 3;

    Ret = HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, &VdecAttr);
    if (Ret != HI_SUCCESS)
    {
        PLAYER_LOGE("call HI_UNF_AVPLAY_SetAttr failed.\n");
        return Ret;
    }

    return Ret;
}

HI_VOID CMCC_MediaProcessorImpl::HIADP_AVPLAY_GetAdecAttr(HI_UNF_ACODEC_ATTR_S *pstAdecAttr, HI_U8  *pu8Extradata, HI_BOOL bIsBigEndian)
{
    DOLBYPLUS_DECODE_OPENCONFIG_S stDolbyDecConfig;
    DTSHD_DECODE_OPENCONFIG_S stDtsHdDecConfig;
    HA_BLURAYLPCM_DECODE_OPENCONFIG_S stLpcmDecConfig;
    WAV_FORMAT_S stWavFormat;

    switch (pstAdecAttr->enType)
    {
        case HA_AUDIO_ID_PCM:
        {
            /* set pcm wav format here base on pcm file */
            stWavFormat.nChannels = 2;
            stWavFormat.nSamplesPerSec = 48000;
            stWavFormat.wBitsPerSample = 16;

            if(HI_TRUE == bIsBigEndian)
            {
            stWavFormat.cbSize =4;
            stWavFormat.cbExtWord[0] = 1;
            }

            HA_PCM_DecGetDefalutOpenParam(&(pstAdecAttr->stDecodeParam),&stWavFormat);

            PLAYER_LOGI("(nChannels = 2, wBitsPerSample = 16, nSamplesPerSec = 48000, isBigEndian = %d)", bIsBigEndian);
        }
            break;
        case HA_AUDIO_ID_DOLBY_PLUS:
        {
            DOLBYPLUS_DECODE_OPENCONFIG_S *pstConfig = (DOLBYPLUS_DECODE_OPENCONFIG_S *)u8DecOpenBuf;
            HA_DOLBYPLUS_DecGetDefalutOpenConfig(pstConfig);
            HA_DOLBYPLUS_DecGetDefalutOpenParam(&(pstAdecAttr->stDecodeParam), pstConfig);
            break;
        }

        case HA_AUDIO_ID_MP2:
            HA_MP2_DecGetDefalutOpenParam(&(pstAdecAttr->stDecodeParam));
            break;

        case HA_AUDIO_ID_AAC:
            HA_AAC_DecGetDefalutOpenParam(&(pstAdecAttr->stDecodeParam));
            break;

        case HA_AUDIO_ID_MP3:
            HA_MP3_DecGetDefalutOpenParam(&(pstAdecAttr->stDecodeParam));
            break;

        case HA_AUDIO_ID_AMRNB:
            if (NULL != (HI_U32*)pu8Extradata)
            {
                HA_AMRNB_GetDecDefalutOpenParam(&(pstAdecAttr->stDecodeParam),
                *((HI_U32*)pu8Extradata));
            }
            break;

        case HA_AUDIO_ID_AMRWB:
            if (NULL != (HI_U32*)pu8Extradata)
            {
                HA_AMRWB_GetDecDefalutOpenParam(&(pstAdecAttr->stDecodeParam),
                *((HI_U32*)pu8Extradata));
            }
            break;

        case HA_AUDIO_ID_WMA9STD:
            if (NULL != (HI_U32*)pu8Extradata)
            {
                HA_WMA_DecGetDefalutOpenParam(&(pstAdecAttr->stDecodeParam),
                *((HI_U32*)pu8Extradata));
            }
            break;

        case HA_AUDIO_ID_COOK:
            if (NULL != (HI_U32*)pu8Extradata)
            {
                HA_COOK_DecGetDefalutOpenParam(&(pstAdecAttr->stDecodeParam),
                *((HI_U32*)pu8Extradata));
            }
            break;

        case HA_AUDIO_ID_DTSHD:
        {
            DTSHD_DECODE_OPENCONFIG_S *pstConfig = (DTSHD_DECODE_OPENCONFIG_S *)u8DecOpenBuf;
            HA_DTSHD_DecGetDefalutOpenConfig(pstConfig);
            HA_DTSHD_DecGetDefalutOpenParam(&(pstAdecAttr->stDecodeParam), pstConfig);
            break;
        }

        case HA_AUDIO_ID_BLYRAYLPCM:
        {
            HA_BLURAYLPCM_DECODE_OPENCONFIG_S *pstConfig = (HA_BLURAYLPCM_DECODE_OPENCONFIG_S *)u8DecOpenBuf;
            HA_BLYRAYLPCM_DecGetDefalutOpenConfig(pstConfig);
            HA_BLYRAYLPCM_DecGetDefalutOpenParam(&(pstAdecAttr->stDecodeParam), pstConfig);
            break;
        }

/*
        case HA_AUDIO_ID_FFMPEG_DECODE:
        stAdecAttr.enFmt = HI_UNF_ADEC_STRAM_FMT_PACKET;
        pstMember->eAudStreamMode = HI_UNF_ADEC_STRAM_FMT_PACKET;

        if (NULL != pu8Extradata && NULL != (HI_U32*)pu8Extradata + 1)
        {
        HA_FFMPEGC_DecGetDefalutOpenParam(&(stAdecAttr.stDecodeParam),
        (HI_U32*)(*((HI_U32*)pu8Extradata + 1)));
        }
        break;
*/
        case HA_AUDIO_ID_TRUEHD:
            HA_TRUEHD_DecGetDefalutOpenParam(&(pstAdecAttr->stDecodeParam));
            pstAdecAttr->stDecodeParam.enDecMode = HD_DEC_MODE_THRU;          /* truehd iust support pass-through */
            ALOGD(" TrueHD decoder(HBR Pass-through only).\n");
            break;

        case HA_AUDIO_ID_AC3PASSTHROUGH:
            HA_AC3PASSTHROUGH_DecGetDefalutOpenParam(&(pstAdecAttr->stDecodeParam));
            pstAdecAttr->stDecodeParam.enDecMode = HD_DEC_MODE_THRU;          /* truehd iust support pass-through */
            ALOGD(" AC3 decoder(Pass-through only).\n");
            break;

        default:
            HA_DRA_DecGetDefalutOpenParam(&(pstAdecAttr->stDecodeParam));
            break;
    }

    return;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_AVPLAY_RegADecLib()
{
    HI_S32          Ret = HI_SUCCESS;

    Ret = HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.AMRWB.codec.so");
    Ret |= HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.MP3.decode.so");
    Ret |= HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.MP2.decode.so");
    Ret |= HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.AAC.decode.so");
    Ret |= HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.DOLBYTRUEHD.decode.so");
    Ret |= HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.DRA.decode.so");
    Ret |= HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.TRUEHDPASSTHROUGH.decode.so");
    Ret |= HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.AMRNB.codec.so");
    Ret |= HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.WMA.decode.so");
    Ret |= HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.COOK.decode.so");
    Ret |= HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.DOLBYPLUS.decode.so");
    Ret |= HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.DTSHD.decode.so");
    Ret |= HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.DTSM6.decode.so");
    Ret |= HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.DTSPASSTHROUGH.decode.so");
    Ret |= HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.AC3PASSTHROUGH.decode.so");
    Ret |= HI_UNF_AVPLAY_RegisterAcodecLib("libHA.AUDIO.PCM.decode.so");

    if (Ret != HI_SUCCESS)
    {
        PLAYER_LOGI("\n\n!!! some audio codec NOT found. you may NOT able to decode some audio type.\n\n");
    }

    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_AVPLAY_RegEvent()
{
    HI_S32          Ret = HI_SUCCESS;

    Ret  = HI_UNF_AVPLAY_RegisterEvent(hAvplay, HI_UNF_AVPLAY_EVENT_NEW_VID_FRAME, Player_EvnetHandler);
    Ret |= HI_UNF_AVPLAY_RegisterEvent(hAvplay, HI_UNF_AVPLAY_EVENT_IFRAME_ERR, Player_EvnetHandler);
    Ret |= HI_UNF_AVPLAY_RegisterEvent(hAvplay, HI_UNF_AVPLAY_EVENT_VID_ERR_RATIO, Player_EvnetHandler);
    Ret |= HI_UNF_AVPLAY_RegisterEvent(hAvplay, HI_UNF_AVPLAY_EVENT_SYNC_STAT_CHANGE, Player_EvnetHandler);

    HI_S32 s32IndexOfAvplay = FindIdleIndex();
    if (-1 != s32IndexOfAvplay)
    {
        memset(&g_playerEventPara[s32IndexOfAvplay], 0, sizeof(PLAYER_EVENT_PARA));
        g_playerEventPara[s32IndexOfAvplay].hAvplay = hAvplay;
		PLAYER_LOGE("g_playerEventPara s32IndexOfAvplay=%d\n",s32IndexOfAvplay);
        return Ret;
    }
    else
    {
        PLAYER_LOGE("Too many players\n");
        return HI_FAILURE;
    }
    return Ret;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_AVPLAY_UnRegEvent()
{
    HI_S32          Ret = HI_SUCCESS;

    Ret = HI_UNF_AVPLAY_UnRegisterEvent(hAvplay, HI_UNF_AVPLAY_EVENT_NEW_VID_FRAME);
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_UNF_AVPLAY_UnRegisterEvent New Frm failed");
    Ret = HI_UNF_AVPLAY_UnRegisterEvent(hAvplay, HI_UNF_AVPLAY_EVENT_IFRAME_ERR);
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_UNF_AVPLAY_UnRegisterEvent Err Frm failed");
    Ret = HI_UNF_AVPLAY_UnRegisterEvent(hAvplay, HI_UNF_AVPLAY_EVENT_VID_ERR_RATIO);
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_UNF_AVPLAY_EVENT_VID_ERR_RATIO  failed");
    Ret = HI_UNF_AVPLAY_UnRegisterEvent(hAvplay, HI_UNF_AVPLAY_EVENT_SYNC_STAT_CHANGE);
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_UNF_AVPLAY_EVENT_SYNC_STAT_CHANGE  failed");

    if (NULL != hAvplay)
    {
        HI_S32 s32IndexOfAvplay = FindIndexByhAvplay(hAvplay);
        if (-1 != s32IndexOfAvplay)
        {
            memset(&g_playerEventPara[s32IndexOfAvplay], 0, sizeof(PLAYER_EVENT_PARA));
            return Ret;
        }
    }

    return Ret;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_AVPLAY_Create(HI_PLAYER_STREAM_TYPE_E eStreamType)
{
    HI_S32          Ret = HI_SUCCESS;

    if (HI_PLAYER_STREAM_TYPE_TS == eStreamType)
    {
        PLAYER_LOGE("Create TsPlayer\n");

        Ret = HIADP_AVPLAY_RegADecLib();
        if (Ret != HI_SUCCESS)
        {
            PLAYER_LOGE("call HIADP_AVPLAY_RegADecLib fail, %d\n", Ret);
            return HI_FAILURE;
        }

        Ret = HIADP_DMX_AttachTSPort();
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HIADP_DMX_AttachTSPort fail, %d\n", Ret);
            return HI_FAILURE;
        }

        Ret = HI_UNF_AVPLAY_GetDefaultConfig(&AvplayAttr, HI_UNF_AVPLAY_STREAM_TYPE_TS);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_AVPLAY_GetDefaultConfig fail, %d\n", Ret);
            goto ERR_DMX_DETACH_PORT;
        }

        AvplayAttr.u32DemuxId = hDmxId;
        AvplayAttr.stStreamAttr.u32VidBufSize = VID_BUF_SIZE;
        AvplayAttr.stStreamAttr.u32AudBufSize = AUD_BUF_SIZE;
        Ret = HI_UNF_AVPLAY_Create(&AvplayAttr, &hAvplay);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_AVPLAY_Create fail, %d\n", Ret);
            goto ERR_DMX_DETACH_PORT;
        }

        Ret = HIADP_AVPLAY_RegEvent();
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HIADP_AVPLAY_RegEvent fail, %d\n", Ret);
            goto ERR_DMX_DETACH_PORT;
        }

        Ret = HI_UNF_AVPLAY_ChnOpen(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID, HI_NULL);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("VID: call HI_UNF_AVPLAY_ChnOpen fail, %d  PLAYER_EVENT_BLANK_SCREEN\n", Ret);
            g_callBackFunc(g_eventHandler, PLAYER_EVENT_BLANK_SCREEN, 0, 0);
            goto ERR_AVPLAY_DESTROY;
        }

        bIsVidChnReady = HI_TRUE;

        Ret = HI_UNF_AVPLAY_ChnOpen(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD, HI_NULL);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("AUD: call HI_UNF_AVPLAY_ChnOpen fail, %d\n", Ret);
            goto ERR_VID_CHN_CLOSE;
        }

        bIsAudChnReady = HI_TRUE;

        Ret = HI_UNF_SND_SetTrackMode(HI_UNF_SND_0, HI_UNF_SND_OUTPUTPORT_ALL, HI_UNF_TRACK_MODE_STEREO);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_SND_SetTrackMode fail, %d\n", Ret);
            goto ERR_AUD_CHN_CLOSE;
        }

        Ret = HI_UNF_AVPLAY_GetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_SYNC, &SyncAttr);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_AVPLAY_GetAttr fail, %d\n", Ret);
            goto ERR_AUD_CHN_CLOSE;
        }

        SyncAttr.enSyncRef = HI_UNF_SYNC_REF_AUDIO;
        SyncAttr.bQuickOutput = HI_TRUE;
        SyncAttr.u32PreSyncTimeoutMs = 1000;

        Ret = HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_SYNC, &SyncAttr);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_AVPLAY_SetAttr fail, %d\n", Ret);
            goto ERR_AUD_CHN_CLOSE;
        }

        Ret = HI_UNF_DMX_ResetTSBuffer(hTsBuf);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_DMX_ResetTSBuffer fail, %d\n", Ret);
            goto ERR_AUD_CHN_CLOSE;
        }

        bIsTsPlayReady = HI_TRUE;

        PLAYER_LOGI("MediaDevice Create TsPlayer Success!\n");
        return HI_SUCCESS;

ERR_AUD_CHN_CLOSE:
    Ret = HI_UNF_AVPLAY_ChnClose(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD);
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_UNF_AVPLAY_ChnClose AUD failed");
ERR_VID_CHN_CLOSE:
    Ret = HI_UNF_AVPLAY_ChnClose(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID);
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_UNF_AVPLAY_ChnClose VID failed");
ERR_AVPLAY_DESTROY:
    Ret = HI_UNF_AVPLAY_Destroy(hAvplay);
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_UNF_AVPLAY_Destroy failed");
ERR_DMX_DETACH_PORT:
    Ret = HIADP_DMX_DetachTSPort();
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HIADP_DMX_DetachTSPort failed");

        return HI_FAILURE;
    }
    else if (HI_PLAYER_STREAM_TYPE_ES == eStreamType)
    {
        PLAYER_LOGE("Create EsPlayer\n");

        bIsDmxReady = HI_FALSE;

        Ret = HI_UNF_AVPLAY_GetDefaultConfig(&AvplayAttr, HI_UNF_AVPLAY_STREAM_TYPE_ES);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_AVPLAY_GetDefaultConfig fail, %d\n", Ret);
            return HI_FAILURE;
        }

        Ret = HI_UNF_AVPLAY_Create(&AvplayAttr, &hAvplay);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_AVPLAY_Create fail, %d\n", Ret);
            return HI_FAILURE;
        }

        Ret = HIADP_AVPLAY_RegEvent();
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HIADP_AVPLAY_RegEvent fail, %d\n", Ret);
            goto ERR_AVPLAY_DESTROY_ES;
        }

        Ret = HI_UNF_AVPLAY_GetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_SYNC, &SyncAttr);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_AVPLAY_GetAttr fail, %d\n", Ret);
            goto ERR_AVPLAY_DESTROY_ES;
        }

        SyncAttr.enSyncRef = HI_UNF_SYNC_REF_AUDIO;
        SyncAttr.u32PreSyncTimeoutMs = 1000;

        Ret = HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_SYNC, &SyncAttr);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_AVPLAY_SetAttr fail, %d\n", Ret);
            goto ERR_AVPLAY_DESTROY_ES;
        }

        bIsEsPlayReady = HI_TRUE;

        PLAYER_LOGI("MediaDevice Create EsPlayer Success!\n");
        return HI_SUCCESS;

ERR_AVPLAY_DESTROY_ES:
    Ret = HI_UNF_AVPLAY_Destroy(hAvplay);
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_UNF_AVPLAY_Destroy failed");

        return HI_FAILURE;
    }

    return HI_FAILURE;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_AVPLAY_Destroy()
{
    HI_S32  Ret;

    PLAYER_LOGE("bIsVidChnReady: %d, bIsAudChnReady: %d\n", bIsVidChnReady, bIsAudChnReady);

    if (HI_TRUE == bIsWindowReady)
    {
        Ret = HIADP_AVPLAY_DetachWindow();
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HIADP_AVPLAY_DetachWindow fail, %d\n", Ret);
            return HI_FAILURE;
        }
    }

    if (HI_TRUE == bIsVidChnReady)
    {
        Ret = HIADP_AVPLAY_ChnClose(HI_PLAYER_MEDIA_CHAN_VID);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_AVPLAY_ChnClose fail, %d\n", Ret);
            return HI_FAILURE;
        }
    }

    if (HI_TRUE == bIsTrackReady)
    {
        Ret = HIADP_AVPLAY_DetachSound();
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HIADP_AVPLAY_DetachSound fail, %d\n", Ret);
            return HI_FAILURE;
        }
    }

    if (HI_TRUE == bIsAudChnReady)
    {
        Ret = HIADP_AVPLAY_ChnClose(HI_PLAYER_MEDIA_CHAN_AUD);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_AVPLAY_ChnClose fail, %d\n", Ret);
            return HI_FAILURE;
        }
    }

    Ret = HIADP_AVPLAY_UnRegEvent();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HIADP_AVPLAY_UnRegEvent fail, %d\n", Ret);
        return HI_FAILURE;
    }

    Ret = HI_UNF_AVPLAY_Destroy(hAvplay);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HI_UNF_AVPLAY_Destroy fail, %d\n", Ret);
        return HI_FAILURE;
    }

    if (HI_TRUE == bIsDmxReady)
    {
        Ret = HIADP_DMX_DetachTSPort();
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HIADP_DMX_DetachTSPort fail, %d\n", Ret);
            return HI_FAILURE;
        }
    }

    bIsDmxReady = HI_FALSE;
    bIsTrackReady = HI_FALSE;
    bIsWindowReady = HI_FALSE;
    bIsAudChnReady = HI_FALSE;
    bIsVidChnReady = HI_FALSE;
    bIsTsPlayReady = HI_FALSE;
    bIsEsPlayReady = HI_FALSE;

    PLAYER_LOGI("MediaDevice Destroy Success!\n");
    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_AVPLAY_Start(HI_PLAYER_MEDIA_CHAN_E eChn, HI_PLAYER_STREAM_TYPE_E eStreamType)
{
    HI_S32      Ret = HI_SUCCESS;
    HI_U32      u32AudPid;
    HI_U32      u32VidPid;
    HI_S32      HisiVidDecType;

    if (eChn & ((HI_U32)HI_PLAYER_MEDIA_CHAN_VID))
    {
        if (eStreamType == HI_PLAYER_STREAM_TYPE_ES)
        {
            Ret = vFormat2HiType(ProgramInfo.stVidStream.vFmt, &HisiVidDecType);
            PLAYER_CHK_PRINTF((true != Ret), Ret, "Call vFormat2HiType fail");
            if (Ret == true)
                Ret = HI_SUCCESS;
            else
                Ret = HI_FAILURE;

            Ret |= HIADP_AVPLAY_SetVdecAttr(hAvplay, (HI_UNF_VCODEC_TYPE_E)HisiVidDecType, HI_UNF_VCODEC_MODE_NORMAL);
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HIADP_AVPLAY_SetVdecAttr failed");

            Ret |= HI_UNF_AVPLAY_Start(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID, HI_NULL);
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_UNF_AVPLAY_Start video failed");

            VidEnable = HI_TRUE;
            if (HI_SUCCESS != Ret)
            {
                PLAYER_LOGE("VidChn Start failed.\n");
                PLAYER_LOGE("PLAYER_EVENT_BLANK_SCREEN es vid start failed\n");
                g_callBackFunc(g_eventHandler, PLAYER_EVENT_BLANK_SCREEN, 0, 0);
            }
        }
        else if (eStreamType == HI_PLAYER_STREAM_TYPE_TS)
        {
            u32VidPid = ProgramInfo.stVidStream.pid;
            Ret = vFormat2HiType(ProgramInfo.stVidStream.vFmt, &HisiVidDecType);
            PLAYER_CHK_PRINTF((true != Ret), Ret, "Call vFormat2HiType fail");
            if (Ret == true)
                Ret = HI_SUCCESS;
            else
                Ret = HI_FAILURE;

            Ret |= HIADP_AVPLAY_SetVdecAttr(hAvplay, (HI_UNF_VCODEC_TYPE_E)HisiVidDecType, HI_UNF_VCODEC_MODE_NORMAL);
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HIADP_AVPLAY_SetVdecAttr failed");

            Ret |= HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_VID_PID, &u32VidPid);
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call AVPlay SetVdecAttr ID_VID_PID failed");

            Ret |= HI_UNF_AVPLAY_Start(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID, HI_NULL);
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_UNF_AVPLAY_Start video failed");

            VidEnable = HI_TRUE;
            PLAYER_LOGI("[Video] PID:%d, vformat:%d\n", u32VidPid, HisiVidDecType);
            if (HI_SUCCESS != Ret)
            {
                PLAYER_LOGE("VidChn Start failed.\n");
                PLAYER_LOGE("PLAYER_EVENT_BLANK_SCREEN ts vid start failed\n");
                g_callBackFunc(g_eventHandler, PLAYER_EVENT_BLANK_SCREEN, 0, 0);
            }
        }
    }

    if (eChn & ((HI_U32)HI_PLAYER_MEDIA_CHAN_AUD))
    {
        if (eStreamType == HI_PLAYER_STREAM_TYPE_ES)

        {
            ProgramInfo.u32AudStreamNum = 1;

            Ret = SetAudStream();
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call SetAudStream failed");

            Ret |= HI_UNF_AVPLAY_Start(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD, HI_NULL);
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "call HI_UNF_AVPLAY_Start audio failed");

            AudEnable = HI_TRUE;
            if (HI_SUCCESS != Ret)
                PLAYER_LOGE("AudChn Start failed.\n");
        }
        else if (eStreamType == HI_PLAYER_STREAM_TYPE_TS)
        {
            u32AudPid = ProgramInfo.astAudStream[0].pid;

            Ret = SetAudStream();
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call SetAudStream failed");

            Ret |= HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_AUD_PID, &u32AudPid);
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_UNF_AVPLAY_ATTR_ID_AUD_PID failed");

            Ret |= HI_UNF_AVPLAY_Start(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD, HI_NULL);
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "call HI_UNF_AVPLAY_Start audio failed");

            AudEnable = HI_TRUE;

            PLAYER_LOGI("[Audio] PID:%d\n", u32AudPid);
            if (HI_SUCCESS != Ret)
                PLAYER_LOGE("AudChn Start failed.\n");
        }
    }

    return Ret;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_AVPLAY_Stop()
{
    HI_S32                  Ret;
    HI_UNF_VCODEC_ATTR_S    stVdecAdvAttr;

    if (bIsVidChnReady == HI_TRUE)
    {
        if ((HI_PLAYER_STATE_FAST == PlayerState) || (HI_PLAYER_STATE_PAUSE == PlayerState)
            || (HI_PLAYER_STATE_PLAY == PlayerState))
        {
            Ret = HI_UNF_AVPLAY_GetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, (HI_VOID*)&stVdecAdvAttr);
            if (HI_SUCCESS != Ret)
            {
                PLAYER_LOGE("Call HI_UNF_AVPLAY_GetAttr failed\n");
                PLAYER_UNLOCK();
                return HI_FAILURE;
            }

            stVdecAdvAttr.bOrderOutput = HI_FALSE;
            stVdecAdvAttr.u32ErrCover = 0;
            stVdecAdvAttr.enMode = HI_UNF_VCODEC_MODE_NORMAL;
            Ret = HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, (HI_VOID*)&stVdecAdvAttr);
            if (HI_SUCCESS != Ret)
            {
                PLAYER_LOGE("Call HI_UNF_AVPLAY_SetAttr failed\n");
                PLAYER_UNLOCK();
                return HI_FAILURE;
            }
        }

        Ret = HI_UNF_AVPLAY_Stop(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID, &PlayerStopMode);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("Call HI_UNF_AVPLAY_Stop CHAN_VID failed\n");
            PLAYER_UNLOCK();
            return HI_FAILURE;
        }

        VidEnable = HI_FALSE;
    }

    if (bIsAudChnReady == HI_TRUE)
    {
        Ret = HI_UNF_AVPLAY_Stop(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD, HI_NULL);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("Call HI_UNF_AVPLAY_Stop CHAN_AUD failed\n");
            PLAYER_UNLOCK();
            return HI_FAILURE;
        }

        AudEnable = HI_FALSE;
    }

    /* Close multiple aud demux channel for MultiAudio, then creat one aud demux channel again*/
    if (ProgramInfo.u32AudStreamNum > 1)
    {
        if (HI_TRUE == bIsTrackReady)
        {
            Ret = HI_UNF_SND_Detach(hTrack, hAvplay);
            if (HI_SUCCESS != Ret)
            {
                PLAYER_LOGE("call HI_UNF_SND_Detach fail, %d\n", Ret);
                PLAYER_UNLOCK();
                return HI_FAILURE;
            }

            Ret = HI_UNF_SND_DestroyTrack(hTrack);
            if (HI_SUCCESS != Ret)
            {
                PLAYER_LOGE("call HI_UNF_SND_DestroyTrack fail, %d\n", Ret);
                PLAYER_UNLOCK();
                return HI_FAILURE;
            }
            bIsTrackReady = HI_FALSE;
        }

        Ret = HI_UNF_AVPLAY_ChnClose(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("Call HI_UNF_AVPLAY_ChnClose CHAN_AUD failed\n");
            PLAYER_UNLOCK();
            return HI_FAILURE;
        }
        Ret = HI_UNF_AVPLAY_ChnOpen(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD, HI_NULL);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("Call HI_UNF_AVPLAY_ChnOpen CHAN_AUD failed\n");
            PLAYER_UNLOCK();
            return false;
        }
    }

    if (CurStreamType == HI_PLAYER_STREAM_TYPE_TS)
    {
        Ret = HI_UNF_DMX_ResetTSBuffer(hTsBuf);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("Call HI_UNF_DMX_ResetTSBuffer failed\n");
            PLAYER_UNLOCK();
            return HI_FAILURE;
        }
    }

    PLAYER_LOGI("Stop!\n");

    HI_S32 s32IndexOfAvplay = FindIndexByhAvplay(hAvplay);
    if (-1 != s32IndexOfAvplay)
    {
        g_playerEventPara[s32IndexOfAvplay].bIsFirVidFrm = HI_FALSE;
    }

    ProgramInfo.u32AudStreamNum = 0;
    PlayerState = HI_PLAYER_STATE_STOP;

    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_AVPLAY_ChnOpen(HI_PLAYER_MEDIA_CHAN_E eChn)
{
    HI_S32  Ret;

    if (eChn & ((HI_U32)HI_PLAYER_MEDIA_CHAN_VID))
    {
        PLAYER_LOGE("HI_UNF_AVPLAY_MEDIA_CHAN_VID");
        Ret = HI_UNF_AVPLAY_ChnOpen(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID, HI_NULL);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("VID: call HI_UNF_AVPLAY_ChnOpen fail, %d\n", Ret);
            return HI_FAILURE;
        }

        bIsVidChnReady = HI_TRUE;
    }

    if (eChn & ((HI_U32)HI_PLAYER_MEDIA_CHAN_AUD))
    {
        PLAYER_LOGE("HI_PLAYER_MEDIA_CHAN_AUD");
        Ret = HIADP_AVPLAY_RegADecLib();
        if (Ret != HI_SUCCESS)
        {
            PLAYER_LOGE("call HIADP_AVPLAY_RegADecLib fail, %d\n", Ret);
            return HI_FAILURE;
        }

        Ret = HI_UNF_AVPLAY_ChnOpen(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD, HI_NULL);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("AUD: call HI_UNF_AVPLAY_ChnOpen fail, %d\n", Ret);
            return HI_FAILURE;
        }

        Ret = HI_UNF_SND_SetTrackMode(HI_UNF_SND_0, HI_UNF_SND_OUTPUTPORT_ALL, HI_UNF_TRACK_MODE_STEREO);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_SND_SetTrackMode fail, %d\n", Ret);
            HI_UNF_AVPLAY_ChnClose(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD);
            return HI_FAILURE;
        }

        bIsAudChnReady = HI_TRUE;
    }

    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_AVPLAY_ChnClose(HI_PLAYER_MEDIA_CHAN_E eChn)
{
    HI_S32  Ret;

    if (eChn & ((HI_U32)HI_PLAYER_MEDIA_CHAN_VID))
    {
        Ret = HI_UNF_AVPLAY_ChnClose(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_AVPLAY_ChnClose fail, %d\n", Ret);
            return HI_FAILURE;
        }
    }

    if (eChn & ((HI_U32)HI_PLAYER_MEDIA_CHAN_AUD))
    {
        Ret = HI_UNF_AVPLAY_ChnClose(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_AVPLAY_ChnClose fail, %d\n", Ret);
            return HI_FAILURE;
        }
    }

    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_AVPLAY_AttachWindow()
{
    HI_S32          Ret = HI_SUCCESS;

    PLAYER_LOGI("HI_UNF_VO_CreateWindow begin");
    Ret = HI_UNF_VO_CreateWindow(&WindowAttr, &hWindow);
    PLAYER_LOGI("HI_UNF_VO_CreateWindow end");
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "call HI_UNF_VO_CreateWindow fail");

    Ret = HI_UNF_VO_AttachWindow(hWindow, hAvplay);
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "call HI_UNF_VO_AttachWindow fail");

    Ret |= HI_UNF_VO_SetWindowEnable(hWindow, HI_TRUE);
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "call HI_UNF_VO_SetWindowEnable fail");

    if (HI_SUCCESS == Ret)
    {
        PLAYER_LOGI("Create Video Win Success\n");
        bIsWindowReady = HI_TRUE;
    }
    else
    {
        bIsWindowReady = HI_FALSE;
        PLAYER_LOGI("PLAYER_EVENT_BLANK_SCREEN HIADP_AVPLAY_AttachWindow failed\n");
        g_callBackFunc(g_eventHandler, PLAYER_EVENT_BLANK_SCREEN, 0, 0);
    }

    return Ret;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_AVPLAY_DetachWindow()
{
    HI_S32          Ret = HI_SUCCESS;

    Ret = HI_UNF_VO_SetWindowEnable(hWindow, HI_FALSE);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HI_UNF_VO_SetWindowEnable fail, %d\n", Ret);
        return HI_FAILURE;
    }

    Ret = HI_UNF_VO_DetachWindow(hWindow, hAvplay);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HI_UNF_VO_DetachWindow fail, %d\n", Ret);
        return HI_FAILURE;
    }

    Ret = HI_UNF_VO_DestroyWindow(hWindow);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HI_UNF_VO_DestroyWindow fail, %d\n", Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_AVPLAY_AttachSound()
{
    HI_S32          Ret = HI_SUCCESS;

    Ret = HI_UNF_SND_GetDefaultTrackAttr(HI_UNF_SND_TRACK_TYPE_MASTER, &TrackAttr);
    Ret|= HI_UNF_SND_CreateTrack(HI_UNF_SND_0, &TrackAttr, &hTrack);
    if (HI_SUCCESS == Ret)
    {
        PLAYER_LOGE("Audio Master Track is idle!\n");
        PLAYER_LOGE("Create Audio Master Track Success\n");
    }
    else
    {
        PLAYER_LOGE("Audio Master Track is Busy!\n");

        Ret = HI_UNF_SND_GetDefaultTrackAttr(HI_UNF_SND_TRACK_TYPE_SLAVE, &TrackAttr);
        Ret|= HI_UNF_SND_CreateTrack(HI_UNF_SND_0, &TrackAttr, &hTrack);
        PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Create Audio Slave Track failed");
    }

    if (HI_SUCCESS == Ret)
    {
        Ret = HI_UNF_SND_Attach(hTrack, hAvplay);
        PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "call HI_UNF_SND_Attach fail");
        if (HI_SUCCESS != Ret)
        {
            bIsTrackReady = HI_FALSE;
        }
        else
        {
            bIsTrackReady = HI_TRUE;
            PLAYER_LOGI("Create Audio Track Success\n");
        }
    }

    return Ret;
}

HI_S32 CMCC_MediaProcessorImpl::HIADP_AVPLAY_DetachSound()
{
    HI_S32          Ret = HI_SUCCESS;

    Ret = HI_UNF_SND_Detach(hTrack, hAvplay);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HI_UNF_SND_Detach fail, %d\n", Ret);
        return HI_FAILURE;
    }

    Ret = HI_UNF_SND_DestroyTrack(hTrack);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HI_UNF_SND_DestroyTrack fail, %d\n", Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;

}

HI_S32 CMCC_MediaProcessorImpl::MediaDeviceInit()
{
    HI_S32  Ret;

    Ret = HI_SYS_Init();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HI_SYS_Init fail, %d\n", Ret);
        return HI_FAILURE;
    }

    Ret = HIADP_Disp_Init();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HIADP_Disp_Init fail, %d\n", Ret);
        goto ERR_SYS_DEINIT;
    }

    Ret = HIADP_Snd_Init();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HIADP_Snd_Init fail, %d\n", Ret);
        goto ERR_DISP_DEINIT;
    }

    Ret = HIADP_VO_Init(HI_UNF_VO_DEV_MODE_NORMAL);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HIADP_VO_Init fail, %d\n", Ret);
        goto ERR_SND_DEINIT;
    }

    Ret = HI_UNF_DMX_Init();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HI_UNF_DMX_Init fail, %d\n", Ret);
        goto ERR_VO_DEINIT;
    }

    Ret = HI_UNF_AVPLAY_Init();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HI_UNF_AVPLAY_Init fail, %d\n", Ret);
        goto ERR_DMX_DEINIT;
    }

    PLAYER_LOGI("MediaDevice Init Success!\n");
    return HI_SUCCESS;

ERR_DMX_DEINIT:
    Ret = HI_UNF_DMX_DeInit();
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_UNF_DMX_DeInit failed");
ERR_VO_DEINIT:
    Ret = HIADP_VO_DeInit();
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HIADP_VO_DeInit failed");
ERR_SND_DEINIT:
    Ret = HIADP_Snd_DeInit();
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HIADP_Snd_DeInit failed");
ERR_DISP_DEINIT:
    Ret = HIADP_Disp_DeInit();
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HIADP_Disp_DeInit failed");
ERR_SYS_DEINIT:
    Ret = HI_SYS_DeInit();
    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "Call HI_SYS_Deinit failed");

    return HI_FAILURE;
}

HI_S32 CMCC_MediaProcessorImpl::MediaDeviceDeInit()
{
    HI_S32  Ret;

    Ret = HI_UNF_AVPLAY_DeInit();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HI_UNF_AVPLAY_DeInit fail.\n");
        return HI_FAILURE;
    }

    Ret = HI_UNF_DMX_DeInit();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HI_UNF_DMX_DeInit fail.\n");
        return HI_FAILURE;
    }

    Ret = HIADP_Snd_DeInit();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HIADP_Snd_DeInit fail.\n");
        return HI_FAILURE;
    }

    Ret = HIADP_VO_DeInit();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HIADP_VO_DeInit fail.\n");
        return HI_FAILURE;
    }

    Ret = HIADP_Disp_DeInit();
   if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HIADP_Disp_DeInit fail.\n");
        return HI_FAILURE;
    }

    Ret = HI_SYS_DeInit();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HI_SYS_DeInit fail.\n");
        return HI_FAILURE;
    }

    PLAYER_LOGI("MediaDevice DeInit Success!\n");
    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::MediaDeviceStart()
{
    HI_S32      Ret = HI_SUCCESS;

    if (CurStreamType == HI_PLAYER_STREAM_TYPE_ES)
    {
        if ((HI_TRUE == bIsWindowReady) && (HI_TRUE == bIsVidChnReady))
        {
            Ret = HIADP_AVPLAY_Start(HI_PLAYER_MEDIA_CHAN_VID, HI_PLAYER_STREAM_TYPE_ES);
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "HIADP_AVPLAY_StartEsVidChn");
        }

        if ((HI_TRUE == bIsTrackReady) && (HI_TRUE == bIsAudChnReady))
        {
            Ret = HIADP_AVPLAY_Start(HI_PLAYER_MEDIA_CHAN_AUD, HI_PLAYER_STREAM_TYPE_ES);
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "HIADP_AVPLAY_StartEsAudChn");
        }
    }

    if (CurStreamType == HI_PLAYER_STREAM_TYPE_TS)
    {
        if ((HI_TRUE == bIsWindowReady) && (HI_TRUE == bIsVidChnReady))
        {
            Ret = HIADP_AVPLAY_Start(HI_PLAYER_MEDIA_CHAN_VID, HI_PLAYER_STREAM_TYPE_TS);
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "HIADP_AVPLAY_StartTsVidChn");
        }

        if ((HI_TRUE == bIsTrackReady) && (HI_TRUE == bIsAudChnReady))
        {
            Ret = HIADP_AVPLAY_Start(HI_PLAYER_MEDIA_CHAN_AUD, HI_PLAYER_STREAM_TYPE_TS);
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "HIADP_AVPLAY_StartEsAudChn");
        }

    }

    LstStreamType = CurStreamType;

    return Ret;
}

HI_S32 CMCC_MediaProcessorImpl::MediaDeviceStop()
{
    HI_S32                  Ret;

    Ret = HIADP_AVPLAY_Stop();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HIADP_AVPLAY_Stop fail.\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::MediaDeviceCreate(HI_PLAYER_STREAM_TYPE_E eStreamType)
{
    HI_S32  Ret;

    if (CurStreamType == LstStreamType)
    {
        if (CurStreamType == HI_PLAYER_STREAM_TYPE_ES)
        {
            if (HI_FALSE == bIsEsPlayReady)
            {
                Ret = HIADP_AVPLAY_Create(HI_PLAYER_STREAM_TYPE_ES);
                PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "MediaDeviceCreate");
            }

            if ((ES_STREAM_PID == ProgramInfo.stVidStream.pid)
              &&(VIDEO_FORMAT_UNKNOWN != ProgramInfo.stVidStream.vFmt))
            {
                if (HI_FALSE == bIsVidChnReady)

                {
                    Ret = HIADP_AVPLAY_ChnOpen(HI_PLAYER_MEDIA_CHAN_VID);
                    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "HIADP_AVPLAY_StartEsAudChn");
                }
            }

            if (ES_STREAM_PID == ProgramInfo.astAudStream[0].pid
             &&(AUDIO_FORMAT_UNKNOWN != ProgramInfo.astAudStream[0].aFmt))
            {
                if (HI_FALSE == bIsAudChnReady)

                {
                    Ret = HIADP_AVPLAY_ChnOpen(HI_PLAYER_MEDIA_CHAN_AUD);
                    PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "HIADP_AVPLAY_StartEsAudChn");
                }
            }
        }

        if (CurStreamType == HI_PLAYER_STREAM_TYPE_TS)
        {
            if (HI_FALSE == bIsTsPlayReady)
            {
                Ret = HIADP_AVPLAY_Create(HI_PLAYER_STREAM_TYPE_TS);
                PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "MediaDeviceCreate");
            }
        }
    }
    else if (CurStreamType == HI_PLAYER_STREAM_TYPE_ES)
    {
        PLAYER_LOGI("CurStreamType == HI_PLAYER_STREAM_TYPE_ES\n");
        if (HI_TRUE == bIsTsPlayReady)
        {
            Ret = HIADP_AVPLAY_Stop();
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "HIADP_AVPLAY_Stop");
            Ret = HIADP_AVPLAY_Destroy();
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "HIADP_AVPLAY_Destroy");
        }

        Ret = HIADP_AVPLAY_Create(HI_PLAYER_STREAM_TYPE_ES);
        PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "HIADP_AVPLAY_Create");

       if ((ES_STREAM_PID == ProgramInfo.stVidStream.pid)
         &&(VIDEO_FORMAT_UNKNOWN != ProgramInfo.stVidStream.vFmt))
        {
            HIADP_AVPLAY_ChnOpen(HI_PLAYER_MEDIA_CHAN_VID);
        }

        if (ES_STREAM_PID == ProgramInfo.astAudStream[0].pid
         &&(AUDIO_FORMAT_UNKNOWN != ProgramInfo.astAudStream[0].aFmt))
        {
            HIADP_AVPLAY_ChnOpen(HI_PLAYER_MEDIA_CHAN_AUD);
        }
    }
    else if (CurStreamType == HI_PLAYER_STREAM_TYPE_TS)
    {
        PLAYER_LOGI("CurStreamType == HI_PLAYER_STREAM_TYPE_TS\n");
        if (HI_TRUE == bIsEsPlayReady)
        {
            Ret = HIADP_AVPLAY_Stop();
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "HIADP_AVPLAY_Stop");
            Ret = HIADP_AVPLAY_Destroy();
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "HIADP_AVPLAY_Destroy");
        }

        Ret = HIADP_AVPLAY_Create(HI_PLAYER_STREAM_TYPE_TS);
        PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "HIADP_AVPLAY_Create");
    }

    if ((HI_FALSE == bIsWindowReady) && (HI_TRUE == bIsVidChnReady))
    {
        Ret = HIADP_AVPLAY_AttachWindow();
        PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "HIADP_AVPLAY_AttachWindow");
    }

    if ((HI_FALSE == bIsTrackReady) && (HI_TRUE == bIsAudChnReady))
    {
        Ret = HIADP_AVPLAY_AttachSound();
        PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), Ret, "HIADP_AVPLAY_AttachSound");
    }

    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::MediaDeviceDestroy()
{
    HI_S32  Ret;

    Ret = HIADP_AVPLAY_Destroy();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call HIADP_AVPLAY_Destroy fail.\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 CMCC_MediaProcessorImpl::SetAudStream()
{
    HI_S32                          Ret = HI_SUCCESS;
    HA_FORMAT_E                     enHaFormat = FORMAT_BUTT;
    HA_CODEC_ID_E                   hiADecID;
    HI_U8                           *pu8Extradata;
    AUDIO_PARA_T*                   pSetAudioPara = NULL;
    HI_UNF_AVPLAY_MULTIAUD_ATTR_S   stMultiAudAttr;
    HI_UNF_ACODEC_ATTR_S            stAdecAttr;
    HI_U32                          AudPid[32] = {0};
    HI_UNF_ACODEC_ATTR_S            AdecAttr[32];
    HI_U32                          i;
    HI_BOOL                         bIsBigEndian = HI_FALSE;

    if(1 == ProgramInfo.u32AudStreamNum)
    {
        pSetAudioPara = &(ProgramInfo.astAudStream[0]);
        pu8Extradata = pSetAudioPara->pExtraData;

        Ret = (HI_S32)aFormat2HiType(pSetAudioPara->aFmt, &enHaFormat, &bIsBigEndian);
        PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), HI_FAILURE, "call aFormat2HiType failed");

        Ret = HI_UNF_AVPLAY_FoundSupportDeoder(enHaFormat, (HI_U32 *)&hiADecID);
        PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), HI_FAILURE, "call HI_UNF_AVPLAY_FoundSupportDeoder failed");

        stAdecAttr.stDecodeParam.enDecMode = HD_DEC_MODE_RAWPCM;
        stAdecAttr.enType = hiADecID;

        HIADP_AVPLAY_GetAdecAttr(&stAdecAttr, pu8Extradata, bIsBigEndian);

        Ret = HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_ADEC, &stAdecAttr);
        PLAYER_CHK_PRINTF((Ret != HI_SUCCESS), Ret, "Call HI_UNF_AVPLAY_SetAttr fail");
        PLAYER_LOGI("[Audio] aformat:0x%x\n", hiADecID);
    }
    else
    {
        stMultiAudAttr.u32PidNum = ProgramInfo.u32AudStreamNum;

        for(i = 0; i < stMultiAudAttr.u32PidNum; i++)
        {
            enHaFormat = FORMAT_BUTT;
            AudPid[i] = ProgramInfo.astAudStream[i].pid;

            pSetAudioPara = &(ProgramInfo.astAudStream[i]);
            pu8Extradata = pSetAudioPara->pExtraData;

            Ret = (HI_S32)aFormat2HiType(pSetAudioPara->aFmt, &enHaFormat, &bIsBigEndian);
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), HI_FAILURE, "call aFormat2HiType failed");

            Ret = HI_UNF_AVPLAY_FoundSupportDeoder(enHaFormat, (HI_U32 *)&hiADecID);
            PLAYER_CHK_PRINTF((HI_SUCCESS != Ret), HI_FAILURE, "call HI_UNF_AVPLAY_FoundSupportDeoder failed");

            AdecAttr[i].stDecodeParam.enDecMode = HD_DEC_MODE_RAWPCM;
            AdecAttr[i].enType = hiADecID;

            HIADP_AVPLAY_GetAdecAttr(&AdecAttr[i], pu8Extradata, bIsBigEndian);
            PLAYER_LOGI("[Audio] aformat[%d]:0x%x\n", i, hiADecID);
        }

        stMultiAudAttr.pu32AudPid = AudPid;
        stMultiAudAttr.pstAcodecAttr = AdecAttr;

        Ret = HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_MULTIAUD, &stMultiAudAttr);
        PLAYER_CHK_PRINTF((Ret != HI_SUCCESS), Ret, "Call HI_UNF_AVPLAY_SetAttr fail");
    }

    return Ret;
}

HI_S32 CMCC_MediaProcessorImpl::InitWinAttr(HI_UNF_WINDOW_ATTR_S* pWinAttr)
{
    HI_S32          Ret = HI_SUCCESS;
    memset(&WindowAttr, 0, sizeof(HI_UNF_WINDOW_ATTR_S));

    pWinAttr->enDisp = HI_UNF_DISPLAY1;
    pWinAttr->enVideoFormat = HI_UNF_FORMAT_YUV_SEMIPLANAR_420;
    pWinAttr->bVirtual = HI_FALSE;
    pWinAttr->stWinAspectAttr.enAspectCvrs = HI_UNF_VO_ASPECT_CVRS_IGNORE;
    pWinAttr->stWinAspectAttr.bUserDefAspectRatio = HI_FALSE;
    pWinAttr->bUseCropRect = HI_FALSE;
    pWinAttr->stInputRect.s32X = 0;
    pWinAttr->stInputRect.s32Y = 0;
    pWinAttr->stInputRect.s32Width = 0;
    pWinAttr->stInputRect.s32Height = 0;
    pWinAttr->stOutputRect.s32X = 0;
    pWinAttr->stOutputRect.s32Y = 0;

    pWinAttr->stWinAspectAttr.u32UserAspectWidth  = 0;
    pWinAttr->stWinAspectAttr.u32UserAspectHeight = 0;

    //set win {w,h} by resolution
    char buffer[PROP_VALUE_MAX];
    int resolution = 720;
    memset(buffer, 0, PROP_VALUE_MAX);
    property_get("persist.sys.resolution", buffer, "720");
    resolution = atoi(buffer);
    PLAYER_LOGI("HiMediaPlayer::createAVPlay, resolution = %d",resolution);
    if (resolution == 1080) {
        pWinAttr->stOutputRect.s32Width  = 1920;
        pWinAttr->stOutputRect.s32Height = 1080;
    } else {
        pWinAttr->stOutputRect.s32Width  = 1280;
        pWinAttr->stOutputRect.s32Height = 720;
    }

    PLAYER_MEMSET(&pWinAttr->stOutputRect, 0x0, sizeof(HI_RECT_S));

    PLAYER_LOGI("Init Window x:%d, y:%d, width:%d, height:%d\n",
        pWinAttr->stInputRect.s32X, pWinAttr->stInputRect.s32Y, pWinAttr->stInputRect.s32Width, pWinAttr->stInputRect.s32Height);

    return HI_SUCCESS;
}

HI_VOID* CMCC_MediaProcessorImpl::GetStatusInfo(HI_VOID *pArg)
{
    HI_S32                          Ret;
    HI_UNF_AVPLAY_STATUS_INFO_S     stStatusInfo;

    PLAYER_LOGI("Thread Create: Player_GetStatusInfo()\n");

    while(!bStatusThreadQuit)
    {
        if ((HI_PLAYER_STATE_STOP != PlayerState) && (HI_PLAYER_STATE_DEINI != PlayerState))
        {
            Ret = HI_UNF_AVPLAY_GetStatusInfo(hAvplay, &stStatusInfo);
            if (HI_SUCCESS == Ret)
            {
                AvplayStatusInfo = stStatusInfo;
            }
        }
        usleep(10000);
    }

    PLAYER_LOGI("Thread Exit: Player_GetStatusInfo()\n");
    return NULL;
}

HI_S32 CMCC_MediaProcessorImpl::StatusThread()
{
    HI_S32                      Ret;

    if (HI_FALSE == bIsPrepared)
    {
        PLAYER_LOGE("HW resource is not prepared !!!\n");
        return HI_FAILURE;
    }

    bStatusThreadQuit = HI_FALSE;

    typedef void* (*__thread_callback)(void *);
    __thread_callback GetStatusInfo = (__thread_callback)&CMCC_MediaProcessorImpl::GetStatusInfo;

    Ret = pthread_create(&GetStatusThd, HI_NULL, GetStatusInfo, this);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Thread Create fail: Player_GetStatusInfo()!\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

/**
 * @brief   获取当前模块的版本号
 * @param   无
 * @return  版本号
 **/
int Get_CMCCMediaProcessorVersion()
{
    ALOGV("Get_CMCCMediaProcessorVersion");
    //PLAYER_LOGI("libCMCC_MediaProcessor.so version: [%d], Release %s \n", CMCC_MEDIA_PROCESSOR_VERSION, CMCC_MEDIA_PROCESSOR_RELEASE);
    return CMCC_MEDIA_PROCESSOR_VERSION;
}

/**
 * @brief   获取Mediaprocessor对象
 * @param   无
 * @return  CMCC_MediaProcessor对象
 **/
CMCC_MediaProcessor* Get_CMCCMediaProcessor()
{
    return new CMCC_MediaProcessorImpl;
}

/**
 * @brief   构造函数
 * @param   无
 * @return
 **/
CMCC_MediaProcessorImpl::CMCC_MediaProcessorImpl()
{
    HI_S32  Ret;

    PLAYER_LOGI("CMCC_MediaProcessorImpl() Start\n");

    InitWinAttr(&WindowAttr);

    Ret = MediaDeviceInit();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("call MediaDeviceInit fail.\n");
    }

    bIsAudChnReady = HI_FALSE;
    bIsVidChnReady = HI_FALSE;
    bIsTsPlayReady = HI_FALSE;
    bIsEsPlayReady = HI_FALSE;

    CurStreamType = HI_PLAYER_STREAM_TYPE_TS;
    LstStreamType = HI_PLAYER_STREAM_TYPE_TS;

    ProgramInfo.u32AudStreamNum = 0;

    VidEnable = HI_FALSE;
    AudEnable = HI_FALSE;

    /*Stop Mode: black, no block*/
    PlayerStopMode.enMode = HI_UNF_AVPLAY_STOP_MODE_BLACK;
    PlayerStopMode.u32TimeoutMs = 0;

    PreSysTime = 0xFFFFFFFFU;
    PreAudPts  = 0xFFFFFFFFU;
    PreVidPts  = 0xFFFFFFFFU;

    if (HI_SUCCESS == Ret)
    {
        bIsPrepared = HI_TRUE;
        PlayerState = HI_PLAYER_STATE_INI;
    }
    else
    {
        bIsPrepared = HI_FALSE;
        PlayerState = HI_PLAYER_STATE_DEINI;
    }

    bIsFirVidFrm = HI_FALSE;
    bIsTrackReady = HI_FALSE;
    bIsWindowReady = HI_FALSE;

    mbSideBand = HI_FALSE;

    GetStatusThd = -1;
    Ret = StatusThread();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Create StatusThread fail.\n");
    }

    PLAYER_LOGI("CMCC_MediaProcessorImpl() End\n");
}

/**
 * @brief   析构函数
 * @param   无
 * @return
 **/
CMCC_MediaProcessorImpl::~CMCC_MediaProcessorImpl()
{
    PLAYER_LOCK();
    HI_S32  Ret;

    PLAYER_LOGI("~CMCC_MediaProcessorImpl() Start\n");

    if ((HI_TRUE == VidEnable) || (HI_TRUE == AudEnable))
    {
        Ret = MediaDeviceStop();
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call MediaDeviceStop fail, %d\n", Ret);
        }

    }

    if ((HI_TRUE == bIsTsPlayReady) || (HI_TRUE == bIsEsPlayReady))
    {
        Ret = MediaDeviceDestroy();
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call MediaDeviceDestroy fail, %d\n", Ret);
        }

    }

    //hiplayer->cmcc->hiplayer	avplay not init
    /*Ret = MediaDeviceDeInit();
        if (HI_SUCCESS != Ret)
        {
                PLAYER_LOGE("call MediaDeviceInit fail, %d\n", Ret);
        }*/

    bStatusThreadQuit = HI_TRUE;
    if (GetStatusThd != -1)
    {
        pthread_join(GetStatusThd, HI_NULL);
        GetStatusThd = -1;
    }

    bIsPrepared = HI_FALSE;
    bIsTrackReady = HI_FALSE;
    bIsWindowReady = HI_FALSE;
    bIsAudChnReady = HI_FALSE;
    bIsVidChnReady = HI_FALSE;
    bIsTsPlayReady = HI_FALSE;
    bIsEsPlayReady = HI_FALSE;
    PlayerState = HI_PLAYER_STATE_DEINI;

    if (mNativeWindow.get())
        native_window_set_sideband_stream(mNativeWindow.get(), NULL);

    PLAYER_UNLOCK();
    PLAYER_LOGI("~CMCC_MediaProcessorImpl() End\n");
}

/**
 * @brief   初始化视频参数
 * @param   pVideoPara - 视频相关参数，采用ES方式播放时，pid=0xffff; 没有video的情况下，vFmt=VFORMAT_UNKNOWN
 * @return  无
 **/
void CMCC_MediaProcessorImpl::InitVideo(PVIDEO_PARA_T pVideoPara)
{
    PLAYER_LOGI("InitVideo enter.\n");

    PLAYER_LOCK();

    if (HI_NULL == pVideoPara)
    {
        PLAYER_LOGE("Para is Invalid\n");
        PLAYER_UNLOCK();
        return ;
    }

    if ((PlayerState != HI_PLAYER_STATE_STOP)
      &&(PlayerState != HI_PLAYER_STATE_INI)
      &&(PlayerState != HI_PLAYER_STATE_DEINI))
    {
        PLAYER_LOGE("Player cur state is %d, can not init audio\n", PlayerState);
        PLAYER_UNLOCK();
        return ;
    }

    if (ES_STREAM_PID == pVideoPara->pid)
    {
        CurStreamType = HI_PLAYER_STREAM_TYPE_ES;
    }
    else
    {
        CurStreamType = HI_PLAYER_STREAM_TYPE_TS;
    }

    PLAYER_LOGI("VideoPara PID:%d, vformat:%d\n", pVideoPara->pid, pVideoPara->vFmt);
    PLAYER_MEMCPY(&ProgramInfo.stVidStream, pVideoPara, sizeof(VIDEO_PARA_T));
    PLAYER_UNLOCK();
    return ;
}

/**
 * @brief   初始化音频参数
 * @param   pAudioPara - 音频相关参数，采用ES方式播放时，pid=0xffff; 没有audio的情况下，aFmt=AFORMAT_UNKNOWN
 * @return  无
 **/
void CMCC_MediaProcessorImpl::InitAudio(PAUDIO_PARA_T pAudioPara)
{
    PLAYER_LOGI("InitAudio enter.\n");
    HI_U32          audioIndex = 0;
    PAUDIO_PARA_T   p = pAudioPara;

    PLAYER_LOCK();

    if (HI_NULL == pAudioPara)
    {
        PLAYER_LOGE("Para is Invalid\n");
        PLAYER_UNLOCK();
        return ;
    }

    if ((PlayerState != HI_PLAYER_STATE_STOP)
      &&(PlayerState != HI_PLAYER_STATE_INI)
      &&(PlayerState != HI_PLAYER_STATE_DEINI))
    {
        PLAYER_LOGE("Player cur state is %d, can not init audio\n", PlayerState);
        PLAYER_UNLOCK();
        return ;
    }

    if (ES_STREAM_PID == p->pid)
    {
        CurStreamType = HI_PLAYER_STREAM_TYPE_ES;
    }
    else
    {
        CurStreamType = HI_PLAYER_STREAM_TYPE_TS;
    }

    if ((p->pid > 0) && (p->pid < 0x1fff))
    {
        PLAYER_LOGI("AudioPara[0] PID:%d, aformat:%d\n", p->pid, p->aFmt);
        PLAYER_MEMCPY(&ProgramInfo.astAudStream[ProgramInfo.u32AudStreamNum], p, sizeof(AUDIO_PARA_T));
        ProgramInfo.u32AudStreamNum++;
        PLAYER_LOGI("AudioPara nChannels :%d \n", ProgramInfo.u32AudStreamNum);
    }

    if (p->pid == ES_STREAM_PID)
    {
        PLAYER_LOGI("AudioPara[0] PID:%d, aformat:%d\n", p->pid, p->aFmt);
        PLAYER_MEMCPY(&ProgramInfo.astAudStream[0], p, sizeof(AUDIO_PARA_T));
        ProgramInfo.u32AudStreamNum = 1;
        PLAYER_LOGI("AudioPara nChannels :%d \n", ProgramInfo.u32AudStreamNum);
    }

    PLAYER_UNLOCK();
    return ;
}

void KeepSPSPPS(HI_HANDLE hAvplay, HI_BOOL bKeepSPSPPS)
{
    HI_CODEC_VIDEO_CMD_S VidCmd;
    memset(&VidCmd,0,sizeof(HI_CODEC_VIDEO_CMD_S));
    VidCmd.u32CmdID = HI_UNF_AVPLAY_KEEP_SPS_PPS_INFO_CMD;
    VidCmd.pPara = &bKeepSPSPPS;

    if (HI_UNF_AVPLAY_Invoke(hAvplay, HI_UNF_AVPLAY_INVOKE_VCODEC, &VidCmd) != HI_SUCCESS)
    {
       PLAYER_LOGI("%s->%d, HI_UNF_AVPLAY_Invoke fail!\n",__func__,__LINE__);
    }
}


/**
 * @brief   开始播放
 * @param   无
 * @return  true  - 成功
 *          false - 失败
 **/
bool CMCC_MediaProcessorImpl::StartPlay()
{
    PLAYER_LOGI("StartPlay enter.\n");
    HI_S32      Ret;

    PLAYER_LOCK();
    PLAYER_LOGE("%s %d\n",__FUNCTION__,__LINE__);
    if (HI_FALSE == bIsPrepared)
    {
        PLAYER_LOGE("HW resource is not prepared !!!\n");
        PLAYER_UNLOCK();
        return false;
    }
    PLAYER_LOGE("%s %d\n",__FUNCTION__,__LINE__);
    if (HI_PLAYER_STATE_PLAY == PlayerState)
    {
        PLAYER_LOGI("MediaDevice already start, return\n");
        PLAYER_UNLOCK();
        return true;
    }
    PLAYER_LOGE("%s %d\n",__FUNCTION__,__LINE__);
    Ret = MediaDeviceCreate(CurStreamType);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGI("call MediaDeviceStart fail.\n\n");
        PLAYER_UNLOCK();
        return false;
    }
    PLAYER_LOGE("%s %d\n",__FUNCTION__,__LINE__);
    if (IsUseSidebandMode(mNativeWindow))
    {
        mbSideBand = HI_TRUE;
        SetSidebandHandle(mNativeWindow);
    }
    PLAYER_LOGE("%s %d\n",__FUNCTION__,__LINE__);
    Ret = MediaDeviceStart();
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGI("call MediaDeviceStart fail.\n\n");
        MediaDeviceDestroy();
        PLAYER_UNLOCK();
        return false;
    }
    PLAYER_LOGE("%s %d\n",__FUNCTION__,__LINE__);
    KeepSPSPPS(hAvplay, HI_TRUE);
    PLAYER_LOGE("%s %d\n",__FUNCTION__,__LINE__);
	
    PlayerState = HI_PLAYER_STATE_PLAY;
    PLAYER_UNLOCK();

    return true;
}

/**
 * @brief   暂停
 * @param   无
 * @return  true  - 成功
 *          false - 失败
 **/
bool CMCC_MediaProcessorImpl::Pause()
{
    PLAYER_LOGI("Pause enter.\n");
    HI_S32      Ret;

    PLAYER_LOCK();

    if ((HI_FALSE == bIsTsPlayReady)
      &&(HI_FALSE == bIsEsPlayReady))
    {
        PLAYER_LOGE("HW resource is not prepared !!!\n");
        PLAYER_UNLOCK();
        return false;
    }

    if (HI_PLAYER_STATE_PAUSE == PlayerState)
    {
        PLAYER_LOGI("MediaDevice already pause, return\n");
        PLAYER_UNLOCK();
        return true;
    }

    Ret = HI_UNF_AVPLAY_Pause(hAvplay, NULL);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_Pause fail.\n");
        PLAYER_UNLOCK();
        return false;
    }

    PLAYER_LOGI("Pause!\n");
    PlayerState = HI_PLAYER_STATE_PAUSE;
    PLAYER_UNLOCK();

    return true;
}

/**
 * @brief   继续播放
 * @param   无
 * @return  true  - 成功
 *          false - 失败
 **/
bool CMCC_MediaProcessorImpl::Resume()
{
    PLAYER_LOGI("Resume enter.\n");
    HI_S32                  Ret;
    HI_UNF_VCODEC_ATTR_S    stVcodecAttr;

    PLAYER_LOCK();

    if ((HI_FALSE == bIsTsPlayReady)
      &&(HI_FALSE == bIsEsPlayReady))
    {
        PLAYER_LOGE("HW resource is not prepared !!!\n");
        PLAYER_UNLOCK();
        return false;
    }

    Ret = HI_UNF_AVPLAY_Resume(hAvplay, NULL);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_Resume fail.\n");
        PLAYER_UNLOCK();
        return false;
    }

    Ret = HI_UNF_AVPLAY_GetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, (HI_VOID*)&stVcodecAttr);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_GetAttr fail.\n");
        PLAYER_UNLOCK();
        return false;
    }

    stVcodecAttr.bOrderOutput = HI_FALSE;
    stVcodecAttr.enMode = HI_UNF_VCODEC_MODE_NORMAL;
    stVcodecAttr.u32ErrCover = 0;

    Ret = HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, (HI_VOID*)&stVcodecAttr);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_SetAttr fail.\n");
        PLAYER_UNLOCK();
        return false;
    }

    PLAYER_LOGI("Resume!\n");
    PlayerState = HI_PLAYER_STATE_PLAY;
    PLAYER_UNLOCK();
    return true;
}

/**
 * @brief   快进快退
 * @param   无
 * @return  true  - 成功
 *          false - 失败
 **/
bool CMCC_MediaProcessorImpl::TrickPlay()
{
    PLAYER_LOGI("TrickPlay enter.\n");
    HI_S32                  Ret;
    HI_UNF_VCODEC_ATTR_S    stVcodecAttr;

    PLAYER_LOCK();

    if ((HI_FALSE == bIsTsPlayReady)
      &&(HI_FALSE == bIsEsPlayReady))
    {
        PLAYER_LOGE("HW resource is not prepared !!!\n");
        PLAYER_UNLOCK();
        return false;
    }

    if (HI_PLAYER_STATE_FAST == PlayerState)
    {
        PLAYER_LOGI("MediaDevice State already in fast, return\n");
        PLAYER_UNLOCK();
        return true;
    }

    KeepSPSPPS(hAvplay, HI_TRUE);

    Ret = HI_UNF_AVPLAY_Reset(hAvplay, NULL);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_Reset fail.\n");
        PLAYER_UNLOCK();
        return false;
    }

    Ret = HI_UNF_AVPLAY_Tplay(hAvplay, NULL);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_Tplay fail.\n");
        PLAYER_UNLOCK();
        return false;
    }

    if (bIsDmxReady == HI_TRUE)
    {
        Ret = HI_UNF_DMX_ResetTSBuffer(hTsBuf);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("Call HI_UNF_DMX_ResetTSBuffer fail.\n");
            PLAYER_UNLOCK();
            return false;
        }
    }

    Ret = HI_UNF_AVPLAY_GetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, (HI_VOID*)&stVcodecAttr);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_GetAttr fail.\n");
        PLAYER_UNLOCK();
        return false;
    }

    stVcodecAttr.bOrderOutput = HI_TRUE;
    stVcodecAttr.enMode = HI_UNF_VCODEC_MODE_I;
    stVcodecAttr.u32ErrCover = 0;

    Ret = HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, (HI_VOID*)&stVcodecAttr);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_SetAttr fail.\n");
        PLAYER_UNLOCK();
        return false;
    }

    PLAYER_LOGI("TrickPlay!\n");
    PlayerState = HI_PLAYER_STATE_FAST;
    PLAYER_UNLOCK();
    return true;
}

/**
 * @brief   停止快进快退
 * @param   无
 * @return  true  - 成功
 *          false - 失败
 **/
bool CMCC_MediaProcessorImpl::StopTrickPlay()
{
    PLAYER_LOGI("StopTrickPlay enter.\n");
    HI_S32                  Ret;
    HI_UNF_VCODEC_ATTR_S    stVcodecAttr;

    PLAYER_LOCK();

    if ((HI_FALSE == bIsTsPlayReady)
      &&(HI_FALSE == bIsEsPlayReady))
    {
        PLAYER_LOGE("HW resource is not prepared !!!\n");
        PLAYER_UNLOCK();
        return false;
    }

    Ret = HI_UNF_AVPLAY_Resume(hAvplay, NULL);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_Resume fail.\n");
        PLAYER_UNLOCK();
        return false;
    }

    Ret = HI_UNF_AVPLAY_GetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, (HI_VOID*)&stVcodecAttr);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_GetAttr fail.\n");
        PLAYER_UNLOCK();
        return false;
    }

    stVcodecAttr.bOrderOutput = HI_FALSE;
    stVcodecAttr.enMode = HI_UNF_VCODEC_MODE_NORMAL;
    stVcodecAttr.u32ErrCover = 0;

    Ret = HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, (HI_VOID*)&stVcodecAttr);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_SetAttr fail.\n");
        PLAYER_UNLOCK();
        return false;
    }

    PLAYER_LOGI("StopTrickPlay!\n");
    PlayerState = HI_PLAYER_STATE_PLAY;
    PLAYER_UNLOCK();
    return true;
}

/**
 * @brief   停止
 * @param   无
 * @return  true  - 成功
 *          false - 失败
 **/
bool CMCC_MediaProcessorImpl::Stop()
{
    PLAYER_LOGI("Stop enter.\n");
    HI_S32      Ret;

    PLAYER_LOCK();

    if ((HI_FALSE == bIsTsPlayReady)
      &&(HI_FALSE == bIsEsPlayReady))
    {
        PLAYER_LOGE("HW resource is not prepared !!!\n");
        PLAYER_UNLOCK();
        return false;
    }

    if (HI_PLAYER_STATE_STOP == PlayerState)
    {
        PLAYER_LOGI("MediaDevice State already in Stop, return\n");
        PLAYER_UNLOCK();
        return true;
    }

    Ret = MediaDeviceStop();
    if (HI_SUCCESS == Ret)
    {
        PLAYER_UNLOCK();
        return true;
    }
    else
    {
        PLAYER_UNLOCK();
        return false;
    }
}

/**
 * @brief   定位
 * @param   无
 * @return  true  - 成功
 *          false - 失败
 **/
bool CMCC_MediaProcessorImpl::Seek()
{
    PLAYER_LOGI("Seek enter.\n");
    HI_S32                  Ret;
    HI_UNF_VCODEC_ATTR_S    stVcodecAttr;

    PLAYER_LOCK();

    if ((HI_FALSE == bIsTsPlayReady)
      &&(HI_FALSE == bIsEsPlayReady))
    {
        PLAYER_LOGE("HW resource is not prepared !!!\n");
        PLAYER_UNLOCK();
        return false;
    }

    KeepSPSPPS(hAvplay, HI_TRUE);

    Ret = HI_UNF_AVPLAY_Reset(hAvplay, HI_NULL);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_Reset failed\n");
        PLAYER_UNLOCK();
        return false;
    }
	

	HI_S32 s32IndexOfAvplay = FindIndexByhAvplay(hAvplay);
	g_playerEventPara[s32IndexOfAvplay].bIsFirVidFrm = HI_FALSE;
	PLAYER_LOGE("bIsFirVidFrm = HI_FALSE  index=%d bIsFirVidFrm=%d\n",s32IndexOfAvplay,bIsFirVidFrm);

    if(HI_TRUE == bIsTsPlayReady){
        PLAYER_LOGI("Reset DMX Buffer!\n");
        Ret = HI_UNF_DMX_ResetTSBuffer(hTsBuf);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("Call HI_UNF_DMX_ResetTSBuffer failed\n");
            PLAYER_UNLOCK();
            return false;
        }
    }

    /*player state is pause, need to change play status*/
    Ret = HI_UNF_AVPLAY_Resume(hAvplay, HI_NULL);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_Resume failed\n");
        PLAYER_UNLOCK();
        return false;
    }

    Ret = HI_UNF_AVPLAY_GetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, (HI_VOID*)&stVcodecAttr);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_GetAttr fail.\n");
        PLAYER_UNLOCK();
        return false;
    }

    stVcodecAttr.bOrderOutput = HI_FALSE;
    stVcodecAttr.enMode = HI_UNF_VCODEC_MODE_NORMAL;
    stVcodecAttr.u32ErrCover = 0;

    Ret = HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, (HI_VOID*)&stVcodecAttr);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_SetAttr fail.\n");
        PLAYER_UNLOCK();
        return false;
    }

    PlayerState = HI_PLAYER_STATE_PLAY;
    PLAYER_UNLOCK();

    PLAYER_LOGI("Seek!\n");
    return true;
}

/**
 * @brief   获取nSize大小的buffer
 * @param   type    - ts/video/audio
 *          pBuffer - buffer地址，如果有nSize大小的空间，则将置为对应的地址;如果不足nSize，置NULL
 *          nSize   - 需要的buffer大小
 * @return  0  - 成功
 *          -1 - 失败
 **/
int32_t CMCC_MediaProcessorImpl::GetWriteBuffer(PLAYER_STREAMTYPE_E type, uint8_t **pBuffer, uint32_t *nSize)
{
    HI_S32                  Ret;

    if ((PlayerState != HI_PLAYER_STATE_FAST) && (PlayerState != HI_PLAYER_STATE_PLAY))
    {
        *pBuffer = NULL;
        *nSize = 0;
        return -1;
    }

    if (CurStreamType == HI_PLAYER_STREAM_TYPE_TS)
    {
        HI_UNF_DMX_TSBUF_STATUS_S Status;
        Ret = HI_UNF_DMX_GetTSBufferStatus(hTsBuf, &Status);

        HI_U32 u32Aligenment = (*nSize % 188) ? (188 - *nSize % 188) : 0;   //补齐188字节
        if( (Status.u32UsedSize + *nSize + u32Aligenment) >= Status.u32BufSize){
            PLAYER_LOGI("GetWriteBuffer fail: UsedBuffer(%d)+RequestBuf(%d) >= TotalBuffer(%d).\n", Status.u32UsedSize, *nSize, Status.u32BufSize);
            *pBuffer = NULL;
            *nSize = 0;
            return -1;
        }

        Ret = HI_UNF_DMX_GetTSBuffer(hTsBuf, *nSize, &TsStreamBuf, 0);
        if ((HI_SUCCESS != Ret) || (TsStreamBuf.u32Size < *nSize))
        {
            *pBuffer = NULL;
            *nSize = 0;
            usleep(10000);
            return -1;
        }

        *pBuffer = TsStreamBuf.pu8Data;
        *nSize = TsStreamBuf.u32Size;
    }
    else if (CurStreamType == HI_PLAYER_STREAM_TYPE_ES)
    {
        if (type == PLAYER_STREAMTYPE_VIDEO)
        {
            Ret = HI_UNF_AVPLAY_GetBuf(hAvplay, HI_UNF_AVPLAY_BUF_ID_ES_VID, *nSize, &VidStreamBuf, 0);
            if ((HI_SUCCESS != Ret) || (VidStreamBuf.u32Size < *nSize))
            {
                *pBuffer = NULL;
                *nSize = 0;
                return -1;
            }

            *pBuffer = VidStreamBuf.pu8Data;
            *nSize = VidStreamBuf.u32Size;
        }
        else if (type == PLAYER_STREAMTYPE_AUDIO)
        {
            Ret = HI_UNF_AVPLAY_GetBuf(hAvplay, HI_UNF_AVPLAY_BUF_ID_ES_AUD, *nSize, &AudStreamBuf, 0);
            if ((HI_SUCCESS != Ret) || (AudStreamBuf.u32Size < *nSize))
            {
                *pBuffer = NULL;
                *nSize = 0;
                return -1;
            }

            *pBuffer = AudStreamBuf.pu8Data;
            *nSize = AudStreamBuf.u32Size;
        }
    }

    return 0;
}

/**
 * @brief   写入数据，和GetWriteBuffer对应使用
 * @param   type      - ts/video/audio
 *          pBuffer   - GetWriteBuffer得到的buffer地址
 *          nSize     - 写入的数据大小
 *          timestamp - ES Video/Audio对应的时间戳(90KHZ)
 * @return  0  - 成功
 *          -1 - 失败
 **/
int32_t CMCC_MediaProcessorImpl::WriteData(PLAYER_STREAMTYPE_E type, uint8_t *pBuffer, uint32_t nSize, uint64_t timestamp)
{
    HI_S32                  Ret;

    if ((PlayerState != HI_PLAYER_STATE_FAST) && (PlayerState != HI_PLAYER_STATE_PLAY))
    {
        return -1;
    }

    if (CurStreamType == HI_PLAYER_STREAM_TYPE_TS)
    {
        Ret = HI_UNF_DMX_PutTSBuffer(hTsBuf, nSize);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("Call HI_UNF_DMX_PutTSBuffer failed, error code:0x%x \n", Ret);
            return -1;
        }
    }
    else if (CurStreamType == HI_PLAYER_STREAM_TYPE_ES)
    {
        if (INVALID_PTS != timestamp)
            timestamp /= TIME_BASE;

        if (type == PLAYER_STREAMTYPE_VIDEO)
        {
			if(0)
			{
				HI_UNF_AVPLAY_PUTBUFEX_OPT_S  stExOpt = { HI_FALSE,HI_FALSE };
				stExOpt.bEndOfFrm = HI_TRUE;
				stExOpt.bContinue = HI_TRUE;
				Ret = HI_UNF_AVPLAY_PutBufEx(hAvplay, HI_UNF_AVPLAY_BUF_ID_ES_VID, nSize, timestamp,&stExOpt);
			}
			else
			{
				Ret = HI_UNF_AVPLAY_PutBuf(hAvplay, HI_UNF_AVPLAY_BUF_ID_ES_VID, nSize, timestamp);
			}

            if (HI_SUCCESS != Ret)
            {
                PLAYER_LOGE("Call HI_UNF_AVPLAY_BUF_ID_ES_VID failed, error code:0x%x \n", Ret);
                return -1;
            }
        }
        else if (type == PLAYER_STREAMTYPE_AUDIO)
        {
			if(0)
			{
				HI_UNF_AVPLAY_PUTBUFEX_OPT_S  stExOpt = { HI_FALSE,HI_FALSE };
				stExOpt.bEndOfFrm = HI_TRUE;
				stExOpt.bContinue = HI_TRUE;
				Ret = HI_UNF_AVPLAY_PutBufEx(hAvplay, HI_UNF_AVPLAY_BUF_ID_ES_AUD, nSize, timestamp,&stExOpt);
			}else{
			    Ret = HI_UNF_AVPLAY_PutBuf(hAvplay, HI_UNF_AVPLAY_BUF_ID_ES_AUD, nSize, timestamp);
			}

            if (HI_SUCCESS != Ret)
            {
                PLAYER_LOGE("Call HI_UNF_AVPLAY_BUF_ID_ES_AUD failed, error code:0x%x \n", Ret);
                return -1;
            }
        }
    }

    return 0;
}

/**
 * @brief   切换音轨
 * @param   pid         - ts播放的情况下，切换音轨时以pid为准
 *          pAudioPara  - es播放的情况下，切换音轨时传入的音频参数
 * @return  无
 **/
void CMCC_MediaProcessorImpl::SwitchAudioTrack(uint16_t pid, PAUDIO_PARA_T pAudioPara)
{
    PLAYER_LOGI("SwitchAudioTrack enter.\n");
    HI_S32                  Ret;

    if (INVALID_TS_PID == pid)
    {
        PLAYER_LOGE("Call SwitchAudioTrack fail, Pid is invalid");
        return;
    }

    PLAYER_LOCK();

    if ((HI_FALSE == bIsTsPlayReady)
      &&(HI_FALSE == bIsEsPlayReady))
    {
        PLAYER_LOGE("HW resource is not prepared !!!\n");
        PLAYER_UNLOCK();
        return;
    }

    Ret = HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_AUD_PID, &pid);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_SetAttr fail, 0x%x\n", Ret);
        PLAYER_UNLOCK();
        return;
    }

    PLAYER_LOGI("PID:%d\n", pid);
    PLAYER_UNLOCK();
    return ;
}

/**
 * @brief   当前播放是否结束
 * @param   无
 * @return  true  - 播放结束
 *          false - 播放未结束
 **/
bool CMCC_MediaProcessorImpl::GetIsEos()
{
    PLAYER_LOGI("GetIsEos enter.\n");
    HI_S32                          Ret;
    HI_U32                          u32CurVidPts  = 0xFFFFFFFFU;
    HI_U32                          u32CurAudPts  = 0xFFFFFFFFU;
    HI_U32                          u32CurSysTime = 0xFFFFFFFFU;
    HI_BOOL                         bIsEmpty;

    if ((HI_FALSE == bIsTsPlayReady)
      &&(HI_FALSE == bIsEsPlayReady))
    {
        return false;
    }

    if ((HI_PLAYER_STATE_PLAY != PlayerState)
        && (HI_PLAYER_STATE_FAST != PlayerState))
    {
        PLAYER_LOGI("PlayerState[%d] not Play or Fast, Not get Eos\n", PlayerState);
        return false;
    }

    if (HI_TRUE == AudEnable)
    {
        u32CurAudPts = AvplayStatusInfo.stSyncStatus.u32LastAudPts;
        if (u32CurAudPts == 0xFFFFFFFFU)
        {
            return false;
        }
    }

    if (HI_TRUE == VidEnable)
    {
        u32CurVidPts = AvplayStatusInfo.stSyncStatus.u32LastVidPts;
        if (u32CurVidPts == 0xFFFFFFFFU)
        {
            return false;
        }
    }
#if 0
    PLAYER_LOGI("CurAudPts = %d, PreAudPts = %d, CurVidPts = %d, PreVidPts = %d\n",
        u32CurAudPts, PreAudPts, u32CurVidPts, PreVidPts);
#endif
    if ((u32CurAudPts == PreAudPts)
      &&(u32CurVidPts == PreVidPts))
    {
        u32CurSysTime = Player_GetSysTime();
#if 0
        PLAYER_LOGI("CurSysTime = 0x%x, PreSysTime = 0x%x\n", u32CurSysTime, PreSysTime);
#endif
        if (((u32CurSysTime > PreSysTime) && ((u32CurSysTime - PreSysTime) > EOS_TIMEOUT))
          ||((u32CurSysTime < PreSysTime) && (((SYSTIME_MAX - PreSysTime) + u32CurSysTime) > EOS_TIMEOUT))
           )
        {
            PLAYER_LOGI("Ts Stream Transfer Over!!!\n");
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        PreAudPts = u32CurAudPts;
        PreVidPts = u32CurVidPts;
        PreSysTime = Player_GetSysTime();
        return false;
    }
}

/**
 * @brief   取得播放状态
 * @param   无
 * @return  PLAYER_STATE_e - 播放状态play/pause/stop
 **/
int32_t CMCC_MediaProcessorImpl::GetPlayMode()
{
    PLAYER_LOGI("GetPlayMode enter.\n");
	HI_PLAYER_STATE_E status;
    PLAYER_LOCK();
    status = PlayerState;
    PLAYER_LOGI("MediaDevice State:%d\n", PlayerState);
    PLAYER_UNLOCK();
    return status;

    
}

/**
 * @brief   取得当前播放的video pts(没有video的情况下，取audio pts)
 * @param   无
 * @return  pts - 当前播放的原始码流中的pts，ms为单位
 **/
int32_t CMCC_MediaProcessorImpl::GetCurrentPts()
{
    HI_S32                          Ret;
#if 0
    HI_UNF_AVPLAY_STATUS_INFO_S     stStatusInfo;

    //PLAYER_LOCK();

    stStatusInfo.stSyncStatus.u32LastAudPts = INVALID_PTS;
    stStatusInfo.stSyncStatus.u32LastVidPts = INVALID_PTS;

    Ret = HI_UNF_AVPLAY_GetStatusInfo(hAvplay, &stStatusInfo);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_GetStatusInfo, 0x%x \n", Ret);
        PLAYER_UNLOCK();
        return INVALID_PTS;
    }

    //PLAYER_UNLOCK();

    if (INVALID_PTS != stStatusInfo.stSyncStatus.u32LastVidPts)
    {
        return stStatusInfo.stSyncStatus.u32LastVidPts;
    }
    else if (INVALID_PTS != stStatusInfo.stSyncStatus.u32LastAudPts)
    {
        return stStatusInfo.stSyncStatus.u32LastAudPts;
    }
    else
    {
        return INVALID_PTS;
    }
#else
    if (INVALID_PTS != AvplayStatusInfo.stSyncStatus.u32LastVidPts)
    {
        return AvplayStatusInfo.stSyncStatus.u32LastVidPts;
    }
    else if (INVALID_PTS != AvplayStatusInfo.stSyncStatus.u32LastAudPts)
    {
        return AvplayStatusInfo.stSyncStatus.u32LastAudPts;
    }
    else
    {
        return INVALID_PTS;
    }

#endif
}

/**
 * @brief   取得视频的宽高
 * @param   *width  - 视频宽度
 *          *height - 视频高度
 * @return  无
 **/
void CMCC_MediaProcessorImpl::GetVideoPixels(int32_t *width, int32_t *height)
{
    PLAYER_LOGI("GetVideoPixels enter.\n");
    HI_S32                          Ret;
    HI_U32                          u32DispWidth;
    HI_U32                          u32DispHeight;
    HI_UNF_DISP_E                   enDisp = HI_UNF_DISPLAY1;

    Ret = HI_UNF_DISP_GetVirtualScreen(enDisp, &u32DispWidth, &u32DispHeight);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_DISP_GetVirtualScreen, 0x%x \n", Ret);
    }

    *width = u32DispWidth;
    *height = u32DispHeight;
    PLAYER_LOGI("width:%d, height:%d\n", u32DispWidth, u32DispHeight);
    return ;
}

/**
 * @brief   获取解码器缓冲区的大小和剩余数据的大小
 * @param   *totalsize - 解码器缓冲区的大小
 *          *datasize  - 剩余未播放数据的大小
 * @return  0  - 成功
 *          -1 - 失败
 **/
int CMCC_MediaProcessorImpl::GetBufferStatus(int32_t *totalsize, int32_t *datasize)
{
    //PLAYER_LOGI("GetBufferStatus enter.\n");
    HI_S32                          Ret;

#if 0
    HI_UNF_AVPLAY_STATUS_INFO_S     stStatusInfo;

    //PLAYER_LOCK();

    Ret = HI_UNF_AVPLAY_GetStatusInfo(hAvplay, &stStatusInfo);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_AVPLAY_GetStatusInfo, 0x%x \n", Ret);
        PLAYER_UNLOCK();
        return -1;
    }

    //PLAYER_UNLOCK();

    *datasize  = stStatusInfo.stBufStatus[HI_UNF_AVPLAY_BUF_ID_ES_VID].u32UsedSize;
    *totalsize = stStatusInfo.stBufStatus[HI_UNF_AVPLAY_BUF_ID_ES_VID].u32BufSize;
#else

    *datasize  = AvplayStatusInfo.stBufStatus[HI_UNF_AVPLAY_BUF_ID_ES_VID].u32UsedSize;
    *totalsize = AvplayStatusInfo.stBufStatus[HI_UNF_AVPLAY_BUF_ID_ES_VID].u32BufSize;

    HI_U32 winUnload;
    Ret = HI_MPI_WIN_GetUnloadTimes(hWindow, &winUnload);
    if (Ret < 0)
    {
        PLAYER_LOGE("GetBufferStatus HI_MPI_WIN_GetUnloadTimes error!!!\n");
        return -1;
    }
    if (winUnload != WinUnloadTime)
    {
        if(bStartStutter == HI_FALSE)
        {
            bStartStutter = HI_TRUE;
            g_callBackFunc(g_eventHandler, PLAYER_EVENT_STUTTER, 0, 0);
            g_callBackFunc(g_eventHandler, PLAYER_EVENT_UNDERFLOW_START, 0, 0);
            PLAYER_LOGE("Event: PLAYER_EVENT_STUTTER start\n");
        }
        WinUnloadTime = winUnload;
    }
    else
    {
        if(bStartStutter == HI_TRUE)
        {
            bStartStutter = HI_FALSE;
            g_callBackFunc(g_eventHandler, PLAYER_EVENT_UNDERFLOW_END, 0, 0);
            PLAYER_LOGE("Event: PLAYER_EVENT_STUTTER end\n");
        }
    }
#endif

    return 0;
}

/**
 * @brief   播放结束时是否保留最后一帧
 * @param   bHoldLastPic - true/false停止播放时是否保留最后一帧
 * @return  无
 **/
void CMCC_MediaProcessorImpl::SetStopMode(bool bHoldLastPic)
{
    PLAYER_LOGI("SetStopMode enter.\n");
    if (true == bHoldLastPic)
    {
        PlayerStopMode.enMode = HI_UNF_AVPLAY_STOP_MODE_STILL;
        PlayerStopMode.u32TimeoutMs = 0;
    }
    else
    {
        PlayerStopMode.enMode = HI_UNF_AVPLAY_STOP_MODE_BLACK;
        PlayerStopMode.u32TimeoutMs = 0;
    }

    PLAYER_LOGI("Stop Mode:%d\n", PlayerStopMode.enMode);
    return ;
}

/**
 * @brief   设置画面显示比例
 * @param   contentMode - 源比例/全屏，默认全屏
 * @return  无
 **/
void CMCC_MediaProcessorImpl::SetContentMode(PLAYER_CONTENTMODE_E contentMode)
{
    PLAYER_LOGI("SetContentMode enter.\n");
    HI_S32                      Ret;
    HI_UNF_VO_ASPECT_CVRS_E     enAspectCvrs;

    if ((contentMode <= PLAYER_CONTENTMODE_NULL) || (contentMode >= PLAYER_CONTENTMODE_MAX))
    {
        PLAYER_LOGE("ContentMode (%d) is invalid !!!\n", contentMode);
        return;
    }

    if (HI_TRUE == bIsWindowReady)
    {
        Ret = HI_UNF_VO_GetWindowAttr(hWindow, &WindowAttr);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("Call HI_UNF_VO_GetWindowAttr fail\n");
            return;
        }
    }

    Ret = AspectCvrs2HiType(contentMode, &enAspectCvrs);
    if (true != Ret)
    {
        PLAYER_LOGE("Call AspectCvrs2HiType fail\n");
        return;
    }

    WindowAttr.stWinAspectAttr.enAspectCvrs = enAspectCvrs;

    if (HI_TRUE == bIsWindowReady)
    {
        Ret = HI_UNF_VO_SetWindowAttr(hWindow, &WindowAttr);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("Call AspectCvrs2HiType fail\n");
            return;
        }
    }

    PLAYER_LOGI("Content Mode:%d\n", enAspectCvrs);
    return ;
}

/**
 * @brief   获取当前声道
 * @param   无
 * @return  1 - left channel
 *          2 - right channel
 *          3 - stereo
 **/
int32_t CMCC_MediaProcessorImpl::GetAudioBalance()
{
    PLAYER_LOGI("GetAudioBalance enter.\n");
    HI_S32                  Ret;
    HI_UNF_TRACK_MODE_E     AudioBalance = HI_UNF_TRACK_MODE_BUTT;

    Ret = HI_UNF_SND_GetTrackMode(HI_UNF_SND_0, HI_UNF_SND_OUTPUTPORT_DAC0, &AudioBalance);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_SND_GetTrackMode fail\n");
    }

    switch(AudioBalance)
    {
        case HI_UNF_TRACK_MODE_STEREO :
            Ret = PLAYER_CH_STEREO; /* 3 - stereo*/
            break;
        case HI_UNF_TRACK_MODE_DOUBLE_LEFT :
            Ret = PLAYER_CH_LEFT;   /* 1 - left channel*/
            break;
        case HI_UNF_TRACK_MODE_DOUBLE_RIGHT:
            Ret = PLAYER_CH_RIGHT;  /*2 - right channel*/
            break;
        default:
            Ret = false;
            break;
    }

    PLAYER_LOGI("AudioBalance:%d\n", Ret);
    return Ret;
}

/**
 * @brief   设置播放声道
 * @param   nAudioBanlance - 1.left channel;2.right channle;3.stereo
 * @return  true  - 成功
 *          false - 失败
 **/
bool CMCC_MediaProcessorImpl::SetAudioBalance(PLAYER_CH_E nAudioBalance)
{
    PLAYER_LOGI("SetAudioBalance enter.\n");
    HI_S32                  Ret;

    if((nAudioBalance <= PLAYER_CH_NULL) || (nAudioBalance >= PLAYER_CH_MAX))
    {
        PLAYER_LOGE("Call SetAudioBalance fail, nAudioBalance invalid\n");
        return false;
    }
    PLAYER_LOGI("nAudioBalance:%d\n", nAudioBalance);

    switch(nAudioBalance)
    {
        /* 1.left channel;*/
        case PLAYER_CH_LEFT:
            Ret = HI_UNF_SND_SetTrackChannelMode(hTrack, HI_UNF_TRACK_MODE_DOUBLE_LEFT);
            break;
        /* 2.right channle;*/
        case PLAYER_CH_RIGHT:
            Ret = HI_UNF_SND_SetTrackChannelMode(hTrack, HI_UNF_TRACK_MODE_DOUBLE_RIGHT);
            break;
        /* 3.stereo;*/
        case PLAYER_CH_STEREO:
            Ret = HI_UNF_SND_SetTrackChannelMode(hTrack, HI_UNF_TRACK_MODE_STEREO);
            break;
        default:
            Ret = false;
            break;
    }

    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("Call HI_UNF_SND_SetTrackMode fail\n");
        return false;
    }
    else
    {
        return true;
    }
}

/**
 * @brief   设置VideoSurfaceTexture,用于Android 4以上版本
 * @param   pViodeoSurfaceTexture - 播放图层
 *          注意：如果是应用于Android4.2的系统，请将指针类型转换为ISurfaceTexture
 *                如果是应用于Android4.4的系统，请将指针类型转换为IGraphicBufferProducer
 * @return  无
 **/
void CMCC_MediaProcessorImpl::SetSurfaceTexture(const void *pVideoSurfaceTexture)
{
    PLAYER_LOGI("SetSurfaceTexture enter.\n");


    status_t err;
    const sp<IGraphicBufferProducer> bufferProducer = (IGraphicBufferProducer*)pVideoSurfaceTexture;
    if (bufferProducer.get() != NULL)
    {
        err = setNativeWindow_l(new Surface(bufferProducer));
        PLAYER_LOGI("new Surface\n");
    }
    else
    {
        err = setNativeWindow_l(NULL);
    }
    if (err < 0)
        PLAYER_LOGE("SetSurfaceTexture error\n");

    return ;
}

bool CMCC_MediaProcessorImpl::IsUseSidebandMode(const sp<ANativeWindow>& native)
{
    HI_CHAR value[PROPERTY_VALUE_MAX] = {0};
    int FromSurfaceFlinger = -1;
    if (property_get("service.media.hiplayer.output", value, "sideband")
    && strcasecmp("sideband", value))
    {
        LOGW("service.media.hiplayer.output [%s]", value);
        return false;
    }
    if (native.get())
    {
        native->query(native.get(), NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER, &FromSurfaceFlinger);
    }
    if (FromSurfaceFlinger == 0)
    {
        return false;
    }
    return true;
}

HI_S32 CMCC_MediaProcessorImpl::SetSidebandHandle(const sp<ANativeWindow>& native)
{
    native_handle_t* pstSideBandHandle = NULL;
    HI_S32 s32Ret = 0;
    HI_HANDLE hWin = hWindow;
    HI_CHAR value[PROPERTY_VALUE_MAX] = {0};
    if (!mbSideBand)
    {
        return NO_ERROR;
    }
    if (mNativeWindow.get() && native.get() != mNativeWindow.get())
    {
        native_window_set_sideband_stream(mNativeWindow.get(), NULL);
    }
    if (native.get())
    {
        pstSideBandHandle = native_handle_create(0, 1);
        if (NULL == pstSideBandHandle)
        {
            LOGE("native_handle_create failed!");
            return UNKNOWN_ERROR;
        }
        pstSideBandHandle->data[0] = hWin;
        native_window_set_sideband_stream(native.get(), pstSideBandHandle);
        native_handle_delete(pstSideBandHandle);
    }
    return NO_ERROR;
}

status_t CMCC_MediaProcessorImpl::setNativeWindow_l(const sp<ANativeWindow>& native)
{
    HI_S32 ret;
    int FromSurfaceFlinger = -1;
    char value[PROPERTY_VALUE_MAX] = {0};
    if (NULL != native.get())
    {
        native->query(native.get(), NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER, &FromSurfaceFlinger);
    }

    if (IsUseSidebandMode(native))
    {
        SetSidebandHandle(native);
        mNativeWindow = native;
        return OK;
    }
    mNativeWindow = native;
    return OK;
}


/**
 * @brief   注册回调函数
 * @param   pfunc  - 回调函数
 *          handle - 回调函数对应的句柄
 * @return  无
 **/
void CMCC_MediaProcessorImpl::playback_register_evt_cb(void *handler, PLAYER_EVENT_CB pfunc)
{
    PLAYER_LOGI("playback_register_evt_cb enter.\n");
    m_eventHandler = handler;
    m_callBackFunc = pfunc;
    g_eventHandler = m_eventHandler;
    g_callBackFunc = m_callBackFunc;
    return;
}

/**
 * @brief   设置参数(从MediaPlayer透传下来)
 * @param   key     - 命令
 *          request - 具体参数
 * @return  无
 **/
void CMCC_MediaProcessorImpl::SetParameter(int32_t key, void* request)
{
    PLAYER_LOGI("setParameter enter.\n");
    Parcel* para = NULL;
    if(request == NULL){
        PLAYER_LOGE("setParameter request = NULL\n");
    } else {
        para = (Parcel*)request;
    }

    if(key == KEY_PARAMETER_VIDEO_POSITION_INFO){
        int left,right,top,bottom;
        Rect newRect;
        int off;
		size_t pos = para->dataPosition();
        const String16 uri16 = para->readString16();
        String8 keyStr = String8(uri16);
        PLAYER_LOGI("setParameter %d=[%s]\n",key,keyStr.string());
        left=getIntfromString8(keyStr,".left=");
        top=getIntfromString8(keyStr,".top=");
        right=getIntfromString8(keyStr,".right=");
        bottom=getIntfromString8(keyStr,".bottom=");
        newRect=Rect(left,top,right,bottom);
        LOGI("setParameter info to newrect=[%d,%d,%d,%d] datapos=%d\n",
                left,top,right,bottom,pos);

        //update surface setting
        SetVideoWindow(left,top,right-left,bottom-top);
    }
    else if (key == KEY_PARAMETER_SET_SYNC)
    {
        HI_S32 Ret;
        int mod, timeout;
        Ret = HI_UNF_AVPLAY_GetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_SYNC, &SyncAttr);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("call HI_UNF_AVPLAY_GetAttr fail, %d\n", Ret);
            return ;
        }

        const String16 uri16 = para->readString16();
        String8 keyStr = String8(uri16);
        PLAYER_LOGI("setParameter %d=[%s]\n",key,keyStr.string());
        mod = getIntfromString8(keyStr,".mod=");
        timeout = getIntfromString8(keyStr,".timeout=");
        LOGI("setParameter info to mod %d timeout %d\n",mod,timeout);

        if (mod == SYNC_MODE_SLOW)
        {
            SyncAttr.enSyncRef = HI_UNF_SYNC_REF_AUDIO;
            SyncAttr.u32PreSyncTimeoutMs = 0;
            Ret = HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_SYNC, &SyncAttr);
        }
        else if (mod == SYNC_MODE_TIMEWAIT)
        {
            SyncAttr.enSyncRef = HI_UNF_SYNC_REF_AUDIO;
            SyncAttr.u32PreSyncTimeoutMs = (timeout <= 0) ? 0 : (HI_U32)timeout;
            Ret = HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_SYNC, &SyncAttr);
        }
        else
        {
            return ;
        }
    }

    return ;
}

/**
 * @brief   获取参数(从MediaPlayer透传下来)
 * @param   key   - 命令
 *          reply - 具体参数
 * @return  无
 **/
void CMCC_MediaProcessorImpl::GetParameter(int32_t key, void* reply)
{
    PLAYER_LOGI("getParameter enter.\n");
    return ;
}

void CMCC_MediaProcessorImpl::SetVideoWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    PLAYER_LOGI("SetVideoWindow enter.\n");
    HI_S32 Ret;
    HI_UNF_VO_ASPECT_CVRS_E     enAspectCvrs;

    if ((x > VIDEO_SIZE_MAX_X)||(y > VIDEO_SIZE_MAX_Y)
      ||(width > VIDEO_SIZE_MAX_W)||(height > VIDEO_SIZE_MAX_H)
       )
    {
        PLAYER_LOGE("call SetVideoWindow fail\n");
        return;
    }

    enAspectCvrs = WindowAttr.stWinAspectAttr.enAspectCvrs;

    if (HI_TRUE == bIsWindowReady)
    {
        Ret = HI_UNF_VO_GetWindowAttr(hWindow, &WindowAttr);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("Call HI_UNF_VO_GetWindowAttr fail %d\n", Ret);
            return;
        }
    }

    WindowAttr.stOutputRect.s32X = x;
    WindowAttr.stOutputRect.s32Y = y;
    WindowAttr.stOutputRect.s32Width = width;
    WindowAttr.stOutputRect.s32Height = height;
    WindowAttr.stWinAspectAttr.enAspectCvrs = enAspectCvrs;

    if (HI_TRUE == bIsWindowReady)
    {
        Ret = HI_UNF_VO_SetWindowAttr(hWindow, &WindowAttr);
        if (HI_SUCCESS != Ret)
        {
            PLAYER_LOGE("Call AspectCvrs2HiType fail\n");
            return;
        }
    }

    return;
}


/**
 * @brief:������ʵ��
 **/
#ifndef _CMCC_MEDIAPROCESSORIMPL_H_
#define _CMCC_MEDIAPROCESSPRIMPL_H_

#include "CMCC_MediaProcessor.h"
#include "MediaFormat.h"
extern "C" {
#include "hi_type.h"
}


using namespace android;

#define PADPT_AUDIO_MAX_DMX_CHN           (32)        /**< sum of demux channel can be opened */
#define KEY_PARAMETER_SET_SYNC            (1234)

typedef enum hi_PLAYER_MEDIA_CHAN
{
    HI_PLAYER_MEDIA_CHAN_AUD  = 0x01,  /**<Audio channel*/
    HI_PLAYER_MEDIA_CHAN_VID  = 0x02,  /**<Video channel*/

    HI_PLAYER_MEDIA_CHAN_BUTT = 0x8
} HI_PLAYER_MEDIA_CHAN_E;

typedef enum hi_PLAYER_STREAM_TYPE_E
{
    HI_PLAYER_STREAM_TYPE_TS = 0,   /**<TS stream*/
    HI_PLAYER_STREAM_TYPE_ES,       /**<ES stream*/

    HI_PLAYER_STREAM_TYPE_BUTT
} HI_PLAYER_STREAM_TYPE_E;

typedef struct hiTS_PROGRAM_INFO_S
{
    VIDEO_PARA_T stVidStream;                                   /**< video stream info*/
    AUDIO_PARA_T astAudStream[PADPT_AUDIO_MAX_DMX_CHN];         /**< audio stream info */
    HI_U32       u32AudStreamNum;                               /**< audio stream number */
} TS_PROGRAM_INFO_S;

/** PLAYER state */
typedef enum hiTS_PLAYER_STATE_E
{
    HI_PLAYER_STATE_INI = 0,    /**< player in init state, when the player created, it is in init state*/
    HI_PLAYER_STATE_DEINI,      /**< player in deinit state*/
    HI_PLAYER_STATE_PLAY,       /**< player play state*/
    HI_PLAYER_STATE_FAST,       /**< player in FF or FB state*/
    HI_PLAYER_STATE_PAUSE,      /**< player in Pause state*/
    HI_PLAYER_STATE_STOP,       /**< player in Stop state*/
    HI_PLAYER_STATE_BUTT
} HI_PLAYER_STATE_E;

/**
 * @CMCC_MediaprocessorImpl
 */
class CMCC_MediaProcessorImpl : public CMCC_MediaProcessor
{

public:

    //����
    CMCC_MediaProcessorImpl();

    //����
    virtual ~CMCC_MediaProcessorImpl();

    //��ʼ����Ƶ����
    virtual void InitVideo(PVIDEO_PARA_T pVideoPara);

    //��ʼ����Ƶ����
    virtual void InitAudio(PAUDIO_PARA_T pAudioPara);

    //��ʼ����
    virtual bool StartPlay();

    //��ͣ
    virtual bool Pause();

    //��������
    virtual bool Resume();

    //�������
    virtual bool TrickPlay();

    //ֹͣ�������
    virtual bool StopTrickPlay();

    //ֹͣ
    virtual bool Stop();

    //��λ
    virtual bool Seek();

    //��ȡnSize��С��buffer
    virtual int32_t GetWriteBuffer(PLAYER_STREAMTYPE_E type, uint8_t **pBuffer, uint32_t *nSize);

    //д�����ݣ���GetWriteBuffer��Ӧʹ��
    virtual int32_t WriteData(PLAYER_STREAMTYPE_E type, uint8_t *pBuffer, uint32_t nSize, uint64_t timestamp);

    //�л�����
    virtual void SwitchAudioTrack(uint16_t pid, PAUDIO_PARA_T pAudioPara);

    //��ǰ�����Ƿ����
    virtual bool GetIsEos();

    //ȡ�ò���״̬
    virtual int32_t GetPlayMode();

    //ȡ�õ�ǰ���ŵ�video pts(û��video������£�ȡaudio pts)
    virtual int32_t GetCurrentPts();

    //ȡ����Ƶ�Ŀ��
    virtual void GetVideoPixels(int32_t *width, int32_t *height);

    //��ȡ�������������Ĵ�С��ʣ�����ݵĴ�С
    virtual int32_t GetBufferStatus(int32_t *totalsize, int32_t *datasize);

    //���Ž���ʱ�Ƿ������һ֡
    virtual void SetStopMode(bool bHoldLastPic);

    //���û�����ʾ����
    virtual void SetContentMode(PLAYER_CONTENTMODE_E contentMode);

    //��ȡ��ǰ����
    virtual int32_t GetAudioBalance();

    //���ò�������
    virtual bool SetAudioBalance(PLAYER_CH_E nAudioBalance);

    //����VideoSurfaceTexture,����Android 4���ϰ汾
    //�����Ӧ����Android4.2��ϵͳ���뽫ָ������ת��ΪISurfaceTexture
    //�����Ӧ����Android4.4��ϵͳ���뽫ָ������ת��ΪIGraphicBufferProducer
    virtual void SetSurfaceTexture(const void *pVideoSurfaceTexture);

    //ע��ص�����
    virtual void playback_register_evt_cb(void *handler, PLAYER_EVENT_CB pfunc);

    //���ò���(��MediaPlayer͸������)
    virtual void SetParameter(int32_t key, void* request);

    //��ȡ����(��MediaPlayer͸������)
    virtual void GetParameter(int32_t key, void* reply);

    virtual void SetVideoWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

    bool IsUseSidebandMode(const sp<ANativeWindow>& native);
private:
    PLAYER_EVENT_CB             m_callBackFunc;
    void                        *m_eventHandler;

    HI_HANDLE                   hAvplay;
    HI_HANDLE                   hWindow;
    HI_HANDLE                   hDmxId;
    HI_HANDLE                   hTsBuf;
    HI_HANDLE                   hTrack;

    HI_BOOL                     AudEnable;
    HI_BOOL                     VidEnable;
    HI_BOOL                     bIsPrepared;
    HI_BOOL                     bIsFirVidFrm;
    HI_BOOL                     bIsAudChnReady;
    HI_BOOL                     bIsVidChnReady;
    HI_BOOL                     bIsDmxReady;
    HI_BOOL                     bIsTrackReady;
    HI_BOOL                     bIsWindowReady;
    HI_BOOL                     bIsEsPlayReady;
    HI_BOOL                     bIsTsPlayReady;
    HI_BOOL                     bStatusThreadQuit;

    HI_PLAYER_STREAM_TYPE_E     LstStreamType;
    HI_PLAYER_STREAM_TYPE_E     CurStreamType;
    HI_UNF_WINDOW_ATTR_S        WindowAttr;
    HI_UNF_AVPLAY_ATTR_S        AvplayAttr;
    HI_UNF_SYNC_ATTR_S          SyncAttr;
    HI_UNF_SND_GAIN_ATTR_S      SndGainAttr;
    HI_UNF_AUDIOTRACK_ATTR_S    TrackAttr;

    HI_UNF_STREAM_BUF_S         TsStreamBuf;
    HI_UNF_STREAM_BUF_S         AudStreamBuf;
    HI_UNF_STREAM_BUF_S         VidStreamBuf;
    HI_UNF_AVPLAY_STOP_OPT_S    PlayerStopMode;

    TS_PROGRAM_INFO_S           ProgramInfo;
    HI_PLAYER_STATE_E           PlayerState;

    HI_UNF_AVPLAY_STATUS_INFO_S AvplayStatusInfo;

    pthread_t                   GetStatusThd;

    HI_U32                      PreSysTime;
    HI_U32                      PreAudPts;
    HI_U32                      PreVidPts;

    HI_U8                       u8DecOpenBuf[1024];
    HI_BOOL                     bStartStutter;
    HI_U32                      WinUnloadTime;
    HI_BOOL                     bunderflow;

    HI_S32                      MediaDeviceInit();
    HI_S32                      MediaDeviceDeInit();
    HI_S32                      MediaDeviceStart();
    HI_S32                      MediaDeviceStop();
    HI_S32                      MediaDeviceCreate(HI_PLAYER_STREAM_TYPE_E eStreamType);
    HI_S32                      MediaDeviceDestroy();
    HI_S32                      InitWinAttr(HI_UNF_WINDOW_ATTR_S* pWinAttr);
    HI_S32                      SetAudStream();
    HI_S32                      StatusThread();
    HI_S32                      HIADP_VO_Init(HI_UNF_VO_DEV_MODE_E enDevMode);
    HI_S32                      HIADP_VO_DeInit();
    HI_S32                      HIADP_Disp_Init();
    HI_S32                      HIADP_Disp_DeInit();
    HI_S32                      HIADP_Snd_Init();
    HI_S32                      HIADP_Snd_DeInit();
    HI_S32                      HIADP_AVPLAY_RegEvent();
    HI_S32                      HIADP_AVPLAY_UnRegEvent();
    HI_S32                      HIADP_AVPLAY_ChnOpen(HI_PLAYER_MEDIA_CHAN_E eChn);
    HI_S32                      HIADP_AVPLAY_ChnClose(HI_PLAYER_MEDIA_CHAN_E eChn);
    HI_S32                      HIADP_AVPLAY_AttachWindow();
    HI_S32                      HIADP_AVPLAY_DetachWindow();
    HI_S32                      HIADP_AVPLAY_AttachSound();
    HI_S32                      HIADP_AVPLAY_DetachSound();
    HI_S32                      HIADP_AVPLAY_Start(HI_PLAYER_MEDIA_CHAN_E eChn, HI_PLAYER_STREAM_TYPE_E eStreamType);
    HI_S32                      HIADP_AVPLAY_Stop();
    HI_S32                      HIADP_AVPLAY_Create(HI_PLAYER_STREAM_TYPE_E eStreamType);
    HI_S32                      HIADP_AVPLAY_Destroy();
    HI_S32                      HIADP_AVPLAY_RegADecLib();
    HI_S32                      HIADP_DMX_AttachTSPort();
    HI_S32                      HIADP_DMX_DetachTSPort();
    HI_S32                      HIADP_AVPLAY_SetVdecAttr(HI_HANDLE hAvplay,HI_UNF_VCODEC_TYPE_E enType,HI_UNF_VCODEC_MODE_E enMode);
    HI_VOID                     HIADP_AVPLAY_GetAdecAttr(HI_UNF_ACODEC_ATTR_S *pstAdecAttr, HI_U8  *pu8Extradata, HI_BOOL bIsBigEndian);
    HI_VOID*                    GetStatusInfo(HI_VOID *pArg);
    sp<ANativeWindow> mNativeWindow;
    HI_BOOL     mbSideBand;
    HI_S32 mSurfaceType;
    HI_S32 SetSidebandHandle(const sp<ANativeWindow>& native);
    status_t setNativeWindow_l(const sp<ANativeWindow>& native);
};

#endif

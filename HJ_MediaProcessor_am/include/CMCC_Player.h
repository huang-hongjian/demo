#ifndef _TSPLAYER_H_
#define _TSPLAYER_H_
#include <android/log.h>    
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <gui/Surface.h>

using namespace android;
extern "C" {
#include <amports/vformat.h>
#include <amports/aformat.h>
#include <amports/amstream.h>
#include <codec.h>
#include <codec_info.h>
}

#include <string.h>
#include <utils/Timers.h>
#include <MediaFormat.h>
#include <CMCC_MediaProcessor.h>

#define lock_t          pthread_mutex_t
#define lp_lock_init(x,v)   pthread_mutex_init(x,v)
#define lp_lock(x)      pthread_mutex_lock(x)
#define lp_unlock(x)    pthread_mutex_unlock(x)

#define TRICKMODE_NONE       0x00
#define TRICKMODE_I          0x01
#define TRICKMODE_FFFB       0x02
#define MAX_AUDIO_PARAM_SIZE 10

class CmccPlayer
{
public:
	CmccPlayer();
	virtual ~CmccPlayer();
public:
	//取得播放模式
	virtual int  GetPlayMode();
	//初始化视频参数
	virtual void InitVideo(PVIDEO_PARA_T pVideoPara);
	//初始化音频参数
	virtual void InitAudio(PAUDIO_PARA_T pAudioPara);
	//
	//开始播放
	virtual bool StartPlay();
    virtual int32_t GetWriteBuffer(PLAYER_STREAMTYPE_E type, uint8_t **pBuffer, uint32_t *nSize);
	//把ts流写入
    virtual int32_t WriteData(PLAYER_STREAMTYPE_E type, uint8_t *pBuffer, uint32_t nSize, uint64_t timestamp);
	//暂停
	virtual bool Pause();
	//继续播放
	virtual bool Resume();
	//快进快退
	virtual bool Fast();
	//停止快进快退
	virtual bool StopFast();
	//停止
	virtual bool Stop();
	//定位
    virtual bool Seek();
	//获取当前声道
	virtual int GetAudioBalance();
	//设置声道
	virtual bool SetAudioBalance(int nAudioBalance);
    virtual int32_t GetCurrentPts();
	//获取视频分辩率
	virtual void GetVideoPixels(int& width, int& height);

    virtual int GetBufferStatus(int32_t *totalsize, int32_t *datasize);
    virtual void SetStopMode(bool bHoldLastPic);
    virtual void SetContentMode(PLAYER_CONTENTMODE_E contentMode);
    virtual void SetSurface(Surface* pSurface);

    virtual void SetSurfaceTexture(const sp<IGraphicBufferProducer>& bufferProducer);

	//16位色深需要设置colorkey来透出视频；

    virtual void SwitchAudioTrack(int pid);
    
    virtual long GetCurrentPlayTime();
    
    virtual void leaveChannel();
	virtual void playback_register_evt_cb(void *hander, PLAYER_EVENT_CB pfunc);
    virtual void update_nativewindow();
	virtual void SetParameter(int key, const Parcel &request);
	virtual void GetParameter(int key, Parcel *reply);
	
protected:
	int		m_bLeaveChannel;
	
private:
    bool mIsEsVideo;
    bool mIsEsAudio;
    bool mHaveVideo;
    bool mHaveAudio;
    bool mIsTsStream;
    codec_para_t *m_vpcodec;
    codec_para_t *m_apcodec;
    codec_para_t m_vcodec_para;
    codec_para_t m_acodec_para;
    struct codec_quality_info pquality_info;	
    int underflow_report_flag=0;
    int blur_report_flag=0;
    uint8_t *mEsVideoInputBuffer;
    uint8_t *mEsAudioInputBuffer;
    uint8_t *mTsInputBuffer;
    bool mStopThread;
    sp<ANativeWindow> mNativeWindow;
    class CheckBuffLevelThread: public Thread {
	public:
        enum dt_statue_t {
            STATUE_STOPPED,
            STATUE_RUNNING,
            STATUE_PAUSE
        };
		CheckBuffLevelThread(CmccPlayer* player);
		int32_t  start ();
		int32_t  stop ();
		int32_t  pause ();
		virtual bool threadLoop();
		dt_statue_t mTaskStatus;
	private:
		CmccPlayer* mPlayer;
	};
    sp<CheckBuffLevelThread> mCheckBuffLevelThread;
    friend class CheckBuffLevelThread;
    AUDIO_PARA_T mAudioPara;
	VIDEO_PARA_T mVideoPara;
	codec_para_t m_Tscodec_para;
	codec_para_t *m_Tspcodec;
	bool		m_bIsPlay;
	int			m_nAudioBalance;
	bool	m_bFast;
    PLAYER_EVENT_CB m_pfunc_play_evt;
    void *m_play_evt_hander;
	int64_t  m_StartPlayTimePoint;
    lock_t mutex;
    pthread_t mThread;
    virtual void checkBuffLevel();
    bool checkIfVodEos();
    bool threadCheckAbend();
    void updateCMCCInfo();	
    bool    m_isBlackoutPolicy;
    bool mFisrtPtsReady;//first video diaplayed
    int64_t    mUpdateInterval_us;
    AM_PLAYER_STATE_E           mPlayerState;
    bool m_bVideoIinit;
    bool m_bAudioIinit;
    bool m_bSeekFlag;
};

#endif

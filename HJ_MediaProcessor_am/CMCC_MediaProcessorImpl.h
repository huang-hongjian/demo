/**
 * @brief:解码器实现
 **/
#ifndef _CMCC_MEDIAPROCESSORIMPL_H_
#define _CMCC_MEDIAPROCESSPRIMPL_H_

#include <CMCC_MediaControl.h>

#include "CMCC_MediaProcessor.h"

using namespace android;

/**
 * @CMCC_MediaprocessorImpl
 */
class CMCC_MediaProcessorImpl : public CMCC_MediaProcessor
{
protected:

    CMCC_MediaControl * cmcc_MediaControl;

public:
	
	//构造
	CMCC_MediaProcessorImpl();
	
	//析构
	virtual ~CMCC_MediaProcessorImpl();
	
	//初始化视频参数
	virtual void InitVideo(PVIDEO_PARA_T pVideoPara);
	
	//初始化音频参数
	virtual void InitAudio(PAUDIO_PARA_T pAudioPara);
	
	//开始播放	
	virtual bool StartPlay();
	
	//暂停	
	virtual bool Pause();
	
	//继续播放
	virtual bool Resume();
	
	//快进快退
	virtual bool TrickPlay();
	
	//停止快进快退
	virtual bool StopTrickPlay();
	
	//停止
	virtual bool Stop();
	
	//定位
    virtual bool Seek();
	
	//获取nSize大小的buffer
	virtual int32_t	GetWriteBuffer(PLAYER_STREAMTYPE_E type, uint8_t **pBuffer, uint32_t *nSize);
	
	//写入数据，和GetWriteBuffer对应使用 
	virtual int32_t WriteData(PLAYER_STREAMTYPE_E type, uint8_t *pBuffer, uint32_t nSize, uint64_t timestamp);
	
	//切换音轨
	virtual void SwitchAudioTrack(uint16_t pid, PAUDIO_PARA_T pAudioPara);
	
	//当前播放是否结束
	virtual bool GetIsEos();
	
	//取得播放状态
	virtual int32_t	GetPlayMode();
	
	//取得当前播放的video pts(没有video的情况下，取audio pts)
	virtual int32_t GetCurrentPts();
	
	//取得视频的宽高	
	virtual void GetVideoPixels(int32_t *width, int32_t *height);
	
	//获取解码器缓冲区的大小和剩余数据的大小
    virtual int32_t GetBufferStatus(int32_t *totalsize, int32_t *datasize);
	
	//播放结束时是否保留最后一帧
    virtual void SetStopMode(bool bHoldLastPic);
	
	//设置画面显示比例
    virtual void SetContentMode(PLAYER_CONTENTMODE_E contentMode);
	
	//获取当前声道
	virtual int32_t GetAudioBalance();
	
	//设置播放声道
	virtual bool SetAudioBalance(PLAYER_CH_E nAudioBalance);
	
	//设置VideoSurfaceTexture,用于Android 4以上版本

	virtual void SetSurfaceTexture(const void *pVideoSurfaceTexture);
	
	//注册回调函数	
	virtual void playback_register_evt_cb(void *handler, PLAYER_EVENT_CB pfunc);
	
	//设置参数(从MediaPlayer透传下来)	
	virtual void SetParameter(int key, const Parcel &request);
	
	//获取参数(从MediaPlayer透传下来)	
	virtual void GetParameter(int key, Parcel *reply);
private:
    PLAYER_EVENT_CB  m_callBackFunc;
	void		     *m_eventHandler;
};

#endif

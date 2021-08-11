/**
 * @brief:解码器实现
 **/
#define LOG_NDEBUG 0 
#define LOG_NDDEBUG 0
#define LOG_TAG "CMCC_MediaProcessorImpl"

#include <utils/Log.h>
#include <utils/Vector.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <binder/Parcel.h>
#include <cutils/properties.h>

#include "CMCC_MediaProcessorImpl.h"

using namespace android;
using android::status_t;

/**
 * @brief	获取当前模块的版本号
 * @param	无
 * @return	版本号
 **/
int Get_CMCCMediaProcessorVersion()
{
    return 1;
}

/**
 * @brief	获取Mediaprocessor对象
 * @param	无	
 * @return  CMCC_MediaProcessor对象
 **/
CMCC_MediaProcessor* Get_CMCCMediaProcessor()
{
    ALOGV("Get_CMCCMediaProcessor");
    return new CMCC_MediaProcessorImpl;
}

/**
 * @brief	构造函数
 * @param	无	
 * @return
 **/
CMCC_MediaProcessorImpl::CMCC_MediaProcessorImpl()
{
    ALOGV("CMCC_MediaProcessorImpl");
    cmcc_MediaControl = GetMediaControl();

}

/**
 * @brief	析构函数	
 * @param	无	
 * @return  
 **/
CMCC_MediaProcessorImpl::~CMCC_MediaProcessorImpl()
{
    ALOGV("~CMCC_MediaProcessorImpl");
    cmcc_MediaControl->~CMCC_MediaControl();

}

/**
 * @brief	初始化视频参数
 * @param	pVideoPara - 视频相关参数，采用ES方式播放时，pid=0xffff; 没有video的情况下，vFmt=VFORMAT_UNKNOWN
 * @return	无
 **/
void CMCC_MediaProcessorImpl::InitVideo(PVIDEO_PARA_T pVideoPara)
{
    ALOGV("InitVideo");
    cmcc_MediaControl->InitVideo(pVideoPara);
	return ;
}

/**
 * @brief	初始化音频参数
 * @param	pAudioPara - 音频相关参数，采用ES方式播放时，pid=0xffff; 没有audio的情况下，aFmt=AFORMAT_UNKNOWN
 * @return	无
 **/
void CMCC_MediaProcessorImpl::InitAudio(PAUDIO_PARA_T pAudioPara)
{
	ALOGV("InitAudio");
    cmcc_MediaControl->InitAudio(pAudioPara);
	return ;
}	

/**
 * @brief	开始播放	
 * @param	无
 * @return	true  - 成功
 * 			false - 失败
 **/
bool CMCC_MediaProcessorImpl::StartPlay()
{
    ALOGV("StartPlay");
    bool result = cmcc_MediaControl->StartPlay();
	return result;
}

/**
 * @brief	暂停	
 * @param	无	
 * @return	true  - 成功
 * 			false - 失败
 **/
bool CMCC_MediaProcessorImpl::Pause()
{
    ALOGV("Pause");
    bool result = cmcc_MediaControl->Pause();
	return result;
}

/**
 * @brief	继续播放
 * @param	无
 * @return	true  - 成功
 * 			false - 失败
 **/
bool CMCC_MediaProcessorImpl::Resume()
{
    ALOGV("Resume");
    bool result = cmcc_MediaControl->Resume();
	return result;
}

/**
 * @brief	快进快退
 * @param	无	
 * @return	true  - 成功
 * 			false - 失败
 **/
bool CMCC_MediaProcessorImpl::TrickPlay()
{
    ALOGV("TrickPlay");
    bool result = cmcc_MediaControl->Fast();	
    return result;
}

/**
 * @brief	停止快进快退
 * @param	无	
 * @return	true  - 成功
 * 			false - 失败
 **/
bool CMCC_MediaProcessorImpl::StopTrickPlay()
{
	ALOGV("StopTrickPlay");
    bool result = cmcc_MediaControl->StopFast();
	return result;
}

/**
 * @brief	停止
 * @param	无	
 * @return	true  - 成功
 * 			false - 失败
 **/
bool CMCC_MediaProcessorImpl::Stop()
{
	ALOGV("Stop");
    bool result = cmcc_MediaControl->Stop();
	return result;
}

/**
 * @brief	定位
 * @param	无	
 * @return	true  - 成功
 * 			false - 失败
 **/
bool CMCC_MediaProcessorImpl::Seek()
{
	ALOGV("Seek");
    bool result = cmcc_MediaControl->Seek();
	return result;
}

/**
 * @brief	获取nSize大小的buffer
 * @param	type    - ts/video/audio
 * 			pBuffer - buffer地址，如果有nSize大小的空间，则将置为对应的地址;如果不足nSize，置NULL
 * 			nSize   - 需要的buffer大小
 * @return	0  - 成功
 * 			-1 - 失败
 **/
int32_t CMCC_MediaProcessorImpl::GetWriteBuffer(PLAYER_STREAMTYPE_E type, uint8_t **pBuffer, uint32_t *nSize)
{
	//ALOGV("GetWriteBuffer type:%d\n", type);
    int32_t result = cmcc_MediaControl->GetWriteBuffer(type, pBuffer, nSize);
	return result;
}

/**
 * @brief	写入数据，和GetWriteBuffer对应使用 
 * @param	type      - ts/video/audio
 * 			pBuffer   - GetWriteBuffer得到的buffer地址 
 * 			nSize     - 写入的数据大小
 * 			timestamp - ES Video/Audio对应的时间戳(90KHZ)
 * @return	0  - 成功
 * 			-1 - 失败
 **/
int32_t CMCC_MediaProcessorImpl::WriteData(PLAYER_STREAMTYPE_E type, uint8_t *pBuffer, uint32_t nSize, uint64_t timestamp)
{
	//ALOGV("WriteData, type:%d, nSize:%d", type, nSize);
    int32_t result = cmcc_MediaControl->WriteData(type, pBuffer, nSize, timestamp);
	return result;
}

/**
 * @brief	切换音轨
 * @param	pid			- ts播放的情况下，切换音轨时以pid为准 
 * 			pAudioPara	- es播放的情况下，切换音轨时传入的音频参数
 * @return	无
 **/
void CMCC_MediaProcessorImpl::SwitchAudioTrack(uint16_t pid, PAUDIO_PARA_T pAudioPara)
{
	ALOGV("SwitchAudioTrack");
	return ;
}

/**
 * @brief	当前播放是否结束
 * @param	无	
 * @return	true  - 播放结束
 * 			false - 播放未结束
 **/
bool CMCC_MediaProcessorImpl::GetIsEos()
{
	ALOGV("GetIsEos");
	return false;
}

/**
 * @brief	取得播放状态
 * @param	无	
 * @return	PLAYER_STATE_e - 播放状态play/pause/stop 
 **/
int32_t	CMCC_MediaProcessorImpl::GetPlayMode()
{
	ALOGV("GetPlayMode");
    
	return cmcc_MediaControl->GetPlayMode();
}

/**
 * @brief	取得当前播放的video pts(没有video的情况下，取audio pts)
 * @param	无	
 * @return	pts - 当前播放的原始码流中的pts，ms为单位
 **/
int32_t CMCC_MediaProcessorImpl::GetCurrentPts()
{
	//ALOGV("GetCurrentPts");
	return cmcc_MediaControl->GetCurrentPts();
}

/**
 * @brief	取得视频的宽高	
 * @param	*width  - 视频宽度
 * 			*height - 视频高度
 * @return	无
 **/
void CMCC_MediaProcessorImpl::GetVideoPixels(int32_t *width, int32_t *height)
{
	ALOGV("GetCurrentPts");
	return ;
}

/**
 * @brief	获取解码器缓冲区的大小和剩余数据的大小
 * @param	*totalsize - 解码器缓冲区的大小
 * 			*datasize  - 剩余未播放数据的大小
 * @return	0  - 成功
 * 			-1 - 失败
 **/
int CMCC_MediaProcessorImpl::GetBufferStatus(int32_t *totalsize, int32_t *datasize)
{
	//ALOGV("GetBufferStatus");
    return cmcc_MediaControl->GetBufferStatus(totalsize, datasize);
}

/**
 * @brief	播放结束时是否保留最后一帧
 * @param	bHoldLastPic - true/false停止播放时是否保留最后一帧 
 * @return	无
 **/
void CMCC_MediaProcessorImpl::SetStopMode(bool bHoldLastPic)
{
	ALOGV("SetStopMode");
    cmcc_MediaControl->SetStopMode(bHoldLastPic);
	return ;
}

/**
 * @brief	设置画面显示比例
 * @param	contentMode - 源比例/全屏，默认全屏
 * @return	无
 **/
void CMCC_MediaProcessorImpl::SetContentMode(PLAYER_CONTENTMODE_E contentMode)
{
	ALOGV("SetContentMode");
    cmcc_MediaControl->SetContentMode(contentMode);
	return ;
}

/**
 * @brief	获取当前声道
 * @param	无	
 * @return	1 - left channel
 * 			2 - right channel
 * 			3 - stereo
 **/
int32_t CMCC_MediaProcessorImpl::GetAudioBalance()
{
	ALOGV("GetAudioBalance");
	return 0;
}

/**
 * @brief	设置播放声道
 * @param	nAudioBanlance - 1.left channel;2.right channle;3.stereo
 * @return	true  - 成功
 * 			false - 失败
 **/
bool CMCC_MediaProcessorImpl::SetAudioBalance(PLAYER_CH_E nAudioBalance)
{
	ALOGV("SetAudioBalance");
	return false;
}

/**
 * @brief	设置VideoSurfaceTexture,用于Android 4以上版本
 * @param	pSurfaceTexture - VideoSurfaceTexture<Android 4.2及以下>
 * 			bufferProducer - GraphicBufferProducer<Android 4.4及以上>
 * @return	无
 **/
void CMCC_MediaProcessorImpl::SetSurfaceTexture(const void *pVideoSurfaceTexture)
{
	ALOGV("SetSurfaceTexture");
    sp<IGraphicBufferProducer> cp((IGraphicBufferProducer*)pVideoSurfaceTexture);
	cmcc_MediaControl->SetSurfaceTexture(cp);
	return ;
}

/**
 * @brief	注册回调函数	
 * @param	pfunc  - 回调函数
 * 			handle - 回调函数对应的句柄  
 * @return	无
 **/
void CMCC_MediaProcessorImpl::playback_register_evt_cb(void *handler, PLAYER_EVENT_CB pfunc)
{
	ALOGV("playback_register_evt_cb");
	m_eventHandler = handler;
	m_callBackFunc = pfunc;
    cmcc_MediaControl->playback_register_evt_cb(handler, pfunc);
	return ;
}

/**
 * @brief	设置参数(从MediaPlayer透传下来)	
 * @param	key 	- 命令
 * 			request - 具体参数
 * @return	无
 **/
void CMCC_MediaProcessorImpl::SetParameter(int key, const Parcel &request)
{
	ALOGV("CMCC_MediaProcessorImpl SetParameter key:%d",key);
	cmcc_MediaControl->SetParameter(key, request);
	return ;
}

/**
 * @brief	获取参数(从MediaPlayer透传下来)
 * @param	key   - 命令
 * 			reply - 具体参数
 * @return	无
 **/
void CMCC_MediaProcessorImpl::GetParameter(int key, Parcel *reply)
{
	ALOGV("CMCC_MediaProcessorImpl GetParameter key:%d",key);
	cmcc_MediaControl->GetParameter(key, reply);
	return ;
}

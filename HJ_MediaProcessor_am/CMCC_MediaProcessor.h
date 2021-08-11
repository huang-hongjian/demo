/**
 * @brief: 解码器基类
 **/
#ifndef _CMCC_MEDIAPROCESSOR_H_
#define _CMCC_MEDIAPROCESSOR_H_

extern "C" {
#include "MediaFormat.h"
}

#include <gui/Surface.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>

using namespace android;

/**
 * @brief 				解码器事件回调处理函数
 * @param handler  	 	播放器句柄
 * @param evt		 	事件类型 
 * @param param1 		事件参数
 * @param param2 		事件参数
 **/
typedef void (*PLAYER_EVENT_CB)(void *handler, PLAYER_EVENT_E evt, uint32_t param1, uint32_t param2);

/**
 * @CMCC_Mediaprocessor
 */
class CMCC_MediaProcessor{
public:
	/**
	 * @brief	构造函数
	 * @param	无	
	 * @return	无
	 **/
	CMCC_MediaProcessor(){}
	
	/**
	 * @brief	析构函数	
	 * @param	无
	 * @return	无
	 **/
	virtual ~CMCC_MediaProcessor(){}

	/**
	 * @brief	初始化视频参数
	 * @param	pVideoPara - 视频相关参数，采用ES方式播放时，pid=0xffff; 没有video的情况下，vFmt=VFORMAT_UNKNOWN
	 * @return	无
	 **/
	virtual void InitVideo(PVIDEO_PARA_T pVideoPara) = 0;
	
	/**
	 * @brief	初始化音频参数
	 * @param	pAudioPara - 音频相关参数，采用ES方式播放时，pid=0xffff; 没有audio的情况下，aFmt=AFORMAT_UNKNOWN
	 * @return	无
	 **/
	virtual void InitAudio(PAUDIO_PARA_T pAudioPara) = 0;
	
	/**
	 * @brief	开始播放	
	 * @param	无
	 * @return	true  - 成功
	 * 			false - 失败
	 **/
	virtual bool StartPlay() = 0;
	
	/**
	 * @brief	暂停	
	 * @param	无	
	 * @return	true  - 成功
	 * 			false - 失败
	 **/
	virtual bool Pause() = 0;
	
	/**
	 * @brief	继续播放
	 * @param	无
	 * @return	true  - 成功
	 * 			false - 失败
	 **/
	virtual bool Resume() = 0;
	
	/**
	 * @brief	快进快退
	 * @param	无	
	 * @return	true  - 成功
	 * 			false - 失败
	 **/
	virtual bool TrickPlay() = 0;
	
	/**
	 * @brief	停止快进快退
	 * @param	无	
	 * @return	true  - 成功
	 * 			false - 失败
	 **/
	virtual bool StopTrickPlay() = 0;
	
	/**
	 * @brief	停止
	 * @param	无	
	 * @return	true  - 成功
	 * 			false - 失败
	 **/
	virtual bool Stop() = 0;
	
	/**
	 * @brief	定位
	 * @param	无	
	 * @return	true  - 成功
	 * 			false - 失败
	 **/
    virtual bool Seek() = 0;
	
	/**
	 * @brief	获取nSize大小的buffer
	 * @param	type    - ts/video/audio
	 * 			pBuffer - buffer地址，如果有nSize大小的空间，则将置为对应的地址;如果不足nSize，置NULL
	 * 			nSize   - 需要的buffer大小
	 * @return	0  - 成功
	 * 			-1 - 失败
	 **/
	virtual int32_t	GetWriteBuffer(PLAYER_STREAMTYPE_E type, uint8_t **pBuffer, uint32_t *nSize) = 0;
	
	/**
	 * @brief	写入数据，和GetWriteBuffer对应使用 
	 * @param	type      - ts/video/audio
	 * 			pBuffer   - GetWriteBuffer得到的buffer地址 
	 * 			nSize     - 写入的数据大小
	 * 			timestamp - ES Video/Audio对应的时间戳(90KHZ)
	 * @return	0  - 成功
	 * 			-1 - 失败
	 **/
	virtual int32_t WriteData(PLAYER_STREAMTYPE_E type, uint8_t *pBuffer, uint32_t nSize, uint64_t timestamp) = 0;
	
	/**
	 * @brief	切换音轨
	 * @param	pid			- ts播放的情况下，切换音轨时以pid为准 
	 * 			pAudioPara	- es播放的情况下，切换音轨时传入以音频参数为准
	 * @return	无
	 **/
	virtual void SwitchAudioTrack(uint16_t pid, PAUDIO_PARA_T pAudioPara) = 0;
    
	/**
	 * @brief	当前播放是否结束
	 * @param	无	
	 * @return	true  - 播放结束
	 * 			false - 播放未结束
	 **/
	virtual bool GetIsEos() = 0;

	/**
	 * @brief	取得播放状态
	 * @param	无	
	 * @return	PLAYER_STATE_E - 播放状态play/pause/stop 
	 **/
	virtual int32_t	GetPlayMode() = 0;
	
	/**
	 * @brief	取得当前播放的video pts(没有video的情况下，取audio pts)
	 * @param	无	
	 * @return	pts - 当前播放的原始码流中的pts，ms为单位
	 **/
	virtual int32_t GetCurrentPts() = 0;

	/**
	 * @brief	取得视频的宽高	
	 * @param	*width  - 视频宽度
	 * 			*height - 视频高度
	 * @return	无
	 **/
	virtual void GetVideoPixels(int32_t *width, int32_t *height) = 0;
	
	/**
	 * @brief	获取解码器缓冲区的大小和剩余数据的大小
	 * @param	*totalsize - 解码器缓冲区的大小
	 * 			*datasize  - 剩余未播放数据的大小
	 * @return	0  - 成功
	 * 			-1 - 失败
	 **/
    virtual int32_t GetBufferStatus(int32_t *totalsize, int32_t *datasize) = 0;
	
	/**
	 * @brief	播放结束时是否保留最后一帧
	 * @param	bHoldLastPic - true/false停止播放时是否保留最后一帧 
	 * @return	无
	 **/
    virtual void SetStopMode(bool bHoldLastPic) = 0;

	/**
	 * @brief	设置画面显示比例
	 * @param	contentMode - 源比例/全屏，默认全屏
	 * @return	无
	 **/
    virtual void SetContentMode(PLAYER_CONTENTMODE_E contentMode) = 0;
	
	/**
	 * @brief	获取当前声道
	 * @param	无	
	 * @return  PLAYER_CH_E	
	 **/
	virtual int32_t GetAudioBalance() = 0;
	
	/**
	 * @brief	设置播放声道
	 * @param	nAudioBanlance - 左声道/右声道/立体声 
	 * @return	true  - 成功
	 * 			false - 失败
	 **/
	virtual bool SetAudioBalance(PLAYER_CH_E nAudioBalance) = 0;
	
	/**
	 * @brief	设置VideoSurfaceTexture,用于Android 4以上版本
	 * @param	pSurfaceTexture - VideoSurfaceTexture<Android 4.2及以下>
	 * 			bufferProducer - GraphicBufferProducer<Android 4.4及以上>
	 * @return	无
	 **/
	virtual void SetSurfaceTexture(const void *pVideoSurfaceTexture) = 0;

	/**
	 * @brief	注册回调函数	
	 * @param	pfunc  - 回调函数
	 * 			handle - 回调函数对应的句柄  
	 * @return	无
	 **/
	virtual void playback_register_evt_cb(void *handler, PLAYER_EVENT_CB pfunc) = 0;

	/**
	 * @brief	设置参数(从MediaPlayer透传下来)	
	 * @param	key 	- 命令
	 * 			request - 具体参数
	 * @return	无
	 **/
	virtual void SetParameter(int key, const Parcel &request) = 0;

	/**
	 * @brief	获取参数(从MediaPlayer透传下来)
	 * @param	key   - 命令
	 * 			reply - 具体参数
	 * @return	无
	 **/
	virtual void GetParameter(int key, Parcel *reply) = 0;
};

/**
 * @brief	获取Mediaprocessor对象
 * @param	无	
 * @return  CMCC_MediaProcessor对象
 **/
CMCC_MediaProcessor* Get_CMCCMediaProcessor();

/**
 * @brief	获取当前模块的版本号
 * @param	无
 * @return	版本号
 **/
int Get_CMCCMediaProcessorVersion();

#endif

/**
 * @brief:定义媒体相关格式
 **/
#ifndef _MEDIAFORMAT_H_
#define _MEDIAFORMAT_H_

/**
 * @视频类型
 */
typedef enum {
    VIDEO_FORMAT_UNKNOWN 	= -1,
    VIDEO_FORMAT_MPEG1,
    VIDEO_FORMAT_MPEG2,
    VIDEO_FORMAT_MPEG4,
    VIDEO_FORMAT_H264,
    VIDEO_FORMAT_H265,
    VIDEO_FORMAT_MAX,
} VIDEO_FORMAT_E;

/**
 * @音频类型
 */
typedef enum {
    AUDIO_FORMAT_UNKNOWN		= -1,
    AUDIO_FORMAT_MPEG,  
    AUDIO_FORMAT_AAC, 
    AUDIO_FORMAT_AC3,
    AUDIO_FORMAT_AC3PLUS,
	AUDIO_FORMAT_MAX,    
}AUDIO_FORMAT_E;

/**
 * @视频参数
 */
typedef struct{
	uint16_t		pid;			//video pid
	VIDEO_FORMAT_E	vFmt;			//视频格式
	uint32_t		nVideoWidth;	//视频宽度
	uint32_t		nVideoHeight;	//视频高度
	uint32_t		nFrameRate;		//帧率
	uint32_t		nExtraSize;
	uint8_t			*pExtraData;
}VIDEO_PARA_T, *PVIDEO_PARA_T;

/**
 * @音频参数
 */
typedef struct{
	uint16_t		pid;			//audio pid
	AUDIO_FORMAT_E	aFmt;			//音频格式
	uint32_t		nChannels;		//声道数
	uint32_t		nSampleRate;	//采样率
	uint32_t		block_align;	//block align
	uint32_t 		bit_per_sample;	//比特率
	uint32_t		nExtraSize;
	uint8_t			*pExtraData;	
}AUDIO_PARA_T, *PAUDIO_PARA_T;

/**
 * @声道
 */
typedef enum {
	PLAYER_CH_NULL	= 0,			
	PLAYER_CH_LEFT,					//左声道	
	PLAYER_CH_RIGHT,				//右声道
	PLAYER_CH_STEREO,				//立体声
	PLAYER_CH_MAX,
}PLAYER_CH_E;

/**
 * @视频显示模式
 */
typedef enum
{
    PLAYER_CONTENTMODE_NULL		= -1,
    PLAYER_CONTENTMODE_LETTERBOX,			//源比例输出
    PLAYER_CONTENTMODE_FULL,				//全屏输出
	PLAYER_CONTENTMODE_MAX,
}PLAYER_CONTENTMODE_E;

/**
 * @数据类型
 */
typedef enum
{
    PLAYER_STREAMTYPE_NULL		= -1,
	PLAYER_STREAMTYPE_TS,					//TS数据	
    PLAYER_STREAMTYPE_VIDEO,				//ES Video数据
    PLAYER_STREAMTYPE_AUDIO,				//ES Audio数据
    PLAYER_STREAMTYPE_MAX,	
}PLAYER_STREAMTYPE_E;

/**
 * @播放状态
 */
typedef enum
{
    PLAYER_STATE_OTHER			= -1,		
    PLAYER_STATE_PLAY,						//Play State
    PLAYER_STATE_PAUSE,						//Pause State
    PLAYER_STATE_STOP,						//Stop State
	PLAYER_STATE_MAX,
}PLAYER_STATE_E;

/**
 * @事件类型
 */
typedef enum
{
	PLAYER_EVENT_OTHER			= -1,			
	PLAYER_EVENT_FIRST_PTS,					//显示出第一帧
	PLAYER_EVENT_BLURREDSCREEN_START,       //花屏开始事件
	PLAYER_EVENT_BLURREDSCREEN_END,         //花屏开始事件
	PLAYER_EVENT_STUTTER,                  	//卡顿事件
	PLAYER_EVENT_UNDERFLOW_START,      		//缓冲区开始欠载
	PLAYER_EVENT_UNDERFLOW_END,     		//缓冲区结束欠载
	PLAYER_EVENT_AUDIO_AND_VIDEO_OUTSYNC,	//音视频不同步
	PLAYER_EVENT_FRAMESKIP,					//跳帧
	PLAYER_EVENT_ERROR,						//异常终止
	PLAYER_EVENT_MAX,
}PLAYER_EVENT_E;

typedef enum TS_PLAYER_STATE_E
{
    AM_PLAYER_STATE_INI = 0,    /**< player in init state, when the player created, it is in init state*/
    AM_PLAYER_STATE_DEINI,      /**< player in deinit state*/
    AM_PLAYER_STATE_PLAY,       /**< player play state*/
    AM_PLAYER_STATE_FAST,       /**< player in FF or FB state*/
    AM_PLAYER_STATE_PAUSE,      /**< player in Pause state*/
    AM_PLAYER_STATE_STOP,       /**< player in Stop state*/
    AM_PLAYER_STATE_BUTT
} AM_PLAYER_STATE_E;

#endif /* _MEDIAFORMAT_H */

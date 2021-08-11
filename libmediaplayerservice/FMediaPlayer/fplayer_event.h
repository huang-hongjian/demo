#ifndef _FPLAYER_EVENT_H_
#define _FPLAYER_EVENT_H_


#define TYPE_NAME_SIZE 48
#define URL_NAME_SIZE 4096
#define PROGRAM_NAME_SIZE 100
#define PROGRAM_TYPE_SIZE 12
#define CODEC_TYPE_SIZE 20
//开始准备播放事件
/*
名称 类型 描述
TYPE String 消息类型，开始准备播放，值为
PLAY_PREPARE，需支持组播播放行为
ID Int 会话标志，用于标识每次用户观看
    视频行为。在盒子运行过程中每次
    观看视频的 ID 都不同，即使多次观
    看同一个视频，ID 也不同；关机重
    起后，ID 可重新计数。在一次观看
    视频过程中发生的播放器事件其 ID
    都是一样的。
URL String 播放视频的 url，可从 setdatasource
    设置的参数中直接获取。
START_TIME Long 从 setdatasource 函数开始记录系统
    时间，这个时间点可通过 JAVA 提供
    系统时间的标准函数
    System.currentTimeMillis()（the 
    current time in milliseconds since 
    January 1, 1970 00:00:00.0 UTC）得
    到，与后面的系统时间相同，单位
    毫秒
*/
typedef struct _fp_play_perpare_event
{
char type[TYPE_NAME_SIZE];
int id;
char url[URL_NAME_SIZE];
int64_t start_time;
}fp_play_perpare_event;

//准备播放完成事件
/*
名称 类型 描述
TYPE String 消息类型，准备播放完成，值为
PREPARE_COMPLETED ，对于非Mediaplayer 播放行为，如果无预加载场景，可不上报
ID Int 会话标志
TIME Long 在函数 prepare 和 prepareAsync 
    完成视频播放准备时候的系统时间。p
    repare 在此函数执行完成后触发，p
    repareAsync 在调用回调函数（OnPr
    eparedListener）前触发。两个地方
    都需进行上报。
*/
typedef struct _fp_prepare_completed_event
{
char type[TYPE_NAME_SIZE];
int id;
int64_t time;
}fp_prepare_completed_event;

//开始播放启动事件
/*
名称 类型 描述
TYPE String 消息类型，开始播放启动，值为
PLAY_STARTUP，对于非 Mediaplayer 播放行为，如果无预加载场景，可不上报
ID Int 会话标志
TIME Long 在调用函数 start 开始时的系统时间。
*/
typedef struct _fp_play_startup_event
{
char type[TYPE_NAME_SIZE];
int id;
int64_t time;
}fp_play_startup_event;

//开始播放事件
/*
TYPE String 消 息 类 型 ， 开 始 播 放 ， 值 为
PLAY_START，需支持组播播放行为
ID Int 会话标志
PLAY_TIME Int ①点播节目，当前播放点的时间（比
    如 1:00:00，数值为 3600s）；②时
    移节目，时移时间点到直播最新时
    间点的差。（比如直播时间为
    10:30:00，时移到 9:00:00，则输出
    时间为 1:30:00，数值为 5400s）。
    时间单位为秒
END_TIME Long 记录播出视频第一帧的系统时间
BITRATE Int 节目码率，对于 hls 可以变码率的节
    目，该参数为可选的，单位为 bps
NAME String 当前节目名称[可选]
PROGRAM_TYPE String 当前节目类型[可选]，类型包括：
    HLS Live：HLS 直播节目，播放进度与直
    播源一致，m3u8 内容不断更新；
    HLS VoD：HLS 点播节目，一般是电
    影、电视剧等节目通过 HLS 点播方
    式下载，一个 m3u8 文件包含所有
    分片；
    HLS Shift：HLS 时移节目，指直播期
    间通过回退进度条，m3u8 内容不断
    更新；
    HLS Replay：HLS 回看节目，指点播
    内容属于直播录制内容的回看，一
    个 m3u8 文件包含所有分片；
    MP4/TS：MP4/TS 大文件下载
    FLV：FLV 大文件下载
WIDTH Int 当前视频宽度 pixel
HEIGHT Int 当前视频高度 pixel 
FRAMERATE Int 当前片源的帧率
VIDEO_CODEC String 当前视频视频编码 [可选]，类型包
    括：H264，MPEG-2，MPEG-4，H265，AVS
AUDIO_CODEC String 当前视频音频编码 [可选]，类型包
    括：AC3，AAC，MPEG-1
*/
typedef struct _fp_play_start_event
{
char type[TYPE_NAME_SIZE];
int id;
int play_time;
int64_t end_time;
int bitrate;
char program_name[PROGRAM_NAME_SIZE];
char program_type[PROGRAM_TYPE_SIZE];
int width;
int height;
int framerate;
char video_codec[CODEC_TYPE_SIZE];
char audio_codec[CODEC_TYPE_SIZE];
}fp_play_start_event;

//开始 SEEK 事件
/*
名称 类型 描述
TYPE String 消 息 类型 ，开 始 SEEK 缓 冲 ，值 为
SEEK_START
ID Int 会话标志
START_TIME Long 从 SEEK 操作开始记录的系统时间
PLAY_TIME Int 当前视频播放时间点，单位秒
*/
typedef struct _fp_seek_start_event
{
char type[TYPE_NAME_SIZE];
int id;
int64_t start_time;
int play_time;
}fp_seek_start_event;

//SEEK 结束事件
/*
名称 类型 描述
TYPE String 消息类型， SEEK 缓 冲 结 束 ， 值 为SEEK_END
ID Int 会话标志
PLAY_TIME Int 当前视频播放时间点，单位秒
END_TIME Long 记录播放第一帧的系统时间
*/
typedef struct _fp_seek_end_event
{
char type[TYPE_NAME_SIZE];
int id;
int play_time;
int64_t end_time;
}fp_seek_end_event;

//暂停事件 
/*
名称 类型 描述
TYPE String 消息类型，值为 PAUSE_MESSAGE
ID Int 会话标志
PLAY_TIME Int 暂停时视频播放时间点，单位秒
TIME Long 记录暂停的系统时间
*/
typedef struct _fp_pause_event
{
char type[TYPE_NAME_SIZE];
int id;
int play_time;
int64_t time;

}fp_pause_event;

//继续播放事件 
/*
名称 类型 描述
TYPE String 消息类型，值为 RESUME_MESSAGE
ID Int 会话标志
PLAY_TIME Int 继续播放时视频播放时间点，单位秒
TIME Long 记录继续播放的系统时间
URL String 播放视频的 url
BITRATE Int 节目码率，对于 hls 可以变码率的节目,该参数时可选的，单位为 bps
NAME String 当前节目名称[可选]
    PROGRAM_TYPE String 当前节目类型[可选]，类型包括：
    HLS Live，HLS VoD，，HLS Shift，HLS Replay，
    MP4/TS ，FLV
WIDTH Int 当前视频宽度 pixel
HEIGHT Int 当前视频高度 pixel 
VIDEO_CODEC String 当前视频视频编码 [可选]，类型包
    括：H264，MPEG-2，MPEG-4，H265，AVS
AUDIO_CODEC String 当前视频音频编码 [可选]，类型包
    括：AC3，AAC，MPEG-1
*/
typedef struct _fp_resume_event
{
char type[TYPE_NAME_SIZE];
int id;
int play_time;
int64_t time;
char url[URL_NAME_SIZE];
int bitrate;
char program_name[PROGRAM_NAME_SIZE];
char program_type[PROGRAM_TYPE_SIZE];
int width;
int height;
char video_codec[CODEC_TYPE_SIZE];
char audio_codec[CODEC_TYPE_SIZE];
}fp_resume_event;

//开始缓冲事件
/*
名称 类型 描述
TYPE String 消息类型，开始缓冲值为 BUFFER_START
ID Int 会话标志
START_TIME Long 记录开始缓冲的系统时间
PLAY_TIME Int 记录卡顿时的视频播放时间点，单位秒
*/
typedef struct _fp_buffer_start_event
{
char type[TYPE_NAME_SIZE];
int id;
int64_t start_time;
int play_time;
}fp_buffer_start_event;

//缓冲结束事件
/*
名称 类型 描述
TYPE String 消息类型，缓冲结束，值为 BUFFER_END
ID Int 会话标志
END_TIME Long 记录缓冲结束，播放第一帧的系统时间
*/
typedef struct _fp_buffer_end_event
{
char type[TYPE_NAME_SIZE];
int id;
int64_t end_time;
}fp_buffer_end_event;

//播放器目前缓存的可播时长（或字节数）事件
/*
名称 类型 描述
TYPE String 消息类型,当前播放器缓冲的节目可播
    长度，值为 PLAYABE_REPORT，需支持组播播放行为
ID Int 会话标志
TIME Long 记录当前的系统时间
SECONDS Long 记录播放器下载的可播内容的时长，单位为秒
PLAY_TIME Int 当前视频播放时间点，单位秒
BYTES Long 标记当前播放器的缓冲字节数 Bytes
PRE_FEC Int [可选]在 10s 周期里统计 FEC 纠错前的
    平均误码率（不统计不存在误码率），
    单位为百分比，值范围为 0-100。
AFTER_FEC Int [可选]在 10s 周期里统计存在误码率的
    FEC 纠错后的平均误码率，单位为百分
    比，值范围为 0-100。
*/
typedef struct _fp_playabe_report_event
{
char type[TYPE_NAME_SIZE];
int id;
int64_t time;
long seconds;
int play_time;
long bytes;
int pre_fec;
int after_fec;
}fp_playabe_report_event;

//码率切换事件
/*
名称 类型 描述
TYPE String 消息类型，当节目播放过程中，网络环境变化的情况下，码率从低到高，或者从高到低切换时发送，值为BITRATE_CHANGE
ID Int 会话标志
TIME Long 记录当前的系统时间
OLDID Int 码率切换前的会话标识 ID，OLDID 和 ID可能一样
TO_URL String 切换后的新 URL [可选]
TO_BITRATE Int 切换后码率[可选]，单位为 bps
*/
typedef struct _fp_bitrate_change_event
{
char type[TYPE_NAME_SIZE];
int id;
int64_t time;
int oldid;
int to_url[URL_NAME_SIZE];
int to_bitrate;
}fp_bitrate_change_event;

//播放结束事件
/*
名称 类型 描述
TYPE String 消息类型,播放结束，值为 PLAY_QUIT，需支持组播播放行为
ID Int 会话标志
TIME Long 记录播放结束的系统时间
PLAY_TIME Int 记录的播放时间点，单位秒
*/
typedef struct _fp_play_quit_event
{
char type[TYPE_NAME_SIZE];
int id;
int64_t time;
int play_time;
}fp_play_quit_event;

//播放器错误事件
/*
名称 类型 描述
TYPE String 消息类型，导致视频出现中断播放的情况上报，值为 ERROR_MESSAGE，需支持组播播放行为
ID Int 会话标志
ERROR_CODE Int 错误大致分为三类：网络问题（主要在启播时间里检测，即在 PLAY_START之前）、demux 和 decoder
TIME Long 记录播放错误发生的系统时间
*/
typedef struct _fp_play_error_event
{
char type[TYPE_NAME_SIZE];
int id;
int error_code;
int64_t time;
}fp_play_error_event;

//开始花屏事件
/*
名称 类型 描述
TYPE String 消息类型，开始花屏，值为
BLURREDSCREEN_START，组播和单播（Rtsp udp）播放行为特有事件
ID Int 会话标志
START_TIME Long 记录开始进入花屏或者出现马赛克的系统时间
PLAY_TIME Int 记录花屏时的视频播放时间点，单位秒
*/
typedef struct _fp_blurredscren_start_event
{
char type[TYPE_NAME_SIZE];
int id;
int64_t start_time;
int play_time;
}fp_blurredscren_start_event;


//花屏结束事件
/*
名称 类型 描述
TYPE String 消息类型，花屏结束，值为
BLURREDSCREEN_END，组播和单播（Rtsp 
udp）播放行为特有事件
ID Int 会话标志
END_TIME Long 记录结束花屏或者马赛克的的系统时
间
RATIO Int 在花屏期间的花屏的帧数与总帧数的
比值，单位为百分比，值范围为 0-100。
*/
typedef struct _fp_blurredscren_end_event
{
char type[TYPE_NAME_SIZE];
int id;
int64_t end_time;
int ratio;
}fp_blurredscren_end_event;

/*
无法播片，出现黑屏、蓝屏、跳帧或者
卡顿等现象，值为 UNLOAD_START，组
播和单播（Rtsp）支持
ID Int 会话标志
START_TIME Long 记录开始欠载的系统时间
PLAY_TIME Int 记录欠载时的视频播放时间点，单位秒
*/
typedef struct _fp_unload_start_event
{
char type[TYPE_NAME_SIZE];
int id;
int64_t start_time;
int play_time;	
}fp_unload_start_event;

/*
TYPE String 消息类型，欠载结束，视频正常播放，
值为 UNLOAD_END，组播和单播（Rtsp）
支持
ID Int 会话标志
END_TIME Long 记录欠载结束，播放第一帧的系统时间
*/
typedef struct _fp_unload_end_event
{
char type[TYPE_NAME_SIZE];
int id;
int64_t end_time;
}fp_unload_end_event;


typedef struct _fp_sqm_start_event
{
char ChannelURL[URL_NAME_SIZE];
int MediaType;
char ChannelIp[20];
int ChannelPort;
int StbPort;
}fp_sqm_start_event;

#endif

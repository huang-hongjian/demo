/*
 * author: jintao.xu@amlogic.com
 * date: 2015-05-22
 * wrap original source code for Cmcc usage
 */

#define LOG_NDEBUG 0
#define LOG_TAG "CmccPlayer"

#include "CMCC_Player.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/system_properties.h>
#include <android/native_window.h>
#include <cutils/properties.h>
#include <fcntl.h>
#include "player_set_sys.h"
#include "Amsysfsutils.h"
#include <sys/times.h>
#include <time.h>
#include <gui/Surface.h>
#include <gui/ISurfaceTexture.h>
//#include <gui/SurfaceTextureClient.h>
#include <gui/ISurfaceComposer.h>

#include <android/native_window.h>
#include <gralloc_priv.h>
#include <cutils/properties.h>

using namespace android;

#define MAX_WRITE_ALEVEL 0.8
#define MAX_WRITE_VLEVEL 0.8

#define EXTERNAL_PTS    (1)
#define SYNC_OUTSIDE    (2)

#define TS_BUFFER_TIME       2300
#define TS_AUDIO_BUFFER_TIME 1000
#define TS_VIDEO_BUFFER_TIME 1000

#define TS_BUFFER_SIZE       1024*1024
#define ES_VIDEO_BUFFER_SIZE 2*1024*1024
#define ES_AUDIO_BUFFER_SIZE 1024*1024

#define SHOW_FIRST_FRAME_NO_SYNC 0

int old_delayms = 0;
int count = 0;
unsigned int last_pts = 0;

static int pause_flag=0;
static int debugDumpFile =  NULL;//"/data/tmp/264.ts";
static FILE* dumpafd = NULL;
static FILE* dumpvfd = NULL;
static int lastHoldLastPic;

int64_t av_gettime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

CmccPlayer::CmccPlayer() {
    ALOGI("CmccPlayer");
    char value[PROPERTY_VALUE_MAX] = {0};
    memset(&pquality_info, 0 , sizeof(codec_quality_info));
    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("cmccplayer.dumpfile", value, "0");
    debugDumpFile = atoi(value);
    
    mEsVideoInputBuffer = (uint8_t *)malloc(ES_VIDEO_BUFFER_SIZE);
    if(!mEsVideoInputBuffer)
        ALOGE("alloc mEsVideoInputBuffer error");
    mEsAudioInputBuffer =(uint8_t *)malloc(ES_AUDIO_BUFFER_SIZE);
    if(!mEsAudioInputBuffer)
        ALOGE("alloc mEsAudioInputBuffer error");
    mTsInputBuffer = (uint8_t *)malloc(TS_BUFFER_SIZE);
    if(!mTsInputBuffer)
        ALOGE("alloc mEsAudioInputBuffer error");

    ALOGI("CmccPlayer, TS_BUFFER_TIME: %d, TS_AUDIO_BUFFER_TIME: %d, TS_VIDEO_BUFFER_TIME: %d,dumpfile:%d\n", TS_BUFFER_TIME, TS_AUDIO_BUFFER_TIME, TS_VIDEO_BUFFER_TIME, debugDumpFile);

    //amsysfs_set_sysfs_int("/sys/class/video/disable_video", 2);
    memset(&mAudioPara, 0, sizeof(mAudioPara));
    memset(&mVideoPara, 0, sizeof(mVideoPara));
    memset(&m_Tscodec_para, 0, sizeof(m_Tscodec_para));
    m_Tspcodec = &m_Tscodec_para;
    codec_audio_basic_init();
    lp_lock_init(&mutex, NULL);

    amsysfs_set_sysfs_int("/sys/class/tsync/enable", 1);

    m_bIsPlay = false;
    m_pfunc_play_evt = NULL;
    m_nAudioBalance = 3;

    m_bFast = false;
    m_StartPlayTimePoint = 0;
    m_isBlackoutPolicy = false;
    mStopThread = false;
    mCheckBuffLevelThread = new CheckBuffLevelThread(this);
    mFisrtPtsReady = false;
    mPlayerState = AM_PLAYER_STATE_INI;
    m_bVideoIinit = false;
    m_bAudioIinit = false;
    ALOGI("%s mPlayerState=%d",__FUNCTION__,mPlayerState);    
}

CmccPlayer::~CmccPlayer() {
	ALOGI("~CmccPlayer");
	mStopThread = true;
	if ((mCheckBuffLevelThread.get() != NULL)) {
		mCheckBuffLevelThread->stop();
	}
	if (mEsVideoInputBuffer) {
        free(mEsVideoInputBuffer);
        mEsVideoInputBuffer = NULL;
    }
    if (mEsAudioInputBuffer) {
        free(mEsAudioInputBuffer);
        mEsAudioInputBuffer = NULL;
    }
    if (mTsInputBuffer) {
        free(mTsInputBuffer);
        mTsInputBuffer = NULL;
    }
	SetStopMode(0);
    m_Tspcodec = NULL;
    m_vpcodec = NULL;
    m_apcodec = NULL;
    mIsTsStream = 0;
    mIsEsVideo = 0;
    mIsEsAudio = 0;
    mPlayerState=AM_PLAYER_STATE_DEINI;
    m_bVideoIinit = false;
    m_bAudioIinit = false;
    ALOGI("%s mPlayerState=%d",__FUNCTION__,mPlayerState);
}

//取得播放模式,保留，暂不用
int CmccPlayer::GetPlayMode()
{
    int PlayerState = mPlayerState;
    ALOGI("%s mPlayerState=%d",__FUNCTION__,PlayerState);
    return PlayerState;
}

void CmccPlayer::InitVideo(PVIDEO_PARA_T pVideoPara)
{
    mVideoPara=*pVideoPara;
    ALOGI("InitVideo mVideoPara->pid: %x, mVideoPara->vFmt: %d, mVideoPara.nFrameRate=%d\n", mVideoPara.pid, mVideoPara.vFmt, mVideoPara.nFrameRate);
    if(m_bVideoIinit == true)
        return;
    if (mVideoPara.vFmt == VIDEO_FORMAT_UNKNOWN) {
        mHaveVideo = false;
    } else {
        mHaveVideo = true;
        ALOGI("InitVideo video is es");
    }
    if (mVideoPara.pid == 0xffff && mHaveVideo) {
        mIsEsVideo = true;
        mIsTsStream = false;
    } else {
        mIsEsVideo = false;
        mIsTsStream = true;
    }
	if (mHaveVideo == false && mVideoPara.pid == 0xffff) {
		mIsEsVideo = false;
		mIsTsStream = false;
	}
    if (mHaveVideo && mIsEsVideo) {
        int ret = CODEC_ERROR_NONE;
        m_vpcodec = &m_vcodec_para;
        memset(m_vpcodec, 0, sizeof(codec_para_t));
        m_vcodec_para.has_video = 1;
        if (mVideoPara.vFmt == VIDEO_FORMAT_H264) {
            m_vcodec_para.video_type = VFORMAT_H264;
            m_vcodec_para.am_sysinfo.format = VIDEO_DEC_FORMAT_H264;
        } else if (mVideoPara.vFmt == VIDEO_FORMAT_MPEG1
                || mVideoPara.vFmt == VIDEO_FORMAT_MPEG2) {
            m_vcodec_para.video_type = VFORMAT_MPEG12;
            m_vcodec_para.am_sysinfo.format = VIDEO_DEC_FORMAT_UNKNOW;
        } else if (mVideoPara.vFmt == VIDEO_FORMAT_MPEG4) {
            m_vcodec_para.video_type = VFORMAT_MPEG4;
			m_vcodec_para.am_sysinfo.format = VIDEO_DEC_FORMAT_MPEG4_5;
        } else if (mVideoPara.vFmt == VIDEO_FORMAT_H265) {
            m_vcodec_para.video_type = VFORMAT_HEVC;
            m_vcodec_para.am_sysinfo.format= VIDEO_DEC_FORMAT_HEVC;
        }
        //m_vcodec_para.am_sysinfo.param = (void *)(EXTERNAL_PTS| SYNC_OUTSIDE);
        m_vcodec_para.stream_type = STREAM_TYPE_ES_VIDEO;
        if (mVideoPara.nFrameRate != 0)
            m_vcodec_para.am_sysinfo.rate = 96000 / mVideoPara.nFrameRate;
        m_vcodec_para.has_audio = 0;
	    m_vcodec_para.noblock = 0;
        m_vcodec_para.has_video = 1;
        m_bVideoIinit = true;
        /*
        ret = codec_init(m_vpcodec);
        if(ret != CODEC_ERROR_NONE)
        {
            ALOGI("v codec init failed, ret=-0x%x", -ret);
            return;
        }
        */
    }
}

void CmccPlayer::InitAudio(PAUDIO_PARA_T pAudioPara) {
    ALOGI("InitAudio\n");
    mAudioPara = *pAudioPara;

    ALOGI("InitAudio mAudioPara.pid: %d, mAudioPara.aFmt: %d, channel=%d, samplerate=%d\n", mAudioPara.pid, mAudioPara.aFmt, mAudioPara.nChannels, mAudioPara.nSampleRate);
    old_delayms = 0;
    count = 0;
    last_pts = 0;
    if(m_bAudioIinit == true)
        return ;
    if (mAudioPara.aFmt == VIDEO_FORMAT_UNKNOWN) {
        mHaveAudio = false;
    } else {
        mHaveAudio = true;
        ALOGI("InitAudio audio is es");
    }
    if (mAudioPara.pid == 0xffff) {
        mIsEsAudio = true;
        mIsTsStream = false;
    } else {
        mIsEsAudio = false;
        mIsTsStream = true;
    }
	if (mHaveAudio == false && mAudioPara.pid == 0xffff) {
		mIsEsAudio = false;
		mIsTsStream = false;
	}
    if (mHaveAudio && mIsEsAudio) {
        int ret = CODEC_ERROR_NONE;
        m_apcodec = &m_acodec_para;
        memset(m_apcodec, 0, sizeof(codec_para_t));
        if (mAudioPara.aFmt == AUDIO_FORMAT_MPEG) {
            m_apcodec->audio_type = AFORMAT_MPEG;
        } else if (mAudioPara.aFmt == AUDIO_FORMAT_AAC) {
            m_apcodec->audio_type = AFORMAT_AAC;
        } else if (mAudioPara.aFmt == AUDIO_FORMAT_AC3) {
            m_apcodec->audio_type = AFORMAT_AC3;
        } else if (mAudioPara.aFmt == AUDIO_FORMAT_AC3PLUS) {
            m_apcodec->audio_type = AFORMAT_EAC3;
        } 
        m_apcodec->stream_type = STREAM_TYPE_ES_AUDIO;
        ALOGI("mAudioPara.aFmt=%x, m_apcodec->audio_type=%x, mAudioPara.aFmt=%d", mAudioPara.aFmt, m_apcodec->audio_type, mAudioPara.aFmt);

        m_apcodec->has_audio = 1;
        m_apcodec->noblock = 0;
        
        //Do NOT set audio info if we do not know it
        m_apcodec->audio_channels = mAudioPara.nChannels;
        m_apcodec->audio_samplerate = mAudioPara.nSampleRate;
        m_apcodec->audio_info.channels = mAudioPara.nChannels;
        m_apcodec->audio_info.sample_rate = mAudioPara.nSampleRate;
        m_bAudioIinit = true;
        /*
        ret = codec_init(m_apcodec);
        if(ret != CODEC_ERROR_NONE)
        {
            printf("a codec init failed, ret=-0x%x", -ret);
            return;
        }
        */
    } 
    return ;
}

bool CmccPlayer::StartPlay() {
    bool ret;
    int filter_afmt;
    mFisrtPtsReady = false;
    char vaule[PROPERTY_VALUE_MAX] = {0};
    ALOGI("StartPlay");
    if (mHaveVideo && mIsEsVideo) {
        int ret = codec_init(m_vpcodec);
        if(ret != CODEC_ERROR_NONE)
        {
            ALOGI("v codec init failed, ret=-0x%x", -ret);

        }
    }
    if (mHaveAudio && mIsEsAudio) {
        int ret = codec_init(m_apcodec);
        if(ret != CODEC_ERROR_NONE)
        {
            ALOGI("a codec init failed, ret=-0x%x", -ret);

        }
    }
    if (debugDumpFile != 0) {
        char tmpfilename[1024] = "";
        static int tmpfileindex = 0;
        memset(vaule, 0, PROPERTY_VALUE_MAX);
        property_get("cmccplayer.dumppath", vaule, "/storage/external_storage/sda1");
        if(mIsTsStream){
            sprintf(tmpfilename, "%s/Live%d.ts", vaule, tmpfileindex);
            tmpfileindex++;
            dumpvfd = fopen(tmpfilename, "wb+");
        }else{
            sprintf(tmpfilename, "%s/Live%d_vwrite.data", vaule, tmpfileindex);
            dumpvfd = fopen(tmpfilename, "wb+");
            sprintf(tmpfilename, "%s/Live%d_awrite.data", vaule, tmpfileindex);
            tmpfileindex++;
            dumpafd = fopen(tmpfilename, "wb+");
        }

    }
    set_sysfs_int("/sys/class/tsync/vpause_flag",0); // reset vpause flag -> 0
    set_sysfs_int("/sys/class/video/show_first_frame_nosync", SHOW_FIRST_FRAME_NO_SYNC);	//keep last frame instead of show first frame
    pause_flag=0;	

	//amsysfs_set_sysfs_int("/sys/class/video/disable_video",0);
	//amsysfs_set_sysfs_int("/sys/class/video/blackout_policy",0);

   // if(amsysfs_get_sysfs_int("/sys/class/video/slowsync_flag")!=1){
   //     amsysfs_set_sysfs_int("/sys/class/video/slowsync_flag",1);
   // }
	amsysfs_set_sysfs_int("/sys/class/video/slowsync_repeat_enable",0);

    if (mIsEsVideo || mIsEsAudio) {
        ret = true;
    } else if (mIsTsStream){
        ALOGI("StartPlay is TS");
        memset(m_Tspcodec,0,sizeof(*m_Tspcodec));
        m_Tspcodec->stream_type = STREAM_TYPE_TS;
        if((int)mVideoPara.pid != 0) {
            if (mVideoPara.vFmt == VIDEO_FORMAT_H264) {
                m_Tspcodec->video_type = VFORMAT_H264;
            } else if (mVideoPara.vFmt == VIDEO_FORMAT_MPEG1
                    || mVideoPara.vFmt == VIDEO_FORMAT_MPEG2) {
                m_Tspcodec->video_type = VFORMAT_MPEG12;
            } else if (mVideoPara.vFmt == VIDEO_FORMAT_MPEG4) {
                m_Tspcodec->video_type = VFORMAT_MPEG4;
            } else if (mVideoPara.vFmt == VIDEO_FORMAT_H265) {
                m_Tspcodec->video_type = VFORMAT_HEVC;
            } else {
                ALOGE("StartPlay mVideoPara.vFmt ERROR mVideoPara.vFmt=%d", mVideoPara.vFmt);
            }
            m_Tspcodec->has_video = 1;
            m_Tspcodec->video_pid = (int)mVideoPara.pid;
            m_Tspcodec->am_sysinfo.param = (void *)(0x04);
            m_Tspcodec->am_sysinfo.rate = 1;
            ALOGE("StartPlay__0x04");
        } else {
            m_Tspcodec->has_video = 0;
            m_Tspcodec->video_pid = -1;
        }

        if(IS_AUIDO_NEED_EXT_INFO(m_Tspcodec->audio_type)) {
            m_Tspcodec->audio_info.valid = 1;
            ALOGI("set audio_info.valid to 1");
        }

        if(!m_bFast) {
            if(mAudioPara.pid != 0) {
                m_Tspcodec->has_audio = 1;
                m_Tspcodec->audio_pid = mAudioPara.pid;
                if (mAudioPara.aFmt == AUDIO_FORMAT_MPEG) {
                    m_Tspcodec->audio_type = AFORMAT_MPEG;
                } else if (mAudioPara.aFmt == AUDIO_FORMAT_AAC) {
                    m_Tspcodec->audio_type = AFORMAT_AAC;
                } else if (mAudioPara.aFmt == AUDIO_FORMAT_AC3) {
                    m_Tspcodec->audio_type = AFORMAT_AC3;
                } else if (mAudioPara.aFmt == AUDIO_FORMAT_AC3PLUS) {
                    m_Tspcodec->audio_type = AFORMAT_EAC3;
                }
            }
            ALOGI("m_Tspcodec->audio_samplerate: %d, m_Tspcodec->audio_channels: %d\n",
                    m_Tspcodec->audio_samplerate, m_Tspcodec->audio_channels);
        } else {
            m_Tspcodec->has_audio = 0;
            m_Tspcodec->audio_pid = -1;
        }
        ALOGI("set vFmt:%d, aFmt:%d, vpid:%d, apid:%d\n", mVideoPara.vFmt, mAudioPara.aFmt, mVideoPara.pid, mAudioPara.pid);
        ALOGI("set has_video:%d, has_audio:%d, video_pid:%d, audio_pid:%d\n", m_Tspcodec->has_video, m_Tspcodec->has_audio, 
                m_Tspcodec->video_pid, m_Tspcodec->audio_pid);
        m_Tspcodec->noblock = 0;

        lp_lock(&mutex);
        int error = codec_init(m_Tspcodec);
        ALOGI("StartPlay codec_init After: %d\n", ret);
        lp_unlock(&mutex);
        codec_set_cntl_syncthresh(m_Tspcodec, m_Tspcodec->has_audio);
        ret = !error;
    } else {
        ALOGE("StartPlay error is no es or ts");
        return false;
    }
    if ((mCheckBuffLevelThread.get() != NULL)
            && (mCheckBuffLevelThread.get()->mTaskStatus != CheckBuffLevelThread::STATUE_RUNNING)) {
        mCheckBuffLevelThread->start();
    }
    for (int i = 0; i<4; i++) {
        update_nativewindow();
    }
    m_bIsPlay = true;
    m_StartPlayTimePoint = av_gettime();
    mPlayerState = AM_PLAYER_STATE_PLAY;
    ALOGI("%s mPlayerState=%d",__FUNCTION__,mPlayerState);
    return ret;
}

int32_t CmccPlayer::GetWriteBuffer(PLAYER_STREAMTYPE_E type, uint8_t **pBuffer, uint32_t *nSize) {
    if (type == PLAYER_STREAMTYPE_TS) {
        *nSize = TS_BUFFER_SIZE;
        *pBuffer = mTsInputBuffer;
    } else if (type == PLAYER_STREAMTYPE_VIDEO) {
        *nSize = ES_VIDEO_BUFFER_SIZE;
        *pBuffer = mEsVideoInputBuffer;
    } else if (type == PLAYER_STREAMTYPE_AUDIO) {
        *nSize = ES_AUDIO_BUFFER_SIZE;
        *pBuffer = mEsAudioInputBuffer;
    }
    return 0;
}

int32_t CmccPlayer::WriteData(PLAYER_STREAMTYPE_E type, uint8_t *pBuffer, uint32_t nSize, uint64_t timestamp) {
    int ret = -1;
    static int retry_count = 0;
    buf_status audio_buf;
    buf_status video_buf;
    float audio_buf_level = 0.00f;
    float video_buf_level = 0.00f;
    codec_para_t *pcodec;

    if(!m_bIsPlay)
        return -1;
	lp_lock(&mutex);

    if (type == PLAYER_STREAMTYPE_TS) {
       // fwrite(pBuffer,1,nSize,dumpfd);
       // ALOGI("dump ts");
        while (1) {
            pcodec = m_Tspcodec;
            codec_get_abuf_state(pcodec, &audio_buf);
            codec_get_vbuf_state(pcodec, &video_buf);
           // ALOGI("WriteData : audio_buf.data_len=%d, video_buf.data_len=%d", audio_buf.data_len, video_buf.data_len);
            if(audio_buf.size != 0)
                audio_buf_level = (float)audio_buf.data_len / audio_buf.size;
            if(video_buf.size != 0)
                video_buf_level = (float)video_buf.data_len / video_buf.size;
            if(mStopThread == true)
            {
                return 0;
            }
            if((audio_buf_level >= MAX_WRITE_ALEVEL) || (video_buf_level >= MAX_WRITE_VLEVEL)) {
             //   ALOGI("WriteData : audio_buf_level= %.5f, video_buf_level=%.5f, Don't writedate()\n", audio_buf_level, video_buf_level);
                usleep(20*1000); //20ms
            } else {
                break;
            }
        }
    } else if (type == PLAYER_STREAMTYPE_VIDEO) {
        pcodec = m_vpcodec;
        while (1) {
            codec_get_vbuf_state(pcodec, &video_buf);
            if(mStopThread == true)
            {
                lp_unlock(&mutex);
                return 0;
            }
            //ALOGI("WriteData : video_buf.data_len=%d, timestamp=%lld", video_buf.data_len, timestamp);
            if (video_buf.data_len > 0x1000*1000) { //4M
                usleep(20*1000);
            } else {
                codec_checkin_pts(pcodec,  timestamp);
                break;
            }
        }

    } else if (type == PLAYER_STREAMTYPE_AUDIO) {
        pcodec = m_apcodec;
        while (1) {
            codec_get_abuf_state(pcodec, &audio_buf);
            if(mStopThread == true)
            {
                lp_unlock(&mutex);
                return 0;
            }
            //ALOGI("WriteData : audio_buf.data_len=%d, timestamp=%lld", audio_buf.data_len, timestamp);
            if (audio_buf.data_len > 0x1000*250*2) { //2M
                usleep(20*1000);
            } else {
                codec_checkin_pts(pcodec,  timestamp);
                break;
            }
        }
    } else {
        return -1;
    }
    //ALOGI("WriteData: nSize=%d, type=%d,audio_buf.data_len: %d, video_buf.data_len: %d,timestamp=%lld\n",  nSize, type, audio_buf.data_len, video_buf.data_len, timestamp/90);

    int temp_size = 0;
    for(int retry_count=0; retry_count<10; retry_count++) {
        ret = codec_write(pcodec, pBuffer+temp_size, nSize-temp_size);
        if((ret < 0) || (ret > nSize)) {
            //if(ret == EAGAIN) {
            if(ret < 0 && errno == EAGAIN) {
                usleep(10);
                if(mStopThread == true)
                {
                    lp_unlock(&mutex);
                    return 0;
                }
                //ALOGI("WriteData: codec_write return EAGAIN!\n");
                continue;
            } else {
                ALOGI("WriteData: codec_write return %d!\n", ret);
                if(pcodec && pcodec->handle > 0){
                    ret = codec_close(pcodec);
                    ret = codec_init(pcodec);
                    if(m_bFast && (type==PLAYER_STREAMTYPE_TS)) {
                        codec_set_mode(pcodec, TRICKMODE_I);
                    }
                    ALOGI("WriteData : codec need close and reinit m_bFast=%d\n", m_bFast);
                } else {
                    ALOGI("WriteData: codec_write return error or stop by called!\n");
                    break;
                }
            }
        } else {
            temp_size += ret;
            //ALOGI("WriteData : codec_write  nSize is %d! temp_size=%d retry_count=%d\n", nSize, temp_size, retry_count);
            if(temp_size >= nSize){
                temp_size = nSize;
                break;
            }
            if(mStopThread == true)
            {
                lp_unlock(&mutex);
                return 0;
            }
            // release 10ms to other thread, for example decoder and drop pcm
            usleep(10000);
        }
    }
    if (ret >= 0 && temp_size > ret)
        ret = temp_size; // avoid write size large than 64k size
    lp_unlock(&mutex);

    if(ret > 0) {
        if ((type == PLAYER_STREAMTYPE_VIDEO)||(type == PLAYER_STREAMTYPE_TS)){
                if(dumpvfd != NULL) {     
                    fwrite(pBuffer, 1, temp_size, dumpvfd);
                }
            }else{
                if(dumpafd != NULL) {     
                    fwrite(pBuffer, 1, temp_size, dumpafd);
                }
        }
    } else {
        ALOGI("WriteData: codec_write fail(%d),temp_size[%d] nSize[%d]\n", ret, temp_size, nSize);
        if (temp_size > 0){
            if ((type == PLAYER_STREAMTYPE_VIDEO)||(type == PLAYER_STREAMTYPE_TS)){
                    if(dumpvfd != NULL) {     
                        fwrite(pBuffer, 1, temp_size, dumpvfd);
                    }
                } else {
                    if(dumpafd != NULL) {     
                        fwrite(pBuffer, 1, temp_size, dumpafd);
                    }
            }
            return temp_size;
        } else
            return -1;
    }
    return ret;
}


bool CmccPlayer::Pause() {
    ALOGI("Pause");
    lp_lock(&mutex);
    pause_flag=1;	
    if (mIsTsStream) {
        codec_pause(m_Tspcodec);
    }
    if (mIsEsVideo) {
        codec_pause(m_vpcodec);
    }
    if (mIsEsAudio) {
        codec_pause(m_apcodec);
    }
    lp_unlock(&mutex);
    mPlayerState = AM_PLAYER_STATE_PAUSE;
    ALOGI("%s mPlayerState=%d",__FUNCTION__,mPlayerState);
    return true;
}

bool CmccPlayer::Resume() {
    ALOGI("Resume");
    lp_lock(&mutex);
    if (mIsTsStream) {
        codec_resume(m_Tspcodec);
    }
    if (mIsEsVideo) {
        codec_resume(m_vpcodec);
    }
    if (mIsEsAudio) {
        codec_resume(m_apcodec);
    }
    pause_flag=0;	
    lp_unlock(&mutex);
    mPlayerState = AM_PLAYER_STATE_PLAY;
    ALOGI("%s mPlayerState=%d",__FUNCTION__,mPlayerState);

    return true;
}

bool CmccPlayer::Fast() {
    int ret;

    ALOGI("Fast");
    ret = amsysfs_set_sysfs_int("/sys/class/video/blackout_policy", 0);
    if(ret)
        return false;
    Stop();
    m_bFast = true;
    //amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_all", 1);
    amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_trick_mode", 2);
    ret = StartPlay();
    if(!ret)
        return false;

    ALOGI("Fast: codec_set_mode: %d\n", m_Tspcodec->handle);
    ret = codec_set_mode(m_Tspcodec, TRICKMODE_I);
    mPlayerState = AM_PLAYER_STATE_FAST;
    ALOGI("%s mPlayerState=%d",__FUNCTION__,mPlayerState);
    return !ret;
}

bool CmccPlayer::StopFast()
{
    int ret;
	return true;

    ALOGI("StopFast");
    m_bFast = false;
    ret = codec_set_mode(m_Tspcodec, TRICKMODE_NONE);
    //amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_all", 0);
    Stop();
    //amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_all", 0);
    amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_trick_mode", 1);
    ret = StartPlay();
    if(!ret)
        return false;
    if(m_isBlackoutPolicy) {
        ret = amsysfs_set_sysfs_int("/sys/class/video/blackout_policy",1);
        if (ret)
            return false;
	}
    mPlayerState = AM_PLAYER_STATE_PLAY;
    ALOGI("%s mPlayerState=%d",__FUNCTION__,mPlayerState);
    return true;
}

bool CmccPlayer::Stop() {
    ALOGI("Stop");
    int ret;
    mStopThread = true;

    if(m_bIsPlay) {

        m_bFast = false;
        m_bIsPlay = false;
        m_StartPlayTimePoint = 0;
        lp_lock(&mutex);
        //amsysfs_set_sysfs_int("/sys/class/video/blackout_policy",1);

        if (mIsTsStream) {
            ret = codec_set_mode(m_Tspcodec, TRICKMODE_NONE);
            //amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_all", 0);
            ALOGI("m_bIsPlay is true");
            
            ret = codec_close(m_Tspcodec);
            m_Tspcodec->handle = -1;
            ALOGI("Stop  codec_close After:%d\n", ret);
            
        }
        if (mIsEsVideo) {
            ret = codec_close(m_vpcodec);
        }
        if (mIsEsAudio) {
            ret = codec_close(m_apcodec);
        }
        lp_unlock(&mutex);

    } else {
        ALOGI("m_bIsPlay is false");
    }
    if(dumpvfd != NULL) {     
        fclose( dumpvfd);
        dumpvfd = NULL;
    }
    if(dumpafd != NULL) {     
        fclose(dumpafd);
       dumpafd = NULL;
    }
    mPlayerState = AM_PLAYER_STATE_STOP;
    ALOGI("%s mPlayerState=%d",__FUNCTION__,mPlayerState);
    return true;
}

bool CmccPlayer::Seek() {
    ALOGI("Seek");
    if (m_bIsPlay) {
        m_bIsPlay = false;
        int ret = 0;
        lp_lock(&mutex);
	SetStopMode(1);
        if (mIsTsStream) {
            ret = codec_set_mode(m_Tspcodec, TRICKMODE_NONE);
            //amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_all", 0);
            ALOGI("m_bIsPlay is false");
            ret = codec_reset(m_Tspcodec);
            //m_Tspcodec->handle = -1;
            if (pause_flag == 1)
            {
                ret = codec_resume(m_Tspcodec);
                pause_flag=0;
            }
            ALOGI("Stop  codec_close After:%d\n", ret);
        }
        if (mIsEsVideo) {
            ret = codec_reset(m_vpcodec);
            if (pause_flag == 1)
            {
                ret = codec_resume(m_vpcodec);
                if (mIsEsAudio)
                    ret = codec_resume(m_apcodec);
                pause_flag=0;
            }
        }
        if (mIsEsAudio) {
            ret = codec_reset(m_apcodec);
        }
        lp_unlock(&mutex);
        m_bIsPlay = true;
    }else
        ALOGI("Seek m_bIsPlay is false");
    mPlayerState = AM_PLAYER_STATE_PLAY;
    m_bSeekFlag = true;
    ALOGI("%s mPlayerState=%d",__FUNCTION__,mPlayerState);
    return true;
}

//get current sound track
//return parameter: 1, Left Mono; 2, Right Mono; 3, Stereo; 4, Sound Mixing
int CmccPlayer::GetAudioBalance()
{
    return m_nAudioBalance;
}

//set sound track 
//input paramerter: nAudioBlance, 1, Left Mono; 2, Right Mono; 3, Stereo; 4, Sound Mixing
bool CmccPlayer::SetAudioBalance(int nAudioBalance)
{
    if((nAudioBalance < 1) && (nAudioBalance > 4))
        return false;
    m_nAudioBalance = nAudioBalance;
    if(nAudioBalance == 1) {
        ALOGI("SetAudioBalance 1 Left Mono\n");
        //codec_left_mono(m_Tspcodec);
         amsysfs_set_sysfs_str("/sys/class/amaudio/audio_channels_mask", "l");
    } else if(nAudioBalance == 2) {
        ALOGI("SetAudioBalance 2 Right Mono\n");
        //codec_right_mono(m_Tspcodec);
        amsysfs_set_sysfs_str("/sys/class/amaudio/audio_channels_mask", "r");
    } else if(nAudioBalance == 3) {
        ALOGI("SetAudioBalance 3 Stereo\n");
        //codec_stereo(m_Tspcodec);
        amsysfs_set_sysfs_str("/sys/class/amaudio/audio_channels_mask", "s");
    } else if(nAudioBalance == 4) {
        ALOGI("SetAudioBalance 4 Sound Mixing\n");
        //codec_stereo(m_Tspcodec);
        amsysfs_set_sysfs_str("/sys/class/amaudio/audio_channels_mask", "c");
    }
    return true;
}
int32_t CmccPlayer::GetCurrentPts() {
    codec_para_t *pcodec;
    unsigned long pts_u32 = 0;
    int audio_delay = 0;
    if (mIsTsStream) {
        pcodec = m_Tspcodec;
        pts_u32 = ((unsigned long)codec_get_vpts(pcodec)) / 90;
    }
    else if (mIsEsVideo) {
        pcodec = m_vpcodec;
	 pts_u32 = ((unsigned long)codec_get_vpts(pcodec)) / 90;
    }
    else if (mIsEsAudio) {
        pcodec = m_apcodec;
        pts_u32 = ((unsigned long)codec_get_apts(pcodec)) / 90;
	 codec_get_audio_cur_delay_ms(pcodec, &audio_delay);
        if (pts_u32 != 0 && count < 100 && (audio_delay - old_delayms) == 0) {
            count++;
            last_pts = pts_u32;
            ALOGI("GetCurrentPts: count=%d\n", count);
        } else if (pts_u32 != 0 && count < 100 && (audio_delay - old_delayms) != 0) {
            count = 0;
            last_pts = pts_u32;
        }
        old_delayms = audio_delay;
        ALOGI("GetCurrentPts: audio_delay=%d\n", audio_delay);
        if (count >= 100) {
            pts_u32 = last_pts;
        }
    }
//    ALOGI("GetCurrentPts:pts_u32=%d", pts_u32);
    return pts_u32;
}

void CmccPlayer::GetVideoPixels(int& width, int& height)
{
    int x = 0, y = 0;
    //OUTPUT_MODE output_mode = get_display_mode();
    //getPosition(output_mode, &x, &y, &width, &height);
    //ALOGI("GetVideoPixels, x: %d, y: %d, width: %d, height: %d", x, y, width, height);
}

int CmccPlayer::GetBufferStatus(int32_t *totalsize, int32_t *datasize) {
    buf_status audio_buf;
    buf_status video_buf;
    *totalsize = 0;
    *datasize = 0;

    lp_lock(&mutex);

    if (mIsTsStream) {
        codec_get_abuf_state(m_Tspcodec, &audio_buf);
        codec_get_vbuf_state(m_Tspcodec, &video_buf);
    }
    if (mIsEsVideo) {
        codec_get_vbuf_state(m_vpcodec, &video_buf);
    }
    if (mIsEsAudio) {
        codec_get_abuf_state(m_apcodec, &audio_buf);
    }
    //ALOGI("video_buf.size=%d,video_buf.data_len=%d", video_buf.size, video_buf.data_len);
    if (mHaveVideo) {
        *totalsize += video_buf.size;
        *datasize += video_buf.data_len;
    }
   // ALOGI("audio_buf.size=%d, audio_buf.data_len=%d", audio_buf.size, audio_buf.data_len);
    if (mHaveAudio) {
        *totalsize += audio_buf.size;
        *datasize += audio_buf.data_len;
    }
    lp_unlock(&mutex);
    ALOGI("*totalsize=%d, *datasize=%d, video_buf.data_len=%d, audio_buf.data_len=%d", *totalsize, *datasize, video_buf.data_len, audio_buf.data_len);
    return 0;
}

void CmccPlayer::SetStopMode(bool bHoldLastPic){
	//bHoldLastPic = amsysfs_get_sysfs_int("/sys/class/video/clrlast_frame");
	ALOGI("SetStopMode bHoldLastPic=%d begin ----\n", bHoldLastPic);
	lastHoldLastPic = bHoldLastPic;
	if (bHoldLastPic == 0) { //surfaceDestroyed
		amsysfs_set_sysfs_int("/sys/class/video/disable_video", 2);
		amsysfs_set_sysfs_int("/sys/class/video/blackout_policy", 1);
	} else { //surfaceCreated
		amsysfs_set_sysfs_int("/sys/class/video/disable_video", 0);
		amsysfs_set_sysfs_int("/sys/class/video/blackout_policy", 0);
	}
}
void CmccPlayer::SetContentMode(PLAYER_CONTENTMODE_E contentMode) {
    ALOGI("SetContentMode contentMode=%d", contentMode);
    //0:normal，1:full stretch，2:4-3，3:16-9
    if (contentMode == PLAYER_CONTENTMODE_LETTERBOX){
        amsysfs_set_sysfs_int("/sys/class/video/screen_mode", 0);
    } else if(contentMode == PLAYER_CONTENTMODE_FULL) {
        amsysfs_set_sysfs_int("/sys/class/video/screen_mode", 1);
    }
}

void CmccPlayer::SwitchAudioTrack(int pid){
    ALOGI("SwitchAudioTrack");
    return;
}

long CmccPlayer::GetCurrentPlayTime() 
{
    long pts = 0;
	ALOGI("GetCurrentPlayTime %d %d %d, %p %p ",mIsTsStream, mIsEsVideo, mIsEsAudio, m_vpcodec, m_apcodec);
    if (mIsTsStream)
    {
        pts = codec_get_vpts(m_Tspcodec);
    }
    else if (mIsEsVideo)
    {
        pts = codec_get_vpts(m_vpcodec);
    }
    else if (mIsEsAudio)
    {
        pts = codec_get_apts(m_apcodec);
    }
    pts = pts/90;
    ALOGI("GetCurrentPlayTime=%d", pts);
    return pts;
}

void CmccPlayer::leaveChannel()
{
    ALOGI("leaveChannel be call\n");
    Stop();
}

void CmccPlayer::SetSurface(Surface* pSurface)
{
    ALOGI("SetSurface pSurface: %p\n", pSurface);
    sp<IGraphicBufferProducer> mGraphicBufProducer;
    sp<ANativeWindow> mNativeWindow;
    mGraphicBufProducer = pSurface->getIGraphicBufferProducer();
    if(mGraphicBufProducer != NULL) {
        mNativeWindow = new Surface(mGraphicBufProducer);
    } else {
        ALOGE("SetSurface, mGraphicBufProducer is NULL!\n");
        return;
    }
    native_window_set_buffer_count(mNativeWindow.get(), 4);
    native_window_set_usage(mNativeWindow.get(), GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP | GRALLOC_USAGE_AML_VIDEO_OVERLAY);
    //native_window_set_buffers_format(mNativeWindow.get(), WINDOW_FORMAT_RGBA_8888);
    native_window_set_buffers_geometry(mNativeWindow.get(),  32, 32, WINDOW_FORMAT_RGBA_8888);
    if (mNativeWindow.get() != NULL) {
        status_t err = native_window_set_scaling_mode(mNativeWindow.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
        if (err != OK) {
        ALOGW("Failed to set scaling mode: %d", err);
        }
    }
}

void CmccPlayer::SetSurfaceTexture(const sp<IGraphicBufferProducer>& bufferProducer) {
    ALOGI("719 SetSurfaceTexture\n");

    if (bufferProducer != NULL) {
        mNativeWindow = new Surface(bufferProducer);
    }
    native_window_set_buffer_count(mNativeWindow.get(), 4);
    native_window_set_usage(mNativeWindow.get(), GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP | GRALLOC_USAGE_AML_VIDEO_OVERLAY);
    //native_window_set_buffers_format(mNativeWindow.get(), WINDOW_FORMAT_RGBA_8888);
    native_window_set_buffers_geometry(mNativeWindow.get(),  32, 32, WINDOW_FORMAT_RGBA_8888);
    if (mNativeWindow.get() != NULL) {
          ALOGI("719 native_window_set_scaling_mode\n");
        status_t err = native_window_set_scaling_mode(mNativeWindow.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
        if (err != OK) {
        ALOGW("Failed to set scaling mode: %d", err);
        }
    }
    ANativeWindowBuffer* buf;
    return;
}

void CmccPlayer::playback_register_evt_cb(void *hander, PLAYER_EVENT_CB pfunc) {
    m_play_evt_hander = hander;
    m_pfunc_play_evt = pfunc;
}


void CmccPlayer::SetParameter(int key, const Parcel &request)
{
	ALOGI("set key:%d",key);
	return ;
}

void CmccPlayer::GetParameter(int key, Parcel *reply)
{
	ALOGI("get key:%d",key);
	return ;
}

void CmccPlayer::update_nativewindow() {
    ANativeWindowBuffer* buf = NULL;
    char* vaddr = NULL;
    
    if (mNativeWindow.get() == NULL) {
        ALOGE("update_nativewindow mNativeWindow Null");
        return;
    }
    
    int err = mNativeWindow->dequeueBuffer_DEPRECATED(mNativeWindow.get(), &buf);
    if (err != 0) {
        ALOGE("dequeueBuffer failed: %s (%d)", strerror(-err), -err);
        return;
    }
    if (buf == NULL) { 
        ALOGE("update_nativewindow buf Null");
        return; 
    }
    mNativeWindow->lockBuffer_DEPRECATED(mNativeWindow.get(), buf);
    sp<GraphicBuffer> graphicBuffer(new GraphicBuffer(buf, false));
    graphicBuffer->lock(1, (void **)&vaddr);
    if (vaddr != NULL) {
        //memset(vaddr, 0x0, graphicBuffer->getWidth() * graphicBuffer->getHeight() * 4); /*to show video in osd hole...*/
    }
    graphicBuffer->unlock();
    graphicBuffer.clear();
    ALOGV("checkBuffLevel___queueBuffer_DEPRECATED");
    
    mNativeWindow->queueBuffer_DEPRECATED(mNativeWindow.get(), buf);

}
void CmccPlayer::checkBuffLevel() {
	int audio_delay=0, video_delay=0;
    float audio_buf_level = 0.00f, video_buf_level = 0.00f;
    buf_status audio_buf;
    buf_status video_buf;

    if(m_bIsPlay) {
		//codec_get_audio_cur_delay_ms(m_Tspcodec, &audio_delay);
		//codec_get_video_cur_delay_ms(m_Tspcodec, &video_delay);	
        if(!m_bFast && m_StartPlayTimePoint > 0 && (((av_gettime() - m_StartPlayTimePoint)/1000 >= TS_BUFFER_TIME)
                || (audio_delay >= TS_AUDIO_BUFFER_TIME || video_delay >= TS_VIDEO_BUFFER_TIME))) {
            ALOGI("av_gettime()=%lld, m_StartPlayTimePoint=%lld, TS_BUFFER_TIME=%d\n", av_gettime(), m_StartPlayTimePoint, TS_BUFFER_TIME);
            ALOGI("audio_delay=%d, TS_AUDIO_BUFFER_TIME=%d, video_delay=%d, TS_VIDEO_BUFFER_TIME=%d\n", audio_delay, TS_AUDIO_BUFFER_TIME, video_delay, TS_VIDEO_BUFFER_TIME);
            ALOGI("checkBuffLevel: resume play now!\n");
            //codec_resume(m_Tspcodec);
            m_StartPlayTimePoint = 0;
        }
    }
}
#define TSYNC_VPTS      "/sys/class/tsync/pts_video"
static int  cmcc_get_sysfs_str(const char *path, char *valstr, int size)
{
    int fd;
    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        memset(valstr, 0, size);
        read(fd, valstr, size - 1);
        valstr[strlen(valstr)] = '\0';
        close(fd);
    } else {
        ALOGI("unable to open file %s,err: %s", path, strerror(errno));
        sprintf(valstr, "%s", "fail");
        return -1;
    };
    //LOGI("get_sysfs_str=%s\n", valstr);
    return 0;
}

static long cmcc_get_seek_after_first_pts()
{
    char buf[50] = {0};
    long vpts = 0;    
    cmcc_get_sysfs_str(TSYNC_VPTS, buf, sizeof(buf));// read vpts
    if (sscanf(buf, "0x%lx", &vpts) < 1) {
        ALOGI("unable to get vpts from: %s", buf);
        return -1L;
    }
    if (vpts == 0) {
        // vpts invalid too
        // loop again
        return -1L;
    }
    return vpts;
}
void CmccPlayer::updateCMCCInfo()
{
    struct av_param_info_t av_param_info;
    int report_flag=0;
    char value[PROPERTY_VALUE_MAX] = {0};
	
    memset(&av_param_info , 0 , sizeof(av_param_info_t));

    codec_para_t *pcodec=NULL;
    if (mIsTsStream)
        pcodec = m_Tspcodec;
    else if (mIsEsVideo)
        pcodec = m_vpcodec;
    else if (mIsEsAudio)
        pcodec = m_apcodec;
    if(m_bSeekFlag && mHaveVideo)
    {
        if(cmcc_get_seek_after_first_pts() != -1L)
        {
             m_pfunc_play_evt(m_play_evt_hander, PLAYER_EVENT_FIRST_PTS,0,0);
             m_bSeekFlag = false;
        }
    }
    //property_get("media.player.cmcc_report.enable", value,"false");
    //report_flag= atoi(value);	
	if (pcodec == NULL)
	{
		ALOGI("updateCMCCInfo pcodec is NULL\n");
		return;
    }
	codec_get_av_param_info(pcodec, &av_param_info);
    //if (!strcmp(value,"true")) {
    if (1) {
        if(codec_get_upload(&av_param_info.av_info, &pquality_info)) {
            if(pquality_info.unload_flag) {
		  ALOGI("PLAYER_EVENT_UNDERFLOW_START\n");			
                m_pfunc_play_evt(m_play_evt_hander, PLAYER_EVENT_UNDERFLOW_START,0,0);
            }
            else{
		  ALOGI("PLAYER_EVENT_UNDERFLOW_END\n");	
                m_pfunc_play_evt(m_play_evt_hander, PLAYER_EVENT_UNDERFLOW_END,0,0);
            }			
        }
        if(codec_get_blurred_screen(&av_param_info.av_info, &pquality_info)) {
            if(pquality_info.blurred_flag){
				ALOGI("PLAYER_EVENT_BLURREDSCREEN_START\n");	
                m_pfunc_play_evt(m_play_evt_hander, PLAYER_EVENT_BLURREDSCREEN_START, 0,0);
            } else {
				ALOGI("PLAYER_EVENT_BLURREDSCREEN_END\n");	
                m_pfunc_play_evt(m_play_evt_hander, PLAYER_EVENT_BLURREDSCREEN_END,0,0);
            }
        }
	 }
}
bool CmccPlayer::threadCheckAbend() {
    ALOGV("threadCheckAbend start\n");
    do {
        usleep(30 * 1000);
        if (mIsTsStream && !mStopThread)
            checkBuffLevel();
        if (!mFisrtPtsReady) {
            if(amsysfs_get_sysfs_int("/sys/class/video/disable_video") == 0) {
                m_pfunc_play_evt(m_play_evt_hander, PLAYER_EVENT_FIRST_PTS, NULL, NULL);
                ALOGV("PLAYER_EVENT_FIRST_PTS");
                mFisrtPtsReady = true;
            }
        }
        if (m_bIsPlay && abs(av_gettime()-mUpdateInterval_us)>100000){
            update_nativewindow();
            mUpdateInterval_us = av_gettime();
        }
	if (m_bIsPlay) 
	    updateCMCCInfo();	
    }
    while(!mStopThread);
    ALOGV("threadCheckAbend end\n");
    return true;
}
CmccPlayer::CheckBuffLevelThread::CheckBuffLevelThread(CmccPlayer* player) :
		Thread(false), mPlayer(player),mTaskStatus(STATUE_STOPPED) {
}

int32_t CmccPlayer::CheckBuffLevelThread::start () {
	if(mTaskStatus == STATUE_STOPPED) {
		Thread::run("CheckBuffLevelThread");
		mTaskStatus = STATUE_RUNNING;
	} else if(mTaskStatus == STATUE_PAUSE) {
		mTaskStatus = STATUE_RUNNING;
	}
	return 0;
}

int32_t CmccPlayer::CheckBuffLevelThread::stop () {
	requestExitAndWait();
	mTaskStatus = STATUE_STOPPED;
	return 0;
}

int32_t CmccPlayer::CheckBuffLevelThread::pause () {
	mTaskStatus = STATUE_PAUSE;
	return 0;
}

bool CmccPlayer::CheckBuffLevelThread::threadLoop() {
	return mPlayer->threadCheckAbend();
}

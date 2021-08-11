//#include <android/log>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utils/String8.h>
#include <dlfcn.h>
#include <gui/Surface.h>
//#include <gui/ISurfaceTexture.h>
//#include <gui/SurfaceTextureClient.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>

#include <android/native_window.h>
#include <sys/prctl.h>
#include <cutils/properties.h>
#include <private/gui/ComposerService.h>
#include "FPlayer.h"
#include "FMediaPlayer.h"
//#include "HiMediaDefine.h"
//MEDIA_INFO_PREPARE_PROGRESS

#define FPLAYER_DLL_PATH "/system/lib/libfplayer.so"
#define FPLAYERHANDLE  (void*)((int)(0+'U'<<24+'M'<<16+'A'<<8+'V'))

namespace android
{

    static const status_t ERROR_NOT_OPEN = -1;
    static const status_t ERROR_OPEN_FAILED = -2;
    static const status_t ERROR_ALLOCATE_FAILED = -4;
    static const status_t ERROR_NOT_SUPPORTED = -8;
    static const status_t ERROR_NOT_READY = -16;
    static const status_t ERROR_TIMEOUT = -32;
    static const status_t STATE_ERROR_INITDEVICE = 0;
    static const status_t STATE_ERROR_INITPLAYER = 1;
    static const status_t STATE_ERROR = 2;
    static const status_t STATE_INIT = 3;
    static const status_t STATE_PREPARE = 4;
    static const status_t STATE_PLAY = 5;
    static const status_t STATE_STOPPING = 6;
    static const status_t STATE_STOPPED = 7;


static UM_HANDLE g_FPlayerDllHdl = NULL;
static UM_HANDLE g_FPlayer_API = NULL;


#define FPLAYER_CHECK_RETURN(Fplayer) \
do{\
     if(Fplayer == NULL)\
     	{\
     	     FLOGE("Fplayer is NULL");\
           return UNKNOWN_ERROR;\
	 }\    
}while(0)
static int do_nothing(status_t status, void* cookie, bool cancelled)
{
	return 0;
}

static UM_U32 getCurrentTime()
{
    struct timeval tv;

    if (0 == gettimeofday(&tv, NULL))
    {
        return (UM_U32)(tv.tv_sec % 10000000 * 1000 + tv.tv_usec / 1000);
    }

    return -1;
}
CommandQueue::CommandQueue(FMediaPlayer* player)
        : mCurCmd(NULL), mExcuting(true),
          mIsPlayerDestroyed(0), mCmdId(0), mPlayer(player), mState(STATE_IDLE),
          mCurSeekPoint(0), mSyncStatus(NO_ERROR), mSuspendState(STATE_IDLE)
    {
        FLOGV("FMediaPlayer::CommandQueue construct!");
        Mutex::Autolock l(mLock);

        createThreadEtc(CommandQueue::sched_thread, this,
                        "sched_thread", ANDROID_PRIORITY_DEFAULT, 0, NULL);
        
        mThreadCond.wait(mLock);
        FLOGV("%s mThreadCond.wait(mLock)",__FUNCTION__);
        FLOGV("FMediaPlayer::CommandQueue construct OUT!");
    }
CommandQueue::~CommandQueue()
    {
        Mutex::Autolock l(mLock);
        mExcuting = false;
        
        FLOGV("%s mThreadCond.signal()",__FUNCTION__);
        mThreadCond.signal();
        mThreadCond.wait(mLock);
         FLOGV("%s mThreadCond.wait(mLock)",__FUNCTION__);
        FLOGV("FMediaPlayer::CommandQueue() deconstruct");
    }
int CommandQueue::syncComplete(status_t status, void* cookie, bool cancelled)
    {
        CommandQueue* pQueue = (CommandQueue*)cookie;
        if (NULL != pQueue)
        {
            pQueue->mSyncStatus = status;
            Mutex::Autolock l(pQueue->mLock);
            //FLOGV("%s mSyncCond.signal()",__FUNCTION__);
            pQueue->mSyncCond.signal();
        }
        return NO_ERROR;
    }
inline int CommandQueue::wait()
{
    Mutex::Autolock l(mCallbackLock);
    UM_S32 ret = 0;//set the init value.

    ret = mCallbackCond.waitRelative(mCallbackLock, 2000 * 1000 * 1000);

    if (ret != 0)
    {
        FLOGW("WARN: HiMediaPlayer::CommandQueue::wait() out time for out ");
        return ERROR_TIMEOUT;
    }

    return NO_ERROR;
}
inline int CommandQueue::signal()
{
    Mutex::Autolock l(mCallbackLock);
    mCallbackCond.signal();
    return NO_ERROR;
}
    
int CommandQueue::enqueueCommand(Command* cmd)
    {
        if (NULL == cmd)
        {
            FLOGE("Command is NULL");
            return UNKNOWN_ERROR;
        }

        bool sync = false;
        int ret;

        if (mState == STATE_IDLE) //special deal with cmd when Preparing!!!
        {
            switch (cmd->getType())
            {
                case Command::CMD_GETPOSITION:
                    ret = doGetPosition(cmd);
                    delete cmd;
                    cmd = NULL;
                    return ret;
                case Command::CMD_GETDURATION:
                    ret =  doGetDuration(cmd);
                    delete cmd;
                    cmd = NULL;
                    return ret;
                case Command::CMD_ISPLAYING:
                    ret =  doIsPlaying(cmd);
                    delete cmd;
                    cmd = NULL;
                    return ret;
                case Command::CMD_PREPARE:
                    break;

                case Command::CMD_DESTROYPLAYER:
                    break;

               // case Command::CMD_SETVSINK:
               //     break;

                default:
                    delete cmd;
                    cmd = NULL;
                    return NO_ERROR;
            }
        }

        cmd->setId(mCmdId++);

        /* set the default callback for sync cmd */
        if (NULL == cmd->getCb())
        {
            cmd->setCb(CommandQueue::syncComplete);
            sync = true;
        }

        {
            Mutex::Autolock l(mLock);

            // after destroy operation is in queue,do not deal any operation.maybe command is enqueued
            // from hiPlayerEventCallback thread
            if (mIsPlayerDestroyed)
            {
                FLOGE("Player is destroyed,do not deal any operation,operation is type %d", cmd->getType());
                delete cmd;
                cmd = NULL;
                return NO_ERROR;
            }

            if (Command::CMD_DESTROYPLAYER == cmd->getType())
            {
                FLOGE("enque destroy player cmd");
                mIsPlayerDestroyed = true;
            }

            mQueue.insertAt(cmd, 0);

            /*wake up sched_thread */
            if (mQueue.size() == 1)
            {
                //FLOGV("%s mThreadCond.signal()",__FUNCTION__);
                mThreadCond.signal();
            }

            /* sync cmd, need wait */
            if (true == sync)
            {
               
                mSyncCond.wait(mLock);
                 //FLOGV("%s mSyncCond.wait(mLock)",__FUNCTION__);
                return mSyncStatus;
            }
        }

        return NO_ERROR;
    }
int CommandQueue::completeCommand(Command* cmd)
    {
        //LOGV("FMediaPlayer::CommandQueue::completeCommand type [%d]", cmd->getType());
        complete_cb pCallback = 0;

        if (NULL == cmd)
        {
            FLOGE("cmd is NULL");
            return UM_FAILURE;
        }
        if (0 != (pCallback = cmd->getCb()))
        {
            pCallback(cmd->getRet(), this, 0);
        }

        return 0;
    }
    int CommandQueue::run()
    {
        FLOGV("FMediaPlayer::CommandQueue::run()");
        //notify the construct, the thread is ok
        {
            Mutex::Autolock l(mLock);
            FLOGV("%s mThreadCond.signal()",__FUNCTION__);
            mThreadCond.signal();
        }

        while (mExcuting)
        {
            {
                //for autolock
                Mutex::Autolock l(mLock);

                if (mQueue.size() == 0)
                {
                    //for the out case, but the no cmds in queue!
                    if (false == mExcuting)
                    {
                        FLOGV("Command Queue sched thread go to exit!");
                        break;
                    }
                    
                    mThreadCond.wait(mLock);
                    //FLOGV("%s mThreadCond.wait(mLock)",__FUNCTION__);
                    continue;
                }

                mCurCmd = mQueue.top();
                mQueue.pop();
            }
            status_t ret = UNKNOWN_ERROR;
            UM_S32 CmdType = mCurCmd->getType();

            switch (CmdType)
            {
                case Command::CMD_PREPARE:
                    ret = doPrepare(mCurCmd);
                    break;

                case Command::CMD_START:
                    ret = doStart(mCurCmd);
                    break;

                case Command::CMD_STOP:
                    ret = doStop(mCurCmd);
                    break;

                case Command::CMD_PAUSE:
                    ret = doPause(mCurCmd);
                    break;

                case Command::CMD_RESUME:
                    ret = doResume(mCurCmd);
                    break;

                case Command::CMD_GETPOSITION:
                    ret = doGetPosition(mCurCmd);
                    break;

                case Command::CMD_GETDURATION:
                    ret = doGetDuration(mCurCmd);
                    break;

                case Command::CMD_SEEK:
                    ret = doSeek(mCurCmd);
                    break;

                case Command::CMD_ISPLAYING:
                    ret = doIsPlaying(mCurCmd);
                    break;

                case Command::CMD_INVOKE:
                    ret = doInvoke(mCurCmd);
                    break;

                case Command::CMD_DESTROYPLAYER:
                    ret = doDestroyPlayer(mCurCmd);
                    break;

                //case Command::CMD_SETVSINK:
                //    ret = doSetVsink(mCurCmd);
                //    break;

                default:
                    FLOGV("do nothing for default!");
                    break;
            }

            mCurCmd->setRet(ret);
            completeCommand(mCurCmd);
            //LOGV("Complete execute Cmd");
            /* notice: free the cmd there! */
            delete mCurCmd;
            mCurCmd = NULL;
        }

        FLOGV("signal thread out");
        //notify the desctruct: the thread is out
        {
            Mutex::Autolock l(mLock);
            FLOGV("mThreadCond.signal()");
            mThreadCond.signal();
        }
        FLOGV("sched_thread out");

        return 0;
    }
int CommandQueue::sched_thread(void* cookie)
    {
	CommandQueue* pQueue = (CommandQueue*)cookie;
	if (NULL == pQueue)
	{
		FLOGE("pQueue is NULL");
		return UM_FAILURE;
	}
	return pQueue->run();
    }

int CommandQueue::doPrepare(Command* cmd)
    {
        FLOGV("[%s] Call %s IN", "CommandQueue:", __FUNCTION__);

        if (NULL == cmd)
        {
            FLOGE("cmd is NULL");
            return UM_FAILURE;
        }
        FPlayer_API * Fplayer = (FPlayer_API*) cmd->getHandle();
		FPLAYER_CHECK_RETURN(Fplayer);
        F_PLAYER_MEDIA_S* pstMediaParam = NULL;
        UM_S32 s32Ret = UM_SUCCESS;
        UM_U32 start_time = getCurrentTime();

        pstMediaParam = (static_cast<CommandPrepare*>(cmd))->getParam();
        if (NULL == pstMediaParam)
        {
            FLOGE("media parameter is null, can not do prepare!");
            return UM_FAILURE;
        }

        //这里调用Prepare
	    Fplayer->setDataSourceAndHeaders((char*)pstMediaParam->aszUrl,NULL,NULL);
	    //sendEvent(android::MEDIA_INFO, android::MEDIA_INFO_PREPARE_PROGRESS, 50);
        Fplayer->prepareAsync();
        //sendEvent(android::MEDIA_INFO, android::MEDIA_INFO_PREPARE_PROGRESS, 60);
        mState = STATE_INIT;

        FLOGE("prepareAsync use time = %d ms", getCurrentTime() - start_time);
        return NO_ERROR;
    }

int CommandQueue::doStart(Command* cmd)
    {
        FLOGV("[%s] Call %s IN", "CommandQueue", __FUNCTION__);
        if (NULL == cmd)
        {
            FLOGE("cmd is NULL");
            return UM_FAILURE;
        }
        FPlayer_API * Fplayer = (FPlayer_API*) cmd->getHandle();
        FPLAYER_CHECK_RETURN(Fplayer);
        if (STATE_ERROR == mState || STATE_IDLE == mState || STATE_PLAY == mState)
        {
            FLOGW("doStart at wrong state %d", mState);
            return NO_ERROR;
        }

/*
        if (STATE_PAUSE == mState || STATE_FWD == mState || STATE_REW == mState)
        {
            if (UM_FAILURE == HI_SVR_PLAYER_Resume(mHandle))
            {
                LOGE("ERR: HI_SVR_PLAYER_Resume fail");
                return ERROR_OPEN_FAILED;
            }
        }
        else
        {

            if (UM_FAILURE == HI_SVR_PLAYER_Play(mHandle))
            {
                LOGE("ERR: HI_SVR_PLAYER_Play fail");
                return ERROR_OPEN_FAILED;
            }
        }
*/
        //调用 start 接口
		Fplayer->start();
             mState = STATE_PLAY;
/*
        UM_S32 ret = wait();

        //add for start use time > 2s to success
        if (ret == ERROR_TIMEOUT && mState != STATE_ERROR)
        {
            FLOGE("ERR: doStart TimeOut");
            ret = NO_ERROR;
            mState = STATE_PLAY;
        }

        if (mState == STATE_ERROR) //start asyn notify fail!
        {
            FLOGE("ERR: doStart fail");
            ret = UNKNOWN_ERROR;
        }
*/
        return NO_ERROR;
    }

int CommandQueue::doStop(Command* cmd)
    {
        FLOGV("[%s] Call %s IN", "CommandQueue", __FUNCTION__);
        if (NULL == cmd)
        {
            FLOGE("cmd is NULL");
            return UM_FAILURE;
        }
        FPlayer_API * Fplayer = (FPlayer_API*) cmd->getHandle();
        FPLAYER_CHECK_RETURN(Fplayer);
 
        if (STATE_IDLE == mState || STATE_STOP == mState)
        {
            FLOGW("don't stop before STATE_INIT or STATE_STOP");
            return NO_ERROR;
        }

        if (STATE_INIT == mState) //beacuse hiplayer don't support stop after setMedia immediately
        {
            FLOGW("stop at STATE_INIT");
            mState = STATE_STOP;
            return NO_ERROR;
        }

        //for freeze switch

/*
        if (UM_SUCCESS != HI_SVR_PLAYER_Stop((HI_HANDLE)mHandle))
        {
            LOGE("ERR: HI_SVR_PLAYER_Stop fail");
            return UNKNOWN_ERROR;
        }
*/
        //调用stop
        Fplayer->stop();
        //Fplayer->release();
        mState = STATE_STOP;
/*
        UM_S32 ret = wait();

        if (ret == ERROR_TIMEOUT) // add for stop use time > 2s to success
        {
             mState = STATE_STOP;
        }
*/
        return NO_ERROR;
    }

int CommandQueue::doPause(Command* cmd)
    {
        FLOGV("[%s] Call %s IN", "CommandQueue", __FUNCTION__);
        if (NULL == cmd)
        {
            FLOGE("cmd is NULL");
            return UM_FAILURE;
        }
        FPlayer_API * Fplayer = (FPlayer_API*) cmd->getHandle();
        FPLAYER_CHECK_RETURN(Fplayer);

        if (STATE_INIT == mState || STATE_ERROR == mState
            || STATE_IDLE == mState || STATE_PAUSE == mState
            || STATE_STOP == mState)
        {
            FLOGW("doPause at wrong state %d", mState);
            return NO_ERROR;
        }


        //调用pause接口
		Fplayer->pause();
        mState = STATE_PAUSE;
/*        UM_S32 ret = wait();

        if (ret == ERROR_TIMEOUT)
        {
            ret = NO_ERROR;
            mState = STATE_PAUSE;
            FLOGI("doPause: TimeOut!");
        }

        return ret;
        */
        return NO_ERROR;
    }

int CommandQueue::doResume(Command* cmd)
    {
        FLOGV("Call %s IN", __FUNCTION__);
        if (NULL == cmd)
        {
            FLOGE("cmd is NULL");
            return UM_FAILURE;
        }
        FPlayer_API * Fplayer = (FPlayer_API*) cmd->getHandle();
        FPLAYER_CHECK_RETURN(Fplayer);
        UM_S32 s32Ret = UM_FAILURE;

        if (STATE_PAUSE == mSuspendState)
        {
            return NO_ERROR;
        }
/*
        s32Ret = HI_SVR_PLAYER_Resume(mHandle);

        if (UM_SUCCESS != s32Ret)
        {
            LOGV("ERR: Resume fail, ret = 0x%x ", s32Ret);
            return UNKNOWN_ERROR;
        }
*/
        //调用resume接口
		bool is_player = Fplayer->isPlaying();
		if(!is_player)
		Fplayer->start();
            mState = STATE_PLAY;
/*
            UM_S32 ret = wait();

        if (ret == ERROR_TIMEOUT)
        {
            ret = NO_ERROR;
            mState = STATE_PLAY;
            FLOGI("doResume: TimeOut!");
        }

        return ret;
*/
       return NO_ERROR;
        
    }

int CommandQueue::doGetPosition(Command* cmd)
    {
       //FLOGV("Call %s IN", __FUNCTION__);
        if (NULL == cmd)
        {
            FLOGE("cmd is NULL");
            return UM_FAILURE;
        }
        FPlayer_API * Fplayer = (FPlayer_API*) cmd->getHandle();
        FPLAYER_CHECK_RETURN(Fplayer);
        UM_S32* position = (static_cast<CommandGetPosition*>(cmd))->getPosition();
        if(STATE_IDLE == mState || STATE_INIT == mState)
        {
            *position = 0;
             FLOGD("idle or init set position to 0");
        }
        //调用getposition
		* position = ((int)Fplayer->getCurrentPosition());
        //FLOGD("doGetPosition out *position:%d", *position);

        return NO_ERROR;
    }

int CommandQueue::doGetDuration(Command* cmd)
    {
        //FLOGV("Call %s IN", __FUNCTION__);
        if (NULL == cmd)
        {
            FLOGE("cmd is NULL");
            return UM_FAILURE;
        }
        FPlayer_API * Fplayer = (FPlayer_API*) cmd->getHandle();
        FPLAYER_CHECK_RETURN(Fplayer);


        UM_S32* duration = (static_cast<CommandGetDuration*>(cmd))->getDuration();

        if (STATE_IDLE == mState || STATE_INIT == mState)
        {
            *duration = 0;
             return NO_ERROR;
        }
        //调用getduration
		*duration=Fplayer->getDuration();
		if(*duration+100>=0)
		mDuration = *duration;
        return NO_ERROR;
    }


int CommandQueue::doSeek(Command* cmd)
    {
        FLOGV("[%s] Call %s IN", "CommandQueue", __FUNCTION__);
        if (NULL == cmd)
        {
            FLOGE("cmd is NULL");
            return UM_FAILURE;
        }
        FPlayer_API * Fplayer = (FPlayer_API*) cmd->getHandle();
        FPLAYER_CHECK_RETURN(Fplayer);
        UM_S32 position = (static_cast<CommandSeek*>(cmd))->getPosition();

        long int msec = position;
        if (STATE_PAUSE == mState)
        {
            msec = (position >= 0 ? position : -position);
        }
        //long int msec =position;
        //调用 seek接口
		Fplayer->seekTo(msec);
        return NO_ERROR;
    }
   int CommandQueue::doInvoke(Command* cmd)
    {
        FLOGV("Call %s IN", __FUNCTION__);
        status_t   ret = OK;
        UM_S32     s32Ret = 0;
        UM_S32     arg = 0;

        if (NULL == cmd)
        {
            FLOGE("Invalid parameter");
            return UM_FAILURE;
        }
        FPlayer_API * Fplayer = (FPlayer_API*) cmd->getHandle();
        FPLAYER_CHECK_RETURN(Fplayer);
        Parcel* request = (static_cast<CommandInvoke*>(cmd))->getRequest();
        Parcel* reply   = (static_cast<CommandInvoke*>(cmd))->getReply();
        //cmd_type_e cmd_type = (cmd_type_e)request->readInt32();

        //����
        return NO_ERROR;
    }
int CommandQueue::doIsPlaying(Command* cmd)
    {
        FLOGV("Call %s IN", __FUNCTION__);
        if (NULL == cmd)
        {
            FLOGE("cmd is NULL");
            return UM_FAILURE;
        }
		FPlayer_API * Fplayer = (FPlayer_API*) cmd->getHandle();
        FPLAYER_CHECK_RETURN(Fplayer);
        bool* bIsPlaying = (static_cast<CommandIsPlaying*>(cmd))->getPlaying();
        if(STATE_INIT == mState || STATE_IDLE == mState || STATE_STOP == mState ||STATE_PAUSE == mState)
            {
         *bIsPlaying = false;
         return NO_ERROR;
        }
 //       *bIsPlaying = (STATE_PLAY == mState || STATE_FWD == mState || STATE_REW == mState);
//用上面的或者用自己的接口  isplaying
        *bIsPlaying = (Fplayer->isPlaying() && (STATE_PLAY == mState || STATE_FWD == mState || STATE_REW == mState));
        return NO_ERROR;
    }

int CommandQueue::doDestroyPlayer(Command* cmd)
    {
        FLOGV("Call %s IN", __FUNCTION__);
        if (NULL == cmd)
        {
            FLOGE("cmd is NULL");
            return UM_FAILURE;
        }
        UM_S32 s32Ret = UM_SUCCESS;
 		FPlayer_API * Fplayer = (FPlayer_API*) cmd->getHandle();
        FPLAYER_CHECK_RETURN(Fplayer);
/*
        if (mState != STATE_STOPPED)
        {
            if (NULL != mPlayer)
            {
                mPlayer->sendEvent(MEDIA_STOPPED, 0, 0, NULL);
            }
        }
*/
        //调接口
		Fplayer->reset();
        //Fplayer->invoke(0,NULL);
        return NO_ERROR;
    }

UM_HANDLE  openDllClass(char* dllname,FPlayer_API**player_api)
{
    UM_HANDLE handle=NULL;	
    FPlayer_API* FPlayer_api=NULL;
    handle = dlopen(dllname, RTLD_LAZY);
    if (! handle) 
    {   
        FLOGV(" Call %s IN", __FUNCTION__);  
		
	     FLOGV("%s, NULL == handle error=%s", __FUNCTION__,dlerror()  );
        *player_api =NULL;
	 return NULL;    
    }   

    FPlayer_api =(FPlayer_API*) dlsym(handle, "fplayer");
    if (!FPlayer_api) {
	    FLOGV("%s,%d, NULL == handle error=%s\n", __FUNCTION__, __LINE__,dlerror());
          *player_api =NULL;
           dlclose(handle);
	    return NULL;    
    }
    *player_api=FPlayer_api;
    return handle;

}	
void closeDllClass(void* handle)
{
    if(handle)
       dlclose(handle);
}

int FgetintfromString8(String8 &s,const char*pre)
{
	int off;
	int val=0;
	if((off=s.find(pre,0))>=0){
		sscanf(s.string()+off+strlen(pre),"%d",&val);
	}
	return val;
}
	
 FMediaPlayer::FMediaPlayer():
        mDuration(0),
        mPosition(0),
		mPlayer(0),
		mListener(0),
		mVideoSurfaceTexture(0)
    {
        int ret;
        FLOGV("[%s] FMediaPlayer construct", __FUNCTION__);
        UM_HANDLE player_api = NULL;
        /*ret = initResource();//可能会需要，暂时先不用
        if (HI_SUCCESS != ret)
        {
            LOGE("initResource fail, ret:%d", ret);
        }*/	
        if(!g_FPlayerDllHdl)
		{
            g_FPlayerDllHdl = openDllClass(FPLAYER_DLL_PATH,(FPlayer_API**)&player_api);
			if(g_FPlayerDllHdl && player_api)
			{
				mPlayer = player_api;
				g_FPlayer_API = mPlayer;
			}else{
				FLOGV("[%s] FMediaPlayer openDllClass err ", __FUNCTION__);
			}
		}
        mCmdQueue = new CommandQueue(this);
		mPlayer = g_FPlayer_API;
		mState = STATE_INIT;
    }	
FMediaPlayer::~FMediaPlayer()
{


}
void  FMediaPlayer::setListener(const wp<MediaPlayerBase> &listener)
{
	mListener = listener;
}

int FMediaPlayer::createFplayer()
    {
        UM_S32 s32Ret = UM_SUCCESS;
        UM_S32 FromSurfaceFlinger = -1;

        FLOGV("[%s] start create FPlayer...", __FUNCTION__);


        if (NULL == mCmdQueue.get())
        {
            FLOGE("Create player,create queue fail");
            return UM_FAILURE;
        }

        //Fplayer->setPostEvent((void*)this,(void*)(&FMediaPlayer::FPlayerCallBack));
        mState = STATE_INIT;

        FLOGV("[%s] create FPlayer...done", __FUNCTION__);

        return UM_SUCCESS;
    }	
status_t   FMediaPlayer::initCheck()
{
    int i =0;
    FLOGV(" Call %s IN", __FUNCTION__);
    if(mState != STATE_ERROR)
        return NO_ERROR;
    else
	 return UNKNOWN_ERROR;
}
	
#if (PLATFORM_SDK_VERSION >= 21)
status_t FMediaPlayer::setDataSource(const sp<IMediaHTTPService>& httpService, const char* uri, const KeyedVector<String8, String8>* headers)
#else
status_t FMediaPlayer::setDataSource(const char* uri, const KeyedVector<String8, String8>* headers)
#endif
{
    FLOGV("%s FMediaPlayer::setDataSource uri[%s]",__FUNCTION__, uri);
	FPlayer_API * Fplayer = (FPlayer_API*)mPlayer;
    FPLAYER_CHECK_RETURN(Fplayer);
    memset(&mMediaParam, 0, sizeof(mMediaParam));
    //char* url1 = "http://192.168.77.3:8080/dabkkckkekekellall50.ts";
    //strncpy(mMediaParam.aszUrl, url1, (size_t)UM_FORMAT_MAX_URL_LEN - 1);
    strncpy(mMediaParam.aszUrl, uri, (size_t)UM_FORMAT_MAX_URL_LEN - 1);
#if (PLATFORM_SDK_VERSION >= 21)
    mHTTPService =  httpService;
#endif

    char *pChar0D0A = mMediaParam.aszUrl;
    while(pChar0D0A && *pChar0D0A != '\0')
    {
        if(*pChar0D0A == 0x0D || *pChar0D0A == 0x0A)
        {
            *pChar0D0A = 0;
            FLOGV("%s uri[%s] have char [0D0A], cut to [%s]",uri, mMediaParam.aszUrl);
            break;
        }
        pChar0D0A++;
    }

    //mFirstPlay = true;

    //sendEvent(android::MEDIA_INFO, android::MEDIA_INFO_PREPARE_PROGRESS, 10);
  

    FLOGV("%s FMediaPlayer::setDataSource OK  url=%s",uri);
    //SetDatasourcePrepare(mMediaParam.aszUrl);//对接的地方!!

    return NO_ERROR;
}
status_t setDataSource(int fd, int64_t offset, int64_t length)
{
    return NO_ERROR;
}
status_t FMediaPlayer::prepareAsync()
{
    return prepare();

}
status_t FMediaPlayer::prepare()
{
    FLOGV(" Call %s IN", __FUNCTION__);

	FPlayer_API * Fplayer = (FPlayer_API*)mPlayer;
    FPLAYER_CHECK_RETURN(Fplayer);
    int i = 0;
    status_t ret=NO_ERROR;
    
    if(createFplayer() != UM_SUCCESS)
        {
          FLOGE("createFplayer failed !");
    }
	if (NULL == mCmdQueue.get())
	{
		FLOGV("mCmdQueue is NULL,execute command return");
		return NO_ERROR;
	}
    Fplayer->native_init();
    Fplayer->setPostEvent((void*)this,(void*)(&FMediaPlayer::FPlayerCallBack));
    //sendEvent(android::MEDIA_INFO, android::MEDIA_INFO_PREPARE_PROGRESS, 20);
    Fplayer->native_setup(FPLAYERHANDLE);
    //if(mWidth>0 && mHeight>0)
    //        Fplayer->setVideoSurface(mVideoSurfaceTexture,mLeft,mTop,(mWidth==720)?719:mWidth,(mHeight==1280)?1279:mHeight);
    //else
    //        Fplayer->setVideoSurface(mVideoSurfaceTexture,0,0,719,1279);

	//sendEvent(android::MEDIA_INFO, android::MEDIA_INFO_PREPARE_PROGRESS, 40);

	ret = mCmdQueue->enqueueCommand(new CommandPrepare(mPlayer, mMediaParam));

    return ret;
}

status_t   FMediaPlayer::start()
{
	FPlayer_API * Fplayer = (FPlayer_API*)mPlayer;
    FPLAYER_CHECK_RETURN(Fplayer);
    FLOGV(" Call %s IN", __FUNCTION__);    
    int i =0;
	status_t ret=NO_ERROR;
	if (NULL == mCmdQueue.get())
	{
		FLOGV("mCmdQueue is NULL,execute command return");
		return NO_ERROR;
	}
    ret = mCmdQueue->enqueueCommand(new CommandStart(mPlayer));

    //sp<FsurfaceSetting> mss = getSurfaceSetting();
    //mss->setWindow(FPLAYER_WIN_HANDLE);
    //mss->updateSurfacePosition(mSurfaceX,mSurfaceY, mSurfaceWidth, mSurfaceHeight);
    //Fplayer->start();


    return ret;
}

status_t   FMediaPlayer::stop()
{
	FPlayer_API * Fplayer = (FPlayer_API*)mPlayer;
    FPLAYER_CHECK_RETURN(Fplayer);
    int i =0;
	status_t ret=NO_ERROR;
    FLOGV(" Call %s IN", __FUNCTION__);
	if (NULL == mCmdQueue.get())
	{
		FLOGV("mCmdQueue is NULL,execute command return");
		return NO_ERROR;
	}
    ret = mCmdQueue->enqueueCommand(new CommandStop(mPlayer, do_nothing));

    return ret;
}

status_t   FMediaPlayer::pause()
{
	status_t ret = NO_ERROR;
	FPlayer_API * Fplayer = (FPlayer_API*)mPlayer;
    FPLAYER_CHECK_RETURN(Fplayer);

    int i =0;
    FLOGV(" Call %s IN", __FUNCTION__);
	if (NULL == mCmdQueue.get())
	{
		FLOGV("mCmdQueue is NULL,execute command return");
		return NO_ERROR;
	}

	ret = mCmdQueue->enqueueCommand(new CommandPause(mPlayer));

    return NO_ERROR;
}

bool   FMediaPlayer::isPlaying()
{
	FPlayer_API * Fplayer = (FPlayer_API*)mPlayer;
    if(!Fplayer)
        return false;
    //FPLAYER_CHECK_RETURN(Fplayer);
    int i =0;
    FLOGV(" Call %s IN", __FUNCTION__);
	bool bIsPlaying = false;

	if (NULL == mCmdQueue.get())
	{
		FLOGV("mCmdQueue is NULL,execute command return");
		return NO_ERROR;
	}
    mCmdQueue->enqueueCommand(new CommandIsPlaying(mPlayer, &bIsPlaying));


    return bIsPlaying;
}

status_t   FMediaPlayer::seekTo(int position)
{
	FPlayer_API * Fplayer = (FPlayer_API*)mPlayer;
    FPLAYER_CHECK_RETURN(Fplayer);
        FLOGV(" Call %s IN", __FUNCTION__);
    int i =0;
	status_t ret;


	if (NULL == mCmdQueue.get())
	{
		FLOGV("mCmdQueue is NULL,execute command return");
		return NO_ERROR;
	}
	ret = mCmdQueue->enqueueCommand(new CommandSeek(mPlayer, (position >= mPosition) ? position : -position));	
    return ret;
}
status_t   FMediaPlayer::resume()
{
	FPlayer_API * Fplayer = (FPlayer_API*)mPlayer;
    FPLAYER_CHECK_RETURN(Fplayer);
    int i =0;
    FLOGV(" Call %s IN", __FUNCTION__);
	if (NULL == mCmdQueue.get())
	{
		FLOGV("mCmdQueue is NULL,execute command return");
		return NO_ERROR;
	}

	return mCmdQueue->enqueueCommand(new CommandResume(mPlayer));
}
status_t   FMediaPlayer::getCurrentPosition(int* position)
{
	FPlayer_API * Fplayer = (FPlayer_API*)mPlayer;
    FPLAYER_CHECK_RETURN(Fplayer);
    int i =0;

    //FLOGV(" Call %s IN", __FUNCTION__);

	if (NULL == mCmdQueue.get())
	{
		FLOGV("mCmdQueue is NULL,execute command return");
		return NO_ERROR;
	}
    status_t ret = mCmdQueue->enqueueCommand(new CommandGetPosition(mPlayer, &mPosition));
    * position = ((int)mPosition);
	if(*position > mDuration)
		* position = mDuration;
    //FLOGV(" Call %s ret=%d", __FUNCTION__,*position);
    return ret;
}

status_t   FMediaPlayer::getDuration(int* duration)
{
	FPlayer_API * Fplayer = (FPlayer_API*)mPlayer;
    FPLAYER_CHECK_RETURN(Fplayer);
    int i =0;
    //FLOGV(" Call %s IN", __FUNCTION__);
   if (NULL == mCmdQueue.get() || NULL == duration)
	{
		FLOGV("mCmdQueue is NULL,execute command return");
		return NO_ERROR;
	}

	status_t ret = NO_ERROR;

	if (mDuration > 0)
	{
		*duration = mDuration;
	}
	else
	{
		ret = mCmdQueue->enqueueCommand(new CommandGetDuration(mPlayer, duration));
		mDuration = *duration;
	}

    //FLOGV(" Call %s  ret=%d", __FUNCTION__,*duration);
	return ret;

}

void FMediaPlayer::destroyFPlayer()
{
	FPlayer_API * Fplayer = (FPlayer_API*)mPlayer;
        FLOGV(" Call %s IN", __FUNCTION__);
    if(Fplayer == NULL);
        {
        FLOGE(" Call %s  Fplayer null ", __FUNCTION__);
        return ;
    }
    int i =0;
    FLOGV(" Call %s IN", __FUNCTION__);
	mCmdQueue->enqueueCommand(new CommandDestroyPlayer(mPlayer));
  return ;	
}

status_t   FMediaPlayer::reset()
{
	FPlayer_API * Fplayer = (FPlayer_API*)mPlayer;
    FPLAYER_CHECK_RETURN(Fplayer);
    int i =0;
    FLOGV(" Call %s IN", __FUNCTION__);
	if (NULL == mCmdQueue.get())
	{
		FLOGV("mCmdQueue is NULL,execute command return");
		return NO_ERROR;
	}	
#if (PLATFORM_SDK_VERSION >= 21)
	if (mHTTPService.get())
	{
		mHTTPService.clear();
	}
#endif	
    mCmdQueue->enqueueCommand(new CommandDestroyPlayer(mPlayer));
    //Fplayer->reset();
    //Fplayer->native_deinit();
    //mSeekNum=0;
    return NO_ERROR;
}

status_t FMediaPlayer::setVideoSurfaceTexture(const void *VideoSurfaceTexture)
{
	//    Mutex::Autolock autoLock(mLock);
	    FLOGV(" Call %s IN", __FUNCTION__);
	FLOGD("FMediaPlayer::setSurfaceTexture");
	status_t err = NO_ERROR;

    mVideoSurfaceTexture = (void *)VideoSurfaceTexture;
	return err;
}

status_t FMediaPlayer::setParameter(int key, const Parcel &request)
{
	//Mutex::Autolock autoLock(mMutex);
	    FLOGV(" Call %s IN", __FUNCTION__);
	FLOGI("setParameter %d\n",key);
	switch(key)
	{
		case 6009://KEY_PARAMETER_VIDEO_POSITION_INFO:
			{
			int left,right,top,bottom;
			Rect newRect;
			int off;
			const String16 uri16 = request.readString16();
			String8 keyStr = String8(uri16);
				FLOGI("setParameter %d=[%s]\n",key,keyStr.string());
			left=FgetintfromString8(keyStr,".left=");
			top=FgetintfromString8(keyStr,".top=");
			right=FgetintfromString8(keyStr,".right=");
			bottom=FgetintfromString8(keyStr,".bottom=");
			newRect=Rect(left,top,right,bottom);
			FLOGI("setParameter info to newrect=[%d,%d,%d,%d]\n",
				left,top,right,bottom);

			//update surface setting
			mLeft = left;
			mTop = top;
			mWidth = right-left;
			mHeight = bottom-top;
            	       FPlayer_API * Fplayer = (FPlayer_API*)mPlayer;
                    if(Fplayer)
                    {
                         UM_S32 rect[4];
                         rect[0] = mLeft;
                         rect[1] = mTop;
                         rect[2] = mWidth;
                         rect[3] = mHeight;
                         FLOGI("Fplayer->invoke INVOKE_SET_HAL_WINDOWS_SIZE");
                          Fplayer->invoke(/*INVOKE_SET_HAL_WINDOWS_SIZE*/7,rect);
                          
                    }
                    //FPlayerCallBack(this , MEDIA_SET_VIDEO_SIZE, mWidth, mHeight,NULL);
			//updateVideoPosition(left,top,right-left,bottom-top);
			break;
			}


		default:
			FLOGI("unsupport setParameter value!=%d\n",key);
	}
	return OK;
}	

status_t	  FMediaPlayer::getParameter(int key, Parcel *reply)
{
    FLOGV(" Call %s IN", __FUNCTION__);
    return OK;
}
status_t    FMediaPlayer::setLooping(int loop)
{
    FLOGV(" Call %s IN", __FUNCTION__);
return OK;
}
    status_t FMediaPlayer::invoke(const Parcel& request, Parcel* reply)
    {
         int cmd_type = (int)request.readInt32();
        FLOGV(" Call %s IN  cmd_type=%d", __FUNCTION__,cmd_type);
     int Ret = NO_ERROR;

    /*
        int HasPreDeal = 0;
       

        int CurrentPos = request.dataPosition();
        Ret = DealPreInvokeSetting(request, reply, &HasPreDeal);
        request.setDataPosition(CurrentPos);

        if (HasPreDeal)
        {
            return Ret;
        }

        if (NULL == mCmdQueue.get())
        {
            cmd_type_e cmd_type = (cmd_type_e)request.readInt32();

            LOGV("invoke cmd = %d ", cmd_type);

            if (CMD_SET_TIMESHIFT_DURATION == cmd_type)
            {
                mTimeShiftDuration = request.readInt32();
                LOGV("set timeshift duration = %d ", mTimeShiftDuration);
                if (NULL != reply)
                    reply->writeInt32(NO_ERROR);

                LOGV("invoke leave, cmd [%d]", cmd_type);
                return NO_ERROR;
            }
            else
            {
                LOGV("mCmdQueue is NULL,execute command return");
            }
            REPLY_INT32(HI_FAILURE);
            return HI_FAILURE;
        }

        Ret =  mCmdQueue->enqueueCommand(new CommandInvoke(mHandle, (Parcel*)&request, reply));

        if (mInvokeResult != 0) //invoke cmd is invalid type,try custom invoke
        {
            request.setDataPosition(CurrentPos);
            if (NULL != reply)
            {
                reply->setDataPosition(0);
            }
            Ret = CustomInvoke(request, reply);
            mInvokeResult = 0;
        }
*/
        return Ret;
    }
 void  FMediaPlayer::FPlayerCallBack(void* weak_thiz , int what, int arg1, int arg2, void* obj)
{
    if(MEDIA_BUFFERING_UPDATE !=what)FLOGV(" ****Call FPlayerCallBack IN****  msg=%x %d %d %d %x",weak_thiz,what,arg1,arg2,obj);

    FMediaPlayer* p=(FMediaPlayer* )weak_thiz;

	
    if(p== NULL)
    {
      FLOGV("weak_thiz is null");
      return ;
    }
      //sp<MediaPlayerBase> listener = mListener.promote();
	sp<MediaPlayerBase> cb = p->mListener.promote(); 
	if(cb == NULL)
		return ;
    switch(what)
    {
	case MEDIA_PREPARED:
		//LOGV("event MEDIA_PREPARED");
		cb->sendEvent(MEDIA_PREPARED);
		break;
      case MEDIA_NOP:
	  	cb->sendEvent(MEDIA_NOP);
	  	//LOGV("event MEDIA_NOP");
	  	break;
      case MEDIA_PLAYBACK_COMPLETE:
	  	cb->sendEvent(MEDIA_PLAYBACK_COMPLETE);
	  	//LOGV("event MEDIA_PLAYBACK_COMPLETE");
	  	break;
      case MEDIA_BUFFERING_UPDATE:
	  	cb->sendEvent(MEDIA_BUFFERING_UPDATE,(arg1),arg2);
	  	//LOGV("event MEDIA_BUFFERING_UPDATE");
	  	break;
      case MEDIA_SEEK_COMPLETE:
	  	cb->sendEvent(MEDIA_SEEK_COMPLETE,arg1,arg2);
	  	//LOGV("event MEDIA_SEEK_COMPLETE");
	       break;
      case MEDIA_SET_VIDEO_SIZE:
	  	cb->sendEvent(MEDIA_SET_VIDEO_SIZE,arg1,arg2);
	  	//LOGV("event MEDIA_SET_VIDEO_SIZE");
	  	break;
      case MEDIA_TIMED_TEXT :
	  	cb->sendEvent(MEDIA_TIMED_TEXT,arg1,arg2);
	  	//LOGV("event MEDIA_TIMED_TEXT");
	  	break;
      case MEDIA_ERROR:
	  	cb->sendEvent(MEDIA_ERROR,arg1,arg2);
	  	//LOGV("event MEDIA_ERROR");
	  	break;
      case MEDIA_INFO:
               cb->sendEvent(MEDIA_INFO,arg1,arg2);
               break;
/*     case MEDIA_INFO_EXTEND_BUFFER_LENGTH:
		
             cb->sendEvent(android::MEDIA_INFO, MEDIA_INFO_EXTEND_BUFFER_LENGTH, arg1);
		FLOGV("event MEDIA_INFO_EXTEND_BUFFER_LENGTH buf_len=%d , %d",arg1,arg2);
             break;
     case MEDIA_INFO_BUFFERED_DATA_LEN:
	 	
	 	FLOGV("event MEDIA_INFO_BUFFERED_DATA_LEN buf_len=%d , %d ",arg1,arg2);
*/	default:
		
		//LOGV("event xxxxxxxxxxxxxx");
	       break;
    }
    cb = NULL;

}

/*
        void        sendEvent(int msg, int ext1 = 0, int ext2 = 0,
                              const Parcel* obj = NULL)
        {
            Mutex::Autolock autoLock(mNotifyLock);
            if (mNotify) { mNotify(mCookie, msg, ext1, ext2, obj); }
        }
*/
}

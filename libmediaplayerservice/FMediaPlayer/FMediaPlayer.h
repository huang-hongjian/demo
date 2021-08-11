#ifndef ANDROID_FPLAYER_H
#define ANDROID_FPLAYER_H
#include <utils/Log.h>
#include <utils/RefBase.h>
#include <media/MediaPlayerInterface.h>
//#include <android/log.h>
#include <stdlib.h>
#include "um_basictypes.h"

namespace android {
class Command;
class CommandQueue;
class FMediaPlayer;	
#define UM_FORMAT_MAX_URL_LEN 3072

#define  FLOGI(...) __android_log_print(ANDROID_LOG_INFO, "FMediaPlayer", __VA_ARGS__)
#define  FLOGW(...) __android_log_print(ANDROID_LOG_WARN, "FMediaPlayer", __VA_ARGS__)
#define  FLOGE(...) __android_log_print(ANDROID_LOG_ERROR, "FMediaPlayer", __VA_ARGS__)
#define  FLOGV(...) __android_log_print(ANDROID_LOG_INFO, "FMediaPlayer", __VA_ARGS__)
#define  FLOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "FMediaPlayer", __VA_ARGS__)

typedef struct F_PLAYER_MEDIA_S_
{
	UM_CHAR  aszUrl[UM_FORMAT_MAX_URL_LEN];
}F_PLAYER_MEDIA_S;
    typedef int (*complete_cb)(android::status_t status, void* cookie, bool cancelled);
 
    struct Command
    {
    public:
        enum code
        {
            CMD_PREPARE = 0,
            CMD_START,
            CMD_STOP,
            CMD_PAUSE,
            CMD_SUSPEND,
            CMD_RESUME = 5,
            CMD_SEEK,
            CMD_GETPOSITION,
            CMD_GETDURATION,
            CMD_ISPLAYING,
            CMD_INVOKE = 10,
            CMD_DESTROYPLAYER,
            CMD_SETVSINK,
        };
        Command(int t, UM_HANDLE handle, complete_cb c, void* ck = 0, int index = 0)
            : mType(t), mHandle(handle), mCb(c), mCookie(ck), mRet(UM_FAILURE), mId(index) {}
        int   getType()
        {
            return mType;
        }

        int   getRet()
        {
            return mRet;
        }

        int   getId()
        {
            return mId;
        }

        void* getCookie()
        {
            return mCookie;
        }

        UM_HANDLE getHandle()
        {
            return mHandle;
        }

        complete_cb getCb()
        {
            return mCb;
        }

        void  setRet(int ret)
        {
            mRet = ret;
        }

        void  setCookie(void* cookie)
        {
            mCookie = cookie;
        }

        void  setId(int id)
        {
            mId = id;
        }

        void  setCb(complete_cb cb)
        {
            mCb = cb;
        }

    private:
        int          mType; // cmd type
        UM_HANDLE    mHandle; // Fplayer handle
        complete_cb  mCb;  // cmd complete callback
        void*        mCookie; // user data
        int          mRet; // ret code;
        int          mId;  // cmd seq index
    };
   class CommandPrepare : public Command
    {
    public:
        CommandPrepare(UM_HANDLE handle, F_PLAYER_MEDIA_S& pMediaParam, complete_cb cb = 0)
            : Command(CMD_PREPARE, handle, cb), mParam(pMediaParam) {};
        F_PLAYER_MEDIA_S* getParam() { return &mParam; }

    private:
        CommandPrepare();
        F_PLAYER_MEDIA_S mParam;
    };

    class CommandStart : public Command
    {
    public:
        CommandStart(UM_HANDLE handle, complete_cb cb = 0)
            : Command(CMD_START, handle, cb) {};
    private:
        CommandStart();
    };

    class CommandStop : public Command
    {
    public:
        CommandStop(UM_HANDLE handle, complete_cb cb = 0)
            : Command(CMD_STOP, handle, cb){};
    private:
        CommandStop();
    };

    class CommandPause  : public Command
    {
    public:
        CommandPause(UM_HANDLE handle, complete_cb cb = 0)
            : Command(CMD_PAUSE, handle, cb) {};
    private:
        CommandPause();
    };

    class CommandSuspend  : public Command
    {
    public:
        CommandSuspend(UM_HANDLE handle, complete_cb cb = 0)
            : Command(CMD_SUSPEND, handle, cb) {};
    private:
        CommandSuspend();
    };

    class CommandResume : public Command
    {
    public:
        CommandResume(UM_HANDLE handle, complete_cb cb = 0)
            : Command(CMD_RESUME, handle, cb) {};
    private:
        CommandResume();
    };

    class CommandSeek : public Command
    {
    public:
        CommandSeek(UM_HANDLE handle, int pos, complete_cb cb = 0)
            : Command(CMD_SEEK, handle, cb), mPos(pos) {};
        int getPosition() { return mPos; }

    private:
        CommandSeek();
        int mPos;
    };

    class CommandGetPosition : public Command
    {
    public:
        CommandGetPosition(UM_HANDLE handle, int* pos, complete_cb cb = 0)
            : Command(CMD_GETPOSITION, handle, cb), mPos(pos) {};
        int* getPosition() { return mPos; }

    private:
        CommandGetPosition();
        int* mPos;
    };

    class CommandGetDuration : public Command
    {
    public:
        CommandGetDuration(UM_HANDLE handle, int* dur, complete_cb cb = 0)
            : Command(CMD_GETDURATION, handle, cb), mDur(dur) {};
        int* getDuration() { return mDur; }

    private:
        CommandGetDuration();
        int* mDur;
    };

    class CommandIsPlaying  : public Command
    {
    public:
        CommandIsPlaying(UM_HANDLE handle, bool* playing, complete_cb cb = 0)
            : Command(CMD_ISPLAYING, handle, cb), mPlaying(playing) {};
        bool* getPlaying() { return mPlaying; }

    private:
        CommandIsPlaying();
        bool* mPlaying;
    };

    class CommandInvoke : public Command
    {
    public:
        CommandInvoke(UM_HANDLE handle, Parcel* req, Parcel* reply, complete_cb cb = 0)
            : Command(CMD_INVOKE, handle, cb), mReq(req), mReply(reply) {};
        Parcel* getRequest() { return mReq; }

        Parcel* getReply()   { return mReply; }

    private:
        CommandInvoke();
        Parcel* mReq;
        Parcel* mReply;
    };

    class CommandDestroyPlayer : public Command
    {
    public:
        CommandDestroyPlayer(UM_HANDLE handle, complete_cb cb = 0)
            : Command(CMD_DESTROYPLAYER, handle, cb) {};
    private:
        CommandDestroyPlayer();
    };

	class CommandQueue : public RefBase
	{
		public:

			enum state
			{
				STATE_IDLE,
				STATE_INIT,
				STATE_PLAY,
				STATE_FWD,
				STATE_REW,
				STATE_STOP,
				STATE_PAUSE,
				STATE_DESTROY,
				STATE_ERROR,
			};
			CommandQueue(FMediaPlayer* player);
			~CommandQueue();
			int enqueueCommand(Command* cmd);
			int dequeueCommand();
			int run();
			FMediaPlayer* getPlayer() { return mPlayer; };
			int wait();
			int signal();
			void          setState(int state) { mState = (int)state;}
			int          getState() const { return mState;}
			Command*        getCurCmd() { return mCurCmd; }

		private:
		    
			Vector <Command*>  mQueue;
			Command*      mCurCmd;
			Mutex mLock;         //for sched_thread and command complete sync
			Mutex mCallbackLock; //for hiplayer async invoke callback
			Condition mSyncCond;     //for sync command complete
			Condition mCallbackCond; //for hiplayer async invoke sync
			Condition mThreadCond;   //for sched_thread and the main thread sync
			bool mExcuting;
			bool mIsPlayerDestroyed;
			int mCmdId;
			FMediaPlayer*     mPlayer;
			int mState;
			int mCurSeekPoint;
			status_t mSyncStatus;
			int mSuspendState;
                    int mDuration;

			int completeCommand(Command* cmd);
			int doPrepare(Command* cmd);
			int doStart(Command* cmd);
			int doStop(Command* cmd);
			int doPause(Command* cmd);
			int doResume(Command* cmd);
			int doGetPosition(Command* cmd);
			int doGetDuration(Command* cmd);
			int doSeek(Command* cmd);
			int doIsPlaying(Command* cmd);
			int doInvoke(Command* cmd);
			int doDestroyPlayer(Command* cmd);
			//int doSetVsink(Command* cmd);
			static int syncComplete(status_t status, void* cookie, bool cancelled);
			static int sched_thread(void* cookie);


	};


bool isFMediaPlayer(const char* url);

class FMediaPlayer
{
public:
    FMediaPlayer();
    ~FMediaPlayer();

	void 				setListener(const wp<MediaPlayerBase> &listener);
    status_t    		initCheck();
#if (PLATFORM_SDK_VERSION >= 21)
    status_t setDataSource(const sp<IMediaHTTPService>& httpService, const char* uri, const KeyedVector<String8, String8>* headers);
#else
    status_t setDataSource(const char* uri, const KeyedVector<String8, String8>* headers);
#endif
status_t setDataSource(int fd, int64_t offset, int64_t length);

    status_t    		setVideoSurfaceTexture(const void *VideoSurfaceTexture);

    status_t    		prepare();
   	status_t    		prepareAsync();
    status_t    		start();
    status_t    		stop();
    status_t    		pause();
        status_t    		resume();
    bool        		isPlaying();
    status_t    		seekTo(int msec);
    status_t    		getCurrentPosition(int* msec);
    status_t    		getDuration(int* mDuration);
    status_t   			reset();
    status_t    		setLooping(int loop);
    status_t    		invoke(const Parcel &request, Parcel *reply);
    status_t	        setParameter(int key, const Parcel &request);
   	status_t			getParameter(int key, Parcel *reply); 
	//status_t    		onMediaEvent(int event_type, uint32_t wparam, uint32_t lparam, uint32_t lextend);
    static  void        FPlayerCallBack(void* FMediaplayer , int what, int arg1, int arg2, void* obj);
	int                 createFplayer();
    void                   destroyFPlayer();
        void        setNotifyCallback(
            void* cookie, notify_callback_f notifyFunc) {
        Mutex::Autolock autoLock(mNotifyLock);
        mCookie = cookie; mNotify = notifyFunc;
    }

    void        sendEvent(int msg, int ext1=0, int ext2=0,
                          const Parcel *obj=NULL) {
        Mutex::Autolock autoLock(mNotifyLock);
        if (mNotify) mNotify(mCookie, msg, ext1, ext2, obj);
    }
	/*
	int           		doPrepare(Command* cmd);
	int           		doStart(Command* cmd);
	int           		doStop(Command* cmd);
	int           		doPause(Command* cmd);
	int           		doSuspend(Command* cmd);
	int           		doResume(Command* cmd);
	int           		doGetPosition(Command* cmd);
	int           		doGetDuration(Command* cmd);
	int           		doSeek(Command* cmd);
	int           		doIsPlaying(Command* cmd);
	int           		doInvoke(Command* cmd);
*/	
	//void				interrupt();
	
private:
#if (PLATFORM_SDK_VERSION >= 21)
        sp<IMediaHTTPService> mHTTPService;
#endif
	sp<CommandQueue>	mCmdQueue;
	wp<MediaPlayerBase> mListener; 
	mutable Mutex 		mLock;
	void*				mPlayer;
    void *              mVideoSurfaceTexture;
    char*       		mDataSourcePath;
	char*			    mDataSourceExtra;
	Condition			mCallbackCond;
	int					mPlayerState;
	int 				mPauseSeekPos;
	int					mWaitWhat;
	int 				mLoop;
	int					mLeft;
	int					mTop;
	int 				mWidth;
	int 				mHeight;
        UM_U64 mDuration;
        int mPosition;
        int mState;
        F_PLAYER_MEDIA_S mMediaParam;
	//void 				notifyListener(int msg, int ext1 = 0, int ext2 = 0, const Parcel *obj = NULL);
	//int					waitWhat(int what, int timeout);
	//int 				finished(int what);
    bool                mDiagnose;
        friend class MediaPlayerService;

    Mutex               mNotifyLock;
    void*               mCookie;
    notify_callback_f   mNotify;
};

};
#endif	//ANDROID_CMCCPLAYER_H

#ifndef ANDROID_FMEDIAPLAYERMANAGER_H
#define ANDROID_FMEDIAPLAYERMANAGER_H

#include <media/MediaPlayerInterface.h>
#include "FMediaPlayer.h"

namespace android 
{
class FMediaPlayerManager : public MediaPlayerInterface 
{
public:
    					FMediaPlayerManager();
     			~FMediaPlayerManager();
	 status_t 	initCheck();

   
     status_t 	setUID(uid_t uid) {return NO_ERROR;}
#ifdef Android_Lollipop
     status_t 	setDataSource(const sp<IMediaHTTPService> &httpService,const char *url, const KeyedVector<String8, String8> *headers);
#else
     status_t 	setDataSource(const char *url, const KeyedVector<String8, String8> *headers);
#endif
     status_t 	setDataSource(int fd, int64_t offset, int64_t length) {return NO_ERROR;}
     status_t 	setDataSource(const sp<IStreamSource> &source) {return NO_ERROR;}

#ifdef Android_JB
	 status_t 	setVideoSurfaceTexture(const sp<ISurfaceTexture> &surfaceTexture);
#endif
#ifdef Android_KitKate
     status_t 	setVideoSurfaceTexture(const sp<IGraphicBufferProducer> &bufferProducer);
#endif
#ifdef Android_Lollipop
     status_t 	setVideoSurfaceTexture(const sp<IGraphicBufferProducer> &bufferProducer);
#endif
     status_t 	prepare();
     status_t 	prepareAsync();
     status_t 	start();
     status_t 	stop();
     status_t 	pause();
     bool 		isPlaying();
     status_t 	seekTo(int msec);
     status_t 	getCurrentPosition(int *msec);
     status_t 	getDuration(int *msec);
     status_t 	reset();
     status_t 	setLooping(int loop);
     player_type playerType();
     status_t 	invoke(const Parcel &request, Parcel *reply);
     status_t	setParameter(int key, const Parcel &request);
     status_t	getParameter(int key, Parcel *reply); 
            void notifyFunc(int msg, int ext1, int ext2, const Parcel *obj = NULL);
    static void notify(void* cookie, int msg, int ext1, int ext2, const Parcel *obj = NULL);
     bool hardwareOutput() {return false;}
private:
    FMediaPlayer *mPlayer;
};

}  // namespace android

#endif  // CMCCMEDIAPLAYERMANAGER_H

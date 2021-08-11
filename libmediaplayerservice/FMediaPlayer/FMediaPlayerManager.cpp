//#define LOG_NDEBUG 0
#define LOG_TAG "FMediaPlayer"
#include <utils/Log.h>

#include "FMediaPlayerManager.h"

namespace android {

FMediaPlayerManager::FMediaPlayerManager()
{
    ALOGV("FMediaPlayerManager");
     mPlayer = new FMediaPlayer();
	mPlayer->setListener(this);
      mPlayer->setNotifyCallback(this, notify);
}

FMediaPlayerManager::~FMediaPlayerManager() {
    ALOGV("~FMediaPlayerManager");
    reset();
    delete mPlayer;
    mPlayer = NULL;
}

status_t FMediaPlayerManager::initCheck() {
    ALOGV("initCheck");
    return OK;
}

#ifdef Android_Lollipop
status_t FMediaPlayerManager::setDataSource(
        const sp<IMediaHTTPService> &httpService,const char *url, const KeyedVector<String8, String8> *headers) {
	ALOGV("setDataSource url:%s", url);
    return mPlayer->setDataSource(url, headers);
}
#else
status_t FMediaPlayerManager::setDataSource(
        const char *url, const KeyedVector<String8, String8> *headers) {
	ALOGV("setDataSource url:%s", url);
    return mPlayer->setDataSource(url, headers);
}
#endif


#ifdef Android_JB
status_t FMediaPlayerManager::setVideoSurfaceTexture(
        const sp<ISurfaceTexture> &surfaceTexture) {
    ALOGV("setVideoSurfaceTexture");
    return mPlayer->setVideoSurfaceTexture((void *)(surfaceTexture.get()));
}
#endif

#ifdef Android_KitKate
status_t FMediaPlayerManager::setVideoSurfaceTexture(
        const sp<IGraphicBufferProducer> &bufferProducer) {
    ALOGV("setVideoSurfaceTexture");
    return mPlayer->setVideoSurfaceTexture((void *)(bufferProducer.get()));
}
#endif

#ifdef Android_Lollipop
status_t FMediaPlayerManager::setVideoSurfaceTexture(
        const sp<IGraphicBufferProducer> &bufferProducer) {
    ALOGV("setVideoSurfaceTexture");
    return mPlayer->setVideoSurfaceTexture((void *)(bufferProducer.get()));
}
#endif

status_t FMediaPlayerManager::prepare() {
	ALOGV("prepare");
    return mPlayer->prepare();
}

status_t FMediaPlayerManager::prepareAsync() {
	ALOGV("prepareAsync");
    return mPlayer->prepareAsync();
}

status_t FMediaPlayerManager::start() {
    ALOGV("start");
    return mPlayer->start();
}

status_t FMediaPlayerManager::stop() {
    ALOGV("stop");
    return mPlayer->stop();
}

status_t FMediaPlayerManager::pause() {
    ALOGV("pause");
    return mPlayer->pause();
}

bool FMediaPlayerManager::isPlaying() {
    ALOGV("isPlaying");
    return mPlayer->isPlaying();
}

status_t FMediaPlayerManager::seekTo(int msec) {
    ALOGV("seek to %d msec",msec);
    return mPlayer->seekTo(msec);
}

status_t FMediaPlayerManager::getCurrentPosition(int *msec) {
    ALOGV("getCurrentPosition");
    status_t err = mPlayer->getCurrentPosition(msec);
    if (err != OK) {
        return err;
    }
    return OK;
}

status_t FMediaPlayerManager::getDuration(int *msec) {
    ALOGV("getDuration");
    status_t err = mPlayer->getDuration(msec);
    if (err != OK) {
        *msec = 0;
        return OK;
    }
    return OK;
}

status_t FMediaPlayerManager::reset() {
    ALOGV("reset");
    mPlayer->reset();
    return OK;
}

status_t FMediaPlayerManager::setLooping(int loop) {
    ALOGV("setLooping");
    return mPlayer->setLooping(loop);
}

player_type FMediaPlayerManager::playerType() {
    ALOGV("playerType");
    return F_PLAYER;
}

status_t FMediaPlayerManager::invoke(const Parcel &request, Parcel *reply) {
   return mPlayer->invoke(request, reply);
}

status_t FMediaPlayerManager::setParameter(int key, const Parcel &request) {
   return mPlayer->setParameter(key, request);
}

status_t FMediaPlayerManager::getParameter(int key, Parcel *reply) {
   return mPlayer->getParameter(key, reply);
}

void FMediaPlayerManager::notifyFunc(int msg, int ext1, int ext2, const Parcel *obj)
{
    sendEvent(msg, ext1, ext2, obj);
    ALOGV("back from callback");
}

void FMediaPlayerManager::notify(void* cookie, int msg, int ext1, int ext2, const Parcel *obj)
{
    ALOGV("message received msg=%d, ext1=%d, ext2=%d", msg, ext1, ext2);

    FMediaPlayerManager *pthis = (FMediaPlayerManager*)cookie;
    if (NULL == pthis)
    {
        ALOGV("HiMediaPlayerManage::notify fail, pthis is NULL");
        return;
    }

    pthis->notifyFunc(msg, ext1, ext2, obj);
}

}  // namespace android

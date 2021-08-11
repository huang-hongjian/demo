#include "hjplayer_adp.h"
#include <jni.h>
#include <utils/Vector.h>
#include <gui/IGraphicBufferProducer.h>
#include <gui/Surface.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <cutils/properties.h>
#include <android_runtime/AndroidRuntime.h>
#include <android_runtime/android_view_Surface.h>
#include "android_runtime/AndroidRuntime.h"
#include "JNIHelp.h"
using namespace std;
using android::String8;
using namespace android;
#define TAG "hjplayer_adp"
#define LOG(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG,  TAG, fmt, ##args)

static CMCC_MediaProcessor *p = NULL;
static void* surface_native = NULL;
sp<IGraphicBufferProducer> native_gbp = NULL;


extern "C"
{
	int HJ_initPlayer(void)
		{
			if(p != NULL)
			{
				p -> ~CMCC_MediaProcessor();
				p = NULL;
			}
	
			p = Get_CMCCMediaProcessor();
			return UM_SUCCESS;
		}
	    void HJ_InitVideo(PVIDEO_PARA_T pVideoPara)
		{
			CHECK_NULL_RET(p);
			p -> InitVideo(pVideoPara);
		}
	
		void HJ_InitAudio(PAUDIO_PARA_T pAudioPara)
		{
			CHECK_NULL_RET(p);
			p -> InitAudio(pAudioPara);
		}
	
		int HJ_StartPlay(void)
		{
			CHECK_NULL_RET_VAL(p);
			bool ret = p -> StartPlay();
			return ret == true ? 0 : -1;
		}
	
		int HJ_Pause(void)
		{
			CHECK_NULL_RET_VAL(p);
			bool ret = p -> Pause();
			return ret == true ? 0 : -1;
		}
	
	
		int HJ_Resume(void)
		{
			CHECK_NULL_RET_VAL(p);
			bool ret = p -> Resume();
			return ret == true ? 0 : -1;
		}
	
		int HJ_TrickPlay(void)
		{
			CHECK_NULL_RET_VAL(p);
			bool ret = p -> TrickPlay();
			return ret == true ? 0 : -1;
		}
	
		int HJ_StopTrickPlay(void)
		{
			CHECK_NULL_RET_VAL(p);
			bool ret = p -> StopTrickPlay();
			return ret == true ? 0 : -1;
		}
	
		int HJ_Stop(void)
		{
			CHECK_NULL_RET_VAL(p);
			bool ret = p -> Stop();
			return ret == true ? 0 : -1;
		}
	
		int HJ_Seek(void)
		{
			CHECK_NULL_RET_VAL(p);
			bool ret = p -> Seek();
			return ret == true ? 0 : -1;
		}
	
		int32_t HJ_GetWriteBuffer(PLAYER_STREAMTYPE_E type, uint8_t **pBuffer, uint32_t *nSize)
		{
			CHECK_NULL_RET_VAL(p);
			int32_t ret = p -> GetWriteBuffer(type, pBuffer, nSize);
			return ret;
		}
	
		int32_t HJ_WriteData(PLAYER_STREAMTYPE_E type, uint8_t *pBuffer, uint32_t nSize, uint64_t timestamp)
		{
			CHECK_NULL_RET_VAL(p);
			int32_t ret = p -> WriteData(type, pBuffer, nSize, timestamp);
			return ret;
		}
	
		void HJ_SwitchAudioTrack(uint16_t pid, PAUDIO_PARA_T pAudioPara)
		{
			CHECK_NULL_RET(p);
			p -> SwitchAudioTrack(pid, pAudioPara);
		}
	
		int HJ_GetIsEos()
		{
			CHECK_NULL_RET_VAL(p);
			bool ret = p -> GetIsEos();
			return ret == true ? UM_SUCCESS : UM_FAILURE;
		}
	
		int32_t HJ_GetPlayMode()
		{
			CHECK_NULL_RET_VAL(p);
			int32_t ret = p -> GetPlayMode();
			return ret;
		}
	
		int32_t HJ_GetCurrentPts()
		{
			CHECK_NULL_RET_VAL(p);
			int32_t ret = p -> GetCurrentPts();
			return ret;
		}
	
		void HJ_GetVideoPixels(int32_t *width, int32_t *height)
		{
			CHECK_NULL_RET(p);
			p -> GetVideoPixels(width, height);
		}
	
		int32_t HJ_GetBufferStatus(int32_t *total_size, int32_t *datasize)
		{
			CHECK_NULL_RET_VAL(p);
			int32_t ret = p -> GetBufferStatus(total_size, datasize);
			return ret;
		}
	
		void HJ_SetStopMode(int bHoldLastPic)
		{
			CHECK_NULL_RET(p);
			if(bHoldLastPic == 1)
			{
				p -> SetStopMode(true);
			}
			else
			{
				p -> SetStopMode(false);
			}
		}
	
		void HJ_SetContentMode(PLAYER_CONTENTMODE_E contentMode)
		{
			CHECK_NULL_RET(p);
			p -> SetContentMode(contentMode);
		}
	
		int32_t HJ_GetAudioBalance()
		{
			CHECK_NULL_RET_VAL(p);
			int ret = p -> GetAudioBalance();
			return ret;
		}
	
		int HJ_SetAudioBalance(PLAYER_CH_E nAudioBalance)
		{
			CHECK_NULL_RET_VAL(p);
			bool ret = p -> SetAudioBalance(nAudioBalance);
	
			return ret == true ? 0 : -1;
		}
	    void HJ_SetSurfaceTexture(const void* pVideoSurfaceTexture)
		{
            CHECK_NULL_RET(p);
			CHECK_NULL_RET(pVideoSurfaceTexture);
			p -> SetSurfaceTexture(pVideoSurfaceTexture);

		}
		
		void HJ_SetEventCB(void *handler, PLAYER_EVENT_CB pfunc)
		{
			CHECK_NULL_RET(p);
	
			p -> playback_register_evt_cb(handler,pfunc);
		}
	
		void HJ_DestoryPlayer()
		{
			CHECK_NULL_RET(p);
			p -> ~CMCC_MediaProcessor();
			p = NULL;
			native_gbp = NULL;
		}
		void HJ_SetParameter(int32_t key, void* request)
		{
		    CHECK_NULL_RET(p);
            if(KEY_PARAMETER_VIDEO_POSITION_INFO == key)
            {
               Parcel para;
               
			   CRect * rect = (CRect *)request;
			   char _rect[50]={0};
			   sprintf(_rect,".left=%d.top=%d.right=%d.bottom=%d",rect->left,rect->top,rect->right,rect->bottom);
			   String16  str = String16(_rect);
			   
               size_t dataPos = para.dataPosition();
			   para.writeString16(str);
			   para.setDataPosition( dataPos);
			   	LOG("%s %d %s dataPos=%d",__FUNCTION__,__LINE__,_rect,dataPos);
                      Parcel &para1 = para;
			   p->SetParameter(key,para1);
			   
			}
			else if(KEY_PARAMETER_SET_SYNC == key)
			{
               Parcel para;
			   CSync * sync = (CSync *)request;
			   char _sync[50]={0};
			   sprintf(_sync,".mod=%d.timeout=%d",sync->mod,sync->timeout);
			   String16  str = String16(_sync);
			   LOG("%s %d %s key=%d",__FUNCTION__,__LINE__,_sync,key);
			   size_t dataPos = para.dataPosition();
			   para.writeString16(str);
			   para.setDataPosition( dataPos);
                       Parcel &para1 = para;
			   p->SetParameter(key,para1);
			}
            return ;
		}
		void	HJ_GetParameter 			 (int32_t key, void* reply)
		{
		
            return ;
		}

		void* HJ_SetSurfaceFromApk(void*env,void*jsurface)
		{
		    if( !env || !jsurface)
				return NULL;

		    sp<IGraphicBufferProducer> new_st = NULL;
            sp<Surface> surface(android_view_Surface_getSurface((JNIEnv*)env,(jobject) jsurface));
			if (surface != NULL)
                new_st = surface->getIGraphicBufferProducer();
			else {

                return NULL;
            }
            native_gbp = new_st;
			return native_gbp.get();
		}
        void* HJ_GetSurfaceFromApk()
		{

		    return native_gbp.get();

		}

}




/**
 * @brief: ����������
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
 * @brief 				�������¼��ص�������
 * @param handler  	 	���������
 * @param evt		 	�¼����� 
 * @param param1 		�¼�����
 * @param param2 		�¼�����
 **/
typedef void (*PLAYER_EVENT_CB)(void *handler, PLAYER_EVENT_E evt, uint32_t param1, uint32_t param2);

/**
 * @CMCC_Mediaprocessor
 */
class CMCC_MediaProcessor{
public:
	/**
	 * @brief	���캯��
	 * @param	��	
	 * @return	��
	 **/
	CMCC_MediaProcessor(){}
	
	/**
	 * @brief	��������	
	 * @param	��
	 * @return	��
	 **/
	virtual ~CMCC_MediaProcessor(){}

	/**
	 * @brief	��ʼ����Ƶ����
	 * @param	pVideoPara - ��Ƶ��ز���������ES��ʽ����ʱ��pid=0xffff; û��video������£�vFmt=VFORMAT_UNKNOWN
	 * @return	��
	 **/
	virtual void InitVideo(PVIDEO_PARA_T pVideoPara) = 0;
	
	/**
	 * @brief	��ʼ����Ƶ����
	 * @param	pAudioPara - ��Ƶ��ز���������ES��ʽ����ʱ��pid=0xffff; û��audio������£�aFmt=AFORMAT_UNKNOWN
	 * @return	��
	 **/
	virtual void InitAudio(PAUDIO_PARA_T pAudioPara) = 0;
	
	/**
	 * @brief	��ʼ����	
	 * @param	��
	 * @return	true  - �ɹ�
	 * 			false - ʧ��
	 **/
	virtual bool StartPlay() = 0;
	
	/**
	 * @brief	��ͣ	
	 * @param	��	
	 * @return	true  - �ɹ�
	 * 			false - ʧ��
	 **/
	virtual bool Pause() = 0;
	
	/**
	 * @brief	��������
	 * @param	��
	 * @return	true  - �ɹ�
	 * 			false - ʧ��
	 **/
	virtual bool Resume() = 0;
	
	/**
	 * @brief	�������
	 * @param	��	
	 * @return	true  - �ɹ�
	 * 			false - ʧ��
	 **/
	virtual bool TrickPlay() = 0;
	
	/**
	 * @brief	ֹͣ�������
	 * @param	��	
	 * @return	true  - �ɹ�
	 * 			false - ʧ��
	 **/
	virtual bool StopTrickPlay() = 0;
	
	/**
	 * @brief	ֹͣ
	 * @param	��	
	 * @return	true  - �ɹ�
	 * 			false - ʧ��
	 **/
	virtual bool Stop() = 0;
	
	/**
	 * @brief	��λ
	 * @param	��	
	 * @return	true  - �ɹ�
	 * 			false - ʧ��
	 **/
    virtual bool Seek() = 0;
	
	/**
	 * @brief	��ȡnSize��С��buffer
	 * @param	type    - ts/video/audio
	 * 			pBuffer - buffer��ַ�������nSize��С�Ŀռ䣬����Ϊ��Ӧ�ĵ�ַ;�������nSize����NULL
	 * 			nSize   - ��Ҫ��buffer��С
	 * @return	0  - �ɹ�
	 * 			-1 - ʧ��
	 **/
	virtual int32_t	GetWriteBuffer(PLAYER_STREAMTYPE_E type, uint8_t **pBuffer, uint32_t *nSize) = 0;
	
	/**
	 * @brief	д�����ݣ���GetWriteBuffer��Ӧʹ�� 
	 * @param	type      - ts/video/audio
	 * 			pBuffer   - GetWriteBuffer�õ���buffer��ַ 
	 * 			nSize     - д������ݴ�С
	 * 			timestamp - ES Video/Audio��Ӧ��ʱ���(90KHZ)
	 * @return	0  - �ɹ�
	 * 			-1 - ʧ��
	 **/
	virtual int32_t WriteData(PLAYER_STREAMTYPE_E type, uint8_t *pBuffer, uint32_t nSize, uint64_t timestamp) = 0;
	
	/**
	 * @brief	�л�����
	 * @param	pid			- ts���ŵ�����£��л�����ʱ��pidΪ׼ 
	 * 			pAudioPara	- es���ŵ�����£��л�����ʱ��������Ƶ����Ϊ׼
	 * @return	��
	 **/
	virtual void SwitchAudioTrack(uint16_t pid, PAUDIO_PARA_T pAudioPara) = 0;
    
	/**
	 * @brief	��ǰ�����Ƿ����
	 * @param	��	
	 * @return	true  - ���Ž���
	 * 			false - ����δ����
	 **/
	virtual bool GetIsEos() = 0;

	/**
	 * @brief	ȡ�ò���״̬
	 * @param	��	
	 * @return	PLAYER_STATE_E - ����״̬play/pause/stop 
	 **/
	virtual int32_t	GetPlayMode() = 0;
	
	/**
	 * @brief	ȡ�õ�ǰ���ŵ�video pts(û��video������£�ȡaudio pts)
	 * @param	��	
	 * @return	pts - ��ǰ���ŵ�ԭʼ�����е�pts��msΪ��λ
	 **/
	virtual int32_t GetCurrentPts() = 0;

	/**
	 * @brief	ȡ����Ƶ�Ŀ��	
	 * @param	*width  - ��Ƶ���
	 * 			*height - ��Ƶ�߶�
	 * @return	��
	 **/
	virtual void GetVideoPixels(int32_t *width, int32_t *height) = 0;
	
	/**
	 * @brief	��ȡ�������������Ĵ�С��ʣ�����ݵĴ�С
	 * @param	*totalsize - �������������Ĵ�С
	 * 			*datasize  - ʣ��δ�������ݵĴ�С
	 * @return	0  - �ɹ�
	 * 			-1 - ʧ��
	 **/
    virtual int32_t GetBufferStatus(int32_t *totalsize, int32_t *datasize) = 0;
	
	/**
	 * @brief	���Ž���ʱ�Ƿ������һ֡
	 * @param	bHoldLastPic - true/falseֹͣ����ʱ�Ƿ������һ֡ 
	 * @return	��
	 **/
    virtual void SetStopMode(bool bHoldLastPic) = 0;

	/**
	 * @brief	���û�����ʾ����
	 * @param	contentMode - Դ����/ȫ����Ĭ��ȫ��
	 * @return	��
	 **/
    virtual void SetContentMode(PLAYER_CONTENTMODE_E contentMode) = 0;
	
	/**
	 * @brief	��ȡ��ǰ����
	 * @param	��	
	 * @return  PLAYER_CH_E	
	 **/
	virtual int32_t GetAudioBalance() = 0;
	
	/**
	 * @brief	���ò�������
	 * @param	nAudioBanlance - ������/������/������ 
	 * @return	true  - �ɹ�
	 * 			false - ʧ��
	 **/
	virtual bool SetAudioBalance(PLAYER_CH_E nAudioBalance) = 0;
	
	/**
	 * @brief	����VideoSurfaceTexture,����Android 4���ϰ汾
	 * @param	pSurfaceTexture - VideoSurfaceTexture<Android 4.2������>
	 * 			bufferProducer - GraphicBufferProducer<Android 4.4������>
	 * @return	��
	 **/
	virtual void SetSurfaceTexture(const void *pVideoSurfaceTexture) = 0;

	/**
	 * @brief	ע��ص�����	
	 * @param	pfunc  - �ص�����
	 * 			handle - �ص�������Ӧ�ľ��  
	 * @return	��
	 **/
	virtual void playback_register_evt_cb(void *handler, PLAYER_EVENT_CB pfunc) = 0;

	/**
	 * @brief	���ò���(��MediaPlayer͸������)	
	 * @param	key 	- ����
	 * 			request - �������
	 * @return	��
	 **/
	virtual void SetParameter(int key, const Parcel &request) = 0;

	/**
	 * @brief	��ȡ����(��MediaPlayer͸������)
	 * @param	key   - ����
	 * 			reply - �������
	 * @return	��
	 **/
	virtual void GetParameter(int key, Parcel *reply) = 0;
};

/**
 * @brief	��ȡMediaprocessor����
 * @param	��	
 * @return  CMCC_MediaProcessor����
 **/
CMCC_MediaProcessor* Get_CMCCMediaProcessor();

/**
 * @brief	��ȡ��ǰģ��İ汾��
 * @param	��
 * @return	�汾��
 **/
int Get_CMCCMediaProcessorVersion();

#endif

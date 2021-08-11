/**
 * @brief:������ʵ��
 **/
#ifndef _CMCC_MEDIAPROCESSORIMPL_H_
#define _CMCC_MEDIAPROCESSPRIMPL_H_

#include <CMCC_MediaControl.h>

#include "CMCC_MediaProcessor.h"

using namespace android;

/**
 * @CMCC_MediaprocessorImpl
 */
class CMCC_MediaProcessorImpl : public CMCC_MediaProcessor
{
protected:

    CMCC_MediaControl * cmcc_MediaControl;

public:
	
	//����
	CMCC_MediaProcessorImpl();
	
	//����
	virtual ~CMCC_MediaProcessorImpl();
	
	//��ʼ����Ƶ����
	virtual void InitVideo(PVIDEO_PARA_T pVideoPara);
	
	//��ʼ����Ƶ����
	virtual void InitAudio(PAUDIO_PARA_T pAudioPara);
	
	//��ʼ����	
	virtual bool StartPlay();
	
	//��ͣ	
	virtual bool Pause();
	
	//��������
	virtual bool Resume();
	
	//�������
	virtual bool TrickPlay();
	
	//ֹͣ�������
	virtual bool StopTrickPlay();
	
	//ֹͣ
	virtual bool Stop();
	
	//��λ
    virtual bool Seek();
	
	//��ȡnSize��С��buffer
	virtual int32_t	GetWriteBuffer(PLAYER_STREAMTYPE_E type, uint8_t **pBuffer, uint32_t *nSize);
	
	//д�����ݣ���GetWriteBuffer��Ӧʹ�� 
	virtual int32_t WriteData(PLAYER_STREAMTYPE_E type, uint8_t *pBuffer, uint32_t nSize, uint64_t timestamp);
	
	//�л�����
	virtual void SwitchAudioTrack(uint16_t pid, PAUDIO_PARA_T pAudioPara);
	
	//��ǰ�����Ƿ����
	virtual bool GetIsEos();
	
	//ȡ�ò���״̬
	virtual int32_t	GetPlayMode();
	
	//ȡ�õ�ǰ���ŵ�video pts(û��video������£�ȡaudio pts)
	virtual int32_t GetCurrentPts();
	
	//ȡ����Ƶ�Ŀ��	
	virtual void GetVideoPixels(int32_t *width, int32_t *height);
	
	//��ȡ�������������Ĵ�С��ʣ�����ݵĴ�С
    virtual int32_t GetBufferStatus(int32_t *totalsize, int32_t *datasize);
	
	//���Ž���ʱ�Ƿ������һ֡
    virtual void SetStopMode(bool bHoldLastPic);
	
	//���û�����ʾ����
    virtual void SetContentMode(PLAYER_CONTENTMODE_E contentMode);
	
	//��ȡ��ǰ����
	virtual int32_t GetAudioBalance();
	
	//���ò�������
	virtual bool SetAudioBalance(PLAYER_CH_E nAudioBalance);
	
	//����VideoSurfaceTexture,����Android 4���ϰ汾

	virtual void SetSurfaceTexture(const void *pVideoSurfaceTexture);
	
	//ע��ص�����	
	virtual void playback_register_evt_cb(void *handler, PLAYER_EVENT_CB pfunc);
	
	//���ò���(��MediaPlayer͸������)	
	virtual void SetParameter(int key, const Parcel &request);
	
	//��ȡ����(��MediaPlayer͸������)	
	virtual void GetParameter(int key, Parcel *reply);
private:
    PLAYER_EVENT_CB  m_callBackFunc;
	void		     *m_eventHandler;
};

#endif

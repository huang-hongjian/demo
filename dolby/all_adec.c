#define LOG_NDEBUG 0
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libavutil/frame.h"
#include "libavutil/avutil.h"
#include "libavutil/samplefmt.h"
#include "libavutil/opt.h"
#include "libavformat/avformat.h"
//#include "um_basictypes.h"
#include "log.h"

#define CHECK_KEY_ENABLED   1

#include "fddp_keysetup.h"

#ifndef PRODUCT_VERSION
#define PRODUCT_VERSION     (0x00000000)
#endif

#define DEFAULT_DECODER_ID  (0x1010) /* 0x81f00055: HA_AC3PASSTHROUGH_ID 0x202f1011:TRUEHD*/


#define REQUEST_CHANNEL_LAYOUT      (AV_CH_LAYOUT_STEREO_DOWNMIX)
#define REQUEST_SAMPLE_RATE         (48000)
#define REQUEST_SAMPLE_FORMAT       (AV_SAMPLE_FMT_S16)
#define REQUEST_CHANNLE_CNT         (2) // dep on REQUEST_CHANNEL_LAYOUT
#define REQUEST_BYTES_PER_SAMPLE    (2) // dep on REQUEST_SAMPLE_FORMAT
#define AC3_SAMPLES_PER_FRAME       (1536) // 256 * 6
#define AC3_BYTES_PER_FRAME         (AC3_SAMPLES_PER_FRAME * REQUEST_CHANNLE_CNT \
                                                          * REQUEST_BYTES_PER_SAMPLE) // 6144
#define EAC3_BYTES_PER_FRAME        (AC3_BYTES_PER_FRAME)

#define AC3_PKT_OFFSET              (AC3_BYTES_PER_FRAME)

#define EAC3_PKT_OFFSET             (24576)

#define TRUEHD_PKT_OFFSET           (61440)
#define TRUEHD_FRAME_OFFSET         (2560)
#define AUDIO_EXTRA_DATA_SIZE       (2048*2)

#define MAX_AUDIO_FRAME_SIZE        (192000) //1SECOND
#define SIZE_FOR_MX_BYPASS (48*1024) //about 0.25s for 48K_16bit
#define AVCODEC_MAX_AUDIO_FRAME_SIZE (500*1024)  //(192000*2)
#define AC3_DEFAULT_READ_SIZE (5*1024)//Set too much to affect audio and video synchronization.
#define EAC3_DEFAULT_READ_SIZE (10*1024)
#define TRUEHD_DEFAULT_READ_SIZE (48*1024)
#define BUFFER_LEN_OFFSET 4 //length|data|length|rawdata  
//BUFFER_LEN_OFFSET 表示buffer前四个字节描述解码出的数据的长度，buffer分两段，一段是pcm数据，一段是raw数据，前四个字节都代表各自长度
#define SWR_MODE 1
uint8_t* tmpbuf[SIZE_FOR_MX_BYPASS]={0};

typedef struct _audio_info {
    int bitrate;
    int samplerate;
    int channels;
    int file_profile;
    int error_num;
} AudioInfo;

typedef struct audio_decoder_operations audio_decoder_operations_t;
struct audio_decoder_operations {
    const char * name;
    int nAudioDecoderType;
    int nInBufSize;
    int nOutBufSize;
    int (*init)(audio_decoder_operations_t *);
    int (*decode)(audio_decoder_operations_t *, char *outbuf, int *outlen, char *inbuf, int inlen);
    int (*release)(audio_decoder_operations_t *);
    int (*getinfo)(audio_decoder_operations_t *, AudioInfo *pAudioInfo);
    void * priv_data;//point to audec
    void * priv_dec_data;//decoder private data
    void *pdecoder; // decoder instance
    int channels;
    unsigned long pts;
    int samplerate;
    int bps;
    int extradata_size;      ///< extra data size
    char extradata[AUDIO_EXTRA_DATA_SIZE];
    int NchOriginal;
};

typedef struct um_adec_
{
    AVCodec *pCodec;
    AVCodecContext *pCodecCtx;
    AVCodecParserContext *pCodecParserCtx;
    struct SwrContext *au_convert_ctx;
	AVFormatContext     *hSpdifFmtCtx;
    AVFrame	*pFrame;
    AVFrame	*pDstFrame;
    AVPacket packet;
    int extradata_size;     
    uint8_t* extradata;
    enum AVCodecID codec_id;
    int64_t in_channel_layout;

    uint64_t out_channel_layout;
    int out_nb_samples;
    enum AVSampleFormat out_sample_fmt;
    int out_sample_rate;
    int out_channels;
    int out_buffer_size	;
    uint8_t* out_buffer_raw;
	int out_buffer_raw_size	;
    int frame_errcut;
}um_adec;


#define CHECK_PARAM_NULL(x) \
do {\
    if (!x) {\
        LOG("Error: Param "#x" is null");\
        return -1;\
    }\
} while (0 == 1)

#define DUMP_FRAME_INFO(pf, id) \
do {\
    LOG("=======DUMP Frame["id"]");\
    LOG("   sample_rate:%d", pf->sample_rate);\
    LOG("   format:%d", pf->format);\
    LOG("   channel_layout:0x%llx", pf->channel_layout);\
    LOG("   nb_samples:%d", pf->nb_samples);\
} while (0)

#define UM_CHECK_VALUE_RETURN(val,log_text,retval)\
do{\
    if(val)\
    {\
        LOG("[%s %d] %s",__FUNCTION__,__LINE__,log_text);\
        return retval;\
    }\
}while(0)
#define UM_CHECK_VALUE_GOTO(val,log_text,go)\
do{\
    if(val)\
    {\
        LOG("[%s %d] %s",__FUNCTION__,__LINE__,log_text);\
        goto go;\
    }\
}while(0)
	
static void ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
    if (level <= AV_LOG_ERROR) {
        __log_vprint(fmt, vl);
    }
}

uint32_t getSystemTickMs() {
    uint32_t tick;
    
    struct timespec monotonic_time;
    memset(&monotonic_time, 0, sizeof(monotonic_time));
    clock_gettime(CLOCK_MONOTONIC, &monotonic_time);
    tick = monotonic_time.tv_sec*1000000 + monotonic_time.tv_nsec/1000;
    return tick ;

}

#if 1 
static int audio_write_file(char* file_name, char*data, int len)
{

    
    static int num=0;
    static  long int data_size=0;
    static FILE * pFile=NULL;
    if(len<=0||!data||!file_name)
        return -1;
    if(num==0 && !pFile)
    {
         unlink(file_name);
         pFile = fopen (file_name, "a+");

    }
    if(!pFile)
        return 0;    
    if(data_size>1024*1024*20)
    {
         if(pFile)
         {
             LOG("umplayer write file ok !");
             fclose (pFile);
             pFile = NULL;
         }
         return 0;
    }
    int ret=-1;
    char* tag_tmp = "********************";
    int tag_len = strlen(tag_tmp);
    char tag[100];
    memset(tag,0,100);

    memcpy(tag,tag_tmp,tag_len);

    snprintf(tag+tag_len,9,"%.8d",num);

    memcpy(tag+tag_len+8,tag_tmp,tag_len);

    ret = fwrite (tag , 1, tag_len+8+tag_len, pFile);

    ret|= fwrite (data , 1, len, pFile);

    if(ret>0)
    {
       data_size+=len;
       num++;
    }
    return 0;
}
#endif
static int audio_printf_byte(char* tag,char* ptr,int len)
{
    char tab[16]={'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    char* p = (char*)malloc(len*3+len/16+10);
	char* tmp=p;
    char* data=ptr;
    memset(p,0,len*3+len/16+10);
    int i=0;
    int j=0;
    int line=len/16;
    for(j=0;j<line;j++)
    {
        for(i=0;i<16;i++)
        {
             p[0] = tab[ (data[0]&0xf0)>>4 ];
             p++;
             p[0] = tab[ data[0]&0x0f ];
             p++;
             data++;
             p[0] = ' ';
             p++;
        }
        p[0] = '\t';
        p++;
    }
    if(len%16)
    {
        for(i=0;i<(len%16);i++)
        {
             p[0] = tab[ (data[0]&0xf0)>>4 ];
             p++;
             p[0] = tab[ data[0]&0x0f ];
             p++;
             data++;
             p[0] = ' ';
             p++;
        }
        p[0] = '\t';

        p++;
    }
    if(tag)
    {
        LOG("hex %s:%s",tag,tmp);
    }
    else{
        LOG("hex:%s",tmp);
    }

    free(tmp);
    return 0;

}
static int audio_write_file1(char* file_name, char*data_, int len,int have_head)
{

    char *data=data_;
    static int num=0;
    static  long int data_size=0;
    static FILE * pFile=NULL;
    if(len<=0||!data||!file_name)
        return -1;
    LOG("av_write_file-->");
    if(num==0 && !pFile)
    {
         unlink(file_name);
         
         pFile = fopen (file_name, "a+");

    }
    
    if(!pFile)
    {
        LOG("open file err!");
        return 0;
    }
       
#if 0    
    if(data_size>1024*1024*20)
    {
         if(pFile)
         {
             av_log(NULL,AV_LOG_INFO,"umplayer write file ok !");
             fclose (pFile);
             pFile = NULL;
         }
         return 0;
    }
#endif
    int ret=0;
    if(have_head)
    {
        char* tag_tmp = "********************";
        int tag_len = strlen(tag_tmp);
        char tag[100];
        memset(tag,0,100);

        memcpy(tag,tag_tmp,tag_len);

        snprintf(tag+tag_len,9,"%.8d",num);

        memcpy(tag+tag_len+8,tag_tmp,tag_len);

        ret = fwrite (tag , 1, tag_len+8+tag_len, pFile);
    }
    ret|= fwrite (data , 1, len, pFile);

    if(ret>0)
    {
       data_size+=len;
       num++;
    }
    else{

           LOG("write file err!");
    }
        
    return 0;

}
#if 1
static int audio_read_file(char* file_name, char**data, int len)
{
    static int num=0;
    static  long int data_size=0;
    static FILE * pFile=NULL;

    if(len<=0||!data||!file_name)
    {
         
          return -1;
    }

    if(num==0 && !pFile)
    {
         //unlink(file_name);

         pFile = fopen (file_name, "rb+");
    }
    if(!pFile)
    {
          return -1;
    }
#if 0
    if(data_size>1024*1024*50)
    {
         if(pFile)
         {
             //LOG("umplayer write file ok !");
             fclose (pFile);
             pFile = NULL;
         }
         return 0;
    }
#endif
    int ret = 0;
    ret= fread (*data , 1, len, pFile);
    if(ret>0)
    {
       LOG("fread  datalen=%d",ret);
       data_size+=len;
       num++;
    }
    else
    {
        return -1;
    }
    return 0;
}
#endif

/*************************************************************************************/

static int audio_dec_get_decpara(audio_decoder_operations_t *adec_ops,AVCodecContext *pCodecCtx,enum AVCodecID codec_id)
{
    if(codec_id == AV_CODEC_ID_AC3)
    {
        //pCodecCtx->bit_rate = 192000;
        pCodecCtx->sample_rate = 48000;
        pCodecCtx->channels = 2;
    }
    else if (codec_id == AV_CODEC_ID_TRUEHD)
    {
        //pCodecCtx->bit_rate = 192000;
        pCodecCtx->sample_rate = 48000;
        pCodecCtx->channels = 8;

    }
    /*
    else if (codec_id == AV_CODEC_ID_DTS)
    {
        //pCodecCtx->bit_rate = 192000;
        pCodecCtx->sample_rate = 48000;
        pCodecCtx->channels = 6;

    }
    */
	else if(codec_id ==AV_CODEC_ID_EAC3)
	{
        //pCodecCtx->bit_rate = 192000;
        pCodecCtx->sample_rate = 48000;
        pCodecCtx->channels = 6;
	}
    else
    {
        pCodecCtx->bit_rate = 192000;
        pCodecCtx->sample_rate = 48000;
        pCodecCtx->channels = 2;
    }
    
    if(adec_ops->channels)
        pCodecCtx->channels = adec_ops->channels;
    if(adec_ops->samplerate)
        pCodecCtx->sample_rate = adec_ops->samplerate;
    if(adec_ops->bps)
        pCodecCtx->bit_rate = adec_ops->bps;
    
    pCodecCtx->request_channel_layout = REQUEST_CHANNEL_LAYOUT;
    pCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;

    pCodecCtx->channel_layout = av_get_default_channel_layout(pCodecCtx->channels);

    return 0;
}
static enum AVCodecID audio_dec_get_codecid(int am_codecid)
{
    if(am_codecid)
    {
        if(am_codecid == 3)
            return AV_CODEC_ID_AC3;
		/*else if(am_codecid == 6)
            return AV_CODEC_ID_DTS;*/
        else if(am_codecid==21)
            return AV_CODEC_ID_EAC3;
        else if(am_codecid ==25)
            return AV_CODEC_ID_TRUEHD;
        return-1;
    }
    else 
        return-1;
}
int audio_dec_init(audio_decoder_operations_t *adec_ops)
{
    LOG("audio_dec_init---------------->\n");
    um_adec* g_adec = NULL;
    AVCodec *pCodec= NULL;
    AVCodecContext *pCodecCtx= NULL;
    AVCodecParserContext *pCodecParserCtx=NULL;
	AVFormatContext *hSpdifFmtCtx=NULL;
    AVFrame	*pFrame= NULL;
    AVFrame	*pDstFrame= NULL;
    uint8_t *out_buffer_raw = NULL;
    enum AVCodecID codec_id;
    struct SwrContext *au_convert_ctx= NULL;
    int64_t in_channel_layout;
    int ret;
    av_log_set_level(AV_LOG_ERROR);
    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    av_log_set_callback(ffmpeg_log_callback);
	LOG("PRODUCT_VERSION=%d",PRODUCT_VERSION);
	avcodec_register_all();
    av_register_all();
    g_adec =(um_adec*) av_malloc(sizeof(um_adec));
    UM_CHECK_VALUE_RETURN(!g_adec,"g_adec av_malloc failed",-1);
    codec_id = audio_dec_get_codecid(adec_ops->nAudioDecoderType);

    LOG("codec_id=%d",adec_ops->nAudioDecoderType);
    UM_CHECK_VALUE_RETURN(codec_id==-1,"Codec type not found\n",-1);


    pCodec = avcodec_find_decoder(codec_id);
    UM_CHECK_VALUE_RETURN(!pCodec,"Codec not found\n",-1);

    pCodecCtx = avcodec_alloc_context3(pCodec);

    UM_CHECK_VALUE_RETURN(!pCodecCtx,"Could not allocate video codec context\n",-1);

    LOG("channels=%d samplerate=%d bps=%d frame_size=%d NchOriginal=%d sample_fmt=%d bits_per_coded_sample=%d\n"
        ,adec_ops->channels,adec_ops->samplerate,adec_ops->bps,pCodecCtx->frame_size
        ,adec_ops->NchOriginal,pCodecCtx->sample_fmt,pCodecCtx->bits_per_coded_sample);
    audio_dec_get_decpara(adec_ops,pCodecCtx,codec_id);

    //pCodecCtx->request_channel_layout = REQUEST_CHANNEL_LAYOUT;
    pCodecParserCtx=av_parser_init(codec_id);
    UM_CHECK_VALUE_RETURN(!pCodecParserCtx,"Could not allocate video parser context\n",-1);

    ret = avcodec_open2(pCodecCtx, pCodec, NULL) ;
    UM_CHECK_VALUE_GOTO(ret<0,"Could not open codec\n",FAILED);
    
    ret= avformat_alloc_output_context2(&hSpdifFmtCtx,NULL, "spdif", NULL);
	UM_CHECK_VALUE_GOTO(ret,"Error: alloc ouput context for spdif failed.",FAILED);
	
    hSpdifFmtCtx->pb = NULL;
    hSpdifFmtCtx->flags = AVFMT_FLAG_CUSTOM_IO;
    avformat_new_stream(hSpdifFmtCtx, pCodec);
    
	
    //out_buffer = av_malloc(MAX_AUDIO_FRAME_SIZE*2);
    pFrame = av_frame_alloc();
    
    av_init_packet(&g_adec->packet);

    //uint8_t *out_buffer=(uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE*2);
    
    //通过av_get_channel_layout_nb_channels()和av_get_default_channel_layout()这些函数可以得到channels和channellayout的转换。
    uint64_t out_channel_layout=REQUEST_CHANNEL_LAYOUT;//AV_CH_LAYOUT_STEREO;
    int out_nb_samples=pCodecCtx->frame_size;
    enum AVSampleFormat out_sample_fmt=AV_SAMPLE_FMT_S16;
    int out_sample_rate=pCodecCtx->sample_rate;
    int out_channels=av_get_channel_layout_nb_channels(out_channel_layout);
    //int out_buffer_size=av_samples_get_buffer_size(NULL,out_channels ,out_nb_samples,out_sample_fmt, 1);
    in_channel_layout=av_get_default_channel_layout(pCodecCtx->channels);
    LOG("out_channel_layout=%d out_nb_samples=%d out_sample_fmt=%d out_sample_rate=%d out_channels=%d in_channel_layout=%d",out_channel_layout,
	out_nb_samples,out_sample_fmt,out_sample_rate,out_channels,in_channel_layout);
    au_convert_ctx = swr_alloc();
    UM_CHECK_VALUE_GOTO(!au_convert_ctx,"swr_alloc err\n",FAILED);
#if SWR_MODE
    pDstFrame = av_frame_alloc();
    g_adec->pDstFrame = pDstFrame;
#else
    au_convert_ctx=swr_alloc_set_opts(au_convert_ctx,out_channel_layout, out_sample_fmt, out_sample_rate,
    in_channel_layout,pCodecCtx->sample_fmt , pCodecCtx->sample_rate,0, NULL);
    ret = swr_init(au_convert_ctx);
	UM_CHECK_VALUE_GOTO(ret,"swr_init err\n",FAILED);
	
#endif

    g_adec->pCodec = pCodec;
    g_adec->pCodecCtx = pCodecCtx;
    g_adec->pCodecParserCtx = pCodecParserCtx;
    g_adec->au_convert_ctx = au_convert_ctx;
	g_adec->hSpdifFmtCtx = hSpdifFmtCtx;
    g_adec->pFrame = pFrame;

    g_adec->codec_id = codec_id;
    g_adec->in_channel_layout = in_channel_layout;
    g_adec->out_channel_layout = out_channel_layout;
    g_adec->out_channels = out_channels;
    g_adec->out_sample_rate = out_sample_rate;
    g_adec->out_sample_fmt = out_sample_fmt;
    
    g_adec->extradata_size = adec_ops->extradata_size;
    g_adec->extradata=adec_ops->extradata;
    if(codec_id ==AV_CODEC_ID_AC3)
        adec_ops->nInBufSize = AC3_DEFAULT_READ_SIZE;
    else if(codec_id ==AV_CODEC_ID_EAC3)
        adec_ops->nInBufSize = EAC3_DEFAULT_READ_SIZE;
    else if(codec_id ==AV_CODEC_ID_TRUEHD)
        adec_ops->nInBufSize = TRUEHD_DEFAULT_READ_SIZE;	
    if(codec_id ==AV_CODEC_ID_EAC3 ||codec_id ==AV_CODEC_ID_TRUEHD||codec_id ==AV_CODEC_ID_AC3/*||codec_id ==AV_CODEC_ID_DTS*/)
	{
		g_adec->out_buffer_size =adec_ops->nOutBufSize =AVCODEC_MAX_AUDIO_FRAME_SIZE+BUFFER_LEN_OFFSET;//AVCODEC_MAX_AUDIO_FRAME_SIZE*2+BUFFER_LEN_OFFSET;
		g_adec->out_buffer_raw = (uint8_t*)av_malloc(sizeof(uint8_t)*MAX_AUDIO_FRAME_SIZE);
		g_adec->out_buffer_raw_size = MAX_AUDIO_FRAME_SIZE;
		
		LOG("out_buffer_raw=%x",g_adec->out_buffer_raw);
	}
    else
	{
		g_adec->out_buffer_size =adec_ops->nOutBufSize = AVCODEC_MAX_AUDIO_FRAME_SIZE+BUFFER_LEN_OFFSET;
		g_adec->out_buffer_raw = (uint8_t*)av_malloc(sizeof(uint8_t)*MAX_AUDIO_FRAME_SIZE);
		g_adec->out_buffer_raw_size = MAX_AUDIO_FRAME_SIZE;
		LOG("out_buffer_raw=%x",g_adec->out_buffer_raw);
	}

    adec_ops->pdecoder = (void*)g_adec;
    LOG("audio_dec_init<----------------\n");
    return 0;

FAILED:

    if(au_convert_ctx)swr_free(au_convert_ctx);
    if(pDstFrame)av_frame_free(&pDstFrame);
    if(pFrame)av_frame_free(&pFrame);
    
    if (hSpdifFmtCtx) {
        if (hSpdifFmtCtx->pb) {
            uint8_t *pu8SpdifBuf = NULL;
            avio_close_dyn_buf(hSpdifFmtCtx->pb, &pu8SpdifBuf);
            hSpdifFmtCtx->pb = NULL;
            if (pu8SpdifBuf) {
                av_free(pu8SpdifBuf);
            }
        }
        avformat_close_input(&hSpdifFmtCtx);
    }
    if (pCodecParserCtx) {
        av_parser_close(pCodecParserCtx);
    }
    if(pCodecCtx)avcodec_free_context(pCodecCtx);
	if(out_buffer_raw)
		av_free(out_buffer_raw);
    adec_ops->pdecoder =NULL;
	return -1;
}
static inline int is_samplerate_valid(int sampleRate) {
    if (sampleRate > 0 && sampleRate <= 192000) {
        return 1;
    }
    return 0;
}
static inline int audio_write_oneframe_to_spdif(um_adec* adec,
                                              AVPacket *pstAvPkt,
                                              uint8_t *ps32BitsOutBuf, 
											  uint32_t u32BitsOutBufSize,
                                              int *pu32BitsOutSize)  
{
    int  s32Ret = 0;

    *pu32BitsOutSize = 0;

    CHECK_PARAM_NULL(adec->hSpdifFmtCtx);
    
    if (adec->hSpdifFmtCtx->pb == NULL) {
        avio_open_dyn_buf(&adec->hSpdifFmtCtx->pb);
        
        int sampleRate = adec->pCodecCtx->sample_rate;
        //LOG("audio_write_oneframe_to_spdif sampleRate=%d",sampleRate);
        if (!is_samplerate_valid(sampleRate)) {
            sampleRate = REQUEST_SAMPLE_RATE;
        }
        
        adec->hSpdifFmtCtx->streams[0]->codec->sample_rate = sampleRate;
        avformat_write_header(adec->hSpdifFmtCtx, NULL);

    }
    
    if (av_write_frame(adec->hSpdifFmtCtx, pstAvPkt) < 0) {
        LOG("Error: write frame to spdif failed.");
        return -1;
    }
  
    if (adec->hSpdifFmtCtx->pb->pos > 0) {
    
        av_write_trailer(adec->hSpdifFmtCtx);
     
        uint8_t *pu8SpdifBuf = NULL;
        int s32SpdifBufSize = avio_close_dyn_buf(adec->hSpdifFmtCtx->pb, &pu8SpdifBuf);

        adec->hSpdifFmtCtx->pb = NULL;
        if (pu8SpdifBuf) {
            if (u32BitsOutBufSize < s32SpdifBufSize) {
                LOG("Error: u32BitsOutBufSize(%d) is too small, we need (%d)",u32BitsOutBufSize, s32SpdifBufSize);
                s32Ret =  -1;
            } else {

                memcpy(ps32BitsOutBuf, pu8SpdifBuf, s32SpdifBufSize);
                *pu32BitsOutSize = s32SpdifBufSize;
            }
            av_free(pu8SpdifBuf);
        }    
    }
    return s32Ret;  
}

int audio_dec_decode(audio_decoder_operations_t *adec_ops,uint8_t *outbuf,int *outlen,uint8_t *indata,int inlen)
{
	
    int starttime = getSystemTickMs();
    int endtime=0;
	static int datasize = 0;
    //LOG("[%s %d] inlen %d starttime=%d",__FUNCTION__,__LINE__,inlen,starttime);
    UM_CHECK_VALUE_RETURN(!adec_ops && !adec_ops->pdecoder,"err adec_ops is null",-1);

    um_adec* g_adec = (um_adec*)adec_ops->pdecoder;
    UM_CHECK_VALUE_RETURN(!g_adec,"err g_adec is null",-1);
    int ret, got_picture,index=0;
    AVCodec *pCodec = g_adec->pCodec;
    AVCodecContext *pCodecCtx= g_adec->pCodecCtx;
    AVCodecParserContext *pCodecParserCtx=g_adec->pCodecParserCtx;
    AVFrame	*pFrame=g_adec->pFrame;

    struct SwrContext *au_convert_ctx = g_adec->au_convert_ctx;
    uint8_t *cur_ptr = indata;
	int out_buffer_raw_len = g_adec->out_buffer_raw_size;
	//int out_buffer_pcm_len = out_buffer_raw_len;
    uint8_t *out_buffer = outbuf;
    uint8_t *out_buffer_raw = g_adec->out_buffer_raw;

	int out_buff_size ;
    int cur_size = inlen;
    int out_len = 0;
	int out_raw_len = 0;
    int byte_frame =AC3_BYTES_PER_FRAME;
    uint64_t max_frame_size =  MAX_AUDIO_FRAME_SIZE;
    AVPacket *pPacket=&g_adec->packet;

    if (cur_size == 0)
        return -1;

    while (cur_size>0){

    	int len = av_parser_parse2(pCodecParserCtx, pCodecCtx,&pPacket->data, &pPacket->size,
    		         cur_ptr , cur_size ,AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);

        if(len<=0 && pPacket->size<=0)
    	{
    		LOG("err av_parser_parse2 ret<=0");
    		goto END;
    	}
    	cur_ptr += len;
    	cur_size -= len;
        //LOG("cur_size=%d  len=%d pPacket->size=%d",cur_size,len,pPacket->size);
    	if(pPacket->size==0)
       {
             //LOG("pPacket->size==0");
             continue;
        }
#if 0
        {
            char *p = outbuf;
            if(audio_read_file("/data/out.truehd.pcm",&p,320))
                LOG("audio_read_file err !!!!!!!!!!!!");
            out_len = 320;
            goto END;
        }
#endif
    	if(1){
    		ret = avcodec_decode_audio4( pCodecCtx, pFrame,&got_picture, pPacket);
    		if ( ret < 0 ) {
    			LOG("Error in decoding audio frame.\n");
    			goto END;
    		}
			
    		if ( got_picture > 0 ){
#if SWR_MODE   
                    /*
                    if(av_sample_fmt_is_planar((enum AVSampleFormat)g_adec->pCodecCtx->sample_fmt))
                    {
                        int plane_size = av_get_bytes_per_sample((enum AVSampleFormat)(g_adec->pCodecCtx->sample_fmt & 0xFF)) * g_adec->pCodecCtx->frame_size;
                        LOG("sample_fmt_is_planar!! plane_size=%d\n",plane_size);
                    }
                    else
                    {
                        LOG("sample_fmt_no_planar!!\n");
                    }
					*/
                    AVFrame	*pDstFrame=g_adec->pDstFrame;

                    pDstFrame->sample_rate = g_adec->out_sample_rate;
                    pDstFrame->channel_layout = g_adec->out_channel_layout;
                    pDstFrame->format = g_adec->out_sample_fmt;
                    pDstFrame->extended_data = pDstFrame->data;
                    pDstFrame->data[0] = out_buffer;
                    pFrame->sample_rate = g_adec->pCodecCtx->sample_rate;
                    pFrame->channel_layout = g_adec->in_channel_layout;
                    pFrame->format = pCodecCtx->sample_fmt;
                    
                    if(g_adec->codec_id == AV_CODEC_ID_TRUEHD ||g_adec->codec_id == AV_CODEC_ID_EAC3)
                    {
                        pDstFrame->linesize[0] = SIZE_FOR_MX_BYPASS-out_len;
                        max_frame_size = SIZE_FOR_MX_BYPASS;

                    }
                    else
                    {
                        pDstFrame->linesize[0] = SIZE_FOR_MX_BYPASS-out_len;
                        max_frame_size = SIZE_FOR_MX_BYPASS;
                    }
                 
                    ret = swr_convert_frame(au_convert_ctx, pDstFrame, pFrame);

                    if (0 != ret) {
                        DUMP_FRAME_INFO(pFrame, "Src");
                        DUMP_FRAME_INFO(pDstFrame, "Dst");
                        if (ret & (AVERROR_INPUT_CHANGED|AVERROR_OUTPUT_CHANGED)) {
                            LOG("Warn: reampler config has changed: 0x%08x. Just close it and retry.", ret);
                            
                            swr_close(au_convert_ctx);
                            
                            ret = swr_convert_frame(au_convert_ctx, pDstFrame, pFrame);
                            if (0 != ret) {
                                LOG("Error: swr_convert_frame() retry failed: 0x%08x", ret);
                                g_adec->frame_errcut++;
                            }

                        }
                        else{
                            LOG("Error: swr_convert_frame() failed: 0x%08x", ret);
                            g_adec->frame_errcut++;
                        }
                        
                    }
                    if(g_adec->frame_errcut>=5)
                    {
                           g_adec->frame_errcut =0;
                           return  -1;
                    }
                    g_adec->frame_errcut =0;
                    //DUMP_FRAME_INFO(pFrame, "Src");
                    //DUMP_FRAME_INFO(pDstFrame, "Dst");
                    //LOG("*out_len=%d data_size=%d out_nb_samples=%d int_nb_samples=%d",out_len,data_size,pDstFrame->nb_samples,pFrame->nb_samples);
#else 
    		       swr_convert(au_convert_ctx,&out_buffer, max_frame_size
    				        ,(const uint8_t **)pFrame->data , pFrame->nb_samples);
                    //DUMP_FRAME_INFO(pFrame, "Src");
                    //LOG("out_len=%d data_size=%d  int_nb_samples=%d",out_len,data_size,pFrame->nb_samples);
#endif
                    //audio_write_file1("/data/out-1.pcm",out_buffer,pDstFrame->nb_samples*2*2,0);
                    byte_frame =pDstFrame->nb_samples*2*2;
                    out_buffer +=byte_frame;
                    out_len +=byte_frame;
					//LOG("nb_samples=%d",pDstFrame->nb_samples);
                    //LOG("pDstFrame->format=%d pFrame->format=%d",pDstFrame->format,pFrame->format);
                    index++;
					
					
					int out_raw_len_tmp = 0;
					uint8_t *out_raw = out_buffer_raw+out_raw_len;
					audio_write_oneframe_to_spdif(g_adec,pPacket,out_raw,out_buffer_raw_len-out_raw_len,&out_raw_len_tmp);
                    if(out_raw_len_tmp>0)//拿到raw数据就返回，避免数据过长导致越界
					{
						out_raw_len += out_raw_len_tmp;
						goto END;
					}

                    if( out_len+byte_frame> max_frame_size)
                    {
                        LOG(" out_len+byte_frame> max_frame_size ");
                        goto END;
                    }
                    //usleep(100);
    		}
    	}
       else//不走这分支
      {
            int out_size = 0;
            ret = avcodec_decode_audio2(pCodecCtx, (short*)out_buffer, &out_size, pPacket->data, pPacket->size);
            if(ret == 0 && out_size>0)
            {
                  out_len += out_size;
                  out_buffer += out_size;
                   if( out_len> max_frame_size - out_size )
                    {
                        LOG(" out_len+byte_frame> max_frame_size ");
                        goto END;
                    }
            }
       }
    }
END:
	endtime = getSystemTickMs();
	*outlen = out_len;

	//LOG("hongjian difftime=%d duration=%d",(endtime-starttime)/1000,out_len*1000/4/g_adec->out_sample_rate);   
	datasize =datasize+ inlen-cur_size;
	//LOG("hongjian inlen=%d *outlen=%d declen=%d out_raw_len=%d endtime=%d",inlen,*outlen,inlen-cur_size,out_raw_len,endtime);
	//LOG("hongjian *outlen=%d retlen=%d datasize=%d",*outlen,inlen-cur_size,datasize);
	if( out_len>0 &&inlen-cur_size>0)
	{
	   
	   memcpy(outbuf+out_len,(uint8_t*)&out_raw_len,sizeof(int));//写raw数据长度 
	   if(out_raw_len>0)
		   memcpy(outbuf+out_len+sizeof(int),out_buffer_raw,out_raw_len);//复制raw数据到outbuf 
	}
   	return (inlen-cur_size);
}
int audio_dec_getinfo(audio_decoder_operations_t *adec_ops, void *pAudioInfo)
{
    return 0;
}

int audio_dec_release(audio_decoder_operations_t *adec_ops)
{
    LOG("audio_dec_release----------------------->\n");
    if(!adec_ops)
    {
        LOG("err adec_ops is null");
        return -1;
    }
    um_adec* g_adec = (um_adec*)adec_ops->pdecoder;
    adec_ops->pdecoder =NULL;
    if(g_adec)
    {
        if(g_adec->au_convert_ctx){
            swr_close(g_adec->au_convert_ctx);

            swr_free(g_adec->au_convert_ctx);
        }
    	av_parser_close(g_adec->pCodecParserCtx);


        if(g_adec->pDstFrame){
            av_frame_free(&g_adec->pDstFrame);
            g_adec->pDstFrame = NULL;

        }
    	if(g_adec->pFrame) {
            av_frame_free(&g_adec->pFrame);
            g_adec->pFrame = NULL;
            }     
		if (g_adec->hSpdifFmtCtx) {
			if (g_adec->hSpdifFmtCtx->pb) {
				uint8_t *pu8SpdifBuf = NULL;
				avio_close_dyn_buf(g_adec->hSpdifFmtCtx->pb, &pu8SpdifBuf);
				g_adec->hSpdifFmtCtx->pb = NULL;
				if (pu8SpdifBuf) {
					av_free(pu8SpdifBuf);
				}
			}
			avformat_close_input(&g_adec->hSpdifFmtCtx);
                    g_adec->hSpdifFmtCtx = NULL;
		}

    	if(g_adec->pCodecCtx)
      {
           avcodec_close(g_adec->pCodecCtx);
       }
    	if(g_adec->pCodecCtx) 
      {
            av_free(g_adec->pCodecCtx);
       }

	if(g_adec->out_buffer_raw) 
      {
        av_free(g_adec->out_buffer_raw);
       }
        av_free(g_adec);

    	g_adec = NULL;

    }
	LOG("audio_dec_release<-----------------------\n");
    return 0;
}





/********************************测试代码*********************************/
//#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
static void setup_array(uint8_t* out[32], AVFrame* in_frame, int format, int samples)
{
	if (av_sample_fmt_is_planar((enum AVSampleFormat)format)) 
	{
		int i;
              int plane_size = av_get_bytes_per_sample((enum AVSampleFormat)(format & 0xFF)) * samples;
              format &= 0xFF;
		for (i = 0; i < in_frame->channels; i++)
		{
			out[i] = in_frame->data[i];
		}
	} 
	else
	{
		out[0] = in_frame->data[0];
	}
}
#define  TMP_BUF_SIZE 4096
int um_test(int argc, char* argv[])
{
    AVCodec *pCodec;
    AVCodecContext *pCodecCtx= NULL;
    AVCodecParserContext *pCodecParserCtx=NULL;
    FILE *fp_in;
    FILE *fp_out;
    AVFrame	*pFrame;
    AVFrame	*pDstFrame;
    uint8_t in_buffer[TMP_BUF_SIZE + 32]={0};
    uint8_t *cur_ptr;
    int cur_size;
    AVPacket packet;
    int ret, got_picture;
    int y_size;
    enum AVCodecID codec_id;
    if(argc!=4)
    {
    printf("argc!=4 [ ./test [input] [output] [audio_type]]  !!");
    return -1;
    }
    if(strcmp(argv[3],"eac3")==0)
    codec_id=AV_CODEC_ID_EAC3;
    if(strcmp(argv[3],"ac3")==0)
    codec_id=AV_CODEC_ID_AC3;
    if(strcmp(argv[3],"aac")==0)
    codec_id=AV_CODEC_ID_AAC;
    if(strcmp(argv[3],"thd")==0)
    codec_id=AV_CODEC_ID_TRUEHD;
    printf("argv[3]=%s codec_id=%d\n",argv[3],codec_id);
    char *filepath_in=argv[1];

    char *filepath_out=argv[2];
    int first_time=1;

    //av_log_set_level(AV_LOG_DEBUG);

    avcodec_register_all();

    pCodec = avcodec_find_decoder(codec_id);
    if (!pCodec) {
        printf("Codec not found\n");
        return -1;
    }

    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx){
        printf("Could not allocate video codec context\n");
        return -1;
    }
    pCodecCtx->request_channel_layout = REQUEST_CHANNEL_LAYOUT;//channel_layout
	pCodecCtx->bit_rate = 192000;
	pCodecCtx->sample_rate =48000;
	pCodecCtx->channels = 6;
       pCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
       pCodecCtx->channel_layout = av_get_default_channel_layout(8);
	pCodecParserCtx=av_parser_init(codec_id);
	if (!pCodecParserCtx){
		printf("Could not allocate video parser context\n");
		return -1;
	}

    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Could not open codec\n");
        return -1;
    }
	//Input File
    fp_in = fopen(filepath_in, "rb");
    if (!fp_in) {
        printf("Could not open input stream\n");
        return -1;
    }

	//Output File
	fp_out = fopen(filepath_out, "wb");
	if (!fp_out) {
		printf("Could not open output YUV file\n");
		return -1;
	}
    printf("codec_id=%d\n",codec_id);
    pFrame = av_frame_alloc();
    pDstFrame= av_frame_alloc();
    av_init_packet(&packet);
    uint8_t *out_buffer=(uint8_t *)av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
    int index = 0;
    int64_t in_channel_layout;
    struct SwrContext *au_convert_ctx;

    uint64_t out_channel_layout=REQUEST_CHANNEL_LAYOUT;

    int out_nb_samples=pCodecCtx->frame_size;
    enum AVSampleFormat out_sample_fmt=AV_SAMPLE_FMT_S16;
    int out_sample_rate=48000;
    int out_channels=av_get_channel_layout_nb_channels(out_channel_layout);

    int out_buffer_size=av_samples_get_buffer_size(NULL,out_channels ,out_nb_samples,out_sample_fmt, 1);

    in_channel_layout=av_get_default_channel_layout(pCodecCtx->channels);

    au_convert_ctx = swr_alloc();
    while (1) {

        cur_size = fread(in_buffer, 1, TMP_BUF_SIZE, fp_in);

        if (cur_size == 0)
            break;
        cur_ptr=in_buffer;

        while (cur_size>0){
            int len = av_parser_parse2(
            pCodecParserCtx, pCodecCtx,
            &packet.data, &packet.size,
            cur_ptr , cur_size ,
            AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);

            cur_ptr += len;
            cur_size -= len;

            if(packet.size==0)
                continue;

            if(1){

                ret = avcodec_decode_audio4( pCodecCtx, pFrame,&got_picture, &packet);
                if ( ret < 0 ) {
                    printf("Error in decoding audio frame.\n");
                    return -1;
                }
                int frame_size = pFrame->nb_samples*2*2;

                int data_size = av_samples_get_buffer_size(NULL, av_frame_get_channels(pFrame)
                                        ,pFrame->nb_samples,(enum AVSampleFormat)pFrame->format, 1);
                
                if ( got_picture > 0 ){
                    if(av_sample_fmt_is_planar((enum AVSampleFormat)pCodecCtx->sample_fmt))
                    {
                        int plane_size = av_get_bytes_per_sample((enum AVSampleFormat)(pCodecCtx->sample_fmt & 0xFF)) * pCodecCtx->frame_size;
                        printf("sample_fmt_is_planar!! plane_size=%d\n",plane_size);
                    }
                    else
                    {
                        //printf("sample_fmt_no_planar!!\n");
                    }
                    uint8_t* m_ain[32]={0};
                    
                    //setup_array(m_ain, pFrame,pCodecCtx->sample_fmt, pCodecCtx->frame_size);
                    pDstFrame->sample_rate =out_sample_rate;
                    pDstFrame->channel_layout = out_channel_layout;
                    pDstFrame->format = out_sample_fmt;
                    pDstFrame->extended_data = pDstFrame->data;
                    pDstFrame->data[0] = out_buffer;
                    pFrame->sample_rate = pCodecCtx->sample_rate;
                    pFrame->channel_layout = in_channel_layout;
                    pFrame->format = pCodecCtx->sample_fmt;
   
                    ret = swr_convert_frame(au_convert_ctx, pDstFrame, pFrame);

                    fwrite(out_buffer, 1, pDstFrame->nb_samples*4, fp_out);
                    index++;
                    
                    if(index==2000)
                        {
                          printf("data_size=%d out_nb_samples=%d in_nb_samples=%d",data_size,pFrame->nb_samples,pDstFrame->nb_samples);
                          // goto end;
                    }
               
                }
            }

        }

    }
end:
    fclose(fp_in);
    fclose(fp_out);
    av_parser_close(pCodecParserCtx);
    av_frame_free(&pFrame);
    av_frame_free(&pDstFrame);
    avcodec_close(pCodecCtx);
    av_free(pCodecCtx);

    return 0;
}





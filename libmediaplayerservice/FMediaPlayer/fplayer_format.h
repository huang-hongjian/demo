
#ifndef __F_PLAYER_H__
#define __F_PLAYER_H__
#include "um_basictypes.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */


#define UM_FORMAT_MAX_URL_LEN               (2048)
#define UM_FORMAT_MAX_LANG_NUM              (32)




/** Input media file */
/** CNcomment:�����ý���ļ� */
typedef struct _F_PLAYER_MEDIA_S
{
    UM_CHAR aszUrl[UM_FORMAT_MAX_URL_LEN];          /**< File path, absolute file path, such as /mnt/filename.ts. *//**< CNcomment:�ļ�·��������·������/mnt/filename.ts */
    UM_S32   s32PlayMode;                            /**< Set the mode of the player. The parameter is ::HI_SVR_PLAYER_PLAY_MODE_E *//**< CNcomment:���ò���ģʽ,����::HI_SVR_PLAYER_PLAY_MODE_E */
    UM_U32   u32ExtSubNum;                        /**< Number of subtitle files *//**< CNcomment:��Ļ�ļ����� */
    UM_CHAR  aszExtSubUrl[UM_FORMAT_MAX_LANG_NUM][UM_FORMAT_MAX_URL_LEN];  /**< Absolute path of a subtitle file, such as /mnt/filename.ts. *//**< CNcomment:��Ļ�ļ�·��������·������/mnt/filename.ts */
    UM_U32   u32UserData;                            /**< Create a handle by calling the fmt_open function, send the user data to the DEMUX by calling the fmt_invoke, and then call the fmt_find_stream function. */
                                                     /**< CNcomment:�û����ݣ�HiPlayer����͸�������ý�����fmt_open֮��ͨ��fmt_invoke�ӿڴ��ݸ����������ٵ���fmt_find_stream�ӿ� */
} F_PLAYER_MEDIA_S;




#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* __HI_SVR_PLAYER_H__ */


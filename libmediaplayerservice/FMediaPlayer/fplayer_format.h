
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
/** CNcomment:输入的媒体文件 */
typedef struct _F_PLAYER_MEDIA_S
{
    UM_CHAR aszUrl[UM_FORMAT_MAX_URL_LEN];          /**< File path, absolute file path, such as /mnt/filename.ts. *//**< CNcomment:文件路径，绝对路径，如/mnt/filename.ts */
    UM_S32   s32PlayMode;                            /**< Set the mode of the player. The parameter is ::HI_SVR_PLAYER_PLAY_MODE_E *//**< CNcomment:设置播放模式,参数::HI_SVR_PLAYER_PLAY_MODE_E */
    UM_U32   u32ExtSubNum;                        /**< Number of subtitle files *//**< CNcomment:字幕文件个数 */
    UM_CHAR  aszExtSubUrl[UM_FORMAT_MAX_LANG_NUM][UM_FORMAT_MAX_URL_LEN];  /**< Absolute path of a subtitle file, such as /mnt/filename.ts. *//**< CNcomment:字幕文件路径，绝对路径，如/mnt/filename.ts */
    UM_U32   u32UserData;                            /**< Create a handle by calling the fmt_open function, send the user data to the DEMUX by calling the fmt_invoke, and then call the fmt_find_stream function. */
                                                     /**< CNcomment:用户数据，HiPlayer仅作透传，调用解析器fmt_open之后，通过fmt_invoke接口传递给解析器，再调用fmt_find_stream接口 */
} F_PLAYER_MEDIA_S;




#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* __HI_SVR_PLAYER_H__ */


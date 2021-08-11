/**************************************************************************
*
*  Copyright(c) 2017,UNIONMAN TECHNOLOGY CO.,LID
*  All rights reserved
*
*  AUTHOR： //文件的创建者
*
*  File: um_basictypes.h
*
*  Description: //文件的功能描述
*
*  History:
*      Date        	Author         	Version   		Descripion
*
**************************************************************************/
#ifndef __UM_BASICTYPES_H__
#define __UM_BASICTYPES_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef char UM_S8;             /* 8位有符号数 */
typedef unsigned char UM_U8;    /* 8位无符号数 */

typedef short UM_S16;           /* 16位有符号数 */
typedef unsigned short UM_U16;  /* 16位无符号数 */

typedef int UM_S32;             /* 32位有符号数 */
typedef unsigned int UM_U32;    /* 32位无符号数 */

typedef long long UM_S64;       /* 64位有符号数 */
typedef unsigned long long UM_U64;  /* 64位无符号数 */

typedef long UM_LONG;               /* 有符号长整形 */
typedef unsigned long UM_ULONG;     /* 无符号长整形 */

typedef char UM_CHAR;           /* 字符型 */
typedef UM_U8   UM_BYTE;        /* 字节类型 */

typedef float UM_FLOAT;         /* 浮点数 */
typedef double UM_DOUBLE;       /* 双精度浮点数 */

typedef UM_U16	UM_WORD;        /* 字 */
typedef UM_U32	UM_DWORD;       /* 双字 */
#define UM_VOID void              /* 空 */ /*typedef void UM_VOID 这种方式g++编译器不支持*/


typedef enum
{
    UM_FALSE    = (0==1),
    UM_TRUE     = (0==0)
} UM_BOOL; /*布尔类型*/

typedef  UM_VOID*  UM_HANDLE ;


#define UM_KEY_NAME_LEN_MAX     (128)
#define UM_KEY_VALUE_LEN_MAX    (1024)

typedef struct UM_KEY_VALUE_S {
    UM_CHAR key[UM_KEY_NAME_LEN_MAX];
    UM_CHAR value[UM_KEY_VALUE_LEN_MAX];
} UM_KEY_VALUE_S;


/******常量定义*******/
#define UM_NULL      (0)
#define UM_SUCCESS ((UM_S32)0)
#define UM_FAILURE ((UM_S32)-1)

typedef struct
{
	UM_U32 u32StreamType;
	UM_U32 u32APid;
	UM_U32 u32VPid;
	UM_U32 u32AStreamNum;
	UM_U32 u32VStreamNum;
	UM_U32 u32AdecType;
	UM_VOID*  pACodecContext;
	UM_U32 u32VdecType;
	UM_U32 u32DispFmt;
	UM_U32 u32VoMode;
    UM_U32 frameRate;
	UM_U32 x;
	UM_U32 y;
	UM_U32 w;
	UM_U32 h;
       UM_U32 samplerate; ///< samples per second
       UM_U32 channels;    ///< number of audio channels

}FPLAYER_ATTR_S;

typedef struct UM_PHAL_CREATE_PARAM_S
{
       UM_BOOL     bUsed;
       UM_HANDLE hAvPlayer;
       UM_HANDLE hWindow;
       UM_HANDLE hTrack;
       FPLAYER_ATTR_S attr;
} UM_PHAL_CREATE_PARAM_S;

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __UM_BASICTYPES_H__ */


#ifndef __FPLAYER_H__
#define __FPLAYER_H__
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <jni.h>
#include <unistd.h>
#include "fplayer_event.h"


typedef unsigned char   BOOL;
typedef void* object;

typedef void (* FPLAYER_CB)( void*weak_thiz , int what, int arg1, int arg2, int obj);


void FMediaPlayer_setDataSourceAndHeaders(char* path,void* keys, void* values);


void FMediaPlayer_setDataSourceFd(int fd);


void FMediaPlayer_setDataSourceCallback(void* callback);


void FMediaPlayer_setDataSourceIjkIOHttpCallback(  void* callback); 

void FMediaPlayer_setVideoSurface(  void* jsurface,int x,int y,int width,int height);


void FMediaPlayer_prepareAsync();


void FMediaPlayer_start();

void FMediaPlayer_stop();


void FMediaPlayer_pause();


void FMediaPlayer_seekTo( long msec);


BOOL FMediaPlayer_isPlaying();


long FMediaPlayer_getCurrentPosition();


long FMediaPlayer_getDuration();

long FMediaPlayer_getBit_Rate();
void FMediaPlayer_release();


void FMediaPlayer_native_setup( void* weak_this);
void FMediaPlayer_reset();

void FMediaPlayer_setLoopCount( int loop_count);


int FMediaPlayer_getLoopCount();


float FMediaPlayer_getPropertyFloat( int id, float default_value);

void FMediaPlayer_setPropertyFloat( int id, float value);


long FMediaPlayer_getPropertyLong( int id, long default_value);

void FMediaPlayer_setPropertyLong(int id, long value);

void FMediaPlayer_setStreamSelected(int stream, BOOL selected);

void FMediaPlayer_setVolume( float leftVolume, float rightVolume);

int FMediaPlayer_getAudioSessionId();

void FMediaPlayer_setOption( int category, object name, object value);

void FMediaPlayer_setOptionLong( int category, object name, long value);

char* FMediaPlayer_getColorFormatName( int mediaCodecColorFormat);


char* FMediaPlayer_getVideoCodecInfo();

char* FMediaPlayer_getAudioCodecInfo();


void* FMediaPlayer_getMediaMeta();

void FMediaPlayer_native_init();
void FMediaPlayer_native_deinit();
void FMediaPlayer_native_setup( void* weak_this);

void FMediaPlayer_native_finalize( char* name, char* value);

void FMediaPlayer_native_profileBegin( char* libName);

void FMediaPlayer_native_profileEnd();

void FMediaPlayer_native_setLogLevel( int level);
void FMediaPlayer_setPostEvent(void* fmediaplayerclass,void* post_event);
void FMediaPlayer_invoke(int cmd,object obj);
int   FMediaPlayer_Report_FpEvent(int msg,void* arg,void* arg2,void*obj);
int FMediaPlayer_SendSqmMsg(int msg);
void FMediaPlayer_fastplay(int speed);
typedef struct __FPlayer_API
{
char* module;
void (*setDataSourceAndHeaders)(char* ,void* , void* );
void (*setDataSourceFd)(int );
void (*setDataSourceCallback)(void* );
void (*setDataSourceIjkIOHttpCallback)(  void* ); 
void (*setVideoSurface)(  void* ,int ,int ,int ,int );
void (*prepareAsync)();
void (*start)(void);
void (*stop)(void);
void (*pause)(void);
void (*seekTo)( long );
BOOL (*isPlaying)(void);
long (*getCurrentPosition)();
long (*getDuration)(void);
long (*getBitRate)(void);
void (*release)(void);
void (*native_setup)( void* );
void (*reset)(void);
void (*setLoopCount)( int );
int   (*getLoopCount)(void);
float (*getPropertyFloat)( int , float );
void (*setPropertyFloat)( int , float );
long (*getPropertyLong)( int , long );
void (*setPropertyLong)(int , long );
void (*setStreamSelected)(int , int );
void (*setVolume)( float , float );
int   (*getAudioSessionId)(void);
void (*setOption)( int , object , object );
void (*setOptionLong)( int , object , long );
char* (*getColorFormatName)( int );
char* (*getVideoCodecInfo)(void);
char* (*getAudioCodecInfo)(void);
void* (*getMediaMeta)(void);
void (*native_init)(void);
void (*native_deinit)(void);
void (*native_finalize)( char* , char* );
void (*native_profileBegin)( char* );
void (*native_profileEnd)(void);
void (*native_setLogLevel)( int );
void (*setPostEvent)(void*,void* );
void (*invoke)(int ,object );
int   (*report_fpevent)(int ,void* ,void* ,void*);
int (*sendSqmMsg)(int );
void (*fastplay)(int);
}FPlayer_API;


#endif
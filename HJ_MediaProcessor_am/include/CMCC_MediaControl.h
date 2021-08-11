/*
 * author: jintao.xu@amlogic.com
 * date: 2015-01-10
 * wrap original source code for CMCC usage
 */

#ifndef _CMCC_MEDIACONTROL_H_
#define _CMCC_MEDIACONTROL_H_
#include "CMCC_Player.h"

class CMCC_MediaControl:public CmccPlayer
{

};

CMCC_MediaControl* GetMediaControl();  // { return NULL; }

int Get_MediaControlVersion();  //  { return 0; }

#endif  // _CMCC_MEDIACONTROL_H_

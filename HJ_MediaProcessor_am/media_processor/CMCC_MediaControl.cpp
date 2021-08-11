/*
 * author: jintao.xu@amlogic.com
 * date: 2015-01-10
 * wrap original source code for CMCC usage
 */

#include "CMCC_MediaControl.h"

CMCC_MediaControl* GetMediaControl()
{
	return new CMCC_MediaControl();
}

int Get_MediaControlVersion()
{
	return 1;
}

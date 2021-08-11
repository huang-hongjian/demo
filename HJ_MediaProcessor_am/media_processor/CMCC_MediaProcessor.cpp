/*
 * author: jintao.xu@amlogic.com
 * date: 2015-05-22
 * wrap original source code for Cmcc usage
 */

#include "Cmcc_MediaProcessor.h"
#include <android/log.h>    

// need single instance?

Cmcc_MediaProcessor* GetMediaProcessor()
{
    return new Cmcc_MediaProcessor();

	
}


int GetMediaProcessorVersion()
{
	return 2;
}

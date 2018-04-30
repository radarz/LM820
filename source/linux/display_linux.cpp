#include "display.h"

void  display_Init(void)
{

}

TBool display_SetRawImage(char *szWindowName, TU8 *pBuf, TU16 nWidth, TU16 nHeight)
{
    return TTrue;
}

TBool display_SetDepthImage(char *szWindowName, TU16 *pDepth, TU16 nDepthSize, float fFov)
{
    return TTrue;
}

void  display_ShowImage(TU32 nTimeout)
{

}

TU32  display_GetEvent(void)
{
    return DISPLAY_EVENT_NOEVENT;
}

void  display_SetStatInfo(char *szWindowName, TU32 nFrameCount, float fFps)
{

}

void  display_SetDebugImageInfo(char *szWindowName, TU8 nProgress, const char *szFileName)
{

}

void  display_SetFovDeviation(char *szWindowName, float fov_deviation)
{

}

void display_SaveCurrentImage(char *szWindowName, char *szFileName)
{

}


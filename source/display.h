#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "hal.h"

#define DISPLAY_EVENT_NOEVENT   (0)
#define DISPLAY_EVENT_CONT      ('c')
#define DISPLAY_EVENT_TRIG      ('t')
#define DISPLAY_EVENT_QUIT      ('q')
#define DISPLAY_EVENT_SAVE      ('s')

void  display_Init(void);
TBool display_SetRawImage(char *szWindowName, TU8 *pBuf, TU16 nWidth, TU16 nHeight);
TBool display_SetDepthImage(char *szWindowName, TU16 *pDepth, TU16 nDepthSize, float fFov);
void  display_ShowImage(TU32 nTimeout);
TU32  display_GetEvent(void);
void  display_SetStatInfo(char *szWindowName, TU32 nFrameCount, float fFps);
void  display_SetDebugImageInfo(char *szWindowName, TU8 nProgress, const char *szFileName);
void  display_SetFovDeviation(char *szWindowName, float fov_deviation);
void  display_SaveCurrentImage(char *szWindowName, char *szFileName);

#ifdef __cplusplus
}
#endif

#endif // __DISPLAY_H__
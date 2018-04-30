#ifndef __UTIL_H__
#define __UTIL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "hal.h"
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// LOG
enum{
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_TRACE
};

TBool LOG_Init(TBool bLogToScreen, const char * szFileName, TBool bCreateFile, TU8 nLogLevel);
void  LOG_DeInit(void);
void  LOG_Print(TU8 nLogLevel, const char *szFmt, ...);
void  LOG_PrintFrame(TU8 nLogLevel, const char *szPrefix, TU8 *pBuf, TU16 nLen);
void  LOG_INFO_Print(const char *szFmt, ...);
void  LOG_TRACE_PrintFrame(const char *szPrefix, TU8 *pBuf, TU16 nLen);
void  LOG_PrintData(TU16 *pData, TU16 nLen, TU16 nFrames);///mydatalog

#define LOG             LOG_INFO_Print
#define LOG_Frame       LOG_TRACE_PrintFrame
#define LOG_Data		LOG_PrintData

////////////////////////////////////////////////////////////////////////////////
// Timer
typedef TU32 Counter_t;

typedef struct {
    Counter_t delay;
    Counter_t start;
    TU8 flag;
} Timer_t;
    
TBool TIMER_SetDelay_ms(Timer_t *timer, Counter_t delay);
void  TIMER_Start(Timer_t *timer);
void  TIMER_Stop(Timer_t *timer);
TBool TIMER_Elapsed(Timer_t *timer);
TBool TIMER_isStarted(Timer_t *timer);

////////////////////////////////////////////////////////////////////////////////
// CRC
TU8 CRC_CalCrc8(TU8 *pBuf, TU16 nLen, TU8 nPrev);

////////////////////////////////////////////////////////////////////////////////
// Misc
#define UTIL_TAB_SIZE(t)        (sizeof(t)/sizeof(t[0]))
#define UTIL_ABS(n)             (((n)>=0)?(n):(-(n)))

#define UTIL_DEC_TU16_MSBF(p)   ( ( ((TU16)*((TU8 *)(p))) << 8) + ((TU16)*(((TU8*)(p))+1)) )
#define UTIL_DEC_TU16_LSBF(p)   ( ( ((TU16)*(((TU8*)(p))+1)) << 8) + ((TU16)*((TU8*)(p))) )
#define UTIL_DEC_TU32_MSBF(p)   ( ( ((TU32)UTIL_DEC_TU16_MSBF(p)) << 16) + UTIL_DEC_TU16_MSBF((p)+2) )
#define UTIL_DEC_TU32_LSBF(p)   ( ( ((TU32)UTIL_DEC_TU16_LSBF((p)+2)) << 16) + UTIL_DEC_TU16_LSBF(p) )

#define UTIL_MAX(a, b)  ((a) > (b) ? (a) : (b))
#define UTIL_MIN(a, b)  ((a) < (b) ? (a) : (b))

////////////////////////////////////////////////////////////////////////////////
// INI parser
TBool INI_Init(const char *pIniFile);
char *INI_GetValueByKey(const char *pKey);

////////////////////////////////////////////////////////////////////////////////
// Bitmap operation
#define UTIL_BMP_SETBIT(bmp, i) ((bmp)[(i)>>3] |= (1 << ((i)&0x07)))
#define UTIL_BMP_CLRBIT(bmp, i) ((bmp)[(i)>>3] &= ~(1 << ((i)&0x07)))
#define UTIL_BMP_CHKBIT(bmp, i) ((bmp)[(i)>>3] &  (1 << ((i)&0x07)))

#ifdef __cplusplus
}
#endif

#endif // __UTIL_H__
#ifndef __HAL_H__
#define __HAL_H__

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// Types
typedef unsigned long   TU32;
typedef   signed long   TS32;
typedef unsigned short  TU16;
typedef   signed short  TS16;
typedef unsigned char   TU8;
typedef   signed char   TS8;
typedef          char   TChar;
typedef unsigned char   TBool;
typedef          void   TVoid;
typedef         float   TFloat;
typedef        double   TDouble;
    
#define TFalse          ((TBool)0)
#define TTrue           ((TBool)(!TFalse))
    
#define TU8_MAX         ((TU8)255)
#define TS8_MAX         ((TS8)127)
#define TS8_MIN         ((TS8)-128)
#define TU16_MAX        ((TU16)65535)
#define TS16_MAX        ((TS16)32767)
#define TS16_MIN        ((TS16)-32768)
#define TU32_MAX        ((TU32)4294967295uL)
#define TS32_MAX        ((TS32)2147483647)
#define TS32_MIN        ((TS32)-2147483648)

#ifndef NULL
#define NULL    (0)
#endif

typedef TU32                    UTIL_HANDLE;
#define INVALID_UTIL_HANDLE     ((UTIL_HANDLE)-1)
    
////////////////////////////////////////////////////////////////////////////////
// Timer
TU32 TIMER_GetNow(void);

////////////////////////////////////////////////////////////////////////////////
// Sleep
void UTIL_Sleep(TU32 nTmInMs);

////////////////////////////////////////////////////////////////////////////////
// UART
enum {
    UART_IO_CTS = 0
};

UTIL_HANDLE UART_Init(const char *szName);
void  UART_Close(UTIL_HANDLE nHandle);
TU32  UART_Read(UTIL_HANDLE nHandle, TU8 * pBuf, TU32 nBufLen);
TU32  UART_Write(UTIL_HANDLE nHandle, TU8 * pBuf, TU32 nLen);
TBool UART_GetIO(UTIL_HANDLE nHandle, TU32 nIO, TBool *pIsHigh);
void  UART_FlushTX(UTIL_HANDLE nHandle);
void  UART_FlushRX(UTIL_HANDLE nHandle);

////////////////////////////////////////////////////////////////////////////////
// Locker
UTIL_HANDLE UTIL_CreateLock(void);
void UTIL_Lock(UTIL_HANDLE hLock);
void UTIL_Unlock(UTIL_HANDLE hLock);
void UTIL_DeleteLock(UTIL_HANDLE hLock);

////////////////////////////////////////////////////////////////////////////////
// Thread
typedef void * (*UTIL_CB_FUNC)(void *);

UTIL_HANDLE THREAD_Create(UTIL_CB_FUNC pCbFunc, void * pParam);
// UTIL_HANDLE THREAD_CreateEx(void (*pCbFunc)( void * ), void * pParam, TU32 nPriority, TU32 nStackSize);
TBool THREAD_IsExist(UTIL_HANDLE nHandle);
void  THREAD_Terminate(UTIL_HANDLE nHandle);

////////////////////////////////////////////////////////////////////////////////
// Socket
UTIL_HANDLE SOCK_Open(TU32 nAddr, TU16 nPort);
TS32  SOCK_GetRxBufLen(UTIL_HANDLE hSock);
TS32  SOCK_Read(UTIL_HANDLE hSock, TU8 * pBuf, TS32 nBufLen, TU32 nTmout);
TS32  SOCK_Write(UTIL_HANDLE hSock, TU8 * pBuf, TS32 nBufLen);
void  SOCK_Close(UTIL_HANDLE hSock);
    
#ifdef __cplusplus
}
#endif

#endif // __HAL_H__
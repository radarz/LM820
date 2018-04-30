#include "hal.h"
#include <stdio.h>
#include <windows.h>

#define _LOG_   printf

////////////////////////////////////////////////////////////////////////////////
// Timer
TU32 TIMER_GetNow(void)
{
    return (TU32)GetTickCount();
}

////////////////////////////////////////////////////////////////////////////////
// Process control
void UTIL_Sleep(TU32 nTmInMs)
{
    Sleep(nTmInMs);
}

void UTIL_Exit(void)
{
    exit(1);
}

////////////////////////////////////////////////////////////////////////////////
// UART
#define UART_BAUD_RATE      (115200)
#define UART_TXRX_BUF       (4096)

UTIL_HANDLE UART_Init(const char *szName)
{
    HANDLE  hComFile;
    DCB     commDCB;
    COMMTIMEOUTS    ComTmouts;
    
    if (szName == NULL) return INVALID_UTIL_HANDLE;
    
    hComFile = CreateFileA(szName, GENERIC_READ|GENERIC_WRITE, \
        0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hComFile == INVALID_HANDLE_VALUE)
    {
        return INVALID_UTIL_HANDLE;
    }
    
    memset(&commDCB, 0, sizeof(DCB));
    commDCB.DCBlength = sizeof(DCB);
    if( ! GetCommState(hComFile, &commDCB) )
    {
        CloseHandle(hComFile);
        return INVALID_UTIL_HANDLE;
    }
    // set com parameter
    commDCB.BaudRate = UART_BAUD_RATE;
    commDCB.ByteSize = 8;
    commDCB.Parity   = NOPARITY;  
    commDCB.StopBits = ONESTOPBIT;
    
    if ( ! SetCommState(hComFile, &commDCB) )
    {
        CloseHandle(hComFile);
        return INVALID_UTIL_HANDLE;
    }
    
    // set com tx/rx buffer
    if ( ! SetupComm(hComFile, UART_TXRX_BUF, UART_TXRX_BUF) )
    {
        CloseHandle(hComFile);
        return INVALID_UTIL_HANDLE;
    }
    
    // setup timeout for synch reading...
    GetCommTimeouts(hComFile, &ComTmouts);
    //   return immediately
    ComTmouts.ReadIntervalTimeout = 0;
    ComTmouts.ReadTotalTimeoutMultiplier = 0;
    ComTmouts.ReadTotalTimeoutConstant = 10;
    ComTmouts.WriteTotalTimeoutMultiplier = 0;
    ComTmouts.WriteTotalTimeoutConstant = 0;
    SetCommTimeouts(hComFile, &ComTmouts);
    
    // clear the TX/RX buffer
    PurgeComm(hComFile, PURGE_TXABORT|PURGE_RXABORT|PURGE_TXCLEAR|PURGE_RXCLEAR );
    
    if (!SetCommMask(hComFile, EV_CTS))
    {
        CloseHandle(hComFile);
        return INVALID_UTIL_HANDLE;
    }
    
    return (UTIL_HANDLE)hComFile;
}

void  UART_Close(UTIL_HANDLE nHandle)
{
    if (nHandle != INVALID_UTIL_HANDLE)
    {
        CloseHandle((HANDLE)nHandle);
    }
}

TU32  UART_Read(UTIL_HANDLE nHandle, TU8 * pBuf, TU32 nBufLen)
{
    DWORD   dwByteRead = 0;
    
    if (nHandle == INVALID_UTIL_HANDLE || pBuf == NULL || nBufLen == 0) return 0;
    
    if (!ReadFile((HANDLE)nHandle, pBuf, (DWORD)nBufLen, &dwByteRead, NULL)) return 0;
    
    return (TU32)dwByteRead;
}

TU32  UART_Write(UTIL_HANDLE nHandle, TU8 * pBuf, TU32 nLen)
{
    DWORD   dwByteWritten = 0;
    
    if (nHandle == INVALID_UTIL_HANDLE || pBuf == NULL || nLen == 0) return 0;
    
    if (!WriteFile ((HANDLE)nHandle, pBuf, nLen, &dwByteWritten, NULL)) return 0;
    
    return (TU32)dwByteWritten;
}

TBool UART_GetIO(UTIL_HANDLE nHandle, TU32 nIO, TBool *pIsHigh)
{
    DWORD   dwState;
    
    if (GetCommModemStatus((HANDLE)nHandle, &dwState)) return TFalse;
    
    switch (nIO)
    {
    case UART_IO_CTS:
        *pIsHigh = (dwState & MS_CTS_ON) ? TTrue : TFalse;
        break;
    default:
        return TFalse;
    }
    
    return TTrue;
}

void  UART_FlushTX(UTIL_HANDLE nHandle)
{
    PurgeComm((HANDLE)nHandle, PURGE_TXABORT | PURGE_TXCLEAR);
}

void  UART_FlushRX(UTIL_HANDLE nHandle)
{
    PurgeComm((HANDLE)nHandle, PURGE_RXABORT | PURGE_RXCLEAR);
}

////////////////////////////////////////////////////////////////////////////////
// localize objects table, like thread, mutex, ...
#define TAB_ALLOC(t)        ( TabAlloc((t), sizeof(t)/sizeof(t[0])) )
#define TAB_FREE(t, i)      ( TabFree((t), sizeof(t)/sizeof(t[0]), (i)) )
#define TAB_IS_EMPTY(t)     ( TabisEmpty((t), sizeof(t)/sizeof(t[0])) )

static TU32 TabAlloc(TU8 * pTab, TU32 nLen)
{
    TU32    i;
    
    for (i=0; i<nLen; i++)
    {
        if (pTab[i] == 0)
        {
            pTab[i] = 1;
            return i;
        }
    }
    
    return INVALID_UTIL_HANDLE;
}

static void TabFree(TU8 * pTab, TU32 nLen, TU32 nIdx)
{
    if (nIdx >=0 && nIdx < nLen)
    {
        pTab[nIdx] = 0;
    }
}

static TBool TabisEmpty(TU8 * pTab, TU32 nLen)
{
    TU32    i;
    for (i=0; i<nLen; i++)
    {
        if (pTab[i] != 0) return TFalse;
    }
    return TTrue;
}

////////////////////////////////////////////////////////////////////////////////
// Locker
#define UTIL_MAX_LOCKER     (64)

static TU8              g_tMutexAllocTab[UTIL_MAX_LOCKER];
static CRITICAL_SECTION g_tMutexHandleTab[UTIL_MAX_LOCKER];
static TU8              g_bMutexTabInited = 0;

#define IS_MUTEX_VALID(h)   ((TU32)(h) < (TU32)UTIL_MAX_LOCKER)

static void MutexTabInit(void)
{
    if (!g_bMutexTabInited)
    {
        memset(g_tMutexAllocTab, 0, sizeof(g_tMutexAllocTab));
        memset(g_tMutexHandleTab, 0, sizeof(g_tMutexHandleTab));
        
        g_bMutexTabInited = 1;
    }
}

UTIL_HANDLE UTIL_CreateLock(void)
{
    TU32    idx;
    
    if (!g_bMutexTabInited) MutexTabInit();
    
    idx = TAB_ALLOC(g_tMutexAllocTab);
    
    if (IS_MUTEX_VALID(idx))
    {
        InitializeCriticalSection(&g_tMutexHandleTab[idx]);
    }
    else
    {
        idx = INVALID_UTIL_HANDLE;
    }
    
    return (UTIL_HANDLE)idx;
}

void UTIL_Lock(UTIL_HANDLE hLock)
{
    if (IS_MUTEX_VALID(hLock))
    {
        EnterCriticalSection(&g_tMutexHandleTab[hLock]);
    }
}

void UTIL_Unlock(UTIL_HANDLE hLock)
{
    if (IS_MUTEX_VALID(hLock))
    {
        LeaveCriticalSection(&g_tMutexHandleTab[hLock]);
    }
}

void UTIL_DeleteLock(UTIL_HANDLE hLock)
{
    if (IS_MUTEX_VALID(hLock))
    {
        DeleteCriticalSection(&g_tMutexHandleTab[hLock]);
        memset(&g_tMutexHandleTab[hLock], 0, sizeof(g_tMutexHandleTab[0]));
        TAB_FREE(g_tMutexAllocTab, hLock);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Thread
UTIL_HANDLE THREAD_Create(UTIL_CB_FUNC pCbFunc, void * pParam)
{
    HANDLE hThread = NULL;
    DWORD  dwThreadId = 0;
    
    hThread = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size
        (LPTHREAD_START_ROUTINE)pCbFunc,            // thread function name
        pParam,                 // argument to thread function
        0,                      // use default creation flags
        &dwThreadId);           // returns the thread identifier
    
    return (TU32)hThread;
}

TBool THREAD_IsExist(UTIL_HANDLE nHandle)
{
    TU32    nExitCode = 0;
    
    GetExitCodeThread((HANDLE)nHandle, &nExitCode);
    
    return (nExitCode == STILL_ACTIVE) ? TTrue : TFalse;
}

void  THREAD_Terminate(UTIL_HANDLE nHandle)
{
    TerminateThread((HANDLE)nHandle, 0);
}

////////////////////////////////////////////////////////////////////////////////
// Socket
#pragma comment(lib, "wsock32.lib")

typedef struct {
    int                 is_inited;
    int                 is_svr;
    struct sockaddr_in  addr;
    SOCKET              fd_listen;
    SOCKET              fd_conn;
}TSockVar;

#define UTIL_MAX_SOCKET     (4)
#define SOCK_TIMEOUT        (0)
#define SOCK_WELL_CLOSED    (-1)
#define SOCK_ERROR_OCCUR    (-2)

#define UTIL_SOCK_INFINITE  (TU32_MAX)

static TU8              g_tSocketAllocTab[UTIL_MAX_SOCKET];
static TSockVar         g_tSocketHandleTab[UTIL_MAX_SOCKET];
static TU8              g_bSocketTabInited = 0;
#define IS_SOCKET_VALID(h)  ((TU32)(h) < (TU32)UTIL_MAX_SOCKET)

static void SocketTabInit(void)
{
    WSADATA wsd;
    
    if (!g_bSocketTabInited)
    {
        memset(g_tSocketAllocTab, 0, sizeof(g_tSocketAllocTab));
        memset(g_tSocketHandleTab, 0, sizeof(g_tSocketHandleTab));
        
        // init WSA
        if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
        {
            return;
        }
        
        g_bSocketTabInited = 1;
    }
}

static TSockVar * SOCK_GetVar(UTIL_HANDLE hSock)
{
    if (!IS_SOCKET_VALID(hSock)) return NULL;
    
    if (!g_tSocketHandleTab[hSock].is_inited || g_tSocketHandleTab[hSock].fd_conn < 0) return NULL;
    
    return &g_tSocketHandleTab[hSock];
}

static int SOCK_SvrOpen(TSockVar * pVar)
{ 
    u_long blocking = 1;
    // create socket
    if ( (pVar->fd_listen = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
    {
        _LOG_("create socket error: %s (errno: %d) \n", strerror(errno), errno);
        return -1;
    }
    
    // bind
    if ( bind(pVar->fd_listen, (struct sockaddr*)&pVar->addr, sizeof(pVar->addr)) == -1 )
    {
        _LOG_("bind socket error: %s (errno: %d) \n", strerror(errno), errno);
        return -1;
    }
    
    // listen
    if ( listen(pVar->fd_listen, 10) == -1 )
    {
        _LOG_("listen socket error: %s (errno: %d) \n", strerror(errno), errno);
        return -1;
    }
    
    _LOG_("SOCK_SVR: listening on port (%d) ...\n", ntohs(pVar->addr.sin_port));
    
    // wait for accept
    do {
        pVar->fd_conn = accept(pVar->fd_listen, (struct sockaddr*)NULL, NULL);
        if (pVar->fd_conn < 0)
        {
            _LOG_("accept socket error: %s (errno: %d) \n", strerror(errno), errno);
        }
    } while (pVar->fd_conn < 0);
    
    // set to non-block socket to use select()
    if (ioctlsocket(pVar->fd_conn, FIONBIO, &blocking) == SOCKET_ERROR)
    {
        _LOG_("set non-block socket failed.\n");
        return -1;
    }
    
    return 0;
}

static int SOCK_CltOpen(TSockVar * pVar)
{
    u_long blocking = 1;
    
    _LOG_("\nConnecting sock to <%s : %d> ... ", inet_ntoa(pVar->addr.sin_addr), ntohs(pVar->addr.sin_port));    
    
    // create socket
    if ( (pVar->fd_conn = socket(AF_INET, SOCK_STREAM, 0))  == -1 )
    {
        _LOG_("create socket error: %s (errno: %d) \n", strerror(errno), errno);
        return -1;
    }   
    // connect
    if ( connect(pVar->fd_conn, (struct sockaddr*)&pVar->addr, sizeof(pVar->addr)) < 0 )
    {
        _LOG_("connect socket error: %s (errno: %d) \n", strerror(errno), errno);
        return -1;
    }
    // set to non-block socket to use select()
    if (ioctlsocket(pVar->fd_conn, FIONBIO, &blocking) == SOCKET_ERROR)
    {
        _LOG_("set non-block socket failed.\n");
        return -1;
    }   
    _LOG_("\nConnected!\n\n");
    
    return 0;
}

UTIL_HANDLE SOCK_Open(TU32 nAddr, TU16 nPort)
{
    int     ret = -1;
    TU32    idx;
    TSockVar    * pVar;
    
    if (!g_bSocketTabInited) SocketTabInit();
    
    idx = TAB_ALLOC(g_tSocketAllocTab);
    
    if (!IS_SOCKET_VALID(idx)) return INVALID_UTIL_HANDLE;
    
    pVar = &g_tSocketHandleTab[idx];
    
    if (pVar->is_inited) SOCK_Close(idx);
    
    // copy the configurations
    pVar->is_svr = (nAddr == 0) ? 1 : 0;
    
    memset(&pVar->addr, 0, sizeof(struct sockaddr_in));
    pVar->addr.sin_family = AF_INET;
    pVar->addr.sin_port = htons(nPort);
    if (pVar->is_svr)
    {
        pVar->addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    else
    {
        pVar->addr.sin_addr.s_addr = htonl(nAddr);
    }
    
    pVar->fd_listen = -1;
    pVar->fd_conn = -1;
    
    ret = (pVar->is_svr ? SOCK_SvrOpen(pVar) : SOCK_CltOpen(pVar));
    if (ret < 0)
    {
        SOCK_Close(idx);
        return INVALID_UTIL_HANDLE;
    }
    
    pVar->is_inited = 1;
    
    return (UTIL_HANDLE)idx;
}

TS32  SOCK_GetRxBufLen(UTIL_HANDLE hSock)
{
    TSockVar    * pVar = SOCK_GetVar(hSock);
    TS32          nRet, nSize;
    
    if (pVar == NULL) return SOCK_ERROR_OCCUR;
    
    nRet = ioctlsocket(pVar->fd_conn, FIONREAD, &nSize);
    if (nRet < 0) return SOCK_ERROR_OCCUR;
    
    return nSize;
}

TS32  SOCK_Read(UTIL_HANDLE hSock, TU8 * pBuf, TS32 nBufLen, TU32 nTmout)
{
    int         nRet, nErr;
    TSockVar    * pVar = SOCK_GetVar(hSock);
    
    struct timeval read_timeout ;  
    fd_set  read_fd;    
    
    if (pVar == NULL) return SOCK_ERROR_OCCUR;
    
    // select() if receive data available
    FD_ZERO(&read_fd);
    FD_SET(pVar->fd_conn, &read_fd);
    
    read_timeout.tv_sec = nTmout / 1000;
    read_timeout.tv_usec = (nTmout - (nTmout / 1000) * 1000) * 1000;
    
    nRet = select(0, &read_fd, 0, 0, (nTmout != UTIL_SOCK_INFINITE) ? &read_timeout : NULL);
    if (nRet == 0)
    {
        // timeout
        return SOCK_TIMEOUT;
    }
    else if ( nRet > 0 && FD_ISSET(pVar->fd_conn, &read_fd))
    {
        // start receive
        nRet = recv(pVar->fd_conn, (char *)pBuf, nBufLen, 0);
        
        if (nRet > 0) return nRet;
        else if (nRet == 0) return SOCK_WELL_CLOSED;
    }
    
    // nRet must < 0 now
    nErr = WSAGetLastError();
    
    if (nErr == WSAEINTR || nErr == WSAEINPROGRESS || nErr == WSAEMSGSIZE)
    {
        return 0;
    }
    else
    {
        return SOCK_ERROR_OCCUR;
    }
}

TS32  SOCK_Write(UTIL_HANDLE hSock, TU8 * pBuf, TS32 nBufLen)
{
    int         nErr, nRet, nSent = 0;
    TSockVar  * pVar = SOCK_GetVar(hSock);
    
    if (pVar == NULL) return SOCK_ERROR_OCCUR;
    
    while (nSent < nBufLen)
    {
        nRet = send(pVar->fd_conn, (char *)(pBuf + nSent), nBufLen - nSent, 0);
        
        if (nRet == SOCKET_ERROR)
        {
            nErr = WSAGetLastError();
            
            if (nErr == WSAEWOULDBLOCK)
            {
                Sleep(1);
                continue;
            }
            else
            {
                return SOCK_ERROR_OCCUR;
            }
        }
        else
        {
            nSent += nRet;
        }
    }
    
    return nBufLen;
}

void  SOCK_Close(UTIL_HANDLE hSock)
{
    TSockVar    * pVar;
    
    if (!IS_SOCKET_VALID(hSock)) return;
    
    pVar = &g_tSocketHandleTab[hSock];
    
    if (pVar->fd_conn >= 0)
    {
        closesocket(pVar->fd_conn);
        pVar->fd_conn = -1;
    }
    
    if (pVar->fd_listen >= 0)
    {
        closesocket(pVar->fd_listen);
        pVar->fd_listen = -1;
    }
    
    pVar->is_inited = 0;
    
    TAB_FREE(g_tSocketAllocTab, hSock);
    
    if (TAB_IS_EMPTY(g_tSocketAllocTab))
    {
        WSACleanup();
        g_bSocketTabInited = 0;
    }
}
#include "hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>

#define _LOG_   printf

////////////////////////////////////////////////////////////////////////////////
// Timer
TU32 TIMER_GetNow(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) return 0;
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

////////////////////////////////////////////////////////////////////////////////
// Process control
void UTIL_Sleep(TU32 nTmInMs)
{
    usleep(nTmInMs * 1000);
}

void UTIL_Exit(void)
{
    exit(1);
}

////////////////////////////////////////////////////////////////////////////////
// UART
UTIL_HANDLE UART_Init(const char *szName)
{
    int fd = -1;
    struct termios   Opt;
    
    fd = open(szName, O_RDWR | O_NOCTTY | O_NDELAY);
    if(fd < 0)
    {
        perror("unable to open comport\n");
        return INVALID_UTIL_HANDLE;
    }
    
    memset(&Opt, 0, sizeof(Opt));  /* clear the new struct */
    
    Opt.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
    Opt.c_iflag = IGNPAR;
    Opt.c_oflag = 0;
    Opt.c_lflag = 0;
    Opt.c_cc[VMIN] = 0; 
    Opt.c_cc[VTIME] = 0;
    
    if (tcsetattr(fd, TCSANOW, &Opt) < 0)
    {
        close(fd);
        perror("unable to adjust portsettings ");
        return INVALID_UTIL_HANDLE;
    }
    
    return (UTIL_HANDLE)fd;
}

void  UART_Close(UTIL_HANDLE nHandle)
{
    close((int)nHandle);
}

TU32  UART_Read(UTIL_HANDLE nHandle, TU8 * pBuf, TU32 nBufLen)
{
    ssize_t ret;

#ifndef __STRICT_ANSI__                       /* __STRICT_ANSI__ is defined when the -ansi option is used for gcc */
    if(nBufLen>SSIZE_MAX)   nBufLen = (int)SSIZE_MAX;  /* SSIZE_MAX is defined in limits.h */
#else
    if(nBufLen>4096)        nBufLen = 4096;
#endif

    ret = read((int)nHandle, pBuf, nBufLen);

    return (ret < 0 ? 0 : (TU32)ret);
}

TU32  UART_Write(UTIL_HANDLE nHandle, TU8 * pBuf, TU32 nLen)
{
    ssize_t ret = write((int)nHandle, pBuf, nLen);

    return (ret < 0 ? 0 : (TU32)ret);
}

TBool UART_GetIO(UTIL_HANDLE nHandle, TU32 nIO, TBool *pIsHigh)
{
    int status;

    ioctl((int)nHandle, TIOCMGET, &status);

    switch (nIO)
    {
    case UART_IO_CTS:
        *pIsHigh = (status & TIOCM_CTS) ? TTrue : TFalse;
        break;
    default:
        return TFalse;
    }
    
    return TTrue;
}

void  UART_FlushTX(UTIL_HANDLE nHandle)
{
    tcflush((int)nHandle,TCOFLUSH);
}

void  UART_FlushRX(UTIL_HANDLE nHandle)
{
    tcflush((int)nHandle,TCIFLUSH);
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

/*
static TBool TabisEmpty(TU8 * pTab, TU32 nLen)
{
    TU32    i;
    for (i=0; i<nLen; i++)
    {
        if (pTab[i] != 0) return TFalse;
    }
    return TTrue;
}
*/

////////////////////////////////////////////////////////////////////////////////
// Critical Section Wrapper
#define UTIL_MAX_LOCKER     (64)
static TU8              g_tMutexAllocTab[UTIL_MAX_LOCKER];
static pthread_mutex_t  g_tMutexHandleTab[UTIL_MAX_LOCKER];
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

// defined in pthread.h, but been disabled by "#define __USE_UNIX98"
extern int pthread_mutexattr_settype (pthread_mutexattr_t *__attr, int __kind) __THROW;

void InitializeCriticalSection(UTIL_HANDLE *mutex)
{
    pthread_mutexattr_t mutexattr;  // Mutex attribute variable
    TU32    idx;
    
    if (!g_bMutexTabInited) MutexTabInit();
    
    idx = TAB_ALLOC(g_tMutexAllocTab);
    
    if (IS_MUTEX_VALID(idx))
    {
        // Set the mutex as recursive number
        pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE_NP);
        
        // Create mutex
        pthread_mutex_init(&g_tMutexHandleTab[idx], &mutexattr);
        
        // Destroy mutex attr
        pthread_mutexattr_destroy(&mutexattr);
        
        *mutex = (UTIL_HANDLE)idx;
    }
    else
    {
        *mutex = INVALID_UTIL_HANDLE;
    }
}

void EnterCriticalSection(UTIL_HANDLE *mutex)
{
    if (IS_MUTEX_VALID(*mutex))
    {
        pthread_mutex_lock(&g_tMutexHandleTab[*mutex]);
    }
}

void LeaveCriticalSection(UTIL_HANDLE *mutex)
{
    if (IS_MUTEX_VALID(*mutex))
    {
        pthread_mutex_unlock(&g_tMutexHandleTab[*mutex]);
    }
}

void DeleteCriticalSection(UTIL_HANDLE *mutex)
{
    if (IS_MUTEX_VALID(*mutex))
    {
        pthread_mutex_destroy(&g_tMutexHandleTab[*mutex]);
        memset(&g_tMutexHandleTab[*mutex], 0, sizeof(g_tMutexHandleTab[0]));
        TAB_FREE(g_tMutexAllocTab, *mutex);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Locker
UTIL_HANDLE UTIL_CreateLock(void)
{
    UTIL_HANDLE hLock;
    
    InitializeCriticalSection(&hLock);

    return hLock;
}

void UTIL_Lock(UTIL_HANDLE hLock)
{
    EnterCriticalSection(&hLock);
}

void UTIL_Unlock(UTIL_HANDLE hLock)
{
    LeaveCriticalSection(&hLock);
}

void UTIL_DeleteLock(UTIL_HANDLE hLock)
{
    DeleteCriticalSection(&hLock);
}

////////////////////////////////////////////////////////////////////////////////
// Thread
#define UTIL_MAX_THREAD     (64)

static TU8          g_tThreadAllocTab[UTIL_MAX_THREAD];
static pthread_t    g_tThreadHandleTab[UTIL_MAX_THREAD];
static TU8          g_bThreadTabInited = 0;

#define IS_THREAD_VALID(h)  ((TU32)(h) < (TU32)UTIL_MAX_THREAD)

static void ThreadTabInit(void)
{
    if (!g_bThreadTabInited)
    {
        memset(&g_tThreadAllocTab[0], 0, sizeof(g_tThreadAllocTab));
        memset(&g_tThreadHandleTab[0], 0, sizeof(g_tThreadHandleTab));
        
        g_bThreadTabInited = 1;
    }
}

UTIL_HANDLE THREAD_Create(UTIL_CB_FUNC pCbFunc, void * pParam)
{
    pthread_t       thread_t;
    pthread_attr_t  threadAttr;  // attribute variable    
    
    TU32            idx;

    if (!g_bThreadTabInited) ThreadTabInit();
    
    idx = TAB_ALLOC(g_tThreadAllocTab);
    
    if ( ! IS_THREAD_VALID(idx) )   return INVALID_UTIL_HANDLE;

    // init
    pthread_attr_init(&threadAttr);
    
    // stack size
    pthread_attr_setstacksize(&threadAttr, 12000*1024);
    
    // detached state
    pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);
    
    // create thread
    pthread_create(&thread_t, &threadAttr, pCbFunc, pParam);
    
    // Destroy mutex attr
    pthread_attr_destroy(&threadAttr);
    
    g_tThreadHandleTab[idx] = thread_t;
    
    return (UTIL_HANDLE)idx;
}

TBool THREAD_IsExist(UTIL_HANDLE nHandle)
{
    int nRet = TFalse;

    if (IS_THREAD_VALID(nHandle))
    {
        nRet = pthread_kill(g_tThreadHandleTab[nHandle], 0);
    }
    
    return (nRet != ESRCH) ? TTrue : TFalse;
}

void  THREAD_Terminate(UTIL_HANDLE nHandle)
{
    if (IS_THREAD_VALID(nHandle))
    {
        pthread_cancel(g_tThreadHandleTab[nHandle]);
        memset(&g_tThreadHandleTab[nHandle], 0, sizeof(g_tThreadHandleTab[0]));
        TAB_FREE(g_tThreadAllocTab, nHandle);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Socket
typedef struct {
    int                 is_inited;
    int                 is_svr;
    struct sockaddr_in  addr;
    int                 fd_listen;
    int                 fd_conn;
}TSockVar;

#define UTIL_MAX_SOCKET     (4)
#define SOCK_TIMEOUT        (0)
#define SOCK_WELL_CLOSED    (-1)
#define SOCK_ERROR_OCCUR    (-2)

static TU8              g_tSocketAllocTab[UTIL_MAX_SOCKET];
static TSockVar         g_tSocketHandleTab[UTIL_MAX_SOCKET];
static TU8              g_bSocketTabInited = 0;

#define IS_SOCKET_VALID(h)  ((TU32)(h) < (TU32)UTIL_MAX_SOCKET)
#define UTIL_SOCK_INFINITE  (TU32_MAX)

static void SocketTabInit(void)
{
    if (!g_bSocketTabInited)
    {
        memset(g_tSocketAllocTab, 0, sizeof(g_tSocketAllocTab));
        memset(g_tSocketHandleTab, 0, sizeof(g_tSocketHandleTab));

        // disable the SIGPIPE signal, otherwise daemon would quite!
        signal(SIGPIPE,SIG_IGN); 
		signal(SIGHUP,SIG_IGN); 

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
    // create socket
    if ( (pVar->fd_listen = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
    {
        _LOG_("create socket error: %s (errno: %d) \n", strerror(errno), errno);
        return -1;
    }

    // bind
    if ( bind(pVar->fd_listen, (struct sockaddr*)&pVar->addr, sizeof(pVar->addr))  == -1 )
    {
        _LOG_("bind socket error: %s (errno: %d) \n", strerror(errno), errno);
        return -1;
    }

    // listen
    if ( listen(pVar->fd_listen, 10)  == -1 )
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
    fcntl(pVar->fd_conn, F_SETFL, O_NONBLOCK);

    return 0;
}

static int SOCK_CltOpen(TSockVar * pVar)
{
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
    fcntl(pVar->fd_conn, F_SETFL, O_NONBLOCK);

    _LOG_("\nConnected sock to <%s : %d>!\n\n", inet_ntoa(pVar->addr.sin_addr), ntohs(pVar->addr.sin_port));

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
    
    nRet = ioctl(pVar->fd_conn, FIONREAD, &nSize);
    if (nRet < 0) return SOCK_ERROR_OCCUR;
    
    return nSize;
}

TS32  SOCK_Read(UTIL_HANDLE hSock, TU8 * pBuf, TS32 nBufLen, TU32 nTmout)
{
    int     nRet;
    
    TSockVar    * pVar = SOCK_GetVar(hSock);
    
    struct timeval read_timeout ;  
    fd_set  read_fd; 	

    if (pVar == NULL) return SOCK_ERROR_OCCUR;

    // select() if receive data available
    FD_ZERO(&read_fd);
    FD_SET(pVar->fd_conn, &read_fd);
    
    read_timeout.tv_sec = nTmout / 1000;
    read_timeout.tv_usec = (nTmout - (nTmout / 1000) * 1000) * 1000;

    nRet = select(pVar->fd_conn + 1, &read_fd, 0, 0, (nTmout != UTIL_SOCK_INFINITE) ? &read_timeout : NULL);
    if (nRet == 0)
    {
        // timeout
        return SOCK_TIMEOUT;
    }
    else if ( nRet > 0 && FD_ISSET(pVar->fd_conn, &read_fd))
    {
        // start receive
        nRet = recv(pVar->fd_conn, (char *)pBuf, nBufLen, MSG_DONTWAIT);
        if (nRet > 0) return nRet;
        else if (nRet == 0) return SOCK_WELL_CLOSED;
    }
    
    return SOCK_ERROR_OCCUR;
}

TS32  SOCK_Write(UTIL_HANDLE hSock, TU8 * pBuf, TS32 nBufLen)
{
    int         nRet, nSent = 0;
    TSockVar  * pVar = SOCK_GetVar(hSock);
    
    if (pVar == NULL) return SOCK_ERROR_OCCUR;
    
    while (nSent < nBufLen)
    {
        nRet = send(pVar->fd_conn, (char *)(pBuf + nSent), nBufLen - nSent, 0);

        if (nRet < 0)
        {
            if (errno == EAGAIN)
            {
                usleep(1000);       // wait 1ms
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
        close(pVar->fd_conn);
        pVar->fd_conn = -1;
    }
    if (pVar->fd_listen >= 0)
    {
        close(pVar->fd_listen);
        pVar->fd_listen = -1;
    }
    pVar->is_inited = 0;

    TAB_FREE(g_tSocketAllocTab, hSock);
}

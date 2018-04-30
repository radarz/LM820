#include "xcom_port.h"
#include "util.h"

static UTIL_HANDLE g_hPort = INVALID_UTIL_HANDLE;

TBool xcom_port_open(const TU8 *szPort)
{
    if (g_hPort != INVALID_UTIL_HANDLE)
    {
        UART_Close(g_hPort);
        g_hPort = INVALID_UTIL_HANDLE;
    }

    g_hPort = UART_Init((const char *)szPort);
    
    return (TBool)(g_hPort != INVALID_UTIL_HANDLE);    
}

TU16  xcom_port_send(TU8 * pBuf, TU16 nLen)
{
    TU16 nRet = (TU16)UART_Write(g_hPort, pBuf, (TU32)nLen);

    if (nRet > 0) LOG_Frame("UART TX: ", pBuf, nRet);
    
    return nRet;
}

TU16  xcom_port_recv(TU8 * pBuf, TU16 nBufLen)
{
    TU16 nRet = (TU16)UART_Read(g_hPort, pBuf, (TU32)nBufLen);

    if (nRet > 0) LOG_Frame("UART RX: ", pBuf, nRet);

    return nRet;
}

void  xcom_port_close(void)
{
    UART_Close(g_hPort);
}
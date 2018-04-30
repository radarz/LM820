#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

static TBool    g_bLogToScreen = 0;
static TU8      g_nLogLevel = LOG_LEVEL_ERROR;
static const char * g_szLogFileName = NULL;

#define LOG_BUF_SIZE            (5120)
static char     g_cLogBuf[LOG_BUF_SIZE+1] = {0};
static FILE   * g_fpLog = NULL;

////////////////////////////////////////////////////////////////////////////////
static void LOG_PrintArray(TU8 *pBuf, TU16 nLen)
{
    TU16 i = 0;
    
    if (g_bLogToScreen)
    {
        for (i=0; i<nLen; i++)
        {
            fprintf(stdout, "%c", pBuf[i]);
        }
    }
    
    if (g_szLogFileName && g_fpLog)
    {   
        for (i=0; i<nLen; i++)
        {
            fprintf(g_fpLog, "%c", pBuf[i]);
        }
    }
    
    fflush(g_fpLog);
}

////////////////////////////////////////////////////////////////////////////////
TBool LOG_Init(TBool bLogToScreen, const char * szFileName, TBool bCreateFile, TU8 nLogLevel)
{
    // create/clear file if needed
    if (szFileName)
    {
        if (bCreateFile)
        {
            g_fpLog = fopen(szFileName, "wt");
        }
        else
        {
            g_fpLog = fopen(szFileName, "a+");
        }

        if (!g_fpLog)
        {
            printf("open log file [%s] failed!\n", szFileName);
        }
    }

    // backup log file name
    g_szLogFileName = szFileName;

    // backup log to screen setting
    g_bLogToScreen = bLogToScreen;

    // backup log level
    g_nLogLevel = nLogLevel;

    return TTrue;
}

void  LOG_DeInit(void)
{
	if (g_fpLog != NULL) fclose(g_fpLog);
}

void  LOG_Print(TU8 nLogLevel, const char *szFmt, ...)
{
    va_list vList;
    
    if (nLogLevel > g_nLogLevel) return;   
    
    if (!g_szLogFileName && !g_bLogToScreen) return;

    va_start(vList, szFmt);
#ifdef WIN32   
    _vsnprintf(g_cLogBuf, LOG_BUF_SIZE, szFmt, vList);
#elif defined __linux__
    vsnprintf(g_cLogBuf, LOG_BUF_SIZE, szFmt, vList);
#endif
    
    LOG_PrintArray((TU8 *)g_cLogBuf, (TU16)strlen(g_cLogBuf));
}

void  LOG_PrintFrame(TU8 nLogLevel, const char *szPrefix, TU8 *pBuf, TU16 nLen)
{
    TU16 i = 0;
    char *p = g_cLogBuf;
    TU32 nPrefixLen = (TU32)strlen(szPrefix);
    TU16 nPrtLen = 0;

    if (nLogLevel > g_nLogLevel) return;
    
    if (!g_szLogFileName && !g_bLogToScreen) return;
    
    if (nPrefixLen >= LOG_BUF_SIZE)
    {
        strncpy(g_cLogBuf, szPrefix, LOG_BUF_SIZE - 1);
        g_cLogBuf[LOG_BUF_SIZE - 1] = '\0';
    }
    else
    {
        // reserve space for \n & \0!
        nPrtLen = (TU16)(LOG_BUF_SIZE - nPrefixLen - 1 - 1) / 3;    
        if (nPrtLen > nLen) nPrtLen = nLen;
        
        strcpy(g_cLogBuf, szPrefix);
        
        p = g_cLogBuf + nPrefixLen;
        
        for (i=0; i<nPrtLen; i++)
        {
            sprintf(p, "%02X ", pBuf[i]);
            p += 3;
        }

        sprintf(p, "\n");
    }

    LOG_PrintArray((TU8 *)g_cLogBuf, (TU16)strlen(g_cLogBuf));
}

void  LOG_PrintData(TU16 *pData, TU16 nLen, TU16 nFrames)
{
    TU16 i = 0;
 
    TFloat Delta = 3.1416 / 2 /nLen;  //   pi /2 
    
//    if (TFalse && g_bLogToScreen)
//    {
//        for (i=0; i<nLen; i++)
//        {
////            fprintf(stdout, "%f,%f,%d\n", pData(2*i)* cos(i*Delta),pData(2*i)*sin(i*Delta), nFrames);
//        }
//    }
//    
    if (g_szLogFileName && g_fpLog)
    {   
        for (i=0; i<nLen; i++)
        {
			if (pData[i]>200 && pData[i]<1500)
			{
			
	            fprintf(g_fpLog,"%f;%f;%d\n", pData[i] * cos(i*Delta), pData[i]*sin(i*Delta), nFrames * 5);
			
			}
        }
    }
    
    fflush(g_fpLog);

}
void  LOG_INFO_Print(const char *szFmt, ...)
{
    va_list vList;
    
    if (g_nLogLevel < LOG_LEVEL_INFO) return;   
    
    if (!g_szLogFileName && !g_bLogToScreen) return;
    
    va_start(vList, szFmt);
#ifdef WIN32   
    _vsnprintf(g_cLogBuf, LOG_BUF_SIZE, szFmt, vList);
#elif defined __linux__
    vsnprintf(g_cLogBuf, LOG_BUF_SIZE, szFmt, vList);
#endif
    
    LOG_PrintArray((TU8 *)g_cLogBuf, (TU16)strlen(g_cLogBuf));
}

void  LOG_TRACE_PrintFrame(const char *szPrefix, TU8 *pBuf, TU16 nLen)
{
    LOG_PrintFrame(LOG_LEVEL_TRACE, szPrefix, pBuf, nLen);
}

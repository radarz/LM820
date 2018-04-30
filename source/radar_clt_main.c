#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "radar_ops.h"
#include "display.h"
#include "util.h"

#define BRIGHTNESS_AUTO_CTRL        (0xFF)
#define DEPTH_SIZE_UNKNOWN          (0xFFFF)

static char           g_szPort[16] = "";                    // -p
static unsigned char  g_nDbgLevel = 2;                      // -L
static unsigned char  g_bLogToScreen = 0;                   // -t
static char         * g_szFileName = NULL;                  // -f
static unsigned char  g_nBrightness = BRIGHTNESS_AUTO_CTRL; // -b
static unsigned short g_nDepthSize = DEPTH_SIZE_UNKNOWN;    // -s
static float          g_fFovDeviation = 0;                  // -d

////////////////////////////////////////////////////////////////////////////////
static void PrintBrief(void)
{
    printf("***************************************************************\n");
    printf("***  Percipio DLL LinearRadar Demo UI (v1.0)                ***\n");
    printf("***                                                         ***\n");
    printf("***                                Percipio Technology Ltd. ***\n");
    printf("***                                 http://www.percipio.xyz ***\n");
    printf("***************************************************************\n\n");
}

static void PrintUsage(void)
{
    printf("\n");
    printf("Usage: radar_clt [-x param] ...\n");
    printf("   [-x param] could be:\n");
    printf("    -p port_num    : UART device name or COM port number\n");
    printf("    -L log_level   : LOG level, default 2\n");
    printf("    -t             : print log to screen\n");
    printf("    -f file        : print log to file\n");
    printf("    -b brightness  : set brightness of the laser, default auto controlled by the device\n");
    printf("    -s depth_size  : set depth size. default max size of device\n");
    printf("    -d deviation   : set the optical axis deviation, default 0\n");
    printf("\n");
}

static int ParseArgs(int argc, char *argv[])
{
    int     i = 1;

    if (argc < 2)
    {
        return -1;
    }

    while (i < argc)
    {
        if (strcmp(argv[i], "-p") == 0)
        {
            if ((++i) >= argc) return -1;
            if (argv[i][0] == '/')
            {
                strncpy(g_szPort, argv[i], 16 - 1); // "/dev/ttyS0"
            }
            else
            {
                unsigned char nComNum = (unsigned char)atoi(argv[i]);
                sprintf(g_szPort, "\\\\.\\COM%d", nComNum); // "\\.\COM1" - "\\.\COM255"
            }
        }
        else if (strcmp(argv[i], "-L") == 0)
        {
            if ((++i) >= argc) return -1;
            g_nDbgLevel = (unsigned char)atoi(argv[i]);
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
            g_bLogToScreen = 1;
        }
        else if (strcmp(argv[i], "-f") == 0)
        {
            if ((++i) >= argc) return -1;
            g_szFileName = argv[i];
        }
        else if (strcmp(argv[i], "-b") == 0)
        {
            if ((++i) >= argc) return -1;
            g_nBrightness = (unsigned char)atoi(argv[i]);
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            if ((++i) >= argc) return -1;
            g_nDepthSize = (unsigned short)atoi(argv[i]);
        }
        else if (strcmp(argv[i], "-d") == 0)
        {
            if ((++i) >= argc) return -1;
            g_fFovDeviation = (float)atof(argv[i]);
        }
        else
        {
            printf("Undefined parameter [%s]!\n", argv[i]);
            return -1;
        }

        i++;
    }

    return 0;
}

static int CheckArgs(void)
{
    if (strcmp(g_szPort, "") == 0)
    {
        printf("UART port not defined!\n");
        return -1;
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Test State Machine

enum {
    TEST_STATE_INIT = 0,
    TEST_STATE_CONT,
    TEST_STATE_TRIG,
    TEST_STATE_SAVE,
    TEST_STATE_EXIT,
};

#define DAT_LEN_FOR_DBGIMG_READ (512)
#define FRM_COUNT_FOR_FPS_STAT  (10)
#define MAX_TIME_FOR_UPDATE_FPS (2000)
#define MAX_DBG_IMG_SIZE        (1280*1024)
#define DEPTH_WINDOW_NAME       ("Percipio Depth")
#define DBG_IMG_WINDOW_NAME     ("RAW Image for Debug")

static TU8   g_nTestState = TEST_STATE_INIT;
static TU32  g_nTestEvent = DISPLAY_EVENT_NOEVENT;

static TU16  g_nFov = 0;
static TBool g_bExit = TFalse;
static TU16  g_nDbgImgWidth = 0;
static TU16  g_nDbgImgHeight = 0;
static TU32  g_nDbgImgOffset = 0;
static char  g_szDbgImgName[30];
static char  g_cDbgImgBuf[MAX_DBG_IMG_SIZE];

static TU32  g_nFrmNumTotal = 0;
static TU32  g_nFrmNumForFps = 0;
static TU32  g_nStartTimeForFps = 0;

static TBool SaveBuf(char *pFileName, TU8 *pBuf, TU32 nBufSize)
{
    FILE * fp = NULL;

    if (!pFileName || !pBuf) return TFalse;

    fp = fopen(pFileName, "wb");
    
    if (!fp)
    {
        LOG("SaveBuf: fopen failed\n");
        return TFalse;
    }
    
    if (fwrite(pBuf, 1, nBufSize, fp) != nBufSize)
    {
        LOG("SaveBuf: fwrite failed !\n");
        fclose(fp);
        return TFalse;
    }
    
    fclose(fp);
    return TTrue;
}


static TU8 DepthTest_OnInit(void)
{
    TDevInfo tDevInfo;

    if (radar_open(g_szPort) < 0)
    {
        LOG("radar_open failed!\n");
        g_bExit = TTrue;
        return TEST_STATE_EXIT;
    }

    if (radar_get_info(&tDevInfo) < 0)
    {
        LOG("radar_get_info failed!\n");
        goto error;
    }
    else
    {
        LOG("Product Name: %s\n", tDevInfo.Name);
        LOG("Product Version: %d.%d\n", tDevInfo.nMajorVer, tDevInfo.nMinorVer);
        LOG("Serial Number: %s\n", tDevInfo.SerialNum);
    }

    if (radar_get_fov(&g_nFov) < 0)
    {
        LOG("radar_get_fov failed!\n");
        goto error;
    }
    else
    {
        LOG("FOV: %f Degree\n", (float)(g_nFov / 10.0));
    }

    if (g_nBrightness != BRIGHTNESS_AUTO_CTRL)
    {
        if (radar_set_ld(g_nBrightness) < 0)
        {
            LOG("radar_set_ld failed!\n");
            goto error;
        }
    }

    if (g_nDepthSize == DEPTH_SIZE_UNKNOWN)
    {
        if (radar_get_max_res(&g_nDepthSize) < 0)
        {
            LOG("radar_get_max_res failed!\n");
            goto error;
        }
        else
        {
            LOG("Use max resolution: %d Points\n", g_nDepthSize);
        }
    }

    if (radar_set_res(g_nDepthSize) < 0)
    {
        LOG("radar_set_res failed!\n");
        goto error;
    }

    // continuous mode by default
    if (radar_set_mode(RADAR_MODE_CONT) < 0)
    {
        LOG("radar_set_mode failed!\n");
        goto error;
    }

    if (radar_cont_start() < 0)
    {
        LOG("radar_cont_start failed!\n");
        goto error;
    }

    g_nFrmNumForFps = 0;
    g_nStartTimeForFps = TIMER_GetNow();

    display_Init();
    display_SetFovDeviation(DEPTH_WINDOW_NAME, g_fFovDeviation);

    return TEST_STATE_CONT;

error:
    return TEST_STATE_EXIT;
}

static TU8 DepthTest_OnCont(void)
{
    TU32  nCurEvent;
    TU8   nNextState;
    TU32  nTimestamp;
    TU16 *pDepth = NULL;
    TU16  nDepthSize;
    time_t nSeconds = time(NULL);
    struct tm *pTm = localtime(&nSeconds);

    nCurEvent = g_nTestEvent;
    g_nTestEvent = DISPLAY_EVENT_NOEVENT;

    switch (nCurEvent)
    {
    case DISPLAY_EVENT_QUIT:
        nNextState = TEST_STATE_EXIT;
        break;
    case DISPLAY_EVENT_SAVE:
        if ((radar_take_dbg_img(&g_nDbgImgWidth, &g_nDbgImgHeight) == RADAR_ERROR_SUCCESS)
         && (g_nDbgImgWidth*g_nDbgImgHeight <= MAX_DBG_IMG_SIZE))
        {
            _snprintf(g_szDbgImgName, 30, "dbg_img_%04d%02d%02d%02d%02d%02d.raw", 
                     pTm->tm_year+1900, pTm->tm_mon+1, pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
			//sprintf(g_szDbgImgName , "db_img_save.raw");

            g_nDbgImgOffset = 0;
            display_SetDebugImageInfo(DEPTH_WINDOW_NAME, 0, g_szDbgImgName);

            LOG("Debug image captured! width=%d, height=%d\n", g_nDbgImgWidth, g_nDbgImgHeight);
            nNextState = TEST_STATE_SAVE;
        }
        else 
        {
            LOG("radar_take_dbg_img failed!\n");
            nNextState = TEST_STATE_INIT;
        }
        break;    
    case DISPLAY_EVENT_TRIG:
        if (radar_set_mode(RADAR_MODE_TRIG) < 0)
        {
            LOG("radar_set_mode failed!\n");
            nNextState = TEST_STATE_INIT;
        }
        else 
        {
            LOG("Set device to TRIG mode\n");
            nNextState = TEST_STATE_TRIG;
        }
        break;
    default:
        if (radar_cont_get_depth(300, &nTimestamp, &pDepth, &nDepthSize) == RADAR_ERROR_SUCCESS)
        {
            LOG("Depth RCVD. timestamp: %u, depth_size: %d\n", nTimestamp, nDepthSize);
            display_SetDepthImage(DEPTH_WINDOW_NAME, pDepth, nDepthSize, (float)(g_nFov/10.0));
//log here
//
			LOG_Data(pDepth, nDepthSize, g_nFrmNumTotal);

            g_nFrmNumTotal++;
            g_nFrmNumForFps++;
        }
        else
        {
            LOG("radar_cont_get_depth failed!\n");
        }

        nNextState = TEST_STATE_CONT;
        break;
    }

    return nNextState;
}

static TU8 DepthTest_OnTrig(void)
{
    TU32  nCurEvent;
    TU8   nNextState;
    TU32  nTimestamp;
    TU16 *pDepth = NULL;
    TU16  nDepthSize;
    time_t nSeconds = time(NULL);
    struct tm *pTm = localtime(&nSeconds);

    nCurEvent = g_nTestEvent;
    g_nTestEvent = DISPLAY_EVENT_NOEVENT;

    switch (nCurEvent)
    {
    case DISPLAY_EVENT_QUIT:
        nNextState = TEST_STATE_EXIT;
        break;
    case DISPLAY_EVENT_SAVE:
        if ((radar_take_dbg_img(&g_nDbgImgWidth, &g_nDbgImgHeight) == RADAR_ERROR_SUCCESS)
         && (g_nDbgImgWidth*g_nDbgImgHeight <= MAX_DBG_IMG_SIZE))
        {
            _snprintf(g_szDbgImgName, 30, "dbg_img_%04d%02d%02d%02d%02d%02d.raw", 
                     pTm->tm_year+1900, pTm->tm_mon+1, pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec);

            g_nDbgImgOffset = 0;
            display_SetDebugImageInfo(DEPTH_WINDOW_NAME, 0, g_szDbgImgName);

            LOG("Debug image captured! width=%d, height=%d\n", g_nDbgImgWidth, g_nDbgImgHeight);
            nNextState = TEST_STATE_SAVE;
        }
        else
        {
            LOG("radar_take_dbg_img failed!\n");
            nNextState = TEST_STATE_INIT;
        }
        break;
    case DISPLAY_EVENT_CONT:
        if (radar_set_mode(RADAR_MODE_CONT) == RADAR_ERROR_SUCCESS)
        {
            if (radar_cont_start() < 0)
            {
                LOG("radar_cont_start failed!\n");
                nNextState = TEST_STATE_INIT;
            }
            else
            {
                LOG("Set device to CONT mode\n");
                nNextState = TEST_STATE_CONT;
            }
        }
        else
        {
            LOG("radar_set_mode failed!\n");
            nNextState = TEST_STATE_INIT;
        }
        break;
    case DISPLAY_EVENT_TRIG:
        if (radar_trig_get_depth(&nTimestamp, &pDepth, &nDepthSize) == RADAR_ERROR_SUCCESS)
        {
            LOG("Depth RCVD. timestamp: %u, depth_size: %d\n", nTimestamp, nDepthSize);
            display_SetDepthImage(DEPTH_WINDOW_NAME, pDepth, nDepthSize, (float)(g_nFov/10.0));

            g_nFrmNumTotal++;
            g_nFrmNumForFps++;            
        }
        else
        {
            LOG("radar_trig_get_depth failed!\n");
        }

        nNextState = TEST_STATE_TRIG;
        break;
    default:
        nNextState = TEST_STATE_TRIG;
        break;
    }

    return nNextState;
}

static TU8 DepthTest_OnSave(void)
{
    TU32  nCurEvent;
    TU8   nNextState;
    TU16  nLen;

    nCurEvent = g_nTestEvent;
    g_nTestEvent = DISPLAY_EVENT_NOEVENT;

    switch (nCurEvent)
    {
    case DISPLAY_EVENT_QUIT:
        nNextState = TEST_STATE_EXIT;
        break;  
    default:
        nLen = DAT_LEN_FOR_DBGIMG_READ;

        if (radar_read_dbg_img(g_nDbgImgOffset, (TU8 *)&g_cDbgImgBuf[g_nDbgImgOffset], &nLen) == RADAR_ERROR_SUCCESS)
        {
            if (nLen < DAT_LEN_FOR_DBGIMG_READ)
            {
                if (SaveBuf(g_szDbgImgName, (TU8 *)g_cDbgImgBuf, g_nDbgImgWidth*g_nDbgImgHeight))
                {
                    LOG("Debug Image done! Filename: %s\n", g_szDbgImgName);

                    display_SetDebugImageInfo(DEPTH_WINDOW_NAME, 100, g_szDbgImgName);
                    display_SetRawImage(DBG_IMG_WINDOW_NAME, (TU8 *)g_cDbgImgBuf, g_nDbgImgWidth, g_nDbgImgHeight);
                    display_ShowImage(0);
                    
                    nNextState = TEST_STATE_INIT;
                }
                else
                {
                    LOG("SaveBuf failed!\n");
                    nNextState = TEST_STATE_SAVE;
                }
            }
            else
            {
                g_nDbgImgOffset += DAT_LEN_FOR_DBGIMG_READ;
            
                display_SetDebugImageInfo(DEPTH_WINDOW_NAME, (TU8)(100*g_nDbgImgOffset/(g_nDbgImgWidth*g_nDbgImgHeight)), g_szDbgImgName);
            
                LOG("radar_read_dbg_img: offset=0x%X, len=%d\n", g_nDbgImgOffset, nLen);
                nNextState = TEST_STATE_SAVE;
            }
        }
        else
        {
            LOG("radar_read_dbg_img failed!\n");
            nNextState = TEST_STATE_SAVE;
        }
        break;
    }

    return nNextState;
}

static TU8 DepthTest_OnExit(void)
{
    radar_close();
    g_bExit = TTrue;

    return TEST_STATE_EXIT;
}

static TU32 DepthTest_GetEvent(void)
{
    if (g_nTestEvent != DISPLAY_EVENT_NOEVENT)
    {
        return g_nTestEvent;
    }

    return display_GetEvent();
}

static void DepthTest_Init(void)
{
    g_nTestState = TEST_STATE_INIT;
    g_nTestEvent = DISPLAY_EVENT_NOEVENT;

    g_nFov = 0;
    g_bExit = TFalse;
    g_nDbgImgWidth = 0;
    g_nDbgImgHeight = 0;
    g_nDbgImgOffset = 0;
    memset(g_szDbgImgName, 0, 30);
    memset(g_cDbgImgBuf, 0, MAX_DBG_IMG_SIZE);
    
    g_nFrmNumTotal = 0;
    g_nFrmNumForFps = 0;
    g_nStartTimeForFps = 0;
}

static void DepthTest_Fsm(void)
{
    TU32 nElapse;
    static float fFps = 0;

    switch (g_nTestState)
    {
    case TEST_STATE_INIT: g_nTestState = DepthTest_OnInit(); break;
    case TEST_STATE_CONT: g_nTestState = DepthTest_OnCont(); break;
    case TEST_STATE_TRIG: g_nTestState = DepthTest_OnTrig(); break;
    case TEST_STATE_SAVE: g_nTestState = DepthTest_OnSave(); break;
    case TEST_STATE_EXIT: g_nTestState = DepthTest_OnExit(); break;
    default:
        break;
    }

    if (g_bExit) return;

    nElapse = TIMER_GetNow() - g_nStartTimeForFps;
    if (g_nFrmNumForFps == FRM_COUNT_FOR_FPS_STAT || nElapse > MAX_TIME_FOR_UPDATE_FPS)
    {
        if (nElapse != 0)
        {
            fFps = (float)(g_nFrmNumForFps * 1000.0 / nElapse);
        }

        g_nFrmNumForFps = 0;
        g_nStartTimeForFps = TIMER_GetNow();
    }
    
    display_SetStatInfo(DEPTH_WINDOW_NAME, g_nFrmNumTotal, fFps);
    display_ShowImage(1);

    g_nTestEvent = DepthTest_GetEvent();
}

////////////////////////////////////////////////////////////////////////////////
void radar_clt_exit(void)
{
    g_nTestEvent = DISPLAY_EVENT_QUIT;
}

int radar_clt_main(int argc, char *argv[])
{
    PrintBrief();

    if (ParseArgs(argc, argv) < 0)
    {
        PrintUsage();
        return -1;
    }

    if (CheckArgs() < 0) return -1;

    LOG_Init(g_bLogToScreen, g_szFileName, 1, g_nDbgLevel);

    DepthTest_Init();

    while (!g_bExit)
    {
        DepthTest_Fsm();
    }

    return 0;
}

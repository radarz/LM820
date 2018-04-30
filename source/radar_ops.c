#include "radar_ops.h"
#include "msg.h"
#include "xcom.h"
#include "xcom_port.h"
#include "util.h"
#include <string.h>

#define IO_DEF_TIMEOUT         (1000)
#define TAKE_DBG_IMG_TIMEOUT   (10000)
#define MAX_IO_TRY_NUM         (3)

// Variables for response from the radar
static TBool g_bRspReady = TFalse;
static TU8   g_nCurId = 0;
static TU8   g_nCurCmd = 0;
static TU8   g_cCurBuf[MAX_PAYLOAD_LEN] = {0};
static TU16  g_nCurLen = 0;

// Variables for Depth reported by the radar
static TU8   g_cCurDepthBuf[MAX_PAYLOAD_LEN] = {0};
static TU16  g_nCurDepthLen = 0;

// Variables for device failed reported by the radar
static TBool g_bDevFailed = TFalse;

////////////////////////////////////////////////////////////////////////////////
static TU8 GetMsgIdToSend(void)
{
    static TU32 g_nTxCount = 0;

    g_nTxCount++;
    return (TU8)g_nTxCount;
}

static void clt_xcom_rcvd_cb(TU8 nId, TU8 nCmd, TU8 *pBuf, TU16 nLen)
{
    LOG("MSG RCVD: Id=0x%02X, Cmd=0x%02X, Len=%d\n", nId, nCmd, nLen);

    if ((nCmd & CMD_MASK_REQ_RSP) == CMD_BIT_RSP)
    {
        nCmd &= ~ CMD_MASK_REQ_RSP;

        // Response message received
        if ((nId == g_nCurId) && (nCmd == g_nCurCmd))
        {
            // ID and CMD matched: save the message
            g_nCurLen = nLen;
            memcpy(g_cCurBuf, pBuf, nLen);

            // Set the response message ready flag
            g_bRspReady = TTrue;
        }

        // Just discard the response message if ID or CMD not matched
    }
    else
    {
        // REQ message received: this is an active reported message from the radar
        nCmd &= ~ CMD_MASK_REQ_RSP;

        // Depth received (in continuous mode)
        if (nCmd == RADAR_CMD_REPORT_DEPTH)
        {
            // Update the depth buffer. Old depth data in the buffer may be discarded!
            memcpy(g_cCurDepthBuf, pBuf, nLen);

            // The depth buffer becomes valid if the length is not 0
            g_nCurDepthLen = nLen;
        }
        else if (nCmd == RADAR_CMD_REPORT_ERROR)
        {
            // Set the device failed flag, to indicate the radar is in fault
            g_bDevFailed = TTrue;
        }
    }
}

static int xcom_io_sync(TU32 nTimeout)
{
    Timer_t tmIO;

    // Notify the caller immediately if the radar is in fault
    if (g_bDevFailed) return RADAR_ERROR_DEVICE_FAILED;

    // Try to send the REQ message with a new ID
    g_nCurId = GetMsgIdToSend();

    if (!xcom_send(g_nCurId, (TU8)(g_nCurCmd | CMD_BIT_REQ), g_cCurBuf, g_nCurLen))
    {
        LOG("xcom_send failed!\n");
        return RADAR_ERROR_IMPLEMENTATION;
    }

    LOG("MSG SENT: Id=0x%02X, Cmd=0x%02X, Len=%d\n", g_nCurId, (TU8)(g_nCurCmd | CMD_BIT_REQ), g_nCurLen);

    // Waiting for the RSP message
    g_bRspReady = TFalse;

    TIMER_SetDelay_ms(&tmIO, nTimeout);
    TIMER_Start(&tmIO);

    while (!TIMER_Elapsed(&tmIO))
    {
        xcom_fsm();

        // Notify the caller immediately if the radar is in fault
        if (g_bDevFailed)
        {
            return RADAR_ERROR_DEVICE_FAILED;
        }

        if (g_bRspReady)
        {
            // RSP message received, return to parse the message
            return RADAR_ERROR_SUCCESS;
        }

        UTIL_Sleep(1);
    }

    // Wait RSP message timeout!
    return RADAR_ERROR_ACCESS_TIMEOUT;
}

////////////////////////////////////////////////////////////////////////////////
int radar_init(void)
{
    int nRet;

    g_nCurCmd = RADAR_CMD_INIT;
    g_nCurLen = 0;

    nRet = xcom_io_sync(IO_DEF_TIMEOUT);

    return nRet;
}

int radar_get_info(TDevInfo * pDevInfo)
{
    int nRet;
    TU8 * p = &g_cCurBuf[0];

    if (!pDevInfo)
    {
        return RADAR_ERROR_WRONG_PARAM;
    }

    g_nCurCmd = RADAR_CMD_GET_INFO;
    g_nCurLen = 0;

    nRet = xcom_io_sync(IO_DEF_TIMEOUT);

    if (nRet == RADAR_ERROR_SUCCESS)
    {
        pDevInfo->nMajorVer = *p++;
        pDevInfo->nMinorVer = *p++;
        memcpy(pDevInfo->SerialNum, p, 32);
        p += 32;
        memcpy(pDevInfo->Name, p, 64);
        p += 64;
    }

    return nRet;
}

int radar_set_ld(TU8 nPower)
{
    int nRet;

    g_nCurCmd = RADAR_CMD_SET_LD;
    g_cCurBuf[0] = nPower;
    g_nCurLen = 1;

    nRet = xcom_io_sync(IO_DEF_TIMEOUT);

    if (nRet == RADAR_ERROR_SUCCESS)
    {
        if (nPower != g_cCurBuf[0])
        {
            return RADAR_ERROR_WRONG_PARAM;
        }
    }

    return nRet;
}

int radar_set_mode(TU8 nMode)
{
    int nRet;

    g_nCurCmd = RADAR_CMD_SET_MODE;
    g_cCurBuf[0] = nMode;
    g_nCurLen = 1;

    nRet = xcom_io_sync(IO_DEF_TIMEOUT);

    if (nRet == RADAR_ERROR_SUCCESS)
    {
        if (nMode != g_cCurBuf[0])
        {
            return RADAR_ERROR_WRONG_PARAM;
        }
    }

    return nRet;
}

int radar_set_res(TU16 nDepthSize)
{
    int nRet;

    g_nCurCmd = RADAR_CMD_SET_RES;
    g_cCurBuf[0] = (TU8)((nDepthSize     ) & 0xFF);
    g_cCurBuf[1] = (TU8)((nDepthSize >> 8) & 0xFF);
    g_nCurLen = 2;

    nRet = xcom_io_sync(IO_DEF_TIMEOUT);

    if (nRet == RADAR_ERROR_SUCCESS)
    {
        if (nDepthSize != UTIL_DEC_TU16_LSBF(&g_cCurBuf[0]))
        {
            return RADAR_ERROR_WRONG_PARAM;
        }
    }

    return nRet;
}

int radar_get_fov(TU16 * pFov)
{
    int nRet;

    if (!pFov)
    {
        return RADAR_ERROR_WRONG_PARAM;
    }

    g_nCurCmd = RADAR_CMD_GET_FOV;
    g_nCurLen = 0;

    nRet = xcom_io_sync(IO_DEF_TIMEOUT);

    if (nRet == RADAR_ERROR_SUCCESS)
    {
        *pFov = UTIL_DEC_TU16_LSBF(&g_cCurBuf[0]);
    }

    return nRet;
}

int radar_get_max_res(TU16 * pMaxRes)
{
    int nRet;

    if (!pMaxRes)
    {
        return RADAR_ERROR_WRONG_PARAM;
    }

    g_nCurCmd = RADAR_CMD_GET_MAX_RES;
    g_nCurLen = 0;

    nRet = xcom_io_sync(IO_DEF_TIMEOUT);

    if (nRet == RADAR_ERROR_SUCCESS)
    {
        *pMaxRes = UTIL_DEC_TU16_LSBF(&g_cCurBuf[0]);
    }

    return nRet;
}

int radar_trig_get_depth(TU32 *pTimestamp, TU16 ** ppDepth, TU16 * pDepthSize)
{
    int nRet;

    if (!pTimestamp || !ppDepth || !pDepthSize)
    {
        return RADAR_ERROR_WRONG_PARAM;
    }

    g_nCurCmd = RADAR_CMD_TRIG_DEPTH;
    g_nCurLen = 0;

    nRet = xcom_io_sync(IO_DEF_TIMEOUT);

    if (nRet == RADAR_ERROR_SUCCESS)
    {
        if (g_nCurLen == 0)
        {
            return RADAR_ERROR_DEPTH_UNAVAILABLE;
        }

        *pTimestamp = UTIL_DEC_TU32_LSBF(&g_cCurBuf[0]);
        *ppDepth = (TU16 *)&g_cCurBuf[4];
        *pDepthSize = (g_nCurLen-4)/2;
    }

    return nRet;
}

int radar_cont_start(void)
{
    int nRet;

    g_nCurCmd = RADAR_CMD_START_DEPTH;
    g_nCurLen = 0;

    nRet = xcom_io_sync(IO_DEF_TIMEOUT);

    return nRet;
}

int radar_cont_stop(void)
{
    int nRet;

    g_nCurCmd = RADAR_CMD_STOP_DEPTH;
    g_nCurLen = 0;

    nRet = xcom_io_sync(IO_DEF_TIMEOUT);

    return nRet;
}

int radar_cont_get_depth(TU32 nTimeout, TU32 *pTimestamp, TU16 ** ppDepth, TU16 * pDepthSize)
{
    Timer_t tmIO;

    if (!pTimestamp || !ppDepth || !pDepthSize)
    {
        return RADAR_ERROR_WRONG_PARAM;
    }

    TIMER_SetDelay_ms(&tmIO, nTimeout);
    TIMER_Start(&tmIO);

    do
    {
        xcom_fsm();

        if (g_nCurDepthLen > 0)
        {
            *pTimestamp = UTIL_DEC_TU32_LSBF(&g_cCurDepthBuf[0]);
            *ppDepth = (TU16 *)&g_cCurDepthBuf[4];
            *pDepthSize = (g_nCurDepthLen-4)/2;

            g_nCurDepthLen = 0;

            return RADAR_ERROR_SUCCESS;
        }
    } while (!TIMER_Elapsed(&tmIO));

    return RADAR_ERROR_DEPTH_UNAVAILABLE;
}

int radar_take_dbg_img(TU16 *pWidth, TU16 *pHeight)
{
    int nRet;

    if (!pWidth || !pHeight)
    {
        return RADAR_ERROR_WRONG_PARAM;
    }

    g_nCurCmd = RADAR_CMD_TAKE_DBG_IMG;
    g_nCurLen = 0;

    nRet = xcom_io_sync(TAKE_DBG_IMG_TIMEOUT);

    if (nRet == RADAR_ERROR_SUCCESS)
    {
        *pWidth  = UTIL_DEC_TU16_LSBF(&g_cCurBuf[0]);
        *pHeight = UTIL_DEC_TU16_LSBF(&g_cCurBuf[2]);
    }

    return nRet;
}

int radar_read_dbg_img(TU32 nOffset, TU8 * pDat, TU16 * pDatLen)
{
    int nRet;
    TU8 *p = &g_cCurBuf[0];

    if (!pDat || !pDatLen || (*pDatLen == 0))
    {
        return RADAR_ERROR_WRONG_PARAM;
    }

    *p++ = (TU8)((nOffset      ) & 0xFF);
    *p++ = (TU8)((nOffset >>  8) & 0xFF);
    *p++ = (TU8)((nOffset >> 16) & 0xFF);
    *p++ = (TU8)((nOffset >> 24) & 0xFF);
    *p++ = (TU8)(((*pDatLen)   ) & 0xFF);
    *p++ = (TU8)(((*pDatLen)>>8) & 0xFF);

    g_nCurCmd = RADAR_CMD_READ_DBG_IMG;
    g_nCurLen = (TU16)(p - &g_cCurBuf[0]);

    nRet = xcom_io_sync(IO_DEF_TIMEOUT);

    if (nRet == RADAR_ERROR_SUCCESS)
    {
        if (g_nCurLen > (*pDatLen)) g_nCurLen = *pDatLen;

        memcpy(pDat, g_cCurBuf, g_nCurLen);
        *pDatLen = g_nCurLen;
    }

    return nRet;
}

////////////////////////////////////////////////////////////////////////////////
int radar_open(char * szPort)
{
    TU8 i = 0;

    // Try to connect the device
    for (i=0; i<MAX_IO_TRY_NUM; i++)
    {
        if ((xcom_port_open((TU8 *)szPort) == TTrue)
         && (xcom_init(clt_xcom_rcvd_cb) == TTrue)
         && (radar_init() == RADAR_ERROR_SUCCESS))
        {
            break;
        }
    }

    if (i == MAX_IO_TRY_NUM)
    {
        return RADAR_ERROR_PORT_FAILED;
    }

    return RADAR_ERROR_SUCCESS;
}

int radar_close(void)
{
    TU8 i = 0;

    // Try to stop the continous depth and turn off the laser
    for (i=0; i<MAX_IO_TRY_NUM; i++)
    {
        if ((radar_cont_stop() == RADAR_ERROR_SUCCESS)
         && (radar_set_ld(0) == RADAR_ERROR_SUCCESS))
        {
            break;
        }
    }

    if (i == MAX_IO_TRY_NUM)
    {
        return RADAR_ERROR_PORT_FAILED;
    }

    // close the port
    xcom_port_close();

    return RADAR_ERROR_SUCCESS;
}
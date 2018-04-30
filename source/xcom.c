#include "xcom.h"
#include "util.h"
#include "xcom_port.h"
#include <string.h>

// Config the max payload len 
#define CFG_MAX_PAYLOAD_LEN     (2100)

////////////////////////////////////////////////////////////////////////////////
// Message: SYNC | VER | ID | CMD | LEN LSB | LEN MSB | PAYLOAD[LENGTH] | CRC8
#define MSG_OFFSET_SYNC     (0)
#define MSG_OFFSET_VER      (1)
#define MSG_OFFSET_ID       (2)
#define MSG_OFFSET_CMD      (3)
#define MSG_OFFSET_LEN      (4)
#define MSG_OFFSET_PAYLOAD  (6)

#define MSG_HEADER_LEN      (MSG_OFFSET_PAYLOAD)
#define MSG_CRC_LEN         (1)

#define MSG_CHAR_SYNC       (0xA5)
#define MSG_CHAR_VER        (0x04)

#define MAX_PAYLOAD_LEN     (CFG_MAX_PAYLOAD_LEN)
#define MAX_MSG_LEN         (MSG_HEADER_LEN + MAX_PAYLOAD_LEN + MSG_CRC_LEN)

////////////////////////////////////////////////////////////////////////////////
static XCOM_RECV_CB g_xcom_recv_msg_cb = NULL;

static TBool g_bTxBusy = TFalse;
static TU16  g_nCurTx = 0;
static TU16  g_nCurRx = 0;

static TU8   g_cTxBuf[MAX_MSG_LEN] = {0};
static TU8   g_cRxBuf[MAX_MSG_LEN] = {0};

////////////////////////////////////////////////////////////////////////////////
static TBool CheckHeader(TU8 *pBuf, TU16 nLen)
{
    TU16 nLenInHeader = 0;

    if (!pBuf || nLen < MSG_HEADER_LEN) return TFalse;
    
    // SYNC failed or VER not expected
    if (pBuf[MSG_OFFSET_SYNC] != MSG_CHAR_SYNC 
     || pBuf[MSG_OFFSET_VER] != MSG_CHAR_VER) 
    {
        return TFalse;
    }

    // LEN larger than max
    nLenInHeader = UTIL_DEC_TU16_LSBF(&pBuf[MSG_OFFSET_LEN]);
    if (nLenInHeader > MAX_PAYLOAD_LEN) return TFalse;

    // Now Correct header!
    return TTrue;
}

static TBool CheckCrc8(TU8 *pBuf, TU16 nLen)
{
    if (!pBuf || nLen < MSG_HEADER_LEN+MSG_CRC_LEN) return TFalse;

    return (TBool)(pBuf[nLen-1] == CRC_CalCrc8(pBuf, (TU16)(nLen-1), 0));
}

static void xcom_rx_fsm(void)
{
    TU16 nRx = 0;
    TU16 nLenInHeader = 0;
    TU16 nRxExpected  = 0;
    
    if (g_nCurRx < MSG_HEADER_LEN)
    {
        // Receive the header
        nRx = xcom_port_recv(&g_cRxBuf[g_nCurRx], (TU16)(MSG_HEADER_LEN-g_nCurRx));
        
        g_nCurRx += nRx;
        
        if (g_nCurRx == MSG_HEADER_LEN)
        {
            if (!CheckHeader(g_cRxBuf, MSG_HEADER_LEN))
            {
                // If the header is wrong, remove the first byte, then check again
                g_nCurRx--;
                memmove(g_cRxBuf, g_cRxBuf+1, g_nCurRx);
            }
        }
    }
    else
    {
        nLenInHeader = UTIL_DEC_TU16_LSBF(&g_cRxBuf[MSG_OFFSET_LEN]);
        nRxExpected  = (TU16)(nLenInHeader + MSG_HEADER_LEN + MSG_CRC_LEN);
        
        // Receive all rest bytes of the message, including PAYLOAD and CRC8
        nRx = xcom_port_recv(&g_cRxBuf[g_nCurRx], (TU16)(nRxExpected-g_nCurRx));
        
        g_nCurRx += nRx;
        
        // The whole message has been received
        if (g_nCurRx == nRxExpected)
        {
            if (CheckCrc8(g_cRxBuf, g_nCurRx) && g_xcom_recv_msg_cb)  ////==============g_cRxBuf??
            {
                // Callback to notifier the caller
                g_xcom_recv_msg_cb(g_cRxBuf[MSG_OFFSET_ID], 
                                   g_cRxBuf[MSG_OFFSET_CMD], 
                                   &g_cRxBuf[MSG_OFFSET_PAYLOAD], 
                                   nLenInHeader);
            }
            else
            {
                // Just discard the message, if CRC not correct!
            }
            
            // Clear the receive buffer, for the next frame
            g_nCurRx = 0;
        }
    }
}

static void xcom_tx_fsm(void)
{
    TU16 nLenInHeader = 0;
    TU16 nTxExpected  = 0;
    TU16 nTx = 0;

    // No message in the buffer to be sent
    if (!g_bTxBusy) return;
    
    nLenInHeader = UTIL_DEC_TU16_LSBF(&g_cTxBuf[MSG_OFFSET_LEN]);
    nTxExpected  = (TU16)(nLenInHeader + MSG_HEADER_LEN + MSG_CRC_LEN);
    
    // Try to send the message
    nTx = xcom_port_send(&g_cTxBuf[g_nCurTx], (TU16)(nTxExpected-g_nCurTx));

    g_nCurTx += nTx;

    // The whole message has been sent
    if (g_nCurTx == nTxExpected)
    {
        // Clear the send buffer, for the next frame
        g_nCurTx  = 0;

        // Reset the TX buffer flag 
        g_bTxBusy = TFalse;
    }
}

////////////////////////////////////////////////////////////////////////////////
TBool xcom_init(XCOM_RECV_CB pCbFunc)
{
    g_xcom_recv_msg_cb = pCbFunc;
    
    g_bTxBusy = TFalse;
    g_nCurTx = 0;
    g_nCurRx = 0;

    memset(g_cTxBuf, 0, MAX_MSG_LEN);
    memset(g_cRxBuf, 0, MAX_MSG_LEN);

    return TTrue;
}

TBool xcom_send(TU8 nId, TU8 nCmd, TU8 *pBuf, TU16 nLen)
{
    if (nLen > MAX_PAYLOAD_LEN) return TFalse;
    
    // Send failed if the TX is ongoing
    if (g_bTxBusy) return TFalse;

    // Add the message header
    g_cTxBuf[MSG_OFFSET_SYNC]   = MSG_CHAR_SYNC;
    g_cTxBuf[MSG_OFFSET_VER]    = MSG_CHAR_VER;
    g_cTxBuf[MSG_OFFSET_ID]     = nId;
    g_cTxBuf[MSG_OFFSET_CMD]    = nCmd;
    g_cTxBuf[MSG_OFFSET_LEN]    = (TU8)((nLen     ) & 0xFF);
    g_cTxBuf[MSG_OFFSET_LEN+1]  = (TU8)((nLen >> 8) & 0xFF);
 
    // Copy the payload to the send buffer
    if (pBuf && nLen > 0)
    {
        memcpy(&g_cTxBuf[MSG_OFFSET_PAYLOAD], pBuf, nLen);
    }
    
    // Set CRC at the end of the message
    g_cTxBuf[MSG_HEADER_LEN+nLen]= CRC_CalCrc8(g_cTxBuf, (TU16)(MSG_HEADER_LEN+nLen), 0);

    // Set the TX buffer flag
    g_bTxBusy = TTrue;

    return TTrue;
}

void  xcom_fsm(void)
{
    xcom_tx_fsm();
    xcom_rx_fsm();
}
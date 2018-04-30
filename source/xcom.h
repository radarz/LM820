#ifndef __XCOM_H__
#define __XCOM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "hal.h"

typedef void (*XCOM_RECV_CB)(TU8 nId, TU8 nCmd, TU8 *pBuf, TU16 nLen);

TBool xcom_init(XCOM_RECV_CB pCbFunc);
TBool xcom_send(TU8 nId, TU8 nCmd, TU8 *pBuf, TU16 nLen);
void  xcom_fsm(void);

#ifdef __cplusplus
}
#endif

#endif // __XCOM_H__
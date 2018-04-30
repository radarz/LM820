#ifndef __XCOM_PORT_H__
#define __XCOM_PORT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "hal.h"

TBool xcom_port_open(const TU8 *szPort);
TU16  xcom_port_send(TU8 * pBuf, TU16 nLen);
TU16  xcom_port_recv(TU8 * pBuf, TU16 nBufLen);
void  xcom_port_close(void);

#ifdef __cplusplus
}
#endif

#endif // __XCOM_PORT_H__
#ifndef __MSG_H__
#define __MSG_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PAYLOAD_LEN         (2100)

#define RADAR_CMD_INIT          (0x00)
#define RADAR_CMD_GET_INFO      (0x01)
#define RADAR_CMD_SET_LD        (0x02)
#define RADAR_CMD_SET_MODE      (0x03)
#define RADAR_CMD_SET_RES       (0x04)
#define RADAR_CMD_GET_FOV       (0x05)
#define RADAR_CMD_GET_MAX_RES   (0x06)

#define RADAR_CMD_TRIG_DEPTH    (0x20)

#define RADAR_CMD_START_DEPTH   (0x30)
#define RADAR_CMD_STOP_DEPTH    (0x31)

#define RADAR_CMD_TAKE_DBG_IMG  (0x60)
#define RADAR_CMD_READ_DBG_IMG  (0x61)

#define RADAR_CMD_REPORT_DEPTH  (0x7E)
#define RADAR_CMD_REPORT_ERROR  (0X7F)

#define CMD_MASK_REQ_RSP        ((TU8)(0x80))  // CMD bit 7
#define CMD_BIT_REQ             ((TU8)(0x80))
#define CMD_BIT_RSP             ((TU8)(0x00))

#ifdef __cplusplus
}
#endif

#endif // __MSG_H__
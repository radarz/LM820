#ifndef __RADAR_OPS_H__
#define __RADAR_OPS_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @mainpage radar_clt demo application for Percipio linear radar device

  @b Introduction

  This documentation describes the interface of the linear radar device SDK developed by the Percipio Technology Ltd.(http://www.percipio.xyz).

  The code described in this manual implements the basic operations between the host and the device.
  This SDK Library manages the HAL layer, XCOM_PORT layer, XCOM layer, DEVICE_OPERATION layer, and the UTILS which can be used by all layers.
  This SDK includes also a demo application, radar_clt, to facilitate the customer use.

  @n The HAL layer helps upper layers to adapt different hardware and operating systems.
  @n The XCOM_PORT layer works as a glue layer between HAL and XCOM.
  @n The XCOM layer works as state machine of common message processing, independently with platform and communication port.
  @n The DEVICE_OPERATION layer provides upper layers the APIs to operate the device.
  @n The UTILS works as utilities of all layers.

  <b> Using the library </b>

  For the use in CONT(continuous) depth mode, the library must be used through the following steps:
  @li Call the radar_open() function, passing the port string as the argument
  @li Call the radar_set_mode() function, passing the mode flag RADAR_MODE_CONT as the argument
  @li Call the radar_cont_start() function to start the continuous depth output
  @li Call the radar_cont_get_depth() function to get the depth frame

  For the use in TRIG(trigger) depth mode, the library must be used through the following steps:
  @li Call the radar_open() function, passing the port string as the argument
  @li Call the radar_set_mode() function, passing the mode flag RADAR_MODE_TRIG as the argument
  @li Call the radar_trig_get_depth() function to get the depth frame
  
  For the use to get debug image from the device, the library must be used through the following steps:
  @li Call the radar_open() function, passing the port string as the argument
  @li Call the radar_take_dbg_img() function to capture an image and save it in the device
  @li Call the radar_read_dbg_img() function repeatedly to read the debug image segmentation from the device

  <b> Example codes </b>

Include the header file of the interface:
~~~{.h}

#include "radar_ops.h"

~~~

Common initialization for both CONT mode and TRIG mode:
~~~{.c}

TBool GetDepth_CommonInit(void)
{
    TDevInfo tDevInfo;
    TU16 nFov;
    TU16 nDepthSize;

    if (radar_open("\\\\.\\COM14") < 0) // Port string: "\\\\.\\COMxx" in Windows, or "/dev/ttySxx" in Linux
    {
        printf("radar_open failed!\n");
        return TFalse;
    }

    if (radar_get_info(&tDevInfo) < 0)
    {
        printf("radar_get_info failed!\n");
        goto error;
    }
    else
    {
        printf("Product Name: %s\n", tDevInfo.Name);
        printf("Product Version: %d.%d\n", tDevInfo.nMajorVer, tDevInfo.nMinorVer);
        printf("Serial Number: %s\n", tDevInfo.SerialNum);
    }

    if (radar_get_fov(&nFov) < 0)
    {
        printf("radar_get_fov failed!\n");
        goto error;
    }
    else
    {
        printf("FOV: %f Degree\n", (float)(nFov / 10.0));
    }

#if 0 // use default brightness recommended
    if (radar_set_ld(50) < 0) // Back to default after reset
    {
        printf("radar_set_ld failed!\n");
        goto error;
    }
#endif

    if (radar_get_max_res(&nDepthSize) < 0)
    {
        printf("radar_get_max_res failed!\n");
        goto error;
    }
    else
    {
        printf("Use max resolution: %d Points\n", nDepthSize);
    }

    if (radar_set_res(nDepthSize) < 0)
    {
        printf("radar_set_res failed!\n");
        goto error;
    }

    return TTrue;

error:
    radar_close();
    return TFalse;
}

~~~

To get depth in CONT mode:
~~~{.c}

void GetDepth_Continuous(void)
{
    TU32 nTimestamp;
    TU16 *pDepth = NULL;
    TU16 nDepthSize;

    if (GetDepth_CommonInit() != TTrue)
    {
        printf("GetDepth_CommonInit failed!\n");
        goto error;
    }

    if (radar_set_mode(RADAR_MODE_CONT) < 0)
    {
        printf("radar_set_mode failed!\n");
        goto error;
    }

    if (radar_cont_start() < 0)
    {
        printf("radar_cont_start failed!\n");
        goto error;
    }

    while (1)
    {
        if (radar_cont_get_depth(300, &nTimestamp, &pDepth, &nDepthSize) == RADAR_ERROR_SUCCESS)
        {
            printf("Depth RCVD. timestamp: %u, depth_size: %d\n", nTimestamp, nDepthSize);
        }
        else
        {
            printf("radar_cont_get_depth failed!\n");
        }
    }

error:
    radar_close();
}

~~~

To get depth in TRIG mode:
~~~{.c}

void GetDepth_Trig(void)
{
    TU32 nTimestamp;
    TU16 *pDepth = NULL;
    TU16 nDepthSize;

    if (GetDepth_CommonInit() != TTrue)
    {
        printf("GetDepth_CommonInit failed!\n");
        goto error;
    }

    if (radar_set_mode(RADAR_MODE_TRIG) < 0)
    {
        printf("radar_set_mode failed!\n");
        goto error;
    }

    while (1)
    {
        if (radar_trig_get_depth(&nTimestamp, &pDepth, &nDepthSize) == RADAR_ERROR_SUCCESS)
        {
            printf("Depth RCVD. timestamp: %u, depth_size: %d\n", nTimestamp, nDepthSize);
        }
        else
        {
            printf("radar_trig_get_depth failed!\n");
        }
    }

error:
    radar_close();
}

~~~

To get debug image from the device:
~~~{.c}

void GetDebugImage(void)
{
#define DAT_LEN_FOR_DBGIMG_READ     (512)
    TU16 nWidth;
    TU16 nHeight;
    TU32 nOffset = 0;
    TU8  buf[DAT_LEN_FOR_DBGIMG_READ];
    TU16 nLen = DAT_LEN_FOR_DBGIMG_READ;

    if (radar_open("\\\\.\\COM14") < 0) // Port string: "\\\\.\\COMxx" in Windows, or "/dev/ttySxx" in Linux
    {
        printf("radar_open failed!\n");
        return;
    }

    if (radar_take_dbg_img(&nWidth, &nHeight) < 0)
    {
        printf("radar_take_dbg_img failed!\n");
        goto error;
    }

    while (1)
    {
        nLen = DAT_LEN_FOR_DBGIMG_READ;

        if (radar_read_dbg_img(nOffset, buf, &nLen) == RADAR_ERROR_SUCCESS)
        {
            printf("radar_read_dbg_img: offset=0x%X, len=%d\n", nOffset, nLen);

            if (nLen < DAT_LEN_FOR_DBGIMG_READ)
            {
                printf("Debug Image done!\n");
                break;
            }

            nOffset += DAT_LEN_FOR_DBGIMG_READ;
        }
    }

error:
    radar_close();
}

~~~

 */

#include "hal.h"

#define RADAR_ERROR_SUCCESS             (0)         /**< @brief error code for return: success */
#define RADAR_ERROR_PORT_FAILED         (-1)        /**< @brief error code for return: port unavailable for communication */
#define RADAR_ERROR_DEVICE_FAILED       (-2)        /**< @brief error code for return: device unavailable to work */
#define RADAR_ERROR_ACCESS_TIMEOUT      (-3)        /**< @brief error code for return: communication timeout */
#define RADAR_ERROR_WRONG_PARAM         (-4)        /**< @brief error code for return: wrong parameters */
#define RADAR_ERROR_DEPTH_UNAVAILABLE   (-5)        /**< @brief error code for return: depth frame not ready */
#define RADAR_ERROR_IMPLEMENTATION      (-6)        /**< @brief error code for return: local implementation failure */

/**
  * @brief mode definition of the device
  * @see radar_set_mode
  */
enum {
    RADAR_MODE_IDLE = 0,    /**< @brief idle mode */
    RADAR_MODE_TRIG,        /**< @brief trigger mode */
    RADAR_MODE_CONT         /**< @brief continuous mode */
};

/**
  * @brief device-specific information
  * @see radar_get_info
  */
typedef struct {
    TU8 nMajorVer;          /**< @brief the major version of the device */
    TU8 nMinorVer;          /**< @brief the minor version of the device */
    TU8 SerialNum[32];      /**< @brief the serial number of the device */
    TU8 Name[64];           /**< @brief the name of the device */
} TDevInfo;

/**
 * @brief   initialize the device
 * @return  0 in case of success or <0 in case of failure
 */
int radar_init(void);

/**
 * @brief   get device-specific information from the device
 * @param   [out] pDevInfo device-specific information of the device
 * @return  0 in case of success or <0 in case of failure
 */
int radar_get_info(TDevInfo * pDevInfo);

/**
 * @brief   set the brightness of the laser on the device
 * @param   [in] nPower the brightness value, 0~100
 * @return  0 in case of success or <0 in case of failure
 */
int radar_set_ld(TU8 nPower);

/**
 * @brief   set the working mode of the device
 * @param   [in] nMode working mode of the device, RADAR_MODE_IDLE or RADAR_MODE_TRIG or RADAR_MODE_CONT
 * @return  0 in case of success or <0 in case of failure
 */
int radar_set_mode(TU8 nMode);

/**
 * @brief   set the depth size output from the device
 * @param   [in] nDepthSize the depth size for output, max or max/2 or max/4 or max/8
 * @return  0 in case of success or <0 in case of failure
 */
int radar_set_res(TU16 nDepthSize);

/**
* @brief   get the field angle from the device
* @param   [out] pFov the field angle of the device(unit: 0.1 degree)
* @return  0 in case of success or <0 in case of failure
*/
int radar_get_fov(TU16 * pFov);

/**
* @brief   get the max depth resolution from the device
* @param   [out] pMaxRes the max resolution of the device
* @return  0 in case of success or <0 in case of failure
*/
int radar_get_max_res(TU16 * pMaxRes);

/**
 * @brief   get a depth frame from the device in TRIG mode
 * @param   [out] pTimestamp the timestamp in ms of the depth frame
 * @param   [out] ppDepth the pointer to buffer of the output depth frame
 * @param   [out] pDepthSize the size of the output depth frame
 * @return  0 in case of success or <0 in case of failure
 */
int radar_trig_get_depth(TU32 *pTimestamp, TU16 ** ppDepth, TU16 * pDepthSize);

/**
 * @brief   start depth frame output in CONT mode
 * @return  0 in case of success or <0 in case of failure
 */
int radar_cont_start(void);

/**
 * @brief   stop depth frame output in CONT mode
 * @return  0 in case of success or <0 in case of failure
 */
int radar_cont_stop(void);

/**
 * @brief   get a depth frame from the device in CONT mode
 * @param   [in] nTimeout the wait time in ms for the depth frame 
 * @param   [out] pTimestamp the timestamp in ms of the depth frame
 * @param   [out] ppDepth the pointer to the buffer of the output depth frame
 * @param   [out] pDepthSize the size of the output depth frame
 * @return  0 in case of success or <0 in case of failure
 */
int radar_cont_get_depth(TU32 nTimeout, TU32 *pTimestamp, TU16 ** ppDepth, TU16 * pDepthSize);

/**
 * @brief   take an image for debug use and get back the resolution
 * @param   [out] pWidth width of the captured image
 * @param   [out] pHeight height of the captured image
 * @return  0 in case of success or <0 in case of failure
 */
int radar_take_dbg_img(TU16 *pWidth, TU16 *pHeight);

/**
 * @brief   read a segmentation of the debug image taken by radar_take_dbg_img
 * @param   [in] nOffset the offset of segmentation to read
 * @param   [out] pDat the pointer to the buffer to hold the segmentation
 * @param   [out] pDatLen the length of the segmentation
 * @return  0 in case of success or <0 in case of failure
 */
int radar_read_dbg_img(TU32 nOffset, TU8 * pDat, TU16 * pDatLen);

/**
 * @brief   open the device 
 * @param   [in] szPort port string to communicate, e.g. COM0 or /dev/ttyS0
 * @return  0 in case of success or <0 in case of failure
 */
int radar_open(char * szPort);

/**
 * @brief   close the radar
 * @return  0 in case of success or <0 in case of failure
 */
int radar_close(void);

#ifdef __cplusplus
}
#endif

#endif // __RADAR_OPS_H__

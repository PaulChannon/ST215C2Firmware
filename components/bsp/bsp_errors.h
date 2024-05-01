/**
 * @file bsp_errors.h
 * @brief BSP layer error codes
*/
#ifndef _BSP_ERRORS_H
#define _BSP_ERRORS_H

#include "common.h"

// BSP error type
typedef int32_t _TBspError;

// BSP error codes
#define BSP_OK                      0
#define BSP_INITIALISATION_ERROR    1
#define BSP_CHANNEL_OUT_OF_RANGE    2
#define BSP_CHANNEL_NOT_CONFIGURED  3
#define BSP_I2C_ERROR               4
#define BSP_RTC_ERROR               5


#endif // _BSP_ERRORS_H

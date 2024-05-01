/**
 * @file bsp.h
 * @brief Implements a board support package (BSP) which provides an interface to the controller hardware
*/
#ifndef _BSP_H
#define _BSP_H

#include "common.h"
#include "bsp_errors.h"
#include "debug_io.h"
#include "timers.h"
#include "i2c_driver.h"
#include "eeprom.h"
#include "rtc.h"

extern _TBspError initialise_bsp(void (*timer_handler)());

#endif // _BSP_H

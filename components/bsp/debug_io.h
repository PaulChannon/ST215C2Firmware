/**
 * @file debug_io.h
 * @brief Provides an interface to debug IO facilities
*/
#ifndef _DEBUG_IO_H
#define _DEBUG_IO_H

#include "common.h"
#include "bsp_errors.h"

extern _TBspError initialise_debug_io();
extern _TBspError set_debug_led_state(bool on);

#endif // _DEBUG_IO_H

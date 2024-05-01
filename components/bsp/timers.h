/**
 * @file timers.h
 * @brief Provides timer resources
*/
#ifndef _TIMERS_H
#define _TIMERS_H

#include "common.h"
#include "bsp_errors.h"

extern _TBspError initialise_timers(void (*timer_handler)());
extern void delay_ms(int32_t delay_ms);

#endif // _TIMERS_H

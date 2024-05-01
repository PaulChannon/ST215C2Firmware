/**
 * @file rtc.h
 * @brief Provides an interface to the real-time clock RV-3032-C7
*/
#ifndef _RTC_H
#define _RTC_H

#include "common.h"
#include "bsp_errors.h"

// Structure used to hold a date and time
typedef struct
{
	// Last two digits of the year in BCD, e.g. 2023 would be 0x23
	uint8_t year;

	// Month number (1 to 12) in BCD, e.g. October would be 0x10
	uint8_t month;

	// Day number (1 to 31) in BCD
	uint8_t day;

	// Hour (0 to 23) in BCD
	uint8_t hour;

	// Minute (0 to 59) in BCD
	uint8_t minute;

	// Second (0 to 59) in BCD
	uint8_t second;
}
_TDateTime;

extern _TBspError initialise_rtc();
extern _TBspError check_rtc_configured(bool *configured);
extern _TBspError configure_rtc();
extern _TBspError set_rtc_time_and_date(_TDateTime *date_time);
extern _TBspError read_rtc_time_and_date(_TDateTime *date_time);
extern int32_t get_rtc_seconds();

#endif // _RTC_H

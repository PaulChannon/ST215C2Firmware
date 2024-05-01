/**
 * @file main.c
 * @brief Contains the firmware entry point 
*/
#include "common.h"

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "bsp.h"
#include "controller.h"

// Controls debug output to the serial port:
//   0 - nothing output
//   1 - basic information and error
//   2 - verbose output
#define DEBUG_OUTPUT 1

#define DEBUG_VERBOSE(format, ...)           if (DEBUG_OUTPUT > 1) { ESP_LOGI(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_INFO(format, ...)              if (DEBUG_OUTPUT > 0) { ESP_LOGI(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_ERROR(format, ...)             if (DEBUG_OUTPUT > 0) { ESP_LOGE(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_DUMP(buffer, buff_len, level)  if (DEBUG_OUTPUT > 0) { ESP_LOG_BUFFER_HEXDUMP(MODULE_NAME, buffer, buff_len, level); }

static const char *MODULE_NAME = "Main";

// Indicates if the controller is ready for operation
static bool _controller_ready = false;

static void scheduler_task(void *parameters);
static void power_down_handler();
static void timer_handler();

/**
 * @brief Firmware entry point
 */
void app_main()
{
	_TBspError status;

	// Initialise the board support package
	status = initialise_bsp(timer_handler, power_down_handler);
	if (status != BSP_OK)
	{
		DEBUG_ERROR("Failed to initialise BSP");
	}

	// Create a task to run the scheduler
    xTaskCreate(&scheduler_task, "scheduler_task", 16384, NULL, 5, NULL);

    // Everything else is performed by tasks and interrupts
	while (true)
	{
		delay_ms(1000);
	}
}

/**
 * @brief Scheduler task which runs the main code
 * 
 * @param parameters Optional task parameters
*/
static void scheduler_task(void *parameters)
{
	bool rtc_configured;

	// Configure the display
    configure_display();

	// Calibrate the ADC
  	calibrate_adc();
	delay_ms(100);

	// Configure the RTC chip if required. This should only happen once after board manufacture
	if (check_rtc_configured(&rtc_configured) == BSP_OK)
	{
		DEBUG_INFO("RTC configured %d", rtc_configured);
		if (!rtc_configured)
		{
			if (configure_rtc() == BSP_OK)
			{
				DEBUG_INFO("Configuration successful");
			}
		}
	}

	// Clear the displays and LEDs
    clear_all_displays();

	// Call the controller task and stay there
	_controller_ready = true;
	controller_task();
}

/**
 * @brief Called at 10ms intervals by a hardware timer
 */
static void timer_handler()
{
	if (!_controller_ready) return;

	int64_t time_since_boot = esp_timer_get_time();
	controller_timer_handler(time_since_boot);
}

/**
 * @brief Called when the controller is about to power down
 */
static void power_down_handler()
{
	if (!_controller_ready) return;

	controller_power_down_handler();
}

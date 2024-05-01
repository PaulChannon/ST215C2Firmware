/**
 * @file timers.c
 * @brief Provides timer resources
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "timers.h"

// Intervals at which the timer handler is to be called
#define TIMER_INTERVAL_US 10000

static void (*_timer_handler)();
static void timer_callback(void *arguments);

/**
 * @brief Initialises the timer system
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError initialise_timers(void (*timer_handler)())
{
	esp_err_t status;
    esp_timer_handle_t timer;

	_timer_handler = timer_handler;

	// Create a 10ms periodic timer
    esp_timer_create_args_t timer_args = {
            .callback = &timer_callback,
            .name = "timer_10ms"
    };
    status = esp_timer_create(&timer_args, &timer);
	if (status != ESP_OK) return BSP_INITIALISATION_ERROR;

	// Start the timer
	status = esp_timer_start_periodic(timer, TIMER_INTERVAL_US);	
	if (status != ESP_OK) return BSP_INITIALISATION_ERROR;

	return BSP_OK;
}

/**
 * @brief Waits for a given time
 * 
 * @param delay_ms Required delay in ms
*/
void delay_ms(int32_t delay_ms)
{
	vTaskDelay(delay_ms / portTICK_PERIOD_MS);
}

/**
 * @brief Called by the periodic timer
 * 
 * @param arguments Optional callback arguments
*/
static void timer_callback(void *arguments)
{
	// Call the timer handler with the current time for reference
    int64_t time_since_boot = esp_timer_get_time();
	_timer_handler(time_since_boot);
}



/**
 * @file debug_io.c
 * @brief Provides an interface to debug IO facilities
*/
#include "driver/gpio.h"
#include "debug_io.h"

// Digital output pins
#define GPIO_DEBUG_LED      10

// Configuration mask
#define GPIO_MASK ((1ULL << GPIO_DEBUG_LED))

/**
 * @brief Configures the digital ouputs used to drive the debug outputs and LED
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError initialise_debug_io()
{
    // Configure the following pins as digital outputs
    //   IO10 - Debug/test LED

	gpio_config_t io_config = {};
	io_config.intr_type = GPIO_INTR_DISABLE;
	io_config.mode = GPIO_MODE_OUTPUT;
	io_config.pin_bit_mask = GPIO_MASK;
	io_config.pull_down_en = 0;
	io_config.pull_up_en = 0;
	gpio_config(&io_config);

	return BSP_OK;
}

/**
 * @brief Sets the state of the debug/test LED
 * 
 * @param on True to turn the LED on
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError set_debug_led_state(bool on)
{
    gpio_set_level(GPIO_DEBUG_LED, on ? 0: 1);

	return BSP_OK;
}

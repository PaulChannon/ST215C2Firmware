/**
 * @file  bsp.c
 * @brief Implements a board support package (BSP) which provides an interface to the hardware
*/
#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "bsp.h"

static void display_banner();

/**
 * @brief Initialises the board support package.  This should be called once at boot-up
 * 
 * @param timer_handler Called at 10ms intervals
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError initialise_bsp(void (*timer_handler)())
{
	_TBspError result;

	// Display a banner on the monitor serial port
	display_banner();

	// Initialise peripherals
    if ((initialise_debug_io()) != BSP_OK) return result;
	if ((initialise_timers(timer_handler)) != BSP_OK) return result;
	if ((initialise_i2c()) != BSP_OK) return result;
	if ((initialise_rtc()) != BSP_OK) return result;
	if ((initialise_eeprom()) != BSP_OK) return result;

	return BSP_OK;
}

/**
 * @brief Displays a banner on the monitor serial port
*/
static void display_banner()
{
    esp_chip_info_t chip_info;
    uint32_t flash_size;

	// Get ESP32 chip information and flash size
    esp_chip_info(&chip_info);
    esp_flash_get_size(NULL, &flash_size);

    // Display banner
    printf("--------------------------------------------------------------------------------\n");
	printf(" ST215C firmware\n");
	printf("\n");
	printf(" ESP32 information:\n");
	printf("   Chip: %s\n", CONFIG_IDF_TARGET);
	printf("   Silicon revision: v%d.%d\n", chip_info.revision / 100, chip_info.revision % 100);
	printf("   Flash size: %lu Mb\n", flash_size / (uint32_t)(1024 * 1024));
	printf("\n");
	printf(" Free heap size: %lu bytes\n", esp_get_minimum_free_heap_size());
	printf("--------------------------------------------------------------------------------\n");
}


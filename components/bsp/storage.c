/**
 * @file storage.c
 * @brief Provides non-volatile data storage facilities
*/
#include "common.h"
#include "nvs_flash.h"
#include "storage.h"

/**
 * @brief Initialises non-volatile storage system
*/
void initialise_nv_storage()
{
	esp_err_t result;

	// Initialise the default NVS partition in flash.  This is used by the
	// WiFi libraries
	result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES)
    {
    	// Initialisation failed, so erase the partition and reinitialise
		ESP_ERROR_CHECK(nvs_flash_erase());
		result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
}


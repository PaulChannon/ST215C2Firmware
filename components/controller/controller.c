/**
 * @file controller.c
 * @brief Holds information about the state of the controller
*/
#include <ctype.h>
#include <stdbool.h>
#include "controller.h"

// Controls debug output to the serial port:
//   0 - nothing output
//   1 - basic information and error
//   2 - verbose output
#define DEBUG_OUTPUT 1

#define DEBUG_VERBOSE(format, ...)           if (DEBUG_OUTPUT > 1) { ESP_LOGI(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_INFO(format, ...)              if (DEBUG_OUTPUT > 0) { ESP_LOGI(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_ERROR(format, ...)             if (DEBUG_OUTPUT > 0) { ESP_LOGE(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_DUMP(buffer, buff_len, level)  if (DEBUG_OUTPUT > 1) { ESP_LOG_BUFFER_HEXDUMP(MODULE_NAME, buffer, buff_len, level); }

static const char *MODULE_NAME = "Controller link";

_TController _controller = {0};

/**
 * @brief Initialises the controller structures
*/
void initialise_controller()
{
/*    
	uint8_t *address;

	// Convert the MAC address into string form
	address = get_mac_address();
	sprintf(_controller.mac_address, "%02X:%02X:%02X:%02X:%02X:%02X",
			address[0], address[1], address[2], address[3], address[4], address[5]);
*/
}

/**
 * @brief Updates the controller data structure via the interface
*/
void update_controller()
{
/*    
    // Read configuration data from the controller if necessary
    if (!_controller.configuration_available)
    {
        if (read_controller_configuration(&(_controller.configuration)))
        {
            _controller.configuration_available = true;
        }
        else
        {
            DEBUG_ERROR("Cannot read configuration from controller");
            return;
        }
    }

    // Read status information from the controller
    if (read_controller_status(&(_controller.status)))
    {
        _controller.status_available = true;
    }
    else
    {
        _controller.status_available = false;
        DEBUG_ERROR("Cannot read status from controller");
        return;
    }

    // If the configuration data has changed, re-read it from the controller on the next cycle
    if (_controller.status.configuration_changed) _controller.configuration_available = false;
*/    
}

/**
 * @brief Resets the PIC controller
*/
void reset_controller()
{

}

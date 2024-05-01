/**
 * @file scheduler.c
 * @brief Schedules all communications functions
*/
#include "common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_interface.h"
#include "http_client.h"
#include "http_server.h"
#include "controller.h"
#include "message_handler.h"
#include "scheduler.h"

// Controls debug output to the serial port:
//   0 - nothing output
//   1 - basic information and error
//   2 - verbose output
#define DEBUG_OUTPUT 1

#define DEBUG_VERBOSE(format, ...)           if (DEBUG_OUTPUT > 1) { ESP_LOGI(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_INFO(format, ...)              if (DEBUG_OUTPUT > 0) { ESP_LOGI(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_ERROR(format, ...)             if (DEBUG_OUTPUT > 0) { ESP_LOGE(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_DUMP(buffer, buff_len, level)  if (DEBUG_OUTPUT > 1) { ESP_LOG_BUFFER_HEXDUMP(MODULE_NAME, buffer, buff_len, level); }

// FreeRTOS ticks per second
#define TICKS_PER_SECOND  configTICK_RATE_HZ

// Maximum interval between posts (seconds)
#define MAX_POST_INTERVAL 60

// Interval until the next post when an error occurs (seconds)
#define ERROR_POST_INTERVAL 10

// Period over which rate limiting rules are applied (seconds)
#define RATE_LIMIT_PERIOD 300

// Maximimum allowed number of posts in that time
#define RATE_LIMIT_MAX_POSTS 100

// Interval until the next post when rate limits are imposed (seconds)
#define RATE_LIMIT_POST_INTERVAL 30

// Scheduler states
#define STATE_INITIALISING  0
#define STATE_DISCONNECTED  1
#define STATE_POSTING       2
#define STATE_WAITING       3

static const char *MODULE_NAME = "Scheduler";
static int32_t _rate_limit_start_tick = 0;
static int32_t _rate_limit_post_count = 0;
static bool _connected_to_router = false;

static void scheduler_task(void *parameters);
static bool check_rate_limit();

/**
 * @brief Initialises the scheduler
*/
void initialise_scheduler()
{
	// Create a task to run the scheduler
    xTaskCreate(&scheduler_task, "scheduler_task", 16384, NULL, 5, NULL);
}

/**
 * @brief Runs the scheduler task
*/
static void scheduler_task(void *parameters)
{
	int32_t firing_state;
	int32_t update_start_tick = 0;
	int32_t ticks_until_post;
	int32_t next_post_delay = 0;

	// Wait for the controller to boot up by attempting to read the firing mode
	while (!read_firing_state(&firing_state) || firing_state == FIRING_STATE_INITIALISING)
	{
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}

	DEBUG_INFO("Initial firing state: %ld", firing_state);

	// Send the MAC address to the controller
	write_mac_address(_controller.mac_address);

	// See if the user has issued a special instruction
	if (firing_state == FIRING_STATE_PAIRING)
	{
		DEBUG_VERBOSE("Pairing mode");

		// Start the WiFi interface in station mode
		start_wifi(false);

		// Run the WPS connection process
		pair_with_router();

		// Regardless of the outcome, reset the controller to clear pairing mode
		while (!reset_controller())
		{
			vTaskDelay(10 / portTICK_PERIOD_MS);
		}
	}
	else if (firing_state == FIRING_STATE_AP)
	{
		DEBUG_VERBOSE("Access point mode");

		// Start the WiFi interface in combined station/access point mode
		start_wifi(true);
	}
	else
	{
		DEBUG_VERBOSE("Normal mode");

		// Start the WiFi interface in station mode
		start_wifi(false);
	}

	// Update the HTTP client and server as required
	while (true)
	{
        // Read status information from the controller.  This is required to keep the STM32
	    // comms watchdog running
        update_controller();

		// Service the HTTP server if the controller is operating as an access point
		if (is_access_point_available())
		{
			DEBUG_VERBOSE("Access point available");

			// Start or stop the web-server as required
			if (is_access_point_in_use())
			{
				DEBUG_VERBOSE("Access point device connected");
				if (!is_http_server_running())
				{
					DEBUG_INFO("Starting web server");
					start_http_server();
				}
			}
			else
			{
				DEBUG_VERBOSE("Access point device not connected");
				if (is_http_server_running())
				{
					DEBUG_INFO("Stopping web server");
					stop_http_server();
				}
			}

			// Check if the server is running
			if (is_http_server_running())
			{
				DEBUG_VERBOSE("HTTP server running");
			}
			else
			{
				DEBUG_VERBOSE("HTTP server not running");
			}
		}
		else
		{
			DEBUG_VERBOSE("Access point not available");
		}

		// Connect to the router if required
		if (is_router_known() && !is_connected_to_router())
		{
			DEBUG_INFO("Connecting to router");
			if (!connect_to_router())
			{
				DEBUG_ERROR("Failed to connect to router");

				// Wait before trying to connect again
				vTaskDelay(5000 / portTICK_PERIOD_MS);
			}
			else
			{
				DEBUG_VERBOSE("Connected to router");
			}
		}

		// Log router connections and disconnections
		if (_connected_to_router)
		{
		    if (!is_connected_to_router())
		    {
		        log_event(EVENT_WIFI_DISCONNECTED, COMMS_ERROR_NONE);
		        _connected_to_router = false;
		    }
		}
		else
		{
            if (is_connected_to_router())
            {
                log_event(EVENT_WIFI_CONNECTED, COMMS_ERROR_NONE);
                _connected_to_router = true;
            }
		}

		// Post a message to the server if connected to a router and not in access point mode
		if (is_connected_to_router() && !is_access_point_available())
		{
			DEBUG_VERBOSE("Connected to router");

			// Check if it is time for a new post
			ticks_until_post = next_post_delay * TICKS_PER_SECOND - (xTaskGetTickCount() - update_start_tick);
			if (ticks_until_post < 0)
			{
				DEBUG_INFO("-------------------------------------------------------------------------");
		        DEBUG_INFO("Tick = %ld, heap left = %ld", (int32_t)xTaskGetTickCount(), (int32_t)xPortGetFreeHeapSize());
				DEBUG_INFO("Posting message to server");

				// If there is new status information available, post a message to the server and process the reply
				if (_controller.status_available && post_message_to_server(&next_post_delay))
				{
					DEBUG_INFO("Posting message succeeded");
				}
				else
				{
					next_post_delay = ERROR_POST_INTERVAL;
					DEBUG_INFO("Posting message failed or status not available");
				}

				// Apply rate limiting rules to avoid overloading the server if something goes wrong
				if (check_rate_limit())
				{
					next_post_delay = RATE_LIMIT_POST_INTERVAL;
				}

				// Note the time ready for the next post cycle
				update_start_tick = xTaskGetTickCount();

				DEBUG_INFO("Next post delay %ld seconds", next_post_delay);
			}
			else
			{
				// Wait before trying again to allow other tasks to run
				vTaskDelay(2000 / portTICK_PERIOD_MS);
			}
		}
		else
		{
			// Wait before trying again to allow other tasks to run
			vTaskDelay(2000 / portTICK_PERIOD_MS);
		}

	    // Check for STM32 comms link errors
	    if (link_error())
	    {
	        DEBUG_ERROR("Controller link locked up");
	        log_event(EVENT_ESP32_LINK_ERROR, COMMS_ERROR_LOCKUP);
	    }
	}
}

/**
 * @brief Applies rate limiting rules to the suggested post delay to avoid overloading the server
 * 
 * @returns True if rate limiting is required
*/
static bool check_rate_limit()
{
	// Reset the rate limit period if the last one has finished
	if ((xTaskGetTickCount() - _rate_limit_start_tick) > (RATE_LIMIT_PERIOD * TICKS_PER_SECOND))
	{
		DEBUG_VERBOSE("Resetting rate limit period");
		_rate_limit_post_count = 0;
		_rate_limit_start_tick = xTaskGetTickCount();
	}

	_rate_limit_post_count++;
	DEBUG_VERBOSE("Rate limit count = %ld", _rate_limit_post_count);

	// Check if rate limiting is required
	return (_rate_limit_post_count > RATE_LIMIT_MAX_POSTS);
}

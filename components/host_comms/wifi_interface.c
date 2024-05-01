/**
 * @file wifi_interface.h
 * @brief Provides an interface to the WiFi peripheral and handles the connection to the router
*/
#include "common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wps.h"
#include "nvs_flash.h"
#include "storage.h"
#include "http_server.h"
#include "wifi_interface.h"

// Controls debug output to the serial port:
//   0 - nothing output
//   1 - basic information and error
//   2 - verbose output
#define DEBUG_OUTPUT 2

#define DEBUG_VERBOSE(format, ...)           if (DEBUG_OUTPUT > 1) { ESP_LOGI(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_INFO(format, ...)              if (DEBUG_OUTPUT > 0) { ESP_LOGI(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_ERROR(format, ...)             if (DEBUG_OUTPUT > 0) { ESP_LOGE(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_DUMP(buffer, buff_len, level)  if (DEBUG_OUTPUT > 1) { ESP_LOG_BUFFER_HEXDUMP(MODULE_NAME, buffer, buff_len, level); }

// Time to wait for the WiFi mutex
#define MUTEX_WAIT_TICKS ((TickType_t)10000)

// Define event flags
const int32_t STA_STARTED_BIT = BIT0;
const int32_t STA_CONNECTED_BIT = BIT1;
const int32_t STA_CONNECTION_FAILED_BIT = BIT2;
const int32_t STA_PAIR_SUCCESS_BIT = BIT3;
const int32_t STA_PAIR_FAIL_BIT = BIT4;
const int32_t AP_CONNECTED_BIT = BIT5;

static const char *MODULE_NAME = "WiFi interface";
static _TRouterDetails _router_details;
static bool _access_point_available = false;
static uint8_t _mac_address[6] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 };
static SemaphoreHandle_t _access_mutex;
static bool _connection_attempt_failed = false;
static EventGroupHandle_t _event_group;
static esp_netif_t *_station_interface = NULL;
static esp_netif_t *_access_point_interface = NULL;
static char *_network_descriptor = "controller";

static bool _connect_to_router();
static bool _pair_with_router();
static void _scan_for_routers(_TRouterDetails routers[], int32_t *routers_found);

static void scan_done_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void station_started_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void station_connected_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void station_disconnected_handler(void *null_argument, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void station_got_ip_handler(void *null_argument, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void wps_success_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void wps_error_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void wps_timeout_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void ap_connected_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void ap_disconnected_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data);

static void wifi_shutdown_handler();

static bool read_router_details(_TRouterDetails *router_details);
static void clear_router_details(_TRouterDetails *router_details);
static void write_router_details(_TRouterDetails *router_details);


/**
 * @brief Initialises the WiFi system
*/
void initialise_wifi()
{
    // Read the MAC address
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(_mac_address));

    // Create a mutex
    _access_mutex = xSemaphoreCreateMutex();
    assert(_access_mutex != NULL);
}

/**
 * @brief Starts the WiFi system
 * 
 * @param access_point Indicates if the WiFi should be an access point as well as a station
 * 
 * @returns True if successful
*/
bool start_wifi(bool access_point)
{
    wifi_config_t wifi_config;
    wifi_init_config_t default_configuration = WIFI_INIT_CONFIG_DEFAULT();

    if (access_point)
    {
    	DEBUG_INFO("Starting WiFi in station + access point mode");
    }
    else
    {
    	DEBUG_INFO("Starting WiFi in station mode");
    }
    _access_point_available = access_point;

    // Initialise the TCP/IP stack
    _event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialise the WiFi module
	ESP_ERROR_CHECK(esp_wifi_init(&default_configuration));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

	// See if there are router details stored in non-volatile storage
	if (read_router_details(&_router_details))
	{
		DEBUG_INFO("Stored router information, SSID = %s, password = %s", _router_details.ssid, _router_details.password);
	}
	else
	{
		// If not, clear the entry just to be tidy
		clear_router_details(&_router_details);
		DEBUG_INFO("No valid router information stored");
	}

    // Put the WiFi system into either station only or station + access point mode
    if (access_point)
    {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    }
    else
	{
    	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	}

	// Set up the network interface for station mode
	esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
	esp_netif_config.if_desc = _network_descriptor;
	esp_netif_config.route_prio = 128;
	_station_interface = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
	esp_wifi_set_default_wifi_sta_handlers();

    // If router details are available, configure the station mode password and SSID
	if (_router_details.valid)
	{
		memset(&wifi_config, 0, sizeof(wifi_config));
		strcpy((char *)wifi_config.sta.ssid, _router_details.ssid);
		strcpy((char *)wifi_config.sta.password, _router_details.password);
		wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
		wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
		wifi_config.sta.threshold.rssi = -127;
		wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;

		ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
		DEBUG_INFO("Connected to %s in station mode", _router_details.ssid);
	}

	// If required, configure the access point
    if (access_point)
    {
//    	_access_point_interface = esp_netif_create_default_wifi_ap();

    	const esp_netif_ip_info_t esp_netif_soft_ap_ip = {
    	        .ip = { .addr = ESP_IP4TOADDR( AP_IP_ADDRESS_1, AP_IP_ADDRESS_2, AP_IP_ADDRESS_3, AP_IP_ADDRESS_4) },
    	        .gw = { .addr = ESP_IP4TOADDR( AP_GW_ADDRESS_1, AP_GW_ADDRESS_2, AP_GW_ADDRESS_3, AP_GW_ADDRESS_4) },
    	        .netmask = { .addr = ESP_IP4TOADDR( AP_NM_ADDRESS_1, AP_NM_ADDRESS_2, AP_NM_ADDRESS_3, AP_NM_ADDRESS_4) }};

    	esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_AP();
    	esp_netif_config.if_desc = _network_descriptor;
    	esp_netif_config.ip_info = &esp_netif_soft_ap_ip;
    	_access_point_interface = esp_netif_create_wifi(WIFI_IF_AP, &esp_netif_config);
    	esp_wifi_set_default_wifi_ap_handlers();



        // Configure the WiFi access point
    	memset(&wifi_config, 0, sizeof(wifi_config));
    	strcpy((char *)wifi_config.ap.password, "");
    	strcpy((char *)wifi_config.ap.ssid, AP_SSID);
    	wifi_config.ap.ssid_len = strlen(AP_SSID);
    	wifi_config.ap.max_connection = 1;
    	wifi_config.ap.beacon_interval = 150;
    	wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
		DEBUG_INFO("Configured in access point mode");


    }

    // Create event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &scan_done_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &station_started_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &station_disconnected_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &station_got_ip_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &station_connected_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_WPS_ER_SUCCESS, &wps_success_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_WPS_ER_FAILED, &wps_error_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_WPS_ER_TIMEOUT, &wps_timeout_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &ap_connected_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &ap_disconnected_handler, NULL));

    ESP_ERROR_CHECK(esp_register_shutdown_handler(&wifi_shutdown_handler));

	// Start the WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    return true;
}

/**
 * @brief Gets the 6-byte MAC address for the WiFi module
 * 
 * @returns The MAC address
*/
uint8_t *get_mac_address()
{
	return _mac_address;
}

/**
 * @brief Indicates if router information is known
 * 
 * @returns True if router information is available
*/
bool is_router_known()
{
	return _router_details.valid;
}

/**
 * @brief Gets the SSID for the stored router
 * 
 * @returns Router SSID
*/
char *get_router_ssid()
{
	return _router_details.ssid;
}

/**
 * @brief Initiates the WPS connection process to pair with a router
 * 
 * @returns True if successful
*/
bool pair_with_router()
{
	bool result;

	// Wait for the WiFi system to be free
	if (xSemaphoreTake(_access_mutex, MUTEX_WAIT_TICKS) != pdTRUE) return false;

	// Attempt to pair
	result = _pair_with_router();

	// Release the WiFi system
	xSemaphoreGive(_access_mutex);

	return result;
}

/**
 * @brief Initiates the WPS connection process to pair with a router
 * 
 * @returns True if successful
*/
static bool _pair_with_router()
{
    esp_wps_config_t config = WPS_CONFIG_INIT_DEFAULT(WPS_TYPE_PBC);
    EventBits_t bits;
    wifi_config_t wifi_config;

	DEBUG_INFO("Pairing initiated");

	// Clear router details from non-volatile memory
	clear_router_details(&_router_details);

	// Clear pairing event bits
    xEventGroupClearBits(_event_group, STA_PAIR_SUCCESS_BIT | STA_PAIR_FAIL_BIT);

	// Start the pairing process
    ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
    ESP_ERROR_CHECK(esp_wifi_wps_start(0));

    // Wait until pairing has finished
	bits = xEventGroupWaitBits(_event_group, STA_PAIR_SUCCESS_BIT | STA_PAIR_FAIL_BIT, false, false, 1000000 / portTICK_PERIOD_MS);

	// Stop the pairing process
	ESP_ERROR_CHECK(esp_wifi_wps_disable());

	// Check the result
	if (bits & STA_PAIR_SUCCESS_BIT)
	{
		// Pairing was successful, so read router information
		ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config));

		// Store in non-volatile memory for next time
		strcpy(_router_details.ssid, (char *)wifi_config.sta.ssid);
		strcpy(_router_details.password, (char *)wifi_config.sta.password);
		_router_details.valid = true;
		write_router_details(&_router_details);

		DEBUG_INFO("Pairing complete, SSID = %s, password = %s", _router_details.ssid, _router_details.password);
		return true;
	}
	else
	{
		DEBUG_ERROR("Pairing failed");
		return false;
	}
}

/**
 * @brief Changes the router to which the WiFi is connected
 * 
 * @param ssid SSID for the router
 * @param password Password for the router
*/
void change_router(char *ssid, char* password)
{
    wifi_config_t wifi_config;

    // Wait for the WiFi system to be free
	if (xSemaphoreTake(_access_mutex, MUTEX_WAIT_TICKS) != pdTRUE) return;

	// Store router details
	strcpy(_router_details.ssid, ssid);
	strcpy(_router_details.password, password);
	_router_details.valid = true;
	write_router_details(&_router_details);

	// Change the SSID and password for station mode
	memset(&wifi_config, 0, sizeof(wifi_config));
	strcpy((char *)wifi_config.sta.ssid, _router_details.ssid);
	strcpy((char *)wifi_config.sta.password, _router_details.password);
	esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);

	// Release the WiFi system
	xSemaphoreGive(_access_mutex);
}

/**
 * @brief Disconnects and forgets the stored router information
*/
void forget_router()
{
	// Wait for the WiFi system to be free
	if (xSemaphoreTake(_access_mutex, MUTEX_WAIT_TICKS) != pdTRUE) return;

	// Clear existing router information
	clear_router_details(&_router_details);

	// If connected to the router, disconnect
	if ((xEventGroupGetBits(_event_group) & STA_CONNECTED_BIT) != 0)
	{
		// Disconnect from the router
		ESP_ERROR_CHECK(esp_wifi_disconnect());

		xEventGroupWaitBits(_event_group, STA_CONNECTED_BIT, false, false, 2000 / portTICK_PERIOD_MS);
	}

	// Release the WiFi system
	xSemaphoreGive(_access_mutex);
}

/**
 * @brief Indicates if the WiFi is connected to a router
 * 
 * @returns True if connected to a router
*/
bool is_connected_to_router()
{
	return (xEventGroupGetBits(_event_group) & STA_CONNECTED_BIT) != 0;
}

/**
 * @brief Indicates if ready to connect to a router
 * 
 * @returns True if the WiFi station has started up
*/
bool is_ready_to_connect()
{
	return (xEventGroupGetBits(_event_group) & STA_STARTED_BIT) != 0;
}

/**
 * @brief Indicates if a connection attempt failed since the last call
 * 
 * @returns True if there was an error
*/
bool connection_attempt_failed()
{
	if (_connection_attempt_failed)
	{
		_connection_attempt_failed = false;
		return true;
	}

	return false;
}

/**
 * @brief Connects to the router
 * 
 * @returns True if a connection was established
*/
bool connect_to_router()
{
	bool result;

	// Wait for the WiFi system to be free
	if (xSemaphoreTake(_access_mutex, MUTEX_WAIT_TICKS) != pdTRUE) return false;

	// Attempt to connect
	result = _connect_to_router();

	// Release the WiFi system
	xSemaphoreGive(_access_mutex);

	return result;
}

/**
 * @brief Connects to the router
 * 
 * @returns True if a connection was established
*/
static bool _connect_to_router()
{
	esp_err_t result;

	DEBUG_INFO("Connecting to router");

	// Clear the error flag
	_connection_attempt_failed = false;

	// Check that router information is available
	if (!_router_details.valid)
	{
		DEBUG_ERROR("No router information available to connect");
		return false;
	}

	// Check that the WiFi system has started up
	if ((xEventGroupGetBits(_event_group) & STA_STARTED_BIT) == 0)
	{
		DEBUG_ERROR("Not ready to connect");
		return false;
	}

	// Check if already connected
	if ((xEventGroupGetBits(_event_group) & STA_CONNECTED_BIT) != 0)
	{
		DEBUG_ERROR("Already connected to router");
		return true;
	}

	DEBUG_VERBOSE("Router SSID '%s'", _router_details.ssid);
	DEBUG_VERBOSE("Router password '%s'", _router_details.password);

	// Start the connection process
	DEBUG_INFO("Connecting...");

    xEventGroupClearBits(_event_group, STA_CONNECTION_FAILED_BIT);
	if ((result = esp_wifi_connect()) != ESP_OK)
	{
		_connection_attempt_failed = true;
		DEBUG_ERROR("Failed to connect to router (error %d)", result - ESP_ERR_WIFI_BASE);
		vTaskDelay(5000 / portTICK_PERIOD_MS);
		return false;
	}

	// Wait for the connection to be made
	xEventGroupWaitBits(_event_group, STA_CONNECTED_BIT | STA_CONNECTION_FAILED_BIT, false, false, 60000 / portTICK_PERIOD_MS);

    if ((xEventGroupGetBits(_event_group) & STA_CONNECTION_FAILED_BIT) != 0)
    {
        DEBUG_ERROR("Failed to connect to router");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return false;
    }
    else
    {
        if ((xEventGroupGetBits(_event_group) & STA_CONNECTED_BIT) != 0)
        {
            DEBUG_INFO("Connected to router");
            return true;
        }
        else
        {
            DEBUG_ERROR("Error connecting to router");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            return false;
        }
    }
}

/**
 * @brief Scans for access points/routers in the vicinity
*/
void scan_for_routers(_TRouterDetails routers[], int32_t *routers_found)
{
	// Wait for the WiFi system to be free
	if (xSemaphoreTake(_access_mutex, MUTEX_WAIT_TICKS) != pdTRUE) return;

	// Scan for routers
	_scan_for_routers(routers, routers_found);

	// Release the WiFi system
	xSemaphoreGive(_access_mutex);
}

/**
 * @brief Scans for access points/routers in the vicinity
*/
static void _scan_for_routers(_TRouterDetails routers[], int32_t *routers_found)
{
	uint16_t access_point_count = MAX_ROUTERS;
	wifi_ap_record_t access_points[MAX_ROUTERS];
	int32_t index;
	int32_t bars;
	int32_t rssi;
	wifi_scan_config_t scan_config = {
		.ssid = 0,
		.bssid = 0,
		.channel = 0
	};

	DEBUG_INFO("Scanning for access points");

	// Must be in access point mode
	if (!_access_point_available)
	{
		DEBUG_ERROR("Must be in access point mode");
	}

	// Start a foreground all-channel scan
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

	// Get a list found access points
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&access_point_count, access_points));

	DEBUG_INFO("Found %d access points:", access_point_count);
	for (index = 0; index < access_point_count; index++)
	{
		// Convert RSSI value to number of "bars"
		rssi = access_points[index].rssi;
		if (rssi > -55) bars = 4;
		else if (rssi > -66) bars = 3;
		else if (rssi > -77) bars = 2;
		else if (rssi > -88) bars = 1;
		else bars = 0;

		// Store access point details
		strncpy(routers[index].ssid, (char *)access_points[index].ssid, MAX_SSID_STRING_LENGTH);
		routers[index].requires_password = access_points[index].authmode > 0;
		routers[index].signal_strength = bars;

		DEBUG_INFO("%32s | %7ld | %7ld", (char *)access_points[index].ssid, rssi, bars);
	}

	*routers_found = access_point_count;
}

/**
 * @brief Obtains the current signal strength for the connection to the router
 * 
 * @param bars Returned signal strength as a number of "bars" (0 to 4)
 * 
 * @returns True if the signal strength could be obtained
*/
bool get_rssi(int32_t *rssi)
{
    wifi_ap_record_t wifi_data;

    // Read the RSSI value
    if (esp_wifi_sta_get_ap_info(&wifi_data) != ESP_OK) return false;

    DEBUG_INFO("Wi-Fi signal strength: %d", wifi_data.rssi);

    *rssi = wifi_data.rssi;

    return true;
}

/**
 * @brief Returns true when the WiFi is in access point mode
*/
bool is_access_point_available()
{
	return _access_point_available;
}

/**
 * @brief Returns true when a device is connected in access point mode
*/
bool is_access_point_in_use()
{
	return _access_point_available && (xEventGroupGetBits(_event_group) & AP_CONNECTED_BIT) != 0;
}

/**
 * @brief Reads router details from non-volatile storage
 * 
 * @param router_details The returned router information
 * 
 * @returns True if the details are valid
*/
static bool read_router_details(_TRouterDetails *router_details)
{
	nvs_handle nvs;
	size_t length;

	// Invalidate the current router details
	router_details->valid = false;

	// Open the non-volatile storage system
	ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs));

	// Read the router SSID string value
    length = MAX_SSID_STRING_LENGTH;
    if (nvs_get_str(nvs, "ssid", router_details->ssid, &length) != ESP_OK || router_details->ssid[0] == 0)
    {
    	nvs_close(nvs);
    	return false;
    }

	// Read the router password string value
    length = MAX_PASSWORD_STRING_LENGTH;
    if (nvs_get_str(nvs, "password", router_details->password, &length) != ESP_OK || router_details->password[0] == 0)
    {
    	nvs_close(nvs);
    	return false;
    }

	// Clean up
	nvs_close(nvs);

	// Router information is probably valid
	router_details->valid = true;

	return true;
}

/**
 * @brief Clears router details from non-volatile storage
 * 
 * @param router_details The returned router information structure
*/
static void clear_router_details(_TRouterDetails *router_details)
{
	int32_t size;

	// Clear the router structure
	size = sizeof(_TRouterDetails);
	memset(router_details, 0, size);

	// Write to non-volatile storage
	write_router_details(router_details);
}

/**
 * @brief Writes router details to non-volatile storage
 * 
 * @param router_details The router information structure
*/
static void write_router_details(_TRouterDetails *router_details)
{
	nvs_handle nvs;

	// Open the non-volatile storage system
	ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs) != ESP_OK);

	// Write the router SSID and password values
	ESP_ERROR_CHECK(nvs_set_str(nvs, "ssid", router_details->ssid));
	ESP_ERROR_CHECK(nvs_set_str(nvs, "password", router_details->password));

	// Commit the new values to flash
	ESP_ERROR_CHECK(nvs_commit(nvs));

	// Clean up
	nvs_close(nvs);
}

/**
 * @brief Handler called when the system shuts down
*/
static void wifi_shutdown_handler()
{
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &station_disconnected_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &station_got_ip_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &station_connected_handler));

    esp_wifi_disconnect();

    esp_err_t error = esp_wifi_stop();
    if (error == ESP_ERR_WIFI_NOT_INIT) return;
    ESP_ERROR_CHECK(error);

    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(_station_interface));

    esp_netif_destroy(_station_interface);

    _station_interface = NULL;
}

/**
 * @brief WiFi event handlers
 * 
 * @param esp_netif Network interface
 * @param event_base Event structure
 * @param event_id Event ID
 * @param event_data Event data
*/
static void scan_done_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    DEBUG_INFO("Handler - Scan finished");
}

static void station_started_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    DEBUG_INFO("Handler - Station started");
    xEventGroupSetBits(_event_group, STA_STARTED_BIT);
}

static void station_connected_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    DEBUG_INFO("Handler - Station connected");
}

static void station_got_ip_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    DEBUG_INFO("Handler - Connected to remote access point at %d.%d.%d.%d", IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(_event_group, STA_CONNECTED_BIT);
}

static void station_disconnected_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    DEBUG_INFO("Handler - Disconnected from remote access point");
    xEventGroupClearBits(_event_group, STA_CONNECTED_BIT);
    xEventGroupSetBits(_event_group, STA_CONNECTION_FAILED_BIT);
}

static void wps_success_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    DEBUG_INFO("Handler - WPS success");
    xEventGroupSetBits(_event_group, STA_PAIR_SUCCESS_BIT);
}

static void wps_error_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    DEBUG_INFO("Handler - WPS error");
    xEventGroupSetBits(_event_group, STA_PAIR_FAIL_BIT);
}

static void wps_timeout_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    DEBUG_INFO("Handler - WPS timeout");
    xEventGroupSetBits(_event_group, STA_PAIR_FAIL_BIT);
}

static void ap_connected_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    DEBUG_INFO("Handler - Station connected to access point");
    xEventGroupSetBits(_event_group, AP_CONNECTED_BIT);
}

static void ap_disconnected_handler(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    DEBUG_INFO("Handler - Station disconnected from access point");
    xEventGroupClearBits(_event_group, AP_CONNECTED_BIT);
}

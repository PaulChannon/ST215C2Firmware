/**
 * @file wifi_interface.h
 * @brief Provides an interface to the WiFi peripheral and handles the connection to the router
*/
#ifndef _WIFI_INTERFACE_H_
#define _WIFI_INTERFACE_H_

#include "common.h"

// Maximum number of routers found during a scan
#define MAX_ROUTERS 20

// Router details
#define MAX_SSID_STRING_LENGTH      32
#define MAX_PASSWORD_STRING_LENGTH  64

// Access point details
#define AP_SSID "Controller"

// Access point IP address
#define AP_IP_ADDRESS_1 192
#define AP_IP_ADDRESS_2 168
#define AP_IP_ADDRESS_3 100
#define AP_IP_ADDRESS_4 1

#define AP_URL "http://192.168.100.1"

// Access point gateway address
#define AP_GW_ADDRESS_1 192
#define AP_GW_ADDRESS_2 168
#define AP_GW_ADDRESS_3 100
#define AP_GW_ADDRESS_4 254

// Access point mask
#define AP_NM_ADDRESS_1 255
#define AP_NM_ADDRESS_2 255
#define AP_NM_ADDRESS_3 255
#define AP_NM_ADDRESS_4 0


// Structure used to hold router information
typedef struct
{
	// Indicates if router details are valid
	bool valid;

	// Router SSID (name)
	char ssid[MAX_SSID_STRING_LENGTH + 1];

	// Router password
	char password[MAX_PASSWORD_STRING_LENGTH + 1];

	// Signal strength, in terms of "bars" 0 to 4
	int32_t signal_strength;

	// Indicates if the network requires a password
	bool requires_password;
}
_TRouterDetails;


extern void initialise_wifi();
extern bool start_wifi(bool access_point);
extern uint8_t *get_mac_address();
extern bool is_router_known();
extern char *get_router_ssid();
extern bool is_ready_to_connect();
extern bool is_connected_to_router();
extern bool connection_attempt_failed();
extern bool is_access_point_available();
extern bool is_access_point_in_use();
extern bool connect_to_router();
extern bool pair_with_router();
extern bool get_rssi(int32_t *rssi);
extern void forget_router();
extern void change_router(char *ssid, char* password);
extern void scan_for_routers(_TRouterDetails routers[], int32_t *routers_found);


#endif // _WIFI_INTERFACE_H_

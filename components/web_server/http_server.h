/**
 * @file http_server.h
 * @brief Implements a simple HTTP server
*/
#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_

#include "common.h"
#include "wifi_interface.h"

// Action codes
#define ACTION_REMEMBER_ROUTER  0
#define ACTION_FORGET_ROUTER    1
#define ACTION_START_SCAN   1

// Structure defining an action requested by the HTTP server
typedef struct
{
	// Action code
	int32_t code;

	// Router SSID
	char ssid[MAX_SSID_STRING_LENGTH + 1];

	// Router password
	char password[MAX_PASSWORD_STRING_LENGTH + 1];
}
_THttpServerAction;

extern void initialise_http_server();
extern void start_http_server();
extern void stop_http_server();
extern bool is_http_server_running();
extern bool get_http_server_action(_THttpServerAction *action);

#endif // _HTTP_SERVER_H_

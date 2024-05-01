/**
 * @file http_client.h
 * @brief Provides facilities for connecting to a web server and sending HTTP messages
*/
#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

#include "common.h"

// Maximum length of a post or response message header
#define MAX_HEADER_LENGTH  400

// Maximum length of a post message
#define MAX_POST_MESSAGE_BODY_LENGTH 3000
#define MAX_POST_MESSAGE_LENGTH (MAX_HEADER_LENGTH + MAX_POST_MESSAGE_BODY_LENGTH)

#define MAX_RESPONSE_MESSAGE_BODY_LENGTH 3000
#define MAX_RESPONSE_MESSAGE_LENGTH (MAX_HEADER_LENGTH + MAX_POST_MESSAGE_BODY_LENGTH)

// HTTP status codes
#define HTTP_OK  200

extern bool resolve_host_address(char *host_name);
extern bool connect_to_server();
extern void disconnect_from_server();
extern bool post_http_message(char *post_message_body,
	    			          char *response_message_body,
							  int32_t *response_status_code);

#endif // _HTTP_CLIENT_H_

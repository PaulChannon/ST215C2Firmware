/**
 * @file http_utils.h
 * @brief Provides utilities for the HTTP server implementation
*/
#ifndef _HTTP_UTILS_H_
#define _HTTP_UTILS_H_

#include "esp_wifi.h"
#include "esp_http_server.h"
#include "common.h"

// HTTP message content types
#define CONTENT_UNKNOWN         0
#define CONTENT_FORM_ENCODED    1

extern esp_err_t extract_get_parameter(httpd_req_t *request, char *parameter_name, char *parameter_value, int32_t parameter_value_length);
extern esp_err_t extract_post_parameter(char *content, char *parameter_name, char *parameter_value, int32_t parameter_value_length, bool url_encoded);
extern esp_err_t extract_content_type(httpd_req_t *request, int32_t *content_type);
extern esp_err_t url_decode(char *encoded_message, char *message, int32_t max_message_length);
extern esp_err_t url_encode(char *message, char *encoded_message, int32_t max_encoded_message_length);


#endif // _HTTP_UTILS_H_

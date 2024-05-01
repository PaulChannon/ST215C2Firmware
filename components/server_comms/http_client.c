/**
 * @file http_client.c
 * @brief Provides facilities for connecting to a web server and sending HTTP messages
*/
#include "common.h"
#include "esp_wifi.h"
#include "esp_tls.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "wifi_interface.h"
#include "http_client.h"

// Controls debug output to the serial port:
//   0 - nothing output
//   1 - basic information and error
//   2 - verbose output
#define DEBUG_OUTPUT 1

#define DEBUG_VERBOSE(format, ...)  if (DEBUG_OUTPUT > 1) { ESP_LOGI(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_INFO(format, ...)     if (DEBUG_OUTPUT > 0) { ESP_LOGI(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_ERROR(format, ...)    if (DEBUG_OUTPUT > 0) { ESP_LOGE(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_DUMP(buffer, buff_len, level)  if (DEBUG_OUTPUT > 1) { ESP_LOG_BUFFER_HEXDUMP(MODULE_NAME, buffer, buff_len, level); }

// Maximum time for a response from the server (s)
#define RESPONSE_TIMEOUT_SECS  10

static const char *MODULE_NAME = "HTTP client";
static char _host_name[50] = "";
static struct addrinfo *_host_address_data = NULL;
static struct in_addr _host_address;
static char _host_address_string[20];
static bool _is_host_resolved = false;

#if USE_SSL

static struct esp_tls *_tls_connection;

#if CUSTOMER == ROHDE
    extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_rohde_pem_start");
    extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_rohde_pem_end");
#else
    extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_stafford_pem_start");
    extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_stafford_pem_end");
#endif

static bool send_ssl_message(char *message);
static bool receive_ssl_message(char *message, int32_t max_message_length, int32_t *message_length);

#else // USE_SSL

static int32_t _socket_id = -1;

#endif // USE_SSL

static char _post_message[MAX_POST_MESSAGE_LENGTH + 1];
static char _response_message[MAX_RESPONSE_MESSAGE_LENGTH + 1];

static char *decode_response(char *response_message, int32_t response_message_length, int32_t *status_code);


/**
 * @brief Uses DNS to resolve a host address
 * 
 * @param host_name Host name
 * 
 * @returns True if successful
*/
bool resolve_host_address(char *host_name)
{
    const struct addrinfo hints =
    {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    int32_t result;

	DEBUG_INFO("DNS lookup of server address");

    // Reset host information
    _is_host_resolved = false;
    if (_host_address_data == NULL)
	{
    	freeaddrinfo(_host_address_data);
    	_host_address_data = NULL;
	}

    // Store the host name
	strcpy(_host_name, host_name);

	// In debug mode, force a local server IP address
	#if DEBUG_SERVER
	strcpy(_host_name, DEBUG_SERVER_IP);
	#endif

	// Perform a DNS lookup to find the address of the web server
	result = getaddrinfo(_host_name, "80", &hints, &_host_address_data);
	if (result != 0 || _host_address_data == NULL)
	{
		DEBUG_ERROR("DNS lookup failed with error = %ld", result);
		return false;
	}

	// Extract the IP address of the web server
	_host_address = *(&((struct sockaddr_in *)_host_address_data->ai_addr)->sin_addr);
	strcpy(_host_address_string, inet_ntoa(_host_address));
	DEBUG_INFO("Host name %s", _host_name);
	DEBUG_INFO("Host IP address %s", _host_address_string);

    _is_host_resolved = true;

	return true;
}

#if USE_SSL
/**
 * @brief Connects to the server
 * 
 * @returns True if successful
*/
bool connect_to_server()
{
	DEBUG_INFO("Connecting to server %s, %s", _host_address_string, _host_name);

	// Resolve the host address if required
	if (!_is_host_resolved)
	{
		resolve_host_address(SERVER_ADDRESS);
	}

	// Create a TLS connection
	esp_tls_cfg_t configuration = {
        .cacert_pem_buf  = server_root_cert_pem_start,
        .cacert_pem_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
    };

	_tls_connection = esp_tls_init();
    if (_tls_connection == NULL)
    {
    	DEBUG_ERROR("Connection failed");
        return false;
    }

#if CUSTOMER == ROHDE
    esp_tls_conn_http_new_sync("https://app.rohde.eu", &configuration, _tls_connection);
#else
    esp_tls_conn_http_new_sync("https://www.kilnportal.co.uk", &configuration, _tls_connection);
#endif

    return true;
}

#else // USE_SSL

/**
 * @brief Connects to the server
 * 
 * @returns True if successful
*/
bool connect_to_server()
{
	int32_t result;

	// Resolve the host address if required
	if (!_is_host_resolved)
	{
		resolve_host_address(SERVER_ADDRESS);
	}

	DEBUG_INFO("Connecting to server");

    // Create a socket
    _socket_id = socket(_host_address_data->ai_family, _host_address_data->ai_socktype, 0);
    if (_socket_id < 0)
    {
        DEBUG_ERROR("Failed to allocate socket");
        return false;
    }

    // Connect with the socket
    result = connect(_socket_id, _host_address_data->ai_addr, _host_address_data->ai_addrlen);
    if (result != 0)
    {
        DEBUG_ERROR("Socket connection failed with error %ld", result);
        close(_socket_id);
        _socket_id = -1;
        return false;
    }

    return true;
}

#endif // USE_SSL

#if USE_SSL
/**
 * @brief Disconnects from the server
*/
void disconnect_from_server()
{
	DEBUG_INFO("Disconnecting from server");

	// Shut down the TLS connection
	esp_tls_conn_destroy(_tls_connection);
}
#else // USE_SSL
/**
 * @brief Disconnects from the server
*/
void disconnect_from_server()
{
	DEBUG_INFO("Disconnecting from server");

	// Close any open socket
	if (_socket_id >= 0)
	{
		close(_socket_id);
		_socket_id = -1;
	}
}
#endif // USE_SSL

#if USE_SSL
/**
 * @brief Posts a message to the server
 * 
 * @param post_message_body Message to send in the body
 * @param response_message_body Buffer supplied to hold the response message
 * @param response_status_code The HTTP status code in the response, e.g. 200 for HTTP_OK
 * 
 * @returns True if successful.  Note that the connection will remain open even if the post fails
*/
bool post_http_message(char *post_message_body,
	    			   char *response_message_body,
					   int32_t *response_status_code)
{
	char value_string[50];
    int32_t response_length;
    int32_t bytes_received;
    char *response_body;
    char *response_offset;

    DEBUG_INFO("Posting message %s", post_message_body);

	// Create an HTTP post message header
	sprintf(value_string, "Content-Length: %d\r\n", strlen(post_message_body));
	strcpy(_post_message, "POST /kiln_incoming/ HTTP/1.1\r\n");
	strcat(_post_message, "Host: ");
	strcat(_post_message, _host_name);
	strcat(_post_message, "\r\nContent-Type: application/json\r\n");
	strcat(_post_message, value_string);
	strcat(_post_message, "Connection: keep-alive\r\n" );
	strcat(_post_message, "\r\n" );

	// Append the message body
	strcat(_post_message, post_message_body );

	// Terminate the message
	strcat(_post_message, "\r\n" );

	// Send the message
	if (!send_ssl_message(_post_message))
	{
		return false;
	}

    // Wait for and read the response.  This can arrive in multiple parts
    response_offset = _response_message;
    response_length = 0;
    while (true)
    {
    	// Read a part of the response
    	if (!receive_ssl_message(response_offset, MAX_RESPONSE_MESSAGE_LENGTH - response_length, &bytes_received))
    	{
    		DEBUG_ERROR("No response");
    		return false;
    	}

		DEBUG_VERBOSE("Received %ld bytes", bytes_received);
		DEBUG_DUMP(response_offset, bytes_received, ESP_LOG_INFO);

		response_length += bytes_received;
		response_offset += bytes_received;

		// Attempt to decode the response
		response_body = decode_response(_response_message, response_length, response_status_code);
		if (response_body != NULL) break;
    }

	DEBUG_INFO("Response %s", response_body);

    // Copy the response body
    strcpy(response_message_body, response_body);

	return true;
}

/**
 * @brief Sends a message via a TLS connection
 * 
 * @param message Message to send
 * 
 * @returns True if the message was sent
*/
static bool send_ssl_message(char *message)
{
	int32_t result;
    size_t bytes_written = 0;
    int32_t message_length = strlen(message);

	// Send the message, splitting into chunks as required by the connection
    do
    {
        result = esp_tls_conn_write(_tls_connection, message + bytes_written, message_length - bytes_written);

        if (result >= 0)
        {
            bytes_written += result;
        }
        else if (result != MBEDTLS_ERR_SSL_WANT_READ  && result != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
    		DEBUG_INFO("Error sending message (%ld)", result);
    		return false;
        }
    }
    while (bytes_written < message_length);

    return true;
}

/**
 * @brief Receives a message via a TLS connection
 * 
 * @param message Returned message
 * @param max_message_length Maximum number of characters to receive
 * @param message_length Returned number of bytes received
 * 
 * @returns True if the message was received
*/
static bool receive_ssl_message(char *message, int32_t max_message_length, int32_t *message_length)
{
	int32_t result;

    while (1)
    {
    	// Clear the message buffer
        bzero(message, max_message_length);

        // Attempt to read the message
        result = esp_tls_conn_read(_tls_connection, message, max_message_length - 1);

        // Keep trying if there is no response
        if (result == MBEDTLS_ERR_SSL_WANT_WRITE  || result == MBEDTLS_ERR_SSL_WANT_READ)
      	{
        	continue;
       	}

        // Check for an error
        if (result <= 0)
        {
			DEBUG_ERROR("Error receiving response (%ld)", result);
            return false;
        }
        else
        {
        	*message_length = result;
        	return true;
        }
    }
}

#else // USE_SSL
/**
 * @brief Posts a message to the server
 * 
 * @param post_message_body Message to send in the body
 * @param response_message_body Buffer supplied to hold the response message
 * @param response_status_code The HTTP status code in the response, e.g. 200 for HTTP_OK
 * 
 * @returns True if successful.  Note that the connection will remain open even if the post fails
*/
bool post_http_message(char *post_message_body,
	    			   char *response_message_body,
					   int32_t *response_status_code)
{
	int32_t result;
	char value_string[50];
    size_t bytes_written = 0;
    int32_t message_length;
    int32_t response_length;
    int32_t bytes_received;
    char *response_body;
    char *response_offset;
    struct timeval receive_timeout;

    DEBUG_INFO("Posting message %s", post_message_body);

	// Create an HTTP post message header
	sprintf(value_string, "Content-Length: %d\r\n", strlen(post_message_body));
	strcpy(_post_message, "POST /kiln_incoming/ HTTP/1.1\r\n");
	strcat(_post_message, "Host: ");
	strcat(_post_message, _host_name);
	strcat(_post_message, "\r\nContent-Type: application/json\r\n");
	strcat(_post_message, value_string);
	strcat(_post_message, "Connection: keep-alive\r\n" );
	strcat(_post_message, "\r\n" );

	// Append the message body
	strcat(_post_message, post_message_body );

	// Terminate the message
	strcat(_post_message, "\r\n" );

    // Send the message, splitting into chunks as required by the connection
    message_length = strlen(_post_message);
    DEBUG_INFO("Sending message of length %ld", message_length);
    do
    {
        result = write(_socket_id, _post_message + bytes_written, message_length - bytes_written);
        DEBUG_INFO("Result = %ld", result);
        if (result < 0)
        {
            DEBUG_INFO("Error sending message (%ld)", result);
            return false;
        }

        bytes_written += result;
    }
    while (bytes_written < message_length);

    // Set the receive timeout on the socket
    receive_timeout.tv_sec = RESPONSE_TIMEOUT_SECS;
    receive_timeout.tv_usec = 0;
    if (setsockopt(_socket_id, SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout)) < 0)
    {
        DEBUG_ERROR("Failed to set socket receive timeout");
        return false;
    }

    // Wait for and read the response.  This can arrive in multiple parts
    response_offset = _response_message;
    response_length = 0;
    while (true)
    {
    	// Read a part of the response
		bytes_received = read(_socket_id, response_offset, MAX_RESPONSE_MESSAGE_LENGTH - response_length);
		if (bytes_received <= 0)
		{
			DEBUG_ERROR("Error receiving response, code %ld:", bytes_received);
			return false;
		}

		response_length += bytes_received;
		response_offset += bytes_received;

		// Attempt to decode the response
		response_body = decode_response(_response_message, response_length, response_status_code);
		if (response_body != NULL) break;
    }

    DEBUG_INFO("Response %s", response_body);

    // Copy the response body
    strcpy(response_message_body, response_body);

	return true;
}
#endif // USE_SSL

/**
 * @brief Decodes an HTTP response
 * 
 * @param response_message The message received from the socket
 * @param response_message_length The returned status code contained in the header
 * @param status_code The returned status code contained in the header
 * 
 * @returns The response body or NULL if the message is incomplete or corrupt
*/
static char *decode_response(char *response_message,
		                     int32_t response_message_length,
							 int32_t *status_code)
{
	char *header_terminator;
	char *status;
	char *header;
	char *body;
	char *content_length_keyword;
	int32_t content_length;
	int32_t header_length;
	int32_t body_length;

	*status_code = -1;

	DEBUG_VERBOSE("Received response length %ld:", response_message_length);
	DEBUG_DUMP(response_message, response_message_length, ESP_LOG_INFO);

	// Null terminate the buffer
	if (response_message_length <= 0 || response_message_length >= MAX_RESPONSE_MESSAGE_LENGTH + 1) return NULL;
	response_message[response_message_length] = 0;

	// Search for the <CR><LF> at the end of the header
	header = (char *)response_message;
	header_terminator = strstr(header, "\r\n\r\n");
	if (header_terminator == NULL) return NULL;

	// Check that the header starts with HTTP
	if (strncmp(header, "HTTP", 4) != 0) return NULL;

	// Extract the status code.  The header will have the form HTTP/1.1 200 OK, so the code
	// comes after the first space
	status = strstr(header, " ");
	if (status == NULL) return NULL;
	if (sscanf(status, "%ld", status_code) != 1) return NULL;

	DEBUG_VERBOSE("Received HTTP header with status %ld:", *status_code);

	// Return an empty body if the status code indicated non-success
	if (*status_code != HTTP_OK)
	{
		// Hijack the header to obtain an empty string
		*header = 0;
		return header;
	}

	// Extract the content length from the header
	content_length_keyword = strstr(header, "Content-Length:");
	if (content_length_keyword == NULL) return NULL;
	content_length = strtol(content_length_keyword + 15, NULL, 10);

	DEBUG_VERBOSE("HTTP header content length: %ld", content_length);

	// If the content length is zero, there is no message body
	if (content_length == 0)
	{
		// Hijack the header to obtain an empty string
		*header = 0;
		return header;
	}

	// Check if there is a body of the right length
	body = header_terminator + 4;
	header_length = body - header;
	body_length = response_message_length - header_length;

	DEBUG_VERBOSE("HTTP body length: %ld", body_length);

	if (body_length != content_length) return NULL;

	return body;
}


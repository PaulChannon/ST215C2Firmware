/**
 * @file http_utils.c
 * @brief Provides utilities for the HTTP server implementation
*/
#include "http_utils.h"

/**
 * @brief Extract a get parameter from a request query string
 * 
 * @param request HTTP request
 * @param parameter_name Name of the parameter to extract
 * @param parameter_value Returned parameter value
 * @param parameter_value_length Maximum length of the returned parameter value
 * 
 * @returns ESP_OK or an error code
*/
esp_err_t extract_get_parameter(httpd_req_t *request, char *parameter_name, char *parameter_value, int32_t parameter_value_length)
{
	esp_err_t result;
	char* query_string;
	int32_t query_string_length;
	char *encoded_value;

	// Get the query string length
	query_string_length = httpd_req_get_url_query_len(request) + 1;
	if (query_string_length == 0) return ESP_ERR_NOT_FOUND;

	// Get the query string
	query_string = malloc(query_string_length);
	if (query_string == NULL)
    {
	    return ESP_ERR_NO_MEM;
    }

	result = httpd_req_get_url_query_str(request, query_string, query_string_length);
	if (result != ESP_OK)
	{
		free(query_string);
		return ESP_ERR_NOT_FOUND;
	}

	// Extract the URL encoded parameter from within the query string
	encoded_value = malloc(query_string_length);
	if (encoded_value == NULL)
    {
        free(query_string);
	    return ESP_ERR_NO_MEM;
    }

	result = httpd_query_key_value(query_string, parameter_name, encoded_value, query_string_length);
	if (result != ESP_OK)
	{
		free(query_string);
		free(encoded_value);
        return ESP_ERR_NOT_FOUND;
	}

	// Decode the parameter value
	result = url_decode(encoded_value, parameter_value, parameter_value_length);

	free(query_string);
	free(encoded_value);

	return result;
}

/**
 * @brief Extracts a post parameter from a request body
 * 
 * @param content HTTP request content
 * @param parameter_name Name of the parameter to extract
 * @param parameter_value Returned parameter value
 * @param parameter_value_length Maximum length of the returned parameter value
 * @param url_encoded Indicates if the data is URL encoded
 * 
 * @returns ESP_OK or an error code
*/
esp_err_t extract_post_parameter(char *content, char *parameter_name, char *parameter_value, int32_t parameter_value_length, bool url_encoded)
{
	esp_err_t result;
	char *encoded_value;


	if (url_encoded)
	{
		// If URL encoded, read the encoded value
		encoded_value = (char *)malloc(parameter_value_length * 3);
		if (encoded_value == NULL) return ESP_ERR_NO_MEM;

		result = httpd_query_key_value(content, parameter_name, encoded_value, parameter_value_length * 3);
		if (result != ESP_OK)
		{
			free(encoded_value);
			return result;
		}

		// Decode the value
		result = url_decode(encoded_value, parameter_value, parameter_value_length);
		free(encoded_value);
		if (result != ESP_OK)
		{
			return result;
		}
	}
	else
	{
		result = httpd_query_key_value(content, parameter_name, parameter_value, parameter_value_length);
		if (result != ESP_OK) return result;
	}

	return ESP_OK;
}

/**
 * @brief Extract the content type from the HTTP header
 * 
 * @param request HTTP request
 * @param content_type Returned content type
 * 
 * @returns ESP_OK or an error code
*/
esp_err_t extract_content_type(httpd_req_t *request, int32_t *content_type)
{
	int32_t content_type_length;
	char *content_type_value;

	// Get the content type entry length
	content_type_length = httpd_req_get_hdr_value_len(request, "Content-Type") + 1;
	if (content_type_length == 0) return ESP_ERR_NOT_FOUND;

	// Get the content type
	content_type_value = (char *)malloc(content_type_length);
	if (content_type_value == NULL) return ESP_ERR_NO_MEM;

	if (httpd_req_get_hdr_value_str(request, "Content-Type", content_type_value, content_type_length) != ESP_OK)
	{
		free(content_type_value);
		return ESP_ERR_NOT_FOUND;
	}

	// Decode the content type
	if (strcmp(content_type_value, "application/x-www-form-urlencoded") == 0)
	{
		*content_type = CONTENT_FORM_ENCODED;
	}
	else
	{
		*content_type = CONTENT_UNKNOWN;
	}

	free(content_type_value);

	return ESP_OK;
}

/**
 * @brief Decodes a url-encoded message
 * 
 * @param encoded_message Message to URL decode
 * @param message Returned decoded message
 * @param max_message_length Maximum length of the decoded message
 * 
 * @returns ESP_OK or an error code
*/
esp_err_t url_decode(char *encoded_message, char *message, int32_t max_message_length)
{
	int32_t encoded_message_index = 0;
	int32_t message_index = 0;
	char high_nibble;
	char low_nibble;
	char ch;

	while (1)
	{
		ch = encoded_message[encoded_message_index++];
		if (ch == '\0') break;

		if (message_index >= max_message_length) return ESP_ERR_INVALID_SIZE;

		if (ch == '%')
		{
			ch = encoded_message[encoded_message_index++];
			high_nibble = isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;

			ch = encoded_message[encoded_message_index++];
			low_nibble = isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;

			message[message_index++] = (high_nibble << 4) | low_nibble;
		}
		else if (ch == '+')
		{
			message[message_index++] = ' ';
		}
		else
		{
			message[message_index++] = ch;
		}
	}

	message[message_index] = '\0';

	return ESP_OK;
}

/**
 * @brief URL encodes a message
 * 
 * @param message Message to URL encode
 * @param encoded_message Returned encoded message
 * @param max_encoded_message_length Maximum length of the encoded message
 * 
 * @returns ESP_OK or an error code
*/
esp_err_t url_encode(char *message, char *encoded_message, int32_t max_encoded_message_length)
{
	int32_t encoded_message_index = 0;
	int32_t message_index = 0;
	char ch;
	char encoded_ch;

	while (1)
	{
		ch = message[message_index++];
		if (ch == '\0') break;

		if (max_encoded_message_length - encoded_message_index < 3) return ESP_ERR_INVALID_SIZE;

		if (isalpha(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~')
		{
			encoded_message[encoded_message_index++] = ch;
		}
		else
		{
			encoded_message[encoded_message_index++] = '%';

			encoded_ch = (ch >> 4) & 0x0F;
			encoded_message[encoded_message_index++] = (encoded_ch <= 9 ? encoded_ch + '0' : encoded_ch + 'a' - 10);

			encoded_ch = ch & 0x0F;
			encoded_message[encoded_message_index++] = (encoded_ch <= 9 ? encoded_ch + '0' : encoded_ch + 'a' - 10);
		}
	}

	encoded_message[encoded_message_index] = '\0';

	return ESP_OK;
}




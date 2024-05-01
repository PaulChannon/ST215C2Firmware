/**
 * @file http_server.c
 * @brief Implements a simple HTTP server
*/
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "common.h"
#include "controller.h"
#include "context_dictionary.h"
#include "template_renderer.h"
#include "http_utils.h"
#include "http_server.h"

// Controls debug output to the serial port:
//   0 - nothing output
//   1 - basic information and error
//   2 - verbose output
#define DEBUG_OUTPUT 1

#define DEBUG_VERBOSE(format, ...)  if (DEBUG_OUTPUT > 1) { ESP_LOGI(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_INFO(format, ...)     if (DEBUG_OUTPUT > 0) { ESP_LOGI(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_ERROR(format, ...)    if (DEBUG_OUTPUT > 0) { ESP_LOGE(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_DUMP(buffer, buff_len, level)  if (DEBUG_OUTPUT > 1) { ESP_LOG_BUFFER_HEXDUMP(MODULE_NAME, buffer, buff_len, level); }

static const char *MODULE_NAME = "HTTP server";
static httpd_handle_t _server = NULL;
static bool _server_running = false;

// Web page handler functions
static esp_err_t home_get_handler(httpd_req_t *request);
static esp_err_t home_post_handler(httpd_req_t *request);
static esp_err_t routers_get_handler(httpd_req_t *request);
static esp_err_t scan_get_handler(httpd_req_t *request);
static esp_err_t connect_get_handler(httpd_req_t *request);
static esp_err_t connect_post_handler(httpd_req_t *request);
static esp_err_t connect_result_get_handler(httpd_req_t *request);
static esp_err_t connect_result_post_handler(httpd_req_t *request);
static esp_err_t diagnostics_get_handler(httpd_req_t *request);
static esp_err_t diagnostics_post_handler(httpd_req_t *request);
static esp_err_t connection_status_get_handler(httpd_req_t *request);
static esp_err_t controller_status_get_handler(httpd_req_t *request);
static esp_err_t controller_settings_get_handler(httpd_req_t *request);
static esp_err_t base_get_handler(httpd_req_t *request);
static esp_err_t jquery_get_handler(httpd_req_t *request);
static esp_err_t spin_get_handler(httpd_req_t *request);
static esp_err_t wifi_0_locked_get_handler(httpd_req_t *request);
static esp_err_t wifi_1_locked_get_handler(httpd_req_t *request);
static esp_err_t wifi_2_locked_get_handler(httpd_req_t *request);
static esp_err_t wifi_3_locked_get_handler(httpd_req_t *request);
static esp_err_t wifi_4_locked_get_handler(httpd_req_t *request);
static esp_err_t wifi_0_unlocked_get_handler(httpd_req_t *request);
static esp_err_t wifi_1_unlocked_get_handler(httpd_req_t *request);
static esp_err_t wifi_2_unlocked_get_handler(httpd_req_t *request);
static esp_err_t wifi_3_unlocked_get_handler(httpd_req_t *request);
static esp_err_t wifi_4_unlocked_get_handler(httpd_req_t *request);
static esp_err_t generate_204_get_handler(httpd_req_t *request);
static esp_err_t connect_test_get_handler(httpd_req_t *request);
static esp_err_t redirect_get_handler(httpd_req_t *request);
static esp_err_t hotspot_get_handler(httpd_req_t *request);

static char *create_icon_url(int32_t bars, bool secure);

// Web pages embedded in flash memory
extern const uint8_t _home_html[] asm("_binary_home_html_start");
extern const uint8_t _scan_html[] asm("_binary_scan_html_start");
extern const uint8_t _connect_html[] asm("_binary_connect_html_start");
extern const uint8_t _connect_request_ssid_html[] asm("_binary_connect_request_ssid_html_start");
extern const uint8_t _connect_result_html[] asm("_binary_connect_result_html_start");
extern const uint8_t _diagnostics_html[] asm("_binary_diagnostics_html_start");
extern const uint8_t _base_css[] asm("_binary_base_css_start");
extern const uint8_t _jquery_js[] asm("_binary_jquery_js_start");
extern const uint8_t _spin_js[] asm("_binary_spin_js_start");
extern const uint8_t _wifi_0_locked_png_start[] asm("_binary_wifi_0_locked_png_start");
extern const uint8_t _wifi_0_locked_png_end[] asm("_binary_wifi_0_locked_png_end");
extern const uint8_t _wifi_1_locked_png_start[] asm("_binary_wifi_1_locked_png_start");
extern const uint8_t _wifi_1_locked_png_end[] asm("_binary_wifi_1_locked_png_end");
extern const uint8_t _wifi_2_locked_png_start[] asm("_binary_wifi_2_locked_png_start");
extern const uint8_t _wifi_2_locked_png_end[] asm("_binary_wifi_2_locked_png_end");
extern const uint8_t _wifi_3_locked_png_start[] asm("_binary_wifi_3_locked_png_start");
extern const uint8_t _wifi_3_locked_png_end[] asm("_binary_wifi_3_locked_png_end");
extern const uint8_t _wifi_4_locked_png_start[] asm("_binary_wifi_4_locked_png_start");
extern const uint8_t _wifi_4_locked_png_end[] asm("_binary_wifi_4_locked_png_end");
extern const uint8_t _wifi_0_unlocked_png_start[] asm("_binary_wifi_0_unlocked_png_start");
extern const uint8_t _wifi_0_unlocked_png_end[] asm("_binary_wifi_0_unlocked_png_end");
extern const uint8_t _wifi_1_unlocked_png_start[] asm("_binary_wifi_1_unlocked_png_start");
extern const uint8_t _wifi_1_unlocked_png_end[] asm("_binary_wifi_1_unlocked_png_end");
extern const uint8_t _wifi_2_unlocked_png_start[] asm("_binary_wifi_2_unlocked_png_start");
extern const uint8_t _wifi_2_unlocked_png_end[] asm("_binary_wifi_2_unlocked_png_end");
extern const uint8_t _wifi_3_unlocked_png_start[] asm("_binary_wifi_3_unlocked_png_start");
extern const uint8_t _wifi_3_unlocked_png_end[] asm("_binary_wifi_3_unlocked_png_end");
extern const uint8_t _wifi_4_unlocked_png_start[] asm("_binary_wifi_4_unlocked_png_start");
extern const uint8_t _wifi_4_unlocked_png_end[] asm("_binary_wifi_4_unlocked_png_end");

// URI definitions
static httpd_uri_t _root_get_uri =
{
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = home_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _root_post_uri =
{
    .uri       = "/",
    .method    = HTTP_POST,
    .handler   = home_post_handler,
    .user_ctx  = ""
};

static httpd_uri_t _home_get_uri =
{
    .uri       = "/home",
    .method    = HTTP_GET,
    .handler   = home_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _home_post_uri =
{
    .uri       = "/home",
    .method    = HTTP_POST,
    .handler   = home_post_handler,
    .user_ctx  = ""
};

static httpd_uri_t _routers_get_uri =
{
    .uri       = "/routers",
    .method    = HTTP_GET,
    .handler   = routers_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _scan_get_uri =
{
    .uri       = "/scan",
    .method    = HTTP_GET,
    .handler   = scan_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _connect_get_uri =
{
    .uri       = "/connect",
    .method    = HTTP_GET,
    .handler   = connect_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _connect_post_uri =
{
    .uri       = "/connect",
    .method    = HTTP_POST,
    .handler   = connect_post_handler,
    .user_ctx  = ""
};

static httpd_uri_t _connect_result_get_uri =
{
    .uri       = "/connect_result",
    .method    = HTTP_GET,
    .handler   = connect_result_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _connect_result_post_uri =
{
    .uri       = "/connect_result",
    .method    = HTTP_POST,
    .handler   = connect_result_post_handler,
    .user_ctx  = ""
};

static httpd_uri_t _diagnostics_get_uri =
{
    .uri       = "/diagnostics",
    .method    = HTTP_GET,
    .handler   = diagnostics_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _diagnostics_post_uri =
{
    .uri       = "/diagnostics",
    .method    = HTTP_POST,
    .handler   = diagnostics_post_handler,
    .user_ctx  = ""
};

static httpd_uri_t _connection_status_get_uri =
{
    .uri       = "/connection_status",
    .method    = HTTP_GET,
    .handler   = connection_status_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _controller_status_get_uri =
{
    .uri       = "/controller_status",
    .method    = HTTP_GET,
    .handler   = controller_status_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _controller_settings_get_uri =
{
    .uri       = "/controller_settings",
    .method    = HTTP_GET,
    .handler   = controller_settings_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _base_uri =
{
    .uri       = "/base.css",
    .method    = HTTP_GET,
    .handler   = base_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _jquery_uri =
{
    .uri       = "/jquery.js",
    .method    = HTTP_GET,
    .handler   = jquery_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _spin_uri =
{
    .uri       = "/spin.js",
    .method    = HTTP_GET,
    .handler   = spin_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _wifi_0_locked_uri =
{
    .uri       = "/wifi_0_locked.png",
    .method    = HTTP_GET,
    .handler   = wifi_0_locked_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _wifi_1_locked_uri =
{
    .uri       = "/wifi_1_locked.png",
    .method    = HTTP_GET,
    .handler   = wifi_1_locked_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _wifi_2_locked_uri =
{
    .uri       = "/wifi_2_locked.png",
    .method    = HTTP_GET,
    .handler   = wifi_2_locked_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _wifi_3_locked_uri =
{
    .uri       = "/wifi_3_locked.png",
    .method    = HTTP_GET,
    .handler   = wifi_3_locked_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _wifi_4_locked_uri =
{
    .uri       = "/wifi_4_locked.png",
    .method    = HTTP_GET,
    .handler   = wifi_4_locked_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _wifi_0_unlocked_uri =
{
    .uri       = "/wifi_0_unlocked.png",
    .method    = HTTP_GET,
    .handler   = wifi_0_unlocked_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _wifi_1_unlocked_uri =
{
    .uri       = "/wifi_1_unlocked.png",
    .method    = HTTP_GET,
    .handler   = wifi_1_unlocked_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _wifi_2_unlocked_uri =
{
    .uri       = "/wifi_2_unlocked.png",
    .method    = HTTP_GET,
    .handler   = wifi_2_unlocked_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _wifi_3_unlocked_uri =
{
    .uri       = "/wifi_3_unlocked.png",
    .method    = HTTP_GET,
    .handler   = wifi_3_unlocked_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _wifi_4_unlocked_uri =
{
    .uri       = "/wifi_4_unlocked.png",
    .method    = HTTP_GET,
    .handler   = wifi_4_unlocked_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _generate_204_get_uri =
{
    .uri       = "/generate_204",
    .method    = HTTP_GET,
    .handler   = generate_204_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _connect_test_get_uri =
{
    .uri       = "/connecttest.txt",
    .method    = HTTP_GET,
    .handler   = connect_test_get_handler,
    .user_ctx  = ""
};

static httpd_uri_t _redirect_get_uri =
{
    .uri       = "/redirect",
    .method    = HTTP_GET,
    .handler   = redirect_get_handler,
    .user_ctx  = ""
};


static httpd_uri_t _hotspot_get_uri =
{
    .uri       = "/hotspot-detect.html",
    .method    = HTTP_GET,
    .handler   = hotspot_get_handler,
    .user_ctx  = ""
};



/**
 * @brief Initialises the HTTP server
*/
void initialise_http_server()
{
	_server_running = false;
}

/**
 * @brief Starts the HTTP server
*/
void start_http_server()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 40;
    config.stack_size = 16384;

    // Start the httpd server
    config.lru_purge_enable = true;
    DEBUG_INFO("Starting server on port %d", config.server_port);
    if (httpd_start(&_server, &config) != ESP_OK)
    {
    	DEBUG_ERROR("Cannot create HTTP server");
    	return;
    }

	// Register URI handlers
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_root_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_root_post_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_home_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_home_post_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_routers_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_scan_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_connect_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_connect_post_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_connect_result_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_connect_result_post_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_diagnostics_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_diagnostics_post_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_connection_status_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_controller_status_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_controller_settings_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_base_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_jquery_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_spin_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_wifi_0_locked_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_wifi_1_locked_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_wifi_2_locked_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_wifi_3_locked_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_wifi_4_locked_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_wifi_0_unlocked_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_wifi_1_unlocked_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_wifi_2_unlocked_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_wifi_3_unlocked_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_wifi_4_unlocked_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_generate_204_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_connect_test_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_redirect_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(_server, &_hotspot_get_uri));


    _server_running = true;
}

/**
 * @brief Stops the HTTP server
*/
void stop_http_server()
{
    if (_server != NULL)
    {
    	httpd_stop(_server);
    	_server = NULL;
    }

    _server_running = false;
}

/**
 * @brief Indicates if the HTTP server is running
 * 
 * @returns True if running
*/
bool is_http_server_running()
{
	return _server_running;
}

/**
 * @brief Serves up the home page
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t home_get_handler(httpd_req_t *request)
{
	esp_err_t result;
	_TContextDictionary *context_dictionary;
	char *response;
	char router_ssid[MAX_SSID_STRING_LENGTH + 1] = "";
	bool router_connected = false;
	bool router_known = false;

	DEBUG_VERBOSE("Get home.html");

	// Create a context dictionary
	context_dictionary = create_context_dictionary();
	if (context_dictionary == NULL)
	{
		return ESP_ERR_NO_MEM;
	}

	// Obtain connection information
    if (is_router_known())
    {
    	strcpy(router_ssid, get_router_ssid());
    	router_known = true;
    	router_connected = is_connected_to_router();
    }

	// Add context data
	if (!add_context_dictionary_entry(context_dictionary, "router_known", router_known ? "true": "false") ||
		!add_context_dictionary_entry(context_dictionary, "router_connected", router_connected ? "true": "false") ||
		!add_context_dictionary_entry(context_dictionary, "router_ssid", router_ssid))
	{
		delete_context_dictionary(context_dictionary);
		return ESP_ERR_NO_MEM;
	}

	// Render the template
	response = render_template((char *)_home_html, context_dictionary);
	if (response == NULL) return ESP_ERR_NO_MEM;

    // Send a response
    result = httpd_resp_send(request, response, strlen(response));

    delete_context_dictionary(context_dictionary);
    free(response);

    return result;
}

/**
 * @brief Serves up the home page
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t home_post_handler(httpd_req_t *request)
{
	esp_err_t result;

	DEBUG_VERBOSE("POST home.html");

	// Forget the current router
	forget_router();

	// Redirect back to the home page
	httpd_resp_set_status(request, "302 Found");
	httpd_resp_set_hdr(request, "Location", AP_URL);
    result = httpd_resp_sendstr(request, "Redirect");

    return result;
}

/**
 * @brief Serves up the connection page
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t connect_get_handler(httpd_req_t *request)
{
	esp_err_t result;
	_TContextDictionary *context_dictionary;
	bool ssid_available = false;
    char ssid[50];
    char password_required[50];
	char *response;

	DEBUG_VERBOSE("GET connect.html");

	// Extract SSID and password required flag from the query string
	result = extract_get_parameter(request, "ssid", ssid, sizeof(ssid));
	if (result == ESP_OK)
	{
	    ssid_available = true;
	    DEBUG_INFO("SSID = %s", ssid);
	}

	result = extract_get_parameter(request, "password_required", password_required, sizeof(password_required));
	if (result != ESP_OK) return result;
	DEBUG_INFO("Password required = %s", password_required);

	// Create a context dictionary
	context_dictionary = create_context_dictionary();
	if (context_dictionary == NULL) return ESP_ERR_NO_MEM;

	if (ssid_available)
    {
	    if (!add_context_dictionary_entry(context_dictionary, "ssid", ssid))
	    {
	        delete_context_dictionary(context_dictionary);
	        return ESP_ERR_NO_MEM;
	    }
	}

    if (!add_context_dictionary_entry(context_dictionary, "password_required", password_required))
	{
		delete_context_dictionary(context_dictionary);
		return ESP_ERR_NO_MEM;
	}

	// Render the appropriate template
    if (ssid_available)
    {
        response = render_template((char *)_connect_html, context_dictionary);
    }
    else
    {
        response = render_template((char *)_connect_request_ssid_html, context_dictionary);
    }
	if (response == NULL) return ESP_ERR_NO_MEM;

    // Send a response
    result = httpd_resp_send(request, response, strlen(response));

    delete_context_dictionary(context_dictionary);
    free(response);

    return result;
}

/**
 * @brief Handles a post from the connect page
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t connect_post_handler(httpd_req_t *request)
{
    char *content;
    int32_t result;
    char password[50];
    char ssid[50];
    char encoded_ssid[100];
	char url[150];
	int32_t content_type;

	DEBUG_VERBOSE("POST connection.html");

	// Read the content type from the header
	result = extract_content_type(request, &content_type);
	if (result != ESP_OK) return result;

	// Allocate memory for the content of the response
    content = (char *)malloc(request->content_len);
    if (content == NULL)
   	{
    	return ESP_ERR_NO_MEM;
   	}

    // Read the content from the response
    result = httpd_req_recv(request, content, request->content_len);
    if (result <= 0)
    {
    	// Check if a timeout occurred
        if (result == HTTPD_SOCK_ERR_TIMEOUT)
        {
            httpd_resp_send_408(request);
        }

    	free(content);
        return ESP_FAIL;
    }

    // Extract the password from the POST parameters
    result = extract_post_parameter(content, "password", password, sizeof(password), content_type == CONTENT_FORM_ENCODED);
    if (result != ESP_OK)
    {
    	free(content);
    	return result;
    }

    // Extract the SSID from the POST parameters
    result = extract_post_parameter(content, "ssid", ssid, sizeof(ssid), content_type == CONTENT_FORM_ENCODED);
    if (result != ESP_OK)
    {
    	free(content);
    	return result;
    }

	free(content);

	// Store router information
	change_router(ssid, password);

	// URL encode the SSID again
	result = url_encode(ssid, encoded_ssid, sizeof(encoded_ssid));
    if (result != ESP_OK) return result;

	// Redirect to the connection result page
	httpd_resp_set_status(request, "302 Found");
	sprintf(url, "%s/connect_result?ssid=%s", AP_URL, encoded_ssid);
	httpd_resp_set_hdr(request, "Location", url);
    result = httpd_resp_sendstr(request, "Redirect");

    return result;
}

/**
 * @brief Serves up the scan page
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t scan_get_handler(httpd_req_t *request)
{
    esp_err_t result;
    char *response;

    DEBUG_VERBOSE("GET scan.html");

    // Send a response
    response = (char *)_scan_html;
    result = httpd_resp_send(request, response, strlen(response));

    return result;
}

/**
 * @brief Serves up the connection results page
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t connect_result_get_handler(httpd_req_t *request)
{
	esp_err_t result;
	_TContextDictionary *context_dictionary;
    char ssid[50];
	char *response;

	DEBUG_VERBOSE("GET connect_result.html");

	// Extract SSID from the query string
	result = extract_get_parameter(request, "ssid", ssid, sizeof(ssid));
	if (result != ESP_OK) return result;
	DEBUG_INFO("SSID = %s", ssid);

	// Create a context dictionary
	context_dictionary = create_context_dictionary();
	if (context_dictionary == NULL)
	{
		return ESP_ERR_NO_MEM;
	}

	// Add controller name as the title
	if (!add_context_dictionary_entry(context_dictionary, "ssid", ssid))
	{
		delete_context_dictionary(context_dictionary);
		return ESP_ERR_NO_MEM;
	}

	// Render the template
	response = render_template((char *)_connect_result_html, context_dictionary);
	free(context_dictionary);
	if (response == NULL) return ESP_ERR_NO_MEM;

    // Send a response
    result = httpd_resp_send(request, response, strlen(response));
    free(response);

    return result;
}

/**
 * @brief Handles a post from the connect result page
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t connect_result_post_handler(httpd_req_t *request)
{
    int32_t result;

	DEBUG_VERBOSE("POST connect_result.html");

	// If connection was unsuccessful, forget the supplied router details
	if (!is_connected_to_router())
	{
		forget_router();
	}

	// Redirect back to the home page
	httpd_resp_set_status(request, "302 Found");
	httpd_resp_set_hdr(request, "Location", AP_URL);
    result = httpd_resp_sendstr(request, "Redirect");

    return result;
}

/**
 * @brief Serves up the diagnostics page
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t diagnostics_get_handler(httpd_req_t *request)
{
    char* buffer;

	DEBUG_VERBOSE("GET diagnostics.html");

	// Send a response
    buffer = (char *)_diagnostics_html;
    httpd_resp_send(request, buffer, strlen(buffer));

    return ESP_OK;
}

/**
 * @brief Handles a post from the diagnostics page
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t diagnostics_post_handler(httpd_req_t *request)
{
	esp_err_t result;
	char url[50];

	DEBUG_VERBOSE("POST diagnostics.html");

	// Reset the PIC controller
	reset_controller();

	// Redirect back to the diagnostics page
	httpd_resp_set_status(request, "302 Found");
	sprintf(url, "%s/diagnostics", AP_URL);
	httpd_resp_set_hdr(request, "Location", url);
    result = httpd_resp_sendstr(request, "Redirect");

    return result;
}

/**
 * @brief Ajax get handler for returning a list of routers in the vicinity
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t routers_get_handler(httpd_req_t *request)
{
	esp_err_t result;
    _TRouterDetails routers[MAX_ROUTERS];
    _TRouterDetails *router;
    int32_t routers_found;
	int32_t index;
	char *response;
	char *current_router_ssid = NULL;
	_TRouterDetails *current_router = NULL;
	bool first_router = true;

	DEBUG_VERBOSE("GET routers.html");

	// Scan for nearby routers
	DEBUG_VERBOSE("Scanning for routers");
    scan_for_routers(routers, &routers_found);
	DEBUG_VERBOSE("Scan complete");

    // Check if we are currently connected to a router
    if (is_router_known())
    {
    	current_router_ssid = get_router_ssid();

    	// See if this router is one of the scanned routers
    	for (index = 0; index < routers_found; index++)
    	{
        	router = &routers[index];
    		if (strcmp(current_router_ssid, router->ssid) == 0)
    		{
    			current_router = router;
    			break;
    		}
    	}
    }

    // Create a JSON response
    response = (char *)malloc(2000);
	if (response == NULL) return ESP_ERR_NO_MEM;

	strcpy(response, "{");

	// Add the current router if known
	if (is_router_known())
	{
		strcat(response, "\"current_router\": {");

		strcat(response, "\"ssid\": \"");
		strcat(response, current_router_ssid);
		strcat(response, "\",");

		strcat(response, "\"image_url\": \"");
		if (current_router == NULL)
		{
			strcat(response, create_icon_url(0, false));
		}
		else
		{
			DEBUG_VERBOSE("Signal strength = %ld", current_router->signal_strength);
			strcat(response, create_icon_url(current_router->signal_strength, current_router->requires_password));
		}
		strcat(response, "\"");

		strcat(response, "},");
	}

	// Add a list of available routers
	strcat(response, "\"available_routers\": [");

	for (index = 0; index < routers_found; index++)
	{
    	router = &routers[index];
    	if (router == current_router) continue;

    	if (first_router)
   		{
    		first_router = false;
   		}
    	else
    	{
    		strcat(response, ",");
    	}

    	strcat(response, "{");

    	strcat(response, "\"ssid\": \"");
    	strcat(response, router->ssid);
    	strcat(response, "\",");

    	strcat(response, "\"password_required\": \"");
    	strcat(response, router->requires_password ? "true" : "false");
    	strcat(response, "\",");

    	strcat(response, "\"image_url\": \"");
    	strcat(response, create_icon_url(router->signal_strength, router->requires_password));
    	strcat(response, "\"");

    	strcat(response, "}");
	}

	strcat(response, "]");

	strcat(response, "}");

	DEBUG_VERBOSE("Response %s", response);

    // Send the response
    result = httpd_resp_send(request, response, strlen(response));
    free(response);

    return result;
}

/**
 * @brief Ajax get handler for returning connection status
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t connection_status_get_handler(httpd_req_t *request)
{
	esp_err_t result;
	char *response;

	DEBUG_VERBOSE("GET connection_status.html");

    // Create a JSON response
    response = (char *)malloc(1000);
	if (response == NULL) return ESP_ERR_NO_MEM;

	strcpy(response, "{");

	// Add router details
	if (is_router_known())
	{
		strcat(response, "\"router_known\": true,");

		strcat(response, "\"router_ssid\": \"");
		strcat(response, get_router_ssid());
		strcat(response, "\",");

		if (is_connected_to_router())
		{
			strcat(response, "\"router_connected\": true");
		}
		else
		{
			strcat(response, "\"router_connected\": false,");

			if (connection_attempt_failed())
			{
				strcat(response, "\"router_connection_error\": true");

				// Forget the stored router information
//				forget_router();
			}
			else
			{
				strcat(response, "\"router_connection_error\": false");
			}
		}
	}
	else
	{
		strcat(response, "\"router_known\": false");
	}

	strcat(response, "}");

	DEBUG_VERBOSE("Response %s", response);

    // Send the response
    result = httpd_resp_send(request, response, strlen(response));
    free(response);

    return result;
}

/**
 * @brief Ajax get handler for returning controller status
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t controller_status_get_handler(httpd_req_t *request)
{
	esp_err_t result;
	char *response;
	char line[100];
	char temperature_units[10];

	DEBUG_VERBOSE("GET controller_status.html");

    // Create a JSON response
    response = (char *)malloc(2000);
	if (response == NULL) return ESP_ERR_NO_MEM;

	strcpy(response, "{");

	// Add current RTC date/time
	strcat(response, "\"date_time\": \"");
	sprintf(line, "%02d/%02d/%02d %02d:%02d:%02d",
			_controller.status.day,
			_controller.status.month,
			_controller.status.year,
			_controller.status.hour,
			_controller.status.minute,
			_controller.status.second);
	strcat(response, line);
	strcat(response, "\",");

	// Add firing state
	strcat(response, "\"firing_state\": \"");
	switch (_controller.status.firing_state)
	{
	default:
	case FIRING_STATE_INITIALISING:
		strcat(response, "Initialising");
		break;

	case FIRING_STATE_IDLE:
		strcat(response, "Idle");
		break;

	case FIRING_STATE_DELAY:
		strcat(response, "Delay");
		break;

	case FIRING_STATE_RAMP_HEATING:
		strcat(response, "Heating ramp");
		break;

	case FIRING_STATE_RAMP_HEATING_PAUSED:
		strcat(response, "Paused during heating ramp");
		break;

	case FIRING_STATE_RAMP_COOLING:
		strcat(response, "Cooling ramp");
		break;

	case FIRING_STATE_RAMP_COOLING_PAUSED:
		strcat(response, "Paused during cooling ramp");
		break;

	case FIRING_STATE_SOAK:
		strcat(response, "Soaking");
		break;

	case FIRING_STATE_SOAK_PAUSED:
		strcat(response, "Paused during soaking");
		break;

	case FIRING_STATE_COOLING:
		strcat(response, "Cooling");
		break;

	case FIRING_STATE_COOL:
		strcat(response, "Cool");
		break;

	case FIRING_STATE_ERROR:
		sprintf(line, "Error %d", _controller.status.error_code);
		strcat(response, "Error");
		break;

	case FIRING_STATE_SETUP:
		strcat(response, "Setup mode");
		break;

	case FIRING_STATE_POWER_FAIL:
		strcat(response, "Power fail");
		break;

	case FIRING_STATE_PAIRING:
		strcat(response, "Pairing mode");
		break;

	case FIRING_STATE_AP:
		strcat(response, "Access-point mode");
		break;
	}
	strcat(response, "\",");

	// Ambient temperature
	if (_controller.configuration.is_fahrenheit_units)
	{
		strcpy(temperature_units, "&degF");
	}
	else
	{
		strcpy(temperature_units, "&degC");
	}

	strcat(response, "\"ambient\": \"");
	sprintf(line, "%.1f %s", _controller.status.ambient_temperature, temperature_units);
	strcat(response, line);
	strcat(response, "\",");

	// Temperature
	strcat(response, "\"temperature\": \"");
	sprintf(line, "%.1f %s", _controller.status.temperature_1, temperature_units);
	strcat(response, line);
	strcat(response, "\",");

	// Temperature set-point
	strcat(response, "\"temperature_set_point\": \"");
	sprintf(line, "%.1f %s", _controller.status.temperature_set_point_1, temperature_units);
	strcat(response, line);
	strcat(response, "\",");

	// Total duty
	strcat(response, "\"total_duty\": \"");
	sprintf(line, "%.1f %%", _controller.status.total_duty_1);
	strcat(response, line);
	strcat(response, "\",");

	// Proportional term duty
	strcat(response, "\"prop_duty\": \"");
	sprintf(line, "%.1f %%", _controller.status.prop_duty_1);
	strcat(response, line);
	strcat(response, "\",");

	// Proportional term duty
	strcat(response, "\"int_duty\": \"");
	sprintf(line, "%.1f %%", _controller.status.int_duty_1);
	strcat(response, line);
	strcat(response, "\",");

	// Proportional term duty
	strcat(response, "\"deriv_duty\": \"");
	sprintf(line, "%.1f %%", _controller.status.diff_duty_1);
	strcat(response, line);
	strcat(response, "\",");

	// Energy
	strcat(response, "\"energy\": \"");
	sprintf(line, "%.1f kWh", _controller.status.energy_used_1);
	strcat(response, line);
	strcat(response, "\",");

	// Program number
	strcat(response, "\"program\": \"");
	sprintf(line, "%d", _controller.status.current_program);
	strcat(response, line);
	strcat(response, "\",");

	// Segment number
	strcat(response, "\"segment\": \"");
	sprintf(line, "%d", _controller.status.current_segment);
	strcat(response, line);
	strcat(response, "\",");

	// Start delay
	strcat(response, "\"start_delay\": \"");
	sprintf(line, "%d minutes", _controller.status.start_delay);
	strcat(response, line);
	strcat(response, "\",");

	// Start delay remaining
	strcat(response, "\"start_delay_remaining\": \"");
	sprintf(line, "%d minutes", _controller.status.start_delay_remaining);
	strcat(response, line);
	strcat(response, "\"");

	strcat(response, "}");

	DEBUG_VERBOSE("Response %s", response);

    // Send the response
    result = httpd_resp_send(request, response, strlen(response));
    free(response);

    return result;
}

/**
 * @brief Ajax get handler for returning controller settings
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t controller_settings_get_handler(httpd_req_t *request)
{
	esp_err_t result;
	char *response;
	char line[100];

	DEBUG_VERBOSE("GET controller_settings.html");

    // Create a JSON response
    response = (char *)malloc(2000);
	if (response == NULL) return ESP_ERR_NO_MEM;

	strcpy(response, "{");

	// PIC firmware version
	strcat(response, "\"main_firmware_version\": \"");
	strcat(response, _controller.configuration.pic_firmware_version);
	strcat(response, "\",");

	// STM32 firmware version
	strcat(response, "\"daughter_board_firmware_version\": \"");
	strcat(response, _controller.configuration.stm32_firmware_version);
	strcat(response, "\",");

	// ESP32 firmware version
	strcat(response, "\"comms_firmware_version\": \"");
	strcat(response, FIRMWARE_VERSION);
	strcat(response, "\",");

	// Thermocouple type
	strcat(response, "\"thermocouple_type\": \"");
	switch (_controller.configuration.configuration_settings[CONFIG_TC_TYPE])
	{
	case THERMOCOUPLE_K:
		strcat(response, "K type");
		break;
	case THERMOCOUPLE_N:
		strcat(response, "N type");
		break;
	case THERMOCOUPLE_R:
		strcat(response, "R type");
		break;
	case THERMOCOUPLE_S:
		strcat(response, "S type");
		break;
	}
	strcat(response, "\",");

	// Error 1 enable
	strcat(response, "\"err_1_enable\": \"");
	strcat(response, _controller.configuration.configuration_settings[CONFIG_ERR1_ENABLED] ? "Enabled": "Disabled");
	strcat(response, "\",");

	// Maximum temperature
	strcat(response, "\"max_temperature\": \"");
	sprintf(line, "%d &degC", _controller.configuration.configuration_settings[CONFIG_MAX_TEMP]);
	strcat(response, line);
	strcat(response, "\",");

	// Display brightness
	strcat(response, "\"display_brightness\": \"");
	sprintf(line, "%d", _controller.configuration.configuration_settings[CONFIG_LED_BRILL]);
	strcat(response, line);
	strcat(response, "\",");

	// Error 4 enable
	strcat(response, "\"err_4_enable\": \"");
	strcat(response, _controller.configuration.configuration_settings[CONFIG_ERR4_ENABLED] ? "Enabled": "Disabled");
	strcat(response, "\",");

	// Error 5 enable
	strcat(response, "\"err_5_enable\": \"");
	strcat(response, _controller.configuration.configuration_settings[CONFIG_ERR5_ENABLED] ? "Enabled": "Disabled");
	strcat(response, "\",");

	// Error 6 firing hours trip
	strcat(response, "\"max_hours\": \"");
	sprintf(line, "%d hours", _controller.configuration.configuration_settings[CONFIG_MAX_HOURS]);
	strcat(response, line);
	strcat(response, "\",");

	// Error 7 room temperature trip
	strcat(response, "\"max_ambient\": \"");
	sprintf(line, "%d &degC", _controller.configuration.configuration_settings[CONFIG_MAX_AMBIENT]);
	strcat(response, line);
	strcat(response, "\",");

	// Power fail recovery enable
	strcat(response, "\"power_fail_enable\": \"");
	strcat(response, _controller.configuration.configuration_settings[CONFIG_PFR_ENABLED] ? "Enabled": "Disabled");
	strcat(response, "\",");

	// Paused time limit
	strcat(response, "\"paused_hours_limit\": \"");
	sprintf(line, "%d hours", _controller.configuration.configuration_settings[CONFIG_PAUSED_HOURS_LIMIT]);
	strcat(response, line);
	strcat(response, "\",");

	// Set point offset
	strcat(response, "\"set_point_offset\": \"");
	sprintf(line, "%d &degC", _controller.configuration.configuration_settings[CONFIG_SP_OFFSET1]);
	strcat(response, line);
	strcat(response, "\",");

	// Proportional band
	strcat(response, "\"proportional_band\": \"");
	sprintf(line, "%d &degC", _controller.configuration.configuration_settings[CONFIG_PID_P1]);
	strcat(response, line);
	strcat(response, "\",");

	// Integral time
	strcat(response, "\"integral_time\": \"");
	sprintf(line, "%d secs", _controller.configuration.configuration_settings[CONFIG_PID_I1]);
	strcat(response, line);
	strcat(response, "\",");

	// Differential time
	strcat(response, "\"differential_time\": \"");
	sprintf(line, "%d secs", _controller.configuration.configuration_settings[CONFIG_PID_D1]);
	strcat(response, line);
	strcat(response, "\",");

	// Kiln element power
	strcat(response, "\"element_power\": \"");
	sprintf(line, "%.1f kW", 0.1F * _controller.configuration.configuration_settings[CONFIG_KILN1_KW]);
	strcat(response, line);
	strcat(response, "\",");

	// Power fail recovery enable
	strcat(response, "\"lockup_enabled\": \"");
	strcat(response, _controller.configuration.configuration_settings[CONFIG_LE_ENABLED] ? "Enabled": "Disabled");
	strcat(response, "\",");

	// Cycle time
	strcat(response, "\"control_cycle_time\": \"");
	sprintf(line, "%d secs", _controller.configuration.configuration_settings[CONFIG_CYCLE_TIME]);
	strcat(response, line);
	strcat(response, "\",");

	// Relay 3 function
	strcat(response, "\"relay_3_function\": \"");
	switch (_controller.configuration.configuration_settings[CONFIG_RL3_FUNCTION])
	{
	case EVENT_RELAY_OFF:
		strcat(response, "Off");
		break;
	case EVENT_RELAY_EVENT:
		strcat(response, "Event");
		break;
	case EVENT_RELAY_DAMPER:
		strcat(response, "Damper");
		break;
	case EVENT_RELAY_FAN:
		strcat(response, "Fan");
		break;
	}
	strcat(response, "\",");

	// Remember start delay
	strcat(response, "\"remember_start_delay\": \"");
	strcat(response, _controller.configuration.configuration_settings[CONFIG_REMEMBER_START_DELAY] ? "Remember": "Forget");
	strcat(response, "\",");

	// Skip start delay on power fail
	strcat(response, "\"skip_start_delay\": \"");
	strcat(response, _controller.configuration.configuration_settings[CONFIG_SKIP_START_DELAY] ? "Skip delay": "Resume delay");
	strcat(response, "\",");

	// USB logging interval
	strcat(response, "\"sampling_rate\": \"");
	sprintf(line, "%d secs", _controller.configuration.configuration_settings[CONFIG_SAMPLE_RATE]);
	strcat(response, line);
	strcat(response, "\",");

	// Temperature units
	strcat(response, "\"temperature_units\": \"");
	strcat(response, _controller.configuration.configuration_settings[CONFIG_DEGF] ? "&degF": "&degC");
	strcat(response, "\",");

	// Maximum number of programs
	strcat(response, "\"max_programs\": \"");
	sprintf(line, "%d", _controller.configuration.configuration_settings[CONFIG_PROGRAMS_CAP]);
	strcat(response, line);
	strcat(response, "\",");

	// Maximum number of segments
	strcat(response, "\"max_segments\": \"");
	sprintf(line, "%d", _controller.configuration.configuration_settings[CONFIG_SEGMENTS_CAP]);
	strcat(response, line);
	strcat(response, "\"");

	strcat(response, "}");

	DEBUG_VERBOSE("Response %s", response);

    // Send the response
    result = httpd_resp_send(request, response, strlen(response));
    free(response);

    return result;
}

/**
 * @brief Handles a GET request for Android connectivity checks
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t generate_204_get_handler(httpd_req_t *request)
{
    esp_err_t result;

    DEBUG_VERBOSE("GET generate_204");

    // Return code 204 - No content
//    httpd_resp_set_status(request, "204 No Content");
//    result = httpd_resp_sendstr(request, "");

    // Redirect back to the home page
    httpd_resp_set_status(request, "302 Found");
    httpd_resp_set_hdr(request, "Location", AP_URL);
    result = httpd_resp_sendstr(request, "Redirect");

    return result;
}

/**
 * @brief Handles a GET request for Windows 10 checks
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t connect_test_get_handler(httpd_req_t *request)
{
    esp_err_t result;

    DEBUG_VERBOSE("GET connecttest.txt");

    // Redirect back to the home page
    httpd_resp_set_status(request, "302 Found");
    httpd_resp_set_hdr(request, "Location", AP_URL);
    result = httpd_resp_sendstr(request, "Redirect");

    return result;
}

/**
 * @brief Handles a GET request for Windows 10 checks
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t redirect_get_handler(httpd_req_t *request)
{
    esp_err_t result;

    DEBUG_VERBOSE("GET redirect");

    // Redirect back to the home page
    httpd_resp_set_status(request, "302 Found");
    httpd_resp_set_hdr(request, "Location", AP_URL);
    result = httpd_resp_sendstr(request, "Redirect");

    return result;
}

/**
 * @brief Handles a GET request for Windows 10 checks
 * 
 * @param request The request
 * 
 * @returns ESP_OK or an error code
*/
static esp_err_t hotspot_get_handler(httpd_req_t *request)
{
    esp_err_t result;

    DEBUG_VERBOSE("GET redirect");

    // Redirect back to the home page
    httpd_resp_set_status(request, "302 Found");
    httpd_resp_set_hdr(request, "Location", AP_URL);
    result = httpd_resp_sendstr(request, "Redirect");

    return result;
}


/**
 * @brief Creates the URL of an icon used to represent a router
 * 
 * @param signal_strength Number of signal strength bars 0 to 4
 * @param secure Indicates if the router requires a password
 * 
 * @returns A URL string
*/
static char *create_icon_url(int32_t signal_strength, bool secure)
{
	if (secure)
	{
		switch (signal_strength)
		{
			default:
			case 0:
				return "wifi_0_locked.png";
			case 1:
				return "wifi_1_locked.png";
			case 2:
				return "wifi_2_locked.png";
			case 3:
				return "wifi_3_locked.png";
			case 4:
				return "wifi_4_locked.png";
		}
	}
	else
	{
		switch (signal_strength)
		{
			default:
			case 0:
				return "wifi_0_unlocked.png";
			case 1:
				return "wifi_1_unlocked.png";
			case 2:
				return "wifi_2_unlocked.png";
			case 3:
				return "wifi_3_unlocked.png";
			case 4:
				return "wifi_4_unlocked.png";
		}
	}
}

/**
 * @brief Serves up static files
*/
static esp_err_t base_get_handler(httpd_req_t *request)
{
	DEBUG_VERBOSE("GET base.ccs");
	httpd_resp_set_type(request, "text/css");
    return httpd_resp_send(request, (char *)_base_css, strlen((char *)_base_css));
}

static esp_err_t jquery_get_handler(httpd_req_t *request)
{
	DEBUG_VERBOSE("GET jquery.js");
	httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, (char *)_jquery_js, strlen((char *)_jquery_js));
}

static esp_err_t spin_get_handler(httpd_req_t *request)
{
	DEBUG_VERBOSE("GET spin.js");
	httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, (char *)_spin_js, strlen((char *)_spin_js));
}

static esp_err_t wifi_0_locked_get_handler(httpd_req_t *request)
{
	DEBUG_VERBOSE("GET wifi_0_locked.png");
	httpd_resp_set_type(request, "image/png");
    return httpd_resp_send(request, (char *)_wifi_0_locked_png_start, _wifi_0_locked_png_end - _wifi_0_locked_png_start);
}

static esp_err_t wifi_1_locked_get_handler(httpd_req_t *request)
{
	DEBUG_VERBOSE("GET wifi_1_locked.png");
	httpd_resp_set_type(request, "image/png");
    return httpd_resp_send(request, (char *)_wifi_1_locked_png_start, _wifi_1_locked_png_end - _wifi_1_locked_png_start);
}

static esp_err_t wifi_2_locked_get_handler(httpd_req_t *request)
{
	DEBUG_VERBOSE("GET wifi_2_locked.png");
	httpd_resp_set_type(request, "image/png");
    return httpd_resp_send(request, (char *)_wifi_2_locked_png_start, _wifi_2_locked_png_end - _wifi_2_locked_png_start);
}

static esp_err_t wifi_3_locked_get_handler(httpd_req_t *request)
{
	DEBUG_VERBOSE("GET wifi_3_locked.png");
	httpd_resp_set_type(request, "image/png");
    return httpd_resp_send(request, (char *)_wifi_3_locked_png_start, _wifi_3_locked_png_end - _wifi_3_locked_png_start);
}

static esp_err_t wifi_4_locked_get_handler(httpd_req_t *request)
{
	DEBUG_VERBOSE("GET wifi_4_locked.png");
	httpd_resp_set_type(request, "image/png");
    return httpd_resp_send(request, (char *)_wifi_4_locked_png_start, _wifi_4_locked_png_end - _wifi_4_locked_png_start);
}

static esp_err_t wifi_0_unlocked_get_handler(httpd_req_t *request)
{
	DEBUG_VERBOSE("GET wifi_0_unlocked.png");
	httpd_resp_set_type(request, "image/png");
    return httpd_resp_send(request, (char *)_wifi_0_unlocked_png_start, _wifi_0_unlocked_png_end - _wifi_0_unlocked_png_start);
}

static esp_err_t wifi_1_unlocked_get_handler(httpd_req_t *request)
{
	DEBUG_VERBOSE("GET wifi_1_unlocked.png");
	httpd_resp_set_type(request, "image/png");
    return httpd_resp_send(request, (char *)_wifi_1_unlocked_png_start, _wifi_1_unlocked_png_end - _wifi_1_unlocked_png_start);
}

static esp_err_t wifi_2_unlocked_get_handler(httpd_req_t *request)
{
	DEBUG_VERBOSE("GET wifi_2_unlocked.png");
	httpd_resp_set_type(request, "image/png");
    return httpd_resp_send(request, (char *)_wifi_2_unlocked_png_start, _wifi_2_unlocked_png_end - _wifi_2_unlocked_png_start);
}

static esp_err_t wifi_3_unlocked_get_handler(httpd_req_t *request)
{
	DEBUG_VERBOSE("GET wifi_3_unlocked.png");
	httpd_resp_set_type(request, "image/png");
    return httpd_resp_send(request, (char *)_wifi_3_unlocked_png_start, _wifi_3_unlocked_png_end - _wifi_3_unlocked_png_start);
}

static esp_err_t wifi_4_unlocked_get_handler(httpd_req_t *request)
{
	DEBUG_VERBOSE("GET wifi_4_unlocked.png");
	httpd_resp_set_type(request, "image/png");
    return httpd_resp_send(request, (char *)_wifi_4_unlocked_png_start, _wifi_4_unlocked_png_end - _wifi_4_unlocked_png_start);
}


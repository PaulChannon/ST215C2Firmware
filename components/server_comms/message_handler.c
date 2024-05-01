/**
 * @file message_handler.c
 * @brief Provides facilities for creating and posting HTTP messages
*/
#include "common.h"
#include "nxjson.h"
#include "wifi_interface.h"
#include "http_client.h"
#include "controller.h"
#include "message_handler.h"

// Controls debug output to the serial port:
//   0 - nothing output
//   1 - basic information and error
//   2 - verbose output
#define DEBUG_OUTPUT 1

#define DEBUG_VERBOSE(format, ...)           if (DEBUG_OUTPUT > 1) { ESP_LOGI(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_INFO(format, ...)              if (DEBUG_OUTPUT > 0) { ESP_LOGI(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_ERROR(format, ...)             if (DEBUG_OUTPUT > 0) { ESP_LOGE(MODULE_NAME, format, ##__VA_ARGS__); }
#define DEBUG_DUMP(buffer, buff_len, level)  if (DEBUG_OUTPUT > 1) { ESP_LOG_BUFFER_HEXDUMP(MODULE_NAME, buffer, buff_len, level); }

// Codes for commands that can be embedded in an HTTP response message
#define COMMAND_CODE_NONE                 0
#define COMMAND_CODE_SEND_CONFIGURATION   1
#define COMMAND_CODE_RETRIEVE_PROGRAM     2
#define COMMAND_CODE_STORE_PROGRAM        3
#define COMMAND_CODE_STOP_PROGRAM         4
#define COMMAND_CODE_RETRIEVE_EVENT       5
#define COMMAND_CODE_CLEAR_EVENTS         6

static const char *MODULE_NAME = "Message handler";
static int32_t _message_id = 0;
static int32_t _command_code = COMMAND_CODE_NONE;
static char _command_error_message[100] = "";
static char _post_message_body[MAX_POST_MESSAGE_BODY_LENGTH + 1];
static char _response_message_body[MAX_RESPONSE_MESSAGE_BODY_LENGTH + 1];
static int32_t _next_post_delay = 0;
static _TProgram _read_program_buffer;
static _TProgram _write_program_buffer;
static _TEvent _read_event_buffer;
static bool _event_logging_enabled = true;

static bool prepare_post_message();
static void encode_json_mandatory_fields(char *message);
static void encode_json_status_node(char *message);
static void encode_json_configuration_command_ack(char *message);
static void encode_json_retrieve_program_command_ack(char *message);
static void encode_json_store_program_command_ack(char *message);
static void encode_json_stop_program_command_ack(char *message);
static void encode_json_retrieve_event_command_ack(char *message);
static void encode_json_clear_events_command_ack(char *message);
static void encode_json_command_nak(char *message, char *code, char *error_message);
static void process_response(char *response_message_body);
static bool decode_json(const nx_json *json);
static bool decode_json_mandatory_fields(const nx_json *node);
static bool decode_json_retrieve_program_command(const nx_json *command_node);
static bool decode_json_store_program_command(const nx_json *command_node);
static bool decode_json_stop_program_command(const nx_json *command_node);
static bool decode_json_retrieve_event_command(const nx_json *command_node);
static bool decode_json_clear_events_command(const nx_json *command_node);
static char *get_firing_state_name(int32_t firing_state);
static char *get_thermocouple_type_name(int32_t thermocouple_type);
static char *get_event_relay_function_name(int32_t event_relay_function);
static char *get_event_type_name(int32_t event_type);

/**
 * @brief Creates a post message, sends it to the server and processes the response
 * 
 * @param next_post_delay Suggested time until the next post as suggested by the server
 * 
 * @returns True if successful
*/
bool post_message_to_server(int32_t *next_post_delay)
{
	int32_t response_status_code;

	DEBUG_INFO("Preparing post message");

	// Prepares a post message, reading data from the controller where necessary
	if (!prepare_post_message())
	{
		DEBUG_ERROR("Error creating post message");
		return false;
	}

	DEBUG_VERBOSE("Sending post message: %s", _post_message_body);

	// Connect to the server
	if (!connect_to_server())
	{
        // Log the first server error after enabling logging
	    if (_event_logging_enabled)
        {
	        log_event(EVENT_SERVER_ERROR, COMMS_ERROR_CANNOT_CONNECT);
	        _event_logging_enabled = false;
        }

		DEBUG_ERROR("Cannot connect to server");
		return false;
	}

	// Turn on the radio LED
	write_radio_led(true);

	// Post the message to the server and wait for a response
	if (!post_http_message(_post_message_body,
						   _response_message_body,
					       &response_status_code))
	{
        // Log the first server error after enabling logging
        if (_event_logging_enabled)
        {
            log_event(EVENT_SERVER_ERROR, COMMS_ERROR_NO_RESPONSE);
            _event_logging_enabled = false;
        }

		DEBUG_ERROR("No response from server");
		disconnect_from_server();
        write_radio_led(true);
		return false;
	}

    // Turn off the radio LED
    write_radio_led(false);

	// Disconnect from the server
	disconnect_from_server();

	DEBUG_INFO("Processing response");

	// Check for a valid response
	if (response_status_code != HTTP_OK)
	{
        // Log the first server error after enabling logging
        if (_event_logging_enabled)
        {
            log_event(EVENT_SERVER_ERROR, COMMS_ERROR_INVALID_RESPONSE);
            _event_logging_enabled = false;
        }

		DEBUG_ERROR("Server responding with error code %ld", response_status_code);
		return false;
	}

	// Process the response
	process_response(_response_message_body);
	*next_post_delay = _next_post_delay;

	// Re-enable server error logging
	_event_logging_enabled = true;

	return true;
}

/**
 * @brief Create a post message, reading the necessary information from the controller 
 * 
 * @returns True if successful
*/
static bool prepare_post_message()
{
    char *message = _post_message_body;
    bool command_success;

	// Build the POST message, starting with the mandatory fields
	strcpy(message, "{");
	encode_json_mandatory_fields(message);

	// Add a status section
	encode_json_status_node(message);

	// Add a command ack/nak if there was a command in the last response
	if (_command_code != COMMAND_CODE_NONE)
	{
	    // See if the command has been handled successfully so far by looking at the error message
	    command_success = (_command_error_message[0] == 0);

		switch (_command_code)
		{
		case COMMAND_CODE_SEND_CONFIGURATION:

			if (command_success)
			{
				if (_controller.configuration_available)
				{
                    encode_json_configuration_command_ack(message);
				}
				else
				{
                    strcpy(_command_error_message, "Controller configuration is not available");
                    encode_json_command_nak(message, "send_conf", _command_error_message);
				}
			}
			else
			{
				encode_json_command_nak(message, "send_conf", _command_error_message);
			}
			break;

		case COMMAND_CODE_RETRIEVE_PROGRAM:

			if (command_success)
			{
			    // Read the program from the controller
		        if (read_program(_read_program_buffer.program_number,
		                         _controller.configuration.max_segments,
		                         &_read_program_buffer))
		        {
                    encode_json_retrieve_program_command_ack(message);
		        }
		        else
		        {
		            strcpy(_command_error_message, "Cannot read program from controller");
                    encode_json_command_nak(message, "get_prog", _command_error_message);
		        }
			}
			else
			{
				encode_json_command_nak(message, "get_prog", _command_error_message);
			}
			break;

		case COMMAND_CODE_STORE_PROGRAM:

			if (command_success)
			{
			    encode_json_store_program_command_ack(message);
			}
			else
			{
				encode_json_command_nak(message, "store_prog", _command_error_message);
			}
			break;

		case COMMAND_CODE_STOP_PROGRAM:

			if (command_success)
			{
				encode_json_stop_program_command_ack(message);
			}
			else
			{
				encode_json_command_nak(message, "stop", _command_error_message);
			}
			break;

        case COMMAND_CODE_RETRIEVE_EVENT:

            if (command_success)
            {
                // Read the event from the controller
                if (read_event(_read_event_buffer.event_id, &_read_event_buffer))
                {
                    encode_json_retrieve_event_command_ack(message);
                }
                else
                {
                    strcpy(_command_error_message, "Cannot read event from controller");
                    encode_json_command_nak(message, "get_event", _command_error_message);
                }
            }
            else
            {
                encode_json_command_nak(message, "get_event", _command_error_message);
            }
            break;

        case COMMAND_CODE_CLEAR_EVENTS:

            if (command_success)
            {
                encode_json_clear_events_command_ack(message);
            }
            else
            {
                encode_json_command_nak(message, "clear_events", _command_error_message);
            }
            break;
        }
	}

	// Complete the message body
	strcat(_post_message_body, "}");

	// Clear the last command, just in case
	_command_code = COMMAND_CODE_NONE;
	strcpy(_command_error_message, "");

	return true;
}

/**
 * @brief Encodes mandatory fields in JSON format and adds to the supplied post message
 * 
 * @param message Post message to which JSON data should be added
*/
static void encode_json_mandatory_fields(char *message)
{
    char line[50];

	sprintf(line, "\"proto_ver\": %d,", PROTOCOL_VERSION);
	strcat(message, line);
	sprintf(line, "\"msg_id\": %ld,", ++_message_id);
	strcat(message, line);
	sprintf(line, "\"mac_addr\": \"%s\",", _controller.mac_address);
	strcat(message, line);
}

/**
 * @brief Encodes status information in JSON format and adds to the supplied post message
 * 
 * @param message Post message to which JSON data should be added
*/
static void encode_json_status_node(char *message)
{
    char line[50];
    _TControllerStatus *status = &(_controller.status);
    int32_t program_index;
    _TCrcInfo *crc_info;
    int32_t wifi_rssi = -100;

	strcat(message, "\"status\": {");

	sprintf(line, "\"rtc\": \"20%02d-%02d-%02d %02d:%02d:%02d\",",
			status->year, status->month, status->day,
			status->hour, status->minute, status->second);
	strcat(message, line);
	sprintf(line, "\"state\": \"%s\",", get_firing_state_name(status->firing_state));
	strcat(message, line);
	sprintf(line, "\"err_code\": %d,", status->error_code);
	strcat(message, line);
	sprintf(line, "\"amb_temp\": %.1f,", status->ambient_temperature);
	strcat(message, line);
	sprintf(line, "\"temp_1\": %.1f,", status->temperature_1);
	strcat(message, line);
	sprintf(line, "\"temp_2\": %.1f,", status->temperature_2);
	strcat(message, line);
	sprintf(line, "\"temp_3\": %.1f,", status->temperature_3);
	strcat(message, line);
	sprintf(line, "\"temp_set_1\": %.1f,", status->temperature_set_point_1);
	strcat(message, line);
	sprintf(line, "\"temp_set_2\": %.1f,", status->temperature_set_point_2);
	strcat(message, line);
	sprintf(line, "\"temp_set_3\": %.1f,", status->temperature_set_point_3);
	strcat(message, line);
	sprintf(line, "\"energy_1\": %.1f,", status->energy_used_1);
	strcat(message, line);
	sprintf(line, "\"energy_2\": %.1f,", status->energy_used_2);
	strcat(message, line);
	sprintf(line, "\"energy_3\": %.1f,", status->energy_used_3);
	strcat(message, line);
	sprintf(line, "\"duty_1\": %.1f,", status->total_duty_1);
	strcat(message, line);
	sprintf(line, "\"duty_2\": %.1f,", status->total_duty_2);
	strcat(message, line);
	sprintf(line, "\"duty_3\": %.1f,", status->total_duty_3);
	strcat(message, line);
    sprintf(line, "\"soak_rem\": %d,", status->soak_remaining);
    strcat(message, line);
    sprintf(line, "\"event_relays\": %d,", status->event_relay_states);
    strcat(message, line);
	sprintf(line, "\"prog\": %d,", status->current_program);
	strcat(message, line);
	sprintf(line, "\"seg\": %d,", status->current_segment);
	strcat(message, line);
	sprintf(line, "\"delay\": %d,", status->start_delay);
	strcat(message, line);
	sprintf(line, "\"delay_rem\": %d,", status->start_delay_remaining);
	strcat(message, line);
    sprintf(line, "\"events\": %d,", status->num_events);
    strcat(message, line);
    sprintf(line, "\"last_event_id\": %ld,", status->last_event_id);
    strcat(message, line);
	sprintf(line, "\"prog_changed\": %s,", status->program_changed ? "true": "false");
	strcat(message, line);
	sprintf(line, "\"conf_changed\": %s,", status->configuration_changed ? "true": "false");
	strcat(message, line);

	strcat(message, "\"prog_crcs\": [");
    for (program_index = 0; program_index < MAX_PROGRAMS; program_index++)
    {
        crc_info = &(status->program_crc_info[program_index]);

        if (crc_info->crc_known)
        {
            sprintf(line, "\"%08lX\"", crc_info->crc);
        }
        else
        {
            sprintf(line, "\"\"");
        }
        strcat(message, line);

        if (program_index != MAX_PROGRAMS - 1)
        {
            strcat(message, ",");
        }
    }
    strcat(message, "],");

    strcat(message, "\"conf_crc\": ");
    if (status->configuration_crc_info.crc_known)
    {
        sprintf(line, "\"%08lX\",", status->configuration_crc_info.crc);
    }
    else
    {
        sprintf(line, "\"\",");
    }
    strcat(message, line);

    get_rssi(&wifi_rssi);
    sprintf(line, "\"wifi_rssi\": %ld", wifi_rssi);
    strcat(message, line);

	strcat(message, "}");
}

/**
 * @brief Encodes a send configuration command acknowledgement in JSON format and adds to the supplied post message
 * 
 * @param message Post message to which JSON data should be added
*/
static void encode_json_configuration_command_ack(char *message)
{
    char line[50];
    _TControllerConfiguration *configuration = &(_controller.configuration);
    int32_t setting_index;

	strcat(message, ",\"cmd_ack\": {");

	strcat(message, "\"code\": \"send_config\"");

	strcat(message, ",\"config\": {");

	sprintf(line, "\"firm_ver\": \"%s\",", configuration->pic_firmware_version);
	strcat(message, line);
	sprintf(line, "\"name\": \"%s\",", configuration->controller_name);
	strcat(message, line);
	sprintf(line, "\"tc_type\": \"%s\",", get_thermocouple_type_name(configuration->thermocouple_type));
	strcat(message, line);
	sprintf(line, "\"units\": \"%s\",", (configuration->is_fahrenheit_units ? "F" : "C"));
	strcat(message, line);
	sprintf(line, "\"max_temp\": %d,", configuration->max_user_temperature);
	strcat(message, line);
	sprintf(line, "\"zones\": %d,", configuration->zones_in_use);
	strcat(message, line);
	sprintf(line, "\"progs\": %d,", configuration->max_programs);
	strcat(message, line);
	sprintf(line, "\"segs\": %d,", configuration->max_segments);
	strcat(message, line);
	sprintf(line, "\"event_1\": \"%s\",", get_event_relay_function_name(configuration->event_relay_function_1));
	strcat(message, line);
	sprintf(line, "\"event_2\": \"%s\",", get_event_relay_function_name(configuration->event_relay_function_2));
	strcat(message, line);

    strcat(message, "\"settings\": [");
	for (setting_index = 0; setting_index < NUM_CONFIG_SETTINGS; setting_index++)
	{
	    sprintf(line, "%d", configuration->configuration_settings[setting_index]);
	    strcat(message, line);

        if (setting_index != NUM_CONFIG_SETTINGS - 1)
        {
            strcat(message, ",");
        }
	}
    strcat(message, "]");

	strcat(message, "}}");
}

/**
 * @brief Encodes a retrieve program command acknowledgement in JSON format and adds to the supplied post message
 * 
 * @param message Post message to which JSON data should be added
*/
static void encode_json_retrieve_program_command_ack(char *message)
{
    char line[50];
    int32_t segment_index;
    _TProgramSegment *segment;
    _TControllerConfiguration *configuration = &(_controller.configuration);

	strcat(message, ",\"cmd_ack\": {");

	strcat(message, "\"code\": \"get_prog\"");

	sprintf(line, ",\"prog\": %d", _read_program_buffer.program_number);
	strcat(message, line);

	strcat(message, ",\"segs\": [");

    for (segment_index = 0; segment_index < _read_program_buffer.segments_used; segment_index++)
    {
    	segment = &(_read_program_buffer.segments[segment_index]);

    	if (configuration->ramp_rate_scaling == 10)
    	{
    	    sprintf(line, "[%.1f,%d,%d,%d]", (float)segment->ramp_rate / configuration->ramp_rate_scaling, segment->target_temperature, segment->soak_time, segment->event_flags);
    	}
    	else
        {
            sprintf(line, "[%d,%d,%d,%d]", segment->ramp_rate, segment->target_temperature, segment->soak_time, segment->event_flags);
        }
    	strcat(message, line);

    	if (segment_index != _read_program_buffer.segments_used - 1)
    	{
    		strcat(message, ",");
    	}
    }

	strcat(message, "]}");
}

/**
 * @brief Encodes a retrieve program command acknowledgement in JSON format and adds to the supplied post message
 * 
 * @param message Post message to which JSON data should be added
*/
static void encode_json_store_program_command_ack(char *message)
{
    char line[50];

	strcat(message, ",\"cmd_ack\": {");

	strcat(message, "\"code\": \"store_prog\"");

	sprintf(line, ",\"prog\": %d", _write_program_buffer.program_number);
	strcat(message, line);

	strcat(message, "}");
}

/**
 * @brief Encodes a stop program command acknowledgement in JSON format and adds to the supplied post message
 * 
 * @param message Post message to which JSON data should be added
*/
static void encode_json_stop_program_command_ack(char *message)
{
	strcat(message, ",\"cmd_ack\": {");
	strcat(message, "\"code\": \"stop_prog\"");
	strcat(message, "}");
}

/**
 * @brief Encodes a retrieve event command acknowledgement in JSON format and adds to the supplied post message
 * 
 * @param message Post message to which JSON data should be added
*/
static void encode_json_retrieve_event_command_ack(char *message)
{
    char line[50];

    strcat(message, ",\"cmd_ack\": {");

    strcat(message, "\"code\": \"get_event\"");

    sprintf(line, ",\"id\": %ld,", _read_event_buffer.event_id);
    strcat(message, line);
    sprintf(line, "\"rtc\": \"20%02d-%02d-%02d %02d:%02d:%02d\",",
            _read_event_buffer.year, _read_event_buffer.month, _read_event_buffer.day,
            _read_event_buffer.hour, _read_event_buffer.minute, _read_event_buffer.second);
    strcat(message, line);
    sprintf(line, "\"type\": \"%s\",", get_event_type_name(_read_event_buffer.event_type));
    strcat(message, line);
    sprintf(line, "\"state\": \"%s\",", get_firing_state_name(_read_event_buffer.firing_state));
    strcat(message, line);
    sprintf(line, "\"err_code\": %d,", _read_event_buffer.error_code);
    strcat(message, line);
    sprintf(line, "\"amb_temp\": %.1f,", _read_event_buffer.ambient_temperature);
    strcat(message, line);
    sprintf(line, "\"temp_1\": %.1f,", _read_event_buffer.temperature_1);
    strcat(message, line);
    sprintf(line, "\"temp_2\": %.1f,", _read_event_buffer.temperature_2);
    strcat(message, line);
    sprintf(line, "\"temp_3\": %.1f,", _read_event_buffer.temperature_3);
    strcat(message, line);
    sprintf(line, "\"temp_set\": %.1f,", _read_event_buffer.temperature_set_point);
    strcat(message, line);
    sprintf(line, "\"prog\": %d,", _read_event_buffer.current_program);
    strcat(message, line);
    sprintf(line, "\"seg\": %d,", _read_event_buffer.current_segment);
    strcat(message, line);
    sprintf(line, "\"comms_err_code\": %d,", _read_event_buffer.comms_error_code);
    strcat(message, line);
    sprintf(line, "\"comms_command\": %d", _read_event_buffer.comms_command_id);
    strcat(message, line);

    strcat(message, "}");
}

/**
 * @brief Encodes a clear events command acknowledgement in JSON format and adds to the supplied post message
 * 
 * @param message Post message to which JSON data should be added
 * @param program Program data
*/
static void encode_json_clear_events_command_ack(char *message)
{
    strcat(message, ",\"cmd_ack\": {");
    strcat(message, "\"code\": \"clear_events\"");
    strcat(message, "}");
}

/**
 * @brief Encodes a retrieve program command command acknowledgement in JSON format and adds to the supplied post message
 * 
 * @param message Post message to which JSON data should be added
 * @param code Command code string
 * @param error_message Error message
*/
static void encode_json_command_nak(char *message, char *code, char *error_message)
{
    char line[50];

	strcat(message, ",\"cmd_nak\": {");

	sprintf(line, "\"code\": \"%s\"", code);
	strcat(message, line);

	sprintf(line, ",\"error\": \"%s\"", error_message);
	strcat(message, line);

	strcat(message, "}");
}

/**
 * @brief Processes a response from the server
 * 
 * @param response_message_body The body of the response message to process
*/
static void process_response(char *response_message_body)
{
	const nx_json *json;

	DEBUG_VERBOSE("Received response message body length %d: %s", strlen(response_message_body), response_message_body);

	// Clear existing command data
    _command_code = COMMAND_CODE_NONE;
    strcpy(_command_error_message, "");

	// Parse the response string into a JSON structure
	json = nx_json_parse(response_message_body, 0);
	if (!json)
	{
	    strcpy(_command_error_message, "Invalid JSON");
		DEBUG_ERROR("Error parsing response message");
		return;
	}

	// Decode the JSON data
	if (!decode_json(json))
	{
		DEBUG_ERROR("Error processing message (%s)", _command_error_message);
	}

	// Free memory
	nx_json_free(json);
}

/**
 * @brief Decodes the JSON data in a response
 * 
 * @param node Top-level node containing all JSON data
*/
static bool decode_json(const nx_json *node)
{
	const nx_json *field;
	const nx_json *command_node;
	const char *command_code;

	// Extract common fields
	if (!decode_json_mandatory_fields(node)) return false;

	// Check for a command node
	command_node = nx_json_get(node, "cmd");
	if (command_node->type == NX_JSON_OBJECT)
	{
		// Extract the command code
		field = nx_json_get(command_node, "code");
		if (field->type != NX_JSON_STRING)
		{
			strcpy(_command_error_message, "Missing or incorrectly formatted command code");
			return false;
		}
		command_code = field->text_value;

		DEBUG_VERBOSE("Command code: %s", command_code);

		// Process according to code
		if (strcmp(command_code, "send_config") == 0)
		{
		    _command_code = COMMAND_CODE_SEND_CONFIGURATION;
		    return true;
		}
		else if (strcmp(command_code, "get_prog") == 0)
		{
		    _command_code = COMMAND_CODE_RETRIEVE_PROGRAM;
			if (!decode_json_retrieve_program_command(command_node)) return false;
		}
		else if (strcmp(command_code, "store_prog") == 0)
		{
			_command_code = COMMAND_CODE_STORE_PROGRAM;
			if (!decode_json_store_program_command(command_node)) return false;
		}
		else if (strcmp(command_code, "stop_prog") == 0)
		{
			_command_code = COMMAND_CODE_STOP_PROGRAM;
			if (!decode_json_stop_program_command(command_node)) return false;
		}
        else if (strcmp(command_code, "get_event") == 0)
        {
            _command_code = COMMAND_CODE_RETRIEVE_EVENT;
            if (!decode_json_retrieve_event_command(command_node)) return false;
        }
        else if (strcmp(command_code, "clear_events") == 0)
        {
            _command_code = COMMAND_CODE_CLEAR_EVENTS;
            if (!decode_json_clear_events_command(command_node)) return false;
        }
		else
		{
			_command_code = COMMAND_CODE_NONE;
		}
	}

	return true;
}

/**
 * @brief Decodes the mandatory field values supplied in a JSON node structure
 * 
 * @param node Top-level JSON node
 * 
 * @returns Returns true if successful
*/
static bool decode_json_mandatory_fields(const nx_json *node)
{
	const nx_json *field;
	int32_t protocol_version;
	int32_t message_id;
	const char *mac_address;

	// Extract and check the protocol version
	field = nx_json_get(node, "proto_ver");
	if (field->type != NX_JSON_INTEGER)
	{
		strcpy(_command_error_message, "Missing or incorrectly formatted protocol version");
		return false;
	}

	protocol_version = field->int_value;

	if (protocol_version != PROTOCOL_VERSION)
	{
		DEBUG_ERROR("Incorrect protocol version");
		strcpy(_command_error_message, "Incorrect protocol version");
		return false;
	}

	// Extract the message number
	field = nx_json_get(node, "msg_id");
	if (field->type != NX_JSON_INTEGER)
	{
		strcpy(_command_error_message, "Missing or incorrectly formatted message ID");
		return false;
	}
	message_id = field->int_value;

	// Extract the MAC address and check the message is for us
	field = nx_json_get(node, "mac_addr");
	if (field->type != NX_JSON_STRING)
	{
		strcpy(_command_error_message, "Missing or incorrectly formatted MAC address");
		return false;
	}

	mac_address = field->text_value;

	if (strcmp(mac_address, _controller.mac_address) != 0)
	{
		DEBUG_ERROR("Incorrect MAC address");
		strcpy(_command_error_message, "Incorrect MAC address");
		return false;
	}

	// Extract the next POST delay suggestion
	field = nx_json_get(node, "next_post");
	if (field->type != NX_JSON_INTEGER)
	{
		strcpy(_command_error_message, "Missing or incorrectly formatted next post delay");
		return false;
	}

	_next_post_delay = field->int_value;

	DEBUG_VERBOSE("Protocol version: %ld", protocol_version);
	DEBUG_VERBOSE("Message ID: %ld", message_id);
	DEBUG_VERBOSE("MAC address: %s", mac_address);
	DEBUG_VERBOSE("Next post delay: %ld", _next_post_delay);

	return true;
}

/**
 * @brief Decodes the fields for a retrieve program command supplied in a JSON node
 * 
 * @param command_node JSON node containing the command
 * 
 * @returns Returns true if successful
*/
static bool decode_json_retrieve_program_command(const nx_json *command_node)
{
	const nx_json *field;

	// Extract and check the program number
	field = nx_json_get(command_node, "prog");
	if (field->type != NX_JSON_INTEGER)
	{
		strcpy(_command_error_message, "Missing or incorrectly formatted program number");
		return false;
	}
	_read_program_buffer.program_number = (uint8_t)field->int_value;
	DEBUG_VERBOSE("Program number: %d", _read_program_buffer.program_number);

    if (_read_program_buffer.program_number < 1 ||
        _read_program_buffer.program_number > _controller.configuration.max_programs)
    {
        strcpy(_command_error_message, "Program number out of range");
        return false;
    }

	return true;
}

/**
 * @brief Decodes the fields for a store program command supplied in a JSON node structure
 * 
 * @param command_node JSON node containing the command
 * 
 * @returns Returns true if successful
*/
static bool decode_json_store_program_command(const nx_json *command_node)
{
	const nx_json *field;
	const nx_json *segments_node;
	const nx_json *segment_node;
	int32_t segment_index;
	_TProgramSegment *segment;
    _TControllerConfiguration *configuration = &(_controller.configuration);

	// Extract and check the program number
	field = nx_json_get(command_node, "prog");
	if (field->type != NX_JSON_INTEGER)
	{
		strcpy(_command_error_message, "Missing or incorrectly formatted program number");
		return false;
	}
	_write_program_buffer.program_number = (uint8_t)field->int_value;

    DEBUG_VERBOSE("Program number: %d", _write_program_buffer.program_number);

	if (_write_program_buffer.program_number < 1 ||
	    _write_program_buffer.program_number > _controller.configuration.max_programs)
    {
        strcpy(_command_error_message, "Program number out of range");
        return false;
    }

	// Extract segment information
	segments_node = nx_json_get(command_node, "segs");
	if (segments_node->type != NX_JSON_ARRAY)
	{
		strcpy(_command_error_message, "Missing or incorrectly formatted segment array");
		return false;
	}

	for (segment_index = 0; segment_index < segments_node->length; segment_index++)
	{
		segment_node = nx_json_item(segments_node, segment_index);
		segment = &(_write_program_buffer.segments[segment_index]);

		if (segment_node->length != 4)
		{
			strcpy(_command_error_message, "Incorrect program segment definition");
			return false;
		}

		field = nx_json_item(segment_node, 0);
		if (field->type != NX_JSON_INTEGER && field->type != NX_JSON_DOUBLE)
		{
			strcpy(_command_error_message, "Missing or incorrectly formatted ramp rate");
			return false;
		}

        if (field->type == NX_JSON_INTEGER)
        {
            segment->ramp_rate = configuration->ramp_rate_scaling * field->int_value;
        }
        else
        {
            segment->ramp_rate = (uint16_t)(configuration->ramp_rate_scaling * field->dbl_value);
        }

		field = nx_json_item(segment_node, 1);
		if (field->type != NX_JSON_INTEGER)
		{
			strcpy(_command_error_message, "Missing or incorrectly formatted target temperature");
			return false;
		}
		segment->target_temperature = field->int_value;

		field = nx_json_item(segment_node, 2);
		if (field->type != NX_JSON_INTEGER)
		{
			strcpy(_command_error_message, "Missing or incorrectly formatted soak time");
			return false;
		}
		segment->soak_time = field->int_value;

		field = nx_json_item(segment_node, 3);
		if (field->type != NX_JSON_INTEGER)
		{
			strcpy(_command_error_message, "Missing or incorrectly formatted event flags");
			return false;
		}
		segment->event_flags = field->int_value;

		DEBUG_VERBOSE("Segment number: %ld", segment_index + 1);
		DEBUG_VERBOSE("Ramp rate: %d", segment->ramp_rate);
		DEBUG_VERBOSE("Target temperature: %d", segment->target_temperature);
		DEBUG_VERBOSE("Soak time: %d", segment->soak_time);
		DEBUG_VERBOSE("Event flags: %d", segment->event_flags);
	}

	// Store the number of segments
	_write_program_buffer.segments_used = (uint8_t)segments_node->length;

	// Write the program to the controller
    if (!write_program(_write_program_buffer.program_number,
                       _controller.configuration.max_segments,
                       &_write_program_buffer))
    {
        strcpy(_command_error_message, "Cannot write program to controller");
        return false;
    }

	return true;
}

/**
 * @brief Decodes the fields for a stop program command supplied in a JSON node structure
 * 
 * @param command_node JSON node containing the command
 * 
 * @returns Returns true if successful
*/
static bool decode_json_stop_program_command(const nx_json *command_node)
{
    // Stop the program
    if (!stop_program())
    {
        strcpy(_command_error_message, "Cannot stop program");
        return false;
    }

	return true;
}

/**
 * @brief Decodes the fields for a retrieve event command supplied in a JSON node structure
 * 
 * @param command_node JSON node containing the command
 * 
 * @returns Returns true if successful
*/
static bool decode_json_retrieve_event_command(const nx_json *command_node)
{
    const nx_json *field;

    // Extract and check the event number
    field = nx_json_get(command_node, "id");
    if (field->type != NX_JSON_INTEGER)
    {
        strcpy(_command_error_message, "Missing or incorrectly formatted event number");
        return false;
    }
    _read_event_buffer.event_id = field->int_value;

    DEBUG_VERBOSE("Event ID: %ld", _read_event_buffer.event_id);

    if (_read_event_buffer.event_id < 1)
    {
        strcpy(_command_error_message, "Invalid event ID number");
        return false;
    }

    return true;
}

/**
 * @brief Decodes the fields for a clear events command supplied in a JSON node structure
 * 
 * @param command_node JSON node containing the command
 * 
 * @returns Returns true if successful
*/
static bool decode_json_clear_events_command(const nx_json *command_node)
{
    // Clear events
    if (!clear_events())
    {
        strcpy(_command_error_message, "Cannot clear events");
        return false;
    }

    return true;
}

/**
 * @brief Returns the name associated with a given firing state code
 * 
 * @param firing_state firing state code
 * 
 * @returns Returns the name of the firing state
*/
static char *get_firing_state_name(int32_t firing_state)
{
	switch (firing_state)
	{
	case FIRING_STATE_INITIALISING: return "initialising";
	case FIRING_STATE_IDLE: return "idle";
	case FIRING_STATE_DELAY: return "delay";
	case FIRING_STATE_RAMP_HEATING: return "ramp_heating";
	case FIRING_STATE_RAMP_HEATING_PAUSED: return "ramp_heating_paused";
	case FIRING_STATE_RAMP_COOLING: return "ramp_cooling";
	case FIRING_STATE_RAMP_COOLING_PAUSED: return "ramp_cooling_paused";
	case FIRING_STATE_SOAK: return "soak";
	case FIRING_STATE_SOAK_PAUSED: return "soak_paused";
	case FIRING_STATE_COOLING: return "cooling";
	case FIRING_STATE_COOL: return "cool";
	case FIRING_STATE_ERROR: return "error";
	case FIRING_STATE_SETUP: return "setup";
	case FIRING_STATE_POWER_FAIL: return "power_fail";
	case FIRING_STATE_PAIRING: return "pairing";
	case FIRING_STATE_AP: return "access_point";
	default: return "";
	}
}

/**
 * @brief Returns the name associated with a given thermocouple type
 * 
 * @param thermocouple_type thermocouple type code
 * 
 * @returns Returns the name of the thermocouple
*/
static char *get_thermocouple_type_name(int32_t thermocouple_type)
{
	switch (thermocouple_type)
	{
	case THERMOCOUPLE_K: return "K";
	case THERMOCOUPLE_N: return "N";
	case THERMOCOUPLE_R: return "R";
	case THERMOCOUPLE_S: return "S";
	default: return "";
	}
}

/**
 * @brief Returns the name associated with a given event relay function
 * 
 * @param event_relay_function event relay function
 * 
 * @returns Returns the name associated with the function
*/
static char *get_event_relay_function_name(int32_t event_relay_function)
{
	switch (event_relay_function)
	{
	case EVENT_RELAY_OFF: return "off";
	case EVENT_RELAY_EVENT: return "event";
	case EVENT_RELAY_DAMPER: return "damper";
	case EVENT_RELAY_FAN: return "fan";
	default: return "";
	}
}

/**
 * @brief Returns the name associated with a given event log entry type
 * 
 * @param event_type event type code
 * 
 * @returns Returns the name of the event type
*/
static char *get_event_type_name(int32_t event_type)
{
    switch (event_type)
    {
    case EVENT_NONE: return "none";
    case EVENT_POWER_ON: return "power_on";
    case EVENT_PROGRAM_STARTED: return "start_prog";
    case EVENT_PROGRAM_STOPPED: return "stop_prog";
    case EVENT_CONTROLLER_ERROR: return "controller_error";
    case EVENT_PIC_LINK_ERROR: return "comms_1_error";
    case EVENT_ESP32_LINK_ERROR: return "comms_2_error";
    case EVENT_WIFI_CONNECTED: return "wifi_connected";
    case EVENT_WIFI_DISCONNECTED: return "wifi_disconnected";
    case EVENT_SERVER_ERROR: return "server_error";
    default: return "";
    }
}

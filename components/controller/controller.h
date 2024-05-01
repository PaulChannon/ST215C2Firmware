/**
 * @file controller.h
 * @brief Holds information about the state of the controller
*/
#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#include "common.h"

// Maximum length of string used to store controller information
#define CONTROLLER_STRING_LENGTH 20

// Maximum number of programs possible on a controller
#define MAX_PROGRAMS 32

// Maximum number of segments possible in a program
#define MAX_SEGMENTS 32

// Firing states
#define FIRING_STATE_INITIALISING          0
#define FIRING_STATE_IDLE                  1
#define FIRING_STATE_DELAY                 2
#define FIRING_STATE_RAMP_HEATING          7
#define FIRING_STATE_RAMP_HEATING_PAUSED   8
#define FIRING_STATE_RAMP_COOLING          9
#define FIRING_STATE_RAMP_COOLING_PAUSED   10
#define FIRING_STATE_SOAK                  11
#define FIRING_STATE_SOAK_PAUSED           12
#define FIRING_STATE_COOLING               13
#define FIRING_STATE_COOL                  14
#define FIRING_STATE_ERROR                 15
#define FIRING_STATE_SETUP                 16
#define FIRING_STATE_POWER_FAIL            17
#define FIRING_STATE_PAIRING               18
#define FIRING_STATE_AP                    19

#define IS_FIRING(state) (state >= FIRING_STATE_DELAY && state <= FIRING_STATE_SOAK_PAUSED)
#define IS_PAUSED(state) (state == FIRING_STATE_RAMP_HEATING_PAUSED || state == FIRING_STATE_RAMP_COOLING_PAUSED)

// Number of configuration settings
#define NUM_CONFIG_SETTINGS 63

// Configuration settings
#define CONFIG_TC_TYPE               0
#define CONFIG_ERR1_ENABLED          1
#define CONFIG_MAX_TEMP              2
#define CONFIG_LED_BRILL             3
#define CONFIG_ERR4_ENABLED          4
#define CONFIG_ERR5_ENABLED          5
#define CONFIG_MAX_HOURS             6
#define CONFIG_MAX_AMBIENT           7
#define CONFIG_PFR_ENABLED           8
#define CONFIG_PAUSED_HOURS_LIMIT    9
#define CONFIG_SP_OFFSET1            10
#define CONFIG_PID_P1                11
#define CONFIG_PID_I1                12
#define CONFIG_PID_D1                13
#define CONFIG_KILN1_KW              14
#define CONFIG_SP_OFFSET2            20
#define CONFIG_PID_P2                21
#define CONFIG_PID_I2                22
#define CONFIG_PID_D2                23
#define CONFIG_KILN2_KW              24
#define CONFIG_SP_OFFSET3            30
#define CONFIG_PID_P3                31
#define CONFIG_PID_I3                32
#define CONFIG_PID_D3                33
#define CONFIG_KILN3_KW              34
#define CONFIG_ZONES                 40
#define CONFIG_CONTROL_STRATEGY      41
#define CONFIG_LINKING_ACTIVATED     42
#define CONFIG_LE_ENABLED            43
#define CONFIG_CYCLE_TIME            44
#define CONFIG_RL3_FUNCTION          45
#define CONFIG_REMEMBER_START_DELAY  46
#define CONFIG_SKIP_START_DELAY      47
#define CONFIG_SAMPLE_RATE           50
#define CONFIG_USB_STRING            51
#define CONFIG_RF_MODE               52
#define CONFIG_PASSWORD4             53
#define CONFIG_CONFIG_ALL            55
#define CONFIG_EEPROM_ALTERED        58
#define CONFIG_EEPROM_INIT           59
#define CONFIG_DEGF                  60
#define CONFIG_PROGRAMS_CAP          61
#define CONFIG_SEGMENTS_CAP          62

// Thermocouple types
#define THERMOCOUPLE_K  0
#define THERMOCOUPLE_N  1
#define THERMOCOUPLE_R  2
#define THERMOCOUPLE_S  3

// Event relay functions
#define EVENT_RELAY_OFF     0
#define EVENT_RELAY_EVENT   1
#define EVENT_RELAY_DAMPER  2
#define EVENT_RELAY_FAN     3

// Event types
#define EVENT_NONE               0
#define EVENT_POWER_ON           1
#define EVENT_PROGRAM_STARTED    2
#define EVENT_PROGRAM_STOPPED    3
#define EVENT_CONTROLLER_ERROR   4
#define EVENT_PIC_LINK_ERROR     5
#define EVENT_ESP32_LINK_ERROR   6
#define EVENT_WIFI_CONNECTED     7
#define EVENT_WIFI_DISCONNECTED  8
#define EVENT_SERVER_ERROR       9

// Comms error codes
#define COMMS_ERROR_NONE               0
#define COMMS_ERROR_TIMEOUT            1
#define COMMS_ERROR_TOO_SHORT          2
#define COMMS_ERROR_TOO_LONG           3
#define COMMS_ERROR_WRONG_LENGTH       4
#define COMMS_ERROR_INVALID_RESPONSE   5
#define COMMS_ERROR_WRONG_CRC          6
#define COMMS_ERROR_LOCKUP             7
#define COMMS_ERROR_CANNOT_CONNECT     8
#define COMMS_ERROR_NO_RESPONSE        9

// Structure used to hold configuration information about a controller
typedef struct
{
    // PIC firmware version number, e.g. V2.01
    char pic_firmware_version[CONTROLLER_STRING_LENGTH];

    // STM32 firmware version number, e.g. V1.00
    char stm32_firmware_version[CONTROLLER_STRING_LENGTH];

    // Controller name, e.g. ST215
    char controller_name[CONTROLLER_STRING_LENGTH];

    // Thermocouple type (one of the TC_XX constants)
    uint8_t thermocouple_type;

    // Maximum temperature that the user can set (degC)
    uint16_t max_user_temperature;

    // Number of zones in use
    uint8_t zones_in_use;

    // Maximum number of programs
    uint8_t max_programs;

    // Maximum number of segments per program
    uint8_t max_segments;

    // Indicates the ramp rate scaling factor (1 for degC/hr or 10 for 0.1 degC/hr)
    uint8_t ramp_rate_scaling;

    // Indicates if the program and display temperature units are fahrenheit (as opposed to Celcius)
    uint8_t is_fahrenheit_units;

    // Event 1 relay functionality (0 - off, 1 - event, 2 - damper, 3 - fan)
    uint8_t event_relay_function_1;

    // Event 2 relay functionality (0 - off, 1 - event)
    uint8_t event_relay_function_2;

    // Configuration settings
    int16_t configuration_settings[NUM_CONFIG_SETTINGS];
}
__attribute__ ((packed)) _TControllerConfiguration;

// Structure used to hold CRC information for a program
typedef struct
{
    // Flag indicating if the CRC is known
    uint8_t crc_known;

    // Program CRC
    uint32_t crc;
}
__attribute__ ((packed)) _TCrcInfo;

// Structure used to hold status information about a controller
typedef struct
{
    // Current data and time (year is last two digits only, e.g. 2019 is 19)
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    // Current state of the controller (one of the FITING_STATE_XX constants)
    uint8_t firing_state;

    // Current error code (-1 for no error)
    int8_t error_code;

    // Ambient temperature (degC)
    float ambient_temperature;

    // Thermo-couple temperatures (degC or degF)
    float temperature_1;
    float temperature_2;
    float temperature_3;

    // Temperature set points (degC or degF)
    float temperature_set_point_1;
    float temperature_set_point_2;
    float temperature_set_point_3;

    // Current duty (%)
    float duty_1;
    float duty_2;
    float duty_3;

    // Energy used by each channel (kWh)
    float energy_used_1;
    float energy_used_2;
    float energy_used_3;

    // Total duty cycle applied to each channel (%)
    float total_duty_1;
    float total_duty_2;
    float total_duty_3;

    // Duty cycle from the proportional term applied to each channel (%)
    float prop_duty_1;
    float prop_duty_2;
    float prop_duty_3;

    // Duty cycle from the integral term applied to each channel (%)
    float int_duty_1;
    float int_duty_2;
    float int_duty_3;

    // Duty cycle from the differential term applied to each channel (%)
    float diff_duty_1;
    float diff_duty_2;
    float diff_duty_3;

    // Remaining soak time (minutes)
    uint16_t soak_remaining;

    // Bitmap of event relay states
    uint8_t event_relay_states;

    // Current program number
    uint8_t current_program;

    // Current segment number
    uint8_t current_segment;

    // Current start delay (minutes)
    uint16_t start_delay;

    // Remaining start delay time (minutes)
    uint16_t start_delay_remaining;

    // Indicates if the user has changed a program
    uint8_t program_changed;

    // Indicates if the user has changed a configuration setting
    uint8_t configuration_changed;

    // Number of event log entries
    uint16_t num_events;

    // ID of the last event in the event log (or 0 if there are no events in the event log)
    int32_t last_event_id;

    // Program CRC information
    _TCrcInfo program_crc_info[MAX_PROGRAMS];

    // Configuration CRC information
    _TCrcInfo configuration_crc_info;
}
__attribute__ ((packed)) _TControllerStatus;

// Structure used to hold information about a controller program segment
typedef struct
{
    // Ramp rate during heating/cooling phase (degC/hr units)
    uint16_t ramp_rate;

    // Target temperature at end of ramp sequence (degC)
    uint16_t target_temperature;

    // Time at which to soak (mins)
    uint16_t soak_time;

    // Bitmap of event flags
    uint8_t event_flags;
}
__attribute__ ((packed)) _TProgramSegment;

// Structure used to hold information about a controller program
typedef struct
{
    // Segment information
    _TProgramSegment segments[MAX_SEGMENTS];

    // The number of the program
    uint8_t program_number;

    // The number of segments used in the program
    uint8_t segments_used;
}
__attribute__ ((packed)) _TProgram;

// Structure used to hold information on an event
typedef struct
{
    // Unique event log entry identifier
    int32_t event_id;

    // Date and time at which the event was logged (year is last two digits only, e.g. 2019 is 19)
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    // Event type (one of the EVENT_XX constants)
    uint8_t event_type;

    // Current state of the controller (one of the FIRING_STATE_XX constants)
    uint8_t firing_state;

    // Ambient temperature (degC)
    float ambient_temperature;

    // Thermo-couple temperatures (degC or degF)
    float temperature_1;
    float temperature_2;
    float temperature_3;

    // Temperature set points (degC or degF)
    float temperature_set_point;

    // Current program number
    uint8_t current_program;

    // Current segment number
    uint8_t current_segment;

    // Current error code (-1 for no error)
    int8_t error_code;

    // Communications error code (one of the COMMS_ERROR_XXX constants)
    uint8_t comms_error_code;

    // Optional identification of the command that caused a communications error
    uint8_t comms_command_id;
}
__attribute__ ((packed)) _TEvent;

// Structure used to hold information about a controller
typedef struct
{
    // Controller MAC address
    char mac_address[CONTROLLER_STRING_LENGTH];

    // Indicates if controller configuration information is available
    uint8_t configuration_available;

    // Controller configuration
    _TControllerConfiguration configuration;

    // Indicates if controller status information is available
    uint8_t status_available;

    // Controller status
    _TControllerStatus status;
}
__attribute__ ((packed)) _TController;

extern _TController _controller;

extern void initialise_controller();
extern void update_controller();
extern uint32_t get_controller_crc();

#endif // _CONTROLLER_H_

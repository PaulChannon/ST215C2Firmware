/**
 * @file common.h
 * @brief Contains common project-wide definitions
*/
#ifndef _COMMON_H
#define _COMMON_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_log.h"

// Customers
#define STAFFORD 1
#define ROHDE    2

#define CUSTOMER STAFFORD

// Indicates if SSL should be used
#define USE_SSL 0

// Server web address
#if CUSTOMER == ROHDE
    #define SERVER_ADDRESS "app.rohde.eu"
#else
    #define SERVER_ADDRESS "www.kilnportal.co.uk"
#endif

// Debug server IP address used to override normal server address
#define DEBUG_SERVER     1
#define DEBUG_SERVER_IP  "192.168.68.122"

// Firmware version number
#define FIRMWARE_VERSION "V2.00"

// Comms protocol version number
#define PROTOCOL_VERSION  3

#endif // _COMMON_H

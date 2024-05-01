/**
 * @file i2c_driver.h
 * @brief Provides an interface to the I2C peripherals
*/
#ifndef _I2C_DRIVER_H
#define _I2C_DRIVER_H

#include "common.h"
#include "bsp_errors.h"

extern _TBspError initialise_i2c();
extern _TBspError write_i2c_data(uint16_t device_address, uint8_t *data, int32_t size);
extern _TBspError read_i2c_data(uint16_t device_address, uint8_t *data, int32_t size);
extern _TBspError write_i2c_memory(uint16_t device_address, uint16_t memory_address, uint8_t *data, int32_t size);
extern _TBspError read_i2c_memory(uint16_t device_address, uint16_t memory_address, uint8_t *data, int32_t size);
extern _TBspError scan_i2c();

#endif // _I2C_DRIVER_H

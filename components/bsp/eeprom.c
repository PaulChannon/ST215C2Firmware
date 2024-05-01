/**
 * @file eeprom.c
 * @brief Provides an interface to the CAT24C256 EEPROM devices connected via I2C1
*/
#include "eeprom.h"
#include "i2c_driver.h"

// I2C base device addresses
#define DEVICE_ADDR 0x50

/**
 * @brief Initialises the EEPROM
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError initialise_eeprom()
{
    return BSP_OK;
}

/**
 * @brief Writes a sequence of bytes of data to EEPROM
 * 
 * @param offset Address offset into the EEPROM area for the first byte
 * @param data Data to write
 * @param size Number of bytes to write
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError write_eeprom(int32_t offset, uint8_t *data, int32_t size)
{
	return write_i2c_memory(DEVICE_ADDR, (uint16_t)offset, data, size);
}

/**
 * @brief Reads a sequence of bytes of data from EEPROM
 * 
 * @param offset Address offset into the EEPROM area for the first byte
 * @param data Returned data
 * @param size Number of bytes to read
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError read_eeprom(int32_t offset, uint8_t *data, int32_t size)
{
	return read_i2c_memory(DEVICE_ADDR, (uint16_t)offset, data, size);
}


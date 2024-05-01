/**
 * @file eeprom.h
 * @brief Provides an interface to the CAT24C256 EEPROM device connected via I2C1
*/
#ifndef _EEPROM_H
#define _EEPROM_H

#include "common.h"
#include "bsp_errors.h"

extern _TBspError initialise_eeprom();
extern _TBspError write_eeprom(int32_t offset, uint8_t *data, int32_t size);
extern _TBspError read_eeprom(int32_t offset, uint8_t *data, int32_t size);

#endif // _EEPROM_H

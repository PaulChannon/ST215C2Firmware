/**
 * @file rtc.c
 * @brief Provides an interface to the real-time clock RV-3032-C7
*/
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "i2c_driver.h"
#include "bsp.h"
#include "rtc.h"

// Digital input pins
#define GPIO_CLKOUT  15

// Configuration mask
#define GPIO_MASK (1ULL << GPIO_CLKOUT)

// 7-bit I2C device address
#define DEVICE_ADDR  0x51

// RAM register addresses
#define SECONDS_100TH_REGISTER   0x00
#define SECONDS_REGISTER         0x01
#define MINUTES_REGISTER         0x02
#define HOURS_REGISTER           0x03
#define WEEKDAY_REGISTER         0x04
#define DAY_REGISTER             0x05
#define MONTH_REGISTER           0x06
#define YEAR_REGISTER            0x07
#define CONTROL_1_REGISTER       0x10
#define TEMPERATURE_LSB_REGISTER 0x0E
#define EEADDR_REGISTER          0x3D
#define EEDATA_REGISTER          0x3E
#define EECMD_REGISTER           0x3F

// EEPROM register addresses
#define EEPROM_PMU_REGISTER      0xC0
#define EEPROM_CLKOUT_2_REGISTER 0xC3

// Mask used to check if the EEPROM is busy
#define EEBUSY_MASK  0x04

// Control register 1 settings to disable and enable EEPROM auto refresh
#define DISABLE_AUTO_REFRESH  0x24
#define ENABLE_AUTO_REFRESH   0x20

// PMU register settings to enable level switch backup power mode
#define BPM_MASK                  0x30
#define BPM_LEVEL_SWITCHING_MODE  0x20

// CLKOUT 2 register settings to enable a 1Hz clock output
#define CLKOUT_1_HZ  0x60

// Command value to save registers to EEPROM
#define EEPROM_SAVE_COMMAND  0x21

// 1 Hz tick count
int32_t _count_1_hz = 0;

static _TBspError wait_for_eeprom();
static _TBspError read_register(uint8_t register_addr, uint8_t *value);
static _TBspError write_register(uint8_t register, uint8_t value);
static void gpio_isr_handler(void* arg);

/**
 * @brief Initialise the RTC
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError initialise_rtc()
{
    // Configure IO8 as a digital input with an internal pull-up resistor and generating interrupts
	// on a positive edge
	gpio_config_t io_config = {};
	io_config.intr_type = GPIO_INTR_POSEDGE;
	io_config.mode = GPIO_MODE_INPUT;
	io_config.pin_bit_mask = GPIO_MASK;
	io_config.pull_down_en = 0;
	io_config.pull_up_en = 1;
	gpio_config(&io_config);

    // Install an interrupt service
    gpio_install_isr_service(0);

	// Hook up the interrupt handler
    gpio_isr_handler_add(GPIO_CLKOUT, gpio_isr_handler, (void *)GPIO_CLKOUT);

	return BSP_OK;
}

/**
 * @brief Sets the RTC time and date
 * 
 * @param date_time _TDateTime structure holding the date and time
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError set_rtc_time_and_date(_TDateTime *date_time)
{
	_TBspError result;
	uint8_t write_data[8];

	// Write minutes and hours to the RTC
	write_data[0] = MINUTES_REGISTER;
	write_data[1] = date_time->minute;
	write_data[2] = date_time->hour;

	result = write_i2c_data(DEVICE_ADDR, write_data, 3);
	if (result != BSP_OK) return result;

	// Write date to the RTC
	write_data[0] = DAY_REGISTER;
	write_data[1] = date_time->day;
	write_data[2] = date_time->month;
	write_data[3] = date_time->year;

	result = write_i2c_data(DEVICE_ADDR, write_data, 4);
	if (result != BSP_OK) return result;

	return BSP_OK;
}

/**
 * @brief Reads the RTC time and date
 * 
* @param date_time _TDateTime structure in which the date and time will be placed
* 
* @returns BSP_OK or a BSP_XXX error code
*/
_TBspError read_rtc_time_and_date(_TDateTime *date_time)
{
	_TBspError result;
	uint8_t write_data[1] = { SECONDS_REGISTER };
	uint8_t read_data[7];

	// Read the temperature
	result = write_i2c_data(DEVICE_ADDR, write_data, 1);
	if (result != BSP_OK) return result;
	result = read_i2c_data(DEVICE_ADDR, read_data, 7);
	if (result != BSP_OK) return result;

	date_time->second = read_data[0];
	date_time->minute = read_data[1];
	date_time->hour = read_data[2];
	date_time->day = read_data[4];
	date_time->month = read_data[5];
	date_time->year = read_data[6];

	return result;
}

/**
 * @brief Gets the current 1 Hz tick count. Note that according to the data sheet this count may be 
 * affected when setting the date/time, i.e. a call to set_rtc_time_and_date
 *
 * @param arg User registered data
 * 
 * @returns The number of 1 second ticks since boot up
 */
int32_t get_rtc_seconds()
{
	return _count_1_hz;
}

/**
 * @brief Checks to see if the RTC is already configured
 * 
 * @param configured Returned as true if the RTC is already configured
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError check_rtc_configured(bool *configured)
{
	_TBspError result;
	uint8_t pmu_value;
	uint8_t clkout_2_value;

	// Read the clock output (CLKOUT 2) EEPROM register
	result = read_register(EEPROM_CLKOUT_2_REGISTER, &clkout_2_value);
	if (result != BSP_OK) return result;

	// Read the power management unit (PMU) EEPROM register
	result = read_register(EEPROM_PMU_REGISTER, &pmu_value);
	if (result != BSP_OK) return result;

	ESP_LOGI("RTC", "PMU register %02x", pmu_value);
	ESP_LOGI("RTC", "CLKOUT register %02x", clkout_2_value);

	// Check the BSM bits to see if level switching backup power mode is set
	*configured = ((pmu_value & BPM_MASK) == BPM_LEVEL_SWITCHING_MODE) && ((clkout_2_value & CLKOUT_1_HZ) == CLKOUT_1_HZ);

	return BSP_OK;
}

/**
 * @brief Configures the RTC, enabling battery standby operation and a 1Hz clock output
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError configure_rtc()
{
	_TBspError result;

	// Disable auto-refresh of RAM registers from EEPROM (Set EERD bit to 1, plus set unused bit 5 to 1)
	result = write_register(CONTROL_1_REGISTER, DISABLE_AUTO_REFRESH);
	if (result != BSP_OK) return result;
	delay_ms(50);

	// Wait for the EEPROM to be not busy
	wait_for_eeprom();

	// Enable level switching backup power mode by setting the BSM bits in the power management unit (PMU) EEPROM register
	result = write_register(EEPROM_PMU_REGISTER, BPM_LEVEL_SWITCHING_MODE);
	if (result != BSP_OK) return result;
	delay_ms(50);

	// Configure a 1Hz clock output setting the FD bits in the CLKOUT 2 EEPROM register
	result = write_register(EEPROM_CLKOUT_2_REGISTER, CLKOUT_1_HZ);
	if (result != BSP_OK) return result;
	delay_ms(50);

	// Save registers to EEPROM
	result = write_register(EECMD_REGISTER, EEPROM_SAVE_COMMAND);
	if (result != BSP_OK) return result;
	delay_ms(50);

	// Wait for the EEPROM to be written
	delay_ms(100);

	// Wait for the EEPROM to be not busy
	wait_for_eeprom();

	// Reenable auto-refresh of RAM registers from EEPROM
	result = write_register(CONTROL_1_REGISTER, ENABLE_AUTO_REFRESH);
	if (result != BSP_OK) return result;
	delay_ms(50);

	// Reset the tick count as it will have been disturbed by the configuration change
	_count_1_hz = 0;

	return BSP_OK;
}

/**
 * @brief Reads a value from a RAM register
 * 
 * @param register_addr Register address (one of the XXX_REGISTER values)
 * @param value Returned register value
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
static _TBspError read_register(uint8_t register_addr, uint8_t *value)
{
	_TBspError result;
	uint8_t write_data[1];
	uint8_t read_data[1];

	write_data[0] = register_addr;
	result = write_i2c_data(DEVICE_ADDR, write_data, 1);
	if (result != BSP_OK) return result;

	result = read_i2c_data(DEVICE_ADDR, read_data, 1);
	if (result != BSP_OK) return result;

	*value = read_data[0];

	return BSP_OK;
}

/**
 * @brief Writes a value to a RAM register
 * 
 * @param register_addr Register address (one of the XXX_REGISTER values)
 * @param value Value to write
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
static _TBspError write_register(uint8_t register_addr, uint8_t value)
{
	uint8_t write_data[2];

	write_data[0] = register_addr;
	write_data[1] = value;
	
	return write_i2c_data(DEVICE_ADDR, write_data, 2);
}

/**
 * @brief Waits for the EEPROM to be not busy
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
static _TBspError wait_for_eeprom()
{
	_TBspError result;
	uint8_t value;
	int32_t timeout;

	// Wait for 100ms for the EEBUSY flag (bit 2) in the temperature register to clear
	timeout = 10;
	while (--timeout > 0)
	{
		result = read_register(TEMPERATURE_LSB_REGISTER, &value);
		if (result != BSP_OK) return result;
		if ((value & EEBUSY_MASK) == 0) return BSP_OK;

		delay_ms(10);
	}

	return BSP_RTC_ERROR;
}

/**
 * @brief Interrupt service routine called at 1 Hz by the RTC chip
 */
static void gpio_isr_handler(void* arg)
{
	_count_1_hz++;
}


/**
 * @file i2c_driver.c
 * @brief Provides an interface to the I2C peripherals
*/
#include "driver/i2c.h"
#include "i2c_driver.h"

// I2C pin numbers
#define GPIO_I2C1_SCL  1
#define GPIO_I2C1_SDA  2
#define GPIO_I2C2_SCL  17
#define GPIO_I2C2_SDA  18

// I2C peripheral numbers
#define I2C1_PERIPHERAL  0

// Maximum time to wait for device to respond (ms)
#define I2C_MASTER_TIMEOUT_MS  100

/**
 * @brief Initialises the I2C interface to the external ADC
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError initialise_i2c()
{
	i2c_config_t i2c1_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = GPIO_I2C1_SDA,
		.scl_io_num = GPIO_I2C1_SCL,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 400000,
	};

	ESP_ERROR_CHECK(i2c_param_config(I2C1_PERIPHERAL, &i2c1_config));
	ESP_ERROR_CHECK(i2c_driver_install(I2C1_PERIPHERAL, I2C_MODE_MASTER, 0, 0, 0));

	return BSP_OK;
}

/**
 * @brief Writes data to the given I2C channel
 * 
 * @param device_address Address of the device to write to
 * @param data data buffer to write
 * @param size size of the data buffer in bytes
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError write_i2c_data(uint16_t device_address,
						  uint8_t *data,
						  int32_t size)
{
	esp_err_t status;

	i2c_cmd_handle_t command = i2c_cmd_link_create();
	i2c_master_start(command);
	i2c_master_write_byte(command, (device_address << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write(command, data, size, true);
	i2c_master_stop(command);
	status = i2c_master_cmd_begin(I2C1_PERIPHERAL,
			                      command,
								  I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(command);

	if (status != ESP_OK) return BSP_I2C_ERROR;

	return BSP_OK;
}

/**
 * @brief Reads data from the I2C
 * 
 * @param device_address Address of the device to read from
 * @param data data buffer to read into
 * @param size number of bytes to read
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError read_i2c_data(uint16_t device_address,
						 uint8_t *data,
						 int32_t size)
{
	esp_err_t status;

	i2c_cmd_handle_t command = i2c_cmd_link_create();
	i2c_master_start(command);
	i2c_master_write_byte(command, (device_address << 1) | I2C_MASTER_READ, true);
	i2c_master_read(command, data, size, I2C_MASTER_LAST_NACK);
	i2c_master_stop(command);
	status = i2c_master_cmd_begin(I2C1_PERIPHERAL,
			                      command,
								  I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(command);

	if (status != ESP_OK) return BSP_I2C_ERROR;

	return BSP_OK;
}

/**
 * @brief Writes to memory using the I2C
 * 
 * @param device_address Address of the device to write to
 * @param memory_address Memory address to write to
 * @param data data buffer to write
 * @param size size of the data buffer in bytes
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError write_i2c_memory(uint16_t device_address,
 						    uint16_t memory_address,
							uint8_t *data,
							int32_t size)
{
	esp_err_t status;

	i2c_cmd_handle_t command = i2c_cmd_link_create();
	i2c_master_start(command);
	i2c_master_write_byte(command, (device_address << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(command, (uint8_t)(memory_address >> 8), true);
	i2c_master_write_byte(command, (uint8_t)(memory_address & 0x00FF), true);
	i2c_master_write(command, data, size, I2C_MASTER_LAST_NACK);
//	i2c_master_read(command, data, read_size, I2C_MASTER_LAST_NACK);
	i2c_master_stop(command);
	status = i2c_master_cmd_begin(I2C1_PERIPHERAL,
			                      command,
								  I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(command);

	if (status != ESP_OK) return BSP_I2C_ERROR;

	return BSP_OK;
}

/**
 * @brief Reads from memory using the I2C
 * 
 * @param device_address Address of the device to read from
 * @param memory_address Memory address to read from
 * @param data data buffer to read into
 * @param size number of bytes to read
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError read_i2c_memory(uint16_t device_address,
						   uint16_t memory_address,
						   uint8_t *data,
						   int32_t size)
{
	esp_err_t status;

	i2c_cmd_handle_t command = i2c_cmd_link_create();
	i2c_master_start(command);
	i2c_master_write_byte(command, (device_address << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(command, (uint8_t)(memory_address >> 8), true);
	i2c_master_write_byte(command, (uint8_t)(memory_address & 0x00FF), true);
	i2c_master_start(command);
	i2c_master_write_byte(command, (device_address << 1) | I2C_MASTER_READ, true);
	i2c_master_read(command, data, size, I2C_MASTER_LAST_NACK);
	i2c_master_stop(command);
	status = i2c_master_cmd_begin(I2C1_PERIPHERAL,
			                      command,
								  I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(command);

	if (status != ESP_OK) return BSP_I2C_ERROR;

	return BSP_OK;
}


/**
 * @brief Scans an I2C channel for connected devices and displays a table in the monitor window
 * 
 * @returns BSP_OK or a BSP_XXX error code
*/
_TBspError scan_i2c()
{
	uint8_t address;
	printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
	for (int i = 0; i < 128; i += 16) {
		printf("%02x: ", i);
		for (int j = 0; j < 16; j++) {
			fflush(stdout);
			address = i + j;
			i2c_cmd_handle_t cmd = i2c_cmd_link_create();
			i2c_master_start(cmd);
			i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, 0x1);
			i2c_master_stop(cmd);
			esp_err_t ret = i2c_master_cmd_begin(I2C1_PERIPHERAL, cmd, 50 / portTICK_PERIOD_MS);
			i2c_cmd_link_delete(cmd);
			if (ret == ESP_OK) {
				printf("%02x ", address);
			} else if (ret == ESP_ERR_TIMEOUT) {
				printf("UU ");
			} else {
				printf("-- ");
			}
		}
		printf("\r\n");
	}

	return BSP_OK;
}

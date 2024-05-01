idf_component_register(SRCS "timers.c" "debug_io.c" "rtc.c" "bsp.c" "i2c_driver.c" "eeprom.c" "rtc.c" 
					   REQUIRES spi_flash driver esp_timer
                       INCLUDE_DIRS "." "..")

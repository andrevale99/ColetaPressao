#include "ads111x.h"

static esp_err_t xErrorCheck = ESP_FAIL;

esp_err_t ads111x_begin(i2c_master_bus_handle_t *master_handle, uint8_t address)
{
    i2c_device_config_t device =
        {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = address,
            .scl_speed_hz = 100000,
        };

    xErrorCheck = i2c_master_bus_add_device(*master_handle, &device, &xads111x_dev_handle)
}

esp_err_t ads111x_get_conversion(void)
{
}

esp_err_t ads111x_set_config(void)
{
}
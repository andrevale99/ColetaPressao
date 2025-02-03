#include "ads111x.h"

#include <memory.h>

#define TIMEOUT_MS 500
#define BUFFER_SIZE 3

static esp_err_t xErrorCheck = ESP_FAIL;

static uint8_t buffer[3] = {0, 0};

esp_err_t ads111x_begin(i2c_master_bus_handle_t *master_handle, uint8_t address, ads111x_struct_t *ads)
{
    i2c_device_config_t device =
        {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = address,
            .scl_speed_hz = 100000, // 100 kHz
        };

    xErrorCheck = i2c_master_bus_add_device(*master_handle, &device, &(ads->dev_handle));

    return xErrorCheck;
}

esp_err_t ads111x_set_gain(ads111x_gain_t gain, ads111x_struct_t *ads)
{
    memset(&buffer[0], 0, (sizeof(uint8_t) * BUFFER_SIZE));

    buffer[0] = ADS111X_ADDR_CONFIG_REG;

    xErrorCheck = i2c_master_transmit_receive(ads->dev_handle,
                                              &buffer[0], 1,
                                              &buffer[1], 1, TIMEOUT_MS);
    buffer[1] &= ~(0x3 << OFFSET_GAIN);

    buffer[1] |= (gain << OFFSET_GAIN);

    xErrorCheck = i2c_master_transmit(ads->dev_handle, &buffer[0],
                                      sizeof(uint8_t) * 2, TIMEOUT_MS);

    return xErrorCheck;
}

esp_err_t ads111x_set_mode(ads111x_mode_t mode, ads111x_struct_t *ads)
{
    memset(&buffer[0], 0, (sizeof(uint8_t) * BUFFER_SIZE));

    buffer[0] = ADS111X_ADDR_CONFIG_REG;

    xErrorCheck = i2c_master_transmit_receive(ads->dev_handle,
                                              &buffer[0], 1,
                                              &buffer[1], 1, TIMEOUT_MS);

    buffer[1] &= ~(1 << OFFSET_MODE);
    buffer[1] |= (mode << OFFSET_MODE);

    xErrorCheck = i2c_master_transmit(ads->dev_handle, &buffer[0],
                                      sizeof(uint8_t) * 2, TIMEOUT_MS);

    return xErrorCheck;
}

esp_err_t ads111x_set_data_rate(ads111x_data_rate_t rate, ads111x_struct_t *ads)
{
    memset(&buffer[0], 0, (sizeof(uint8_t) * BUFFER_SIZE));

    buffer[0] = ADS111X_ADDR_CONFIG_REG;

    xErrorCheck = i2c_master_transmit_receive(ads->dev_handle,
                                              &buffer[0], 1,
                                              &buffer[1], 1, TIMEOUT_MS);

    buffer[1] &= ~(0x3 << OFFSET_DATA_RATE);
    buffer[1] |= (rate << OFFSET_DATA_RATE);

    xErrorCheck = i2c_master_transmit(ads->dev_handle, &buffer[0],
                                      sizeof(uint8_t) * 2, TIMEOUT_MS);

    return xErrorCheck;
}

esp_err_t ads111x_set_input_mux(ads111x_mux_t mux, ads111x_struct_t *ads)
{
    memset(&buffer[0], 0, (sizeof(uint8_t) * BUFFER_SIZE));

    buffer[0] = ADS111X_ADDR_CONFIG_REG;

    xErrorCheck = i2c_master_transmit_receive(ads->dev_handle,
                                              &buffer[0], 1,
                                              &buffer[1], 1, TIMEOUT_MS);

    buffer[1] &= ~(0x3 << OFFSET_INPUT_MUX);
    buffer[1] |= (mux << OFFSET_INPUT_MUX);

    xErrorCheck = i2c_master_transmit(ads->dev_handle, &buffer[0],
                                      sizeof(uint8_t) * 2, TIMEOUT_MS);

    return xErrorCheck;
}

uint8_t ads111x_get_conf_reg(ads111x_struct_t *ads)
{
    memset(&buffer[0], 0, (sizeof(uint8_t) * BUFFER_SIZE));

    buffer[0] = ADS111X_ADDR_CONFIG_REG;

    uint8_t reg = i2c_master_transmit_receive(ads->dev_handle,
                                              &buffer[0], 1,
                                              &buffer[1], 1, TIMEOUT_MS);

    return reg;
}

void ads111x_get_conversion(ads111x_struct_t *ads)
{
    memset(&buffer[0], 0, (sizeof(uint8_t) * BUFFER_SIZE));

    buffer[0] = ADS111X_ADDR_CONFIG_REG;

    if (buffer[0] & (1 << OFFSET_OPERATION_STATUS))
    {
        buffer[0] = ADS111X_ADDR_CONVERSION_REG;
        i2c_master_transmit_receive(ads->dev_handle, &buffer[0], 1,
                                    &buffer[1], 2, TIMEOUT_MS);

        ads->conversion = (buffer[1] << 8) | buffer[2];
    }
}
/**
 * @brief Biblioteca baseada na ESP-IDF-ib de
 * @author UncleRus
 */

#ifndef ADS111X_H
#define ADS111X_H

#include <driver/i2c_master.h>

#define ADS111X_ADDR 0x48
#define ADS111X_ADDR_VDD 0x49
#define ADS111X_ADDR_SCL 0x4B
#define ADS111X_ADDR_SDA 0x4A

#define ADS111X_ADDR_CONVERSION_REG 0x00
#define ADS111X_ADDR_CONFIG_REG 0x01
#define ADS111X_ADDR_LO_THRESH_REG 0x02
#define ADS111X_ADDR_HI_THRESH_REG 0x03

#define OFFSET_DATA_RATE 5
#define OFFSET_MODE 0
#define OFFSET_GAIN 1
#define OFFSET_INPUT_MUX 4
#define OFFSET_OPERATION_STATUS 7

typedef enum
{
    ADS111X_GAIN_6V144 = 0, //!< +-6.144V
    ADS111X_GAIN_4V096,     //!< +-4.096V
    ADS111X_GAIN_2V048,     //!< +-2.048V (default)
    ADS111X_GAIN_1V024,     //!< +-1.024V
    ADS111X_GAIN_0V512,     //!< +-0.512V
    ADS111X_GAIN_0V256,     //!< +-0.256V
    ADS111X_GAIN_0V256_2,   //!< +-0.256V (same as ADS111X_GAIN_0V256)
    ADS111X_GAIN_0V256_3,   //!< +-0.256V (same as ADS111X_GAIN_0V256)
} ads111x_gain_t;

typedef enum
{
    ADS111X_MUX_0_1 = 0, //!< positive = AIN0, negative = AIN1 (default)
    ADS111X_MUX_0_3,     //!< positive = AIN0, negative = AIN3
    ADS111X_MUX_1_3,     //!< positive = AIN1, negative = AIN3
    ADS111X_MUX_2_3,     //!< positive = AIN2, negative = AIN3
    ADS111X_MUX_0_GND,   //!< positive = AIN0, negative = GND
    ADS111X_MUX_1_GND,   //!< positive = AIN1, negative = GND
    ADS111X_MUX_2_GND,   //!< positive = AIN2, negative = GND
    ADS111X_MUX_3_GND,   //!< positive = AIN3, negative = GND
} ads111x_mux_t;

typedef enum
{
    ADS111X_DATA_RATE_8 = 0, //!< 8 samples per second
    ADS111X_DATA_RATE_16,    //!< 16 samples per second
    ADS111X_DATA_RATE_32,    //!< 32 samples per second
    ADS111X_DATA_RATE_64,    //!< 64 samples per second
    ADS111X_DATA_RATE_128,   //!< 128 samples per second (default)
    ADS111X_DATA_RATE_250,   //!< 250 samples per second
    ADS111X_DATA_RATE_475,   //!< 475 samples per second
    ADS111X_DATA_RATE_860    //!< 860 samples per second
} ads111x_data_rate_t;

typedef enum
{
    ADS111X_MODE_CONTINUOUS = 0, //!< Continuous conversion mode
    ADS111X_MODE_SINGLE_SHOT     //!< Power-down single-shot mode (default)
} ads111x_mode_t;

typedef enum
{
    ADS111X_COMP_MODE_NORMAL = 0, //!< Traditional comparator with hysteresis (default)
    ADS111X_COMP_MODE_WINDOW      //!< Window comparator
} ads111x_comp_mode_t;

typedef enum
{
    ADS111X_COMP_POLARITY_LOW = 0, //!< Active low (default)
    ADS111X_COMP_POLARITY_HIGH     //!< Active high
} ads111x_comp_polarity_t;

typedef enum
{
    ADS111X_COMP_LATCH_DISABLED = 0, //!< Non-latching comparator (default)
    ADS111X_COMP_LATCH_ENABLED       //!< Latching comparator
} ads111x_comp_latch_t;

typedef enum
{
    ADS111X_COMP_QUEUE_1 = 0,   //!< Assert ALERT/RDY pin after one conversion
    ADS111X_COMP_QUEUE_2,       //!< Assert ALERT/RDY pin after two conversions
    ADS111X_COMP_QUEUE_4,       //!< Assert ALERT/RDY pin after four conversions
    ADS111X_COMP_QUEUE_DISABLED //!< Disable comparator (default)
} ads111x_comp_queue_t;

typedef struct
{
    uint16_t conversion;

    i2c_master_dev_handle_t dev_handle;

} ads111x_struct_t;

esp_err_t ads111x_begin(i2c_master_bus_handle_t *mastet_handle, uint8_t address, ads111x_struct_t *ads);
esp_err_t ads111x_set_gain(ads111x_gain_t gain, ads111x_struct_t *ads);
esp_err_t ads111x_set_mode(ads111x_mode_t mode, ads111x_struct_t *ads);
esp_err_t ads111x_set_data_rate(ads111x_data_rate_t rate, ads111x_struct_t *ads);
esp_err_t ads111x_set_input_mux(ads111x_mux_t mux, ads111x_struct_t *ads);

void ads111x_get_conversion(ads111x_struct_t *ads);

#endif
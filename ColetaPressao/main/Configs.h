#ifndef CONFIG_H
#define CONFIG_H

#include <driver/i2c_master.h>
#include <driver/gptimer.h>
#include <driver/ledc.h>
#include <driver/pulse_cnt.h>

#include <esp_check.h>

#define TIMER_RESOLUTION_HZ 1000000

#define LED_SD GPIO_NUM_26

#define BOMBA_GPIO GPIO_NUM_13

#define PULSE_COUNTER_GPIO GPIO_NUM_14
#define PULSE_COUNTER_MIN_LIMITE -1
#define PULSE_COUNTER_MAX_LIMITE 10000

/**
 *  @brief Funcao de Configuracao do I2C
 */
esp_err_t I2C_config(i2c_master_bus_handle_t *handle_I2C_master)
{
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = CONFIG_COLETA_PRESSAO_SCL_PIN,
        .sda_io_num = CONFIG_COLETA_PRESSAO_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, handle_I2C_master));

    return ESP_OK;
}

/**
 *  @brief Funcao de Configuracao do SPI e
 *  manipulacao de arquivos com o SD
 */
esp_err_t SD_config(esp_vfs_fat_sdmmc_mount_config_t *mount_sd,
                    sdmmc_host_t *host,
                    sdspi_device_config_t *slot_config)
{
    mount_sd->format_if_mount_failed = false;
    mount_sd->max_files = 5;
    mount_sd->max_files = 5;
    mount_sd->allocation_unit_size = 16 * 1024;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CONFIG_COLETA_PRESSAO_MOSI_PIN,
        .miso_io_num = CONFIG_COLETA_PRESSAO_MISO_PIN,
        .sclk_io_num = CONFIG_COLETA_PRESSAO_CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    if (spi_bus_initialize(host->slot, &bus_cfg, SDSPI_DEFAULT_DMA) != ESP_OK)
    {
        ESP_LOGE("[SDMMC_config]", "Failed to initialize bus.");
        return ESP_ERR_INVALID_ARG;
    }

    slot_config->gpio_cs = CONFIG_COLETA_PRESSAO_CS_PIN;
    slot_config->host_id = host->slot;

    return ESP_OK;
}

/**
 *  @brief Funcao de Configuracao dos GPIOS
 */
esp_err_t GPIO_config(void)
{

    gpio_config_t config =
        {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_INPUT_OUTPUT,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pin_bit_mask = (1 << LED_SD),
        };

    return gpio_config(&config);
}

/**
 * @brief Configuração do Timer
 *
 * @note A resolução padrao do TIMER eh de 1 MHz
 * (T = 1/1e6 = 1us)
 */
esp_err_t Timer_config(gptimer_handle_t *handle_Timer)
{
    gptimer_config_t config =
        {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            .resolution_hz = TIMER_RESOLUTION_HZ, // 1 MHz = 1 us
        };

    ESP_ERROR_CHECK(gptimer_new_timer(&config, handle_Timer));

    return ESP_OK;
}

esp_err_t PWM_config(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 1,
        .freq_hz = 1 * 1000 * 10, // Set output frequency at 10 kHz
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = 1,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = BOMBA_GPIO,
        .duty = 0, // Set duty to 0%
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    return ESP_OK;
}

esp_err_t PULSE_COUNTER_config(pcnt_unit_handle_t *handle_pcnt)
{
    pcnt_unit_config_t unitConfig =
        {
            .high_limit = PULSE_COUNTER_MAX_LIMITE,
            .low_limit = PULSE_COUNTER_MIN_LIMITE,
        };
    ESP_ERROR_CHECK(pcnt_new_unit(&unitConfig, handle_pcnt));

    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(*handle_pcnt, &filter_config));

    pcnt_channel_handle_t pcnt_chan_a = NULL;
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = PULSE_COUNTER_GPIO,
        .level_gpio_num = -1,
    };
    ESP_ERROR_CHECK(pcnt_new_channel(*handle_pcnt, &chan_a_config, &pcnt_chan_a));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a,
                                                 PCNT_CHANNEL_EDGE_ACTION_HOLD,
                                                 PCNT_CHANNEL_EDGE_ACTION_INCREASE));

    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a,
                                                  PCNT_CHANNEL_LEVEL_ACTION_HOLD,
                                                  PCNT_CHANNEL_LEVEL_ACTION_KEEP));

    ESP_ERROR_CHECK(pcnt_unit_enable(*handle_pcnt));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(*handle_pcnt));
    ESP_ERROR_CHECK(pcnt_unit_start(*handle_pcnt));

    return ESP_OK;
}

#endif
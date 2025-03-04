#ifndef CONFIG_H
#define CONFIG_H

#include <driver/i2c_master.h>
#include <driver/gptimer.h>

#include <esp_check.h>

#define TIMER_RESOLUTION_HZ 1000000 / 2

#define LED_SD GPIO_NUM_17

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
            .mode = GPIO_MODE_OUTPUT,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pin_bit_mask = (1 << LED_SD),
        };

    return gpio_config(&config);
}

/**
 *  @brief Configuração do Timer para contabilizar
 *  o tempo decorrido de um dado para o outro, para
 *  depois ser gravado no SD.
 */
esp_err_t Timer_config(gptimer_handle_t *handle_Timer)
{
    gptimer_config_t config =
        {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            .resolution_hz = TIMER_RESOLUTION_HZ, // 1 kHz = 1 ms
        };

    ESP_ERROR_CHECK(gptimer_new_timer(&config, handle_Timer));

    gptimer_set_raw_count(*handle_Timer, 0);

    gptimer_enable(*handle_Timer);
    gptimer_start(*handle_Timer);

    return ESP_OK;
}

#endif
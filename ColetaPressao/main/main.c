#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <driver/i2c_master.h>

#include <esp_check.h>

//============================================
//  VARS
//============================================

//============================================
//  PROTOTIPOS e VARS_RELACIONADAS
//============================================

/**
 *  @brief Task para o ADS1115
 * 
 *  @param pvArg Ponteiro dos argumentos, caso precise fazer alguma 
 *  configuracao
 */
static void vTaskADS1115(void *pvArg);
TaskHandle_t xTaskHandle_ADS115 = NULL;

/**
 *  @brief Funcao de Configuracao do I2C
 */
static esp_err_t I2C_config(void);
i2c_master_bus_handle_t xI2C_master_handle = NULL;

//============================================
//  MAIN
//============================================
void app_main(void)
{
    I2C_config();

    xTaskCreate(vTaskADS1115, "ADS115 TASK", configMINIMAL_STACK_SIZE + 1024*1, 
                NULL, 1, &xTaskHandle_ADS115);

    // Caso precise da appmain
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

//============================================
//  FUNCS
//============================================
static void vTaskADS1115(void *pvArg)
{
    while(1)
    {
        ESP_LOGW( "[ADS1115]","%s\n", esp_err_to_name(i2c_master_probe(xI2C_master_handle, 0x48, 1000)));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static esp_err_t I2C_config(void)
{
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = CONFIG_COLETA_PRESSAO_SCL_PIN,
        .sda_io_num = CONFIG_COLETA_PRESSAO_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &xI2C_master_handle));

    return ESP_OK;
}
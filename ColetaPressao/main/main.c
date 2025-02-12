#include <stdio.h>
#include <math.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <driver/i2c_master.h>

#include <esp_check.h>

#include "ads111x.h"

//============================================
//  VARS GLOBAIS
//============================================
int16_t adc0 = 0;
int16_t adc1 = 0;
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
TaskHandle_t handleTask_ADS115 = NULL;
const char *TAG_ADS = "[ADS111]";

static void vTaskProcessADS(void *pvArg);
TaskHandle_t handleTask_ProcessADS = NULL;
const char *TAG_PROCESS_ADS = "[PROCESS_ADS]";

/**
 *  @brief Funcao de Configuracao do I2C
 */
static esp_err_t I2C_config(void);
i2c_master_bus_handle_t handle_I2Cmaster = NULL;

//============================================
//  MAIN
//============================================
void app_main(void)
{
    I2C_config();

    xTaskCreate(vTaskADS1115, "ADS115 TASK", configMINIMAL_STACK_SIZE + 1024 * 2,
                NULL, 1, &handleTask_ADS115);

    xTaskCreate(vTaskProcessADS, "PROCESS ADS TASK", configMINIMAL_STACK_SIZE + 1024 * 10,
                NULL, 1, &handleTask_ProcessADS);

    // Caso precise da appmain
    // while (1)
    // {
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
}

//============================================
//  FUNCS
//============================================
static void vTaskADS1115(void *pvArg)
{
    ads111x_struct_t ads;

    ESP_LOGI(TAG_ADS, "BEGIN: %s", esp_err_to_name(ads111x_begin(&handle_I2Cmaster, ADS111X_ADDR, &ads)));
    ads111x_set_gain(ADS111X_GAIN_4V096, &ads);
    ads111x_set_mode(ADS111X_MODE_SINGLE_SHOT, &ads);
    ads111x_set_data_rate(ADS111X_DATA_RATE_128, &ads);

    while (1)
    {
        ads111x_set_input_mux(ADS111X_MUX_0_GND, &ads);
        ads111x_get_conversion_sigle_ended(&ads);
        adc0 = ads.conversion;

        ads111x_set_input_mux(ADS111X_MUX_1_GND, &ads);
        ads111x_get_conversion_sigle_ended(&ads);
        adc1 = ads.conversion;

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void vTaskProcessADS(void *pvArg)
{
    float v_0 = 0.0;
    float v_1 = 0.0;

    float p_0 = 0.0;
    float p_1 = 0.0;

    float c_0 = 0.0;
    float c_1 = 0.0;

    int cont = 0;

    while (1)
    {
        v_0 = (adc0 * 0.1875) / 1000;
        v_1 = (adc1 * 0.1875) / 1000;

        p_0 = (((v_0 / 5) - 0.04) / 0.018) + c_0;
        p_1 = (((v_1 / 5) - 0.04) / 0.018) + c_1;

        if (cont == 20)
        {
            if (round(p_0 * 100) > 1 || round(p_0 * 100) < -1)
            {
                vTaskDelay(pdMS_TO_TICKS(500));
                c_0 = -p_0;
            }
            if (round(p_1 * 100) > 1 || round(p_1 * 100) < -1)
            {
                vTaskDelay(pdMS_TO_TICKS(500));
                c_1 = -p_1;
            }
            else
            {
            }
        }

        if (cont < 101)
        {
            cont += 1;
        }

        printf("%d\n", adc0);
        fflush(stdout);

        vTaskDelay(pdMS_TO_TICKS(10));
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

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &handle_I2Cmaster));

    return ESP_OK;
}
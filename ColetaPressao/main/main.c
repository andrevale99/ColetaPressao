#include <stdio.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <driver/i2c_master.h>

#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

#include <esp_check.h>

#include "ads111x.h"

#define MOUNT_POINT "/sdcard"

#define BUFFER_SIZE 256

//============================================
//  VARS GLOBAIS
//============================================
int16_t adc0 = 0;
int16_t adc1 = 0;

float p_0 = 0.03;
float p_1 = 0.01;

uint64_t contador_tabela = 0;

char buffer[BUFFER_SIZE];

SemaphoreHandle_t Semaphore_ADS_to_SD = NULL;
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

/**
 *  @brief Task para o calculo da pressao
 *
 *  @param pvArg Ponteiro dos argumentos, caso precise fazer alguma
 *  configuracao
 */
static void vTaskProcessADS(void *pvArg);
TaskHandle_t handleTask_ProcessADS = NULL;
const char *TAG_PROCESS_ADS = "[PROCESS_ADS]";

/**
 *  @brief Task para gravar os dados no SD
 *
 *  @param pvArg Ponteiro dos argumentos, caso precise fazer alguma
 *  configuracao
 */
static void vTaskSDMMC(void *pvArg);
TaskHandle_t handleTask_SDMMC = NULL;
const char *TAG_SDMMC = "[SDMMC]";

/**
 *  @brief Funcao de Configuracao do I2C
 */
static esp_err_t I2C_config(void);
i2c_master_bus_handle_t handle_I2Cmaster = NULL;

/**
 *  @brief Funcao de Configuracao do SPI e
 *  manipulacao de arquivos com o SD
 */
static esp_err_t SD_config(void);
esp_vfs_fat_sdmmc_mount_config_t mount_sd;
sdmmc_card_t *card;
sdmmc_host_t host = SDSPI_HOST_DEFAULT();
sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();

/**
 *  @brief Funcao de Configuracao da UART
 *  para poder enviar os comandos para ESP32
 */
static esp_err_t UART_config(void);

//============================================
//  MAIN
//============================================
void app_main(void)
{
    I2C_config();
    SD_config();

    Semaphore_ADS_to_SD = xSemaphoreCreateBinary();

    // xTaskCreate(vTaskADS1115, "ADS115 TASK", configMINIMAL_STACK_SIZE + 1024 * 2,
    //             NULL, 1, &handleTask_ADS115);

    // xTaskCreate(vTaskProcessADS, "PROCESS ADS TASK", configMINIMAL_STACK_SIZE + 1024 * 10,
    //             NULL, 1, &handleTask_ProcessADS);

    xTaskCreate(vTaskSDMMC, "PROCESS SD MMC", configMINIMAL_STACK_SIZE + 1024 * 2,
                NULL, 1, &handleTask_SDMMC);
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
    ads111x_set_data_rate(ADS111X_DATA_RATE_32, &ads);

    while (1)
    {
        ads111x_set_input_mux(ADS111X_MUX_0_GND, &ads);
        ads111x_get_conversion_sigle_ended(&ads);
        adc0 = ads.conversion;

        ads111x_set_input_mux(ADS111X_MUX_1_GND, &ads);
        ads111x_get_conversion_sigle_ended(&ads);
        adc1 = ads.conversion;

        vTaskDelay(pdMS_TO_TICKS(32));
    }
}

static void vTaskProcessADS(void *pvArg)
{
    float v_0 = 0.0;
    float v_1 = 0.0;

    float c_0 = 0.0;
    float c_1 = 0.0;

    uint8_t cont = 0;

    while (1)
    {
        v_0 = (adc0 * 0.1875) / 1000;
        v_1 = (adc1 * 0.1875) / 1000;

        p_0 = (((v_0 / 5) - 0.04) / 0.018) + c_0;
        p_1 = (((v_1 / 5) - 0.04) / 0.018) + c_1;

        if (cont == 20)
        {
            if (round(p_0 * 100) > 1 || round(p_0 * 100) < -1)
                c_0 = -p_0;
            if (round(p_1 * 100) > 1 || round(p_1 * 100) < -1)
                c_1 = -p_1;
        }

        if (cont < 101)
            cont += 1;

        printf("%lld\t%0.2f\t%0.2f\n", contador_tabela, p_0, p_1);
        fflush(stdout);

        contador_tabela++;

        xSemaphoreGive(Semaphore_ADS_to_SD);

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

static void vTaskSDMMC(void *pvArg)
{
    const char mount_point[] = MOUNT_POINT;

    ESP_LOGW(TAG_SDMMC, "SD MMC mount: %s",
             esp_err_to_name(esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_sd, &card)));

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    const char *file_name = MOUNT_POINT "/data.txt";

    FILE *arq = fopen(file_name, "a+");

    while (1)
    {
        if (arq != NULL)
            break;

        vTaskDelay(1000);
        arq = fopen(file_name, "a+");
    }

    snprintf(buffer, BUFFER_SIZE, "Contador\tP0\tP1\n");
    fprintf(arq, buffer);

    while (1)
    {
        if (xSemaphoreTake(Semaphore_ADS_to_SD, 10) == pdTRUE)
        {
            snprintf(buffer, BUFFER_SIZE, "%lld\t%0.2f\t%0.2f\n", contador_tabela, p_0, p_1);
            fprintf(arq, buffer);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    fclose(arq);
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

static esp_err_t SD_config(void)
{
    mount_sd.format_if_mount_failed = false;
    mount_sd.max_files = 5;
    mount_sd.max_files = 5;
    mount_sd.allocation_unit_size = 16 * 1024;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CONFIG_COLETA_PRESSAO_MOSI_PIN,
        .miso_io_num = CONFIG_COLETA_PRESSAO_MISO_PIN,
        .sclk_io_num = CONFIG_COLETA_PRESSAO_CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    if (spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA) != ESP_OK)
    {
        ESP_LOGE("[SDMMC_config]", "Failed to initialize bus.");
        return ESP_ERR_INVALID_ARG;
    }

    slot_config.gpio_cs = CONFIG_COLETA_PRESSAO_CS_PIN;
    slot_config.host_id = host.slot;

    return ESP_OK;
}

static esp_err_t UART_config(void)
{
    return ESP_OK;
}
#include <stdio.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <memory.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <driver/i2c_master.h>
#include <driver/uart.h>

#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

#include <esp_check.h>

#include "ads111x.h"

#define MOUNT_POINT "/sdcard"

#define BUFFER_SIZE 128
#define RX_BUFFER_SIZE 1024

#define LED_SD GPIO_NUM_17

//============================================
//  VARS GLOBAIS
//============================================

int16_t adc0;
int16_t adc1;

struct sistema_t
{
    float p0;
    float p0Total; // Mais a coluna D'agua

    float p1;
    float p1Total; // Mais a coluna D'agua
} SistemaData;

uint64_t contador_tabela = 0;

char buffer_sd[BUFFER_SIZE];
char buffer_rx[RX_BUFFER_SIZE];

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
static void vTaskSD(void *pvArg);
TaskHandle_t handleTask_SD = NULL;
const char *TAG_SD = "[SD]";

/**
 *  @brief Task para Receber os dados via UART
 *
 *  @param pvArg Ponteiro dos argumentos, caso precise fazer alguma
 *  configuracao
 */
static void vTaskUARTRx(void *pvArg);
TaskHandle_t handleTask_UARTRx = NULL;
const char *TAG_UARTRX = "[UART RX]";

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
 *  @brief Funcao de Configuracao dos GPIOS
 */
static esp_err_t GPIO_config(void);

/*
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
    // UART_config();
    GPIO_config();

    // xTaskCreate(vTaskADS1115, "ADS115 TASK", configMINIMAL_STACK_SIZE + 1024 * 5,
    //             NULL, 1, &handleTask_ADS115);

    // xTaskCreate(vTaskProcessADS, "PROCESS ADS TASK", configMINIMAL_STACK_SIZE + 1024 * 10,
    //             NULL, 1, &handleTask_ProcessADS);

    xTaskCreate(vTaskSD, "PROCESS SD", configMINIMAL_STACK_SIZE + 1024 * 10,
                NULL, 1, &handleTask_SD);

    // xTaskCreate(vTaskUARTRx, "UART RX TASK", configMINIMAL_STACK_SIZE + 1024 * 2,
    //             NULL, 1, &handleTask_UARTRx);
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

    int cont = 0;

    while (1)
    {
        v_0 = (adc0 * 0.1875) / 1000;
        v_1 = (adc1 * 0.1875) / 1000;

        SistemaData.p0Total = (((v_0 / 5) - 0.04) / 0.018);
        SistemaData.p1Total = (((v_1 / 5) - 0.04) / 0.018);

        SistemaData.p0 = (((v_0 / 5) - 0.04) / 0.018) + c_0;
        SistemaData.p1 = (((v_1 / 5) - 0.04) / 0.018) + c_1;

        if (cont == 20)
        {
            if (round(SistemaData.p0 * 100) > 1 || round(SistemaData.p0 * 100) < -1)
                c_0 = -SistemaData.p0;
            if (round(SistemaData.p1 * 100) > 1 || round(SistemaData.p1 * 100) < -1)
                c_1 = -SistemaData.p1;
        }

        if (cont < 101)
            cont += 1;

        printf("%0.3f\t%0.2f\t%0.2f\t%0.2f\t%0.2f\n",
               contador_tabela * 0.032, SistemaData.p0, SistemaData.p0Total,
               SistemaData.p1, SistemaData.p1Total);
        fflush(stdout);

        contador_tabela++;

        // xSemaphoreGive(Semaphore_ADS_to_SD);

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

static void vTaskSD(void *pvArg)
{
    const char mount_point[] = MOUNT_POINT;

    if (esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_sd, &card) != ESP_OK)
    {
        while(esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_sd, &card) != ESP_OK)
        {
            ESP_LOGW(TAG_SD, "Insira ou verifique o SD");

            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    const char *file_name = MOUNT_POINT "/data.txt";

    FILE *arq = fopen(file_name, "a");

    while (arq == NULL)
    {
        FILE *arq = fopen(file_name, "a");

        if (arq != NULL)
            break;

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    snprintf(buffer_sd, BUFFER_SIZE,
             "Tempo(s)\tP0(KPa)\tP0+Coluna(KPa)\tP1(KPa)\tP1+Coluna(KPa)\n");
    fprintf(arq, buffer_sd);

    fclose(arq);

    while (1)
    {
        FILE *arq = fopen(file_name, "a");

        snprintf(buffer_sd, BUFFER_SIZE, "%0.3f\t%0.2f\t%0.2f\t%0.2f\t%0.2f\n",
                 contador_tabela++ * 0.032, SistemaData.p0, SistemaData.p0Total,
                 SistemaData.p1, SistemaData.p1Total);

        if (fprintf(arq, buffer_sd) <= 0)
            gpio_set_level(LED_SD, 0);

        gpio_set_level(LED_SD, 1);

        fclose(arq);

        vTaskDelay(pdMS_TO_TICKS(30));
    }

    fclose(arq);
}

static void vTaskUARTRx(void *pvArg)
{
    uint8_t length = 0;

    while (1)
    {
        // Leitura da porta serial 0 (USB do Dev-kit module v1)
        length = uart_read_bytes(UART_NUM_0, buffer_rx, RX_BUFFER_SIZE - 1, pdMS_TO_TICKS(10));

        if (length)
            buffer_rx[length] = '\n';
        //  Loop back para verificar o que chegou
        uart_write_bytes(UART_NUM_0, (const char *)buffer_rx, length);

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

static esp_err_t GPIO_config(void)
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

static esp_err_t UART_config(void)
{
    const uart_port_t uart_num = UART_NUM_0;
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = CONFIG_COLETA_PRESSAO_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(uart_num, RX_BUFFER_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, CONFIG_COLETA_PRESSAO_TX_PIN, CONFIG_COLETA_PRESSAO_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    return ESP_OK;
}
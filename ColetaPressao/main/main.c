#include <stdio.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <memory.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>

#include <driver/i2c_master.h>
#include <driver/uart.h>
#include <driver/gptimer.h>

#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

#include <esp_console.h>

#include <esp_check.h>

#include "ads111x.h"

#define PRINTS 1

#define MOUNT_POINT "/sdcard"

#define BUFFER_SIZE 128
#define RX_BUFFER_SIZE 1024

#define LED_SD GPIO_NUM_17

#define TIMER_RESOLUTION_HZ 1000000 / 2

#define CONSOLE_PROMPT_STR CONFIG_IDF_TARGET
#define CONSOLE_MAX_LEN_CMD 1024
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

struct timer_sample_t
{
    uint64_t valor_contador;
    float tempo_decorrido;
} TempoDeAmostragem;

char buffer_sd[BUFFER_SIZE];
char buffer_rx[RX_BUFFER_SIZE];

SemaphoreHandle_t Semaphore_ProcessADS_to_SD = NULL;
EventGroupHandle_t Eventhandle_cmdBits = NULL;
EventBits_t EventBits_cmdBits;
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

/**
 *  @brief Configuração do Timer para contabilizar
 *  o tempo decorrido de um dado para o outro, para
 *  depois ser gravado no SD.
 */
static esp_err_t Timer_config(void);
gptimer_handle_t handle_Timer = NULL;

//============================================
//  MAIN
//============================================
void app_main(void)
{
    Semaphore_ProcessADS_to_SD = xSemaphoreCreateBinary();
    Eventhandle_cmdBits = xEventGroupCreate();

    I2C_config();
    SD_config();
    GPIO_config();
    Timer_config();

    // esp_console_repl_t *repl = NULL;
    // esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    // /* Prompt to be printed before each line.
    //  * This can be customized, made dynamic, etc.
    //  */
    // repl_config.prompt = CONSOLE_PROMPT_STR ">";
    // repl_config.max_cmdline_length = CONSOLE_MAX_LEN_CMD;

    // esp_console_register_help_command();

    // esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    // ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

    // ESP_ERROR_CHECK(esp_console_start_repl(repl));

    xTaskCreate(vTaskADS1115, "ADS115 TASK", configMINIMAL_STACK_SIZE + 1024 * 5,
                NULL, 1, &handleTask_ADS115);

    xTaskCreate(vTaskProcessADS, "PROCESS ADS TASK", configMINIMAL_STACK_SIZE + 1024 * 10,
                NULL, 1, &handleTask_ProcessADS);

    // xTaskCreate(vTaskSD, "PROCESS SD", configMINIMAL_STACK_SIZE + 1024 * 10,
    //             NULL, 1, &handleTask_SD);

    while (1)
    {
        if (EventBits_cmdBits & 0x1)
            gpio_set_level(LED_SD, 1);
        else if (EventBits_cmdBits & 0x2)
            gpio_set_level(LED_SD, 0);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
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

        gptimer_get_raw_count(handle_Timer, &(TempoDeAmostragem.valor_contador));

        TempoDeAmostragem.tempo_decorrido += TempoDeAmostragem.valor_contador;

#if PRINTS
        printf("%0.3f\t%0.2f\t%0.2f\t%0.2f\t%0.2f\n",
              TempoDeAmostragem.tempo_decorrido / 500000, SistemaData.p0, SistemaData.p0Total,
               SistemaData.p1, SistemaData.p1Total);
        fflush(stdout);
#endif

        gptimer_set_raw_count(handle_Timer, 0);

        xSemaphoreGive(Semaphore_ProcessADS_to_SD);

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

static void vTaskSD(void *pvArg)
{
    const char mount_point[] = MOUNT_POINT;

    while (esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_sd, &card) != ESP_OK)
    {
        ESP_LOGW(TAG_SD, "Insira ou verifique o SD");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    const char *file_name = MOUNT_POINT "/data.txt";

    FILE *arq = fopen(file_name, "a");

    while (arq == NULL)
    {
        FILE *arq = fopen(file_name, "a");

        ESP_LOGW(TAG_SD, "Nao foi possivel abrir o arquivo: %s", file_name);

        if (arq != NULL)
            break;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    snprintf(buffer_sd, BUFFER_SIZE,
             "Tempo(s)\tP0(KPa)\tP0+Coluna(KPa)\tP1(KPa)\tP1+Coluna(KPa)\n");
    fprintf(arq, buffer_sd);

    fclose(arq);

    while (1)
    {
        if (xSemaphoreTake(Semaphore_ProcessADS_to_SD, 10) == pdTRUE)
        {
            FILE *arq = fopen(file_name, "a");

            snprintf(buffer_sd, BUFFER_SIZE, "%0.3f\t%0.2f\t%0.2f\t%0.2f\t%0.2f\n",
                     (TempoDeAmostragem.tempo_decorrido / 1000000), SistemaData.p0, SistemaData.p0Total,
                     SistemaData.p1, SistemaData.p1Total);

            if (fprintf(arq, buffer_sd) <= 0)
                gpio_set_level(LED_SD, 0);

            gpio_set_level(LED_SD, 1);

            fclose(arq);
        }

        vTaskDelay(pdMS_TO_TICKS(32));
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

static esp_err_t Timer_config(void)
{
    TempoDeAmostragem.tempo_decorrido = 0;
    TempoDeAmostragem.valor_contador = 0;

    gptimer_config_t config =
        {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            .resolution_hz = TIMER_RESOLUTION_HZ, // 1 kHz = 1 ms
        };

    ESP_ERROR_CHECK(gptimer_new_timer(&config, &handle_Timer));

    gptimer_set_raw_count(handle_Timer, 0);

    gptimer_enable(handle_Timer);
    gptimer_start(handle_Timer);

    return ESP_OK;
}
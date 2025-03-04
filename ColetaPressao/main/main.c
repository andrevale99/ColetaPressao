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
#include <driver/gptimer.h>

#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

#include <esp_console.h>

#include <esp_check.h>

#include "Configs.h"
#include "ads111x.h"

#define MOUNT_POINT "/sdcard"

#define BUFFER_SIZE 128
#define RX_BUFFER_SIZE 1024

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

struct SD_t
{
};

char buffer_sd[BUFFER_SIZE];
char buffer_rx[RX_BUFFER_SIZE];

/// @brief Semaphore entre o processo dos dados do ADS111X com
/// a gravcao do SD
SemaphoreHandle_t Semaphore_ProcessADS_to_SD = NULL;

/// @brief Handle do I2C 
i2c_master_bus_handle_t handle_I2Cmaster = NULL;

/// @brief Estruturas para iniciar e montar o protocolo
/// SPI e o SD para a gravacao, respectivamente
esp_vfs_fat_sdmmc_mount_config_t mount_sd;
sdmmc_card_t *card;
sdmmc_host_t host = SDSPI_HOST_DEFAULT();
sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();

/// @brief Handle para o Timer, usado para contabilizar
/// o tempo entre uma gravacao e outra
gptimer_handle_t handle_Timer = NULL;
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

//============================================
//  MAIN
//============================================
void app_main(void)
{
    Semaphore_ProcessADS_to_SD = xSemaphoreCreateBinary();

    TempoDeAmostragem.tempo_decorrido = 0;
    TempoDeAmostragem.valor_contador = 0;

    I2C_config(&handle_I2Cmaster);
    SD_config(&mount_sd, &host, &slot_config);
    GPIO_config();
    Timer_config(&handle_Timer);

    // xTaskCreate(vTaskADS1115, "ADS115 TASK", configMINIMAL_STACK_SIZE + 1024 * 5,
    //             NULL, 1, &handleTask_ADS115);

    // xTaskCreate(vTaskProcessADS, "PROCESS ADS TASK", configMINIMAL_STACK_SIZE + 1024 * 10,
    //             NULL, 1, &handleTask_ProcessADS);

    // xTaskCreate(vTaskSD, "PROCESS SD", configMINIMAL_STACK_SIZE + 1024 * 10,
    //             NULL, 1, &handleTask_SD);

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

            gptimer_get_raw_count(handle_Timer, &(TempoDeAmostragem.valor_contador));

            TempoDeAmostragem.tempo_decorrido += TempoDeAmostragem.valor_contador;

            snprintf(buffer_sd, BUFFER_SIZE, "%0.3f\t%0.2f\t%0.2f\t%0.2f\t%0.2f\n",
                     (TempoDeAmostragem.tempo_decorrido / 1000000), SistemaData.p0, SistemaData.p0Total,
                     SistemaData.p1, SistemaData.p1Total);

            gptimer_set_raw_count(handle_Timer, 0);

            if (fprintf(arq, buffer_sd) <= 0)
                gpio_set_level(LED_SD, 0);

            gpio_set_level(LED_SD, 1);

            fclose(arq);
        }

        vTaskDelay(pdMS_TO_TICKS(32));
    }

    fclose(arq);
}
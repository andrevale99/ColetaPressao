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

#include <sdmmc_cmd.h>

#include <esp_vfs_fat.h>
#include <esp_console.h>
#include <esp_check.h>
#include <argtable3/argtable3.h>

#include "Macros.h"
#include "ads111x.h"
#include "Configs.h"

#define TERMINAL_PROMPT "LabFlu"

#define SD_BUFFER_SIZE 1024

#define SD_MOUNT_POINT "/sdcard"
#define SD_MAX_LEN_FILE_NAME (1 << 12)

#define CONSOLE_MAX_LEN_CMD (1 << 10)

// Colocar bits de eventos para o ADS
#define EVENT_BIT_START_TASKS (1 << 0)
#define EVENT_BIT_SD_ON_WRITE (1 << 1)
#define EVENT_BIT_SD_FILE_CREATE (1 << 2)
#define EVENT_BIT_SD_OK (1 << 23)

#define PRINTS_SERIAL

//============================================
//  VARS GLOBAIS
//============================================

struct sistema_t
{
    int16_t adc0;
    float p0;
    float p0Total; // Mais a coluna D'agua
    float offset0;

    int16_t adc1;
    float p1;
    float p1Total; // Mais a coluna D'agua
    float offset1;

} SistemaData;

struct timer_sample_t
{
    uint64_t valor_contador;
    float tempo_decorrido;
} TempoDeAmostragem;

FILE *arq = NULL;
char buffer_sd[SD_BUFFER_SIZE];
char file_name[SD_MAX_LEN_FILE_NAME];
const char mount_point[] = SD_MOUNT_POINT;

/// @brief Handle do I2C.
i2c_master_bus_handle_t handleI2Cmaster = NULL;

/// @brief Estruturas para iniciar e montar o protocolo
/// SPI e o SD para a gravacao, respectivamente.
sdmmc_card_t *card;
esp_vfs_fat_sdmmc_mount_config_t mount_sd;
sdmmc_host_t host = SDSPI_HOST_DEFAULT();
sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();

/// @brief Handle para o Timer, usado para contabilizar
/// o tempo entre uma gravacao e outra.
gptimer_handle_t handleTimer = NULL;

SemaphoreHandle_t handleSemaphore_to_ADS = NULL;

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
TaskHandle_t handleTaskADS115 = NULL;
const char *TAG_ADS = "[ADS111]";

/**
 *  @brief Task para gravar os dados no SD
 *
 *  @param pvArg Ponteiro dos argumentos, caso precise fazer alguma
 *  configuracao
 */
static void vTaskSD(void *pvArg);
TaskHandle_t handleTaskSD = NULL;
const char *TAG_SD = "[SD]";

/**
 *  @brief Funcao atrelado ao SD
 *
 *  @param argc Quantidade de argumentos
 *  @param argv String dos valores
 */
int sd_terminal(int argc, char **argv);

/**
 * @brief Registra os comandos do SD
 * no terminal
 */
esp_err_t cmd_register_sd(void);

/**
 * @brief Calcula o offset para estabilizar a pressao
 * considerando a pressao diferencial, ou seja, ao ligar
 * o sistema o sistema ira considerar a pressao o qual o sensor
 * está captando como um novo ZERO
 *
 * @param sistema Ponteiro para a estrutura de dados do sistema
 * @param ads Estrutura do sensor
 */
void set_offset_pressure(struct sistema_t *sistema, ads111x_struct_t *ads);

/**
 * @brief Calcula o valor da pressao
 *
 * @param sistema Ponteiro para a estrutura de dados do sistema
 */
void process_pressures(struct sistema_t *sistema);

static bool IRAM_ATTR example_timer_on_alarm_cb_v2(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;
    xSemaphoreGiveFromISR(handleSemaphore_to_ADS, &high_task_awoken);
    return (high_task_awoken == pdTRUE);
}
//============================================
//  MAIN
//============================================
void app_main(void)
{
    handleSemaphore_to_ADS = xSemaphoreCreateBinary();

    I2C_config(&handleI2Cmaster);
    SD_config(&mount_sd, &host, &slot_config);
    GPIO_config();
    Timer_config(&handleTimer);

    gptimer_event_callbacks_t cbs = {
        .on_alarm = example_timer_on_alarm_cb_v2,
    };

    ESP_ERROR_CHECK(gptimer_register_event_callbacks(handleTimer, &cbs, handleSemaphore_to_ADS));

    gptimer_alarm_config_t alarm_config2 = {
        .reload_count = 0,
        .alarm_count = 1000000, // period = 1s
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(handleTimer, &alarm_config2));

    gptimer_enable(handleTimer);

    gptimer_start(handleTimer);

    xTaskCreate(vTaskADS1115, "ADS115 TASK", configMINIMAL_STACK_SIZE + 1024 * 5,
                NULL, 2, &handleTaskADS115);

    // xTaskCreate(vTaskSD, "PROCESS SD", configMINIMAL_STACK_SIZE + 1024 * 10,
    //             NULL, 1, &handleTaskSD);
}

//============================================
//  FUNCS
//============================================

static void vTaskADS1115(void *pvArg)
{
    ads111x_struct_t ads;

    

    while (1)
    {
        if (xSemaphoreTake(handleSemaphore_to_ADS, pdMS_TO_TICKS(1)))
        {

        }
        vTaskDelay(pdMS_TO_TICKS(31));
    }
}

static void vTaskSD(void *pvArg)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(32));
    }
}

void set_offset_pressure(struct sistema_t *sistema, ads111x_struct_t *ads)
{
    float v_0 = 0.0;
    float v_1 = 0.0;

    sistema->offset0 = 0.0;
    sistema->offset1 = 0.0;

    uint8_t cont = 0;
    float soma_p0 = 0;
    float soma_p1 = 0;

    for (cont = 0; cont < 100; ++cont)
    {
        ads111x_set_input_mux(ADS111X_MUX_0_GND, ads);
        ads111x_get_conversion_sigle_ended(ads);
        sistema->adc0 = ads->conversion;

        ads111x_set_input_mux(ADS111X_MUX_1_GND, ads);
        ads111x_get_conversion_sigle_ended(ads);
        sistema->adc1 = ads->conversion;

        v_0 = (sistema->adc0 * 0.1875) / 1000;
        v_1 = (sistema->adc1 * 0.1875) / 1000;

        sistema->p0Total = (((v_0 / 5) - 0.04) / 0.018);
        sistema->p1Total = (((v_1 / 5) - 0.04) / 0.018);

        soma_p0 += sistema->p0Total;
        soma_p1 += sistema->p1Total;
    }

    sistema->offset0 = -(soma_p0 / 100);
    sistema->offset1 = -(soma_p1 / 100);
}

void process_pressures(struct sistema_t *sistema)
{
    float v_0 = (sistema->adc0 * 0.1875) / 1000;
    float v_1 = (sistema->adc1 * 0.1875) / 1000;

    sistema->p0Total = (((v_0 / 5) - 0.04) / 0.018);
    sistema->p1Total = (((v_1 / 5) - 0.04) / 0.018);

    sistema->p0 = (((v_0 / 5) - 0.04) / 0.018) + sistema->offset0;
    sistema->p1 = (((v_1 / 5) - 0.04) / 0.018) + sistema->offset1;
}

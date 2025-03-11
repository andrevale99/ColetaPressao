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

#include "Macros.h"
#include "ads111x.h"
#include "Configs.h"
#include "SD_terminal.h"
#include "Motor_terminal.h"

#define TERMINAL_PROMPT "LabFlu"

#define CONSOLE_MAX_LEN_CMD 1024

#define PRINTS_SERIAL 1

//============================================
//  VARS GLOBAIS
//============================================

struct dirent *dirs = NULL;

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

char buffer_sd[SD_BUFFER_SIZE];

/// @brief Semaphore entre o processo dos dados do ADS111X com
/// a gravcao do SD.
SemaphoreHandle_t Semaphore_ProcessADS_to_SD = NULL;

/// @brief Bits de eventos para sinalizar ao sistema
/// o comando que foi digitado no terminal.
EventBits_t EventBits_cmd;
EventGroupHandle_t handleEventBits_cmd = NULL;

/// @brief Handle do I2C.
i2c_master_bus_handle_t handleI2Cmaster = NULL;

/// @brief Estruturas para iniciar e montar o protocolo
/// SPI e o SD para a gravacao, respectivamente.
esp_vfs_fat_sdmmc_mount_config_t mount_sd;
sdmmc_card_t *card;
sdmmc_host_t host = SDSPI_HOST_DEFAULT();
sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();

/// @brief Handle para o Timer, usado para contabilizar
/// o tempo entre uma gravacao e outra.
gptimer_handle_t handleTimer = NULL;

/// @brief Estrutura para criar o terminal.
esp_console_repl_t *repl = NULL;

//============================================
//  PROTOTIPOS e VARS_RELACIONADAS
//============================================

/**
 *  @brief Funcao que ira criar um arquivo com o
 *  sufixo diferente, para nao sobrescrever os arquivos
 *  e difenrenciar qual foi o utlimo arquivo criado.
 *
 *  @param _arq Ponteiro para a variavel do tipo FILE
 *  o qual ira manipular o arquivo.
 *
 *  @param file_name Ponteiro para uma cadeia de caracteres
 *  que ira armazenar o nome do arquivo.
 */
void check_file_exist(FILE *_arq, char *file_name);

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

//============================================
//  MAIN
//============================================
void app_main(void)
{
    Semaphore_ProcessADS_to_SD = xSemaphoreCreateBinary();
    handleEventBits_cmd = xEventGroupCreate();

    TempoDeAmostragem.tempo_decorrido = 0;
    TempoDeAmostragem.valor_contador = 0;

    I2C_config(&handleI2Cmaster);

#if !PRINTS_SERIAL
    SD_config(&mount_sd, &host, &slot_config);
#endif

    GPIO_config();
    Timer_config(&handleTimer);

#if !PRINTS_SERIAL
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    repl_config.prompt = TERMINAL_PROMPT ">";
    repl_config.max_cmdline_length = CONSOLE_MAX_LEN_CMD;

    esp_console_register_help_command();

    vTaskDelay(pdMS_TO_TICKS(10));

    esp_console_new_repl_uart(&hw_config, &repl_config, &repl);
    esp_console_start_repl(repl);

    cmd_register_motor(&EventBits_cmd, &handleEventBits_cmd);
    cmd_register_sd();
#endif

    xTaskCreate(vTaskADS1115, "ADS115 TASK", configMINIMAL_STACK_SIZE + 1024 * 5,
                NULL, 1, &handleTaskADS115);

#if !PRINTS_SERIAL
    xTaskCreate(vTaskSD, "PROCESS SD", configMINIMAL_STACK_SIZE + 1024 * 10,
                NULL, 1, &handleTaskSD);
#endif

    // while (1)
    // {
    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }
}

//============================================
//  FUNCS
//============================================

void check_file_exist(FILE *_arq, char *file_name)
{
    for (uint8_t sufixo = 0; sufixo < UINT8_MAX; ++sufixo)
    {
        snprintf(file_name, SD_MAX_LEN_FILE_NAME,
                 SD_MOUNT_POINT "/" CONFIG_COLETA_PRESSAO_SD_PREFIX_FILE_NAME "_%d.txt", sufixo);

        _arq = fopen(file_name, "r");
        if (_arq == NULL)
        {
            snprintf(file_name, SD_MAX_LEN_FILE_NAME,
                     SD_MOUNT_POINT "/" CONFIG_COLETA_PRESSAO_SD_PREFIX_FILE_NAME "_%d.txt", sufixo);

            return;
        }
        fclose(_arq);
    }

    snprintf(file_name, SD_MAX_LEN_FILE_NAME,
             SD_MOUNT_POINT "/" CONFIG_COLETA_PRESSAO_SD_PREFIX_FILE_NAME "_%d.txt", 0);
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

#if PRINTS_SERIAL
    printf("%0.3f\t%0.2f\t%0.2f\t%0.2f\t%0.2f\n",
           TempoDeAmostragem.tempo_decorrido / TIMER_RESOLUTION_HZ, sistema->p0, sistema->p0Total,
           sistema->p1, sistema->p1Total);
    fflush(stdout);
#endif
}

static void vTaskADS1115(void *pvArg)
{
    ads111x_struct_t ads;

    ESP_LOGI(TAG_ADS, "BEGIN: %s", esp_err_to_name(ads111x_begin(&handleI2Cmaster, ADS111X_ADDR, &ads)));
    ads111x_set_gain(ADS111X_GAIN_4V096, &ads);
    ads111x_set_mode(ADS111X_MODE_SINGLE_SHOT, &ads);
    ads111x_set_data_rate(ADS111X_DATA_RATE_32, &ads);

    set_offset_pressure(&SistemaData, &ads);

    while (1)
    {
        ads111x_set_input_mux(ADS111X_MUX_0_GND, &ads);
        ads111x_get_conversion_sigle_ended(&ads);
        SistemaData.adc0 = ads.conversion;

        ads111x_set_input_mux(ADS111X_MUX_1_GND, &ads);
        ads111x_get_conversion_sigle_ended(&ads);
        SistemaData.adc1 = ads.conversion;

        gptimer_get_raw_count(handleTimer, &(TempoDeAmostragem.valor_contador));
        TempoDeAmostragem.tempo_decorrido += TempoDeAmostragem.valor_contador;

        process_pressures(&SistemaData);

        gptimer_set_raw_count(handleTimer, 0);

        xSemaphoreGive(Semaphore_ProcessADS_to_SD);

        vTaskDelay(pdMS_TO_TICKS(32));
    }
}

static void vTaskSD(void *pvArg)
{
    FILE *arq = NULL;
    const char mount_point[] = SD_MOUNT_POINT;
    char file_name[SD_MAX_LEN_FILE_NAME];

    while (esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_sd, &card) != ESP_OK)
    {
        sd_set_bitmask(false, SD_MASK_DETECTED);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    sd_set_bitmask(true, SD_MASK_DETECTED);

    check_file_exist(arq, &file_name[0]);

    do
    {
        arq = fopen(file_name, "a");

        if (arq != NULL)
        {
            sd_set_bitmask(true, SD_MASK_FILE_CREATED);
            sd_file_name(&file_name[0]);
            break;
        }

        sd_set_bitmask(false, SD_MASK_FILE_CREATED);

        vTaskDelay(pdMS_TO_TICKS(1000));
    } while (arq == NULL);

    snprintf(buffer_sd, SD_BUFFER_SIZE,
             "Tempo(s)\tP0(KPa)\tP0+Coluna(KPa)\tP1(KPa)\tP1+Coluna(KPa)\n");
    fprintf(arq, buffer_sd);

    fclose(arq);

    while (1)
    {
        if (xSemaphoreTake(Semaphore_ProcessADS_to_SD, 10) == pdTRUE)
        {
            arq = fopen(file_name, "a");

            snprintf(buffer_sd, SD_BUFFER_SIZE, "%0.3f\t%0.2f\t%0.2f\t%0.2f\t%0.2f\n",
                     (TempoDeAmostragem.tempo_decorrido / TIMER_RESOLUTION_HZ), SistemaData.p0, SistemaData.p0Total,
                     SistemaData.p1, SistemaData.p1Total);

            if (fprintf(arq, buffer_sd) <= 0)
                sd_set_bitmask(false, SD_MASK_ON_WRITE);

            sd_set_bitmask(true, SD_MASK_ON_WRITE);

            fclose(arq);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    fclose(arq);
}
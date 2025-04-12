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

static struct
{
    struct arg_str *status;
    struct arg_str *check;
    struct arg_str *rename;
    struct arg_str *start_stop;
    struct arg_end *end;
} sd_args;

FILE *arq = NULL;
char buffer_sd[SD_BUFFER_SIZE];
char file_name[SD_MAX_LEN_FILE_NAME];
const char mount_point[] = SD_MOUNT_POINT;

/// @brief Bits de eventos para sinalizar ao sistema
/// o comando que foi digitado no terminal.
EventBits_t EventBits_cmd;
EventGroupHandle_t handleEventBits_cmd = NULL;

/// @brief Semaphore das Tasks ADS111X e SD
SemaphoreHandle_t handleSemaphore_ADS_to_SD = NULL;

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

/// @brief Estrutura para criar o terminal.
esp_console_repl_t *repl = NULL;

//============================================
//  PROTOTIPOS e VARS_RELACIONADAS
//============================================

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

void setEventBit(uint32_t bit)
{
    EventBits_cmd = xEventGroupSetBits(handleEventBits_cmd, bit);
}

void clearEventBit(uint32_t bit)
{
    EventBits_cmd = xEventGroupClearBits(handleEventBits_cmd, bit);
}

//============================================
//  MAIN
//============================================
void app_main(void)
{
    handleSemaphore_ADS_to_SD = xSemaphoreCreateBinary();

    handleEventBits_cmd = xEventGroupCreate();
    EventBits_cmd = xEventGroupClearBits(handleEventBits_cmd, 0xFFFF);

    TempoDeAmostragem.tempo_decorrido = 0;
    TempoDeAmostragem.valor_contador = 0;

    I2C_config(&handleI2Cmaster);
    SD_config(&mount_sd, &host, &slot_config);
    GPIO_config();
    Timer_config(&handleTimer);

    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    repl_config.prompt = TERMINAL_PROMPT ">";
    repl_config.max_cmdline_length = CONSOLE_MAX_LEN_CMD;

    esp_console_register_help_command();

    vTaskDelay(pdMS_TO_TICKS(10));

    esp_console_new_repl_uart(&hw_config, &repl_config, &repl);
    esp_console_start_repl(repl);

    cmd_register_sd();

    // if (esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_sd, &card) != ESP_OK)
    //     clearEventBit(EVENT_BIT_SD_OK);
    // else
    //     setEventBit(EVENT_BIT_SD_OK);

    xTaskCreate(vTaskADS1115, "ADS115 TASK", configMINIMAL_STACK_SIZE + 1024 * 5,
                NULL, 2, &handleTaskADS115);

    // xTaskCreate(vTaskSD, "PROCESS SD", configMINIMAL_STACK_SIZE + 1024 * 10,
    //             NULL, 1, &handleTaskSD);
}

//============================================
//  FUNCS
//============================================

int sd_terminal(int argc, char **argv)
{
    static uint8_t cnt_argc = 0;
    for (cnt_argc = 1; cnt_argc < argc; ++cnt_argc)
    {
        if (strcmp(argv[cnt_argc], "status") == 0)
        {
            if (EventBits_cmd & EVENT_BIT_SD_OK)
                printf("%s SD Card ENCONTRADO %s \n", BHGRN, COLOR_RESET);
            else
                printf("%s SD Card NAO encontrado %s \n", BHRED, COLOR_RESET);

            if (EventBits_cmd & EVENT_BIT_SD_FILE_CREATE)
                printf("%s Arquivo %s Criado %s\n", BHGRN, file_name, COLOR_RESET);
            else
                printf("%s Erro ao criar arquivo %s %s\n", BHRED, file_name, COLOR_RESET);

            if (EventBits_cmd & EVENT_BIT_SD_ON_WRITE)
                printf("%s Gravando %s \n", BHGRN, COLOR_RESET);
            else
                printf("%s SEM Gravacao %s \n", BHRED, COLOR_RESET);
        } // fim do status

        if ((strcmp(argv[cnt_argc], "rename") == 0))
        {
            if (arq != NULL)
                fclose(arq);

            cnt_argc += 1;
            printf("Rename: %s to ", file_name);

            snprintf(file_name, SD_MAX_LEN_FILE_NAME,
                     SD_MOUNT_POINT "/%s.txt", argv[cnt_argc]);

            printf("%s \n", file_name);

            arq = fopen(file_name, "a");

            if (arq == NULL)
                clearEventBit(EVENT_BIT_SD_FILE_CREATE);
            else
            {
                setEventBit(EVENT_BIT_SD_FILE_CREATE);
                snprintf(buffer_sd, SD_BUFFER_SIZE,
                         "Tempo(s)\tP0(KPa)\tP0+Coluna(KPa)\tP1(KPa)\tP1+Coluna(KPa)\n");
                fprintf(arq, buffer_sd);
                fclose(arq);
            }
        } // fim do rename

        if ((strcmp(argv[cnt_argc], "check") == 0))
        {
            if (esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_sd, &card) != ESP_OK)
                clearEventBit(EVENT_BIT_SD_OK);
            else
                setEventBit(EVENT_BIT_SD_OK);
        } // fim do check

        if ((strcmp(argv[cnt_argc], "s") == 0))
        {
            TempoDeAmostragem.tempo_decorrido = 0;
            TempoDeAmostragem.valor_contador = 0;
            gptimer_set_raw_count(handleTimer, 0);

            setEventBit(EVENT_BIT_START_TASKS);
        }
        else if ((strcmp(argv[cnt_argc], "t") == 0))
            clearEventBit(EVENT_BIT_START_TASKS | EVENT_BIT_SD_ON_WRITE);

    } // fim do for

    printf("\n");
    fflush(stdout);

    return 0;
}

esp_err_t cmd_register_sd(void)
{
    sd_args.status = arg_str0(NULL, NULL, "status", "Verifica a situacao do SD");
    sd_args.check = arg_str0(NULL, NULL, "check", "Tenta Verificar se ha SD");
    sd_args.rename = arg_str0(NULL, NULL, "rename", "Renomeia o arquivo");
    sd_args.start_stop = arg_str0(NULL, NULL, "s/t", "Inicia/Pausa a gravacao");
    sd_args.end = arg_end(1);

    const esp_console_cmd_t sd_cmd = {
        .command = "sd",
        .help = "Comandos para verificacao do SD e arquivos",
        .hint = NULL,
        .func = sd_terminal,
        .argtable = &sd_args,
    };

    return (esp_console_cmd_register(&sd_cmd));
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

static void vTaskADS1115(void *pvArg)
{
    ads111x_struct_t ads;

    do
    {
        if (ads111x_begin(&handleI2Cmaster, ADS111X_ADDR, &ads) == ESP_OK)
            break;

        ESP_LOGE(TAG_ADS, "Erro ao chamar o ADS111X");
        clearEventBit(EVENT_BIT_START_TASKS);

        vTaskDelay(pdMS_TO_TICKS(1000));
    } while (1);

    ads111x_set_gain(ADS111X_GAIN_4V096, &ads);
    ads111x_set_mode(ADS111X_MODE_SINGLE_SHOT, &ads);
    ads111x_set_data_rate(ADS111X_DATA_RATE_250, &ads);

    set_offset_pressure(&SistemaData, &ads);

    gptimer_start(handleTimer);

    while (1)
    {
        if (EventBits_cmd & EVENT_BIT_START_TASKS)
        {
            ads111x_set_input_mux(ADS111X_MUX_0_GND, &ads);
            ads111x_get_conversion_sigle_ended(&ads);
            SistemaData.adc0 = ads.conversion;

            gptimer_get_raw_count(handleTimer, &(TempoDeAmostragem.valor_contador));

            TempoDeAmostragem.tempo_decorrido += TempoDeAmostragem.valor_contador;

            process_pressures(&SistemaData);

            gptimer_set_raw_count(handleTimer, 0);

#ifdef PRINTS_SERIAL
            snprintf(buffer_sd, SD_BUFFER_SIZE, "%0.3f\t%0.2f\t%0.2f\t%0.2f\t%0.2f\n",
                     (TempoDeAmostragem.tempo_decorrido / TIMER_RESOLUTION_HZ),
                     SistemaData.p0, SistemaData.p0Total,
                     SistemaData.p1, SistemaData.p1Total);
            printf("%s", buffer_sd);
            fflush(stdout);
#endif
            xSemaphoreGive(handleSemaphore_ADS_to_SD);
        }
        
        vTaskDelay(32 / portTICK_PERIOD_MS);
    }
}

static void vTaskSD(void *pvArg)
{
    snprintf(file_name, SD_MAX_LEN_FILE_NAME, SD_MOUNT_POINT "/nan.txt");
    do
    {
        arq = fopen(file_name, "w");

        if (arq != NULL)
        {
            setEventBit(EVENT_BIT_SD_FILE_CREATE);
            fclose(arq);
            break;
        }

        clearEventBit(EVENT_BIT_START_TASKS);
        clearEventBit(EVENT_BIT_SD_FILE_CREATE);

        vTaskDelay(pdMS_TO_TICKS(100));
    } while (1);

    while (1)
    {
        if (xSemaphoreTake(handleSemaphore_ADS_to_SD, (TickType_t)0))
        {
            arq = fopen(file_name, "a");

            snprintf(buffer_sd, SD_BUFFER_SIZE, "%0.3f\t%0.2f\t%0.2f\t%0.2f\t%0.2f\n",
                     (TempoDeAmostragem.tempo_decorrido / TIMER_RESOLUTION_HZ),
                     SistemaData.p0, SistemaData.p0Total,
                     SistemaData.p1, SistemaData.p1Total);

            if (fprintf(arq, buffer_sd) > 0)
                setEventBit(EVENT_BIT_SD_ON_WRITE);
            else
                clearEventBit(EVENT_BIT_SD_ON_WRITE);

#ifdef PRINTS_SERIAL
            printf("%s", buffer_sd);
            fflush(stdout);
#endif
            fclose(arq);
        } // Fim da gravacao pelo semaphore

        vTaskDelay(32 / portTICK_PERIOD_MS);
    }
    fclose(arq);
}
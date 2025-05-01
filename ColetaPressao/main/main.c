#include <stdio.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <memory.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <driver/i2c_master.h>
#include <driver/gptimer.h>

#include <sdmmc_cmd.h>

#include <esp_vfs_fat.h>
#include <esp_check.h>

#include "Macros.h"
#include "ads111x.h"
#include "Configs.h"

#define SD_BUFFER_SIZE 1024
#define SD_MOUNT_POINT "/sdcard"
#define SD_MAX_LEN_FILE_NAME (1 << 12)

#define ALARM_TIMER_TO_ADS 32000 // 32 ms

// Como a aquisicao do sensor de vazao é de acordo com o
// timer que esta configurador para 32 ms, deve multiplicar
// a quantidade de pulsos por: 1000ms / 32ms = 31.25
#define CONVERSAO_DADOS_POR_SEGUNDOS 1000 / 1000

#define VAZAO_L_POR_S(_pulsos) (((float)_pulsos / 2.0 * CONVERSAO_DADOS_POR_SEGUNDOS) / 109.0)
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

int VazaoPulseCounter = 0;
short EstouroVazao = 0;
float fVazao = 0.0;

FILE *arq = NULL;
char buffer_sd[SD_BUFFER_SIZE];
char file_name[SD_MAX_LEN_FILE_NAME];
const char mount_point[] = SD_MOUNT_POINT;

/// @brief Handle do I2C.
i2c_master_bus_handle_t handleI2Cmaster = NULL;

/// @brief Estruturas para iniciar e montar o protocolo
/// SPI e o SD para a gravacao, respectivamente.
sdmmc_card_t *card;
esp_vfs_fat_sdmmc_mount_config_t mount_config_sd;
sdmmc_host_t host = SDSPI_HOST_DEFAULT();
sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();

/// @brief Handle para o Timer, usado para contabilizar
/// o tempo entre uma gravacao e outra.
gptimer_handle_t handleTimer = NULL;

pcnt_unit_handle_t handlePulseCounter = NULL;

SemaphoreHandle_t handleSemaphore_Alarm_to_ADS = NULL;
SemaphoreHandle_t handleSemaphore_ADS_to_SD = NULL;
QueueHandle_t handleQueue_PulseCounter = NULL;
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
 *  @brief Task para o SD
 *
 *  @param pvArg Ponteiro dos argumentos, caso precise fazer alguma
 *  configuracao
 */
void vTaskSD(void *pvArg);
TaskHandle_t handleTaskSD = NULL;
const char *TAG_SD = "[SD]";

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

/// @brief Testa se ha SD no sistema
esp_err_t check_sd(void);

//============================================
//  INTERRUPCOES
//============================================

/**
 * @brief Interrucao do tipo ALARM para o timer. Usado para
 * liberar o semaphore para a task relacionado ao ADS para fazer
 * o processamento dos dados.
 *
 * @param timer Handle Timer
 * @param edata estrutura de dados para fazer o alarm
 * @param  user_data dado que sera passado para a interrupcao
 * para ser tratado.
 */
static bool IRAM_ATTR timer_alarm_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_awoken = pdTRUE;
    xSemaphoreGiveFromISR(handleSemaphore_Alarm_to_ADS, &high_task_awoken);

    EstouroVazao++;
    if (EstouroVazao > 31)
    {
        EstouroVazao = 0;
        pcnt_unit_get_count(handlePulseCounter, &VazaoPulseCounter);
        xQueueSendFromISR(handleQueue_PulseCounter, &VazaoPulseCounter, pdFALSE);
        pcnt_unit_clear_count(handlePulseCounter);
    }

    return high_task_awoken;
}

//============================================
//  MAIN
//============================================
void app_main(void)
{
    handleSemaphore_Alarm_to_ADS = xSemaphoreCreateBinary();
    handleSemaphore_ADS_to_SD = xSemaphoreCreateBinary();
    handleQueue_PulseCounter = xQueueCreate(1, sizeof(int) * 2);

    I2C_config(&handleI2Cmaster);
    SD_config(&mount_config_sd, &host, &slot_config);
    GPIO_config();
    Timer_config(&handleTimer);
    PWM_config();
    PULSE_COUNTER_config(&handlePulseCounter);

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_alarm_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(handleTimer, &cbs, handleSemaphore_Alarm_to_ADS));

    gptimer_alarm_config_t alarm_config2 = {
        .reload_count = 0,
        .alarm_count = ALARM_TIMER_TO_ADS,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(handleTimer, &alarm_config2));

    gptimer_enable(handleTimer);
    gptimer_start(handleTimer);

    xTaskCreate(vTaskADS1115, "ADS115 TASK", configMINIMAL_STACK_SIZE + 1024 * 10,
                NULL, 5, &handleTaskADS115);

    // xTaskCreate(vTaskSD, "SD TASK", configMINIMAL_STACK_SIZE + 1024 * 5,
    //             NULL, 1, &handleTaskSD);

    // int16_t cnt = 10;
    // while (1)
    // {

    //     ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, cnt));
    //     ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0));

    //     cnt += 1;

    //     if (cnt > 1023)
    //         cnt = 0;

    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }
}

//============================================
//  TASKS
//============================================

static void vTaskADS1115(void *pvArg)
{
#define TAM 10

    ads111x_struct_t ads;
    uint64_t cnt_timer = 0;
    int iVazao = 0;

    while (i2c_master_probe(handleI2Cmaster, ADS111X_ADDR, 100))
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGW(TAG_ADS, "ADS nao encontrado");
    }

    if (ads111x_begin(&handleI2Cmaster, ADS111X_ADDR, &ads) != ESP_OK)
        ESP_LOGE(TAG_ADS, "Erro ao iniciar o ADS111X");
    else
        ESP_LOGI(TAG_ADS, "ADS111X encontrado");

    ads111x_set_data_rate(ADS111X_DATA_RATE_32, &ads);
    ads111x_set_gain(ADS111X_GAIN_4V096, &ads);
    ads111x_set_mode(ADS111X_MODE_SINGLE_SHOT, &ads);

    set_offset_pressure(&SistemaData, &ads);
    ESP_LOGI(TAG_ADS, "Offset finalizado");

    while (1)
    {
        if (xSemaphoreTake(handleSemaphore_Alarm_to_ADS, pdMS_TO_TICKS(0)) == pdTRUE)
        {

            ads111x_set_input_mux(ADS111X_MUX_0_GND, &ads);
            ads111x_get_conversion_sigle_ended(&ads);
            SistemaData.adc0 = ads.conversion;

            ads111x_set_input_mux(ADS111X_MUX_1_GND, &ads);
            ads111x_get_conversion_sigle_ended(&ads);
            SistemaData.adc1 = ads.conversion;

            process_pressures(&SistemaData);

            // Dividir a quantidade de pulsos por 2 e dividir por 109
            // para calcular a vazal em l/min
            // (Pulsos / 2 * 31.25) / 109 [L/min]
            xQueueReceiveFromISR(handleQueue_PulseCounter, (void *)&iVazao, pdMS_TO_TICKS(0));

            fVazao = VAZAO_L_POR_S(iVazao);

            printf("%0.3f\t%0.2f\t%0.2f\t%0.2f\t%0.2f\t%0.3f\t%i\n",
                   (0.032 * cnt_timer),
                   SistemaData.p0, SistemaData.p0Total,
                   SistemaData.p1, SistemaData.p1Total,
                   fVazao, VazaoPulseCounter);

            cnt_timer += 1;

            xSemaphoreGive(handleSemaphore_ADS_to_SD);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void vTaskSD(void *pvArg)
{
    if (check_sd() != ESP_OK)
        vTaskSuspend(handleTaskSD);

    snprintf(file_name, SD_MAX_LEN_FILE_NAME,
             SD_MOUNT_POINT "/" CONFIG_COLETA_PRESSAO_SD_PREFIX_FILE_NAME ".txt");

    // NAO ESQUECER DE RETIRAR ESTA LINHA
    // NOS CODIGOS FUTUROS (para testes)
    remove(file_name);

    int iBytesEscritos = 0;

    while (1)
    {
        if (xSemaphoreTake(handleSemaphore_ADS_to_SD, pdMS_TO_TICKS(0)) == pdTRUE)
        {
            arq = fopen(file_name, "a");
            if (arq != NULL)
            {
                snprintf(buffer_sd, SD_BUFFER_SIZE,
                         "%0.2f\t%0.2f\t%0.2f\t%0.2f\t%0.2f\n",
                         SistemaData.p0, SistemaData.p0Total,
                         SistemaData.p1, SistemaData.p1Total,
                         fVazao);

                iBytesEscritos = fprintf(arq, buffer_sd);

                if (iBytesEscritos < 0)
                    ESP_LOGW(TAG_SD, "Erro ao gravar no SD");
                else
                    ESP_LOGI(TAG_SD, "Bytes escritos: %i", iBytesEscritos);

                fclose(arq);
            }
            else
                ESP_LOGW(TAG_SD, "Erro ao ABRIR o arquivo %s", file_name);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

//============================================
//  FUNCS
//============================================
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

esp_err_t check_sd(void)
{
    esp_err_t errCheckSD = ESP_FAIL;

    errCheckSD = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config_sd, &card);

    if (errCheckSD != ESP_OK)
    {
        ESP_LOGW("[CHECK_SD]", "Erro ao montar o sistema");
        return errCheckSD;
    }

    sdmmc_card_print_info(stdout, card);

    return errCheckSD;
}
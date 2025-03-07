#ifndef CMD_H
#define CMD_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <esp_console.h>

#include <argtable3/argtable3.h>

#define CMD_MOTOR_BIT (1 << 0)
#define CMD_SD_CHECK (1 << 1)
#define CMD_SD_STATUS (1 << 2)

#define FUNCTION_POINTER_DEFAULT int (*func)(int argc, char **argv)

extern EventBits_t EventBits_cmd;
extern EventGroupHandle_t handleEventBits_cmd;

//===============================================
//  COMANDOS DO MOTOR
//===============================================

static struct
{
    struct arg_str *start_stop;
    struct arg_int *potencia;
    struct arg_end *end;
} motor_args;

/**
 *  @brief Funcao de controle atrelado ao Terminal
 *  para ligar e desligar o motor
 *
 *  @param argc Quantidade de argumentos
 *  @param argv String dos valores
 */
int motor_cmd(int argc, char **argv)
{
    if (strcmp(argv[1], "S") == 0)
    {
        // colocar logica para pegar o valor da potencia

        EventBits_cmd = xEventGroupSetBits(
            handleEventBits_cmd, // The event group being updated.
            CMD_MOTOR_BIT);      // The bits being set.
    }
    else if (strcmp(argv[1], "T") == 0)
    {
        EventBits_cmd = xEventGroupClearBits(
            handleEventBits_cmd, // The event group being updated.
            CMD_MOTOR_BIT);      // The bits being cleared.
    }

    return 0;
}

esp_err_t cmd_register_motor(FUNCTION_POINTER_DEFAULT)
{
    motor_args.start_stop = arg_str0(NULL, NULL, "S/T", "Ligar (S) ou desligar (T)");
    motor_args.potencia = arg_int0(NULL, NULL, "VALOR", "Potencia do motor quando for ligar");
    motor_args.end = arg_end(2);

    const esp_console_cmd_t motor_cmd = {
        .command = "motor",
        .help = "Ligar ou Desliga o motor com uma certa Potencia",
        .hint = NULL,
        .func = func,
        .argtable = &motor_args,
    };

    return (esp_console_cmd_register(&motor_cmd));
}

//===============================================
//  COMANDOS DO SD
//===============================================

static struct
{
    struct arg_str *status;
    struct arg_str *check;
    struct arg_end *end;
} sd_args;

/**
 *  @brief Funcao atrelado ao SD
 *
 *  @param argc Quantidade de argumentos
 *  @param argv String dos valores
 */
int sd_cmd(int argc, char **argv)
{
    if (strcmp(argv[1], "check") == 0)
    {
        EventBits_cmd = xEventGroupSetBits(
            handleEventBits_cmd, // The event group being updated.
            CMD_SD_CHECK);       // The bits being set.
    }
    else if (strcmp(argv[1], "status") == 0)
    {
        EventBits_cmd = xEventGroupSetBits(
            handleEventBits_cmd, // The event group being updated.
            CMD_SD_STATUS);      // The bits being set.
    }

    return 0;
}

esp_err_t cmd_register_sd(FUNCTION_POINTER_DEFAULT)
{
    sd_args.status = arg_str0(NULL, NULL, "status", "Verifica a situacao do SD");
    sd_args.check = arg_str0(NULL, NULL, "check", "Verifica o conteudo do SD");
    sd_args.end = arg_end(2);

    const esp_console_cmd_t sd_cmd = {
        .command = "sd",
        .help = "Comandos para verificacao do SD e arquivos",
        .hint = NULL,
        .func = func,
        .argtable = &sd_args,
    };

    return (esp_console_cmd_register(&sd_cmd));
}

#endif
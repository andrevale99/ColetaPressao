#ifndef CMD_H
#define CMD_H

#include <esp_console.h>

#include <argtable3/argtable3.h>

#define CMD_MOTOR_BIT (1 << 0)
#define CMD_SD_CHECK (1 << 1)
#define CMD_SD_STATUS (1 << 2)

#define FUNCTION_POINTER_DEFAULT int (*func)(int argc, char **argv)

static struct
{
    struct arg_str *start_stop;
    struct arg_int *potencia;
    struct arg_end *end;
} motor_args;

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

static struct
{
    struct arg_str *status;
    struct arg_str *check;
    struct arg_end *end;
} sd_args;

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
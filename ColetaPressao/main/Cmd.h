#ifndef CMD_H
#define CMD_H

#include <esp_console.h>

#include <argtable3/argtable3.h>

#define CMD_MOTOR_BIT (1 << 0)

#define FUNCTION_POINTER_DEFAULT int (*func)(int argc, char **argv)

static struct
{
    struct arg_str *start_stop;
    struct arg_int *potencia;
    struct arg_end *end;
} motor_args;

void cmd_register_motor(FUNCTION_POINTER_DEFAULT)
{
    motor_args.start_stop = arg_str0(NULL, NULL, "<S/T>", "Ligar (S) ou desligar (T)");
    motor_args.potencia = arg_int0(NULL, NULL, "<VALOR>", "Potencia do motor");
    motor_args.end = arg_end(2);

    const esp_console_cmd_t motor_cmd = {
        .command = "motor",
        .help = "Ligar ou Desliga o motor com uma certa Potencia",
        .hint = NULL,
        .func = func,
        .argtable = &motor_args,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&motor_cmd));
}

#endif
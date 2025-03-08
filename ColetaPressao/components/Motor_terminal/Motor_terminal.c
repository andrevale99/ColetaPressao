#include "Motor_terminal.h"

EventBits_t *EventBits_cmd_motor;
EventGroupHandle_t *handleEventBits_cmd_motor;

static struct
{
    struct arg_str *start_stop;
    struct arg_int *potencia;
    struct arg_end *end;
} motor_args;

void motor_link_eventgroup(EventBits_t *_EventBits_cmd_from_main,
                          EventGroupHandle_t *_handleEventBits_cmd_from_main)
{
    EventBits_cmd_motor = _EventBits_cmd_from_main;
    handleEventBits_cmd_motor = _handleEventBits_cmd_from_main;
}

int motor_cmd(int argc, char **argv)
{
    if (strcmp(argv[1], "S") == 0)
    {
        // colocar logica para pegar o valor da potencia

        *EventBits_cmd_motor = xEventGroupSetBits(
            *handleEventBits_cmd_motor, // The event group being updated.
            CMD_MOTOR_BIT);                 // The bits being set.
    }
    else if (strcmp(argv[1], "T") == 0)
    {
        *EventBits_cmd_motor = xEventGroupClearBits(
            *handleEventBits_cmd_motor, // The event group being updated.
            CMD_MOTOR_BIT);                 // The bits being cleared.
    }

    return 0;
}

esp_err_t cmd_register_motor(int (*func)(int argc, char **argv))
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
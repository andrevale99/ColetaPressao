#include "SD_terminal.h"

EventBits_t *EventBits_cmd_sd;
EventGroupHandle_t *handleEventBits_cmd_sd;

static struct
{
    struct arg_str *status;
    struct arg_str *check;
    struct arg_end *end;
} sd_args;

void sd_link_eventgroup(EventBits_t *_EventBits_cmd_from_main,
                       EventGroupHandle_t *_handleEventBits_cmd_from_main)
{
    EventBits_cmd_sd = _EventBits_cmd_from_main;
    handleEventBits_cmd_sd = _handleEventBits_cmd_from_main;
}

int sd_cmd(int argc, char **argv)
{
    if (strcmp(argv[1], "check") == 0)
    {
        *EventBits_cmd_sd = xEventGroupSetBits(
            *handleEventBits_cmd_sd, // The event group being updated.
            CMD_SD_CHECK);                  // The bits being set.
    }
    else if (strcmp(argv[1], "status") == 0)
    {
        *EventBits_cmd_sd = xEventGroupSetBits(
            *handleEventBits_cmd_sd, // The event group being updated.
            CMD_SD_STATUS);                 // The bits being set.
    }

    return 0;
}

esp_err_t cmd_register_sd(int (*func)(int argc, char **argv))
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
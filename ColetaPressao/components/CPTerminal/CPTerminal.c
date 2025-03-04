#include "CPTerminal.h"

/** Arguments used by 'join' function */
static struct
{
    struct arg_str *action;
    struct arg_int *potencia;
    struct arg_end *end;
} join_args;

int cpterminal_motor_cmd(int argc, char **argv)
{
    return 0;
}

void cpterminal_register_cmd()
{
    join_args.action = arg_str0(NULL, NULL, "<S/T>", "Acao da bomba (Ligar (S)/Desligar (T))");
    join_args.potencia = arg_int0(NULL, NULL, "<0...100>", "Potencia da bomba em \%");
    join_args.end = arg_end(2);

    const esp_console_cmd_t join_cmd = {
        .command = "motor",
        .help = "Liga a bomba comu uma determinada potencia\n"
                "  E desliga a bomba.",
        .hint = NULL,
        .func = &cpterminal_motor_cmd,
        .argtable = &join_args};

    ESP_ERROR_CHECK(esp_console_cmd_register(&join_cmd));
}

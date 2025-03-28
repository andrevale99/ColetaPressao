#ifndef SD_TERMINAL_H
#define SD_TERMINAL_H

#include <esp_console.h>
#include <argtable3/argtable3.h>

#define SD_BUFFER_SIZE 1024

#define SD_MOUNT_POINT "/"
#define SD_MAX_LEN_FILE_NAME 256

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
int sd_terminal(int argc, char **argv)
{
    return 0;
}

esp_err_t cmd_register_sd(void)
{
    sd_args.status = arg_str0(NULL, NULL, "status", "Verifica a situacao do SD");
    sd_args.check = arg_str0(NULL, NULL, "check", "Verifica o conteudo do SD");
    sd_args.end = arg_end(2);

    const esp_console_cmd_t sd_cmd = {
        .command = "sd",
        .help = "Comandos para verificacao do SD e arquivos",
        .hint = NULL,
        .func = sd_terminal,
        .argtable = &sd_args,
    };

    return (esp_console_cmd_register(&sd_cmd));
}


#endif
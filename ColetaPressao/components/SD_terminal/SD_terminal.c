#include "SD_terminal.h"

static uint8_t MaskBit = 0x00;
static char *file_name;

static struct
{
    struct arg_str *status;
    struct arg_str *check;
    struct arg_end *end;
} sd_args;

static int sd_terminal(int argc, char **argv)
{
    if (strcmp(argv[1], "status") == 0)
    {
        if (MaskBit & SD_MASK_DETECTED)
            printf("-> SD Detectado\n");
        else
            printf("-> SEM SD, verificar SD\n");

        if (MaskBit & SD_MASK_ON_WRITE)
            printf("-> SD sendo escrito\n");
        else
            printf("-> ERRO na escrita verificar SD\n");

        if (MaskBit & SD_MASK_FILE_CREATED)
            printf("-> Arquivo Criado: %s\n", file_name);
        else
            printf("-> ERRO ao criar o arquivo\n");

        printf("\n");
        fflush(stdout);
    }

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

void sd_set_bitmask(bool flag, uint8_t bit)
{
    if (flag)
        MaskBit |= bit;
    else
        MaskBit &= ~bit;
}

void sd_file_name(char *_file_name)
{
    file_name = _file_name;
}
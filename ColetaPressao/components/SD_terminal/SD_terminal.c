#include "SD_terminal.h"

static FILE *arq = NULL;
static uint8_t MaskBit = 0x00;
static char file_name[SD_MAX_LEN_FILE_NAME];

static struct
{
    struct arg_str *status;
    struct arg_str *rename_file;
    struct arg_str *create_file;
    struct arg_str *start_stop;
    struct arg_end *end;
} sd_args;

static void sd_set_file_name(char *_file_name)
{
    snprintf(file_name, SD_MAX_LEN_FILE_NAME, SD_MOUNT_POINT "/%s", _file_name);
}

static int sd_terminal(int argc, char **argv)
{
    if (strcmp(argv[1], "status") == 0)
    {
        if (MaskBit & SD_MASK_DETECTED)
            printf("%s -> SD Detectado %s\n", BHGRN, COLOR_RESET);
        else
            printf("%s -> SEM SD, verificar SD %s\n", BHRED, COLOR_RESET);

        if (MaskBit & SD_MASK_ON_WRITE)
            printf("%s -> SD sendo escrito %s\n", BHGRN, COLOR_RESET);
        else
            printf("%s -> ERRO na escrita verificar SD%s\n", BHRED, COLOR_RESET);

        if (MaskBit & SD_MASK_FILE_OPENED)
            printf("%s -> Arquivo Aberto: %s %s\n", BHGRN, file_name, COLOR_RESET);
        else
            printf("%s -> ERRO ao criar o arquivo %s\n", BHRED, COLOR_RESET);
    }

    else if (strcmp(argv[1], "create") == 0)
    {
        if (argc > 2)
        {
            sd_set_file_name(argv[2]);

            arq = fopen(sd_get_file_name(), "a");

            if (arq == NULL)
                printf("%s -> Erro ao criar o arquivo %s %s\n", BHRED, sd_get_file_name(), COLOR_RESET);
            else
            {
                printf("%s -> Arquivo %s foi criado %s\n", BHGRN, sd_get_file_name(), COLOR_RESET);
                fclose(arq);
            }
        }
        else
            printf("%s -> Coloque um nome para criar o arquivo %s\n", BHRED, COLOR_RESET);
    }

    else if (strcmp(argv[1], "rename") == 0)
    {
        if (argc > 2)
        {
            printf("%s -> Renomeando para %s %s\n", BHGRN, argv[2], COLOR_RESET);
            sd_set_file_name(argv[2]);
        }
        else

            printf("%s -> Coloque um nome %s\n", BHRED, COLOR_RESET);
    }

    else if (strcmp(argv[1], "start") == 0)
        sd_set_bitmask(true, SD_MASK_START);

    else if (strcmp(argv[1], "stop") == 0)
        sd_set_bitmask(false, SD_MASK_START);

    printf("\n");
    fflush(stdout);

    return 0;
}

esp_err_t cmd_register_sd(void)
{
    sd_args.status = arg_str0(NULL, NULL, "status", "Verifica a SITUACAO do SD");
    sd_args.rename_file = arg_str0(NULL, NULL, "rename", "RENOMEIA o arquivo de escrita do SD");
    sd_args.create_file = arg_str0(NULL, NULL, "create", "CRIA o arquivo de escrita do SD");
    sd_args.start_stop = arg_str0(NULL, NULL, "start / stop", "Inicia e Para o processo de escrita");
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

uint8_t sd_get_bitmask(void)
{
    return MaskBit;
}

char *sd_get_file_name(void)
{
    return &file_name[0];
}
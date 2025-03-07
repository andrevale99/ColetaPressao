#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define MAX_BUFFER 32

#define PREFIX_FILE_NAME "data"

char buffer[MAX_BUFFER];
uint8_t cont = 0;
FILE *arq;

void check_file_exist(FILE *_arq, char *file_name, uint8_t len)
{
    for (uint8_t sufixo = 0; sufixo < UINT8_MAX; ++sufixo)
    {
        snprintf(file_name, len, "data_%d.txt", sufixo);

        _arq = fopen(file_name, "r");
        if (_arq != NULL)
        {
            printf("Arquivo %s ENCONTRADO\n", file_name);
            fclose(_arq);

            sufixo++;

            snprintf(file_name, len, "data_%d.txt", sufixo);

            return;
        }
    }

    snprintf(file_name, len, "data_%d.txt", 0);
}

int main()
{
    check_file_exist(arq, buffer, MAX_BUFFER);

    printf("NOVO NOME: %s\n", buffer);

    return 0;
}
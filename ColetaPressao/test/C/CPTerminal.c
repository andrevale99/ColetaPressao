/**
 * @author Andre Menezes de Freitas Vale
 *
 * @brief Codigo que simula as mensagens que chegarar
 * pela UART do ESP32 e decodificara para realizar os
 * comandos do sistema.
 *
 * @note Codigo somente para nao ficar gravando o tempo
 * todo na flash do ESP32.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define MAX_LEN_BUFFER 32

//==========================================
//  VARS
//==========================================

/// @brief Buffer das mensagens que virao pela UART
char buffer[MAX_LEN_BUFFER];

//==========================================
//  FUNCS
//==========================================

/// @brief Funcao que ira decodificar a string
///
/// @param cmd string que vira da UART
void cpterminal_decode(const char *cmd);

//==========================================
//  MAIN
//==========================================
int main(int argc, char **argv)
{
    cpterminal_decode("SD \n");

    return 0;
}

//==========================================
//  FUNCS
//==========================================

void cpterminal_decode(const char *cmd)
{
    for (uint8_t i = 0; i < strlen(cmd); ++i)
    {
        if ((cmd[i] == ' ') || (cmd[i]=='\n'))
        {
            if (strcmp(buffer, "S") == 0)
            {
                //comando para ligar o motor
            }
            if (strcmp(buffer, "T") == 0)
            {
                //comando para desligar o motor
            }
            if (strcmp(buffer, "SD") == 0)
            {
                //comandos para configuracao do SS
            }

            break;
        }

        buffer[i] = cmd[i];
    }

    memset(buffer, '\0', MAX_LEN_BUFFER);
}
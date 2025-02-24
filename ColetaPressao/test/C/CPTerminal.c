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

/// @brief Funcao para receber os dados pela uart
/// e gravar no buffer da biblioteca
///
/// @param cmd string que vira da UART 
void cpterminal_recv(const char *cmd);

/// @brief Funcao que ira decodificar a string
void cpterminal_decode(void);

//==========================================
//  MAIN
//==========================================
int main(int argc, char **argv)
{
    snprintf(buffer, MAX_LEN_BUFFER, "S 50");

    cpterminal_recv(buffer);

    return 0;
}

//==========================================
//  FUNCS
//==========================================

void cpterminal_recv(const char *cmd)
{
    printf("" , strchr(cmd, ' '));
}

void cpterminal_decode(void)
{
    
}
#ifndef SD_TERMINAL_H
#define SD_TERMINAL_H

#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <esp_console.h>

#include <argtable3/argtable3.h>

#define SD_MOUNT_POINT "/sdcard"
#define SD_BUFFER_SIZE 128
#define SD_MAX_LEN_FILE_NAME 128

#define SD_MASK_DETECTED (1 << 0)
#define SD_MASK_ON_WRITE (1 << 1)
#define SD_MASK_FILE_CREATED (1 << 2)

/**
 *  @brief Funcao atrelado ao SD
 *
 *  @param argc Quantidade de argumentos
 *  @param argv String dos valores
 */
static int sd_terminal(int argc, char **argv);

/**
 * @brief Funcao para registrar de SD no terminal
 */
esp_err_t cmd_register_sd(void);

/**
 * @brief Funcao para setar o bit de uma maskara
 * para verificacao da situacao do SD durante a
 * consulta do console
 *
 * @param flag se é true ou false
 *
 * @note Exemplo: Se foi detectado o SD, colocar a flag
 * como "true" e colocar qual bit é. Para saber qual bit
 * colocar basta consultar os defines da biblioteca
 * todos os defines comecam com "SD_MASK...".
 */
void sd_set_bitmask(bool flag, uint8_t bit);

/**
 * @brief Nome do arquivo o qual esta sendo realizado 
 * a escrita
 */
void sd_file_name(char *_file_name);

#endif
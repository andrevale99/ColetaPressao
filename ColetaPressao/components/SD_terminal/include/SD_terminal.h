#ifndef SD_TERMINAL_H
#define SD_TERMINAL_H

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <esp_console.h>

#include <argtable3/argtable3.h>

#define CMD_SD_CHECK (1 << 1)
#define CMD_SD_STATUS (1 << 2)

/**
 *  @brief Funcao atrelado ao SD
 *
 *  @param argc Quantidade de argumentos
 *  @param argv String dos valores
 */
static int sd_terminal(int argc, char **argv);

/**
 * @brief Funcao para registrar de SD no terminal
 *
 * @param _EventBits_cmd_from_main A variavel de Bits de evento que sera
 * vinculado
 * 
 * @param _handleEventBits_cmd_from_main Handle dogrupo de eventos
 */
esp_err_t cmd_register_sd(EventBits_t *_EventBits_cmd_from_main,
                          EventGroupHandle_t *_handleEventBits_cmd_from_main);

#endif
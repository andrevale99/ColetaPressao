#ifndef MOTOR_TERMINAL_H
#define MOTOR_TERMINAL_H

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <esp_console.h>

#include <argtable3/argtable3.h>

#define CMD_MOTOR_BIT (1 << 0)

/**
 *  @brief Funcao de controle atrelado ao Terminal
 *  para ligar e desligar o motor
 *
 *  @param argc Quantidade de argumentos
 *  @param argv String dos valores
 */
static int motor_cmd(int argc, char **argv);

esp_err_t cmd_register_motor(EventBits_t *_EventBits_cmd_from_main,
                             EventGroupHandle_t *_handleEventBits_cmd_from_main);

#endif

#ifndef CPTERMINAL_H
#define CPTERMINAL_H

#include <esp_console.h>

#include <argtable3/argtable3.h>

void cpterminal_register_cmd();
int cpterminal_motor_cmd(int argc, char **argv);

#endif
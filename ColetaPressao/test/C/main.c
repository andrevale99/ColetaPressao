#include <stdio.h>

#define CONSTANTE_PULSOS_32MS_TO_1S 31.25

#define VAZAO_L_POR_S(_pulsos) (float)(((float)_pulsos / 2 * CONSTANTE_PULSOS_32MS_TO_1S) / 109) 

int main(int argc, char **argv)
{

    int pulsos = 10;

    float vazao = VAZAO_L_POR_S(pulsos);

    printf("%0.3f\n", vazao);

    return 0;
}
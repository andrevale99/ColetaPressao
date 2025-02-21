# Dados dos experimentos

Diretório que contém os dados coletados no experimentos.

<br>

## [Água](./agua/)
Experimento feito somente com água. A coluna P1 **NÃO DEVE
SER CONSIDERADA**, já que não tinha sensores suficientes.

### [Curva de Potência](./agua/curva_de_potencia/)

Diretório que contém os testes com água variando a potência da bomba
para verificar a curva de potência.

<br>

## Observações

A Coluna Contador representa o tempo da coleta naquele instante de tempo.
Considerando que o sensor está com uma taxa de 32 dados por segundos, para
transformar a contagem em segundos deve multiplicar cada valor da coluna por
$0.032\ s$.

> | Contador | Tempo (s) |
> |:--------:|:---------:|
> |    1     |   0.032   |
> |    103   |     x     |
>
>   $ x = 103 * 0.032 = 3.296 s

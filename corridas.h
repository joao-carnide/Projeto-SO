// João Filipe Carnide de Jesus Nunes   2017247442

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <string.h>

// estrutura com as configurações do ficheiro
typedef struct Dados {
    int unidades_sec;
    int d_volta, n_voltas;
    int equipas;
    int T_Avaria;
    int T_Box_min, T_Box_Max;
    int capacidade;
} dados;
typedef dados* config;

typedef struct corrida {
    dados * config;
} corrida;

dados* read_config(char* fname);
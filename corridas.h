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

typedef struct dados {
    int unidades_sec;
    int d_volta, n_voltas;
    int equipas;
    int T_Avaria;
    int T_Box_min, T_Box_Max;
    int capacidade;
} dados;

typedef struct corrida {
    dados * config;
} corrida;

corrida * cria_corrida();
corrida * read_config(char* fname, corrida* race);
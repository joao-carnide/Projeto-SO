// João Filipe Carnide de Jesus Nunes   2017247442
// Rui Alexandre Coelho Tapadinhas      2018283200

#ifndef CORRIDAS_INCLUDED
#define CORRIDAS_INCLUDED

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
#include <pthread.h>
#include <sys/fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/msg.h>

#define DEBUG
#define PROCS_INCIAIS 2
#define MAX_CAR_TEAM 100
#define MAX_EQUIPAS 100
#define TIME_ABASTECIMENTO 2
#define TOP_FIVE 5
#define PIPE_NAME "named_pipe"
#define MAX_CHAR 1024

// estrutura com as configurações do ficheiro config.txt
typedef struct Dados {
    int unidades_sec;
    int d_volta, n_voltas;
    int equipas;
    int max_cars_team;
    int T_Avaria;
    int T_Box_min, T_Box_Max;
    int capacidade;
} dados;
typedef dados* config;

typedef struct carro {
    int num;
    int speed;
    float consumption;
    int reliability;
    int n_voltas;
    int n_paragens;
    int avariado;
} carro;

typedef struct equipa {
    char nome_equipa[MAX_CHAR];
    char box[MAX_CHAR];
    carro carros [MAX_CAR_TEAM];
    int size_carros;
} equipa;

typedef struct Estatistica {
    carro top_carros[TOP_FIVE];
    carro ultimo_carro;
    int t_avarias;
    int n_paragens;
    int n_carros_pista;
} estatisticas;

typedef struct mem_structure {
    equipa equipas [MAX_EQUIPAS];
    int size_equipas;
    time_t init_time;
    int total_avarias;
    int n_paragens;
    estatisticas stats;
} mem_structure;

typedef struct malfunction_msg {
    long msg_type;
    
} mal_msg;

dados* read_config(char* fname);
char* get_current_time();
void write_log(FILE *fp, char* message);
void gestor_corrida();
void gestor_avarias();
void gestor_equipa();
void init_shm();
void init_semaphores();
void init_pipe();
void *check_carros( void* id_thread);
void terminate(int signal);
void wrong_command(char* cmd);
void load_car_to_shm(char* team, int car, int speed, float consumption, int reliability);
void start_command(char* cmd);
void new_car_command(char* team, int car, int speed, float consumption, int reliability);


#endif
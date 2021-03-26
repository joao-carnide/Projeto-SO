// João Filipe Carnide de Jesus Nunes   2017247442
// Rui Alexandre Coelho Tapadinhas      2018283200

#include "corridas.h"
#define DEBUG
#define TIME_ABASTECIMENTO 2

config race_config;
pid_t race_sim;
pid_t child_corrida, child_avarias;
int shmid;
mem_structure *race_stats;

dados* read_config(char* fname) {
    char buffer[20];
    char* ptr_buffer;
    dados* race = malloc(sizeof(dados));
    FILE *fp = fopen(fname, "r");
    if (fp == NULL) {
        perror("Error opening race config file");
        exit(1);
    }
    else {
        fgets(buffer, 20, fp);
        race->unidades_sec = atoi(buffer);
        fgets(buffer, 20, fp);
        ptr_buffer = strtok(buffer, ",");
        race->d_volta = atoi(ptr_buffer);
        ptr_buffer = strtok(NULL, ",");
        race->n_voltas = atoi(ptr_buffer);
        fgets(buffer, 20, fp);
        race->equipas = atoi(buffer);
        if (race->equipas < 3) {
            printf("Race config file rejected, number of teams [%d] must be at least 3\n", race->equipas);
            exit(1);
        }
        fgets(buffer, 20, fp);
        race->max_cars_team = atoi(buffer);
        fgets(buffer, 20, fp);
        race->T_Avaria = atoi(buffer);
        fgets(buffer, 20, fp);
        ptr_buffer = strtok(buffer, ",");
        race->T_Box_min = atoi(ptr_buffer);
        ptr_buffer = strtok(NULL, ",");
        race->T_Box_Max = atoi(ptr_buffer);
        fgets(buffer, 20, fp);
        race->capacidade = atoi(buffer);
        printf("Race configurations validated successfully!\n");
    }
    return race;
}

void gestor_corrida() {
    #ifdef DEBUG
    printf("[%d] Gestor de Corrida\n", getpid());
    #endif
    for (int i = 0; i < race_config->equipas; i++) {
        pid_t childs_equipas = fork();
        if (childs_equipas == 0) {
            gestor_equipa();
            exit(0);
        }
        wait(NULL);
    }
}

void gestor_avarias() {
    #ifdef DEBUG
    printf("[%d] Gestor de Avarias\n", getpid());
    #endif
}

void gestor_equipa() {
    #ifdef DEBUG
    printf("[%d] Gestor de Equipa\n", getpid());
    #endif
}

void init_shm() {
    if ((shmid = shmget(IPC_PRIVATE, sizeof(mem_structure), IPC_CREAT|0766)) < 0) {
		perror("Error in shmget with IPC_CREAT\n");
		exit(1);
	}

	if ((race_stats = shmat(shmid, NULL, 0)) == (mem_structure*)-1) {
		perror("Shmat error!");
		exit(1);
	}
}


int main(int argc, char *argv[]) {
    race_config = read_config("config.txt");
    
    init_shm();
    

    child_corrida = fork();
    if (child_corrida == 0) {
        gestor_corrida();
        exit(0);
    }
    else {
        child_avarias = fork();
        if (child_avarias == 0) {
            gestor_avarias();
            exit(0);
        }
    }
    for (int i = 0; i < PROCS_INCIAIS; i++) {
        wait(NULL);
    }
    race_sim = getpid();

    exit(0);
}
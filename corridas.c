// João Filipe Carnide de Jesus Nunes   2017247442

#include "corridas.h"

config race_config;

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

int main(int argc, char *argv[]) {
    race_config = read_config("config.txt");
    printf("Número de unidades de tempo por segundo: %d\n", race_config->unidades_sec);
    printf("Distância de cada volta: %d metros\n", race_config->d_volta);
    printf("Número de voltas da corrida: %d\n", race_config->n_voltas);
    printf("Número de equipas: %d\n", race_config->equipas);
    printf("T_Avaria: %d\n", race_config->T_Avaria);
    printf("T_Box_min: %d\nT_Box_Max: %d\n", race_config->T_Box_min, race_config->T_Box_Max);
    printf("Capacidade do deposito: %d litros\n", race_config->capacidade);
    exit(0);
}
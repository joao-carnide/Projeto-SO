#include "corridas.h"

corrida * cria_corrida() {
    corrida* race = (corrida*)malloc(sizeof(corrida));
    race->config = NULL;
    return race;
}

corrida * read_config(char* fname, corrida* race) {
    FILE *fp = fopen(fname, "r");
    dados* temp = (dados*)malloc(sizeof(dados));
    temp = race->config;
    if (fp == NULL) {
        perror("Error opening race config file");
        exit(1);
    }
    fscanf(fp, "%d", &temp->unidades_sec);
    printf("unidade de segundo = %ls", &temp->unidades_sec);
    fclose(fp);
    return race;
}

int main(int argc, char *argv[]) {
    corrida* race = cria_corrida();
    read_config("config.txt", race);
    exit(0);
}
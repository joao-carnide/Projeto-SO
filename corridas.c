// João Filipe Carnide de Jesus Nunes   2017247442
// Rui Alexandre Coelho Tapadinhas      2018283200

#include "corridas.h"

config race_config;

pid_t race_sim;
pid_t child_corrida, child_avarias;

int shmid_race, shmid_equipa, shmid_carro;
mem_structure *shared_race;

FILE * fp_log;

pthread_t threads_carro [MAX_CAR_TEAM];
int threads_ids [MAX_CAR_TEAM];

sem_t *semaforo, *sem_log;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex_equipas[MAX_EQUIPAS];
pthread_cond_t box_open[MAX_EQUIPAS];
pthread_cond_t box_ocupied[MAX_EQUIPAS];

sigset_t set_sinais;

int fd_named_pipe;
int fd_unnamed_pipes[MAX_EQUIPAS][2];
int num_car_unnamed_pipe;
struct timeval timeout;

int mqid;


// Função de leitura do ficheiro config.txt
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
        #ifdef DEBUG
        write_log(fp_log, "RACE CONFIGURATIONS VALIDATED SUCCESSFULLY");
        #endif
    }
    return race;
}

/* Funções para o ficheiro log.txt */
char* get_current_time() {
    char* current_time;
    time_t rawtime;
    struct tm * timeinfo;
    current_time = (char*)malloc(sizeof(char)*8); // 2 digitos de hora + 2 digitos minuto + 2 digitos segundo + 2 :
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    sprintf(current_time, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    return current_time;
}

void write_log(FILE *fp, char* message) {
    sem_wait(sem_log);
    char* str = (char*)malloc(sizeof(char)*1024);
    sprintf(str, "%s %s\n",get_current_time(), message);
    fprintf(fp, "%s", str);
    printf("%s", str);
    free(str);
    fflush(fp_log);
    sem_post(sem_log);
}

void wrong_command(char* cmd) {
    char* str = (char*)malloc(sizeof(char)*MAX_CHAR);
    sprintf(str, "WRONG COMMAND => %s", cmd);
    write_log(fp_log, str);
    free(str);
}

void start_command(char* cmd) {
    char* str = (char*)malloc(sizeof(char)*MAX_CHAR);
    sprintf(str, "NEW COMMAND RECEIVED: %s", cmd);
    write_log(fp_log, str);
    free(str);
}

void new_car_command(char* team, int car, int speed, float consumption, int reliability) {
    char* str = (char*)malloc(sizeof(char)*MAX_CHAR);
    sprintf(str, "NEW CAR LOADED => TEAM: %s, CAR: %02d, SPEED: %d, CONSUMPTION: %.2f, RELIABILITY: %d", team, car, speed, consumption, reliability);
    write_log(fp_log, str);
    free(str);
}

void new_malfunction(int car_num) {
    char* str = (char*)malloc(sizeof(char)*MAX_CHAR);
    sprintf(str, "NEW PROBLEM IN CAR %02d", car_num);
    write_log(fp_log, str);
    free(str);
}

void finished_race(int car_num) {
    char* str = (char*)malloc(sizeof(char)*MAX_CHAR);
    sprintf(str, "CAR %02d FINISHED THE RACE", car_num);
    write_log(fp_log, str);
    free(str);
}

void quit_race(int car_num) {
    char* str = (char*)malloc(sizeof(char)*MAX_CHAR);
    sprintf(str, "CAR %02d GAVE UP THE RACE", car_num);
    write_log(fp_log, str);
    free(str);
}

void car_in_box(int car_num) {
    char* str = (char*)malloc(sizeof(char)*MAX_CHAR);
    sprintf(str, "CAR %02d ARRIVED AT BOX FOR REPAIRING", car_num);
    write_log(fp_log, str);
    free(str);
}

void car_leave_box(int car_num) {
    char* str = (char*)malloc(sizeof(char)*MAX_CHAR);
    sprintf(str, "CAR %02d LEFT THE BOX", car_num);
    write_log(fp_log, str);
    free(str);
}

/* Funções para memoria partilhada---------------------------------------------------------------------------- */

void load_car_to_shm(char* team, int car, int speed, float consumption, int reliability) {
    int flag_adicionado = 0;
    sem_wait(semaforo);
    int existe_carro_num = check_carro_num(car);   
    if (existe_carro_num == 0) {
        int ind_eq = shared_race->size_equipas;
        
        for (int i = 0; i < ind_eq; i++) {
            if (strcmp(shared_race->equipas[i].nome_equipa, team) == 0) {
                if (shared_race->equipas[i].size_carros < race_config->max_cars_team) {
                    int i_car = shared_race->equipas[i].size_carros;
                    shared_race->equipas[i].carros[i_car].num = car;
                    shared_race->equipas[i].carros[i_car].speed = speed;
                    shared_race->equipas[i].carros[i_car].consumption = consumption;
                    shared_race->equipas[i].carros[i_car].reliability = reliability;
                    shared_race->equipas[i].carros[i_car].n_voltas = 0;
                    shared_race->equipas[i].carros[i_car].n_paragens = 0;
                    shared_race->equipas[i].carros[i_car].avariado = 0;
                    shared_race->equipas[i].carros[i_car].fuel = race_config->capacidade;
                    shared_race->equipas[i].carros[i_car].distancia = 0;
                    shared_race->equipas[i].size_carros++;
                    flag_adicionado = 1;
                    new_car_command(team, car, speed, consumption, reliability); //writes to log
                    break;
                }
                else {
                    flag_adicionado = 1;
                    write_log(fp_log, "CANNOT ADD MORE CARS, MAX REACHED");
                    break;
                }
            }
        }
        if (flag_adicionado == 0) {
            if (ind_eq < race_config->equipas) {
                strcpy(shared_race->equipas[ind_eq].nome_equipa, team);
                shared_race->equipas[ind_eq].carros[0].num = car;
                shared_race->equipas[ind_eq].carros[0].speed = speed;
                shared_race->equipas[ind_eq].carros[0].consumption = consumption;
                shared_race->equipas[ind_eq].carros[0].reliability = reliability;
                shared_race->equipas[ind_eq].carros[0].n_voltas = 0;
                shared_race->equipas[ind_eq].carros[0].n_paragens = 0;
                shared_race->equipas[ind_eq].carros[0].avariado = 0;
                shared_race->equipas[ind_eq].carros[0].fuel = race_config->capacidade;
                shared_race->equipas[ind_eq].carros[0].distancia = 0;
                shared_race->equipas[ind_eq].size_carros = 1;
                shared_race->equipas[ind_eq].flag_carro_box = 0;
                strcpy(shared_race->equipas[ind_eq].box, "livre");
                shared_race->size_equipas++;
                new_car_command(team, car, speed, consumption, reliability); //writes to log
            }
            else {
                write_log(fp_log, "CANNOT ADD MORE TEAMS, MAX REACHED");
            }
        }
    }
    else {
        write_log(fp_log, "CAR NUMBER ALREADY EXISTS");
    }
    sem_post(semaforo);
}


int check_carro_num(int num) {
    for (int e = 0; e<shared_race->size_equipas; e++) {
        for (int c = 0; c<shared_race->equipas[e].size_carros; c++) {
            if (shared_race->equipas[e].carros[c].num == num) {
                return 1;
            }
        }
    }
    return 0;
}
carro encontra_carro(int num) {
    carro carro_res;
    sem_wait(semaforo);
    for (int e = 0; e < shared_race->size_equipas; e++) {
        for (int c = 0; c < shared_race->equipas[e].size_carros; c++) {
            if (shared_race->equipas[e].carros[c].num == num) {
                carro_res = shared_race->equipas[e].carros[c];
            }
        }
    }
    sem_post(semaforo);
    return carro_res;
}

equipa encontra_equipa(int num_car) {
    equipa eq_aux;
    sem_wait(semaforo);
    for (int i = 0; i<shared_race->size_equipas; i++) {
        for (int c = 0; c < shared_race->equipas[i].size_carros; c++) {
            if (shared_race->equipas[i].carros[c].num == num_car) {
                eq_aux = shared_race->equipas[i];
                break;
            }
        }
    }
    sem_post(semaforo);
    return eq_aux;
}

int encontra_ind_carro (int num_car) {
    int ind_carro_aux;
    //sem_wait(semaforo);
    int ind_eq = encontra_ind_equipa(num_car);
    for (int c = 0; c < shared_race->equipas[ind_eq].size_carros; c++) {
        if (shared_race->equipas[ind_eq].carros[c].num == num_car) {
            ind_carro_aux = c;
        }
    }
    //sem_post(semaforo);
    return ind_carro_aux;
}

int encontra_ind_equipa (int num_car) {
    int ind_eq_aux;
    //sem_wait(semaforo);
    for (int i = 0; i<shared_race->size_equipas; i++) {
        for (int c = 0; c < shared_race->equipas[i].size_carros; c++) {
            if (shared_race->equipas[i].carros[c].num == num_car) {
                ind_eq_aux = i;
                break;
            }
        }
    }
    //sem_post(semaforo);
    return ind_eq_aux;
}

int atualiza_carro(int num) {
    sem_wait(semaforo);
    int ind_eq = encontra_ind_equipa(num);
    int ind_car = encontra_ind_carro(num);
    //TODO: falta escrever todas as alterações para o unnamed pipe
    if (strcmp(shared_race->equipas[ind_eq].carros[ind_car].estado, "corrida") == 0 || strcmp(shared_race->equipas[ind_eq].carros[ind_car].estado, "seguranca") == 0) {
        //comum ao estado corrida e segurança
        if (shared_race->equipas[ind_eq].carros[ind_car].distancia + shared_race->equipas[ind_eq].carros[ind_car].speed >= race_config->d_volta && shared_race->equipas[ind_eq].flag_carro_box == shared_race->equipas[ind_eq].carros[ind_car].num) {
            pthread_mutex_lock(&mutex_equipas[ind_eq]);
            strcpy(shared_race->equipas[ind_eq].carros[ind_car].estado, "box");
            strcpy(shared_race->equipas[ind_eq].box, "ocupada");
            pthread_cond_signal(&box_ocupied[ind_eq]);
            pthread_mutex_unlock(&mutex_equipas[ind_eq]);
        }

        if (shared_race->equipas[ind_eq].carros[ind_car].distancia >= race_config->d_volta) {
            shared_race->equipas[ind_eq].carros[ind_car].distancia -= race_config->d_volta;
            shared_race->equipas[ind_eq].carros[ind_car].n_voltas += 1;
        }
    }
    if (strcmp(shared_race->equipas[ind_eq].carros[ind_car].estado, "corrida") == 0) {
        shared_race->equipas[ind_eq].carros[ind_car].distancia += shared_race->equipas[ind_eq].carros[ind_car].speed;
        shared_race->equipas[ind_eq].carros[ind_car].fuel -= shared_race->equipas[ind_eq].carros[ind_car].consumption;
        if (shared_race->equipas[ind_eq].carros[ind_car].fuel <= 0) { // nao deve ser preciso pq neste momento o carro já está em modo de segurança
            strcpy(shared_race->equipas[ind_eq].carros[ind_car].estado, "desistencia");
        }
        else if (shared_race->equipas[ind_eq].carros[ind_car].fuel < 2 * shared_race->equipas[ind_eq].carros[ind_car].consumption * (race_config->d_volta/shared_race->equipas[ind_eq].carros[ind_car].speed)) {
            strcpy(shared_race->equipas[ind_eq].carros[ind_car].estado, "seguranca");
            if (strcmp(shared_race->equipas[ind_eq].carros[ind_car].estado, "livre") == 0) {
                strcpy(shared_race->equipas[ind_eq].carros[ind_car].estado, "reservado");
                shared_race->equipas[ind_eq].flag_carro_box = shared_race->equipas[ind_eq].carros[ind_car].num; //guarda na equipa o número do carro que reservou a boxpassword
            }
        }
        else if (shared_race->equipas[ind_eq].carros[ind_car].fuel < 4 * shared_race->equipas[ind_eq].carros[ind_car].consumption * (race_config->d_volta/shared_race->equipas[ind_eq].carros[ind_car].speed)) {
            //TODO: verificar estado da box
            if (strcmp(shared_race->equipas[ind_eq].carros[ind_car].estado, "livre") == 0) {
                strcpy(shared_race->equipas[ind_eq].carros[ind_car].estado, "reservado");
                shared_race->equipas[ind_eq].flag_carro_box = shared_race->equipas[ind_eq].carros[ind_car].num; //guarda na equipa o número do carro que reservou a boxpassword
            }
        }
        if (shared_race->equipas[ind_eq].carros[ind_car].n_voltas == race_config->n_voltas) {
            strcpy(shared_race->equipas[ind_eq].carros[ind_car].estado, "terminado");
        }
    }
    else if (strcmp(shared_race->equipas[ind_eq].carros[ind_car].estado, "seguranca") == 0) {
        shared_race->equipas[ind_eq].carros[ind_car].distancia += 0.3 * shared_race->equipas[ind_eq].carros[ind_car].speed;
        shared_race->equipas[ind_eq].carros[ind_car].fuel -= 0.4 * shared_race->equipas[ind_eq].carros[ind_car].consumption;
        if (shared_race->equipas[ind_eq].carros[ind_car].fuel <= 0) {
            strcpy(shared_race->equipas[ind_eq].carros[ind_car].estado, "desistencia");
        }
        if (shared_race->equipas[ind_eq].carros[ind_car].n_voltas == race_config->n_voltas) {
            strcpy(shared_race->equipas[ind_eq].carros[ind_car].estado, "terminado");
        }
    }
    else if (strcmp(shared_race->equipas[ind_eq].carros[ind_car].estado, "box") == 0) {
        strcpy(shared_race->equipas[ind_eq].box, "ocupada");
        sleep(rand() % (race_config->T_Box_Max - race_config->T_Box_min + 1) + race_config->T_Box_min); // random entre T_Box_min e T_Box_Max
        strcpy(shared_race->equipas[ind_eq].box, "livre");
        shared_race->equipas[ind_eq].flag_carro_box = 0;
        shared_race->equipas[ind_eq].carros[ind_car].distancia = 0;
        shared_race->equipas[ind_eq].carros[ind_car].n_voltas++;
    }
    else if (strcmp(shared_race->equipas[ind_eq].carros[ind_car].estado, "terminado") == 0) {
        finished_race(shared_race->equipas[ind_eq].carros[ind_car].num);
        shared_race->stats.n_carros_pista--;
        shared_race->total_cars_finished++;
        sem_post(semaforo);
        return 1;
    }
    else if (strcmp(shared_race->equipas[ind_eq].carros[ind_car].estado, "desistencia") == 0) {
        quit_race(shared_race->equipas[ind_eq].carros[ind_car].num);
        shared_race->stats.n_carros_pista--;
        shared_race->total_cars_finished++;
        sem_post(semaforo);
        return -1;
    }
    sem_post(semaforo);
    return 0;
}

/* ----------------------------------------------------------------------------------------------------------- */

/* Funções para as estatísticas */
void print_estatisticas(int signal) {
    if (getpid() == race_sim) {
        write_log(fp_log, "SIGNAL SIGTSTP RECEIVED");
        printf("-----------------STATISTICS-----------------\n");
        sem_wait(semaforo);
        ordena_carros();
        printf("TOTAL NUMBER OF MALFUNCIONS: %d\n", shared_race->stats.total_avarias);
        printf("TOTAL NUMBER OF REFUELLING: %d\n", shared_race->stats.num_paragens);
        printf("NUMBER OF CARS ON THE TRACK: %d\n", shared_race->stats.n_carros_pista);
        sem_post(semaforo);
        printf("--------------------------------------------\n");
    }
}

void print_estatisticas_final() {
    printf("-----------FINAL STATISTICS-----------------\n");
    sem_wait(semaforo);
    for (int i = 0; i < shared_race->size_tabela_posicoes_finais; i++) {
        printf("%d.\t%d\n", i+1, shared_race->tabela_posicoes_finais[i]);
    }
    printf("TOTAL NUMBER OF MALFUNCIONS: %d\n", shared_race->stats.total_avarias);
    printf("TOTAL NUMBER OF REFUELLING: %d\n", shared_race->stats.num_paragens);
    printf("NUMBER OF CARS ON THE TRACK: %d\n", shared_race->stats.n_carros_pista);
    sem_post(semaforo);
    printf("--------------------------------------------\n");
}

void ordena_carros() {
    printf("TOP 5 CARS\n");
    int k = 0;
    for (int e = 0; e < shared_race->size_equipas; e++) {
        for (int c = 0; c < shared_race->equipas[e].size_carros; c++) {
            shared_race->tabela_posicoes[k++] = shared_race->equipas[e].carros[c].num;
        }
    }
    shared_race->size_tabela_posicoes = k;

    bubbleSortDistancia(shared_race->tabela_posicoes, shared_race->size_tabela_posicoes);

    bubbleSortVoltas(shared_race->tabela_posicoes, shared_race->size_tabela_posicoes);

    printf("RANK\tNUM\tEQUIPA\tVOLTAS\tPARAGENS\n");

    for (int k = 0; k < shared_race->size_tabela_posicoes; k++) {
        int ind_eq = encontra_ind_equipa(shared_race->tabela_posicoes[k]);
        int ind_car = encontra_ind_carro(shared_race->tabela_posicoes[k]);

        printf("%d.\t%d\t%s\t%d\t%d\n", k+1, shared_race->equipas[ind_eq].carros[ind_car].num, shared_race->equipas[ind_eq].nome_equipa, shared_race->equipas[ind_eq].carros[ind_car].n_voltas, shared_race->equipas[ind_eq].carros[ind_car].n_paragens);
    }
    //ultimo
    printf("CAR IN LAST PLACE\n");
    int num_ultimo_pos = shared_race->tabela_posicoes[shared_race->size_tabela_posicoes-1];
    int ind_eq = encontra_ind_equipa(num_ultimo_pos);
    int ind_car = encontra_ind_carro(num_ultimo_pos);

    printf("%d.\t%d\t%s\t%d\t%d\n", shared_race->size_tabela_posicoes, shared_race->equipas[ind_eq].carros[ind_car].num, shared_race->equipas[ind_eq].nome_equipa, shared_race->equipas[ind_eq].carros[ind_car].n_voltas, shared_race->equipas[ind_eq].carros[ind_car].n_paragens);
}

void swap(int *xp, int *yp)
{
    int temp = *xp;
    *xp = *yp;
    *yp = temp;
}

void bubbleSortDistancia(int arr[], int n)
{
   int i, j;
   int ind_eq1 = -1, ind_eq2 = -1;
   int ind_car1 = -1, ind_car2 = -1;
   for (i = 0; i < n-1; i++)     
 
    // Last i elements are already in place  
    for (j = 0; j < n-i-1; j++){
        ind_eq1 = encontra_ind_equipa(arr[j]);
        ind_car1 = encontra_ind_carro(arr[j]);
        ind_eq2 = encontra_ind_equipa(arr[j+1]);
        ind_car2 = encontra_ind_carro(arr[j+1]);

        if (shared_race->equipas[ind_eq1].carros[ind_car1].distancia < shared_race->equipas[ind_eq2].carros[ind_car2].distancia)
            swap(&arr[j], &arr[j+1]);
    }
}

void bubbleSortVoltas(int arr[], int n)
{
   int i, j;
   int ind_eq1 = -1, ind_eq2 = -1;
   int ind_car1 = -1, ind_car2 = -1;
   for (i = 0; i < n-1; i++)     
 
    // Last i elements are already in place  
    for (j = 0; j < n-i-1; j++){
        ind_eq1 = encontra_ind_equipa(arr[j]);
        ind_car1 = encontra_ind_carro(arr[j]);
        ind_eq2 = encontra_ind_equipa(arr[j+1]);
        ind_car2 = encontra_ind_carro(arr[j+1]);

        if (shared_race->equipas[ind_eq1].carros[ind_car1].n_voltas < shared_race->equipas[ind_eq2].carros[ind_car2].n_voltas)
            swap(&arr[j], &arr[j+1]);
    }
}

/* ----------------------------------------------------------------------------------------------------------- */

void interrompe(int signal) {
    if (getpid() == child_corrida) {
        write_log(fp_log, "SIGNAL SIGUSR1 RECEIVED");
        kill(race_sim, SIGTERM);
        wait(NULL);
    }
}

void gestor_corrida( ) {
    fd_set read_set;
    int number_of_chars;
    char str[MAX_CHAR];
    timeout.tv_sec = 0;
    timeout.tv_usec = 10;
    #ifdef DEBUG
    write_log(fp_log, "RACE MANAGER PROCESS CREATED");
    #endif
    for (int i = 0; i < race_config->equipas; i++) {
        pipe(fd_unnamed_pipes[i]);
        pid_t childs_equipas = fork();
        if (childs_equipas == 0) {
            #ifdef DEBUG
            char* str = (char*)malloc(sizeof(char)*50);
            sprintf(str, "TEAM %d MANAGER PROCESS CREATED", i+1);
            write_log(fp_log, str);
            free(str);
            #endif
            gestor_equipa(i);
            exit(0);
        }
    }
    while (1) {
		FD_ZERO(&read_set);
        FD_SET(fd_named_pipe, &read_set);
		if (select(fd_named_pipe+1, &read_set, NULL, NULL, NULL) > 0) {
			if(FD_ISSET(fd_named_pipe,&read_set)){
				number_of_chars=read(fd_named_pipe, str, sizeof(str));
				str[number_of_chars-1]='\0'; //put a \0 in the end of string
                char* ptr_buffer;
                ptr_buffer = strtok(str, "\n");
                while (ptr_buffer != NULL){
                    if (strcmp(ptr_buffer, "START RACE!") == 0) {
                        start_command(ptr_buffer);
                        if (shared_race->size_equipas < race_config->equipas) {
                            write_log(fp_log, "CANNOT START, NOT ENOUGH TEAMS");
                        }
                        else {
                            sem_wait(semaforo);
                            if (shared_race->flag_corrida == 0) {
                                pthread_mutex_lock(&(shared_race->mutex_race_state));
                                shared_race->flag_corrida = 1;
                                int total_cars = 0;
                                for (int i = 0; i < shared_race->size_equipas; i++) {
                                    total_cars += shared_race->equipas[i].size_carros;
                                }
                                shared_race->stats.n_carros_pista = total_cars;
                                pthread_cond_broadcast(&(shared_race->cv_race_started));
                                pthread_mutex_unlock(&(shared_race->mutex_race_state));
                            }
                            else{
                                write_log(fp_log, "RACE ALREADY STARTED");
                            }
                            sem_post(semaforo);
                        }   
                    } 
                    else if (strncmp(ptr_buffer, "ADDCAR", strlen("ADDCAR")) == 0) {
                        char ptr_str_aux[MAX_CHAR];
                        strcpy(ptr_str_aux, ptr_buffer);
                        char team[20];
                        int car, speed, reliability;
                        float consumption;

                        int val = sscanf(ptr_str_aux, "ADDCAR TEAM: %s CAR: %d, SPEED: %d, CONSUMPTION: %f, RELIABILITY: %d", team, &car, &speed, &consumption, &reliability);
                        if (val == 5) {
                            team[strlen(team)-1] = '\0'; //takes the ',' out
                            load_car_to_shm(team,car, speed, consumption, reliability);
                        }
                        else {
                            wrong_command(ptr_buffer);
                        }
                    } 
                    else {
                        wrong_command(ptr_buffer);
                    }
                    ptr_buffer = strtok(NULL, "\n"); 
                }   
			}
		}
        sem_wait(semaforo);
        int total_carros = 0;
        for (int e = 0; e<shared_race->size_equipas; e++) {
            total_carros += shared_race->equipas[e].size_carros;
        }
        sem_post(semaforo);
        sem_wait(semaforo);
        if (shared_race->total_cars_finished < total_carros) {
            sem_post(semaforo);
            for (int i = 0; i < race_config->equipas; i++) {
                //FD_ZERO(&read_set);
                FD_SET(fd_unnamed_pipes[i][0], &read_set);
                if (select(fd_unnamed_pipes[i][0]+1, &read_set, NULL, NULL, &timeout) > 0) {

                    if(FD_ISSET(fd_unnamed_pipes[i][0],&read_set)) {
                        read(fd_unnamed_pipes[i][0], &num_car_unnamed_pipe, sizeof(int));
                        sem_wait(semaforo);
                        shared_race->total_cars_finished++;
                        sem_post(semaforo);
                    }
                }
            }
        }
	}
}

void gestor_avarias() {
    #ifdef DEBUG
    write_log(fp_log, "MALFUNCTION MANAGER PROCESS CREATED");
    #endif
    //start race
    pthread_mutex_lock(&(shared_race->mutex_race_state));
    while(shared_race->flag_corrida != 1) {
        pthread_cond_wait(&(shared_race->cv_race_started), &(shared_race->mutex_race_state));
    }
    pthread_mutex_unlock(&(shared_race->mutex_race_state));
    #ifdef DEBUG
    printf("Começar a enviar avarias\n");
    #endif
    while (1) {
        sleep(race_config->T_Avaria * race_config->unidades_sec);
        int min = INT_MAX, min_team_ind = -1, min_car_ind = -1;
        //int *ptr_flag = NULL; // Serve para só por a flag avariado = 1 apenas no carro com a menor reliability restante
        sem_wait(semaforo);
        for(int t = 0; t < shared_race->size_equipas; t++) {
            for (int c = 0; c < shared_race->equipas[t].size_carros; c++) {
                if (shared_race->equipas[t].carros[c].reliability < min && !shared_race->equipas[t].carros[c].avariado) {
                    min = shared_race->equipas[t].carros[c].reliability;
                    min_team_ind = t;
                    min_car_ind = c;
                }
            }
        }
        sem_post(semaforo);
        if (min_team_ind >= 0 && min_car_ind >= 0) {
            shared_race->equipas[min_team_ind].carros[min_car_ind].avariado = 1;
            sem_post(semaforo);
        }

        if (min <= 100) {
            mal_msg mal_message;
            mal_message.msg_type = shared_race->equipas[min_team_ind].carros[min_car_ind].num;
            if (msgsnd(mqid, &mal_message, sizeof(mal_msg), 0)) {
                perror("Error sending message");
            }
        }
        else if (min == INT_MAX) {
            #ifdef DEBUG
            printf("Não há mais carros para avariar\n");
            #endif
        }
        //reset min vars
        min = INT_MAX;
        min_team_ind = -1;
        min_car_ind = -1;

    }
}

void gestor_equipa(int ind_eq) {
    pthread_mutex_lock(&(shared_race->mutex_race_state));
    while(shared_race->flag_corrida != 1) {
        pthread_cond_wait(&(shared_race->cv_race_started), &(shared_race->mutex_race_state));
    }
    pthread_mutex_unlock(&(shared_race->mutex_race_state));
    #ifdef DEBUG
    printf("Criar threads carro e começar a corrida\n");
    #endif
    sem_wait(semaforo);
    for (int i = 0; i < shared_race->equipas[ind_eq].size_carros; i++) {
        strcpy(shared_race->equipas[ind_eq].carros[i].estado, "corrida");
    }
    sem_post(semaforo);
    sem_wait(semaforo);
    if (shared_race->flag_corrida == 1) {
        for (int i = 0; i < shared_race->equipas[ind_eq].size_carros; i++) {
            threads_ids[i] = i+1;
            int num_car = shared_race->equipas[ind_eq].carros[i].num;
            pthread_create(&threads_carro[ind_eq * race_config->max_cars_team + i], NULL, check_carros, &num_car);
            char* str = (char*)malloc(sizeof(char)*1024);
            sprintf(str, "CAR THREAD %d CREATED", num_car);
            write_log(fp_log, str);
            free(str);
        }
        sem_post(semaforo);
        while (1) {
            sem_wait(semaforo);
            pthread_mutex_lock(&mutex_equipas[ind_eq]);
            while (strcmp(shared_race->equipas[ind_eq].box, "livre") == 0) {
                pthread_cond_wait(&box_ocupied[ind_eq], &mutex_equipas[ind_eq]);
            }
            pthread_mutex_unlock(&mutex_equipas[ind_eq]);
            sem_post(semaforo);
            gerir_box(ind_eq);
        }
    }
    for (int i = 0; i < race_config->max_cars_team; i++) {
        pthread_join(threads_carro[ind_eq * race_config->max_cars_team + i], NULL);
    }

}

void gerir_box(int ind_eq) {
    sem_wait(semaforo);
    int num_car = shared_race->equipas[ind_eq].flag_carro_box;
    car_in_box(num_car);
    sem_post(semaforo);
    int ind_car = encontra_ind_carro(num_car);
    sem_wait(semaforo);
    shared_race->equipas[ind_eq].carros[ind_car].fuel = race_config->capacidade;
    shared_race->stats.num_paragens++;
    shared_race->equipas[ind_eq].carros[ind_car].n_paragens++;
    shared_race->equipas[ind_eq].carros[ind_car].distancia = 0;
    shared_race->equipas[ind_eq].carros[ind_car].n_voltas += 1;
    sleep(rand() % (race_config->T_Box_Max - race_config->T_Box_min + 1) + race_config->T_Box_min);
    strcpy(shared_race->equipas[ind_eq].box, "livre");
    shared_race->equipas[ind_eq].flag_carro_box = 0;
    strcpy(shared_race->equipas[ind_eq].carros[ind_car].estado, "corrida");
    car_leave_box(num_car);
    sem_post(semaforo);
}

void *check_carros(void* num_car) {
    int num = *((int *)num_car);
    int ind_eq = encontra_ind_equipa(num);
    int ind_car = encontra_ind_carro(num);

    pthread_mutex_lock(&mutex);

    mal_msg message;
    int check_msg;
    int check_estado;
    while(1) {
        check_msg = msgrcv(mqid, &message, sizeof(message), shared_race->equipas[ind_eq].carros[ind_car].num, IPC_NOWAIT);
        if (check_msg > 0) {
            new_malfunction(num);
            #ifndef DEBUG
            printf("[car] reliability = %ld\n", message.msg_type);
            #endif
            //sem_wait(semaforo);
            shared_race->stats.total_avarias++;
            strcpy(shared_race->equipas[ind_eq].carros[ind_car].estado, "seguranca");
            shared_race->equipas[ind_eq].flag_carro_box = num; //guarda na equipa o número do carro que reservou a boxpassword
            //sem_post(semaforo);
        }
        sem_post(semaforo);
        check_estado = atualiza_carro(num);
        if (check_estado > 0) {
            //TODO: tratar de terminar a thread em caso de desistencia ou terminado
            if (check_estado == -1) {
                // estado = desistencia
                #ifndef DEBUG
                printf("carro %02d desistiu\n", num);
                #endif
                close(fd_unnamed_pipes[ind_eq][0]);
                num_car_unnamed_pipe = num;
                write(fd_unnamed_pipes[ind_eq][1], &num_car_unnamed_pipe, sizeof(int));
                break;
            }
            else if (check_estado == 1) {
                // estado = terminado

                sem_wait(semaforo);
                shared_race->tabela_posicoes_finais[shared_race->size_tabela_posicoes_finais++] = num;
                sem_post(semaforo);                
                #ifndef DEBUG
                printf("carro %02d terminou\n", num);
                #endif
                close(fd_named_pipe);
                for (int i = 0; i < race_config->equipas; i++) {
                    close(fd_unnamed_pipes[i][0]);
                    if (i != ind_eq) {
                        close(fd_unnamed_pipes[i][1]);
                    }
                }
                num_car_unnamed_pipe = num;
                write(fd_unnamed_pipes[ind_eq][1], &num_car_unnamed_pipe, sizeof(int));
                break;
            }
        }
        if (shared_race->equipas[ind_eq].flag_carro_box == num) {
            //TODO: entrar na box
        }
        sleep(race_config->unidades_sec);
    }
    pthread_mutex_unlock(&mutex);
    char* str2 = (char*)malloc(sizeof(char)*1024);
    sprintf(str2, "CAR THREAD %d LEAVING...", num);
    write_log(fp_log, str2);
    free(str2);
    sleep(1);
    pthread_exit(NULL);
}

/* Funções de inicialização */
void init_shm() {
    if ((shmid_race = shmget(IPC_PRIVATE, sizeof(mem_structure), IPC_CREAT|0766)) < 0) {
		perror("Error in race shmget with IPC_CREAT");
		exit(1);
	}
	if ((shared_race = shmat(shmid_race, NULL, 0)) == (mem_structure*)-1) {
		perror("Shmat race error!");
		exit(1);
	}

    // initiate size vars
    shared_race->size_equipas = 0;
    shared_race->flag_corrida = 0;
    shared_race->stats.num_paragens = 0;
    shared_race->stats.total_avarias = 0;
    shared_race->stats.n_carros_pista = 0;
    shared_race->total_cars_finished = 0;
}

void init_semaphores() {
    sem_unlink("ACESSO");
    semaforo = sem_open("ACESSO", O_CREAT|O_EXCL, 0766, 1);
    sem_unlink("LOG");
    sem_log = sem_open("LOG", O_CREAT|O_EXCL, 0766, 1);
}

void init_pipe() {
    unlink(PIPE_NAME);
    if ((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600) < 0) && (errno != EEXIST)) {
        perror("Cannot create pipe");
        exit(0);
    }
    if ((fd_named_pipe = open(PIPE_NAME, O_RDWR)) < 0) {
        perror("Cannot open pipe for reading");
        exit(0);
    }
}

void init_mq() {
    mqid = msgget(IPC_PRIVATE, IPC_CREAT|0777);
    if (mqid < 0) {
        perror("Error creating message queue");
        exit(1);
    }
}

void init_cv () {
    pthread_mutexattr_t attrmutex; 
    pthread_condattr_t attrcondv;

    pthread_mutexattr_init(&attrmutex);
    pthread_mutexattr_setpshared(&attrmutex, PTHREAD_PROCESS_SHARED);

    pthread_condattr_init(&attrcondv);
    pthread_condattr_setpshared(&attrcondv, PTHREAD_PROCESS_SHARED);
    
    pthread_mutex_init(&(shared_race->mutex_race_state), &attrmutex);
    pthread_cond_init(&shared_race->cv_race_started, &attrcondv);

    for (int i = 0; i < race_config->equipas; i++) {
        pthread_mutex_init(&mutex_equipas[i], NULL);
        pthread_cond_init(&box_open[i], NULL);
        pthread_cond_init(&box_ocupied[i], NULL);
    }
}

void handle_signals() {
    sigemptyset(&set_sinais);
    sigaddset(&set_sinais, SIGINT);
    sigaddset(&set_sinais, SIGTSTP);
    sigaddset(&set_sinais, SIGUSR1);
    signal(SIGINT, terminate);
    signal(SIGTSTP, print_estatisticas);
}

/* ----------------------------------------------------------------------------------------------------------- */

void terminate(int signal) {
    if (getpid() == race_sim) {
        printf("\n^C PRESSED. TERMINATING PROGRAM\n");
        write_log(fp_log, "SIMULATOR CLOSING");
    }
    fclose(fp_log);
    sem_close(semaforo);
    sem_unlink("ACESSO");
    sem_close(sem_log);
    sem_unlink("LOG");
    shmdt(shared_race);
    shmctl(shmid_race, IPC_RMID, NULL);
    unlink(PIPE_NAME);
    for (int i = 0; i < race_config->equipas; i++) {
        close(fd_unnamed_pipes[i][1]);
        close(fd_unnamed_pipes[i][0]);
    }
    msgctl(mqid, IPC_RMID, NULL);
    exit(0);
}

int main(int argc, char *argv[]) {
    race_sim = getpid();
    fp_log = fopen("log.txt", "w");
    init_shm();
    init_semaphores();
    init_pipe();
    init_mq();
    write_log(fp_log, "SIMULATOR STARTING");
    race_config = read_config("config.txt");
    init_cv();
    handle_signals();
    child_corrida = fork();
    if (child_corrida == 0) {
        signal(SIGUSR1, interrompe);
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
}
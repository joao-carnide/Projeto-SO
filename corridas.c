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
pthread_cond_t box_open = PTHREAD_COND_INITIALIZER;
pthread_cond_t box_reserved = PTHREAD_COND_INITIALIZER;
pthread_cond_t box_ocupied = PTHREAD_COND_INITIALIZER;

sigset_t set_sinais;

int fd_named_pipe;
int fd_unnamed_pipes[MAX_EQUIPAS][2];

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

/* Funções para memoria partilhada---------------------------------------------------------------------------- */

void load_car_to_shm(char* team, int car, int speed, float consumption, int reliability) {
    int flag_adicionado = 0;
    sem_wait(semaforo);

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
            shared_race->equipas[ind_eq].size_carros = 1;
            strcpy(shared_race->equipas[ind_eq].box, "livre");
            shared_race->size_equipas++;
            new_car_command(team, car, speed, consumption, reliability); //writes to log
        }
        else {
            write_log(fp_log, "CANNOT ADD MORE TEAMS, MAX REACHED");
        }
    }
    sem_post(semaforo);
}




/* ----------------------------------------------------------------------------------------------------------- */

/* Funções para as estatísticas */
void print_estatisticas(int signal) {
    if (getpid() == race_sim) {
        write_log(fp_log, "SIGNAL SIGTSTP RECEIVED");
    }
}

/* ----------------------------------------------------------------------------------------------------------- */

void interrompe(int signal) {
    kill(race_sim, SIGTERM);
    wait(NULL);
}

void gestor_corrida( ) {
    fd_set read_set;
    int number_of_chars;
    char str[MAX_CHAR];

    #ifdef DEBUG
    write_log(fp_log, "RACE MANAGER PROCESS CREATED");
    #endif


    for (int i = 0; i < race_config->equipas; i++) {
        pipe(fd_unnamed_pipes[i]);
        // printf("unnamed pipe para a equipa %d criado\n", i+1);
        pid_t childs_equipas = fork();
        if (childs_equipas == 0) {
            #ifdef DEBUG
            char* str = (char*)malloc(sizeof(char)*50);
            sprintf(str, "TEAM %d MANAGER PROCESS CREATED", i+1);
            write_log(fp_log, str);
            free(str);
            #endif
            gestor_equipa();
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
                //printf("Received \"%s\" command\n", str);
                char* ptr_buffer;
                ptr_buffer = strtok(str, "\n");
                while (ptr_buffer != NULL){
                    if (strcmp(ptr_buffer, "START RACE!") == 0) {
                        start_command(ptr_buffer);
                        if (shared_race->size_equipas < race_config->equipas) {
                            write_log(fp_log, "CANNOT START, NOT ENOUGH TEAMS");
                        }
                        else {
                            //TODO: Começar a corrida
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
	}
}

void gestor_avarias() {
    #ifdef DEBUG
    write_log(fp_log, "MALFUNCTION MANAGER PROCESS CREATED");
    #endif
    while (1) {
        

        int min = INT_MAX;
        int *ptr_flag = NULL; // serve para repor a flag a 0 caso o valor anterior em min nao seja o verdadeiro min
        sem_wait(semaforo);
        for(int t = 0; t < shared_race->size_equipas; t++) {
            for (int c = 0; c < shared_race->equipas[t].size_carros; c++) {
                if (shared_race->equipas[t].carros[c].reliability < min && !shared_race->equipas[t].carros[c].avariado) {
                    min = shared_race->equipas[t].carros[c].reliability;
                    ptr_flag = &shared_race->equipas[t].carros[c].avariado;
                }
            }
        }
        if (ptr_flag != NULL) {
            *ptr_flag = 1;
        }
        sem_post(semaforo);

        //printf("mqid = %d\n", mqid);
        //printf("msg_type = %d\n", min);

        if (min <= 100) {
            mal_msg mal_message;
            mal_message.msg_type = min;
            if (msgsnd(mqid, &mal_message, sizeof(mal_msg), 0)) {
                perror("Error sending message");
            }
            //printf("\nmensagem enviada para o ar\n\n");
        }

        sleep(race_config->T_Avaria * race_config->unidades_sec);
    }
}

void gestor_equipa() {
    for (int i = 0; i < race_config->max_cars_team; i++) {
        threads_ids[i] = i+1;
        int reliability = 50;
        //pthread_create(&threads_carro[i], NULL, check_carros, &threads_ids[i]);
        pthread_create(&threads_carro[i], NULL, check_carros, &reliability);
        #ifdef DEBUG
        char* str = (char*)malloc(sizeof(char)*1024);
        sprintf(str, "CAR THREAD %d CREATED", i+1);
        write_log(fp_log, str);
        free(str);
        #endif
    }
    for (int i = 0; i < race_config->max_cars_team; i++) {
        pthread_join(threads_carro[i], NULL);
        #ifdef DEBUG
		//printf("Thread carro %d joined\n", i+1);
        #endif
    }

}

void *check_carros( void* id_thread) {
    int id = *((int *)id_thread);
    int reliab = *((int *)id_thread); //TODO: é possível passar como argumento um array de ints???
    pthread_mutex_lock(&mutex);
    char* str = (char*)malloc(sizeof(char)*1024);
    sprintf(str, "CAR THREAD %d DOING STUFF...", id);
    write_log(fp_log, str);

    printf("mqid = %d\n", mqid); //TODO: é necessário estar em shared memory???

    mal_msg message;

    msgrcv(mqid, &message, sizeof(message), reliab, 0);

    printf("[car] reliability = %ld\n", message.msg_type);


    sleep(2);
    pthread_mutex_unlock(&mutex);
    sprintf(str, "CAR THREAD %d LEAVING...", id);
    write_log(fp_log, str);
    free(str);
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
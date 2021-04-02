// João Filipe Carnide de Jesus Nunes   2017247442
// Rui Alexandre Coelho Tapadinhas      2018283200

#include "corridas.h"

config race_config;
pid_t race_sim;
pid_t child_corrida, child_avarias;
int shmid_race, shmid_equipa, shmid_carro;
mem_structure *shared_race;
equipa *shared_equipa;
carro *shared_carro;
FILE * fp_log;
pthread_t threads_carro [MAX_CAR_TEAM];
int threads_ids [MAX_CAR_TEAM];
sem_t *semaforo, *sem_log;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t box_open = PTHREAD_COND_INITIALIZER;
pthread_cond_t box_reserved = PTHREAD_COND_INITIALIZER;
pthread_cond_t box_ocupied = PTHREAD_COND_INITIALIZER;

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

// Funções para o ficheiro log.txt
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
    char* str = (char*)malloc(sizeof(char)*1024);
    sprintf(str, "%s %s\n",get_current_time(), message);
    fprintf(fp, "%s", str);
    printf("%s", str);
    fflush(fp_log);
}

void gestor_corrida() {
    #ifdef DEBUG
    write_log(fp_log, "RACE MANAGER PROCESS CREATED");
    #endif
    for (int i = 0; i < race_config->equipas; i++) {
        pid_t childs_equipas = fork();
        if (childs_equipas == 0) {
            #ifdef DEBUG
            char* str = (char*)malloc(sizeof(char)*50);
            sprintf(str, "TEAM %d MANAGER PROCESS CREATED", i+1);
            write_log(fp_log, str);
            #endif
            gestor_equipa();
            exit(0);
        }
        wait(NULL);
    }
}

void gestor_avarias() {
    #ifdef DEBUG
    write_log(fp_log, "MALFUNCTION MANAGER PROCESS CREATED");
    #endif
}

void gestor_equipa() {
    for (int i = 0; i < race_config->max_cars_team; i++) {
        threads_ids[i] = i+1;
        pthread_create(&threads_carro[i], NULL, check_carros, &threads_ids[i]);
        #ifdef DEBUG
        char* str = (char*)malloc(sizeof(char)*1024);
        sprintf(str, "CAR THREAD %d CREATED", i+1);
        write_log(fp_log, str);
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
    pthread_mutex_lock(&mutex);
    char* str = (char*)malloc(sizeof(char)*1024);
    sprintf(str, "CAR THREAD %d DOING STUFF...", id);
    write_log(fp_log, str);
    sleep(2);
    pthread_mutex_unlock(&mutex);
    sprintf(str, "CAR THREAD %d LEAVING...", id);
    write_log(fp_log, str);
    sleep(1);
    pthread_exit(NULL);
}

// Funções de inicialização
void init_shm() {
    // memória dados da corrida
    if ((shmid_race = shmget(IPC_PRIVATE, sizeof(mem_structure), IPC_CREAT|0766)) < 0) {
		perror("Error in race shmget with IPC_CREAT\n");
		exit(1);
	}
	if ((shared_race = shmat(shmid_race, NULL, 0)) == (mem_structure*)-1) {
		perror("Shmat race error!\n");
		exit(1);
	}
    // Memória dados da equipa
    if ((shmid_equipa = shmget(IPC_PRIVATE, sizeof(equipa), IPC_CREAT|0766)) < 0) {
        perror("Error in teams shmget with IPC_CREAT\n");
        exit(1);
    }
    if ((shared_equipa = shmat(shmid_equipa, NULL, 0)) == (equipa*)-1) {
        perror("Shmat teams error!\n");
        exit(1);
    }
    // memória dados do carro
    if ((shmid_carro = shmget(IPC_PRIVATE, sizeof(carro), IPC_CREAT|0766)) < 0) {
        perror("Error in cars shmget with IPC_CREAT\n");
        exit(1);
    }
    if ((shared_carro = shmat(shmid_carro, NULL, 0)) == (carro*)-1) {
        perror("Shmat cars error!\n");
        exit(1);
    }
    #ifdef DEBUG
    write_log(fp_log, "SHARED MEMORIES CREATED SUCCESSFULLY");
    #endif
}

void init_semaphores() {
    sem_unlink("ACESSO");
    semaforo = sem_open("ACESSO", O_CREAT|O_EXCL, 0766, 1);
    sem_unlink("LOG");
    sem_log = sem_open("LOG", O_CREAT|O_EXCL, 0766, 1);
    #ifdef DEBUG
    write_log(fp_log, "SEMAPHORES CREATED SUCCESSFULLY");
    #endif
}

void terminate() {
    write_log(fp_log, "SIMULATOR CLOSING");
    fclose(fp_log);
    sem_close(semaforo);
    sem_unlink("ACESSO");
    sem_close(sem_log);
    sem_unlink("LOG");
    shmdt(shared_race);
    shmctl(shmid_race, IPC_RMID, NULL);
    shmdt(shared_equipa);
    shmctl(shmid_equipa, IPC_RMID, NULL);
    shmdt(shared_carro);
    shmctl(shmid_carro, IPC_RMID, NULL);
    exit(0);
}

int main(int argc, char *argv[]) {
    fp_log = fopen("log.txt", "w");
    write_log(fp_log, "SIMULATOR STARTING");
    race_config = read_config("config.txt");
    init_shm();
    init_semaphores();
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
    terminate();
}
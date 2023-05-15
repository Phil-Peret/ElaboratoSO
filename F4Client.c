#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#define PATH_MAX 4096

void print_map(char[],int,int);
int check_choose(int, char[], int);

struct info_game{
    int n_player;
    int width;
    int height;
    int semaphore;
    int shared_memory;
    pid_t pid_server;
};

struct msg_info_game{
    long int msg_type;
    struct info_game info;

};

struct message_registration{
    long int msg_type;
    pid_t pid;
};

int main(){
    //cartella corrente
    char cwd[PATH_MAX];
    if(getcwd(cwd, sizeof(cwd)) != NULL){
            printf("Cartella corrente: %s\n",cwd);
    }
    else{
		exit(0);
    }

    //Semaforo per la gestione degli accessi alla partita
    int sem_access = semget(ftok(cwd,5), 2, 0666); //semaforo per la gestione della prima connessione
    int sem_turn;
    int shm_id;
    int dim_map[2];
    if (sem_access==-1){
		perror("Seaphore not created by Server");
		exit(0);
    }

    struct sembuf sops={0,-1,0};

    if(semop(sem_access,&sops,1)==-1){
		perror("Error in semop Client");
		exit(0);
    }

    //Message queue per l'iscrizione alla partita
    printf("Game registration...\n");
    int msg_id;
    if((msg_id=msgget(ftok(cwd,2),0666)) == -1){
		perror("Error call msgqueue");
    }

    //send pid to Server
    struct message_registration msg_reg;
    msg_reg.msg_type=1;
    msg_reg.pid=getpid();
    if(msgsnd(msg_id, &msg_reg, sizeof(msg_reg.pid), 0)==-1){
		perror("Error send message on Server");
		exit(0);
    }

    sops.sem_num=1; //semnum set
    if(semop(sem_access, &sops,1)==-1){
		perror("Semaphore error");
		exit(0);
    }

    sops.sem_num=2;
    sops.sem_op=0;

    //in attesa della risposta dal server
    if (semop(sem_access, &sops,1)==-1){
		perror("Error semaphore");
    }

    //lettura dei messaggi in queue
    struct msg_info_game info_recive;
    if(msgrcv(msg_id, &info_recive, sizeof(info_recive),(long int)(getpid()), 0)==-1){
	    perror("Error read message in a queue");
    }

    printf("Id shared memory: %i\n",info_recive.info.shared_memory);
    printf("Id semaphore: %i\n",info_recive.info.semaphore);
    printf("Dim map: %i x %i\n",info_recive.info.width, info_recive.info.height);
    printf("You are player%i, wait your turn...\n",info_recive.info.n_player);
    sem_turn=info_recive.info.semaphore;
    shm_id=info_recive.info.shared_memory;
    dim_map[0]=info_recive.info.width;
    dim_map[1]=info_recive.info.height;
    sops.sem_num=0;
    sops.sem_op=-1;
    if(semop(sem_turn,&sops,1)==-1){
		perror("Error sem ops!");
    }
    //carico la memoria condivisa
    char *shm_map=shmat(shm_id, NULL, 0666);
    if (shm_map == (void *) -1){
        perror("Shared memory attach!");
        exit(0);
    }
    print_map(shm_map,dim_map[0],dim_map[1]);
	int choose;
    printf("Choose position:");
	int valid=0;
	do {
		scanf("%i", &choose);
		if((valid=check_choose(choose,shm_map,dim_map[0]))){
			printf("Choose not valid, try again..\nChoose position: ");
		}
	}while(valid);




    return 0;


}

void print_map(char map[],int width,int height){
    for (int i=0; i<height; i++){
	printf("| ");
	for(int j=0; j<width; j++){
	    printf("%c | ",map[(i*height)+j]);
	}
	printf("\n");
    }
}


int check_choose(int choose, char map[], int width){
	if (map[choose]==' ' && choose>=0 && choose<width){
		return 0;
	}
	return 1;
}
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/msg.h>

#define PATH_MAX 4096
#define PLAYERS 2

void check_args(int, char**, int[]);
void print_guide();
void clean_map(char*, int length, int width);
void shm_remove(int);
void sem_remove(int);
void msg_queue_remove(int);
int message_in_queue(int);
void pprint();

struct info_game{
    int width;
    int height;
    key_t semaphore;
    pid_t pid_server;
};

struct msg_info_game{
    long int msg_type;
    struct info_game info;

};

struct msg_registration{
    long int msg_type;
    pid_t pid;
};

struct info_message{
    char pid_server[10];
    char height[3];
    char width[3];
    char pid_sem[10];
};

int main(int argc, char **argv){

    int dim_map[2];
    check_args(argc, argv, dim_map);
    char symbols[]={argv[3][0],argv[4][0]};
    printf("width: %d, height: %d \n",dim_map[0], dim_map[1]);
    printf("Symbol Player 1: %c\nSymbol Player 2: %c\n",symbols[0],symbols[1]);
    char  map[dim_map[0]*dim_map[1]];
    printf("Size of map: %lu\n",sizeof(map));

    //working directory per ftok...

    char cwd[PATH_MAX];
    if(getcwd(cwd, sizeof(cwd)) != NULL){
            printf("Cartella corrente: %s\n",cwd);
    }
    else{
	exit(0);
    }

    union semun {
	int val;
	struct semid_ds *buf;
	unsigned short  *array;
    } arg_sem;


    key_t key_shm_map = ftok(cwd,1);
    key_t key_msg = ftok(cwd,2);
    key_t key_sem_player1 = ftok (cwd, 3);
    key_t key_sem_player2 = ftok (cwd, 4);
    key_t key_sem_players = ftok(cwd,5);

    int sem_id_player[]={semget(key_sem_player1, 1, IPC_CREAT | 0666), semget(key_sem_player2, 1, IPC_CREAT | 0666)};//semafori per singolo giocatore
    int sem_id_access = semget(key_sem_players, 3, IPC_CREAT | 0666); //semaforo per la gestione di accesso alla partita e scambio di messaggi

    int shm_id_map = shmget(key_shm_map,sizeof(map),IPC_CREAT | 0666); //shared memory il campo di gioco
    int msg_id = msgget(key_msg, IPC_CREAT | 0666); //message queue

    if (shm_id_map==-1 || msg_id==-1 || sem_id_player[0]==-1 || sem_id_player[1]==-1 || sem_id_access==-1){
        perror("Error initialize IPC!");
        exit(0);
    }


    unsigned short value[]={2,2,1};
    arg_sem.array=value;

    //inizializzazione semafori per l'acesso'
    if(semctl(sem_id_access,3,SETALL,arg_sem)){
	perror("Semctl error, in SETVAL");
	exit(0);
    }

    //Server in attesa che i client si iscrivino alla partita
    printf("Waiting Players...\n");
    struct sembuf sops={1,0,0}; //attende finchè il semaforo non ha valore 0

    if(semop(sem_id_access, &sops, 1)==-1){
	perror("Error semop");
	exit(0);
    }
    printf("Players ready!\n");

    struct msg_registration msg_recive;
    pid_t players[PLAYERS];
    //lettura del messaggio da parte dei client su msgqueue
    for (int i=0;message_in_queue(msg_id)!=0; i++){
	if(msgrcv(msg_id, &msg_recive, sizeof(msg_recive), 0, 0)==-1){
	    perror("Error read message in a queue");
	}
	printf("Pid: %i\n",(int)(msg_recive.pid));
	players[i]=msg_recive.pid;
    }


    //risposta del server ai client su msgqueue
    struct msg_info_game msg_send;
    msg_send.info.semaphore=key_sem_player1;
    msg_send.info.pid_server=getpid();
    msg_send.info.width = dim_map[0];
    msg_send.info.height = dim_map[1];


    for(int i=0;i<PLAYERS; i++){//risposta per ogni client
	if(msgsnd(msg_id, &msg_send, sizeof(struct info_game), 0)==-1){
	    perror("Error send message on Server");
	exit(0);
	}
    }


    //Attach della memorie condivise
    char *shm_map=shmat(shm_id_map, NULL, 0666);
    if (shm_map == (void *) -1){
        perror("Shared memory attach!");
        exit(0);
    }

    clean_map(shm_map, dim_map[0], dim_map[1]);//Set /space in ogni posizione

    //set semaphore operation
    sops.sem_flg=0;
    sops.sem_op=-1;
    sops.sem_num=2;

    if(semop(sem_id_access, &sops,1)==-1){//sblocco i client per la lettura dei messagi
	perror("Error server semop");
    }

    sleep(5);
    shm_remove(shm_id_map);
    sem_remove(sem_id_player[0]);
    sem_remove(sem_id_player[1]);
    sem_remove(sem_id_access);
    msg_queue_remove(msg_id);
    //Invio informazioni ai client


    return 0;
}

int message_in_queue(int msqid){
    struct msqid_ds msq_info;
    if(msgctl(msqid, IPC_STAT, &msq_info)==-1){
	perror("Error get info by msq");
	exit(0);
    }
    return msq_info.msg_qnum;
}

void clean_map(char *campo, int length, int width){ //pulizia campo
    for(int i=0; i<length*width ; i++){
        campo[i]=' ';
    }
}

void check_args(int argc, char **argv, int dim[]){
    if (argc==1){
        printf("No argument passed\n");
        print_guide();
        exit(0);
    }

    if(strcmp(argv[1],"-h")==0){
        print_guide();
        exit(0);
    }

    if (argc != 5){
        printf("Need 4 args!\n");
        print_guide();
        exit(0); //TODO Gestione degli errori
    }
    dim[0]=atoi(argv[1]);
    dim[1]=atoi(argv[2]);
    if(argc != 5 || dim[0] < 5 || dim[1] < 5 || dim[0] > 15 || dim[1] > 15 ){
        printf("Error: Args not accepted!\n");
        print_guide();
        exit(0); //TODO Gestione degli errori
    }
}

void shm_remove(int shm_id){
    if(shmctl(shm_id,IPC_RMID,NULL) == -1){
	perror("Error detach shm\n");
	exit(0);
    }
}

void sem_remove(int sem_id){
    if(semctl(sem_id,0,IPC_RMID) == -1){
	perror("Error remove sem\n");
    }
}

void msg_queue_remove(int msg_id){
    if(msgctl(msg_id,IPC_RMID,NULL)){
	perror("Error remove msgqueue");
	exit(0);
    }
}

void pprint(){
    printf("⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢴⣶⣶⣿⣿⣿⣆\n⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡀⣀⣠⣴⣾⣮⣝⠿⠿⠿⣻⡟\n⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣶⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠁⠉⠀\n⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠟⠉⠀⠀⠀⠀\n⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢰⣿⣿⣿⣿⣿⣿⣿⣛⣛⣻⠉⠁⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢿⣿⣿⣿⣿⣿⡿⣿⣿⡿⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⠻⠿⠟⠋⠑⠛⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n");
}

void print_guide(){
    printf("./F4Server [map_length] [map_width] [symbol_player1] [symbiol_player2]\nesempio:\n./F4Server 5 5 X O\n");
}





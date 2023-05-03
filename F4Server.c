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

#define PATH_MAX 4096

void check_args(int, char**, int[]);
void print_guide();
void clean_map(char*, int length, int width);

struct shmseg{
    int pid[2];
};

int main(int argc, char **argv){

    int dim_map[2];
    check_args(argc, argv, dim_map);
    char symbols[]={argv[3][0],argv[4][0]};
    printf("Lunghezza: %d, Larghezza: %d \n",dim_map[0], dim_map[1]);
    printf("Symbol Player 1: %c\nSymbol Player 2: %c\n",symbols[0],symbols[1]);
    char  map[dim_map[0]*dim_map[1]];
    printf("Size of map: %lu\n",sizeof(map));

    //path corrente
    char cwd[PATH_MAX];
    if(getcwd(cwd, sizeof(cwd)) != NULL){
            printf("Cartella corrente: %s\n",cwd);
    }

    int sem = semget(ftok(cwd,0), 2, IPC_CREAT | 0666); //da cambiare?
    int shm_id_map = shmget(ftok(cwd,1),sizeof(map),IPC_CREAT | 0666);
    int shm_id_server = shmget(ftok(cwd,2), sizeof(struct shmseg), IPC_CREAT | 0666);
    if (shm_id_map==-1){
        perror("Shared Memory error!");
        exit(0);
    }

    if (shm_id_server==-1){
        perror("Shared Memory error!");
        exit(0);
    }
    //Attach della memoria condivisa
    char *shm_map=shmat(shm_id_map, NULL, 0666);
    if (shm_map == (void *) -1){
        perror("Shared memory attach!");
        exit(0);
    }

    clean_map(shm_map, dim_map[0], dim_map[1]);

    char *fifo_player1="Player1";
    char *fifo_player2="Player2";

    if(mkfifo(fifo_player1,0666)==-1){
        perror("Error make fifo");
        exit(0);
    }

    if(mkfifo(fifo_player2,0666)==-1){
        perror("Error make fifo");
        exit(0);
    }
    //In attesa dei Client
    int child_A=fork();
    if(child_A==0){
        int fd = open(fifo_player1, O_RDONLY);
	close(fd);
    }
    else{
        int child_B=fork();
        if(child_B==0){
            int fd = open(fifo_player2, O_RDONLY);
	    close(fd);
        }
    }
    pid_t wpid;
    int status=0;
    while ((wpid=wait(&status)) > 0);//I giocatori sono pronti alla partita
    //selezionare il primo giocatore, tramite semafori!

    int fd_fifo1=open(fifo_player1, O_RDONLY);
    char pid_player1[8];
    read(fd_fifo1, pid_player1, 8);
    printf("Player 1 pid: %s\n", pid_player1);
    close(fd_fifo1);

    int fd_fifo2=open(fifo_player2, O_RDONLY);
    char pid_player2[8];
    read(fd_fifo2, pid_player2, 8);
    printf("Player 2 pid: %s\n",pid_player2);
    close(fd_fifo2);




    return 0;
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

void print_guide(){
    printf("./F4Server [map_length] [map_width] [symbol_player1] [symbiol_player2]\nesempio:\n./F4Server 5 5 X O\n");
}





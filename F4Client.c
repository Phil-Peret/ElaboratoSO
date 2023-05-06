#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#define PATH_MAX 4096

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
    int sem_players = semget(ftok(cwd,5), 2, 0666); //semaforo per la gestione della prima connessione
    if (sem_players==-1){
	perror("Seaphore not created by Server");
	exit(0);
    }

    struct sembuf sops={0,-1,0};

    if(semop(sem_players,&sops,1)==-1){
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

    sops.sem_num=1;
    if(semop(sem_players, &sops,1)==-1){
	perror("Semaphore error");
	exit(0);
    }
    return 0;

}



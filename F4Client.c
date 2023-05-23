#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#define PATH_MAX 4096

void print_map(char[],int,int);
int check_choose(char*, char[], int);
void insert_getton_on_map(char*, int, int, int, char);
void fdrain(FILE *const);

struct info_game{
    int n_player;
	char symbol;
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

void sig(int sig){
	printf("Ehi ehi ci sono anche io\n");
}


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
	char symbol;
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
    printf("You are player%i & your symbol is %c, wait for your turn...\n",info_recive.info.n_player, info_recive.info.symbol);
	symbol=info_recive.info.symbol;
    sem_turn=info_recive.info.semaphore;
    shm_id=info_recive.info.shared_memory;
    dim_map[0]=info_recive.info.width;
    dim_map[1]=info_recive.info.height;
    //carico la memoria condivisa
    char *shm_map=shmat(shm_id, NULL, 0666);
    if (shm_map == (void *) -1){
        perror("Shared memory attach!");
        exit(0);
    }

	while(1){
		sops.sem_num=0;
		sops.sem_op=-1;

		//Semaforo per l'accesso al truno
		int ret;
		do{
			ret = semop(sem_turn,&sops,1);
		}while(ret == -1 && errno == EINTR);
		if (ret == -1 && errno != EINTR ){
			perror("Error with semop Client turn");
		}
		print_map(shm_map,dim_map[0],dim_map[1]);
		char choose[3];
		int pos=0;
		//clear standar input
		do {
			if(pos==-1){
				printf("Position not valid\n");
			}
			printf("Choose position: ");
			fdrain(stdin);
			fgets(choose,3,stdin);
			perror("Error fget");
		}while((pos=check_choose(choose,shm_map,dim_map[0])) == -1);
		insert_getton_on_map(shm_map,dim_map[0],dim_map[1], pos, symbol);
		print_map(shm_map,dim_map[0],dim_map[1]);
		printf("\n-----------\n");
		//semaforo per la conferma dell'inserimento della mossa
		sops.sem_num=1;
		do{
			ret = semop(sem_turn, &sops, 1);
		}while(ret == -1 && errno == EINTR);
		if( ret == -1 && errno != EINTR ){
			perror("Error with semop Client turn");
		}
	}
    return 0;


}


void fdrain(FILE *const in){
    if (in) {
        int const descriptor = fileno(in);
        int dummy;
        long flags;

        flags = fcntl(descriptor, F_GETFL);
        fcntl(descriptor, F_SETFL, flags | O_NONBLOCK);

        do {
            dummy = getc(in);
        } while (dummy != EOF);

        fcntl(descriptor, F_SETFL, flags);
    }
}


void insert_getton_on_map(char map[], int width, int height, int pos, char symbol){
	//that's simple?
	for (int i=((height*width)-1)+(pos-(width-1)); i>=0; i=i-width){
		if(map[i]==' '){
			map[i]=symbol;
			break;
		}
	}
}

void print_map(char map[],int width,int height){
    for (int i=0; i<height; i++){
	printf("| ");
		for(int j=0; j<width; j++){
	    		printf("%c | ",map[(i*width)+j]);
		}
	printf("\n");
    }
}


int check_choose(char* choose, char map[], int width){
	if(choose[strlen(choose)-1] != '\n') return -1; //se l'ultimo carattere non è \n allora '
	int pos = atoi(choose);
	if (pos >=0 && pos < width){
		if(map[pos]!=' '){
			return -1;
		}
	}
	return pos; //ritorna la posizione in int
}
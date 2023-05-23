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
#include <signal.h>
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
int check_winner(char*, int, int, char*);
void pprint();
void print_map(char*, int, int);
char get_value_by_position(char*, int, int, int, int);
int check_winner(char*, int, int, char*);

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

struct msg_registration{
    long int msg_type;
    pid_t pid;
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

    int sem_id_player[]={semget(key_sem_player1, 2, IPC_CREAT | 0666), semget(key_sem_player2, 2, IPC_CREAT | 0666)};//semafori per singolo giocatore
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
    if(semctl(sem_id_access,3,SETALL,arg_sem)){//semnum è ignorato
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
    msg_send.info.pid_server=getpid();
    msg_send.info.width = dim_map[0];
    msg_send.info.height = dim_map[1];
    msg_send.info.shared_memory = shm_id_map;

    for(int i=0;i<PLAYERS; i++){//risposta per ogni client
	msg_send.info.semaphore=(int)(sem_id_player[i]);
	msg_send.info.n_player=(i+1);
	msg_send.info.symbol=symbols[i];
	msg_send.msg_type=(long int)(players[i]);
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
	print_map(shm_map, dim_map[0], dim_map[1]);
    //set semaphore operation
    sops.sem_flg=0;
    sops.sem_op=-1;
    sops.sem_num=2;

    if(semop(sem_id_access, &sops,1)==-1){//sblocco i client per la lettura dei messaggi
		perror("Error server semop");
    }

	while (1){
		//Il truno inizia dal primo giocatore iscritto alla partita!
		struct sembuf start_turn[2] = {{0,1,0},{1,1,0}};
		if(semop(sem_id_player[0],start_turn,2)==-1){
			perror("Error semaphore ops!");
			printf("File: %s, Line %i\n", __FILE__, __LINE__);
		}

		struct sembuf end_turn[2] = {{0,0,0},{1,0,0}};
		//Il turno del giocatore 1 finisce
		if(semop(sem_id_player[0],end_turn, 2)==-1){
			perror("Error semaphore ops!");
			printf("File: %s, Line %i\n", __FILE__, __LINE__);
		}


		if(check_winner(shm_map, dim_map[0], dim_map[1], symbols)){
			printf("Player 1 won the game!\n");
			kill(players[0], SIGUSR1);
			break;
		}

		//Inizio turno giocatore 2
		if(semop(sem_id_player[1], start_turn, 2)){
			perror("Error semaphore ops!");
			printf("File: %s Line %i\n", __FILE__,__LINE__);
		}
		//Fine turno giocatore 2
		if(semop(sem_id_player[1],end_turn, 2)==-1){
			perror("Error semaphore ops!");
			printf("File: %s, Line %i\n", __FILE__, __LINE__);
		}

		if(check_winner(shm_map, dim_map[0], dim_map[1], symbols)){
			printf("Player 2 won the game!\n");
			kill(players[1], SIGUSR1);
			break;
		}
		//cancellazione delle IPC create
	}
    shm_remove(shm_id_map);
    sem_remove(sem_id_player[0]);
    sem_remove(sem_id_player[1]);
    sem_remove(sem_id_access);
    msg_queue_remove(msg_id);
    return 0;
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



int message_in_queue(int msqid){
    struct msqid_ds msq_info;
    if(msgctl(msqid, IPC_STAT, &msq_info)==-1){
	perror("Error get info by msq");
	exit(0);
    }
    return msq_info.msg_qnum;
}

void clean_map(char *campo, int height, int width){ //pulizia campo
    for(int i=0; i<(height*width); i++){
        campo[i]= ' ';
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

	if(strlen(argv[3]) != 1 || strlen(argv[4]) != 1){
		printf("Error: Only single char is accepted\n");
		print_guide();
		exit(0);
	}

    dim[0]=atoi(argv[1]);
    dim[1]=atoi(argv[2]);
    if(argc != 5 || dim[0] < 5 || dim[1] < 5 || dim[0] > 20 || dim[1] > 20 ){
        printf("Error: Args not accepted!\n");
        print_guide();
        exit(0); //TODO Gestione degli errori
    }

}

int check_winner(char* map, int width, int height, char symbol[]){
	//check riga
	for (int i=0; i<height; i++){
		for (int j=0,p1=0,p2=0; j<width; j++){
			if(get_value_by_position(map, width, height, i, j) == symbol[0]){
				p1++;
				p2=0;
			}
			else if (get_value_by_position(map, width, height, i, j) == symbol[1]){
				p2++;
				p1=0;
			}
			else if(get_value_by_position(map, width, height, i, j) == ' '){
				p1=0;
				p2=0;
			}
			if (p1>=4 || p2>=4){
				return 1;
			}
		}
	}

	//check colonna
	for (int i=0,p1=0,p2=0; i<width; i++){
		for (int j=0; j<height; j++){
			if(get_value_by_position(map, width, height, j, i) == symbol[0]){
				p1++;
				p2=0;
			}
			else if (get_value_by_position(map, width, height, j, i) == symbol[1]){
				p2++;
				p1=0;
			}
			else if (get_value_by_position(map, width, height, j, i) == ' '){
				p2=0;
				p1=0;
			}
			printf("%i %i\n",p1,p2);
			if (p1>=4 || p2>=4){
				return 1;
			}
		}
	}

	//check diagonale sup   |
	for(int i=3; i<height ; i++){
		for(int j=i, k=0, p1=0, p2=0; j>=0 && j<width; j--, k++){
			if(get_value_by_position(map, width, height, j, k) == symbol[0]){
				p1++;
				p2=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == symbol[1]){
				p2++;
				p1=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == ' '){
				p1=0;
				p2=0;
			}
			if (p1>=4 || p2>=4){
				return 1;
			}
		}
	}

	//check diagonale inferiore __ sottrazione 3 per limite del campo
	for (int i=1; i<(width-3); i++){
		for(int j=height, k=i, p1=0, p2=0; k<width; k++,j--){
			if(get_value_by_position(map, width, height, j, k)==symbol[0]){
				p1++;
				p2=0;
			}
			else if(get_value_by_position(map, width, height, j, k)==symbol[1]){
				p2++;
				p2=0;
			}
			else if(get_value_by_position(map, width, height, j, k)==' '){
				p1=0;
				p2=0;
			}
			if(p1>=4 || p2 >=4){
				return 1;
			}
		}
	}

	//check diagonale superiore destra
	for(int i=height; i>=0; i--){
		for(int j=height, k=width, p1=0, p2=0; j>=0; j--, k--){
			if(get_value_by_position(map, width, height, j, k) == symbol[0]){
				p1++;
				p2=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == symbol[1]){
				p2++;
				p1=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == ' '){
				p1=0;
				p2=0;
			}
			if (p1>=4 || p2>=4){
				return 1;
			}
		}
	}

	//check diagonale inferiore destra
	for(int i=height; i>=2; i--){
		for(int j=i, k=width, p1=0, p2=0; j>=0; j--, k--){
			if(get_value_by_position(map, width, height, j, k) == symbol[0]){
				p1++;
				p2=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == symbol[1]){
				p2++;
				p1=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == ' '){
				p1=0;
				p2=0;
			}
			if (p1>=4 || p2>=4){
				return 1;
			}
		}
	}

	for(int i=(width-1); i>=2; i--){
		for(int j=height, k=i, p1=0, p2=0; j>=0; j--, k--){
			if(get_value_by_position(map, width, height, j, k) == symbol[0]){
				p1++;
				p2=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == symbol[1]){
				p2++;
				p1=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == ' '){
				p1=0;
				p2=0;
			}
			if (p1>=4 || p2>=4){
				return 1;
			}
		}
	}
	return 0;
}


char get_value_by_position(char* map, int width, int height, int y, int x){
	if((x < width && x>=0) && (y < height && y >= 0)){
		return map[(y*width)+x];
	}
	return 'E';
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




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
int check_map_game(char*, int);

struct info_game{
    int n_player;
	char symbol;
    int width;
    int height;
    int semaphore;
    int shared_memory;
	char name_vs[16]; //nome avversario
    pid_t pid_server;
};

struct player{
	pid_t pid;
	char name[16];
};

struct msg_info_game{
    long int msg_type;
    struct info_game info;

};

struct end_game{
	int winner;
	long int status;
};

struct msg_end_game{
	long int msg_type;
	struct end_game info;
};


struct registration{
	pid_t pid;
	char name[16];  //nome giocatore
};

struct msg_registration{
    long int msg_type;
	struct registration info;
};

//variabili globali

int msg_id=0;
struct player p[PLAYERS];


void signal_client_exit(int sig){
	printf("One player left the game\n");
	struct msg_end_game msg;
	if(msgrcv(msg_id, &msg, sizeof(struct msg_end_game), (long int)(getpid()), 0) == -1){
		perror("Error get message in message queue\n");
	}
	printf("%ld \n",msg.info.status);
	for (int i=0; i<PLAYERS; i++){
		if (msg.info.status == (long int)(p[i].pid)){
			printf("%s left the game!\n", p[i].name);
			msg.msg_type = i == 0 ? (long int)(p[1].pid) : (long int)(p[0].pid);
			msg.info.winner = 0;
			msg.info.status = (long int)1;
			if(msgsnd(msg_id, &msg, sizeof(struct msg_end_game), 0) == -1){
				perror("Error send message");
			}
			kill(p[i].pid, SIGUSR1);
		}
	}
	exit(0);

}

int main(int argc, char **argv){
	if ((signal(SIGUSR2, signal_client_exit) == (void*)-1)){
		perror("Error setting signal");
	}
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
    msg_id = msgget(key_msg, IPC_CREAT | 0666); //message queue
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
	int ret;

	//protezione semafori dalle interruzioni causate dai segnali
	do{
		ret = semop(sem_id_access,&sops,1);
	}while(ret == -1 && errno == EINTR);
	if (ret == -1 && errno != EINTR ){
		perror("Error with semop Client turn");
		exit(0);
	}
    printf("Players ready!\n");

    struct msg_registration msg_recive;
    //lettura del messaggio da parte dei client per la registrazione alla partita su msgqueue
    for (int i=0;message_in_queue(msg_id)!=0; i++){
		if(msgrcv(msg_id, &msg_recive, sizeof(msg_recive), 0, 0)==-1){
			perror("Error read message in a queue");
		}
		printf("Pid: %i\n",(int)(msg_recive.info.pid));
		p[i].pid=msg_recive.info.pid;
		strcpy(p[i].name, msg_recive.info.name);
    }

	printf("Pid: %i, Name: %s\n", p[0].pid, p[0].name);
	printf("Pid: %i, Name: %s\n",p[1].pid, p[1].name);


    //risposta del server ai client su msgqueue
    struct msg_info_game msg_send;
    msg_send.info.pid_server=getpid();
    msg_send.info.width = dim_map[0];
    msg_send.info.height = dim_map[1];
    msg_send.info.shared_memory = shm_id_map;

    for(int i=0; i<PLAYERS; i++){//risposta per ogni client
		msg_send.info.semaphore=(int)(sem_id_player[i]);
		msg_send.info.n_player=(i+1);
		strcpy(msg_send.info.name_vs, p[1-i].name);
		msg_send.info.symbol=symbols[i];
		msg_send.msg_type=(long int)(p[i].pid);
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

    do{
		ret = semop(sem_id_access,&sops,1);
	}while(ret == -1 && errno == EINTR);
	if (ret == -1 && errno != EINTR ){
		perror("Error with semop Client turn");
	}

	int check;
	while(check_map_game(shm_map, dim_map[0])){
		//Inizio turno giocatore 1
		struct sembuf start_turn[2] = {{0,1,0},{1,1,0}};
		do{
			ret = semop(sem_id_player[0],start_turn,2);
		}while(ret == -1 && errno == EINTR);
		if(ret == -1 && errno != EINTR ){
			perror("Error with semop Client turn");
		}

		struct sembuf end_turn[2] = {{0,0,0},{1,0,0}};

		//Fine turno giocatore 2
		do{
			ret = semop(sem_id_player[0],end_turn,2);
		}while(ret == -1 && errno == EINTR);
		if(ret == -1 && errno != EINTR ){
			perror("Error with semop Client turn");
		}

		if((check=check_winner(shm_map, dim_map[0], dim_map[1], symbols)) == -1){ //informazioni della fine della partita ai client
			printf("Tie!\n");
			struct msg_end_game msg;
			for (int i=0; i<PLAYERS; i++){
				msg.msg_type = (long int)p[i].pid;
				msg.info.winner = -1;
				msg.info.status = 0;
				if(msgsnd(msg_id, &msg, sizeof(msg), 0)==-1){
					perror("Error send message on Server");
					exit(0);
				}
				kill(p[i].pid, SIGUSR1);
			}
			break;
		}
		else if (check){
			struct msg_end_game msg;
			for (int i=0; i<PLAYERS; i++){
				msg.msg_type = (long int)p[i].pid;
				msg.info.winner = i==0 ? 1 : 0;
				msg.info.status = 0;
				if(msgsnd(msg_id, &msg, sizeof(msg), 0)==-1){
					perror("Error send message on Server");
					exit(0);
				}
				kill(p[i].pid, SIGUSR1);
			}
			break;
		}

		//Inizio turno giocatore 2
		do{
			ret = semop(sem_id_player[1],start_turn,2);
		}while(ret == -1 && errno == EINTR);
		if(ret == -1 && errno != EINTR ){
			perror("Error with semop Client turn");
		}

		//Fine turno giocatore 2
		do{
			ret = semop(sem_id_player[1],start_turn,2);
		}while(ret == -1 && errno == EINTR);
		if(ret == -1 && errno != EINTR ){
			perror("Error with semop Client turn");
		}

		if((check=check_winner(shm_map, dim_map[0], dim_map[1], symbols)) == -1){ //informazioni della fine della partita ai client
			printf("Tie!\n");
			struct msg_end_game msg;
			for (int i=0; i<PLAYERS; i++){
				msg.msg_type = (long int)p[i].pid;
				msg.info.winner = -1;
				msg.info.status = 0;
				if(msgsnd(msg_id, &msg, sizeof(msg), 0)==-1){
					perror("Error send message on Server");
					exit(0);
				}
				kill(p[i].pid, SIGUSR1);
			}
			break;
		}
		else if (check){
			struct msg_end_game msg;
			for (int i=0; i<PLAYERS; i++){
				msg.msg_type = (long int)p[i].pid;
				msg.info.winner = i==1 ? 1 : 0;
				msg.info.status = 0;
				if(msgsnd(msg_id, &msg, sizeof(msg), 0)==-1){
					perror("Error send message on Server");
					exit(0);
				}
				kill(p[i].pid, SIGUSR1);
			}
			break;
		}
		//cancellazione delle IPC create
	}
	sleep(3);
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

int check_map_game(char* map, int width){
	for (int i=0; i<width; i++){
		if (map[i]==' '){
			return 1;
		}
	}
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
	//check parità
	int valid=0;
	for (int i=0; i<width; i++){
		if(map[i] != ' '){
			valid++;
		}
	}
	if (valid==width){
		return -1;
	}
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




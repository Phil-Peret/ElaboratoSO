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
#include "message_queue.c"
#include "semaphore.c"
#include "map.c"
#include "color.c"

#define PATH_MAX 4096
#define TIMEOUT 30
#define PLAYERS 2

void check_args(int, char**, int[]);
void print_guide();
void shm_remove(int);
void sem_remove(int);
void msg_queue_remove(int);
int message_in_queue(int);
void pprint();
char get_value_by_position(char*, int, int, int, int);
int check_map_game(char*, int);
int check_winner(int, int, int, char *);
void clear_ipc();

struct info_game{
	int n_player;
	char symbol;
	int width;
	int height;
	int semaphore;
	int shared_memory;
	pid_t pid_server;
	int sem_end;
};

struct name_op{
	char name[16];
	char symbol;
};

struct msg_name_op{
	long int msg_type;
	struct name_op op;
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
	int vs_cpu;
};

struct msg_registration{
	long int msg_type;
	struct registration info;
};

struct data_select_cell{
	int move;
};

struct select_cell{
	long int msg_type;
	struct data_select_cell data;
};

//variabili globali

int msg_id=0;
struct player p[PLAYERS];
int sem_id_player[PLAYERS];
int sem_id_end_player[PLAYERS];
int sem_id_access;
int shm_id_map;
int counter_c = 0;
pid_t child_pid = 0;
char *shm_map;


void signal_client_exit(int sig){
	kill(child_pid, SIGTERM);
	struct sembuf sops={0,0,0}; //attesa 0
	struct msg_end_game msg;
	if(msgrcv(msg_id, &msg, sizeof(struct msg_end_game), (long int)(getpid()), 0) == -1){
		perror("Error get message in message queue\n");
	}
	for (int i=0; i<PLAYERS; i++){
		if (msg.info.status == (long int)(p[i].pid)){
			kill((pid_t)msg.msg_type, SIGUSR1);
			printf("%s ha lasciato la partita\n", p[i].name);
			msg.msg_type =(long int)(p[1-i].pid);
			msg.info.winner = 1;
			msg.info.status = 1;
			send_message(msg_id, &msg, sizeof(struct msg_end_game), 0);
			semop_siginterrupt(sem_id_end_player[1-i], &sops, 1);
		}
	}
	clear_ipc();
	exit(0);
}



void signal_term_server(int sig){
	struct sembuf sops={0,0,0}; //attesa 0
	if(sig==SIGINT && counter_c==0){
		printf("Premi Ctrl+C per terminare il programma(5 sec)\n");
		counter_c++;
		alarm(5);
	}
	else if(sig == SIGINT && counter_c != 0){
		if(p[0].pid == 0 || p[1].pid == 0){
			for (int i=0; i<PLAYERS; i++){
				if(p[i].pid != 0){
					struct msg_name_op msg;
					msg.msg_type = (long int)(p[i].pid);
					msg.op.name[0]=' ';
					msg.op.symbol = -1;
					send_message(msg_id, &msg, sizeof(struct msg_end_game), 0);
					semop_siginterrupt(sem_id_end_player[i], &sops, 1);
				}
			}
		}
		else{
			if(child_pid!=0){
				kill(child_pid, SIGTERM);
			}
			printf("Server shutdown...\n");
			//mando ai client l'avviso di terminazione del server
			for (int i=0; i<PLAYERS; i++){
				kill(p[i].pid, SIGUSR1);
				struct msg_end_game msg;
				msg.msg_type = (long int)(p[i].pid);
				printf("mando messaggio a %i\n", p[i].pid);
				msg.info.winner = -1;
				msg.info.status = 1;
				send_message(msg_id, &msg, sizeof(struct msg_end_game), 0);
				semop_siginterrupt(sem_id_end_player[i], &sops, 1);
			}
		}
		clear_ipc();
		exit(0);
	}
	else if(sig == SIGALRM){
		counter_c=0;
		printf("Operazione annullata\n");
	}
}


int main(int argc, char **argv){
	p[0].pid=0;
	p[1].pid=0;
	pid_t server_pid = getpid();
	//handler signal
	if (signal(SIGUSR2, signal_client_exit) == (void*)-1){
		perror("Error setting signal");
	}
	if(signal(SIGINT, signal_term_server) == (void*)-1){
		perror("Error setting signal");
	}
	if(signal(SIGALRM, signal_term_server) == (void*)-1){
		perror("Error setting signal");
	}
	int dim_map[2];
	check_args(argc, argv, dim_map);
	char symbols[]={argv[3][0],argv[4][0]};
	char  map[dim_map[0]*dim_map[1]];
	printf("Dimensione del campo: %lu\n",sizeof(map));
	char cwd[PATH_MAX];

	//working directory per ftok...

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
	key_t key_sem_end_player1 = ftok(cwd, 6);
	key_t key_sem_end_player2 = ftok(cwd, 7);

	sem_id_player[0] = semget(key_sem_player1, 3, IPC_CREAT | IPC_EXCL | 0666);
	sem_id_player[1] = semget(key_sem_player2, 3, IPC_CREAT | IPC_EXCL | 0666); //semafori per singolo giocatore
	sem_id_access = semget(key_sem_players, 2, IPC_CREAT | IPC_EXCL | 0666); //semaforo per la gestione di accesso alla partita e scambio di messaggi
	shm_id_map = shmget(key_shm_map,sizeof(map),IPC_CREAT | IPC_EXCL | 0666); //shared memory il campo di gioco
	msg_id = msgget(key_msg, IPC_CREAT | IPC_EXCL | 0666); //message queue
	sem_id_end_player[0] = semget(key_sem_end_player1, 1, IPC_CREAT | IPC_EXCL | 0666);
	sem_id_end_player[1] = semget(key_sem_end_player2, 1, IPC_CREAT | IPC_EXCL | 0666);

	//controllo errori nella creazione delle IPC
	if (shm_id_map==-1 || msg_id==-1 || sem_id_player[0]==-1 || sem_id_player[1]==-1 || sem_id_access==-1){
		perror("Non è possibile inizializzare un nuovo server!\n");
		exit(0);
	}

	unsigned short value[]={2};
	arg_sem.array=value;

	//inizializzazione semafori per l'accesso
	if(semctl(sem_id_access,0,SETALL,arg_sem)){	//semnum è ignorato
		perror("Semctl error, in SETALL");
		exit(0);
	}

	//Server in attesa che i client si iscrivino alla partita
	printf("In attesa di altri giocatori...\n");
	struct sembuf sops={0,0,0}; //attende finchè il semaforo non ha valore 0
	//semop_siginterrupt(sem_id_access,&sops,1);
	//printf("Players ready!\n");

	//setto il valore del semaforo per la conferma terminazione dei client
	arg_sem.array = NULL;
	arg_sem.val = 1;
	for (int i = 0; i < PLAYERS ; i++ ){
		if(semctl(sem_id_end_player[i], 0, SETVAL, arg_sem)){
			perror("Semctl error in SETVAL");
			exit(0);
		}
	}

	//lettura del messaggio da parte dei client per la registrazione alla partita su msgqueue e risposta
	for (int i=0; i<PLAYERS; i++){
		struct msg_registration msg_recive;
		recive_message(msg_id, &msg_recive, sizeof(msg_recive), 1, 0);
		p[i].pid=msg_recive.info.pid;
		strcpy(p[i].name, msg_recive.info.name);
		printf("Giocatore %s connesso -> symbol %c \n", p[i].name, symbols[i]);
		if(msg_recive.info.vs_cpu == 1 && i == 0){
			printf("CPU Player creato!\n");
			if(fork() == 0){
				sigset_t signal_set;
				sigemptyset(&signal_set);
				sigaddset(&signal_set, SIGINT);
				sigprocmask(SIG_BLOCK, &signal_set, NULL);
				execl(strcat(cwd, "/F4ClientAuto.o"),"CPU\0", (char*)NULL);
				exit(0);
			}
		}
		//risposta del server
		struct msg_info_game msg_send;
		msg_send.msg_type=(long int)(p[i].pid);
		msg_send.info.n_player = (i+1);
		msg_send.info.symbol = symbols[i];
		msg_send.info.width = dim_map[0];
		msg_send.info.height = dim_map[1];
		msg_send.info.semaphore=(int)(sem_id_player[i]);
		msg_send.info.pid_server = server_pid;
		msg_send.info.shared_memory = shm_id_map;
		msg_send.info.sem_end = sem_id_end_player[i];
		send_message(msg_id, &msg_send, sizeof(struct msg_info_game), 0);
	}

	for (int j=0; j < PLAYERS; j++){ //aggiunta
		struct msg_name_op msg_send;
		msg_send.msg_type = p[j].pid;
		strcpy(msg_send.op.name, p[1-j].name);
		msg_send.op.symbol = symbols[1-j];
		msgsnd(msg_id, &msg_send, sizeof(struct msg_name_op), 0);
	}

	//Attach della memorie condivise
	shm_map=shmat(shm_id_map, NULL, 0666);
	if (shm_map == (void *) -1){
		perror("Shared memory attach!");
		exit(0);
	}
	clean_map(shm_map, dim_map[0], dim_map[1]); //Set /space in ogni posizione

	while(check_map_game(shm_map, dim_map[0])){
		//Inizio turno giocatore 1
		struct sembuf start_turn[2] = {{0,1,0},{1,1,0}};
		struct sembuf end_turn[2] = {{0,0,0},{1,0,0}};
		semop_siginterrupt(sem_id_player[0], start_turn, 2);
		struct sembuf confirm_move = {2,1,0}; //conferma inserimento mossa nella memoria condivisa
		child_pid=fork();
		if(child_pid==0){
			sigset_t signal_set;
			sigemptyset(&signal_set);
			sigaddset(&signal_set, SIGINT);
			sigprocmask(SIG_BLOCK, &signal_set, NULL);
			sleep(TIMEOUT);
			kill(p[0].pid, SIGUSR2);
			struct select_cell sel;
			sel.data.move=-1;
			sel.msg_type = (long int)server_pid;
			send_message(msg_id, &sel, sizeof(struct select_cell), 0); //sblocco padre in attesa di messaggio
			exit(0);
		}

		//attesa messaggio da player1
		struct select_cell sel;
		recive_message(msg_id, &sel, sizeof(struct select_cell), (long int)(getpid()), 0);
		if(sel.data.move!=-1){
			insert_getton_on_map(shm_map, dim_map[0], dim_map[1], sel.data.move, symbols[0]);
			semop_siginterrupt(sem_id_player[0], &confirm_move, 1);
		}
		//Fine turno giocatore 1
		semop_siginterrupt(sem_id_player[0], end_turn, 2);
		kill(child_pid, SIGTERM);

		if(check_winner(0, dim_map[0], dim_map[1], symbols)){ //se c'è un vincitore
			break;
		}

		//Inizio turno giocatore 2
		semop_siginterrupt(sem_id_player[1], start_turn, 2);
		child_pid=fork();
		if(child_pid==0){
			//aggiungo il segnale SIGINT alla lista dei segnali ignorati/bloccati
			sigset_t signal_set;
			sigemptyset(&signal_set);
			sigaddset(&signal_set, SIGINT);
			sigprocmask(SIG_BLOCK, &signal_set, NULL);
			sleep(TIMEOUT);
			kill(p[1].pid, SIGUSR2);
			struct select_cell sel;
			sel.data.move=-1;
			sel.msg_type = (long int)server_pid;
			send_message(msg_id, &sel, sizeof(struct select_cell), 0); //sblocco padre in attesa di messaggio
			exit(0);
		}
		recive_message(msg_id, &sel, sizeof(struct select_cell), (long int)(getpid()), 0);
		if(sel.data.move!=-1){
			insert_getton_on_map(shm_map, dim_map[0], dim_map[1], sel.data.move , symbols[1]);
			semop_siginterrupt(sem_id_player[1], &confirm_move, 1);
		}
		//Fine turno giocatore 2
		semop_siginterrupt(sem_id_player[1], end_turn, 2);
		kill(child_pid, SIGTERM);
		if(check_winner(1, dim_map[0], dim_map[1], symbols)){ //se c'è un vincitore
			break;
		}
	}
	print_map(shm_map, dim_map[0], dim_map[1]);
	sops.sem_num=0;
	sops.sem_op=0;
	for(int i = 0; i < PLAYERS ; i++ )
		semop_siginterrupt(sem_id_end_player[i], &sops, 1);
	clear_ipc();
	return 0;
}

int check_winner(int player_turn, int width, int height, char* symbols){
	int check;
	if((check=check_map(shm_map, width, height, symbols)) == -1){ //informazioni della fine della partita ai client
			printf("Pareggio!\n");
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
			return 1;
	}
	else if (check){
		struct msg_end_game msg;
		printf("%s ha vinto la partita\n", p[player_turn].name);
		for (int i=0; i<PLAYERS; i++){
			msg.msg_type = (long int)p[i].pid;
			msg.info.winner = i == player_turn ? 1 : 0;
			msg.info.status = 0;
			if(msgsnd(msg_id, &msg, sizeof(msg), 0)==-1){
				perror("Error send message on Server");
				exit(0);
			}
			kill(p[i].pid, SIGUSR1);
		}
		return 1;
	}

	return 0;
}

void clear_ipc(){
	shmdt(shm_map);
	shm_remove(shm_id_map);
	sem_remove(sem_id_player[0]);
	sem_remove(sem_id_player[1]);
	sem_remove(sem_id_access);
	sem_remove(sem_id_end_player[0]);
	sem_remove(sem_id_end_player[1]);
	msg_queue_remove(msg_id);
	exit(0);
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


void check_args(int argc, char **argv, int dim[]){
	if (argc==1){
		printf("Nessun arogmento passato\n");
		print_guide();
		exit(0);
	}

	if(strcmp(argv[1],"-h")==0){
		print_guide();
		exit(0);
	}

	if (argc != 5){
		printf("Devono essere passati 4 argomenti!\n");
		print_guide();
		exit(0); //TODO Gestione degli errori
	}

	if(strlen(argv[3]) != 1 || strlen(argv[4]) != 1){
		printf("Solo un singolo carattere deve essere passato!\n");
		print_guide();
		exit(0);
	}

	dim[0]=atoi(argv[1]);
	dim[1]=atoi(argv[2]);
	if(dim[0] < 5 || dim[1] < 5 || dim[0] > 20 || dim[1] > 20 ){
		printf("La dimensione del campo deve essere compresa da 5 a 20!\n");
		print_guide();
		exit(0); //TODO Gestione degli errori
	}

	if(argv[3][0] == argv[4][0]){
		printf("I simboli non possono essere uguali!\n");
		exit(0);
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

void print_guide(){
	printf("./F4Server [map_length] [map_width] [symbol_player1] [symbiol_player2]\nesempio:\n./F4Server 5 5 X O\n");
}




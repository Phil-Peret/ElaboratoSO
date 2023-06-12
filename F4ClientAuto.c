#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include "semaphore.c"
#include "message_queue.c"
#include "color.c"
#include "map.c"
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#define PATH_MAX 4096

int check_choose(char*, char[], int);
void fdrain(FILE *const);
int valid_position(char*, int, int*);

//variabili globali per gestione dei segnali
int sem_access;
int msg_id;
int shm_id;
pid_t server_pid;
int sem_end; //semaforo terminazione
int timeout=0;
char *shm_map;


struct info_game{
	int n_player;
	char symbol;
	int width;
	int height;
	int semaphore;
	int shared_memory;
	char name_vs[16]; //nome avversario
	pid_t pid_server;
	int sem_end;
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



void sig_handler_end(int sig){
	struct sembuf sops= {0,-1,0};
	printf("SIGUSR1 recive\n");

	struct msg_end_game msg;
	if(msgrcv(msg_id, &msg,  sizeof(struct msg_end_game), (long int)(getpid()), 0) == -1){
		perror("Error read message in a queue");
	}
	if (msg.info.winner == -1){
		printf("Tie!\n");
		if(msg.info.status == 1){
			yellow();
			printf("Server has been terminated\n");
			reset_color();
		}
		else if(msg.info.status == 0){
			printf("Map is full!\n");
		}
	}
	else if (msg.info.winner == 0){
		red();
		printf("You lose the game!\n");
		reset_color();
	}
	else {
		green();
		printf("You won the match\n");
		reset_color();
		if(msg.info.status==1){
			printf("Opponent retired from the match!\n");
		}
	}
	if(shmdt(shm_map) == -1){ //detach memoria condivisa
			perror("Error in detach shm");
	}
	semop_siginterrupt(sem_end, &sops, 1);
	exit(0);
}

void sig_handler_exit(int sig){
	struct sembuf sops= {0,-1,0};
	printf("SIGINT recive\n");
	char a;
	fdrain(stdin);
	yellow();
	printf("Are you sure to exit of the game? [y/n]");
	reset_color();
	a=fgetc(stdin);
	if(a == 'y' || a == 'Y'){
		struct msg_end_game msg;
		msg.msg_type=(long int)server_pid;
		msg.info.winner=0;
		msg.info.status=(long int)getpid();
		if(msgsnd(msg_id, &msg, sizeof(struct msg_end_game), 0) == -1){
			perror("Error message send \n");
		}
		if(shmdt(shm_map) == -1){ //detach memoria condivisa
			perror("Error in detach shm");
		}
		semop_siginterrupt(sem_end, &sops, 1);
		kill(server_pid, SIGUSR2); //mando al server il segnale che il giocatore si è ritirato
		exit(0);
	}
	else if(a=='n' || a=='N'){
		return;
	}
	else{
		printf("Not valid choose");
		return;
	}

}

void sig_timeout_turn(int sig){
	timeout=1;
	printf("Timeout! Skip this turn!\n");
}

void ignore_sigint(int sig){
	printf("It is not possible to exit the game while waiting for an opponent\n");
}



int main(int argc, char** argv){
	srand(time(NULL));
	//cambio funzione uscita programma
	if((signal(SIGUSR1, sig_handler_end))==(void*)-1){
		perror("Error setting signal");
	}
	char cwd[PATH_MAX];
	if(getcwd(cwd, sizeof(cwd)) != NULL){
			printf("Cartella corrente: %s\n",cwd);
	}
	else{
		exit(0);
	}

	char name[16];
	strcpy(name, argv[0]);
	printf("Name player: %s\n", name);
	//cartella corrente for tok

	//Semaforo per la gestione degli accessi alla partita
	sem_access = semget(ftok(cwd,5), 2, 0666); //semaforo per la gestione della prima connessione
	int sem_turn;
	int dim_map[2];

	if (sem_access==-1){
		perror("Seaphore not created by Server");
		exit(0);
	}

	struct sembuf sops={0,-1,IPC_NOWAIT};
	semop_siginterrupt(sem_access, &sops, 1);

	//Message queue per l'iscrizione alla partita
	printf("Access to the game...\n");
	if((msg_id=msgget(ftok(cwd,2),0666)) == -1){
		perror("Error call msgqueue");
		exit(0);
	}
	sops.sem_flg=0; //reset semflg

	//il processo manda il proprio pid al server
	struct msg_registration msg_reg;
	msg_reg.msg_type=1;
	msg_reg.info.pid=getpid();

	strcpy(msg_reg.info.name, name);
	send_message(msg_id, &msg_reg, sizeof(struct msg_registration), 0);

	sops.sem_num=1;
	sops.sem_op=0;

	//in attesa della risposta dal server
	semop_siginterrupt(sem_access, &sops, 1);

	//lettura dei messaggi in queue
	struct msg_info_game info_recive;
	recive_message(msg_id, &info_recive, sizeof(struct msg_info_game), (long int)(getpid()), 0);
	sem_turn=info_recive.info.semaphore;
	shm_id=info_recive.info.shared_memory;
	dim_map[0]=info_recive.info.width;
	dim_map[1]=info_recive.info.height;
	server_pid = info_recive.info.pid_server;
	sem_end = info_recive.info.sem_end;

	if (signal(SIGINT, sig_handler_exit) == (void*)-1){
		perror("Error setting signal");
	}

	//carico la memoria condivisa
	shm_map=shmat(shm_id, NULL, 0666);
	if (shm_map == (void *) -1){
		perror("Shared memory attach!");
		exit(0);
	}

	while(1){ //cambiare con for in base alla dimensione del campo(?)
		if(signal(SIGUSR2, sig_timeout_turn)==(void*)-1){
			perror("Error in set signal handler");
		}
		sops.sem_num=0;
		sops.sem_op=-1;
		//Semaforo per l'accesso al truno
		semop_siginterrupt(sem_turn, &sops, 1);
		//print_map(shm_map,dim_map[0],dim_map[1]);
		int* valid_pos = (int*)malloc(sizeof(int)); //almeno una posizione libera ci deve sempre essere
		int len_array = valid_position(shm_map, dim_map[0], valid_pos);
		int pos = valid_pos[rand()%len_array];
		struct sembuf wait_confirm = {2,-1,0};
		struct select_cell sel;
		sel.msg_type = (long int)server_pid;
		sel.data.move = pos;
		send_message(msg_id, &sel, sizeof(struct select_cell), 0);
		semop_siginterrupt(sem_turn, &wait_confirm,1);
		//insert_getton_on_map(shm_map,dim_map[0],dim_map[1], pos, symbol);
		//print_map(shm_map,dim_map[0],dim_map[1]);
		//printf("\n-----------\n");
		//semaforo per la conferma dell'inserimento della mossa

		sops.sem_num=1;
		semop_siginterrupt(sem_turn, &sops, 1);
		timeout=0;

	}
	return 0;
}

int valid_position(char *map, int width, int *valid_pos){
	int count=0;
	for (int i=0; i<width; i++){
		if(map[i]==' '){
			count++;
			valid_pos = realloc(valid_pos, count*sizeof(int));
			valid_pos[count-1] = i;
		}
	}
	return count;
}



void fdrain(FILE *const in){ //apro stdin come file
	if (in) {
		int const descriptor = fileno(in);
		int dummy;
		long flags;

		flags = fcntl(descriptor, F_GETFL);
		fcntl(descriptor, F_SETFL, flags | O_NONBLOCK); //setto il flag del filedescriptor aperto (stdin) come non bloccante

		do { 	//adesso è possibile "eliminare" i caratteri precedenti immessi nel stdin
			dummy = getc(in);
		} while (dummy != EOF);

		fcntl(descriptor, F_SETFL, flags);
	}
}



int check_choose(char* choose, char map[], int width){
	if(choose[strlen(choose)-1] != '\n') return -1; //se l'ultimo carattere non è \n allora ritorna -1
	int pos = atoi(choose);
	if (pos >=0 && pos < width){
		if(map[pos] != ' '){
			return -1;
		}
	}
	return pos; //ritorna la posizione in int
}
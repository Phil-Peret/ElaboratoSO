/************************************
*VR421770
*Filippo Peretti
*15/06/2023
*************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
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
void check_args(int, char**, char*, char*);
int nfile_current_dir(char*);
void set_signal_interrupt(int,int);
int valid_position(char*, int, int*);


//variabili globali per gestione dei segnali
int sem_access;
int msg_id;
int shm_id;
pid_t server_pid;
int sem_end = 0; //semaforo terminazione
int timeout = 0;
char *shm_map = NULL;
int dim_map[2];

struct msg_info_game{
	long int msg_type;
	int n_player;
	char symbol;
	int width;
	int height;
	int semaphore;
	int shared_memory;
	pid_t pid_server;
	int sem_end;
};

struct msg_name_op{
	long int msg_type;
	char name[16];
	char symbol;
};

struct msg_end_game{
	long int msg_type;
	int winner;
	long int status;
};


struct msg_registration{
	long int msg_type;
	pid_t pid;
	char name[16];  //nome giocatore
	int vs_cpu;
};

struct select_cell{
	long int msg_type;
	int move;
};



void sig_handler_end(int sig){
	struct sembuf sops = {0,-1,0};
	printf("SIGUSR1 recive\n");
	struct msg_end_game msg;
	recive_message(msg_id, &msg,  sizeof(struct msg_end_game) - sizeof(long int), (long int)(getpid()), 0);
	if(shmdt(shm_map) == -1){ //detach memoria condivisa
		perror("Error in detach shm");
	}
	semop_siginterrupt(sem_end, &sops, 1);
	exit(0);
}


int main(int argc, char** argv){
	//set signal_hendler
	if((signal(SIGUSR1, sig_handler_end))==(void*)-1){
		perror("Error setting signal");
	}

	//get current work dir
	char cwd[PATH_MAX];
	if(getcwd(cwd, sizeof(cwd)) != NULL){
			printf("Cartella corrente: %s\n",cwd);
	}
	else{
		exit(0);
	}
	//controllo file presenti nella cartella corrente
	int n_file = nfile_current_dir(cwd);
	char name[16];
	strcpy(name, argv[0]);
	printf("Nome del giocatore: %s\n", name);
	//Semaforo per la gestione degli accessi alla partita
	sem_access = semget(ftok(cwd,5), 1, 0666); //semaforo per la gestione della prima connessione
	int sem_turn;

	if (sem_access==-1){
		perror("Errore: Probabilmente il server non è attivo!");
		exit(0);
	}

	struct sembuf sops={0,-1,IPC_NOWAIT};
	semop_siginterrupt(sem_access, &sops, 1);

	//Message queue per l'iscrizione alla partita
	printf("Accesso alla partita...\n");
	if((msg_id=msgget(ftok(cwd,2),0666)) == -1){
		perror("Error call msgqueue");
		exit(0);
	}


	//il processo manda il proprio pid al server
	struct msg_registration msg_reg;
	msg_reg.msg_type=1;
	msg_reg.pid=getpid();
	msg_reg.vs_cpu = argc == (n_file + 2) ? 1 : 0;
	strcpy(msg_reg.name, name);
	send_message(msg_id, &msg_reg, sizeof(struct msg_registration) - sizeof(long int), 0);

	//lettura dei messaggi in queue
	struct msg_info_game info_recive;
	recive_message(msg_id, &info_recive, sizeof(struct msg_info_game) - sizeof(long int), (long int)(getpid()), 0);

	printf("Il tuo simbolo è %c, attendi il tuo turno...\n", info_recive.symbol);
	sem_turn = info_recive.semaphore;
	shm_id = info_recive.shared_memory;
	dim_map[0] = info_recive.width;
	dim_map[1] = info_recive.height;
	server_pid = info_recive.pid_server;
	sem_end = info_recive.sem_end;

	//carico la memoria condivisa
	shm_map = shmat(shm_id, NULL, 0666);
	if (shm_map == (void *) -1){
		perror("Shared memory attach!");
		exit(0);
	}
	//attesa informazioni del server sull'avversario'
	struct msg_name_op msg_rcv;
	recive_message(msg_id, &msg_rcv, sizeof(struct msg_name_op) - sizeof(long int), (long int)(getpid()), 0);
	printf("Il nome dell'avversario è %s  con il simbolo %c, buona fortuna!\n", msg_rcv.name, msg_rcv.symbol);


	while(1){ //cambiare con for in base alla dimensione del campo(?)
		sops.sem_flg=0; //reset semflg
		sops.sem_num=0;
		sops.sem_op=-1;
		//Semaforo per l'accesso al truno
		semop_siginterrupt(sem_turn, &sops, 1);
		//print_map(shm_map,dim_map[0],dim_map[1]);
		int* valid_pos = (int*)malloc(sizeof(int)); //almeno una posizione libera ci deve sempre essere
		int len_array = valid_position(shm_map, dim_map[0], valid_pos);
		int pos = valid_pos[rand()%len_array];
		struct select_cell sel;
		sel.msg_type = (long int)server_pid;
		sel.move = pos;
		send_message(msg_id, &sel, sizeof(struct select_cell) - sizeof(long int), 0);
		struct sembuf wait_confirm = {2,-1,0};
		semop_siginterrupt(sem_turn, &wait_confirm,1);
		sops.sem_num=1;
		semop_siginterrupt(sem_turn, &sops, 1);
	}
	return 0;
}

void set_signal_interrupt(int sgn, int val){ //non utilissimo ma codice gradevole
	if(siginterrupt(sgn, 1) == -1){
		perror("Error set siginterrupt");
	}
}


int nfile_current_dir(char * path){
	int count=0;
	DIR * dirp;
	struct dirent * entry;
	dirp = opendir(path);
	while ((entry = readdir(dirp)) != NULL) {
			if ((entry->d_type == DT_REG || entry->d_type == DT_DIR) && entry->d_name[0]!='.') {
			 count++;
			}
	}
	closedir(dirp);
	return count;
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


void check_args(int argc, char** argv, char* name, char* cwd){

	//controllo file presenti nella cartella corrente
	int n_file = nfile_current_dir(cwd);

	if((argc != 2) && (argc != (n_file + 2))){
		printf("Errore: deve essere passato l'argomento nome del giocatore\n");
		exit(0);
	}
	else if(strlen(argv[1]) > 15){
		printf("Il nome del giocatore deve essere di massimo 15 caratteri! %lu\n", strlen(argv[1]));
		exit(0);
	}
	//se non ha generato errori
	for (int i=0; i<strlen(argv[1]); i++){
		name[i] = argv[1][i];
	}
	name[strlen(argv[1])] = '\0';
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

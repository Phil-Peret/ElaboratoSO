/************************************
*VR421770
*Filippo Peretti
*15/06/2023
*************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
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

//variabili globali per gestione dei segnali
int sem_access;
int msg_id;
int shm_id;
pid_t server_pid;
int sem_end = 0; //semaforo terminazione
int timeout = 0;
char *shm_map = NULL;
int dim_map[2];


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

struct msg_info_game{
	long int msg_type;
	struct info_game info;

};

struct name_op{
	char name[16];
	char symbol;
};

struct msg_name_op{
	long int msg_type;
	struct name_op op;
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
	struct sembuf sops = {0,-1,0};
	printf("SIGUSR1 recive\n");
	struct msg_end_game msg;
	printf("mypid %i\n",(getpid()));
	recive_message(msg_id, &msg,  sizeof(struct msg_end_game), (long int)(getpid()), 0);
	print_map(shm_map, dim_map[0], dim_map[1]);
	if (msg.info.winner == -1){
		printf("Tie!\n");
		if(msg.info.status == 1){
			yellow();
			printf("Il server è stato terminato \n");
			reset_color();
		}
		else if(msg.info.status == 0){
			printf("Map is full!\n");
		}
	}
	else if (msg.info.winner == 0){
		red();
		printf("Hai perso la partita!\n");
		reset_color();
	}
	else {
		green();
		printf("Hai vinto la partita!\n");
		reset_color();
		if(msg.info.status==1){
			printf("L'avversario si è ritirato dalla partita!\n");
		}
	}
	if(shmdt(shm_map) == -1){ //detach memoria condivisa
		perror("Error in detach shm");
	}
	semop_siginterrupt(sem_end, &sops, 1);
	exit(0);
}

void sig_handler_exit(int sig){
	if(siginterrupt(SIGUSR2, 1) == -1){ //il segnale SIGUSR2 può interrompere la scanf
		perror("Error set siginterrupt");
	}
	struct sembuf sops = {0,-1,0};
	printf("SIGINT recive\n");
	char a;
	fdrain(stdin);
	yellow();
	printf("Sicuro di uscire dalla partita? Perderai per abbandono... [s/n]");
	reset_color();
	a=fgetc(stdin);
	if(a == 's' || a == 'S'){
		struct msg_end_game msg;
		msg.msg_type=(long int)server_pid;
		msg.info.winner=0;
		msg.info.status=(long int)getpid();
		if(msgsnd(msg_id, &msg, sizeof(struct msg_end_game), 0) == -1){
			perror("Error send message \n");
		}
		if(shmdt(shm_map) == -1){ //detach memoria condivisa
			perror("Error in detach shm");
		}
		semop_siginterrupt(sem_end, &sops, 1);
		kill(server_pid, SIGUSR2); //mando al server il segnale che il giocatore si è ritirato
		exit(0);
	}
	else if(a == 'n' || a == 'N'){
		return;
	}
	else{
		printf("Scelta non valida\n");
		return;
	}

}

void sig_timeout_turn(int sig){
	timeout=1;
	printf("Timeout! Il turno passa al prossimo giocatore!\n");
}

void ignore_sigint(int sig){
	printf("Non è possibile abbandonare la partita mentre si è in attesa di un altro giocatore\n");
}



int main(int argc, char** argv){
	if (signal(SIGINT, ignore_sigint) == (void*)-1){
		perror("Error setting signal");
	}
	//set signal_hendler
	if(signal(SIGUSR1, sig_handler_end)==(void*)-1){
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
	//controllo argomenti inseriti
	check_args(argc, argv, name, cwd);
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
	msg_reg.info.pid=getpid();
	msg_reg.info.vs_cpu = argc == (n_file + 2) ? 1 : 0;
	strcpy(msg_reg.info.name, name);
	send_message(msg_id, &msg_reg, sizeof(struct msg_registration), 0);

	//lettura dei messaggi in queue
	struct msg_info_game info_recive;
	recive_message(msg_id, &info_recive, sizeof(struct msg_info_game), (long int)(getpid()), 0);

	printf("Il tuo simbolo è %c, attendi il tuo turno...\n", info_recive.info.symbol);
	sem_turn = info_recive.info.semaphore;
	shm_id = info_recive.info.shared_memory;
	dim_map[0] = info_recive.info.width;
	dim_map[1] = info_recive.info.height;
	server_pid = info_recive.info.pid_server;
	sem_end = info_recive.info.sem_end;

	//carico la memoria condivisa
	shm_map = shmat(shm_id, NULL, 0666);
	if (shm_map == (void *) -1){
		perror("Shared memory attach!");
		exit(0);
	}
	//attesa informazioni del server sull'avversario'
	struct msg_name_op msg_rcv;
	recive_message(msg_id, &msg_rcv, sizeof(struct msg_name_op), (long int)(getpid()), 0);
	if(msg_rcv.op.name[0]==' ' && msg_rcv.op.symbol == -1){
		printf("Il server è stato terminato\n");
		if(shmdt(shm_map) == -1){ //detach memoria condivisa
			perror("Error in detach shm");
		}
		semop_siginterrupt(sem_end, &sops, 1);
		exit(0);
	}
	printf("Il nome dell'avversario è %s  con il simbolo %c, buona fortuna!\n", msg_rcv.op.name, msg_rcv.op.symbol);
	//adesso è possibile uscire dalla partita
	if (signal(SIGINT, sig_handler_exit) == (void*)-1){
		perror("Error setting signal");
	}


	while(1){ //cambiare con for in base alla dimensione del campo(?)
		if(signal(SIGUSR2, sig_timeout_turn)==(void*)-1){
			perror("Error in set signal handler");
		}
		sops.sem_flg=0;
		sops.sem_num=0;
		sops.sem_op=-1;
		//Semaforo per l'accesso al turno
		semop_siginterrupt(sem_turn, &sops, 1);
		print_map(shm_map,dim_map[0],dim_map[1]);
		char choose[4];
		int pos=0;
		//clear standar input
		do {
			//il segnale SIGUSR blocca le chiamate di sistema, in modo da sospendere l'esecuzione del codice'
			set_signal_interrupt(SIGUSR2,1);
			set_signal_interrupt(SIGINT, 1);
			if(pos==-1){
				printf("Posizione non valida\n");
			}
			//cancello caratteri indesiderati prima dell'insetimento dell'input
			fdrain(stdin);
			printf("Scegli la posizione: ");
			fgets(choose,4,stdin);
		}while((pos=check_choose(choose,shm_map,dim_map[0])) == -1 && timeout != 1);
		//reset signal
		set_signal_interrupt(SIGINT, 0);
		set_signal_interrupt(SIGUSR2, 0);
		if(timeout != 1){ //se il timer è scaduto, salta l'inserimento della mossa
			struct sembuf wait_confirm = {2,-1,0};
			struct select_cell sel;
			sel.msg_type = (long int)server_pid;
			sel.data.move = pos;
			send_message(msg_id, &sel, sizeof(struct select_cell), 0);
			semop_siginterrupt(sem_turn, &wait_confirm,1);
			print_map(shm_map,dim_map[0],dim_map[1]);
			printf("\n--------------------------------\n");
			//semaforo per la conferma dell'inserimento della mossa
		}
		sops.sem_num=1;
		//non necessario con msgqueue
		semop_siginterrupt(sem_turn, &sops, 1);
		timeout=0;

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
	int len = strlen(choose)-1;
	if(choose[len] != '\n') return -1; //se l'ultimo carattere non è \n allora ritorna -1
	for (int i=0; i<len ; i++){
		if(!isdigit(choose[i])) return -1;
	}
	int pos = atoi(choose);
	if (pos >=0 && pos < width){
		if(map[pos] != ' '){
			return -1;
		}
	}
	return pos; //ritorna la posizione in int
}
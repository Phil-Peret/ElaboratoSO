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
void check_args(int, char**, char*);

//variabili globali per gestione dei segnali
int msg_id;
int shm_id;
pid_t server_pid;


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

void sig_handler_end(int sig){
	printf("SIGUSR1 recive\n");
	struct msg_end_game msg;
    if(msgrcv(msg_id, &msg,  sizeof(msg), (long int)(getpid()), 0) == -1){
	    perror("Error read message in a queue");
    }
	if (msg.info.winner == -1){
		printf("Tie!");
	}
	else if (msg.info.winner == 0){
		printf("You lose the game!\n");
	}
	else {
		printf("Player%i Win!\n",msg.info.winner);
		if(msg.info.status==1){
			printf("Other player retired from the match!\n");
		}
	}
	exit(0);
}

void sig_handler_exit(int sig){
	printf("SIGINT recive\n");
	char a;
	printf("Are you sure to exit of the game? [y/n]");
	a=fgetc(stdin);
	if(a=='y' || a=='Y'){
		struct msg_end_game msg;
		msg.msg_type=(long int)server_pid;
		msg.info.winner=0;
		msg.info.status=(long int)getpid();
		if(msgsnd(msg_id, &msg, sizeof(struct msg_end_game), 0) == -1){
			perror("Error message send \n");
		}
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


int main(int argc, char** argv){
	if((signal(SIGUSR1, sig_handler_end))==(void*)-1){
		perror("Error setting signal");
	}
	if ((signal(SIGINT, sig_handler_exit)) == (void*)-1){
		perror("Error setting signal");
	}


	char name[16];
	check_args(argc, argv, name);
	printf("Name player: %s\n", name);
	//cartella corrente for tok
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
    printf("Access to the game...\n");
    if((msg_id=msgget(ftok(cwd,2),0666)) == -1){
		perror("Error call msgqueue");
    }

    //il processo manda il proprio pid al server
    struct msg_registration msg_reg;
    msg_reg.msg_type=1;
    msg_reg.info.pid=getpid();
	strcpy(msg_reg.info.name, name);
    if(msgsnd(msg_id, &msg_reg, sizeof(msg_reg), 0)==-1){
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
	printf("Opponent name: %s\n", info_recive.info.name_vs);
    printf("Your symbol is %c, wait for your turn...\n", info_recive.info.symbol);
	symbol=info_recive.info.symbol;
    sem_turn=info_recive.info.semaphore;
    shm_id=info_recive.info.shared_memory;
    dim_map[0]=info_recive.info.width;
    dim_map[1]=info_recive.info.height;
	server_pid = info_recive.info.pid_server;

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

void check_args(int argc, char** argv, char* name){
	if(argc!=2){
		printf("Error: only one argument can be passed (name player)\n");
		exit(0);
	}
	else if(strlen(argv[1]) > 15){
		printf("Max 15 char for args name! %lu\n", strlen(argv[1]));
		exit(0);
	}
	//se non ha generato errori
	for (int i=0; i<strlen(argv[1]) ; i++){
		name[i] = argv[1][i];
	}
	name[strlen(argv[1])] = '\0';
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